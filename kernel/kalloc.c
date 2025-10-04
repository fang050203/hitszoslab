// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{//修改为结构体名称
  struct spinlock lock;
  struct run *freelist;
};
struct kmem kmems[NCPU];//为每个CPU分配一个freelist和锁



void
kinit()
{
  for(int i=0;i<NCPU;i++)
  {
    //都命名为同一个名字kmem 
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void//把空闲内存页加入到链表内
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  //循环分配内存
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void//释放指定内存页然后加入到freelist
kfree(void *pa)//传入调用的cpuid
{
  struct run *r;

   //获取cpuid
  push_off();
  int id=cpuid();
  pop_off();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  //安全获取cpuid
  push_off();
  int id=cpuid();
  pop_off();

  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if(r)//当当前所需cpu有空闲内存块时
  {
    kmems[id].freelist = r->next;
    release(&kmems[id].lock);
  }
  else{
    //释放当前cpu的kmem锁
    release(&kmems[id].lock);    
    for(int i =1;i<NCPU;i++)
    {
      //先获取锁
      acquire(&kmems[(id+i)%NCPU].lock);
      struct kmem *nextmem=&kmems[(id+i)%NCPU];
      struct spinlock* nextlock=&kmems[(id+i)%NCPU].lock;
      //假如有空闲页
      if(nextmem->freelist)
      {
        r=nextmem->freelist;
        nextmem->freelist = r->next;
        release(nextlock);
        break;
      }
      //无论如何都要释放锁
      release(nextlock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
