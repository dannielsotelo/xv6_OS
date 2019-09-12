#ifdef CS333_P5
#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
//  int mode;

  // too many or few arguments
  if(argc > 3 || argc < 3){
    printf(1, "Too few or too many arguments\n");
    exit();
  }

  char * mode = argv[2];

  if(strlen(mode) <  4 || strlen(mode) > 4)
      return -1;
  for(int i = 0; i < 4; i++){
    if(mode[i] < '0' || mode[i] > '7'){
      printf(1, "Invalid value of %d for chmod\n", mode);
      exit();
    }
  }

  // mode is converted to octal
  chmod(argv[1], atoo(argv[2]));
  exit();
}
#endif // CS333_p5
