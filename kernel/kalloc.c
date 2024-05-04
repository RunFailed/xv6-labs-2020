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

//struct {
//  struct spinlock lock;
//  struct run *freelist;
//} kmem;

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem[NCPU];   //Lab8:1

void
kinit()
{
    //Lab8:1 初始化每一个锁
    for(int i = 0; i < NCPU; i++)
    {
        char buf[10] = {'\0'};
        snprintf(buf, sizeof(buf), "kmem%d", i);
        initlock(&kmem[i].lock, buf);
    }
  //initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

////Lab8:1    freerange时把空闲页面平均分配给每一个core
//void
//freerange(void *pa_start, void *pa_end)
//{
//    char *p;
//    p = (char*)PGROUNDUP((uint64)pa_start);
//    for(int i = 0; p + PGSIZE <= (char*)pa_end; p += PGSIZE, i++)
//    {
//        struct run *r;
//
//        if(((uint64)p % PGSIZE) != 0 || (char*)p < end || (uint64)p >= PHYSTOP)
//            panic("freerange");
//
//        // Fill with junk to catch dangling refs.
//        memset(p, 1, PGSIZE);
//
//        r = (struct run*)p;
//
//        acquire(&kmem[i % NCPU].lock);
//        r->next = kmem[i % NCPU].freelist;
//        kmem[i % NCPU].freelist = r;
//        release(&kmem[i % NCPU].lock);
//    }
//        //kfree(p);
//}

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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

    push_off();     //Lab8:1关闭中断
    int cid = cpuid();  //获取cpuid
    pop_off();  //关闭中断
    acquire(&kmem[cid].lock);
    r->next = kmem[cid].freelist;
    kmem[cid].freelist = r;
    release(&kmem[cid].lock);


//  acquire(&kmem.lock);
//  r->next = kmem.freelist;
//  kmem.freelist = r;
//  release(&kmem.lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//void *
//kalloc(void)
//{
//  struct run *r;
//
//    push_off();     //Lab8:1关闭中断
//    int cid = cpuid();  //获取cpuid
//    pop_off();  //关闭中断
//    acquire(&kmem[cid].lock);
//    r = kmem[cid].freelist;
//    if(r)
//    {
//        kmem[cid].freelist = r->next;
//        release(&kmem[cid].lock);
//        goto getfreepage;
//    }
//    else
//    {
//        release(&kmem[cid].lock);
//        for(int i = 0; i < NCPU; i++)
//        {
//            acquire(&kmem[i].lock);
//            r = kmem[i].freelist;
//            if(r)
//            {
//                kmem[i].freelist = r->next;
//                release(&kmem[i].lock);
//                goto getfreepage;
//            }
//            release(&kmem[i].lock);
//        }
//    }
//
//
//
////  acquire(&kmem.lock);
////  r = kmem.freelist;
////  if(r)
////    kmem.freelist = r->next;
////  release(&kmem.lock);
//getfreepage:
//  if(r)
//    memset((char*)r, 5, PGSIZE); // fill with junk
//  return (void*)r;
//}

struct run *steal(int cpu_id)
{
    int i;
    int cid = cpu_id;
    struct run *fast, *slow, *head;
    //确保实际运行steal的cpuid和传递进来的cpuid一致
    if(cpu_id != cpuid()) panic("steal");
    //找一个具有空物理页的CPU，并把其freelist的前半部分分配给传入的cpu_id
    for(i = 1; i < NCPU; i++)
    {
        if(++cid == NCPU) cid = 0;
        acquire(&kmem[cid].lock);
        if(kmem[cid].freelist)
        {
            //快慢指针找链表中间点
            slow = head = kmem[cid].freelist;
            fast = slow->next;
            while(fast)
            {
                fast = fast->next;
                if(fast)
                {
                    fast = fast->next;
                    slow = slow->next;
                }
            }
            kmem[cid].freelist = slow->next;
            release(&kmem[cid].lock);
            slow->next = 0;
            return head;
        }
        release(&kmem[cid].lock);
    }
    //其他CPU物理页也均为空
    return 0;
}

void *
kalloc(void)
{
    struct run *r;

    push_off();     //Lab8:1关闭中断
    int cid = cpuid();  //获取cpuid
    pop_off();  //关闭中断
    acquire(&kmem[cid].lock);
    r = kmem[cid].freelist;
    if(r) kmem[cid].freelist = r->next;
    release(&kmem[cid].lock);

    if(!r && (r = steal(cid)))
    {
        acquire(&kmem[cid].lock);
        kmem[cid].freelist = r->next;
        release(&kmem[cid].lock);
    }

    if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
}
