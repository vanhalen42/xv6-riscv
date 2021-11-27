

## Installation Directions

---
1. Download the zip 
2. cd into the directory `xv6-riscv`

## Run

```bash
make clean
make qemu
```

## Scheduler options

1. Round robin
2. First come first serve (FCFS)
3. Priority Based Scheduling (PBS)


# Syscall tracing

---

## **Commands to run**

```cpp
strace mask command [args]
```
We add the following to the makefile:

```cpp
$U/_strace\
```

**Perform the following steps in the kernel/proc.c**

While forking, pass the trace mask from parent to child.



**Perform the following steps in the kernel/sysproc.c**

1. We initialise the mask to 0.
2. We derive the command line arguments from the user using `argint()`
3. Provide it to the mask variable
4. Next, check if this command executed successfully.
5. Perform a proper error handling.  
5. Finally, we set the myproc() →systrackmask variable to the mask which was passed as a parameter. 

```cpp
uint64 
sys_trace(void)
{
  int mask = 0;
  if (argint(0, &mask) < 0)
    return -1;
  myproc()->mask = mask;
  return 0;
}
```


**Perform the following steps in the kernel/proc.c**


### Print the syscall name and the arguments

If these conditions are met then, we execute `strace` .

**The printing is as following:**

1. The PID: `p->pid`
2. The syscall name: using `syscall_names`
3. The decimal value of the arguments:  using `arrayarg` which stores the values.
4. The syscall return value: `p->trapframe->a0` 

```cpp
//the system call names stored in an array
static int syscall_args[] = {
    [SYS_fork] 0,
    [SYS_exit] 1,
    [SYS_wait] 1,
    [SYS_pipe] 0,
    [SYS_read] 3,
    [SYS_kill] 2,
    [SYS_exec] 2,
    [SYS_fstat] 1,
    [SYS_chdir] 1,
    [SYS_dup] 1,
    [SYS_getpid] 0,
    [SYS_sbrk] 1,
    [SYS_sleep] 1,
    [SYS_uptime] 0,
    [SYS_open] 2,
    [SYS_write] 3,
    [SYS_mknod] 3,
    [SYS_unlink] 1,
    [SYS_link] 2,
    [SYS_mkdir] 1,
    [SYS_close] 1,
    [SYS_trace] 1,
    [SYS_waitx] 3,
    
};

void syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num])
  {
    int arguments[3] = {0};
    arguments[0] = p->trapframe->a0;
    arguments[1] = p->trapframe->a1;
    arguments[2] = p->trapframe->a2;
    p->trapframe->a0 = syscalls[num]();
    int mask = p->mask;
    if ((mask >> num) & 0x1)
    {
      printf("%d: syscall %s (", p->pid, system_call_name[num]);
      for (int i = 0; i < syscall_args[num]; i++)
      {
        printf("%d", arguments[i]);
        if (i != syscall_args[num]-1)
          printf(" ");
      }
      printf(")-> %d\n", p->trapframe->a0);
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

**Perform the following steps in the user/strace.c:**

1. Accept command line arguments 
2. We convert the string command line input to integer. 
3. Perform error handling 
4. We then pass these to strace which calls kernel `SYS_trace` and would further set mask. 
5. Now this is used to print the required in syscall().
   

```cpp
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
  char *args[MAXARG];

  if (3 > argc || (argv[1][0] < '0' || argv[1][0] > '9'))
  {
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0)
  {
    fprintf(2, "%s: strace failed\n", argv[0]);
    exit(1);
  }

  for (int i = 2; i < MAXARG && i < argc; i++)
  {
    args[i - 2] = argv[i];
  }
  exec(args[0], args);
  exit(0);
}
```

---

# Scheduling

## Modifications

## Makeﬁle

Support SCHEDULER - a macro to compile the speciﬁed scheduling algorithm.



**Perform the following steps in the kernel/proc.h:**
Modification of  `struct proc` to store extra data about the process.

```cpp
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID
  int mask;
  int init_time;               // Init Time :)
  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)


  //PBS scheduler related
  int static_priority;         // in the rance of 0,100, default 60
  int dynamic_priority;        // caulculated from static priority and niceness
  int niceness;                // 0 to 10
  int run_time;
  int sleep_time;
  int nrun;                    // number of time process was picked
  uint etime;                   // When did the process exited

};

