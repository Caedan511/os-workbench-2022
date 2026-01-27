#include <common.h>

static void os_init() {
  pmm->init();
  pmm->alloc(sizeof(long)); //8
  long* ptr1 =pmm->alloc(sizeof(long));//8
  pmm->alloc(sizeof(long));   //8
  pmm->alloc(2*sizeof(long)); //16
  pmm->alloc(4*sizeof(long)); //32
  pmm->alloc(8*sizeof(long)); //64
  pmm->alloc(8*sizeof(long)); //64

  printf("\n");
  pmm->alloc(16);

  long* ptr2 = pmm->alloc(32);
  
  long* ptr3 = pmm->alloc(64);

  // pmm->alloc(32*(1<<20));

  // pmm->alloc(32*(1<<20));
  pmm->free(ptr1);
  pmm->free(ptr2);
  pmm->free(ptr3);
  // pmm->alloc(32*(1<<20));

  pmm->alloc(16);
  pmm->alloc(16);

  void* k4ptr[64];
  for(int i = 0;i < 64;i++) {
    k4ptr[i] = pmm->alloc(4*(1<<10));
  }
  for(int i = 63;i >= 0 ;i--) {
    pmm->free(k4ptr[i]);
  }

  void *ptr5k = pmm->alloc(5*(1<<10));
  pmm->free(ptr5k);
  

  
  // int *p = pmm->alloc(sizeof(int));
  int* p  = (int*)(0x123456789l);
  *p = 100;
  *p += 100;
  printf("%d %p\n",*p, p);
  pmm->free((void*)0x123456789l);
  printf("%d, %d\n",cpu_count(),cpu_current());

  int* ps  = (int*)(0x6000008);
  *ps = 0;

}
int locked = 0;
int value;
void* ptrcpu[2];

static void os_run() {
  // for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
  //   putch(*s == '*' ? '0' + cpu_current() : *s);
  // }
  for(;;) {
    void *ptr5k = pmm->alloc(4*(1<<10));
    // ptrcpu[cpu_current()] = ptr5k;
    // assert(ptr5k != ptrcpu[-(cpu_current()-1)]);

    // ptrcpu[cpu_current()] = 0;
    pmm->free(ptr5k);

    lock(&locked);
    
    // value = 42 + cpu_current();
    // printf("cpu:%d, value:%d\n",cpu_current(), value);
    // assert(value == 42 + cpu_current());

    unlock(&locked);
    // pmm->free(p);
  }
  while (1) ;
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
};
