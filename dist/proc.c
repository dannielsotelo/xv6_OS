#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif // CS333_P2
#ifdef CS333_P3
#define statecount NELEM(states)
#endif // CS333_P3

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif // CS333_P3

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3
  struct ptrs list[statecount];
  #endif //CS333_P3
  #ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
  #endif //CS333_P4
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

#ifdef CS333_P3
static void stateListAdd(struct ptrs*, struct proc* p);
static int stateListRemove(struct ptrs* list, struct proc* p);
static void assertState(struct proc* p, enum procstate state);
static void initProcessLists(void);
static void initFreeList(void);
#endif // CS333_P3

#ifdef CS333_P4
static int search4Proc(int pid, struct proc **p);
static void adjustPriority();
#endif // CS333_P4

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  #ifdef CS333_P3
  acquire(&ptable.lock);
  p = ptable.list[UNUSED].head;

  if(!p->next){
    release(&ptable.lock);
    return 0;
  }


  int rc = stateListRemove(&ptable.list[UNUSED], p);
  if(rc != 0)
    panic("Process was not removed from UNUSED list inside allocproc \n");
  assertState(p, UNUSED);
  p->state = EMBRYO;
  stateListAdd(&ptable.list[EMBRYO], p);
  p->pid = nextpid++;
  release(&ptable.lock);
  #else
  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);
  #endif


  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    #ifdef CS333_P3
    acquire(&ptable.lock);
    int rc = stateListRemove(&ptable.list[EMBRYO], p);
    if(rc != 0)
      panic("Process was not removed from EMBRYO in allocproc\n");
    assertState(p, EMBRYO);
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
    release(&ptable.lock);
    #else
    acquire(&ptable.lock);
    p->state = UNUSED;
    release(&ptable.lock);
    #endif
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  p->start_ticks = ticks;

  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;

  return p;
}

