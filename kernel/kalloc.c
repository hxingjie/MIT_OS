// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// kernel base: 0x8000 0000
// phystop:     0x8800 0000
// 0x800 0000 byte -> 0x8000 page -> 2^15 page
// 使用 2^15 个 uint 记录 2^15 个页面的使用情况

// 注意区分不同进制

/*
2^0 dummy -> chunk(4kb) -> chunk(4kb)
2^1 dummy -> chunk(8kb) -> chunk(8kb)
2^2 dummy -> chunk(16kb) -> chunk(16kb)
2^3 dummy -> chunk(32kb) -> chunk(32kb)
*/

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct page {
  struct page* prev;
  struct page* next;
  int order;
};

struct {
  struct spinlock lock;
  struct page* freelist[4]; // 2^0, 2^1, 2^2, 2^3
} kmem;

// my func
#define MAX_ORDER 3
#define PA2IDX(pa) (pa-KERNBASE) / PGSIZE
uint8 record[32768]; // 2^15 byte -> 2^15 / 2^12 = 2^3 page
struct page* get_buddy_chunk(uint64 chunk, int order) {
    uint64 buddy_chunk;
    if ((chunk & 1 << (12 + order)) == 0) {
        buddy_chunk = chunk | 1 << (12+ order);
    } else {
        buddy_chunk = chunk & ~(1 << (12 + order));
    }

    if (buddy_chunk >= PHYSTOP) {
        return 0;
    } else {
        return (struct page*)buddy_chunk;
    }
}
void insert_list(struct page* chunk) {
    // must has kmem's lock
    int order = chunk->order;
    //acquire(&kmem.lock[order]);

    if (kmem.freelist[order] != 0) {
        chunk->prev = kmem.freelist[order]->prev;
        chunk->next = kmem.freelist[order];
        
        kmem.freelist[order]->prev->next = chunk;
        kmem.freelist[order]->prev = chunk;
        kmem.freelist[order] = chunk;
    } else {
        chunk->prev = chunk;
        chunk->next = chunk;
        kmem.freelist[order] = chunk;
    }

    //release(&kmem.lock[order]);
}
void delete_list(struct page* chunk) {
    // must has kmem's lock
    int order = chunk->order;
    //acquire(&kmem.lock[order]);

    if (chunk->next == chunk) { // only one
        kmem.freelist[order] = 0;
    } else {
        chunk->prev->next = chunk->next;
        chunk->next->prev = chunk->prev;
        if (kmem.freelist[order] == chunk) {
            kmem.freelist[order] = chunk->next;
        }
    }

    //release(&kmem.lock[order]);

    // reset
    chunk->prev = 0;
    chunk->next = 0;
}
void print_freelist() {
    for (int i = 0; i <= MAX_ORDER; i++) {
        printf("======== %d ========\n", i);
        struct page* tmp = kmem.freelist[i];
        if (tmp == 0) {
            printf("order[%d] is null\n", i);
            printf("======== %d ========\n", i);
            continue;
        }

        struct page* ptr = tmp;
        while (ptr->next != tmp) {
            printf("%p -> ", ptr->next);
            ptr = ptr->next;
        }
        printf("%p.\n", ptr->next);
        printf("======== %d ========\n", i);
    }
    printf("\n\n");
}
struct page* merge_chunk(struct page* chunk) {
    // must has kmem's lock
    // input: free chunk(not in freelist)
    // output: no need to merge, can insert to list
    if (chunk->order == MAX_ORDER) {
        return chunk;
    }

    struct page* buddy_chunk = get_buddy_chunk((uint64)chunk, chunk->order);
    if (buddy_chunk == 0) {
        // 不存在buddy, 越界
        return chunk;

    } else if (record[PA2IDX((uint64)buddy_chunk)] > 0) {
        // 该页面不在空闲链表, 那么无论其order
        return chunk;
        
    } else if (chunk->order != buddy_chunk->order) {
        // record[PA2IDX(buddy_chunk)] == 0
        // 对应的buddy pages 大小不同（已经被split了）
        return chunk;

    } else {
        // 1.del buddy from list
        delete_list(buddy_chunk);

        // 2.merge to more page(choose less one)
        chunk = (uint64)chunk < (uint64)buddy_chunk ? chunk : buddy_chunk;
        chunk->order += 1;

        // continue merge
        return merge_chunk(chunk);
    }
}
struct page* split_chunk(struct page* chunk, int order) {
    // must has kmem's lock
    // input: free chunk(not in freelist)
    // output: no need to split

    if (chunk->order == 0) {
        return chunk;
    }

    chunk->order -= 1;

    struct page* buddy_chunk = get_buddy_chunk((uint64)chunk, chunk->order);
    // init buddy_chunk
    buddy_chunk->order = chunk->order;
    buddy_chunk->prev = 0;
    buddy_chunk->next = 0;

    insert_list(buddy_chunk);
    return split_chunk(chunk, order);
}
// my func

