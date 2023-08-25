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

//内核之后的第一个地址。由kernel.ld定义。
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct pagerefcnt
{
  struct spinlock lock; 
  uint8 count[PHYSTOP/PGSIZE];
}ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock,"ref");
  freerange(end, (void*)PHYSTOP);
}

//因为kfree语义变了，一开始在没有kalloc之前，freerange调用kfree就会造成 引用计数初始化为 -1。因为freerange全局只调用一次，所以我们可以 提前将引用计数初始化为1。
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    ref.count[(uint64)p/PGSIZE]=1;
    kfree(p);
  } 
}

//当引用物理页面的用户页表的数量增加或减少时，需要在引用计数数组中进行记录
void inref(uint64 va)
{
  acquire(&ref.lock);
  if(va<0 || va>PHYSTOP)
    panic("wrong physicl address\n");
  ref.count[va/PGSIZE]++;
  release(&ref.lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
//释放由v指向的物理内存页，该页通常应该通过调用kalloc()返回。
//(在初始化分配器时例外;见上面的kinit。)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  //如果引用计数变为0才挂到空闲页表链上
  acquire(&ref.lock);
  if((--ref.count[(uint64)pa/PGSIZE])>0)
  {
    release(&ref.lock);
    return;
  }
  release(&ref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//分配一个4096字节的物理内存页。
//返回一个内核可以使用的指针。
//如果无法分配内存，则返回0。
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  acquire(&ref.lock);
  ref.count[(uint64)r/PGSIZE]=1;
  release(&ref.lock);
  return (void*)r;
}
