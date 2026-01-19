#include <common.h>

static void os_init() {
  pmm->init();
  pmm->alloc(sizeof(long));
  pmm->alloc(sizeof(long));
  pmm->alloc(sizeof(long));
  pmm->alloc(32*(1<<20));
  pmm->alloc(32*(1<<20));
  pmm->alloc(1);
  pmm->alloc(1);

  int* ptr  = (int*)(0x123456789l);
  *ptr = 100;
  *ptr += 100;
  printf("%d %p\n",*ptr, ptr);


}

static void os_run() {
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  while (1) ;
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
