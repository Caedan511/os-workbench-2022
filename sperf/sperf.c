#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <regex.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

typedef struct {
  char name[64];
  double time;
} SysCall;

typedef struct {
  unsigned int row, col, w, h;
  int color;
  char name[64];
  double per;
} Pos;

void drawrectangle(Pos* rectangle) ;

int parse_line(const char *line, char *syscall, double *time) {
  static regex_t reg;
  static int inited = 0;
  regmatch_t pm[3];

  if (!inited) {
      regcomp(&reg,
        "^([a-zA-Z_][a-zA-Z0-9_]*)\\(.*\\).+<([0-9.]+)>",
        REG_EXTENDED);
      inited = 1;
  }

  if (regexec(&reg, line, 3, pm, 0) != 0)
      return -1;

  int len = pm[1].rm_eo - pm[1].rm_so;
  strncpy(syscall, line + pm[1].rm_so, len);
  syscall[len] = '\0';

  *time = atof(line + pm[2].rm_so);
  return 0;
}


int main(int argc, char *argv[]) {
  char **exec_argv = malloc(sizeof(char *) * (argc + 2));
  exec_argv[0] = "strace";
  exec_argv[1] = "-T";

  for (int i = 1; i < argc; i++)
    exec_argv[i + 1] = argv[i];

  exec_argv[argc + 1] = NULL;

  char *exec_envp[] = { "PATH=/bin", NULL, };

  int fildes[2];
  pipe(fildes);
  // printf("%d, %d\n",fildes[0], fildes[1]);
  pid_t pid = fork();


  if(pid == 0) {
    /*
      fork a new precess to execute the strace process
    */
    int devnull = open("/dev/null", O_WRONLY);
    dup2(devnull, STDOUT_FILENO);
    close(devnull);

    close(fildes[0]);
    dup2(fildes[1], STDERR_FILENO);
    close(fildes[1]);

    execve("strace",          exec_argv, exec_envp);
    execve("/bin/strace",     exec_argv, exec_envp);
    execve("/usr/bin/strace", exec_argv, exec_envp);
    perror(argv[0]);
    exit(EXIT_FAILURE);

  } else {
    /*
      read the mes through pipe 
    */
    char buf[4096];
    char line[1024];
    ssize_t n;
    SysCall syscalls_states[400], syscalls_tmp; 
    int line_len = 0, sys_len = 0, j;
    double total_times = 0;
    int color[] = {22, 130, 60, 24, 131, 236};//236

    close(fildes[1]);

    //read the size of stdin terminal
    struct winsize ws;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
    printf("rows: %d, cols: %d\n", ws.ws_row, ws.ws_col);

    time_t last = time(NULL);
    int showed = 0;

    /*
      if the pipe write port didn't close, continuously read the pipe contents
    */
    while ((n = read(fildes[0], buf, sizeof(buf))) > 0 || !showed) {

      // accumulate times of each syscall

      // write(STDOUT_FILENO, buf, n);
      if(n > 0) {
        for (ssize_t i = 0; i < n; i++) {
          if (buf[i] == '\n') {
              line[line_len] = '\0';
  
              parse_line(line, syscalls_tmp.name, &syscalls_tmp.time);
              for(j = 0;j < sys_len;j++){
                if(!strcmp(syscalls_states[j].name, syscalls_tmp.name)) {
                  syscalls_states[j].time += syscalls_tmp.time;
                  total_times += syscalls_tmp.time;
                  break;
                }
              }
              if(j == sys_len){
                strcpy(syscalls_states[sys_len].name, syscalls_tmp.name);
                syscalls_states[sys_len].time = syscalls_tmp.time;
                total_times += syscalls_tmp.time;
                sys_len++;
              }
              
              // printf("%-20s%.6f\n",syscalls_tmp.name,syscalls_tmp.time);
              line_len = 0;
          } else if (line_len < sizeof(line) - 1) {
              line[line_len++] = buf[i];
          }
        }
      }
      
      //analyze the data and show the percent of each syscall
      time_t now = time(NULL);
      if(now != last || !n){
        last = now;
        
        printf("\033[H\033[2J");//clear the screen and reset cursor
        printf("============ start anaylze ==========\n");
        Pos rectangle = {.row = 1, .col = 1};
        double showed_time = 0;
        /*
          selection sort and show
        */
        for(int i = 0;i < sys_len;i++){
          /*
            selection sort part
          */
          int max_id = 0;
          double max_time = 0;
          for(int j = i;j < sys_len;j++) {
            if(syscalls_states[j].time > max_time ) {
              max_id = j;
              max_time = syscalls_states[j].time;
            }  
          }

          char tmp[64];
          strcpy(tmp, syscalls_states[max_id].name);
          strcpy(syscalls_states[max_id].name, syscalls_states[i].name);
          syscalls_states[max_id].time = syscalls_states[i].time;

          strcpy(syscalls_states[i].name, tmp);
          syscalls_states[i].time = max_time;

          //print the percent by list
          // printf("%-20s%.1f%%\n",syscalls_states[i].name,syscalls_states[i].time * 100/total_times);

          /*
            draw the percentage rectangle of each syscall while the sort circulate 
            STRUCT rectangle
              .raw .col is the coordinate of the rectangle‘s upper left corner, it update while drawing each rectangle
          */
          if(i < 5) {
            rectangle.color = color[i];
            strcpy(rectangle.name, syscalls_states[i].name);
            rectangle.per = syscalls_states[i].time/total_times;
            

            if(i & 1){
              rectangle.h = (unsigned int)((ws.ws_row * ws.ws_col) * rectangle.per / (ws.ws_col - rectangle.col + 1));
              rectangle.w = (unsigned int)(ws.ws_col - rectangle.col );//there can add 1 to reach the edge 
              if(rectangle.w <= 2||rectangle.h <= 1 )break;//stop draw when the rectangle is too small
              drawrectangle(&rectangle);
              rectangle.row += rectangle.h ;
            }
            else {
              rectangle.w = (unsigned int)((ws.ws_row * ws.ws_col) * rectangle.per / (ws.ws_row - rectangle.row + 1));
              rectangle.h = (unsigned int)(ws.ws_row - rectangle.row );
              if(rectangle.w <= 2||rectangle.h <= 1 )break;
              drawrectangle(&rectangle);
              rectangle.col += rectangle.w ;
            }

            showed_time += syscalls_states[i].time;
          }
        }
        //draw the 'other' rectangle
        rectangle.color = color[5];
        strcpy(rectangle.name, "other");
        rectangle.per = 1 - showed_time/total_times;

        rectangle.w = (unsigned int)(ws.ws_col - rectangle.col );
        rectangle.h = (unsigned int)(ws.ws_row - rectangle.row );
        drawrectangle(&rectangle);


        //set the cursor to bottom
        printf("\033[%d;%dH", ws.ws_row , 0);
        fflush(stdout);
        showed = 1;
      }
    }

    //final exit
    // printf("============ final anaylze ==========\n");
    // for(int i = 0;i < sys_len;i++){
    //   // printf("%-20s%.6f\n",syscalls_states[i].name,syscalls_states[i].time);
    //   printf("%-20s%.1f%%\n",syscalls_states[i].name,syscalls_states[i].time * 100/total_times);
    // }

    // printf("\033[?25h"); visiable the cursor
    close(fildes[0]);
    wait(NULL);
    printf("parent exit\n");
    exit(EXIT_SUCCESS);
  }
}

void drawrectangle(Pos* rectangle) {

  printf("\033[48;5;%dm", rectangle->color);//Set background color.
  printf("\033[38;5;15m");//Set Foreground Color :White

  for (int i = 1; i < rectangle->h; i++) {
    printf("\033[%d;%dH", rectangle->row + i, rectangle->col + 2);//moves cursor to line #, column #
    for (int j = 2; j < rectangle->w; j++) {
      putchar(' ');
    }
  }
  //print syscall name 
  printf("\033[%d;%dH", \
    rectangle->row + rectangle->h/2, \
    (int)strlen(rectangle->name) <= rectangle->w/2 ? (rectangle->col + rectangle->w/2) : rectangle->col +  rectangle->w - (int)strlen(rectangle->name));
  printf("%s",rectangle->name);

  //print percentage of syscall time usage  
  printf("\033[%d;%dH", \
    rectangle->row + rectangle->h/2 + 1,\
    7 <= rectangle->w/2 ? (rectangle->col + rectangle->w/2) : rectangle->col +  rectangle->w - 7);
  printf("(%.1f%%)",rectangle->per * 100);

  printf("\033[0m");//reset all modes (styles and colors)

}