#include <common.h>


/*
  slow path: buddy, alloc page
  fast path: slab, alloc small size memory
*/
#define BUDDY_AND_SLAB



#ifdef BUDDY_AND_SLAB

#define PAGE_ORDER 16    //page_size: 64kB
#define MAX_ORDER 26     //buddy total memory 64MB

#define MAX_KALLOC_ORDER 24   //max kalloc size 16MB

//cache size 32B -> 4KB
#define MIN_SLAB_ORDER 5
#define MAX_SLAB_ORDER 12

uintptr_t heap_base = 0x4000000;

#define NUM_BLOCKS (1 << MAX_ORDER) / (1 << PAGE_ORDER)
#define MAX_CPU 8

typedef struct _block {
  struct _block *next;
} block;

//buddy page data
static uint8_t block_order[NUM_BLOCKS];
static block *free_lists[MAX_ORDER + 1];
int page_lock = 0;

/*
  slab head struct, include cache(32B -> 4KB), freelist head, next slab pointer
  struct size is 26B 
  slab size is 1 page: 64KB
*/
typedef struct _slab {
  block head;
  struct _slab *next;
  short slab_order;
  short inuse;
  short owner_cpu;
  unsigned int magic_num;
} slab;

/*
  per cpu cache, include its own cache slab and cache lock
*/
typedef struct {
  slab *slab_list_head[MAX_SLAB_ORDER + 1];  // slab_list_head[order] is a permanent anchor slab, never freed
  int cache_lock;
} Cache;

//define the cpu cache
static Cache cpu_cache[MAX_CPU];


/*
  get the order, you can change the min order
*/
static int size_to_order(size_t s) {
  int order = MIN_SLAB_ORDER;
  size_t size = 1 << order;
  while (size < s) {
      size <<= 1;
      order++;
  }
  return order;
}


//page alloc
static void *pgalloc(int order) {
  lock(&page_lock);
  
  int cur = order;

  //find a order contain the order in need
  while (cur <= MAX_ORDER && free_lists[cur] == NULL) {
      cur++;
  }
  if (cur > MAX_ORDER) {
    printf("heap out of memory\n");
    unlock(&page_lock);
    return NULL;
  }
  block *b = free_lists[cur];
  free_lists[cur] = b->next;
  
  //the address return 
  uintptr_t addr = (uintptr_t)b;
  //cut the meeory to order 
  while (cur > order) {
    cur--;
    uintptr_t buddy_addr = addr + (1UL << cur);

    block *buddy = (block *)buddy_addr;
    buddy->next = free_lists[cur];
    free_lists[cur] = buddy;

    int buddy_idx = (buddy_addr - heap_base) >> PAGE_ORDER;
    block_order[buddy_idx] = cur;
  }
  int idx = (addr - heap_base) >> PAGE_ORDER;
  block_order[idx] = order;

  printf("pgalloc address: %p, order: %d \n",(void*)b, (int)order);

  unlock(&page_lock);
  return (void*)b;
}

//page free 
static void pgfree(void *ptr) {
  lock(&page_lock);
  
  uintptr_t addr = (uintptr_t)ptr;
  int idx = (addr - heap_base) >> PAGE_ORDER;
  int order = block_order[idx];

  while(order < MAX_ORDER) {

    //get the buddy page index
    int buddy_idx = idx ^ (1UL << (order - PAGE_ORDER));
    uintptr_t buddy_addr = ((uintptr_t)buddy_idx << PAGE_ORDER) + heap_base;

    block *prev = NULL;
    block *cur = free_lists[order];

    //find the buddy in the freelist to ensure buddy is exist
    while (cur) {
      if ((uintptr_t)cur == buddy_addr)
          break;
      prev = cur;
      cur = cur->next;
    }

    if (!cur) {
      //buddy is not free, the curr is null
        break;
    }

    //delete the buddy node
    if (prev)
      prev->next = cur->next;
    else
      free_lists[order] = cur->next;

    //ensure the addr is the left one
    if (buddy_addr < addr) {
        addr = buddy_addr;
        idx = buddy_idx;
    }
    order++; 
  }

  block *b = (block *)addr;
  b->next = free_lists[order];
  free_lists[order] = b;

  block_order[idx] = order;

  printf("pgfree address: %p, order: %d\n", (void *)addr, order);
  unlock(&page_lock);
}


static void *create_slab(short order, int owner_cpu) {
  void *ptr = pgalloc(PAGE_ORDER);

  if(ptr == NULL) {
    return NULL;
  }
  slab *slab_base = (slab *)ptr;

  //parameter set
  slab_base->slab_order = order;
  slab_base->inuse = 0;
  slab_base->next = NULL;
  slab_base->owner_cpu = owner_cpu;
  slab_base->magic_num = 0xdeadbeef;
  
  //creat the freelist
  block *node = &slab_base->head;
  uintptr_t node_addr = (uintptr_t)ptr;

  for(int j = 1;j < (1UL << (PAGE_ORDER - order)) ; j++) {
    uintptr_t next_addr = node_addr + (1UL << order);
    block *next = (block *)next_addr;
    node->next = next;
    
    node_addr = next_addr;
    node = next;
  }
  node->next = NULL;

  return slab_base;
}