```

---

**A new function `schedule()`was made:**
- It schedules a process given to it as argument
```cpp
void schedule(struct proc *p, struct cpu *c)
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
```
## First come - First Served (FCFS)

**Perform the following in the kernel/proc.c:**


In `scheduler`

- We iterate through all available processes
- If we can run the process, check whether there was a process existing before.
- If a process existed, check whether the process's time of creation `p->init_time` is lesser than the current.
- Then  called `schedule(next_process, c);`
```cpp
#ifdef FCFS
    struct proc *next_process = 0;
    for (p = proc; p < &proc[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
      {
        if (next_process == 0 || p->init_time < next_process->init_time)
        {
          next_process = p;
        }
      }
    }
    if (next_process != 0)
      schedule(next_process, c);
#endif
```

---

## Priority Based Scheduler (PBS)

**We perform the following in kernel/proc.c:**

In `scheduler` we do the following:

### Initialisation

- Intialise the dynamic_priority to a value 0.

### Action based on existence of previous process
- If no process was scheduled for before `p->sleep_time + p->run_time == 0`, initialise `niceness` to `neutral` value $5$ of a newly scheduled process. 
- If a process was indeed scheduled before, compute the niceness using the formula provided.
- The `sleep_time` and `run_time` is derived in  updateTime()` which is called at every clock interrupt.
- Calculate the `dynamic_priority` of the process using `dynamic_priority = MAX(0, MIN((static_priority - niceness + 5), 100));`

- If no processes were scheduled before, this would be the process that is gonna be executed.
- If there was a process, check if the `dynamic priority` was lesser than the current shortest `prevs_dynamic_priority` .  
- If they are equal, use the next tie-breaker conditions based on number of times it was scheduled and time it was created.

```cpp
#ifdef PBS
    struct proc *next_process = 0;
    for (p = proc; p < &proc[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
      {
        if (next_process == 0 || p->dynamic_priority < next_process->dynamic_priority)
        {
          next_process = p;
        }
        else if (p->dynamic_priority == next_process->dynamic_priority)
        {
          if (p->nrun < next_process->nrun)
            next_process = p;
          else if (p->nrun == next_process->nrun)
          {
            if (p->init_time <= next_process->init_time)
              next_process = p;
          }
        }
      }
    }
    if (next_process != 0)
      schedule(next_process, c);
#endif
```

In `setpriority` we do the following:

- Iterate through all processes till an input pid is found
- Make required priority changes. 
- The process is also rescheduled.

```cpp
int setpriority(int new_priority, int pid)
{
  struct proc *p;
  int old_priority;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      acquire(&p->lock);
      old_priority = p->static_priority;
      p->static_priority = new_priority;
      release(&p->lock);
      return old_priority;
    }
  }
  return -1;
}
```




---


## Benchmarks

---

We observe the following values on running the `schedulertest.c` 

**In roundrobin**

```
$ schedulertest

Name              PID          State          rtime          wtime          nrun 
init              1          sleeping       1          290          0
sh              2          sleeping       0          290          0
schedulertest          3          sleeping       0          15          0
schedulertest          4          runnable      2          4          0
schedulertest          5          runnable      4          1          0
schedulertest          6          runnable      2          0          0
schedulertest          7          runnable      0          0          0
schedulertest          8          runnable      0          0          0
schedulertest          9          running         6          0          0
schedulertest          10          running         17          0          0
schedulertest          11          runnable      18          0          0
schedulertest          12          running         15          0          0
schedulertest          13          runnable      17          0          0
Process 6 finished
Process 5 finished
Process 8 finished
Process 7 finished
Process 9 finished
PPPrrrooocceessss  c01  ffiinniisshheedd

essPP rr2o cfeisnsi sh4eod cfeisnsi sh
3e df
inished
Average rtime 78,  wtime 66
```

**In fcfs:**

```
$ schedulertest  

Name              PID          State          rtime          wtime          nrun 
init              1          sleeping       3          293          0
sh              2          sleeping       0          293          0
schedulertest          3          sleeping       0          19          0
schedulertest          4          running         38          0          0
schedulertest          5          running         38          0          0
schedulertest          6          running         38          0          0
schedulertest          7          runnable      0          0          0
schedulertest          8          runnable      0          0          0
schedulertest          9          runnable      0          0          0
schedulertest          10          runnable      0          0          0
schedulertest          11          runnable      0          0          0
schedulertest          12          runnable      0          0          0
schedulertest          13          runnable      0          0          0
Process 1 finished
Process 2 finished
Process 0 finished
Process 3 finished
Process 5 finished
Process 4 finished
Process 6 finished
Process 8 finished
Process 7 finished
Process 9 finished
Average rtime 9,  wtime 105
```

**In pbs:**

```
$ schedulertest

Name              PID      Priority      State          rtime          wtime          nrun 
init              1      56          sleeping       3          215          0
sh              2      56          sleeping       1          214          0
schedulertest          3      55          sleeping       0          19          0
schedulertest          4      80          sleeping       0          0          0
schedulertest          5      80          sleeping       0          0          0
schedulertest          6      80          sleeping       0          0          0
schedulertest          7      85          runnable      1          0          0
schedulertest          8      85          runnable      2          0          0
schedulertest          9      85          runnable      1          0          0
schedulertest          10      85          runnable      2          0          0
schedulertest          11      85          running         36          0          0
schedulertest          12      85          running         32          0          0
schedulertest          13      85          running         35          0          0
Process 9 finished
Process 8 finished
Process 7 finished
Process 6 finished
Process 5 finished
PPPrroorcoecsess sc4  1f ifeisnsins he0ids
 fhiePdn
isrhoecdP
esrso c2e sfs in3i sfhienids
hed
Average rtime 77,  wtime 57
```
### ROUND_ROBIN: Average rtime 78,  wtime 66

### FCFS: Average rtime 9,  wtime 105

### PBS: Average rtime 77,  wtime 57
