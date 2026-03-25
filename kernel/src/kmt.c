#include <os.h>


static task_t *current[MAX_CPU];
static task_t *cpu_idle[MAX_CPU];
static cpu_t *cpus[MAX_CPU];
static queue ready_queue;

static task_t *to_be_awakened = NULL; //awaken list head
static spinlock_t awakened_lock;

static task_t *last_task[MAX_CPU];

static void queue_init(queue* q, const char* name) {
  q->front = NULL;
  q->rear = NULL;
  kmt->spin_init(&q->lock, name);
}
//pop a task from queue and return it, if queue is empty, return NULL
static task_t *dequeue(queue* q) {
  if(q->front) {
    task_t *next = q->front;
    q->front = next->next;
    if(q->front == NULL)
      q->rear = NULL;

    next->next = NULL;
    return next;
  }
  return NULL;
}

//push a task to queue
static void enqueue(queue *q, task_t* task) {
  task->next = NULL;
  if(q->front == NULL) {
    q->front = task;
    q->rear = task;
  } 
  else {
    q->rear->next = task;
    q->rear = task;
  }
}

static void handle_pre_task() {

  task_t *prev = current[cpu_current()];
  task_t *safe_to_enqueue = last_task[cpu_current()];

  if(safe_to_enqueue) {
    if (safe_to_enqueue->state == READY && safe_to_enqueue != cpu_idle[cpu_current()]) {
      enqueue(&ready_queue, safe_to_enqueue);
    }
    else if (safe_to_enqueue->state == BLOCKING){
      safe_to_enqueue->state = BLOCKED;
    }
    last_task[cpu_current()] = NULL;
  }

  if (prev->state == RUNNING) {
      prev->state = READY;
  }
  else if(prev->state == BLOCKING) {
    // assert(ev.event == EVENT_YIELD);
  }
  else {
    panic("wrong entry");
  }
  last_task[cpu_current()] = prev;
}
static task_t* pick_next_task() {
  //pop a task from ready queue, 
  //if queue is empty, return NULL, next task will be idle task 
  task_t *next = dequeue(&ready_queue);
  while(next != NULL && next->state == DEAD) {  //if task has treedown, just pop from ready queue
    next = dequeue(&ready_queue);
  }
  if(next == NULL) {
    next = cpu_idle[cpu_current()];
  }
  else {
    assert(next->state == READY);
  }
  //set next state
  next->state = RUNNING;
  // printf("c%d next task:%s\n",cpu_current(), next->name);
  return next;
}

//awakened the task in to_be_awakened,make sure this task is BLOCKED
static void awaken() {
  //get the awakened_list
  kmt->spin_lock(&awakened_lock);
  task_t *t = to_be_awakened;
  to_be_awakened = NULL; 

  while(t) {
    task_t *next = t->next;
    if(t->state == BLOCKED) {
      t->next = NULL;
      t->state = READY;
      enqueue(&ready_queue, t);

    } else {
      assert(t->state == BLOCKING);
      t->next = to_be_awakened;
      to_be_awakened = t;
    }
    t = next;
  }
  kmt->spin_unlock(&awakened_lock);
}
/*
  three main work in scheduler: 
  handle pre_task(the trap stack is in pre task, so it should be careful),
  awaken, the sem_signal awaken some waiting task, it will be manager in there
  pick next task, pick from the ready queue  
*/
static Context* kmt_schedule(Event ev, Context *context) {
  if(ev.event == EVENT_IRQ_TIMER || ev.event == EVENT_YIELD) {

    //ready queue can only be modified in here and kmt_create
    kmt->spin_lock(&ready_queue.lock);

    handle_pre_task();
    awaken();
 
    task_t *next = pick_next_task();
    current[cpu_current()] = next;

    kmt->spin_unlock(&ready_queue.lock);
    //ATTENTION!!!
    //Here could cause a huge bug
    //the ready queue lock have been released,
    //some other cpu can run the scheduler at the same time 
    //if some cpu get pre task, and start run it 
    //then, the two cpu run in same stack space
    //the stack will crash

    // for (int volatile i = 0; i < 100000; i++) ;
    return next->context;
  }
  else {
    return context;
  }
}
static Context* kmt_context_save(Event ev, Context *context) {

  task_t *cur = current[cpu_current()];
  assert(cur->state == RUNNING || cur->state == BLOCKING);
  cur->context = context;
    
  return NULL;
}
task_t* mytask() {
  return current[cpu_current()];
}
//error handler, detect stack overflow and error interrupt
static Context* error_detect(Event ev, Context *context) {

  if(mytask() != cpu_idle[cpu_current()])
    //canaries to make sure there are no stack overflow
    panic_on(*(uint32_t*)(mytask()->stack.start) != 0xdeadbeef, "kmt stack overflow");

  if(ev.event == EVENT_ERROR||ev.event == EVENT_PAGEFAULT)
    panic("fault");

  return NULL;
}
/*
  kernel multithread init, add the schedule, error handler etc. to interrupt function
  the idle task struct don't have context after init, it will be filled when schedule first.

*/
static void kmt_init() {
  
  os->on_irq(INT_MAX - 1, EVENT_NULL, kmt_schedule);
  os->on_irq(INT_MIN + 1, EVENT_NULL, kmt_context_save);
  os->on_irq(INT_MIN, EVENT_NULL, error_detect);
  os->on_irq(INT_MAX, EVENT_NULL, error_detect);

  //initial the idle task and cpus 
  for(int i = 0;i < cpu_count();i++ ) {
    task_t* idle = (task_t *)pmm->alloc(sizeof(task_t));
    idle->name = "per-cpu idle task";
    idle->state = RUNNING;
    kmt->spin_init(&idle->lock, "idle task lock");
    idle->next = NULL;

    current[i] = idle;
    cpu_idle[i] = idle;

    cpu_t *cpu = pmm->alloc(sizeof(cpu_t));
    cpu->intena = 0;
    cpu->noff = 0;
    cpus[i] = cpu;
  }

  queue_init(&ready_queue, "ready queue");
  kmt->spin_init(&awakened_lock, "awakened list");

  printf("kmt init succeed\n");

}
/*
  init the task struct, alloc the stack memory, you should allocate memory for task struct before calling the function.
*/
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg) {
  task->name = name;

  task->stack.start = pmm->alloc(STACK_SIZE);
  *(uint32_t*)(task->stack.start) = 0xdeadbeef;   //add magic number in the top of stack
  task->stack.end = (void*)(task->stack.start + STACK_SIZE);

  task->context = kcontext(task->stack, entry, arg);
  task->state = READY;
  task->next = NULL;

  kmt->spin_init(&task->lock, name);

  kmt->spin_lock(&ready_queue.lock);
  enqueue(&ready_queue, task);
  kmt->spin_unlock(&ready_queue.lock);

  printf("kmt create task:%s\n",name);
  return true;
}

