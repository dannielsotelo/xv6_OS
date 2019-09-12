#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "date.h"

int main(int argc, char *argv[])
{
  int start = uptime();
  int end;
  int pid = 0;

  if(argv[1] == 0x00)
    ;
  else
    pid = fork();
  if (pid < 0)
    exit();
  if(pid == 0)
    exec(argv[1] , argv +1);
  else
    wait();

  end = uptime();
  if(argv[1] == 0x0)
    ;

  int seconds = (end - start)/1000;
  int decseconds = (end - start)%1000;

  printf(1, "%s", argv[1]);
  printf(1, " ran in %d.%d seconds\n", seconds, decseconds);
  exit();
}
#endif //CS333_P2
