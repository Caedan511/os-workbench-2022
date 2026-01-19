#include <game.h>

#define FPS 30
#define FRAME_US (1000000 / FPS)
// Operating system is a C program!
int main(const char *args) {
  ioe_init();
  
  printf("mainargs = \"%s\"\n",args);// make run mainargs=xxx
  // splash();

  printf("num test nine: %d\nchar test s: %c\noctal num test 8: %o\nhex num test 65535: %x\nstring test: %s\nptr test: %p\n",\
          9,'s',8,65535,"i like you",(void*)0x123456789abcdef);
  printf("no 2th arg test\n");

  image_init();
  
  uint64_t last = uptime_us();
  int angle = 0;
  AM_INPUT_KEYBRD_T event = { .keycode = AM_KEY_NONE };

  while(1)
  {
    uint64_t now = uptime_us();
    if (now - last < FRAME_US) continue;
    last += FRAME_US;

    draw_cube(angle);
    angle = (angle + 1) % 360;

    ioe_read(AM_INPUT_KEYBRD, &event);
    if (event.keycode  == AM_KEY_ESCAPE && event.keydown) {
      halt(01);
    }
  }
  return 0;
}
