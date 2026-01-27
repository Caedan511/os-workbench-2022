#include "co.h"
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>
#include "stdio.h"
#include <time.h>

#define STACK_SIZE (int)(1<<16)

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %2, %%rdi; jmp *%1"
    : 
    : "b"((uintptr_t)sp), "d"(entry), "a"(arg) 
    : "memory"
#else
    "movl %0, %%esp; movl %2, 4(%0); jmp *%1"
    : 
    : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg) 
    : "memory"
#endif
  );
}

enum co_status {
  CO_NEW = 1, // new
  CO_RUNNING, // executed
  CO_WAITING, // waiting not use
  CO_DEAD,    // coroutine done
};

struct co {
  const char *name;
  void (*func)(void *); // co_start entry and parameter
  void *arg;
  
  enum co_status status;  
  struct co *    waiter;  // not use
  jmp_buf        context; // regisier context 
  uint8_t        stack[STACK_SIZE]; // stack of coroutine
};



static struct co* co_array[128];
static int co_nums = 0;
static struct co* current;


static void co_exit();

//start run a coroutine
static void co_switch(struct co * co){
  uintptr_t sp = (uintptr_t)(co->stack + STACK_SIZE);
  
  sp &= ~0xFul;
#if __x86_64__
  sp -= 8;
  *(uintptr_t *)(sp) = (uintptr_t)co_exit;   // fake return address
#else 
  *(uintptr_t *)(sp - 8) = (uintptr_t)co_exit;
#endif
  current = co;
  stack_switch_call((void*)sp, co->func, (uintptr_t)co->arg);
}

static void co_exit(){

  current->status = CO_DEAD;
  current = co_array[0];
  longjmp(current->context,0);

}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  if(co_array[0] == NULL){
    co_array[0] = malloc(sizeof(struct co));

    struct co* co_main = co_array[0];
    co_main->name = "main";
    co_main->status = CO_RUNNING;

    co_nums++;

    current = co_main;
    srand((unsigned)time(NULL));
  }
  struct co * ptr = malloc(sizeof(struct co));
  ptr->name = name;
  ptr->status = CO_NEW;
  ptr->func = func;
  ptr->arg = arg;

  co_array[co_nums] = ptr;
  co_nums++;


  return ptr;
}

void co_wait(struct co *co) {
  while(1)
  {
    if(co->status == CO_NEW||co->status == CO_RUNNING){
      co_yield();
    }
    else if(co->status == CO_DEAD){
      // printf("free:%s\n",co->name);
      free(co);
      break;
    }
  } 
}

void co_yield() {
  int val = setjmp(current->context);
  if (val == 0) {
    while(1)
    {
      int next = rand()%co_nums;
      // printf("switch to coroutine num: %d\n",next);
      struct co *co_next = co_array[next];
      
      if(co_next->status == CO_NEW){
        co_next->status = CO_RUNNING;
        co_switch(co_next);
        break;
      }
      else if(co_next->status == CO_RUNNING){
        current = co_next;
        longjmp(co_next->context,0);
        break;
      }
    }
  } else {

  }
}