static void *slab_alloc(int order, int cpu_current) {
  lock(&cpu_cache[cpu_current].cache_lock);

  slab *slab_base = cpu_cache[cpu_current].slab_list_head[order];

  block *free_node = slab_base->head.next;

  //if free_node is NULL,this slab is full,find next slab
  while(free_node == NULL) {
    //if slab is all full
    if(slab_base->next == NULL) {

      //alloc a new page and add in the slab list
      slab* new_base = create_slab(order, cpu_current);
      //if page if all used, return NULL 
      if(new_base == NULL) {
        return NULL;
      }

      //insert the new slab to the tail
      slab_base->next = new_base;
      
      //pick the freenode
      slab_base = new_base;
      free_node = slab_base->head.next;
      break;
    }
    slab_base = slab_base->next;
    free_node = slab_base->head.next;
  }


  slab_base->head.next = free_node->next; 
  slab_base->inuse += 1;

  printf("slab alloc address: %p, order: %d, slab in use: %d \n",(void*)free_node, (int)order, slab_base->inuse);

  unlock(&cpu_cache[cpu_current].cache_lock);
  return (void*)free_node;
}


static int slab_free(void *ptr) {
  block *node = (block*)ptr;
  
  //find the base address of this slab
  uintptr_t slab_base_addr = (uintptr_t)ptr & ~((1UL << PAGE_ORDER) -1);
  slab *slab_base = (slab *)slab_base_addr;
  
  //find which cpu cache this memory belong to
  short owner_cpu = slab_base->owner_cpu;

  lock(&cpu_cache[owner_cpu].cache_lock);
  
  //if the magic number is wrong, the ptr is point to a page, not slab, return -1 to free page 
  if(slab_base->magic_num != 0xdeadbeef) {
    unlock(&cpu_cache[owner_cpu].cache_lock);
    return -1;
  }

  node->next = slab_base->head.next;
  slab_base->head.next = node;

  slab_base->inuse -= 1;

  printf("slab free address: %p, order: %d, slab in use: %d \n",(void*)node, (int)slab_base->slab_order, slab_base->inuse);

  //slab is empty,try to free this page 
  if(slab_base->inuse == 0) {
    short order = slab_base->slab_order;

    //if this slab is the head slab
    if(slab_base != cpu_cache[owner_cpu].slab_list_head[order]) {

      //this slab is not head, free it in the list
      slab* pre_slab = cpu_cache[owner_cpu].slab_list_head[order];
      while(pre_slab && pre_slab->next != slab_base) {    //find the pre slab of base slab
        pre_slab = pre_slab->next;
      }
      if(pre_slab) {
        pre_slab->next = slab_base->next;
      }

      //change the magic_num
      slab_base->magic_num = 0;
      pgfree(slab_base);
    }
  }
  unlock(&cpu_cache[owner_cpu].cache_lock);
  return 0;
}


/*
  this page_init function is not completed, i use a fixed heap_base as the start of the heap
  if the memory is not aligned, for example the real physical mem is  96MB, it will waste 32MB
*/
static void page_init() {
  for(int i = PAGE_ORDER;i <= MAX_ORDER; i++){
    free_lists[i] = NULL;
  }

  block *base = (block *)heap_base;
  base->next = NULL;
  free_lists[MAX_ORDER] = base;

}

static void *kalloc(size_t size) {
  int order = size_to_order(size);
  if(order <= MAX_SLAB_ORDER) {
    return slab_alloc(order, cpu_current());
  }
  else if(order <= MAX_KALLOC_ORDER) {              //the most allow memory alloc is 16MB
    order = order > PAGE_ORDER ? order:PAGE_ORDER;
    return pgalloc(order);
  }
  else {
    printf("please alloc less memory\n");
    return NULL;
  }
}

static void kfree(void *ptr) {
  if((uintptr_t)ptr < heap_base || (uintptr_t)ptr > (uintptr_t)heap.end) {
    printf("ERROR:Pointer Out of Heap Range!\n");
    return ;
  }
  if(slab_free(ptr) == -1) {
    pgfree(ptr);
  }
}

#endif



static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

#ifdef BUDDY_AND_SLAB

  printf("page slab init\n");

  page_init();

  //assert the size of slab structer smaller than the slab block
  assert(size_to_order(sizeof(slab)) <= MIN_SLAB_ORDER); 

  //create slab cache for each cpu
  for(int i = 0;i < cpu_count(); i++) {

    //set the cache lock open
    cpu_cache[i].cache_lock = 0; 

    //slab init, creat slab
    for(short order = MIN_SLAB_ORDER; order <= MAX_SLAB_ORDER; order++) {
      cpu_cache[i].slab_list_head[order] = create_slab(order, i);
    }
  }

  printf("inital succeed\n\n"); 

#endif
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
