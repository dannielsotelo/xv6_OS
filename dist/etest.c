//etest.c based of code provided by Michael, fellow student in CS333. Code was provided in pdx-cs CS333-fall2018 slack channel
#ifdef CS333_P3
#include "types.h"
#include "user.h"
#include "uproc.h"


#define SLEEP_TIME 7*TPS

static void
etest(void)

{
  int pid;
  printf(1, "\n----------\nRunning Exit Test\n----------\n");
  printf(1, "Forking process with PID %d..\n", getpid());
  //sleep(5*TPS);
  pid = fork();
  if(pid == 0) {
    printf(1, "Exiting child process.\n\n");
   // sleep(5*TPS);
    exit();
  }
  else {
    sleep(100);
    printf(1, "Press CTRL+Z once to display zombie list.\n");
    sleep(SLEEP_TIME);
    wait();
  }
  exit();
}

int main(int argc, char *argv[]){
  etest();
  exit();
}
#endif
