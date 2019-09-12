#ifdef CS333_P2
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "***** In %s: my uid is %d\n\n", argv[0], getuid());
//  printf(1, "***** In %s: my gid is %d\n\n", argv[0], getgid());
 // printf(1, "***** In %s: Will set uid to 10\n\n", argv[0], setuid(10));
 // printf(1, "***** In %s: Will set gid to 9\n\n", argv[0], setgid(9));
  //printf(1, "***** In %s: my uid is %d\n\n", argv[0], getuid());
  //printf(1, "***** In %s: my gid is %d\n\n", argv[0], getgid());
  exit();
}
#endif
