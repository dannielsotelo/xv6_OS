#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
  int owner;

  if(argc > 3 || argc < 3){
    printf(1, "Too few or too many arguments\n");
    exit();
  }

  owner = atoi(argv[2]);

  if(owner < 0 || owner > 32767){
    printf(1, "Invalid value of %d for chown\n", owner);
    exit();
  }
  chown(argv[1], owner);

  exit();
}
#endif // CS333_p5
