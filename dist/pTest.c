#ifdef CS333_P4
#include "types.h"
#include "user.h"
#include "uproc.h"


void pTest(void){
  printf(1, "setpriority() test.......\n");
  sleep(2000);
  //int process = getpid();
  int newprio = 1;
  printf(1, "Process %d will attempt to change its priority to value other than 0\n", getpid());
  int rc = setpriority(getpid(), newprio);
  if(rc != 0)
    printf(1, "Value of return code of setpriority(%d, %d) is %d indicating failure.\n", getpid(), newprio, rc);
  sleep(2000);
}

int main(int argc,char *argv[]){
  pTest();
  exit();
}
#endif