// Set up first user process.
#ifdef CS333_P4
void
userinit(void) //initialize lists here
{

  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  release(&ptable.lock);

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.

  acquire(&ptable.lock);
  int rc = stateListRemove(&ptable.list[EMBRYO], p);
  if(rc != 0)
    panic("Process was not removed from EMBRYO list\n");
  assertState(p, EMBRYO);
  p->state = RUNNABLE;
  p->priority = MAXPRIO;
  p->budget = DEFAULT_BUDGET;
  stateListAdd(&ptable.ready[p->priority], p);

  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;
  release(&ptable.lock);
}
#else
void
userinit(void) //initialize lists here
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];



  //initialize the processlists and free/UNUSED list
  #ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
  #endif // CS333_P3

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  #ifdef CS333_P3
  acquire(&ptable.lock);
  int rc = stateListRemove(&ptable.list[EMBRYO], p);
  if(rc != 0)
    panic("Process was not removed from EMBRYO list\n");
  assertState(p, EMBRYO);
  p->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], p);
  release(&ptable.lock);
  #else
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
  #endif

  #ifdef CS333_P2
  acquire(&ptable.lock);
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;
  release(&ptable.lock);
  #endif// CS333_P2
}
#endif

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}
// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
#ifdef CS333_P4
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;

    acquire(&ptable.lock);
    int rc = stateListRemove(&ptable.list[EMBRYO], np);
    if(rc != 0)
      panic("Process was not removed from EMBRYO list\n");
    assertState(np, EMBRYO);
    np->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], np);
    release(&ptable.lock);

    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
    // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  if(curproc != NULL){
    acquire(&ptable.lock);
    np->uid = curproc->uid;
    np->gid = curproc->gid;
    release(&ptable.lock);
  }

  acquire(&ptable.lock);
  int rc = stateListRemove(&ptable.list[EMBRYO], np);
  if(rc != 0)
    panic("Process was not removed from EMBRYO list\n");
  assertState(np, EMBRYO);
  np->state = RUNNABLE;
  np->budget = DEFAULT_BUDGET; // CS333_P4
  np->priority = MAXPRIO;
  stateListAdd(&ptable.ready[np->priority], np); //add to ready[MAXPRIO} because this is a newly created process.
  release(&ptable.lock);

  return pid;
}
#else
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;

   #ifdef CS333_P3
    acquire(&ptable.lock);
    int rc = stateListRemove(&ptable.list[EMBRYO], np);
    if(rc != 0)
      panic("Process was not removed from EMBRYO list\n");
    assertState(np, EMBRYO);
    np->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], np);
    release(&ptable.lock);
    #else
    np->state = UNUSED;
    #endif
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
    // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  #ifdef CS333_P2
  // CS333_P2
  if(curproc != NULL){
    acquire(&ptable.lock);
    np->uid = curproc->uid;
    np->gid = curproc->gid;
    release(&ptable.lock);
  }
  #endif // CS333_P2

  #ifdef CS333_P3
  acquire(&ptable.lock);
  int rc = stateListRemove(&ptable.list[EMBRYO], np);
  if(rc != 0)
    panic("Process was not removed from EMBRYO list\n");
  assertState(np, EMBRYO);
  np->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], np);
  release(&ptable.lock);
  #else
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  #endif

  return pid;
}
#endif

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P4
void
exit(void)  // do i adjust budget here? NOOOO!!
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  p = ptable.list[EMBRYO].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }

  for(int i = MAXPRIO; i >= 0; i--){  // RUNNABLE
    p = ptable.ready[i].head;
    while(p != 0){
      if(p->parent == curproc){
        p->parent = initproc;
      }
      p = p->next;
    }
  }
  p = ptable.list[RUNNING].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }
  p = ptable.list[SLEEPING].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }
  p = ptable.list[ZOMBIE].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
    p = p->next;
  }

  // Jump into the scheduler, never to return.
  int rc = stateListRemove(&ptable.list[RUNNING], curproc);
  if(rc != 0)
    panic("Process was not removed from RUNNING list in exit()\n");
  assertState(curproc, RUNNING);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);

  sched();
  panic("zombie exit");
}
#elif defined(CS333_P3)
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  p = ptable.list[EMBRYO].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }
  p = ptable.list[RUNNABLE].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }
  p = ptable.list[RUNNING].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }
  p = ptable.list[SLEEPING].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
    }
    p = p->next;
  }
  p = ptable.list[ZOMBIE].head;
  while(p != 0){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
    p = p->next;
  }

  // Jump into the scheduler, never to return.
  int rc = stateListRemove(&ptable.list[RUNNING], curproc);
  if(rc != 0)
    panic("Process was not removed from RUNNING list in exit()\n");
  assertState(curproc, RUNNING);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);

  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P4
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();
  //struct proc * temp;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children. ie children in the zombie state
    havekids = 0;

    p = ptable.list[EMBRYO].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    for(int i = MAXPRIO; i >= 0; i--){ //RUNNABLE
      p = ptable.ready[i].head;
      while(p != 0){
        if(p->parent == curproc)
          havekids = 1;
        p = p->next;
      }
    }

    p = ptable.list[RUNNING].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    p = ptable.list[SLEEPING].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    p = ptable.list[ZOMBIE].head;
    /// only zombie returns
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      if(p->state == ZOMBIE){
        //found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        int rc = stateListRemove(&ptable.list[ZOMBIE], p);
        if(rc!=0)
          panic("Process was not removed from ZOMBIE list in wait()\n");
        assertState(p, ZOMBIE);
        p->state = UNUSED;
        stateListAdd(&ptable.list[UNUSED], p);
        release(&ptable.lock);
        return pid;
      }
      p = p->next;
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#elif defined(CS333_P3)
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();
  //struct proc * temp;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children. ie children in the zombie state
    havekids = 0;

    p = ptable.list[EMBRYO].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    p = ptable.list[RUNNABLE].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    p = ptable.list[RUNNING].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    p = ptable.list[SLEEPING].head;
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      p = p->next;
    }

    p = ptable.list[ZOMBIE].head;
    /// only zombie returns
    while(p != 0){
      if(p->parent == curproc)
        havekids = 1;
      if(p->state == ZOMBIE){
        //found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        int rc = stateListRemove(&ptable.list[ZOMBIE], p);
        if(rc!=0)
          panic("Process was not removed from ZOMBIE list in wait()\n");
        assertState(p, ZOMBIE);
        p->state = UNUSED;
        stateListAdd(&ptable.list[UNUSED], p);
        release(&ptable.lock);
        return pid;
      }
      p = p->next;
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef CS333_P4
void
scheduler(void) // check to see if it is time to adjust priorities
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    //look for process to run
    acquire(&ptable.lock);
    //check for promotion and promote if necessary
    if(MAXPRIO > 0){
      if(ticks >= ptable.PromoteAtTime){
        //cprintf("\nXXXXXXXXX PROMOTING!!!!! XXXXXXXXX\n");
        adjustPriority();
        ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
      }
    }
    for(int i = MAXPRIO; i >= 0; i--){
      p = ptable.ready[i].head;
      if(p)
        break;
    }

    if(p != 0){
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
#ifdef PDX_XV6
    idle = 0;  // not idle this timeslice
#endif // PDX_XV6
    c->proc = p;
#ifdef CS333_P2
    p->cpu_ticks_in = ticks;
#endif // CS333_P2
    switchuvm(p);

    int rc = stateListRemove(&ptable.ready[p->priority], p);
    if(rc != 0)
      panic("Process was not removed from RUNNABLE list in scheduler()\n");
    assertState(p, RUNNABLE);
    p->state = RUNNING;
    stateListAdd(&ptable.list[RUNNING], p);

    swtch(&(c->scheduler), p->context); // this is where it is handed to the cpu?
    switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
    c->proc = 0;

//    cprintf("idle = %d \n", idle);
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#elif defined(CS333_P3)
void
scheduler(void) // check to see if it is time to adjust priorities
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    //look for process to run
    acquire(&ptable.lock);
    p = ptable.list[RUNNABLE].head;
    if(p != 0){
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
#ifdef PDX_XV6
    idle = 0;  // not idle this timeslice
#endif // PDX_XV6
    c->proc = p;
#ifdef CS333_P2
    p->cpu_ticks_in = ticks;
#endif // CS333_P2
    switchuvm(p);

    int rc = stateListRemove(&ptable.list[RUNNABLE], p);
    if(rc != 0)
      panic("Process was not removed from RUNNABLE list\n");
    assertState(p, RUNNABLE);
    p->state = RUNNING;
    stateListAdd(&ptable.list[RUNNING], p);

    swtch(&(c->scheduler), p->context);
    switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
    c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif // CS333_P2
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)  // time budget may be set here, promotion in sched before head of list
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  #ifdef CS333_P2
  p->cpu_ticks_total += ticks - p->cpu_ticks_in;
  #endif // CS333_P2
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifdef CS333_P4
void
yield(void)  // demotion done here
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock

  int rc = stateListRemove(&ptable.list[RUNNING], curproc);
  if(rc != 0)
    panic("Process was not removed from appropriate RUNNING list inside of yield()\n");
  assertState(curproc, RUNNING);
  curproc->state = RUNNABLE;
  curproc->budget -= ticks - curproc->cpu_ticks_in; // change for cpu_ticks_total? changed from curproc->budget -= ticks - curproc->..

  if(MAXPRIO > 0){
    if(curproc->budget <= 0){
      if(curproc->priority > 0)
        curproc->priority--;
      curproc->budget = DEFAULT_BUDGET;
    }
  }

  stateListAdd(&ptable.ready[curproc->priority], curproc);

  sched();
  release(&ptable.lock);
}
#elif CS333_P3
void
yield(void)  // demotion done here
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock

  int rc = stateListRemove(&ptable.list[RUNNING], curproc);
  if(rc != 0)
    panic("Process was not removed from appropriate RUNNING list inside of yield\n");
  assertState(curproc, RUNNING);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.list[RUNNABLE], curproc);

  sched();
  release(&ptable.lock);
}
#else
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P4
void
sleep(void *chan, struct spinlock *lk)  //demotion done here
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;

  int rc = stateListRemove(&ptable.list[RUNNING], p);
  if(rc != 0)
    panic("Process was not removed from RUNNING list in sleep\n");
  assertState(p, RUNNING);
  p->state = SLEEPING;

  p->budget -= ticks - p->cpu_ticks_in;
  if(MAXPRIO > 0){
    if(p->budget <= 0){
      if(p->priority > 0)
        p->priority--;
      p->budget = DEFAULT_BUDGET;
    }
  }
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#elif defined(CS333_P3)
void
sleep(void *chan, struct spinlock *lk)  //demotion done here
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;

  int rc = stateListRemove(&ptable.list[RUNNING], p);
  if(rc != 0)
    panic("Process was not removed from RUNNING list in sleep\n");
  assertState(p, RUNNING);
  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif


// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P4
static void
wakeup1(void *chan) // should i deduct budget here?  NOOOO
{
  struct proc *p = ptable.list[SLEEPING].head;

  while(p!= 0){
    if(p->state == SLEEPING && p->chan == chan){
      int rc = stateListRemove(&ptable.list[SLEEPING], p); // from SLEEPING
      if(rc != 0)
        panic("Process was not removed from appropriate SLEEPING list in wakeup1\n");
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
    }
    p = p->next;
  }
}
#elif defined(CS333_P3)
static void
wakeup1(void *chan)
{
  struct proc *p = ptable.list[SLEEPING].head;

  while(p!= 0){
    if(p->state == SLEEPING && p->chan == chan){
      int rc = stateListRemove(&ptable.list[SLEEPING], p); // from SLEEPING
      if(rc != 0)
        panic("Process was not removed from appropriate SLEEPING list in wakeup1\n");
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
      stateListAdd(&ptable.list[RUNNABLE], p);
      //p = ptable.list[SLEEPING].head;
    }
   // else
    p = p->next;
  }
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifdef CS333_P4
int
kill(int pid)  // should i reduce budget here?
{
  acquire(&ptable.lock);
  struct proc *p;

  p = ptable.list[SLEEPING].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        int rc = stateListRemove(&ptable.list[SLEEPING], p); // from SLEEPING
        if(rc != 0)
          panic("Process was not removed from appropriate SLEEPING list in kill\n");
        assertState(p, SLEEPING);
        p->state = RUNNABLE;
        stateListAdd(&ptable.ready[p->priority], p);
      }
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }


  p = ptable.list[EMBRYO].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  /*p = ptable.list[RUNNABLE].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }*/
  for(int i = MAXPRIO; i >= 0; i--){  // RUNNABLE
    p = ptable.ready[i].head;
    while(p != 0){
      if(p->pid == pid){
        p->killed = 1;
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }

  p = ptable.list[RUNNING].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  p = ptable.list[ZOMBIE].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  release(&ptable.lock);
  return -1;
}
#elif defined(CS333_P3)
int
kill(int pid)
{
  acquire(&ptable.lock);
  struct proc *p;

  p = ptable.list[SLEEPING].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        int rc = stateListRemove(&ptable.list[SLEEPING], p); // from SLEEPING
        if(rc != 0)
        panic("Process was not removed from appropriate SLEEPING list in kill\n");
        assertState(p, SLEEPING);
        p->state = RUNNABLE;
        stateListAdd(&ptable.list[RUNNABLE], p);
      }
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }


  p = ptable.list[EMBRYO].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  p = ptable.list[RUNNABLE].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  p = ptable.list[RUNNING].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  p = ptable.list[ZOMBIE].head;
  while(p != 0){
    if(p->pid == pid){
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
#ifdef CS333_P4
void
procdump(void)  //TODO: possibly write a new procdump for CS333_P4
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  cprintf("\nPID\tName\t\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\tPC\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    uint difference = ticks - p->start_ticks;
    uint diffdiv = difference/1000;
    uint diffmod = difference%1000;
    uint cpudiv = p->cpu_ticks_total/1000;
    uint cpumod = p->cpu_ticks_total%1000;
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state]) //mark said do use this check with the else ??? for names. may ask for more clarification
      state = states[p->state];
    else
      state = "???";
    int ppid;
    // CS333_P2
    if(p->parent)
      ppid = p->parent->pid;
    else
      ppid = p->pid; // CS333_P2
    cprintf("%d\t%s\t\t%d\t%d\t%d\t%d\t", p->pid, p->name, p->uid, p->gid, ppid, p->priority);
    //adds leading 0's for diffmod
    cprintf("%d.", diffdiv);
    if(diffmod < 100 && diffmod >= 10) cprintf("0");
    else if(diffmod < 10 && diffmod > 0) cprintf("00");
    else if(diffmod == 0) cprintf("000");
    cprintf("%d\t", diffmod);
    //adds leading 0's for cpumod
    cprintf("%d.", cpudiv);
    if(cpumod < 100 && cpumod >= 10) cprintf("0");
    else if(cpumod < 10 && cpumod > 0) cprintf("00");
    else if(cpumod == 0) cprintf("000");
    cprintf("%d\t", cpumod);


    cprintf("%s\t%d\t", state, p->sz);

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf("%p ", pc[i]);
    }
    cprintf("\n");
  }
}
#else
void
procdump(void)  //TODO: possibly write a new procdump for CS333_P4
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  cprintf("\nPID\tName\t\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\tPC\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    uint difference = ticks - p->start_ticks;
    uint diffdiv = difference/1000;
    uint diffmod = difference%1000;
    uint cpudiv = p->cpu_ticks_total/1000;
    uint cpumod = p->cpu_ticks_total%1000;
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state]) //mark said do use this check with the else ??? for names. may ask for more clarification
      state = states[p->state];
    else
      state = "???";
    int ppid;
    // CS333_P2
    if(p->parent)
      ppid = p->parent->pid;
    else
      ppid = p->pid; // CS333_P2
    cprintf("%d\t%s\t\t%d\t%d\t%d\t", p->pid, p->name, p->uid, p->gid, ppid);
    //adds leading 0's for diffmod
    cprintf("%d.", diffdiv);
    if(diffmod < 100 && diffmod >= 10) cprintf("0");
    else if(diffmod < 10 && diffmod > 0) cprintf("00");
    else if(diffmod == 0) cprintf("000");
    cprintf("%d\t", diffmod);
    //adds leading 0's for cpumod
    cprintf("%d.", cpudiv);
    if(cpumod < 100 && cpumod >= 10) cprintf("0");
    else if(cpumod < 10 && cpumod > 0) cprintf("00");
    else if(cpumod == 0) cprintf("000");
    cprintf("%d\t", cpumod);


    cprintf("%s\t%d\t", state, p->sz);

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf("%p ", pc[i]);
    }
    cprintf("\n");
  }
}
#endif // CS333_P4

