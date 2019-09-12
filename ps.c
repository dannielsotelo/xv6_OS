// new file for ps command in project 2
#ifdef CS333_P2
#include "types.h"
#include "stat.h"
#include "user.h"
#include "uproc.h"

//static void doTesting();
static void printheader();
static void printprocesses(struct uproc *print, int tablesize);
static void printelapmod(uint elapdiv, uint elapmod);
static void printcpumod(uint cpudiv, uint cpumod);
//ps should call getproc system call and get info and then print it similar to proc dump

int
main (int argc, char *argv[])
{
  int max = 72;
  struct uproc *table = malloc(max * sizeof(struct uproc));

  int tablesize = getprocs(max, table);

  if(tablesize == 0 || table == NULL){
    printf(2, "Table size is 0 or table failed to load correctly\n");
    exit();
  }

  printheader();
  printprocesses(table, tablesize);
 // doTesting();
  free(table);
  exit();
}

static void printheader(){
  printf(1, "PID\tName\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\n");
}

static void printprocesses(struct uproc *print, int tablesize){
  for(int i = 0; i < tablesize; i++){
    uint elapdiv = print[i].elapsed_ticks/1000;
    uint elapmod = print[i].elapsed_ticks%1000;
    uint cpudiv = print[i].CPU_total_ticks/1000;
    uint cpumod = print[i].CPU_total_ticks%1000;

    #ifdef CS333_P4
    printf(1, "%d\t%s\t%d\t%d\t%d\t%d\t", print[i].pid, print[i].name, print[i].uid, print[i].gid, print[i].ppid, print[i].priority);
    printelapmod(elapdiv, elapmod);
    printcpumod(cpudiv, cpumod);
    printf(1, "%s\t%d\n", print[i].state, print[i].size);
    #else
    printf(1, "%d\t%s\t%d\t%d\t%d\t", print[i].pid, print[i].name, print[i].uid, print[i].gid, print[i].ppid);
    printelapmod(elapdiv, elapmod);
    printcpumod(cpudiv, cpumod);
    printf(1, "%s\t%d\n", print[i].state, print[i].size);
    #endif
  }
}

static void printelapmod(uint elapdiv, uint elapmod){
  printf(1, "%d.", elapdiv);
  if(elapmod < 100 && elapmod >= 10) printf(1, "0");
  else if(elapmod < 10 && elapmod > 0) printf(1, "00");
  else if(elapmod == 0) printf(1, "000");
  printf(1, "%d\t", elapmod);
}

static void printcpumod(uint cpudiv, uint cpumod){
  printf(1, "%d.", cpudiv);
  if(cpumod < 100 && cpumod >= 10) printf(1, "0");
  else if(cpumod < 10 && cpumod > 0) printf(1, "00");
  else if(cpumod == 0) printf(1, "000");
  printf(1, "%d\t", cpumod);
}


/* commented out so testing does not run on every execution of ps
static void
doTesting() {
  int i, j, rc;
  int max[] = {-1, 0, 1, 16, 72 ,NPROC, NPROC + 10, 10000};
  int MAXCOUNT = sizeof(max)/sizeof(max[i]);
  struct uproc *table;

  for (i=0; i<MAXCOUNT; i++) {
    printf(1, "\n**** MAX: %d\n", max[i]);
    if (max[i] <= 0) {  // don't make malloc() barf
      printf(2, "Invalid.  MAX == %d. If xv6 malloc() did error checking ... \n", max[i]);
      continue;
    }

    table = malloc(max[i] * sizeof(struct uproc));
    if (0 == table) {
      printf(2, "Error: malloc call failed. %s at line %d\n", __FILE__, __LINE__);
      return;
    }

    rc = getprocs(max[i], table);
    if (rc < 0) {
      printf(1, "Error: getprocs call failed for MAX == %d. %s at line %d\n",
          max[i], __FILE__, __LINE__);
      return;
    }
    if (rc != max[i]) {
      printf(1, "Info: getprocs() asked for %d and %d returned\n", max[i], rc);
    }

    //printf(1, HEADER);
    sleep(5 * TPS);  // now type control-p
    printheader();
    for (j=0; j<rc; j++)
      printprocesses(table, MAXCOUNT);
      //printTableEntry(&table[j]);
    free(table);
  }
  // Invalid test based on not malloc'ing enough memory. allocate 10, ask for 11
  table = malloc(10 * sizeof(struct uproc));
  rc =getprocs(11, table);
  if (rc < 0) {
    printf(1, "\n**** MAX: 11 but malloc for 10\n");
    printf(1, "getprocs call failed for MAX == 11 and malloc'd for 10 as expected.\n");
    return;
  }
  else {
    printf(1, "Error: getprocs should have failed but did not. %s at line %d\n",
        __FILE__, __LINE__);
  }
  return;
}*/
#endif //CS333_P2
