 long long btime;

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int systemTime;
   unsigned long long int systemAllTime;
   unsigned long long int idleAllTime;
   unsigned long long int idleTime;
   unsigned long long int niceTime;
   unsigned long long int ioWaitTime;
   unsigned long long int irqTime;
   unsigned long long int softIrqTime;
   unsigned long long int stealTime;
   unsigned long long int guestTime;
   
   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int systemPeriod;
   unsigned long long int systemAllPeriod;
   unsigned long long int idleAllPeriod;
   unsigned long long int idlePeriod;
   unsigned long long int nicePeriod;
   unsigned long long int ioWaitPeriod;
   unsigned long long int irqPeriod;
   unsigned long long int softIrqPeriod;
   unsigned long long int stealPeriod;
   unsigned long long int guestPeriod;
} CPUData;

typedef struct ProcessList_ {
   int totalTasks;
   int runningTasks;
   int userlandThreads;
   int kernelThreads;

   int cpuCount;

   unsigned long long int totalMem;
   unsigned long long int usedMem;
   unsigned long long int freeMem;
   unsigned long long int sharedMem;
   unsigned long long int buffersMem;
   unsigned long long int cachedMem;
   unsigned long long int totalSwap;
   unsigned long long int usedSwap;
   unsigned long long int freeSwap;

} ProcessList;

typedef struct LinuxProcessList_ {
   ProcessList super;
   CPUData* cpus;
} LinuxProcessList;


typedef enum {
   CPU_METER_NICE = 0,
   CPU_METER_NORMAL = 1,
   CPU_METER_KERNEL = 2,
   CPU_METER_IRQ = 3,
   CPU_METER_SOFTIRQ = 4,
   CPU_METER_STEAL = 5,
   CPU_METER_GUEST = 6,
   CPU_METER_IOWAIT = 7,
   CPU_METER_ITEMCOUNT = 8, // number of entries in this enum
} CPUMeterValues;

LinuxProcessList* ProcessList_new();
double Platform_setCPUValues(LinuxProcessList* linuxpl, int cpu);
static inline double LinuxProcessList_scanCPUTime(LinuxProcessList* this) ;
static inline void LinuxProcessList_scanMemoryInfo(ProcessList* this);
void ProcessList_init(ProcessList* pl);
