#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
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
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
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

uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->strace = mask;
  return 0;
}

uint64
sys_sigalarm(void)
{
  myproc()->sigalarm = 0;

  int interval;
  argint(0, &interval);
  myproc()->sigalarm_interval = interval;

  uint64 handler;
  argaddr(1, &handler);
  myproc()->sigalarm_handler = handler;

  myproc()->CPU_ticks = 0;
  myproc()->sigalarm = 1;

  return 0;
}

uint64
sys_sigreturn(void)
{
  *(myproc()->trapframe) = *(myproc()->cpy_trapframe);
  myproc()->sigalarm = 1;
  usertrapret();

  return 0;
}

uint64
sys_settickets(void)
{
  int tickets = 1;
#ifdef LBS
  argint(0, &tickets);
#endif
  return tickets;
}

uint64
sys_set_priority(void)
{
  int priority;
  int pid;

  argint(0, &priority);
  argint(1, &pid);

  return set_priority(priority, pid);
}

uint64
sys_waitx(void)
{
  uint64 addr, addr1, addr2;
  uint wtime, rtime;
  argaddr(0, &addr);
  argaddr(1, &addr1); // user virtual memory
  argaddr(2, &addr2);
  int ret = waitx(addr, &wtime, &rtime);
  struct proc *p = myproc();
  if (copyout(p->pagetable, addr1, (char *)&wtime, sizeof(int)) < 0)
    return -1;
  if (copyout(p->pagetable, addr2, (char *)&rtime, sizeof(int)) < 0)
    return -1;
  return ret;
}