#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#ifdef CS333_P5
#include "print_mode.c"
#endif // CS333_P5

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

#ifdef CS333_P5
  printf(1, "mode\t\tname\t\tuid\tgid\tinode\tsize\n");
#endif // CS333_P5

  switch(st.type){
  case T_FILE:  // for file types, will run through all cases until a break is reached. hence t-file and t-dev will both run
  case T_DEV: // for device types
#ifdef CS333_P5
    print_mode(&st);
    printf(1, "\t%s\t%d\t%d\t%d\t%d\n", fmtname(path), st.uid, st.gid, st.ino, st.size);
#else
    printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
#endif
    break;

  case T_DIR:  // for directory types
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf(1, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf(1, "ls: cannot stat %s\n", buf);
        continue;
      }
#ifdef CS333_P5
    print_mode(&st);
    printf(1, "\t%s\t%d\t%d\t%d\t%d\n", fmtname(buf), st.uid, st.gid, st.ino, st.size);  // we are copying to buffer from path on line 67
#else
      printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
#endif
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit();
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit();
}