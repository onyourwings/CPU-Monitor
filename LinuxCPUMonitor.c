#define _BSD_SOURCE
#include <errno.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>  
#include <fcntl.h>

#include "LinuxCPUMonitor.h"

#define PROCSTATFILE  "/proc/stat"
#define PROCMEMINFOFILE  "/proc/meminfo"
#define PROC_LINE_LENGTH 4096
#define String_startsWith(s, match) (strncmp((s),(match),strlen(match)) == 0)
#define String_contains_i(s1, s2) (strcasestr(s1, s2) != NULL)
#define MAX(a,b) ((a)>(b)?(a):(b))
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#define DETAILED_CPU_TIME 1
#define ACCOUNT_GUEST_IN_CPU_METTER 1


void* xCalloc(size_t nmemb, size_t size) {
   void* data = calloc(nmemb, size);
   if (!data && nmemb > 0 && size > 0) {
      printf("calloc failed");
   }
   return data;
}
int main(int argc, char** argv) {

   LinuxProcessList* linuxpl = ProcessList_new();
   ProcessList* pl = &(linuxpl->super);
   // linuxpl->super.cpuCount++;
   char* cpuBuffer = malloc(sizeof(char)*500);
   char* memBuffer = malloc(sizeof(char)*100);
   cpuBuffer[0] = '\0';
   memBuffer[0] = '\0';
   while(1) {
      // usleep(100000);
      sleep(2);
      int period = LinuxProcessList_scanCPUTime(linuxpl);
      LinuxProcessList_scanMemoryInfo(pl);
      memset(cpuBuffer, 0, sizeof(cpuBuffer));
      memset(memBuffer, 0, sizeof(memBuffer));
      strcat(cpuBuffer, "echo @@@ cpu_usg ");
      strcat(memBuffer, "echo @@@ mem_usg ");
      for(int cpu_index=0; cpu_index < linuxpl->super.cpuCount; cpu_index++) {
         char cpuLoadStr[10];
         sprintf(cpuLoadStr, "%d_%05.1f ", cpu_index, Platform_setCPUValues(linuxpl, cpu_index));
         strcat(cpuBuffer, cpuLoadStr);
      }
      system(cpuBuffer);
      if(pl->totalMem == 0) {
         pl->totalMem = 1;
      }
      char memLoadStr [10];
      sprintf(memLoadStr, "%5.1f\n", (((double) pl->usedMem)/(pl->totalMem))*100) ;
      strcat(memBuffer, memLoadStr);
      system(memBuffer);
   }
}


LinuxProcessList* ProcessList_new() {
   LinuxProcessList* this = xCalloc(1, sizeof(LinuxProcessList));
   ProcessList* pl = &(this->super);
   ProcessList_init(pl);

   // Update CPU count:
   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      printf("%s\n", "Error! Cannot open /proc/stat/");
   }
   int cpus = 0;
   do {
      char buffer[PROC_LINE_LENGTH + 1];
      if (fgets(buffer, PROC_LINE_LENGTH + 1, file) == NULL) {
         printf("%s\n", "No btime");
      } else if (String_startsWith(buffer, "cpu")) {
         cpus++;
      } else if (String_startsWith(buffer, "btime ")) {
         sscanf(buffer, "btime %lld\n", &btime);
         break;
      }
   } while(true);

   fclose(file);

   pl->cpuCount = MAX(cpus - 1, 1);
   this->cpus = xCalloc(cpus, sizeof(CPUData));
   printf("cpuCount = %d \n", pl->cpuCount);
   for (int i = 0; i < cpus; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }
   return this;
}

double Platform_setCPUValues(LinuxProcessList* linuxpl, int cpu) {
   CPUData* cpuData = &(linuxpl->cpus[cpu]);
   double total = (double) ( cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod);
   double percent;
   double* values = malloc(sizeof(double) * CPU_METER_ITEMCOUNT);
   memset(values, 0, sizeof(double) * CPU_METER_ITEMCOUNT);
   double* v = values;
   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (DETAILED_CPU_TIME) {
      v[CPU_METER_KERNEL]  = cpuData->systemPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->irqPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = cpuData->softIrqPeriod / total * 100.0;
      v[CPU_METER_STEAL]   = cpuData->stealPeriod / total * 100.0;
      v[CPU_METER_GUEST]   = cpuData->guestPeriod / total * 100.0;
      v[CPU_METER_IOWAIT]  = cpuData->ioWaitPeriod / total * 100.0;
      // Meter_setItems(this, 8);
      if (ACCOUNT_GUEST_IN_CPU_METTER) {
         percent = v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6];
      } else {
         percent = v[0]+v[1]+v[2]+v[3]+v[4];
      }
   } else {
      v[2] = cpuData->systemAllPeriod / total * 100.0;
      v[3] = (cpuData->stealPeriod + cpuData->guestPeriod) / total * 100.0;
      // Meter_setItems(this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   }
   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;
   return percent;
}

