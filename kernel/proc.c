#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

#ifdef MLFQ
struct proc *proc_queues[5][NPROC] = {0};
int count_queues[5] = {0, 0, 0, 0, 0};
int time_slice[5] = {1, 2, 4, 8, 16};
#endif

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  if ((p->cpy_trapframe = (struct trapframe *)kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  p->strace = -1;
  p->sigalarm = 0;
  p->sigalarm_interval = -1;
  p->sigalarm_handler = 0;
  p->CPU_ticks = 0;
  p->rtime = 0;
  p->etime = 0;
  p->ctime = ticks;

#ifdef FCFS
  p->time_creation = ticks;
#endif
#ifdef LBS
  p->tickets = 1;
#endif
#ifdef PBS
  p->priority = 60;
  p->num_schd = 0;
  p->time_sleep = 0;
  p->time_run = 0;
  p->time_start = 0;
#endif
#ifdef MLFQ
  p->curr_queue = -1;
  p->prev_queue = -1;
  p->time_wait = 0;
  p->time_run_queue = 0;
#endif

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->cpy_trapframe)
    kfree((void *)p->cpy_trapframe);
  p->cpy_trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

#ifdef MLFQ
void push(struct proc *p, int queue)
{
  if (queue == -1)
  {
    queue = 0;
  }

  if (count_queues[queue] != NPROC)
  {
    for (int i = 0; i < count_queues[queue]; i++)
    {
      if (p->pid == proc_queues[queue][i]->pid)
      {
        return;
      }
    }

    proc_queues[queue][count_queues[queue]] = p;
    count_queues[queue]++;
    p->curr_queue = queue;
    p->time_wait = 0;
    p->time_run_queue = 0;
  }

  return;
}

void pop(struct proc *p, int queue)
{
  if (count_queues[queue] != 0)
  {
    for (int i = 0; i < count_queues[queue]; i++)
    {
      if (p->pid == proc_queues[queue][i]->pid)
      {
        p->prev_queue = p->curr_queue;
        p->curr_queue = -1;
        for (int j = i; j < count_queues[queue] - 1; j++)
        {
          proc_queues[queue][j] = proc_queues[queue][j + 1];
        }
        count_queues[queue]--;
        p->time_wait = 0;
        p->time_run_queue = 0;
        return;
      }
    }
  }
}
#endif

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

#ifdef MLFQ
  push(p, 0);
#endif

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

#ifdef LBS
  acquire(&np->lock);
  np->tickets = p->tickets;
  release(&np->lock);
#endif

#ifdef MLFQ
  acquire(&np->lock);
  push(np, 0);
  release(&np->lock);
#endif

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;
  p->etime = ticks;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
      if (pp->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE)
        {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0)
          {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p))
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}

int waitx(uint64 addr, uint *wtime, uint *rtime)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (np = proc; np < &proc[NPROC]; np++)
    {
      if (np->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if (np->state == ZOMBIE)
        {
          // Found one.
          pid = np->pid;
          *rtime = np->rtime;
          *wtime = np->etime - np->ctime - np->rtime;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                   sizeof(np->xstate)) < 0)
          {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || p->killed)
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}

void update_time()
{
  for (struct proc *p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == RUNNABLE)
    {
#ifdef MLFQ
      if (p->curr_queue != -1)
      {
        p->time_wait++;
      }
#endif
    }
    if (p->state == SLEEPING)
    {
#ifdef PBS
      p->time_sleep++;
#endif
    }
    if (p->state == RUNNING)
    {
      p->rtime++;
#ifdef PBS
      p->time_run++;
#endif
#ifdef MLFQ
      p->time_run_queue++;
#endif
    }
    release(&p->lock);
  }

  // #ifdef MLFQ
  //   struct proc *p;
  //   for (p = proc; p < &proc[NPROC]; p++)
  //   {
  //     if (p->pid >= 4 && p->pid <= 13 && p->curr_queue != -1)
  //     {
  //       printf("%d %d %d\n", p->pid, p->curr_queue, ticks);
  //     }
  //     else if (p->pid >= 4 && p->pid <= 13 && p->curr_queue == -1)
  //     {
  //       printf("%d %d %d\n", p->pid, p->prev_queue, ticks);
  //     }
  //   }
  // #endif
}

