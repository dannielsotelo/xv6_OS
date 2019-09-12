#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#ifdef PDX_XV6
#include "pdx-kernel.h"
#endif // PDX_XV6
#ifdef CS333_P2
#include "uproc.h"
#endif // CS333_P2
int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
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

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

#ifdef PDX_XV6
// Turn off the computer
int
sys_halt(void)
{
  cprintf("Shutting down ...\n");
  outw( 0x604, 0x0 | 0x2000);
  return 0;
}
#endif // PDX_XV6

#ifdef CS333_P1
int
sys_date(void)
{
  struct rtcdate *d;
  if(argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0)
    return -1;

  cmostime(d);
  return 0;
}
#endif // CS333_P1

#ifdef CS333_P2 // getprocs should be implemented in proc.c gets should call helper function in getX in proc.c
int
sys_getuid(void)  // helper for get
{
  return get_uid();
}

int
sys_getgid(void)
{
  return get_gid();
}

int
sys_getppid(void)//ptable lock in proc.c
{
  int ppid;
  if(argint(0, &ppid)<0)
    return -1;

  ppid = get_ppid();
  return ppid;
}

int
sys_setuid(void)
{
  uint uID;
  if(argint(0, (void*)&uID)<0)
    return -1;
  if(uID < 0 || uID > 32767)
    return -1;
  setuid(uID);
  return 0;
}

int
sys_setgid(void)
{
  uint gID;
  if(argint(0, (void*)&gID)< 0)
    return -1;

  if(gID < 0 || gID > 32767)
    return -1;
  setgid(gID);
  return 0;
}

int
sys_getprocs(void)
{
  struct uproc* up;
  uint max;

  if(argint(0, (int*)&max)<0)
    return -1;
  if(argptr(1, (void*)&up, max*sizeof(struct uproc))<0)
    return -1;

  return getptable(max, up);
}
#endif // CS333_P2

#ifdef CS333_P4
int
sys_setpriority(void)
{
  //this system call will have the effect of setting the priority for an active process with PID pid to priority and resetting the budget to the deafult value.
  //return an error (probably -1) if the values for pid or priority are not correct
  //this means that the system call is required to enfore bounds checking on priority value; you may not assume that a user program only pases correct values

  // int rc = setpriority(getpid(), newPriority);  this is for a process to set its own priority, newPriority is an integer

  int pid;
  int newpriority;

  if(argint(0, &pid)<0 || argint(1, &newpriority) < 0) //could possibly remove int*?
    return -1;

  int rc = setpriority(pid, newpriority);
  return rc;
}

int
sys_getpriority(void)
{
  // the return value is the priority of the process or -1 if the process is not active (RUNNING?)
  // int rc = getpriority(getpid());  this is a simple way for a process to get its own priority

  int pid;
  if(argint(0, (int*)&pid)<0)
    return -1;

  int rc = getpriority(pid);
  return rc;
}
#endif // CS333_P4
