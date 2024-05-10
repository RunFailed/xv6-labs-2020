#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#ifdef LAB_MMAP
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#endif

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  }
  else if(r_scause() == 13 || r_scause() == 15)     //13是读page fault，15是写page fault     r_scause() == 12 ||
  {
      //先找到是在哪个VMA区域内出错
      uint64 va = r_stval();    //出错的虚拟内存地址
      int idx;
      for(idx = 0; idx < MAXVMANUM; idx++)
      {
          if(p->vma_arr[idx].fptr == 0) continue;
          if(va >= p->vma_arr[idx].addr + p->vma_arr[idx].offset && va < p->vma_arr[idx].addr + p->vma_arr[idx].offset + p->vma_arr[idx].length)
          {
              break;
          }
      }
      if(idx >= MAXVMANUM) goto bad;
      //检查出错原因与映射文件访问权限是否匹配
      //如果是read page fault但是内存映射时未设置读权限
      //或者是write page fault但是内存映射时未设置写权限
      //则goto bad
      //
      if( (r_scause() == 13 &&  p->vma_arr[idx].fptr->readable == 0)
            || (r_scause() == 15 && p->vma_arr[idx].fptr->writable == 0) )
          goto bad;
      //找一个空闲的物理页面
      uint64 pa = (uint64)kalloc();
      if(pa == 0)
      {
          //如果无空闲物理页面，则从已分配物理内存的内存映射区中找一个物理页面换出
          for(int i = 0; i < MAXVMANUM; i++)
          {
              for(uint64 swapout_va = p->vma_arr[i].addr; swapout_va < p->vma_arr[i].addr + PGROUNDUP(p->vma_arr[i].length); swapout_va += PGSIZE)
              {
                  pte_t *pte = walk(p->pagetable, swapout_va, 0);
                  //如果在内存映射区找到已分配的物理页面，则把该物理页面重新分配给现在出错的进程
                  if(pte != 0 && (*pte & PTE_V) != 0 && (*pte & PTE_U) != 0)
                  {
                      pa = PTE2PA(*pte);
                      *pte = 0;
                  }
              }
          }
      }
      if(pa == 0) goto bad;     //未找到合适的换出页面
      memset((void*)pa, 0, PGSIZE);
      //把文件中的数据读入内存，并映射到用户地址空间
      //va = PGROUNDDOWN(va);
      ilock(p->vma_arr[idx].fptr->ip);
      int ret = readi(p->vma_arr[idx].fptr->ip, 0, pa, PGROUNDDOWN(va - p->vma_arr[idx].addr) + p->vma_arr[idx].offset, PGSIZE);
      if(ret == 0)
      {
          iunlock(p->vma_arr[idx].fptr->ip);
          kfree((void*)pa);
          goto bad;
      }
      iunlock(p->vma_arr[idx].fptr->ip);
      int perm = PTE_U;
      if(p->vma_arr[idx].prot & PROT_READ) perm |= PTE_R;
      if(p->vma_arr[idx].prot & PROT_WRITE) perm |= PTE_W;
      if(p->vma_arr[idx].prot & PROT_EXEC) perm |= PTE_X;
      if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, pa, perm) != 0) //  | PTE_V
      {
          kfree((void*)pa);
          goto bad;
      }
  }
  else if((which_dev = devintr()) != 0){
    // ok
  } else {
bad:
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