void
kinit()
{
  initlock(&kmem.lock, "kmem");
//   for (int i = 0; i < 4; i++) {
//     initlock(&kmem.lock[i], "kmem");
//   }

  for (int i = 0; i < 4; i++) {
    kmem.freelist[i] = 0;
  }
  
  uint64 tmp = (uint64)end;
  tmp = PGROUNDUP(tmp);
  for (int i = PA2IDX(tmp); i < 32768; i++) {
    record[i] = 1;
  }
  
  printf("kinit\n");
  freerange(end, (void*)PHYSTOP);
  printf("kinit finish\n");
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
  }
    
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  int order = 0;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
 
  // init chunk
  struct page* chunk = (struct page*)pa;
  chunk->order = order;
  chunk->prev = 0;
  chunk->next = 0;

  // update record
  uint cnt = 1;
  for (int i = 0; i < order; i++)
    cnt *= 2;
  int idx = PA2IDX((uint64)chunk);
  for (int i = 0; i < cnt; i++) {
    record[idx] = 0;
    idx += 1;
  }

  acquire(&kmem.lock);

  chunk = merge_chunk(chunk);
  //printf("insert list, %p\n", chunk);
  insert_list(chunk);

  //print_freelist();
  
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void * kalloc(void) {
    int order = 0;

    acquire(&kmem.lock);

    struct page* chunk = 0;
    for (int cur = order; cur <= MAX_ORDER; cur += 1) {
        //acquire(&kmem.lock[cur]);

        if (kmem.freelist[cur] != 0) { // find the chunk
            chunk = kmem.freelist[cur];
            //release(&kmem.lock[cur]);

            delete_list(chunk); // free from list
            chunk = split_chunk(chunk, order); // split to order chunk
            break;
        } else {
            //release(&kmem.lock[cur]);
        }
    }

    if (chunk != 0) {
        uint cnt = 1;
        for (int i = 0; i < order; i++)
            cnt *= 2;
        memset((char*)chunk, 5, cnt * PGSIZE); // fill with junk

        record[PA2IDX((uint64)chunk)] = 1;
    }

    release(&kmem.lock);
    return (void*)chunk;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
my_kfree(void *pa, int order)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
 
  // init chunk
  struct page* chunk = (struct page*)pa;
  chunk->order = order;
  chunk->prev = 0;
  chunk->next = 0;

  // update record
  uint cnt = 1;
  for (int i = 0; i < order; i++)
    cnt *= 2;
  int idx = PA2IDX((uint64)chunk);
  for (int i = 0; i < cnt; i++) {
    record[idx] = 0;
    idx += 1;
  }

  acquire(&kmem.lock);

  chunk = merge_chunk(chunk);
  //printf("insert list, %p\n", chunk);
  insert_list(chunk);

  //print_freelist();
  
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void * my_kalloc(int order) {
    acquire(&kmem.lock);

    struct page* chunk = 0;
    for (int cur = order; cur <= MAX_ORDER; cur += 1) {
        //acquire(&kmem.lock[cur]);

        if (kmem.freelist[cur] != 0) { // find the chunk
            chunk = kmem.freelist[cur];
            //release(&kmem.lock[cur]);

            delete_list(chunk); // free from list
            chunk = split_chunk(chunk, order); // split to order chunk
            break;
        } else {
            //release(&kmem.lock[cur]);
        }
    }

    if (chunk != 0) {
        uint cnt = 1;
        for (int i = 0; i < order; i++)
            cnt *= 2;
        memset((char*)chunk, 5, cnt * PGSIZE); // fill with junk

        record[PA2IDX((uint64)chunk)] = 1;
    }

    release(&kmem.lock);
    return (void*)chunk;
}

// // Physical memory allocator, for user processes,
// // kernel stacks, page-table pages,
// // and pipe buffers. Allocates whole 4096-byte pages.

// #include "types.h"
// #include "param.h"
// #include "memlayout.h"
// #include "spinlock.h"
// #include "riscv.h"
// #include "defs.h"

// // kernel base: 0x8000 0000
// // phystop:     0x8800 0000
// // 0x800 0000 byte -> 0x8000 page -> 2^15 page
// // 使用 2^15 个 uint 记录 2^15 个页面的使用情况
// uint8 record[32768]; // 2^15 byte -> 2^15 / 2^12 = 2^3 page
// // 注意区分不同进制

// /*
// 2^0 dummy -> chunk(4kb) -> chunk(4kb)
// 2^1 dummy -> chunk(8kb) -> chunk(8kb)
// 2^2 dummy -> chunk(16kb) -> chunk(16kb)
// 2^3 dummy -> chunk(32kb) -> chunk(32kb)
// */

// void freerange(void *pa_start, void *pa_end);

// extern char end[]; // first address after kernel.
//                    // defined by kernel.ld.

// struct run {
//   struct run *next;
// };

// struct {
//   struct spinlock lock;
//   struct run *freelist;
// } kmem;

// void
// kinit()
// {
//   initlock(&kmem.lock, "kmem");
//   freerange(end, (void*)PHYSTOP);
// }

// void
// freerange(void *pa_start, void *pa_end)
// {
//   char *p;
//   p = (char*)PGROUNDUP((uint64)pa_start);
//   for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
//     kfree(p);
// }

// // Free the page of physical memory pointed at by v,
// // which normally should have been returned by a
// // call to kalloc().  (The exception is when
// // initializing the allocator; see kinit above.)
// void
// kfree(void *pa)
// {
//   struct run *r;

//   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
//     panic("kfree");

//   // Fill with junk to catch dangling refs.
//   memset(pa, 1, PGSIZE);

//   r = (struct run*)pa;

//   acquire(&kmem.lock);
//   r->next = kmem.freelist;
//   kmem.freelist = r;
//   release(&kmem.lock);
// }

// // Allocate one 4096-byte page of physical memory.
// // Returns a pointer that the kernel can use.
// // Returns 0 if the memory cannot be allocated.
// void *
// kalloc(void)
// {
//   struct run *r;

//   acquire(&kmem.lock);
//   r = kmem.freelist;
//   if(r)
//     kmem.freelist = r->next;
//   release(&kmem.lock);

//   if(r)
//     memset((char*)r, 5, PGSIZE); // fill with junk
//   return (void*)r;
// }
