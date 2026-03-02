#include <os.h>


static task_t *current[MAX_CPU];
static task_t *cpu_idle[MAX_CPU];
static cpu_t *cpus[MAX_CPU];
static queue ready_queue;

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

static task_t* pick_next_task(task_t *pre) {
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
  //judge if we should push previous task to queue
  //if previous state is RUNNING, not BLOCKED or others, set it state to READY
  //while previous is not idle task, push it to ready queue
  if(pre->state == RUNNING) {
    pre->state = READY;
    if(pre != cpu_idle[cpu_current()]) {
      enqueue(&ready_queue, pre);
    } 
  }
  //set next state
  next->state = RUNNING;
  // printf("c%d next task:%s\n",cpu_current(), next->name);
  return next;
}

static Context* kmt_schedule(Event ev, Context *context) {
  if(ev.event == EVENT_IRQ_TIMER || ev.event == EVENT_YIELD) {
    // if(ev.event == EVENT_IRQ_TIMER)
    kmt->spin_lock(&ready_queue.lock);
    task_t *prev = current[cpu_current()];
    
    //if prev is to be blocked, it is from sem_wait yield()
    if(prev->state == BLOCKING){
      assert(ev.event == EVENT_YIELD);
      prev->state = BLOCKED;
      kmt->spin_unlock(&prev->lock);
    }

    task_t *next = pick_next_task(prev);

    current[cpu_current()] = next;
    kmt->spin_unlock(&ready_queue.lock);
    return next->context;
  }
  else {
    return context;
  }
}
static Context* kmt_context_save(Event ev, Context *context) {

  task_t *cur = current[cpu_current()];
  assert(cur->state == RUNNING || cur->state == BLOCKING) ;
  cur->context = context;
    
  return NULL;
}

static void kmt_init() {
  
  os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
  os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);

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

  printf("kmt init succeed\n");

}

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg) {
  task->name = name;
  task->stack.start = pmm->alloc(STACK_SIZE);
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
  sem->wait_list = NULL;

  kmt->spin_init(&sem->lock, name);
}

static void sem_wait(sem_t *sem) {
  kmt->spin_lock(&sem->lock); //aquire sem spinlock
  sem->count--; 
  if (sem->count < 0) {
    task_t *cur = current[cpu_current()];
    assert(cur->state == RUNNING);

    kmt->spin_lock(&cur->lock);
    cur->state = BLOCKING;
    cur->next = sem->wait_list;
    sem->wait_list = cur;
    
    kmt->spin_unlock(&sem->lock);
    //ATTENTION!!!
    //there could cause a huge bug,
    //if another cpu run sem_sugnal here, the blocked state will change to ready  
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
    task_t *t = sem->wait_list;

    //remove from wait list
    sem->wait_list = t->next;
    
    kmt->spin_lock(&t->lock);
    kmt->spin_lock(&ready_queue.lock);
    
    assert(t->state == BLOCKED);
    t->state = READY;
    enqueue(&ready_queue, t);

    kmt->spin_unlock(&t->lock);
    kmt->spin_unlock(&ready_queue.lock);
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
