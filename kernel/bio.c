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
// bucket number for bufmap
#define NBUFMAP_BUCKET 13
// hash function for bufmap
#define BUFMAP_HASH(dev, blockno) ((((dev)<<27)|(blockno))%NBUFMAP_BUCKET)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
    // Hash map: dev and blockno to buf
  struct buf bufmap[NBUFMAP_BUCKET];
  struct spinlock bufmap_locks[NBUFMAP_BUCKET];
} bcache;

void
binit(void)
{
  // Initialize bufmap
  for(int i=0;i<NBUFMAP_BUCKET;++i)
  {
    initlock(&bcache.bufmap_locks[i],"bcache_bufmap");
    bcache.bufmap[i].next=0;
  }
  // Initialize buffers

  for(int i=0;i<NBUF;++i)
  {
    struct buf* b=&bcache.buf[i];
    initsleeplock(&b->lock,"buffer");
    b->lastuse=0;
    b->refcnt=0;
    b->next=bcache.bufmap[0].next;
    bcache.bufmap[0].next=b;
  }
  initlock(&bcache.lock, "bcache_lock");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key=BUFMAP_HASH(dev,blockno);
  acquire(&bcache.bufmap_locks[key]);

  // Is the block already cached?
  for(b = bcache.bufmap[key].next; b ; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 注意这里的 acquire 和 release 的顺序
  // 先释放 key 桶锁，防止查找驱逐时出现环路死锁
  // 获得驱逐锁，防止多个 CPU 同时驱逐影响后续判断

  release(&bcache.bufmap_locks[key]);
  acquire(&bcache.lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

    // **再次查找 blockno 的缓存是否存在**，若是直接返回，若否继续执行
  // 这里由于持有 eviction_lock，没有任何其他线程能够进行驱逐操作，所以
  // 没有任何其他线程能够改变 bufmap[key] 桶链表的结构，所以这里不事先获取
  // 其相应桶锁而直接开始遍历是安全的。

  for(b=bcache.bufmap[key].next;b;b=b->next)
  {
    if(b->dev == dev && b->blockno == blockno)
    {
      acquire(&bcache.bufmap_locks[key]);
      b->refcnt++;
      release(&bcache.bufmap_locks[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }


  // 缓存不存在，查找可驱逐缓存 b
  // 仍未缓存。
  // 我们现在只持有驱逐锁，所有的水桶锁都不是我们持有的。
  // 因此，现在获取任何水桶的锁都是安全的，不会有循环等待和死锁的风险。

  // 在所有桶中找到最近使用次数最少的一个 buf。
  // 在它对应的桶锁被锁住的情况下完成。

  struct buf* least_buf=0;
  uint holding_lock=-1;
  for(int i=0;i<NBUFMAP_BUCKET;i++)
  {
    acquire(&bcache.bufmap_locks[i]);
    int newfound=0; // 在该水桶中找到最近使用次数最少的新 buf
    for(b=&bcache.bufmap[i];b->next;b=b->next)
    {
      if(b->next->refcnt==0 && (!least_buf || b->next->lastuse<least_buf->next->lastuse))
      {
        least_buf=b;
        newfound=1;
      }
    }
    // 如果找到新的未使用时间更长的空闲块，则将原来的块所属桶的锁释放掉，保持新块所属桶的锁...
    if(!newfound)
    {
      release(&bcache.bufmap_locks[i]);
    }
    else
    {
      if(holding_lock!=-1)
      {
        release(&bcache.bufmap_locks[holding_lock]);
      }
      holding_lock=i;
    }
  }

  if(!least_buf)
    panic("bget: no buffers");
  
  b=least_buf ->next;

  if(holding_lock!=key)
  {
    // 将 buf 从原来的存储桶中移除
    // 重洗并将其添加到目标数据桶中
    least_buf->next=b->next;
    release(&bcache.bufmap_locks[holding_lock]);
    acquire(&bcache.bufmap_locks[key]);
    b->next=bcache.bufmap[key].next;
    bcache.bufmap[key].next=b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bufmap_locks[key]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;


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

  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->lastuse = ticks;
  }
  release(&bcache.bufmap_locks[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt++;
  release(&bcache.bufmap_locks[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_locks[key]);
  b->refcnt--;
  release(&bcache.bufmap_locks[key]);
}