int set_priority(int new_priority, int pid)
{
  int priority = -1;
#ifdef PBS
  struct proc *p;
  int flag = 0;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      priority = p->priority;
      p->priority = new_priority;
      p->time_run = 0;
      p->time_sleep = 0;
      flag = 1;
      break;
    }
    release(&p->lock);
  }

  if (flag)
  {
    release(&p->lock);
    if (priority < p->priority)
    {
      yield();
    }
  }

#endif
  return priority;
}

// from FreeBSD.
int do_rand(unsigned long *ctx)
{
  /*
   * Compute x = (7^5 * x) mod (2^31 - 1)
   * without overflowing 31 bits:
   *      (2^31 - 1) = 127773 * (7^5) + 2836
   * From "Random number generators: good ones are hard to find",
   * Park and Miller, Communications of the ACM, vol. 31, no. 10,
   * October 1988, p. 1195.
   */
  long hi, lo, x;

  /* Transform to [1, 0x7ffffffe] range. */
  x = (*ctx % 0x7ffffffe) + 1;
  hi = x / 127773;
  lo = x % 127773;
  x = 16807 * lo - 2836 * hi;
  if (x < 0)
    x += 0x7fffffff;
  /* Transform to [0, 0x7ffffffd] range. */
  x--;
  *ctx = x;
  return (x);
}

unsigned long rand_next = 1;

int rand(void)
{
  return (do_rand(&rand_next));
}

int settickets(int tickets)
{
#ifdef LBS
  myproc()->tickets = tickets;
#endif
  return tickets;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    intr_on();

#ifdef RR
    // Avoid deadlock by ensuring that devices can interrupt.

    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
#endif
#ifdef FCFS
    struct proc *p_first = 0;
    int min_time_creation = -1;

    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        if (p_first != 0 && p->time_creation < min_time_creation)
        {
          release(&p_first->lock);
          p_first = p;
          min_time_creation = p->time_creation;
          continue;
        }
        else if (p_first != 0 && p->time_creation >= min_time_creation)
        {
          release(&p->lock);
          continue;
        }
        else
        {
          p_first = p;
          min_time_creation = p->time_creation;
          continue;
        }
      }
      release(&p->lock);
    }

    if (p_first != 0 && p_first->state == RUNNABLE)
    {
      p_first->state = RUNNING;
      c->proc = p_first;
      swtch(&c->context, &p_first->context);
      c->proc = 0;
      release(&p_first->lock);
    }
#endif
#ifdef LBS
    struct proc *p_lottery = 0;
    int total_tickets = 0;

    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        total_tickets += p->tickets;
      }
      release(&p->lock);
    }

    int threshold = rand();
    threshold = (threshold % total_tickets);

    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        if (threshold > 0)
        {
          threshold -= p->tickets;
        }
        else
        {
          p_lottery = p;
          break;
        }
      }
      release(&p->lock);
    }

    if (p_lottery != 0)
    {
      if (p_lottery->state == RUNNABLE)
      {
        p_lottery->state = RUNNING;
        c->proc = p_lottery;
        swtch(&c->context, &p_lottery->context);
        c->proc = 0;
      }
      release(&p_lottery->lock);
    }