static void kmt_teardown(task_t *task) {

  task->state = DEAD;
  pmm->free(task->stack.start);
}

/*
  push off and pop off, manage the interrupt
*/
static void push_off() {
  int old = ienabled();  
  iset(false);            // disable interrupt
  asm volatile("" ::: "memory");
  cpu_t *cpu = cpus[cpu_current()];
  
  assert(cpu == cpus[cpu_current()]);
  assert(!ienabled()); 

  if (cpu->noff == 0) {
      cpu->intena = old;  // record the outside state
  }

  cpu->noff++;
}

static void pop_off() {
  cpu_t *cpu = cpus[cpu_current()];
  assert(!ienabled());    //assert interrupt disable
  assert(cpu->noff > 0);

  cpu->noff--;

  if (cpu->noff == 0 && cpu->intena) {
      iset(true);          // recover 
  }
}
/*
  spinlock, strong lock, aquire the lock will disable the interrrupt
*/
static void spin_init(spinlock_t *lk, const char *name) {
  lk->name = name;
  lk->locked = 0;
}
static void spin_lock(spinlock_t *lk) {
  push_off();
  while (atomic_xchg(&lk->locked, 1)); 
}
static void spin_unlock(spinlock_t *lk) {
  atomic_xchg(&lk->locked, 0); 
  pop_off();
}

/*
  semaphore
*/
static void sem_init(sem_t *sem, const char *name, int value) {
  sem->name = name;
  sem->count = value;
  queue_init(&sem->wait_list, name);

  kmt->spin_init(&sem->lock, name);
}

static void sem_wait(sem_t *sem) {
  kmt->spin_lock(&sem->lock); //aquire sem spinlock
  sem->count--; 
  if (sem->count < 0) {
    task_t *cur = current[cpu_current()];
    assert(cur->state == RUNNING);

    //Use the BLOCKING state to ensure that the task does not prematurely enter the ready queue
    //this task will really blocked in schedule
    cur->state = BLOCKING;

    enqueue(&sem->wait_list, cur);
    
    kmt->spin_unlock(&sem->lock);
    //ATTENTION!!!
    //There could cause a huge bug:
    //if another cpu run sem_signal here, the blocked state will change to ready and enqueue to ready queue
    //then, another cpu trap and get a ready task, this task can be assigned,
    //and in the same time, this function haven't yield, there will have two cpu run one task, crash! 
    yield();
  }
  else {
    kmt->spin_unlock(&sem->lock);
  }
}

static void sem_signal(sem_t *sem) {
  kmt->spin_lock(&sem->lock);
  sem->count++;

  if (sem->count <= 0) {
    //chose one wait task
    task_t *t = dequeue(&sem->wait_list);
    assert(t);

    //add this task to awakened list,this task will be awakened in schedule
    kmt->spin_lock(&awakened_lock);
    t->next = to_be_awakened;
    to_be_awakened = t;
    kmt->spin_unlock(&awakened_lock);

  }
  kmt->spin_unlock(&sem->lock);
}


MODULE_DEF(kmt) = {
 .init = kmt_init,
 .create = kmt_create,
 .teardown = kmt_teardown,
 .spin_init = spin_init,
 .spin_lock = spin_lock,
 .spin_unlock = spin_unlock,
 .sem_init = sem_init,
 .sem_wait = sem_wait,
 .sem_signal = sem_signal,
};