#ifdef CS333_P2
void
setuid(uint x)
{
  struct proc *p = myproc();
  acquire(&ptable.lock);
  p->uid = x;
  release(&ptable.lock);
}

void
setgid(uint x)
{
  struct proc *p = myproc();
  acquire(&ptable.lock);
  p->gid = x;
  release(&ptable.lock);
}

int
get_ppid(void)
{
  acquire(&ptable.lock);
  struct proc *curproc = myproc();
  int ppid = curproc->parent ? curproc->parent->pid : curproc->pid;
  release(&ptable.lock);
  return ppid;
}

int
get_uid(void)
{
  acquire(&ptable.lock);
  struct proc *curproc = myproc();
  int uid = curproc->uid;
  release(&ptable.lock);
  return uid;
}

int
get_gid(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  int gid = curproc->gid;
  release(&ptable.lock);
  return gid;
}

int
getptable(int max, struct uproc* table)
{
  struct proc *curproc;
  int count;
  acquire(&ptable.lock);

  // need to keep track of the count to return number of entries in the uproc* table
  for(count = 0,curproc = ptable.proc; curproc < &ptable.proc[NPROC]; curproc++){
    if(count >= max)
      break;
    if((curproc->state == UNUSED) || (curproc->state == EMBRYO))
      continue;//should this be continue?
    table[count].pid = curproc->pid;
    table[count].uid = curproc->uid;
    table[count].gid = curproc->gid;
#ifdef CS333_P4
    table[count].priority = curproc->priority;
#endif // CS333_P5
    table[count].ppid = curproc->parent ? curproc->parent->pid : curproc->pid;
    table[count].elapsed_ticks = ticks - curproc->start_ticks;
    table[count].CPU_total_ticks = curproc->cpu_ticks_total;
    table[count].size = curproc->sz;
    safestrcpy(table[count].name, curproc->name, sizeof(curproc->name));

    safestrcpy(table[count].state, states[curproc->state], sizeof(table[count].state));
    count++;
    }
  release(&ptable.lock);
  return count;
}
#endif// CS333_P2

