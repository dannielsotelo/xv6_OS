#ifdef CS333_P3
#include "types.h"
#include "user.h"
#include "uproc.h"


#define SLEEP_TIME 7*TPS

static void
wtest(void)
{
  int pid;
  printf(1, "\n----------\nRunning Wait Test\n----------\n");
  printf(1, "Forking process with PID %d..\n", getpid());
  pid = fork();
  if(pid == 0) {
    printf(1, "Press CTRL+F once to display free list and then CTRL+P.\n");
    sleep(SLEEP_TIME);
  }
  else {
    wait();
    printf(1, "Child has finished.\n");
    printf(1, "Press CTRL+F once to display free list and then CTRL+P.\n");
    sleep(SLEEP_TIME);
  }
  exit();
}

int main(int argc,char *argv[]){
  wtest();
  exit();
}
#endif
