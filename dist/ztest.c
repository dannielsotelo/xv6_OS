#ifdef CS333_P3
#include "types.h"
#include "user.h"
#include "uproc.h"

void
ztest(void){
  /*Just spawn a child that goes into an infinite loop, then kill it, then have the parent e.g. sleep for a
  bit before calling wait (or exiting) so that you have time to print the zombie list while the child is still on it.*/

  //int parent;
  //int child = fork();
  //int clean = 1;

  while(1){
    //if(clean >0){
    printf(1, "XXXXXXXXXXX\n");
    sleep(7*TPS);
    int child = fork();
    sleep(7*TPS);
    printf(1, "XXXXXXXXXXX\n");
    //printf(1, "about to kill child process\n");
    kill(child);
    printf(1, "XXXXXXXXXXX\n");
    //printf(1, "about to sleep\n");
    sleep(7*TPS);
    //kill(parent);
    exit();
    //}
  }
}

int main(int argc,char *argv[]){
  ztest();
  exit();
}
#endif