#ifdef CS333_P3
// list management helper functions
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    cprintf("list is NULL inside of stateListRemove\n");
    return -1;
  }
  struct proc* current = (*list).head;
  struct proc* previous = 0;

 if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we’ve removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if(current == NULL){
    cprintf("hello from statelistREmove down low\n");
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn’t point into the list.
  p->next = NULL;

  return 0;
}

static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
  #ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
  #endif
}

static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}

static void
assertState(struct proc *p, enum procstate state)
{
  if(p->state == state)
    return;
  panic("State is incorrect");
}

void
printFreeList()
{
  acquire(&ptable.lock);
  int numFree = 0;
  struct proc *p = ptable.list[UNUSED].head;
  while(p){
      numFree++;
      p = p->next;
  }
  cprintf("Free List Size: %d processes\n", numFree);
  release(&ptable.lock);
}

#ifdef CS333_P4
void
printRunnableList()
{
  acquire(&ptable.lock);
  struct proc *p;
  cprintf("\nReady List Processes:\n");
  for(int i = MAXPRIO; i >=0; i--){
    cprintf("%d: ", i);
    p = ptable.ready[i].head;
    while(p != 0){
      cprintf("(%d, %d)", p->pid, p->budget);
      if(p->next)
        cprintf("->");
      p = p->next;
    }
    cprintf("\n");
  }
  release(&ptable.lock);
}
#else
void
printRunnableList()
{
  acquire(&ptable.lock);
  struct proc *p = ptable.list[RUNNABLE].head;
  cprintf("Ready List Processes:\n");
  while(p){
    if(p != ptable.list[RUNNABLE].tail)
      cprintf("%d->", p->pid);
    if(p == ptable.list[RUNNABLE].tail)
      cprintf("%d\n", p->pid);
    p = p->next;
  }
  release(&ptable.lock);
}
#endif

