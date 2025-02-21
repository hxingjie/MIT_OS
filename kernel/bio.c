// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

/*struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno); // get buf
  if(!b->valid) {
    virtio_disk_rw(b, 0); // 读磁盘
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}*/

#define MYNUM 23
struct {
  //struct spinlock lock;
  struct buf buf[NBUF];

  struct spinlock bucket_lock[MYNUM];
  struct buf head[MYNUM];
} bcache;

void
binit(void)
{ 
  // init lock
  //initlock(&bcache.lock, "bcache"); // debug
  for (uint i = 0; i < MYNUM; i++) {
    initlock(&bcache.bucket_lock[i], "bcache");
  }

  // init bucket
  for (uint i = 0; i < MYNUM; i++) {
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  
  // init buf
  for (uint i = 0; i < NBUF; i++){
    struct buf* b = &bcache.buf[i];
    uint idx = i % MYNUM;
    
    // head insert
    b->next = bcache.head[idx].next;
    b->prev = &bcache.head[idx];
    initsleeplock(&b->lock, "buffer");
    bcache.head[idx].next->prev = b;
    bcache.head[idx].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint idx = blockno % MYNUM;

  //acquire(&bcache.lock); // debug
  acquire(&bcache.bucket_lock[idx]);

  // Is the block already cached?
  // front to back
  for (b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++; // add ref cnt

      release(&bcache.bucket_lock[idx]);
      //release(&bcache.lock); // debug

      acquiresleep(&b->lock);

      return b;
    }
  }

  release(&bcache.bucket_lock[idx]);

  // Not cached.
  // find unused buffer from back to front
  for (uint i = 0; i < MYNUM; i++) {
    acquire(&bcache.bucket_lock[i]);

    for (b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev){
      if(b->refcnt == 0) {
        // init buf
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // take out this buf
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.bucket_lock[i]);

        // head insert
        uint new_idx = b->blockno % MYNUM;
        acquire(&bcache.bucket_lock[new_idx]);
        b->next = bcache.head[new_idx].next;
        b->prev = &bcache.head[new_idx];
        bcache.head[new_idx].next->prev = b;
        bcache.head[new_idx].next = b;
        release(&bcache.bucket_lock[idx]);

        //release(&bcache.lock); // debug
        acquiresleep(&b->lock);
        return b;
      }
    }
    
    release(&bcache.bucket_lock[i]);
  }
  
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno); // get buf
  if(!b->valid) {
    virtio_disk_rw(b, 0); // 读磁盘
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  uint idx = b->blockno % MYNUM;
  releasesleep(&b->lock);

  //acquire(&bcache.lock); // debug
  acquire(&bcache.bucket_lock[idx]);

  b->refcnt--;
  if (b->refcnt == 0) { // no one is waiting for it.
    // take out this buf
    b->next->prev = b->prev;
    b->prev->next = b->next;

    // tail insert
    b->next = &bcache.head[idx];
    b->prev = bcache.head[idx].prev;
    bcache.head[idx].prev->next = b;
    bcache.head[idx].prev = b;
  }
  
  release(&bcache.bucket_lock[idx]);
  //release(&bcache.lock); // debug
}

void
bpin(struct buf *b) {
  uint idx = b->blockno % MYNUM;
  acquire(&bcache.bucket_lock[idx]);
  b->refcnt++;
  release(&bcache.bucket_lock[idx]);
}

void
bunpin(struct buf *b) {
  uint idx = b->blockno % MYNUM;
  acquire(&bcache.bucket_lock[idx]);
  b->refcnt--;
  release(&bcache.bucket_lock[idx]);
}


