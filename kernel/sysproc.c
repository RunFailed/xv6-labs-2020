#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
    backtrace();  //Lab4:2 Backtrace
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_sigalarm(void)   //Lab4:3 test0
{
    struct proc* p = myproc();
    int alarminterval;
    uint64 alarmhandler;
    if(argint(0, &alarminterval) < 0 || argaddr(1, &alarmhandler) < 0 || alarminterval < 0)
        return -1;
    p->alarmInterval = alarminterval;
    p->alarmHandler = alarmhandler;
    p->alarmTicks = 0;
    return 0;
}

uint64 sys_sigreturn(void)  //Lab4:3 test0  1/2
{
    struct proc* p = myproc();
    if(p->trapframe_copy != (struct trapframe*)((uint64)(p->trapframe) + 2048)) return -1;
    //恢复进入定时中断时用户寄存器的值
    memmove(p->trapframe, p->trapframe_copy, sizeof(struct trapframe));
    p->alarmTicks = 0;  //防止对处理程序的重复调用——如果处理程序还没有返回，内核就不应该再次调用它
    p->trapframe_copy = 0;
    return p->trapframe->a0;    //避免原a0寄存器中的值被返回值覆盖
}