void
printSleepList()
{
  acquire(&ptable.lock);
  struct proc *p = ptable.list[SLEEPING].head;
  cprintf("Sleep List Processes:\n");
  while(p){
    if(p != ptable.list[SLEEPING].tail)
      cprintf("%d-> ", p->pid);
    if(p == ptable.list[SLEEPING].tail)
      cprintf("%d\n", p->pid);
    p = p->next;
  }
  release(&ptable.lock);
}

void
printZombieList()
{
  acquire(&ptable.lock);
  struct proc *p = ptable.list[ZOMBIE].head;
  cprintf("Zombie List Processes:\n");
  while(p){
    if(p != ptable.list[ZOMBIE].tail)
      cprintf("(PID%d, PPID%d)->", p->pid, p->parent->pid);
    if(p == ptable.list[ZOMBIE].tail)
      cprintf("(PID%d, PPID%d)\n", p->pid, p->parent->pid);
    p = p->next;
  }
  release(&ptable.lock);
}
#endif // CS333_P3

#ifdef CS333_P4
int
setpriority(int pid, int priority){
  int rc = -1; //default error code if something is incorrect
  struct proc *p;

  if(priority < 0 || priority > MAXPRIO)
    return -1;

  acquire(&ptable.lock);
  //need to find process in process lists
  rc = search4Proc(pid, &p);  // only need to search runnable, running, and sleep lists?
  if(rc == 0){
    if(p->state == RUNNABLE && p->priority != priority){
        int returncode = stateListRemove(&ptable.ready[p->priority], p); // from SLEEPING
        if(returncode != 0)
          panic("Process was not removed from appropriate ready[i] list in setpriority()\n");
        assertState(p, RUNNABLE);
        p->state = RUNNABLE;
        p->priority = priority;
        p->budget = DEFAULT_BUDGET;
        stateListAdd(&ptable.ready[p->priority], p);
    }
    else if (p->state != RUNNABLE){
      p->priority = priority;
      p->budget = DEFAULT_BUDGET; // probably needs to be inside an if loop if search4proc succeeds
    }
  }

  release(&ptable.lock);
  return rc;
}