static inline double LinuxProcessList_scanCPUTime(LinuxProcessList* this) {

   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      printf("%s\n", "Error! Cannot open /proc/stat/");
   }
   int cpus = this->super.cpuCount;
   assert(cpus > 0);
   for (int i = 0; i <= cpus; i++) {
      char buffer[PROC_LINE_LENGTH + 1];
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;
      ioWait = irq = softIrq = steal = guest = guestnice = 0;
      // Depending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      char* ok = fgets(buffer, PROC_LINE_LENGTH, file);
      if (!ok) buffer[0] = '\0';
      // if (i == 0)
      //    sscanf(buffer,   "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu",         &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      // else {
      int cpuid;
      sscanf(buffer, "cpu%4d %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         // assert(cpuid == i);
      // printf("i = %d, cpuid  = %d\n", i, cpuid);
      // }
      // Guest time is already accounted in usertime
      usertime = usertime - guest;
      nicetime = nicetime - guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      unsigned long long int idlealltime = idletime + ioWait;
      unsigned long long int systemalltime = systemtime + irq + softIrq;
      unsigned long long int virtalltime = guest + guestnice;
      unsigned long long int totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      // printf("total time = %d\n", totaltime);
      if(i>0) {
         CPUData* cpuData = &(this->cpus[i-1]);
      // Since we do a subtraction (usertime - guest) and cputime64_to_clock_t()
      // used in /proc/stat rounds down numbers, it can lead to a case where the
      // integer overflow.
      #define WRAP_SUBTRACT(a,b) (a > b) ? a - b : 0
         cpuData->userPeriod = WRAP_SUBTRACT(usertime, cpuData->userTime);
         cpuData->nicePeriod = WRAP_SUBTRACT(nicetime, cpuData->niceTime);
         cpuData->systemPeriod = WRAP_SUBTRACT(systemtime, cpuData->systemTime);
         cpuData->systemAllPeriod = WRAP_SUBTRACT(systemalltime, cpuData->systemAllTime);
         cpuData->idleAllPeriod = WRAP_SUBTRACT(idlealltime, cpuData->idleAllTime);
         cpuData->idlePeriod = WRAP_SUBTRACT(idletime, cpuData->idleTime);
         cpuData->ioWaitPeriod = WRAP_SUBTRACT(ioWait, cpuData->ioWaitTime);
         cpuData->irqPeriod = WRAP_SUBTRACT(irq, cpuData->irqTime);
         cpuData->softIrqPeriod = WRAP_SUBTRACT(softIrq, cpuData->softIrqTime);
         cpuData->stealPeriod = WRAP_SUBTRACT(steal, cpuData->stealTime);
         cpuData->guestPeriod = WRAP_SUBTRACT(virtalltime, cpuData->guestTime);
         cpuData->totalPeriod = WRAP_SUBTRACT(totaltime, cpuData->totalTime);
      #undef WRAP_SUBTRACT
         cpuData->userTime = usertime;
         cpuData->niceTime = nicetime;
         cpuData->systemTime = systemtime;
         cpuData->systemAllTime = systemalltime;
         cpuData->idleAllTime = idlealltime;
         cpuData->idleTime = idletime;
         cpuData->ioWaitTime = ioWait;
         cpuData->irqTime = irq;
         cpuData->softIrqTime = softIrq;
         cpuData->stealTime = steal;
         cpuData->guestTime = virtalltime;
         cpuData->totalTime = totaltime;
      }
   }
   double period = (double)this->cpus[0].totalPeriod / cpus;
   fclose(file);
   return period;
}


void System_init() {

}
void ProcessList_init(ProcessList* pl) {
   pl -> totalTasks = 0;
   pl -> runningTasks = 0;
   pl -> userlandThreads = 0;
   pl -> kernelThreads = 0;
   pl -> cpuCount = 0;

   pl -> totalMem = 0;
   pl -> usedMem = 0;
   pl -> freeMem = 0;
   pl -> sharedMem =0 ;
   pl -> buffersMem = 0;
   pl -> cachedMem = 0;
   pl -> totalSwap = 0;
   pl -> usedSwap = 0;
   pl -> freeSwap = 0;
}

static inline void LinuxProcessList_scanMemoryInfo(ProcessList* this) {
   unsigned long long int swapFree = 0;
   unsigned long long int shmem = 0;
   unsigned long long int sreclaimable = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      printf("Could not open file %s\n", PROCMEMINFOFILE);
      return;
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {

      #define tryRead(label, variable) do { if (String_startsWith(buffer, label) && sscanf(buffer + strlen(label), " %32llu kB", variable)) { break; } } while(0)
      switch (buffer[0]) {
      case 'M':
         tryRead("MemTotal:", &this->totalMem);
         tryRead("MemFree:", &this->freeMem);
         tryRead("MemShared:", &this->sharedMem);
         break;
      case 'B':
         tryRead("Buffers:", &this->buffersMem);
         break;
      case 'C':
         tryRead("Cached:", &this->cachedMem);
         break;
      case 'S':
         switch (buffer[1]) {
         case 'w':
            tryRead("SwapTotal:", &this->totalSwap);
            tryRead("SwapFree:", &swapFree);
            break;
         case 'h':
            tryRead("Shmem:", &shmem);
            break;
         case 'R':
            tryRead("SReclaimable:", &sreclaimable);
            break;
         }
         break;
      }
      #undef tryRead
   }

   this->usedMem = this->totalMem - this->freeMem;
   this->cachedMem = this->cachedMem + sreclaimable - shmem;
   this->usedSwap = this->totalSwap - swapFree;
   fclose(file);
}

