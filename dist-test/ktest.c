#ifdef CS333_P3
#include "types.h"
#include "user.h"
#include "uproc.h"

void
ktest(void){
  int kids[NPROC];
  int pid;
  int numchild = 0;

  while( (pid = fork()) != -1){
    if(pid == 0)
      while(1);
    kids[numchild++] = pid;
  }

  for(numchild -= 1; numchild >= 0; numchild--){
    kill(kids[numchild]);
    printf(1, "Marked %d child with PID of %d for death\n", numchild, kids[numchild]);
    sleep(2*TPS);

    while(wait() != kids[numchild])
      ;

    printf(1, "Reaped [%d] child with PId %d\n", numchild, kids[numchild]);
    sleep(2*TPS);
  }

  printf(1, "Parent Reaping complete..\n");
  sleep(5*TPS);

}

int main(int argc,char *argv[])
{
  ktest();
  exit();
}
#endif