static int
search4Proc(int pid, struct proc **p){  //only for runnable, running, and sleep lists?
  //make search lock is still being held from setpriority()
  if(!holding(&ptable.lock))
    panic("search4Proc does not have ptable.lock");

  int rc = -1;
  struct proc *curr;

  curr = ptable.list[RUNNING].head;
  while(curr != 0){
    if(curr->pid == pid){
      *p = curr;
      rc = 0;
      return rc;
    }
    curr = curr->next;
  }

  curr = ptable.list[SLEEPING].head;
  while(curr != 0){
    if(curr->pid == pid){
      *p = curr;
      rc = 0;
      return rc;
    }
    curr = curr->next;
  }

  for(int i = MAXPRIO; i >= 0; i--){
    curr = ptable.ready[i].head;
    while(curr != 0){
      if(curr->pid == pid){
        *p = curr;
        rc = 0;
        return rc;
      }
      curr = curr->next;
    }
  }
  return rc;
}

int
getpriority(int pid){
  // the return value is the priority of the process or -1 if the process is not active (RUNNING?)
  // int rc = getpriority(getpid());  this is a simple way for a process to get its own priority
  int rc = -1;
  struct proc *p;

  acquire(&ptable.lock);
  //need to find process in process lists
  rc = search4Proc(pid, &p);  // only need to search runnable, running, and sleep lists?
  if(rc == 0){
    release(&ptable.lock);
    return p->priority;
  }

  release(&ptable.lock);
  return rc;
}

static void
adjustPriority(){
  struct proc *curr;
  struct proc *temp = NULL;

  if(!holding(&ptable.lock))
    panic("adjustPriority() does not have ptable.lock");

  //RUNNABLE
  for(int i = MAXPRIO; i >= 0 ; i--){
    curr = ptable.ready[i].head;
    while(curr != 0){
      if(curr->priority < MAXPRIO){
        temp = curr->next;
        int rc = stateListRemove(&ptable.ready[curr->priority], curr);
        if(rc != 0)
          panic("Process was not removed from ready list in adjustPriority()1\n");
        assertState(curr, RUNNABLE);
        curr->state = RUNNABLE;
        curr->priority++;
        curr->budget = DEFAULT_BUDGET;
        stateListAdd(&ptable.ready[curr->priority], curr);
        curr = temp;
        continue;
      }
      curr = curr->next;
    }
  }

  curr = ptable.list[RUNNING].head;
  while(curr != 0){
    if(curr->priority < MAXPRIO){
      curr->priority++;
      curr->budget = DEFAULT_BUDGET;
    }
    curr = curr->next;
  }

  curr = ptable.list[SLEEPING].head;
  while(curr != 0){
    if(curr->priority < MAXPRIO){
      curr->priority++;
      curr->budget = DEFAULT_BUDGET;
    }
    curr = curr->next;
  }
}
#endif // CS333_P4
