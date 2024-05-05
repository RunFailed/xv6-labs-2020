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

#define BUCKET_NUM 13   //Lab8:2
extern uint ticks;      //Lab8:2

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf hash_buf[BUCKET_NUM];
  struct spinlock bucket_lock[BUCKET_NUM];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b = bcache.buf;
    //初始化bcache锁
  initlock(&bcache.lock, "bcache");
  //初始化每个hash桶的锁
  for(int i = 0; i < BUCKET_NUM; i++)
  {
      char bucket_lock_name[10] = {'\0'};
      snprintf(bucket_lock_name, sizeof(bucket_lock_name), "bcache%d", i);
      initlock(&bcache.bucket_lock[i], bucket_lock_name);
  }

  //初始化每个buffer的锁，并把每个buffer分配到对应的hash桶中
  for(int i = 0; b < bcache.buf + NBUF; b++, i++)
  {
      char buffer_lock_name[10] = {'\0'};
      snprintf(buffer_lock_name, sizeof(buffer_lock_name), "buffer%d", i);
      initsleeplock(&b->lock, buffer_lock_name);
      int bktidx = i % BUCKET_NUM;
      b->next = bcache.hash_buf[bktidx].next;
      bcache.hash_buf[bktidx].next = b;
  }

//  // Create linked list of buffers
//  //环形双向链表，每次插入到head之后
//  bcache.head.prev = &bcache.head;
//  bcache.head.next = &bcache.head;
//  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
//    b->next = bcache.head.next;
//    b->prev = &bcache.head;
//    initsleeplock(&b->lock, "buffer");
//    bcache.head.next->prev = b;
//    bcache.head.next = b;
//  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

    //Lab8:2
//  acquire(&bcache.lock);
//
//  // Is the block already cached?
//  for(b = bcache.head.next; b != &bcache.head; b = b->next){
//    if(b->dev == dev && b->blockno == blockno){
//      b->refcnt++;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }

//Lab8:2
    int bucketidx = blockno % BUCKET_NUM;
    acquire(&bcache.bucket_lock[bucketidx]);

    //目标块在缓存中，直接返回
    // Is the block already cached?
    for(b = bcache.hash_buf[bucketidx].next; b != 0; b = b->next){
        if(b->dev == dev && b->blockno == blockno){
            b->refcnt++;
            b->tick = ticks;
            release(&bcache.bucket_lock[bucketidx]);
            acquiresleep(&b->lock);
            return b;
        }
    }
    release(&bcache.bucket_lock[bucketidx]);

    //目标块不在缓存中，淘汰最久未使用的块，并把目标块读入缓存
    acquire(&bcache.lock);
    uint mintick = -1;
    struct buf *minb = 0, *prevb = 0, *minprevb = 0;
    int idx = bucketidx;
    for(int i = 0; i < BUCKET_NUM; i++)
    {
        if(++idx == BUCKET_NUM) idx = 0;
        acquire(&bcache.bucket_lock[idx]);
        for(prevb = &bcache.hash_buf[idx], b =prevb->next; b != 0; prevb = b, b = b->next)
        {
            if(idx == bucketidx && b->dev == dev && b->blockno == blockno){
                b->refcnt++;
                b->tick = ticks;
                release(&bcache.bucket_lock[idx]);
                release(&bcache.lock);
                acquiresleep(&b->lock);
                return b;
            }
            if(b->refcnt == 0 && b->tick < mintick)
            {
                mintick = b->tick;
                minb = b;
                minprevb = prevb;
            }
        }
        release(&bcache.bucket_lock[idx]);
    }
    //如果缓存中有可淘汰的空闲块
    if(minb)
    {
        int minbidx = minb->blockno % BUCKET_NUM;
        acquire(&bcache.bucket_lock[minbidx]);
        minb->dev = dev;
        minb->blockno = blockno;
        minb->valid = 0;
        minb->refcnt = 1;
        minb->tick = ticks;
        //如果可淘汰的空闲块和待读入的块不在一个bucket
        //则把即将淘汰的空闲块移到待读入的块所在的bucket
        if(minbidx != bucketidx)
        {
            acquire(&bcache.bucket_lock[bucketidx]);
            minprevb->next = minb->next;
            minb->next = bcache.hash_buf[bucketidx].next;
            bcache.hash_buf[bucketidx].next = minb;
            release(&bcache.bucket_lock[bucketidx]);
        }
        release(&bcache.bucket_lock[minbidx]);
        release(&bcache.lock);
        acquiresleep(&minb->lock);
        return minb;
    }
    release(&bcache.lock);




//  // Not cached.
//  // Recycle the least recently used (LRU) unused buffer.
//  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
//    if(b->refcnt == 0) {
//      b->dev = dev;
//      b->blockno = blockno;
//      b->valid = 0;
//      b->refcnt = 1;
//      release(&bcache.lock);
//      acquiresleep(&b->lock);
//      return b;
//    }
//  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
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

  //acquire(&bcache.lock);
    int bucketidx = b->blockno % BUCKET_NUM;
    acquire(&bcache.bucket_lock[bucketidx]);
  b->refcnt--;
    b->tick = ticks;    //Lab8:2
//  if (b->refcnt == 0) {
//    // no one is waiting for it.
////    b->next->prev = b->prev;
////    b->prev->next = b->next;
////    b->next = bcache.head.next;
////    b->prev = &bcache.head;
////    bcache.head.next->prev = b;
////    bcache.head.next = b;
////    b->tick = ticks;    //Lab8:2
//  }
    release(&bcache.bucket_lock[bucketidx]);
  //release(&bcache.lock);
}

//Lab8:2
void
bpin(struct buf *b) {
  //acquire(&bcache.lock);
  int bucketidx = b->blockno % BUCKET_NUM;
    acquire(&bcache.bucket_lock[bucketidx]);
  b->refcnt++;
    release(&bcache.bucket_lock[bucketidx]);
  //release(&bcache.lock);
}

//Lab8:2
void
bunpin(struct buf *b) {
  //acquire(&bcache.lock);
    int bucketidx = b->blockno % BUCKET_NUM;
    acquire(&bcache.bucket_lock[bucketidx]);
  b->refcnt--;
    release(&bcache.bucket_lock[bucketidx]);
  //release(&bcache.lock);
}


