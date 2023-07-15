# README

---

# Specification 1 : System Calls

System Calls provide an interface to the user programs to communicate requests to the operating systems. In this specification, new system calls were added to the operating system, along with the user program to call them, if needed.

## System Call 1 : `trace`

- A new file `strace.c` was created in the `user` directory in xv6, containing the user program `strace` that utilizes the `trace` system call. The user program converts the string argument of the `mask` to an integer value, using the `atoi` command, throwing the necessary error, if the `mask` given is not positive. Also, it calls the `trace` system call and following it, it runs the `command` given after the `mask`. Necessary changes were made to include the compilation of `strace.c` in the `Makefile`, by adding under the `UPROGS=\` the line `$U/_strace\`.

```c
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(2, "Usage: strace mask command [args]\n");
        exit(1);
    }

    int mask = atoi(argv[1]);
    if (mask <= 0)
    {
        fprintf(2, "strace: mask must be a postive integer\n");
        exit(1);
    }

    trace(mask);

    char *cmd[MAXARG];
    for (int i = 2; i < argc; i++)
    {
        cmd[i - 2] = argv[i];
    }

    exec(cmd[0], cmd);
    fprintf(2, "exec %s failed\n", cmd[0]);
    exit(0);
}
```

- In the file `[user/usys.pl](http://usys.pl)`, the entry to the new system call was being made by adding the line `entry("trace");` at the end of the contents of the file.
- In the file `user/user.h`, the system call definition was added, by adding the line `int trace(int);` under the `system calls` section of the file.
- In the file `kernel/defs.h`, the system call was defined again, by adding the line `int trace(int);` under the `proc.c` section of the file.
- In the file `kenrel/syscall.h`, the `systemm call number` of the system call was declared, by adding the line `#define SYS_trace 22` to the file.
- In this file `kernel/syscall.c`, the system call was added to the array of system calls `static uint64 (*syscalls[])(void)` by adding the line `[SYS_trace] sys_trace,` inside the declared array. Along with this, the implementation of the system call was included to the the file, by adding the line `extern uint64 sys_trace(void)`;. Following the changes above, the arrays `char *syscall_name[SYS_waitx + 1]` and `int syscall_argc[SYS_waitx + 1]` were added. They store the name of the system call and the arguments passed in it at the index, same as that of their `system call number`. Further changes made to this file have been given later in this section.
- The variable `int strace` was declared in the `struct proc` in the file `kernel/proc.h` and initialized as `p->strace = -1` in the function `static struct proc * allocproc(void)` in the file `kernel/proc.c`
- In the file `kernel/sysproc.c`, the system call has actually been implemented as shown in the following piece of code. In the code, firstly, the argument value of `mask` has been set by reading the value from the first register and the parameter `strace` in the process structure of the current calling process has been set to this read value. This paves way for the execution of the following `command` with the pre-hand knowledge of the `mask` of the `strace` command.

```c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->strace = mask;
  return 0;
}
```

- Finally, again the file `kernel/syscall.c`, has been changed, by editing the function `void syscall(void)`. Firstly, we check for the fact whether `strace` has been called or not, if called, we already store the registers that will be needed to print the `arguments` used in the system calls called in the `command`. They are printed as shown in the piece of code. It must be noted that the register `a7` stores the `system call number` and the register `a0` stores the return value of the system call.

```c
// Prototypes for the functions that handle system calls.
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_kill(void);
extern uint64 sys_exec(void);
extern uint64 sys_fstat(void);
extern uint64 sys_chdir(void);
extern uint64 sys_dup(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_uptime(void);
extern uint64 sys_open(void);
extern uint64 sys_write(void);
extern uint64 sys_mknod(void);
extern uint64 sys_unlink(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_close(void);
extern uint64 sys_trace(void);
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);
extern uint64 sys_settickets(void);
extern uint64 sys_set_priority(void);
extern uint64 sys_waitx(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
    [SYS_fork] sys_fork,
    [SYS_exit] sys_exit,
    [SYS_wait] sys_wait,
    [SYS_pipe] sys_pipe,
    [SYS_read] sys_read,
    [SYS_kill] sys_kill,
    [SYS_exec] sys_exec,
    [SYS_fstat] sys_fstat,
    [SYS_chdir] sys_chdir,
    [SYS_dup] sys_dup,
    [SYS_getpid] sys_getpid,
    [SYS_sbrk] sys_sbrk,
    [SYS_sleep] sys_sleep,
    [SYS_uptime] sys_uptime,
    [SYS_open] sys_open,
    [SYS_write] sys_write,
    [SYS_mknod] sys_mknod,
    [SYS_unlink] sys_unlink,
    [SYS_link] sys_link,
    [SYS_mkdir] sys_mkdir,
    [SYS_close] sys_close,
    [SYS_trace] sys_trace,
    [SYS_sigalarm] sys_sigalarm,
    [SYS_sigreturn] sys_sigreturn,
    [SYS_settickets] sys_settickets,
    [SYS_set_priority] sys_set_priority,
    [SYS_waitx] sys_waitx,
};

char *syscall_name[SYS_waitx + 1] = {
    "index",
    "fork",
    "exit",
    "wait",
    "pipe",
    "read",
    "kill",
    "exec",
    "fstat",
    "chdir",
    "dup",
    "getpid",
    "sbrk",
    "sleep",
    "uptime",
    "open",
    "write",
    "mknod",
    "unlink",
    "link",
    "mkdir",
    "close",
    "trace",
    "sigalarm",
    "sigreturn",
    "setttickets",
    "set_priority",
    "waitx",
};

int syscall_argc[SYS_waitx + 1] = {
    0,
    0,
    1,
    1,
    0,
    3,
    2,
    2,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    2,
    3,
    3,
    1,
    2,
    1,
    1,
    1,
    2,
    0,
    1,
    2,
    3,
};

void syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  int a0 = p->trapframe->a0;
  int a1 = p->trapframe->a1;
  int a2 = p->trapframe->a2;
  p->trapframe->a0 = syscalls[num]();
  if (num > 0 && num < NELEM(syscalls) && syscalls[num])
  {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    if (p->strace != -1 && (p->strace & 1 << num))
    {
      printf("%d: syscall %s ( ", p->pid, syscall_name[num]);
      for (int i = 0; i < syscall_argc[num]; i++)
      {
        if (i == 0)
        {
          printf("%d ", a0);
        }
        if (i == 1)
        {
          printf("%d ", a1);
        }
        if (i == 2)
        {
          printf("%d ", a2);
        }
      }
      printf(") -> %d\n", p->trapframe->a0);
    }
  }
  else
  {
    printf("%d %s: unknown sys call %d\n",
           p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

## System Call 2 : `sigalarm` and `sigreturn`

- A new file `alarmtest.c` was created in the `user` directory in xv6, containing the user program `alarmtest` that tests the `sigalarm` and `sigreturn` system calls. Necessary changes were made to include the compilation of `alarmtest.c` in the `Makefile`, by adding under the `UPROGS=\` the line `$U/_alarmtest\`.

```
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"

void test0();
void test1();
void test2();
void test3();
void periodic();
void slow_handler();
void dummy_handler();

int main(int argc, char *argv[])
{
    test0();
    test1();
    test2();
    test3();
    exit(0);
}

volatile static int count;

void periodic()
{
    count = count + 1;
    printf("alarm!\n");
    sigreturn();
}

// tests whether the kernel calls
// the alarm handler even a single time.
void test0()
{
    int i;
    printf("test0 start\n");
    count = 0;
    sigalarm(2, periodic);
    for (i = 0; i < 1000 * 500000; i++)
    {
        if ((i % 1000000) == 0)
            write(2, ".", 1);
        if (count > 0)
            break;
    }
    sigalarm(0, 0);
    if (count > 0)
    {
        printf("test0 passed\n");
    }
    else
    {
        printf("\ntest0 failed: the kernel never called the alarm handler\n");
    }
}

void __attribute__((noinline)) foo(int i, int *j)
{
    if ((i % 2500000) == 0)
    {
        write(2, ".", 1);
    }
    *j += 1;
}

//
// tests that the kernel calls the handler multiple times.
//
// tests that, when the handler returns, it returns to
// the point in the program where the timer interrupt
// occurred, with all registers holding the same values they
// held when the interrupt occurred.
//
void test1()
{
    int i;
    int j;

    printf("test1 start\n");
    count = 0;
    j = 0;
    sigalarm(2, periodic);
    for (i = 0; i < 500000000; i++)
    {
        if (count >= 10)
            break;
        foo(i, &j);
    }
    if (count < 10)
    {
        printf("\ntest1 failed: too few calls to the handler\n");
    }
    else if (i != j)
    {
        // the loop should have called foo() i times, and foo() should
        // have incremented j once per call, so j should equal i.
        // once possible source of errors is that the handler may
        // return somewhere other than where the timer interrupt
        // occurred; another is that that registers may not be
        // restored correctly, causing i or j or the address ofj
        // to get an incorrect value.
        printf("\ntest1 failed: foo() executed fewer times than it was called\n");
    }
    else
    {
        printf("test1 passed\n");
    }
}

//
// tests that kernel does not allow reentrant alarm calls.
void test2()
{
    int i;
    int pid;
    int status;

    printf("test2 start\n");
    if ((pid = fork()) < 0)
    {
        printf("test2: fork failed\n");
    }
    if (pid == 0)
    {
        count = 0;
        sigalarm(2, slow_handler);
        for (i = 0; i < 1000 * 500000; i++)
        {
            if ((i % 1000000) == 0)
                write(2, ".", 1);
            if (count > 0)
                break;
        }
        if (count == 0)
        {
            printf("\ntest2 failed: alarm not called\n");
            exit(1);
        }
        exit(0);
    }
    wait(&status);
    if (status == 0)
    {
        printf("test2 passed\n");
    }
}

void slow_handler()
{
    count++;
    printf("alarm!\n");
    if (count > 1)
    {
        printf("test2 failed: alarm handler called more than once\n");
        exit(1);
    }
    for (int i = 0; i < 1000 * 500000; i++)
    {
        asm volatile("nop"); // avoid compiler optimizing away loop
    }
    sigalarm(0, 0);
    sigreturn();
}

//
// dummy alarm handler; after running immediately uninstall
// itself and finish signal handling
void dummy_handler()
{
    sigalarm(0, 0);
    sigreturn();
}

//
// tests that the return from sys_sigreturn() does not
// modify the a0 register
void test3()
{
    uint64 a0;

    sigalarm(1, dummy_handler);
    printf("test3 start\n");

    asm volatile("lui a5, 0");
    asm volatile("addi a0, a5, 0xac"
                 :
                 :
                 : "a0");
    for (int i = 0; i < 500000000; i++)
        ;
    asm volatile("mv %0, a0"
                 : "=r"(a0));

    if (a0 != 0xac)
        printf("test3 failed: register a0 changed\n");
    else
        printf("test3 passed\n");
}
```

- In the file `[user/usys.pl](http://usys.pl)`, the entries to the new system calls were being made by adding the lines `entry("sigalarm");` and `entry("sigreturn")`; at the end of the contents of the file.
- In the file `user/user.h`, the system calls definitions were added, by adding the lines `int sigalarm(int, void *);` and `int sigreturn(void)`; under the `system calls` section of the file.
- In the file `kernel/defs.h`, the system calls were defined again, by adding the lines `int sigalarm(int, void *);` and `int sigreturn(void)`; under the `proc.c` section of the file.
- In the file `kenrel/syscall.h`, the `systemm call number` of the system calls were declared, by adding the lines `#define SYS_sigalarm 23` and `#define SYS_sigreturn 24` to the file.
- In this file `kernel/syscall.c`, the system call were added to the array of system calls `static uint64 (*syscalls[])(void)` by adding the lines `[SYS_sigalarm] sys_sigalarm,` and `[SYS_sigreturn] sys_sigreturn,` inside the declared array. Along with this, the implementations of the system calls were included to the the file, by adding the lines `extern uint64 sys_sigalarm(void);` and `extern uint64 sys_sigreturn(void);`. Following the changes above, the arrays `char *syscall_name[SYS_waitx + 1]` and `int syscall_argc[SYS_waitx + 1]` were updated accordingly with the names and arguments of these system calls.
- The following variables were declared in the `struct proc` in the file `kernel/proc.h` and initialized as shown in the function `static struct proc * allocproc(void)` in the file `kernel/proc.c`. Each variable stores the data as it has been named, in order of declaration being the bool type variable for showing the calling of `sigalarm`, the `interval` and the `handler()` it has been called with. `CPU_ticks` stores the `ticks` passed until the `handler()` has been called. The variable `struct trapframe *cpy_trapframe` has been declared to store the state of the `trapframe` before the `handler()` has been called and later on using to to reset the original one. Initialization of the last variable has been done similar to that of the original `trapframe`, and so has it’s freeing under the `static void freeproc(struct proc *p)` function in the file `kernel/proc.c`.

```c
 if ((p->cpy_trapframe = (struct trapframe *)kalloc()) == 0)
  {
  freeproc(p);
    release(&p->lock);
    return 0;
  }

 p->sigalarm = 0;
  p->sigalarm_interval = -1;
  p->sigalarm_handler = 0;
  p->CPU_ticks = 0;
```

```c
 if (p->cpy_trapframe)
    kfree((void *)p->cpy_trapframe);
  p->cpy_trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
```

- In the file `kernel/sysproc.c`, the system calls have been implemented as shown in the following piece of code. In the code, firstly, for the first function, the argument values of the required variables have been set by reading the value from the first three registers and the corresponding parameters have been assigned those values. In the second function, the `trapframe` has been reset using the copy created and initialised while calling the `handler()`.

```c
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
```

# Specification 2 : Scheduling

The default scheduler of xv6 is round-robin based. In this task, four other scheduling policies were implemented and incorporated in xv6. The scheduling policy used by the kernel will be declared by the user during compilation, with the user specifying the scheduling type using predefined flags. The `SCHEDULER` macro was defined in the `Makefile` to handle the compilation of xv6 according to the specified scheduling algorithm done by the flags set by the user. Compilation instructions with the flags have been shown under each of the individual scheduling algorithms.

## First Come First Serve ( FCFS )

### Compilation and Execution

```bash
cd xv6-riscv-riscv
make clean
make qemu SCHEDULER=FCFS
```

### Implementation

- A variable `int time_creation` was declared in the `struct proc` data structure under the macro `FCFS` in the file `kernel/proc.h` and initialized in the `static struct proc * allocproc(void)` function in the file `kernel/proc.c` to the value of `ticks` at that moment. It is evident that `ctime` in the same structure can be used to implement this algorithm, but for the sake of separate and independent implementation of the algorithm and to provide a complete and thorough explanation, another separate variable with the same functionality has been declared.

```c
#ifdef FCFS
  p->time_creation = ticks;
#endif
```

- Under the `void scheduler(void)` function, in the file `kernel/proc.c`, the algorithm, has been implemented under the `FCFS` macro. Firstly, the array `struct proc proc[NPROC]` is being traversed to obtain the process `p_first` with the `min_time_creation` and once found, it is being scheduled as a `RUNNING` process, following which is the context switch, running the process till it no longer needs the CPU time.

```c
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
```

- As a non-preemptive scheduler was being implemented, the process must not be preempted and hence, in the functions, `void usertrap(void)` and `void kerneltrap(void)` in the file `kernel/trap.c` the function `yield()` must not be called under the `FCFS` macro, if present.

### Time Analysis

On being run on a single CPU, the data obtained is as follows ➖

- Average run time = 28
- Average wait time = 128

## Lottery Based Scheduler ( LBS )

### Compilation and Execution

```bash
cd xv6-riscv-riscv
make clean
make qemu SCHEDULER=LBS
```

### Implementation

- A variable `int tickets` was declared in the `struct proc` data structure under the macro `LBS` in the file `kernel/proc.h` and initialized in the `static struct proc * allocproc(void)` function in the file `kernel/proc.c` to `1` at that moment. The `tickets` of a process would be used to assign the `time slices` to them for running in proportion to the `tickets`.

```c
#ifdef LBS
  p->tickets = 1;
#endif
```

- In the function `int fork(void)`, in the file `kernel/proc.c` it is being added that the child process must have `tickets` equal to that of it’s parent upon being generated.

```c
#ifdef LBS
  acquire(&np->lock);
  np->tickets = p->tickets;
  release(&np->lock);
#endif
```

- Under the `void scheduler(void)` function, in the file `kernel/proc.c`, the algorithm, has been implemented under the `LBS` macro. Firstly, the array `struct proc proc[NPROC]` is being traversed to obtain the `total_tickets` which is the sum of the `tickets` of all the `RUNNABLE` processes. Once found, a variable `threshold` is being declared and allocated a random value in the range `0` to `total_tickets - 1`.
- Upon traversing the same array again, if the sum of `ticekts` till a process exceeds the `threshold`, it get’s scheduled and the `context-switch` takes place.
- As a preemptive scheduler was being implemented, the process must be preempted and hence, in the functions, `void usertrap(void)` and `void kerneltrap(void)` in the file `kernel/trap.c` the function `yield()` must be called under the `LBS` macro, if present.

```c
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
```

- A system call `int settickets(int number)` was also implemented, similar to the making of the first two system calls following the exact same procedure to assign the calling process the argument passed as it’s `tickets`

```c
int settickets(int tickets)
{
#ifdef LBS
  myproc()->tickets = tickets;
#endif
  return tickets;
}
```

### Time Analysis

On being run on a single CPU, the data obtained is as follows ➖

- Average run time = 14
- Average wait time = 156

## Priority Based Scheduler ( PBS )

### Compilation And Execution

```bash
cd xv6-riscv-riscv
make clean
make qemu SCHEDULER=PBS
```

### Implementation

- The following variables were declared in `struct proc` data structure in the file `kernel/proc.h`, and initialized as shown below in the `static struct proc * allocproc(void)` function in the file `kernel/proc.c`. The variables in order store the static priority of the process, the number of times it has been scheduled, the time it has slept for since it’s last scheduling, time it has run for since it’s last scheduling and the start time of the process.

```c
#ifdef PBS
  p->priority = 60;
  p->num_schd = 0;
  p->time_sleep = 0;
  p->time_run = 0;
  p->time_start = 0;
#endif
```

- The `void scheduler(void)` function in `kernel/proc.c` schedules the process and hence, the scheduling algorithm is under the `PBS` macro. The process with the `max_dynamic_priority`, calculated using the `niceness` which in turn is calculated using `time_sleep` and `time_run` is being selected and scheduled to `RUNNING`. This is implemented by traversing the array of processes `struct proc proc[NPROC]` and then calculating the `dynamic_priority` from the parameters defined in it’s definition, choosing the maximum priority process based on the conditions given in the details of the priority based scheduling algorithm, details implying handling of the tie-breaking cases. Once the process to be scheduled is selected, it is given a `CPU` to run, and `context switch` takes place. Since, the scheduling algorithm is non-preemptive, `yield()` must not be called under `void usertrap(void)` and `void kerneltrap(void)` in the `kernel/trap.c` file for this scheduling algorithm

```c
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
```

- In the function `void update_time()`, the time variables used in this implementation were updated as shown in the code below, as those variables have been used to calculate the `niceness` of the process.

```c
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
```

- A new system call `int set_priority(int new_priority, int pid)` was included that can be used to change the `static priority` of the process having `pid` passed as the argument. It has been implemented in exactly the same way as the first specification, with the corresponding user program being set as `setpriority priority pid`

```c
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
```

### Time Analysis

On being run on a single CPU, the data obtained is as follows ➖

- Average run time = 14
- Average wait time = 128

## Multi Level Feedback Queue ( MLFQ )

### Compilation and Execution

```bash
cd xv6-riscv-riscv
make clean
make qemu SCHEDULER=MLFQ
```

### Implementation

- The following variables were declared in the `struct proc` data structure, inside the file `kernel/proc.h` and initialized as shown below in the `static proc * allocproc(void)` function in the file `kernel/proc.c`. The variables, in order of their declaration store the current priority queue the process is a part of, the previous priority queue the process was a part of before getting popped, the time the process has waited since it being last scheduled or witnessed a shift of queue and finally the time for which the process has been running since being last scheduled.

```c
#ifdef MLFQ
  p->curr_queue = -1;
  p->prev_queue = -1;
  p->time_wait = 0;
  p->time_run_queue = 0;
#endif
```

- The priority queues have been initialized as `struct proc * proc_queues[5][NPROC] = {0};` in the file `kernel/proc.c` which initializes five priority queues with each element being a pointer to the process in the array `struct proc proc[NPROC]`. Initially, the pointers are all `0` Along with this, another variable storing the count of the processes at every moment in each of the queues has been declared and initialized to `0` as `int count_queues[5] = {0, 0, 0, 0, 0};`. Finally, the `time-slice` for each queue has also been declared as needed for the implementation of the algorithm as `int time_slice[5] = {1, 2, 4, 8, 16};`.

```c
#ifdef MLFQ
struct proc *proc_queues[5][NPROC] = {0};
int count_queues[5] = {0, 0, 0, 0, 0};
int time_slice[5] = {1, 2, 4, 8, 16};
#endif
```

- Following this, two functions, have been initialized in the file `kernel/proc.h` with their implementations in the file `kernel/proc.c`. The functions are `void push(struct proc *p, int queue)` and `void pop(struct proc *p, int queue)`. The first one pushes the `struct proc *p`into the queue with `priority` as `queue`, whereas the second one pops out the same argument out of the `priority queue` numbered `queue`.

```c
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
```

> It must be noted that during popping, always the first process for some queue would be popped out, but this precautionary measure of checking the `pid` before popping is being as not always the queues have only `RUNNABLE` processes. This is because in a single tick of the CPU, multiple times the scheduler function has been called
>
- Under the `void scheduler(void)` function, in the file `kernel/proc.c`, the scheduling algorithm was being written under the macro `MLFQ`. Firstly, we check he array of processes `struct proc proc[NPROC]` for any process that is `RUNNABLE` but not pushed in the queue till this point as scheduling begins here, including both, processes being scheduled for the first time and those who have been scheduled before as well. Further, it is ensured that the queues only have `RUNNABLE` processes and processes which are in the state `ZOMBIE` or `SLEEPING` have been popped out of their queues.
- Now, in the same function, it is checked whether the calling process, if `RUNNING` has exceeded it’s `time-slice` according to the `priority queue` it was popped from. If yes, we would `yield()` it, that is preempt it. The same would also be done if there is some other process in a higher queue than the calling one.
- For all the processes in the `struct proc proc[NPROC]` array, we check if the `time_wait` of any process has exceeded the `aging` time limit set as `30`, if so, we pop it from it’s current queue and push it in a queue with a higher priority if it exists.
- Finally, scheduling is being done by selecting the process lying first in the first non-empty queue with highest priority. On choosing a process, we pop it from it’s existing queue
- In some functions in the file `kernel/proc.c`, a line of code that pushes a process into a priority queue, depending on the nature of the process has been added as they generate new processes in the `RUNNABLE` state.
- Modify the `void update_time()` function in the file `kernel/proc.c` to change the value of the time variables after every CPU tick, as necessary.

### Time Analysis

On being run on a single CPU and aging ticks to be 30, the data obtained is as follows ➖

- Average run time = 14
- Average wait time = 158

![Untitled](README%204838fe2a28324fb28913fd1cc3c957da/Untitled.png)

The given graph shows the path of 5 processes through the priority queues with the ticks of the CPU at the moment plotted on the x-axis.
