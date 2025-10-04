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
#define NBUCKETS 13
struct {
  //每个哈希桶都有一个锁
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];
  //全局大锁实现
  struct spinlock global_lock;  
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
  //哈希桶优化
  struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list及一个lock
} bcache;

void
binit(void)
{
  struct buf *b;

  //initlock(&bcache.lock, "bcache");
  initlock(&bcache.global_lock,"global_lock");
  // Create linked list of buffers
  //对每一个哈希桶头都初始化
  for(int i = 0;i<NBUCKETS;i++)
  {
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }
  int j=0;//用于循环哈希链表
  for(b = bcache.buf; b < bcache.buf+NBUF; b++,j = (j+1) % NBUCKETS){
    b->next = bcache.hashbucket[j].next;
    b->prev = &bcache.hashbucket[j];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[j].next->prev = b;
    bcache.hashbucket[j].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  //哈希索引
  int hash = blockno % 13;
  //获取对应的hash锁
  acquire(&bcache.lock[hash]);

  // Is the block already cached?
  for(b = bcache.hashbucket[hash].next; b != &bcache.hashbucket[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  //如果没找到，那就先释放锁
  release(&bcache.lock[hash]);
  //先获取全局大锁
  acquire(&bcache.global_lock);
  //再获取需要查找的锁
  acquire(&bcache.lock[hash]);
  //再查找一遍
  for(b = bcache.hashbucket[hash].next; b != &bcache.hashbucket[hash]; b = b->next){
      if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        release(&bcache.lock[hash]);
        //别忘记释放全局锁
        release(&bcache.global_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  //如果没找到，查找是否有可分配缓存块
  for(b = bcache.hashbucket[hash].prev; b != &bcache.hashbucket[hash]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      //新添加，buf块的hash信息
      b->hashid = hash;
      release(&bcache.lock[hash]);
      //别忘记释放全局锁
      release(&bcache.global_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  //假如也没找到合适的块
  for(int j=1;j<NBUCKETS;j++)
  {
    //首先获取这个锁
    acquire(&bcache.lock[(hash + j) % NBUCKETS]);


    for(b = bcache.hashbucket[(hash + j) % NBUCKETS].next; b != &bcache.hashbucket[(hash + j) % NBUCKETS]; b = b->next){
      if(b->dev == dev && b->blockno == blockno){
        b->refcnt++;


        //新添加，buf块的hash信息
        b->hashid = hash;
        //要把内存块插入到指定的哈希桶中
        //先把它从原来的哈希桶中拿出来
        b->next->prev = b->prev;
        b->prev->next = b->next;
        //然后再插入
        b->next = bcache.hashbucket[hash].next;
        b->prev = &bcache.hashbucket[hash];
        bcache.hashbucket[hash].next->prev = b;
        bcache.hashbucket[hash].next = b;


        release(&bcache.lock[(hash + j) % NBUCKETS]);
        release(&bcache.lock[hash]);
        //别忘记释放全局锁
        release(&bcache.global_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }


    for(b = bcache.hashbucket[(hash + j) % NBUCKETS].prev; b != &bcache.hashbucket[(hash + j) % NBUCKETS]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      //新添加，buf块的hash信息
      b->hashid = hash;
      //要把内存块插入到指定的哈希桶中

      //先把它从原来的哈希桶中拿出来
      b->next->prev = b->prev;
      b->prev->next = b->next;
      //然后再插入
      b->next = bcache.hashbucket[hash].next;
      b->prev = &bcache.hashbucket[hash];
      bcache.hashbucket[hash].next->prev = b;
      bcache.hashbucket[hash].next = b;

      release(&bcache.lock[(hash + j) % NBUCKETS]);
      release(&bcache.lock[hash]);
      //别忘记释放全局锁
      release(&bcache.global_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
    release(&bcache.lock[(hash + j) % NBUCKETS]);
  }
  //这里就不释放了
  //release(&bcache.global_lock);
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

  acquire(&bcache.lock[b->hashid]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[b->hashid].next;
    b->prev = &bcache.hashbucket[b->hashid];
    bcache.hashbucket[b->hashid].next->prev = b;
    bcache.hashbucket[b->hashid].next = b;
  }
  
  release(&bcache.lock[b->hashid]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[b->hashid]);
  b->refcnt++;
  release(&bcache.lock[b->hashid]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[b->hashid]);
  b->refcnt--;
  release(&bcache.lock[b->hashid]);
}


