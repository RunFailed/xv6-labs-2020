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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


struct{
    uint8 cnt;
    struct spinlock lock;
}phyPageRefcnt[(PHYSTOP - KERNBASE) >> 12];

uint8 incrRefcnt(uint64 pa)
{
    if(pa < KERNBASE) return 0;
    pa = (pa - KERNBASE) >> 12;
    acquire(&phyPageRefcnt[pa].lock);
    uint8 res = ++phyPageRefcnt[pa].cnt;
    release(&phyPageRefcnt[pa].lock);
    return res;
}

uint8 decrRefcnt(uint64 pa)
{
    if(pa < KERNBASE) return 0;
    pa = (pa - KERNBASE) >> 12;
    acquire(&phyPageRefcnt[pa].lock);
    uint8 res = --phyPageRefcnt[pa].cnt;
    release(&phyPageRefcnt[pa].lock);
    return res;
}

uint8 getRefcnt(uint64 pa)
{
    if(pa < KERNBASE) return 0;
    pa = (pa - KERNBASE) >> 12;
    acquire(&phyPageRefcnt[pa].lock);
    uint8 res = phyPageRefcnt[pa].cnt;
    release(&phyPageRefcnt[pa].lock);
    return res;
}

void
kinit()
{
    int totalpages = (PHYSTOP - KERNBASE) >> 12;
    for(int i = 0; i < totalpages; i++)
    {
        phyPageRefcnt[i].cnt = 0;
        initlock(&phyPageRefcnt[i].lock, "refcnt");
    }
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
      incrRefcnt((uint64)p);    //Lab6 COW
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(decrRefcnt((uint64)pa) > 0) return;
  //if(getRefcnt((uint64)pa) > 0) return;

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
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
  if((uint64)r >= KERNBASE)
  {
      uint64 idx = ((uint64)r - KERNBASE) >> 12;
      acquire(&phyPageRefcnt[idx].lock);
      phyPageRefcnt[idx].cnt = 1;
      release(&phyPageRefcnt[idx].lock);
  }
    //incrRefcnt((uint64)r);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
