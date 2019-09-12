// This test is based on code provided by Christopher Teters and added functions by Mirko Draganic and p4-test.c
#ifdef CS333_P4
#include "types.h"
#include "user.h"
#include "param.h"

const int plevels = MAXPRIO;

void
sleepMessage(int time, char message[])
{
  printf(1, message);
  sleep(time);
}

int
createInfiniteProc()
{
  int pid = fork();
  //setpriority(pid, 0);
  if(pid == 0)
    while(1);
  printf(1, "Process %d created...\n", pid);
  return pid;
}

int
createProc()
{
  int pid = fork();
  if(pid == 0)
    while(1);
  printf(1, "Process %d created...\n", pid);
  return pid;
}

void
cleanupProcs(int pid[], int max)

{
  int i;
  for(i = 0; i < max; i++)
    kill(pid[i]);

  while(wait() > 0);
}

//Based of coded provided by Christopher Teters
void
setprioTest(){
  int i;
  int max = 3;
  int pid[max];
  int invalidP = 8;
  int invalidPID = 22;
  //int newpriority = 6;
  //int thisPID = getpid();

  printf(1, "+=+=+=+=+=+=+=+=+=+=+=+=+=+=\n");
  printf(1, "| Start: Set Priority Test |\n");
  printf(1, "+=+=+=+=+=+=+=+=+=+=+=+=+=+=\n");

  printf(1, "\nCurrent MAXPRIO = %d\n", MAXPRIO);

  for(i = 0; i < max; i++){
    pid[i] = createInfiniteProc();
  }

  sleepMessage(1000, "xxxxxxxxxxxxxxxxxxxxxxxxxx\n");
  printf(1, "Will set PID %d to invalid priority of %d\n", getpid(), invalidP);
  int rc =setpriority(getpid(), invalidP);
  if(rc != 0)
    printf(1, "return code of setpriority() with invalid priority = %d\n", rc);

  sleepMessage(1000, "xxxxxxxxxxxxxxxxxxxxxxxxxx\n");

  printf(1, "Will set invalid PID %d to valid priority of %d\n", invalidPID, 0);
  rc =setpriority(invalidPID, 0);
  if(rc != 0)
    printf(1, "return code of set priority of invalid pid= %d\n", rc);
  sleepMessage(1000, "xxxxxxxxxxxxxxxxxxxxxxxxxx\n");
  cleanupProcs(pid, max);

  printf(1, "+=+=+=+=+=+=+=+=+=+=+=+=+=+=\n");
  printf(1, "| End: Set Priority Test |\n");
  printf(1, "+=+=+=+=+=+=+=+=+=+=+=+=+=+=\n");
  ///kill(thisPID);
  exit();
}

//Based off of code provided by Mirko Draganic
void
setToSamePriority(void)
{
  int i;
  int max = 8;
  int pid[max];
  //int samepriority = 6;

  printf(1, "\n");
  printf(1, "Testing Setting to same priority\n");
 // sleepMessage(2000, "Sleeping... ctrl-p\n");

  for(i = 0; i < max/2; i++)
    pid[i] = createProc();

  sleepMessage(2000, "Sleeping... ctrl-p\n");

  printf(1, "\n");
  printf(1, "Testing Setting PID: 4 To Priority 6\n", getpid());
  //sleepMessage(2000, "Sleeping... ctrl-p\n");

  setpriority(5, 3);
  setpriority(4,3);
  sleepMessage(2000, "Sleeping... ctrl-p OR ctrl-r\n");

  setpriority(4,3);
  sleepMessage(2000, "Sleeping... ctrl-p OR ctrl-r\n");
  /*for(i = 0; i <= MAXPRIO; i++)
  {
    printf(1, "\nCalling setpriority(4, 3)\n");
    setpriority(4, 3);
    sleepMessage(2000, "Sleeping... ctrl-p OR ctrl-r\n");
  }*/

  if(MAXPRIO == 0)
    sleepMessage(2000, "Sleeping... ctrl-p OR ctrl-r\n");

  //sleep(3000);

  cleanupProcs(pid, max);
  printf(1, "TEST END\n");
}

//Based off of code provided by Mirko Draganic
void
getPriorityProcess(void)
{
  sleepMessage(3000, "\nSleeping... ctrl-p OR ctrl-r\n");


  printf(1, "\nTest getpriority() on current process. Using getpid() current PID = %d\n", getpid());
  printf(1, "getpriority(getpid()) = %d\n", getpriority(getpid()));

  printf(1, "\nTest getpriority() on init PID 1.\n");
  printf(1, "getpriority(1) = %d\n", getpriority(1));

  printf(1, "\nTest getpriority() with an invalid pid = 10 will return a -1.\n");
  printf(1, "getpriority(10) = %d\n", getpriority(10));


  sleepMessage(3000, "\nSleeping... ctrl-p OR ctrl-r\n");
}




int
main(int argc, char *argv[])
{
  //setprioTest();
  //setToSamePriority();
  getPriorityProcess();
  exit();
}
#endif