#endif
#ifdef PBS
    struct proc *p_priority = 0;
    int max_dynamic_priority = -1;

    for (struct proc *p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        int niceness = 5;
        int dynamic_priority = p->priority + 5;

        if (p->num_schd != 0)
        {
          niceness = (p->time_sleep) / (p->time_run + p->time_sleep);
          niceness *= 10;
        }

        dynamic_priority -= niceness;

        if (dynamic_priority > 100)
        {
          dynamic_priority = 100;
        }

        if (dynamic_priority < 0)
        {
          dynamic_priority = 0;
        }

        if (p_priority != 0)
        {
          if (max_dynamic_priority > dynamic_priority)
          {
            release(&p_priority->lock);
            p_priority = p;
            max_dynamic_priority = dynamic_priority;
            continue;
          }
          else if (max_dynamic_priority == dynamic_priority)
          {
            if (p->num_schd > p_priority->num_schd)
            {
              release(&p_priority->lock);
              p_priority = p;
              max_dynamic_priority = dynamic_priority;
              continue;
            }
            else if (p->num_schd == p_priority->num_schd)
            {
              if (p->time_start < p_priority->time_start)
              {
                release(&p_priority->lock);
                p_priority = p;
                max_dynamic_priority = dynamic_priority;
                continue;
              }
            }
          }
        }
        else
        {
          p_priority = p;
          max_dynamic_priority = dynamic_priority;
          continue;
        }
      }
      release(&p->lock);
    }

    if (p_priority != 0)
    {
      if (p_priority->state == RUNNABLE)
      {
        p_priority->state = RUNNING;
        p_priority->num_schd++;
        p_priority->time_sleep = 0;
        p_priority->time_run = 0;
        if (p_priority->time_start == 0)
        {
          p_priority->time_start = ticks;
        }
        c->proc = p_priority;
        swtch(&c->context, &p_priority->context);
        c->proc = 0;
      }
      release(&p_priority->lock);
    }
#endif
#ifdef MLFQ
    struct proc *p = 0;

    for (p = proc; p < &proc[NPROC]; p++)
    {
      if (p != 0)
      {
        acquire(&p->lock);
        if (p->state == RUNNABLE)
        {
          if (p->curr_queue == -1 && p->prev_queue == -1)
          {
            push(p, 0);
          }
          else if (p->curr_queue == -1)
          {
            push(p, p->prev_queue);
          }
        }
        if (p->state == ZOMBIE || p->state == SLEEPING)
        {
          if (p->curr_queue != -1)
          {
            pop(p, p->curr_queue);
          }
        }
        release(&p->lock);
      }
    }

    p = myproc();

    if (p != 0 && p->state == RUNNING)
    {
      if (p->time_run_queue >= time_slice[p->prev_queue])
      {
        if (p->prev_queue != 4)
        {
          push(p, (p->prev_queue + 1));
        }
        else
        {
          push(p, 4);
        }
        yield();
      }
    }

    if (p != 0 && p->state == RUNNING)
    {
      for (int i = 0; i < p->prev_queue; i++)
      {
        if (count_queues[i] != 0 && proc_queues[i][count_queues[i] - 1]->state == RUNNABLE)
        {
          push(p, p->prev_queue);
          yield();
        }
      }
    }

    for (int i = 1; i < 5; i++)
    {
      for (int j = 0; j < count_queues[i]; i++)
      {
        p = proc_queues[i][j];
        acquire(&p->lock);
        if (p->state == RUNNABLE)
        {
          if (p->time_wait > 30)
          {
            pop(p, p->curr_queue);
            push(p, p->prev_queue - 1);
          }
        }
        release(&p->lock);
      }
    }

    struct proc *p_multilevel = 0;

    int done = 0;
    for (int i = 0; i < 5; i++)
    {
      for (int j = 0; j < count_queues[i]; j++)
      {
        p = proc_queues[i][j];
        if (p != 0)
        {
          acquire(&p->lock);
          if (p != 0 && p->state == RUNNABLE)
          {
            p_multilevel = p;
            pop(p, i);
            p_multilevel->state = RUNNING;
            c->proc = p_multilevel;
            swtch(&c->context, &p_multilevel->context);
            c->proc = 0;
            release(&p_multilevel->lock);
            done = 1;
            break;
          }
          release(&p->lock);
        }
      }

      if (done)
      {
        break;
      }
    }

#endif
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); // DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan)
      {
        p->state = RUNNABLE;
#ifdef MLFQ
        push(p, p->prev_queue);
#endif
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [USED] "used",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}