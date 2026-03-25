#include <common.h>

#define STACK_SIZE 4096   //kernel thread stack: 4K
#define NHANDLER 16       //max num of irq handler function

struct spinlock {
  int locked;       // Is the lock held?
  // For debugging:
  const char *name;        // Name of lock.
};

typedef struct {
  spinlock_t lock;
  
  task_t *front;
  task_t *rear;
} queue;

enum task_state { UNUSED, USED, READY, RUNNABLE, RUNNING, BLOCKED, BLOCKING, DEAD};

struct task {
  spinlock_t lock;

  enum task_state state;
  const char *name;
  Context *context;
  Area stack;
  struct task *next;
  
};

struct semaphore {
  const char *name;
  spinlock_t lock;
  int count;
  queue wait_list;
};

struct cpu {
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

typedef struct {
  handler_t handler;
  int event;
  int seq;
} handler_s;
