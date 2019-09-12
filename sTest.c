//This test function is based off of code provided by Amie R in the CS333 slack channel
#ifdef CS333_P3
#include "types.h"
#include "user.h"
#include "uproc.h"


void sleepTest(void)
{
  printf(1, "Process: %d is being moved to sleep state list\n", getpid());
  sleep(10*TPS);
  printf(1, "Process: Is now awake and exiting\n");
}

int main(int argc,char *argv[]){
  sleepTest();
  exit();
}
#endif
