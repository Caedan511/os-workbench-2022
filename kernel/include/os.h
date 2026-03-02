#include <common.h>

#define STACK_SIZE 8192

struct spinlock {
  int locked;       // Is the lock held?
  // For debugging:
  const char *name;        // Name of lock.
};

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
  task_t *wait_list;
};
struct cpu {
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

typedef struct {
  spinlock_t lock;
  
  task_t *front;
  task_t *rear;
} queue;
