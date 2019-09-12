#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
  int group;

  if(argc > 3 || argc < 3){
    printf(1, "Too few or too many arguments\n");
    exit();
  }

  group = atoi(argv[2]);

  if(group < 0 || group > 32767){
    printf(1, "Invalid value of %d for chgrp\n", group);
    exit();
  }

  chgrp(argv[1], group);

  exit();
}
#endif // CS333_p5
