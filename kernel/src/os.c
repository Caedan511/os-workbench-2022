// #include <common.h>
#include<os.h>
#include<devices.h>

static void tty_reader(void *arg) {
  device_t *tty = dev->lookup(arg);
  char cmd[128], resp[128], ps[16];
  snprintf(ps, 16, "(%s) $ ", arg);
  while (1) {
    tty->ops->write(tty, 0, ps, strlen(ps));
    int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
    cmd[nread] = '\0';
    sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
    tty->ops->write(tty, 0, resp, strlen(resp));
    
  }
}
static void os_init() {
  pmm->init();
  kmt->init();
  dev->init();
  kmt->create(pmm->alloc(sizeof(task_t)), "tty_reader", tty_reader, "tty1");
  kmt->create(pmm->alloc(sizeof(task_t)), "tty_reader", tty_reader, "tty2");
}

int locked = 0;
int value;

static void os_run() {
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  iset(true);
  #ifdef PMM_TEST
  //stress test
  long* kint[64];
  void* page[64];
  for(;;) {

    for(int i = 0;i < 64;i++) {
      kint[i] = pmm->alloc(32);
      page[i] = pmm->alloc(1*(1<<10));

      *(int*)(page[i]) = i + cpu_current();
      *(kint[i]) = i;
    }

    for(int i = 63;i >= 0 ;i--) {
      assert(*(kint[i]) == i);
      assert(*(int*)(page[i]) == (i + cpu_current()));

      pmm->free(kint[i]);
      pmm->free(page[i]);
    }

    lock(&locked);
    
    // value = 42 + cpu_current();
    // printf("cpu:%d, value:%d\n",cpu_current(), value);
    // assert(value == 42 + cpu_current());

    unlock(&locked);

  }
  #endif

  while (1) ;
}

#define NHANDLER 64

typedef struct {
  handler_t handler;
  int event;
  int seq;
} handler_s;

static handler_s handlers_sorted_by_seq[NHANDLER];
static int handlers_num = 0;


//this is the only entry of interrupt, return context to switch.
static Context *trap(Event ev, Context *context) {

  // printf("i am traped,%d \n",ienabled());
  
  Context *next = NULL;
  for (handler_s *h = handlers_sorted_by_seq;h < &handlers_sorted_by_seq[handlers_num];h++) {
    if (h->event == EVENT_NULL || h->event == ev.event) {
      Context *r = h->handler(ev, context);
      panic_on(r && next, "returning multiple contexts");
      if (r) next = r;
    }
  }
  panic_on(!next, "returning NULL context");
  // panic_on(sane_context(next), "returning to invalid context");
  return next;
}

//creat the interrupt handler function, Sort the function  according to seq using insertion sort.
static void on_irq(int seq, int event, handler_t handler) {

  int i = handlers_num; //inser_pos
  
  while(i > 0 && handlers_sorted_by_seq[i - 1].seq > seq) {
    handlers_sorted_by_seq[i].event = handlers_sorted_by_seq[i - 1].event; 
    handlers_sorted_by_seq[i].seq = handlers_sorted_by_seq[i - 1].seq; 
    handlers_sorted_by_seq[i].handler = handlers_sorted_by_seq[i - 1].handler; 
    i--;
  }
  handlers_sorted_by_seq[i].event = event; 
  handlers_sorted_by_seq[i].seq = seq; 
  handlers_sorted_by_seq[i].handler = handler;
  
  handlers_num++;

  //debug the handlers in seq
  // for(int i = 0;i < handlers_num;i++ ){
  //   printf("%d,",handlers_sorted_by_seq[i].seq);
  // }
  // printf("\n");
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = trap,
  .on_irq = on_irq,
};
