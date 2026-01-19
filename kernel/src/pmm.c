#include <common.h>


//choose one pmm algorithm
// #define FREELIST 
#define BUDDY




#ifdef BUDDY

#define MIN_ORDER 3
#define MAX_ORDER 26

#define NUM_BLOCKS (1 << MAX_ORDER) / (1 << MIN_ORDER)

typedef struct _block {
  struct _block *next;
} block;


// static uint8_t block_order[NUM_BLOCKS];
static block *free_lists[MAX_ORDER + 1];

static int size_to_order(size_t s) {
  int order = MIN_ORDER;
  size_t size = 1 << order;
  while (size < s) {
      size <<= 1;
      order++;
  }
  return order;
}


static void *kalloc(size_t size) {
  int order = size_to_order(size);
  int cur = order;
  while (cur <= MAX_ORDER && free_lists[cur] == NULL) {
      cur++;
  }
  if (cur > MAX_ORDER) {
    printf("heap out of memory\n");
    return NULL;
  }
  block *b = free_lists[cur];
  free_lists[cur] = b->next;

  while (cur > order) {
    cur--;
    uintptr_t addr = (uintptr_t)b;
    uintptr_t buddy = addr + (1UL << cur);

    block *bb = (block *)buddy;
    bb->next = free_lists[cur];
    free_lists[cur] = bb;
  }
  printf("kalloc address: %p, size: %d \n",(void*)b, (int)size);
  return (void*)b;
}



#else
typedef struct {
  int size;
  uintptr_t magic;
} header_t;

typedef struct __node_t {
  int size;
  struct __node_t *next;
} node_t;

static node_t* head;

static void *kalloc(size_t size) {
  int ptr_size = 1;
  node_t* node = head;
  while(ptr_size < size)ptr_size <<= 1;
  while(1){
    if(node->size >= size){
      uintptr_t block_begin = (uintptr_t)node + sizeof(node_t);
      uintptr_t ptr = (block_begin + ptr_size - 1) & ~(ptr_size - 1);
      if(ptr == block_begin){
        node = 
      }
      if(node->size >= size + ptr - (uintptr_t)node){

      }
    }
  }

  printf("kalloc address: %p, size: %d \n",(void*)b, (int)size);
  return NULL;
}
#endif

static void kfree(void *ptr) {
}

static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

#ifdef BUDDY
  for(int i = MIN_ORDER;i <= MAX_ORDER; i++){
    free_lists[i] = NULL;
  }
  block *b = (block *)(0x4000000);
  b->next = NULL;
  free_lists[MAX_ORDER] = b;
#else
  node_t *head = heap.start;
  head->size = pmsize - sizeof(node_t);

#endif
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
