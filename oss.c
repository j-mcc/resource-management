
 /*
 * $Author: o1-mccune $
 * $Date: 2017/11/13 05:18:44 $
 * $Revison: $
 * $Log: oss.c,v $
 * Revision 1.3  2017/11/13 05:18:44  o1-mccune
 * Removed unused #include.
 * Final.
 *
 * Revision 1.2  2017/11/13 05:10:47  o1-mccune
 * Final. Added run statistics.
 *
 * Revision 1.1  2017/11/11 01:42:52  o1-mccune
 * Initial revision
 *
 */

#include "oss.h"
#include "resourcedescriptor.h"
#include "bitarray.h"
#include "pcb.h"
#include "simulatedclock.h"
#include "messagequeue.h"
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>


static int logEntries = 0;
static int logEntryCutoff = 100000;
static unsigned int maxProcessTime = 20;
static char defaultLogFilePath[] = "logfile.txt";
static char *logFilePath = NULL;
static FILE *logFile;

/*--System Report Variables--*/
static int numAllocations = 0;
static int finished[PCB_TABLE_SIZE];
static int ranDeadlockDetection = 0;
static int numDeadlocks = 0;
static int newDeadlock = 0;
static int numProcessesTerminatedByDeadlockResolver = 0;
/*---------------------------*/

/*--Shared Memory Variables--*/
static pcb_t *pcb;
static resource_descriptor_list_t *resourceDescriptor;
static sem_t *semaphore;
static sim_clock_t *simClock;
static message_t messageBuffer;
static key_t messageQueueKey;
static key_t messageSharedMemoryKey;
static key_t resourceDescriptorMemoryKey;
static key_t clockSharedMemoryKey;
static key_t pcbSharedMemoryKey;
static key_t semaphoreSharedMemoryKey;
static int messageQueueId;
static int semaphoreSharedMemoryId;
static int resourceDescriptorMemoryId;
static int messageSharedMemoryId;
static int pcbSharedMemoryId;
static int clockSharedMemoryId;
/*---------------------------*/

static pid_t *children;
static int childCounter = 0;
static int terminatedIndex;

unsigned int verbose = 0; //By default, verbose logging is off

/*-------------------------------------------*/

static void printOptions(){
  fprintf(stderr, "OSS:  Command Help\n");
  fprintf(stderr, "\tOSS:  '-h': Prints Command Usage\n");
  fprintf(stderr, "\tOSS:  Optional '-l': Filename of log file. Default is logfile.txt\n");
  fprintf(stderr, "\tOSS:  Optional '-t': Input number of seconds before the main process terminates. Default is 20 seconds.\n");
  fprintf(stderr, "\tOSS:  Optional '-v': If option -v is specified, verbose(increased) logging will be performed.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  while ((c = getopt (argc, argv, "ht:l:v")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 't':
        maxProcessTime = atoi(optarg);
        break;
      case 'l':
	logFilePath = malloc(sizeof(char) * (strlen(optarg) + 1));
        memcpy(logFilePath, optarg, strlen(optarg));
        logFilePath[strlen(optarg)] = '\0';
        break;
      case 'v':
        verbose = 1;
        break;
      case '?':
        if(optopt == 't'){
          maxProcessTime = 20;
	  break;
	}
	else if(isprint (optopt))
          fprintf(stderr, "OSS: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "OSS: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  if(!logFilePath){
    logFilePath = malloc(sizeof(char) * strlen(defaultLogFilePath) + 1);
    memcpy(logFilePath, defaultLogFilePath, strlen(defaultLogFilePath));
    logFilePath[strlen(defaultLogFilePath)] = '\0';
  }
  return 0;
}

static int initMessageQueue(){
  if((messageQueueKey = ftok("./oss", 10)) == -1) return -1;
  if((messageQueueId = getMessageQueue(messageQueueKey)) == -1){
    perror("OSS: Failed to get message queue");
    return -1;
  }
  return 0;
}

/*
static int initMessageSharedMemory(){
  if((messageSharedMemoryKey = ftok("./oss", 4)) == -1) return -1;
  if((messageSharedMemoryId = shmget(messageSharedMemoryKey, sizeof(shared_message_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for message");
    return -1;
  }
  return 0;
}
*/

static int initSemaphoreSharedMemory(){
  if((semaphoreSharedMemoryKey = ftok("./oss", 1)) == -1) return -1;
  if((semaphoreSharedMemoryId = shmget(semaphoreSharedMemoryKey, sizeof(sem_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for semaphore");
    return -1;
  }
  return 0;
}

static int initPCBSharedMemory(){
  if((pcbSharedMemoryKey = ftok("./oss", 2)) == -1) return -1;
  if((pcbSharedMemoryId = shmget(pcbSharedMemoryKey, sizeof(pcb_t) * PCB_TABLE_SIZE, IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for PCB");
    return -1;
  }
  return 0;
}

static int initClockSharedMemory(){
  if((clockSharedMemoryKey = ftok("./oss", 3)) == -1) return -1;
  if((clockSharedMemoryId = shmget(clockSharedMemoryKey, sizeof(sim_clock_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for clock");
    return -1;
  }
  return 0;
}

static int initResourceDescriptorMemory(){
  if((resourceDescriptorMemoryKey = ftok("./oss", 5)) == -1) return -1;
  if((resourceDescriptorMemoryId = shmget(resourceDescriptorMemoryKey, sizeof(resource_descriptor_list_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for Resource Descriptors");
    return -1;
  }
  return 0;
}

static int removeResourceDescriptorSharedMemory(){
  if(shmctl(resourceDescriptorMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove resource descriptor shared memory");
    return -1;
  }
  return 0;
}

/*
static int removeMessageSharedMemory(){
  if(shmctl(messageSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove message shared memory");
    return -1;
  }
  return 0;
}
*/

static int removeSemaphoreSharedMemory(){
  if(shmctl(semaphoreSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove semaphore shared memory");
    return -1;
  }
  return 0;
}

static int removePCBSharedMemory(){
  if(shmctl(pcbSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove PCB shared memory");
    return -1;
  }
  return 0;
}

static int removeClockSharedMemory(){
  if(shmctl(clockSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove clock shared memory");
    return -1;
  }
  return 0;
}

static int detachResourceDescriptorSharedMemory(){
  return shmdt(resourceDescriptor);
}

/*
static int detachMessageSharedMemory(){
  return shmdt(message);
}
*/

static int detachSemaphoreSharedMemory(){
  return shmdt(semaphore);
}

static int detachClockSharedMemory(){
  return shmdt(simClock);
}

static int detachPCBSharedMemory(){
  return shmdt(pcb);
}

static int attachResourceDescriptorMemory(){
  if((resourceDescriptor = shmat(resourceDescriptorMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

/*
static int attachMessageSharedMemory(){
  if((message = shmat(messageSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}
*/

static int attachSemaphoreSharedMemory(){
  if((semaphore = shmat(semaphoreSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachPCBSharedMemory(){
  if((pcb = shmat(pcbSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachClockSharedMemory(){
  if((simClock = shmat(clockSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

/*
 * Before detaching and removing IPC shared memory, use this to destroy the semaphore.
 */
static int removeSemaphore(sem_t *semaphore){
  if(sem_destroy(semaphore) == -1){
    perror("OSS: Failed to destroy semaphore");
    return -1;
  }
  return 0;
}

/*
 * After IPC shared memory has been allocated and attached to process, use this to initialize the semaphore. 
 */
static int initSemaphore(sem_t *semaphore, int processOrThreadSharing, unsigned int value){
  if(sem_init(semaphore, processOrThreadSharing, value) == -1){
    perror("OSS: Failed to initialize semaphore");
    return -1;
  }
  return 0;
}

static void cleanUp(int signal){
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    //if(children[i] > 0){
      if(signal == 2) fprintf(stderr, "Parent sent SIGINT to Child %d\n", pcb[i].pid);
      else if(signal == 14)fprintf(stderr, "Parent sent SIGALRM to Child %d\n", pcb[i].pid);
      kill(pcb[i].pid, signal);
      waitpid(-1, NULL, 0);
    //}
  }
  
  if(removeMessageQueue(messageQueueId) == -1) perror("OSS: Failed to remove message queue");
  if(detachResourceDescriptorSharedMemory() == -1) perror("OSS: Failed to detach resource descriptors");
  if(removeResourceDescriptorSharedMemory() == -1) perror("OSS: Failed to remove resource descriptors");
 // if(detachMessageSharedMemory() == -1) perror("OSS: Failed to detach message memory");
 // if(removeMessageSharedMemory() == -1) perror("OSS: Failed to remove message memory");
  if(detachPCBSharedMemory() == -1) perror("OSS: Failed to detach PCB memory");
  if(removePCBSharedMemory() == -1) perror("OSS: Failed to remove PCB memory");
  if(detachClockSharedMemory() == -1) perror("OSS: Failed to detach clock memory");
  if(removeClockSharedMemory() == -1) perror("OSS: Failed to remove clock memory");
  if(removeSemaphore(semaphore) == -1) fprintf(stderr, "OSS: Failed to remove semaphore");
  if(detachSemaphoreSharedMemory() == -1) perror("OSS: Failed to detach semaphore shared memory");
  if(removeSemaphoreSharedMemory() == -1) perror("OSS: Failed to remove seamphore shared memory");
  if(children) free(children);
  if(logFile) fclose(logFile);
  freeBitVector();
  fprintf(stderr, "OSS: Total children created :: %d\n", childCounter); 
  fprintf(stderr, "OSS: Total number of deadlock checks :: %d\n", ranDeadlockDetection);
  fprintf(stderr, "OSS: Number of processes killed naturally :: %d\n", childCounter - numProcessesTerminatedByDeadlockResolver);
  fprintf(stderr, "OSS: Number of processes killed to fix deadlock :: %d\n", numProcessesTerminatedByDeadlockResolver);
  fprintf(stderr, "OSS: Average number of processes killed to resolve deadlock :: %f\n", ((numProcessesTerminatedByDeadlockResolver * 1.)/numDeadlocks));
  fprintf(stderr, "OSS: Number of resource allocations :: %d\n", numAllocations);
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(signal);
}

static int initAlarmWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL));
}

static int initInterruptWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGINT, &action, NULL));
}

/*
 *  Transform an integer into a string
 */
static char *itoa(int num){
  char *asString = malloc(sizeof(char)*16);
  snprintf(asString, sizeof(char)*16, "%d", num);
  return asString;
}


static void printAllocationTable(){
  int i, j;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(i == 0) fprintf(stderr, "       R[%d]    ", i);
    else if(i == 9) fprintf(stderr, "R[%d]   ", i);
    else if(i > 9) fprintf(stderr, "R[%d]   ", i);
    else fprintf(stderr, "R[%d]    ", i);
  }
  fprintf(stderr, "\n");
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(i < 10) fprintf(stderr, "P[%d]      ", i);
    else if(i < 100) fprintf(stderr, "P[%d]     ", i);
    for(j = 0; j < NUM_RESOURCE_TYPES; j++){
      fprintf(stderr, "%d       ", pcb[i].aquiredResources[j]);
    }
    fprintf(stderr, "\n");
  }

  if((logEntries = (logEntries + PCB_TABLE_SIZE + 1)) < logEntryCutoff){
    if(verbose){
      for(i = 0; i < NUM_RESOURCE_TYPES; i++){
        if(i == 0) fprintf(logFile, "       R[%d]    ", i);
        else if(i == 9) fprintf(logFile, "R[%d]   ", i);
        else if(i > 9) fprintf(logFile, "R[%d]   ", i);
        else fprintf(logFile, "R[%d]    ", i);
      }
      fprintf(logFile, "\n");
      for(i = 0; i < PCB_TABLE_SIZE; i++){
        if(i < 10) fprintf(logFile, "P[%d]      ", i);
        else if(i < 100) fprintf(logFile, "P[%d]     ", i);
        for(j = 0; j < NUM_RESOURCE_TYPES; j++){
          fprintf(logFile, "%d       ", pcb[i].aquiredResources[j]);
        }
        fprintf(logFile, "\n");
      }
    }
  }
}

static void logRequest(int childNum, int *resource){
  fprintf(stderr, "OSS: Received REQUEST from C[%d] for ", childNum);
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(resource[i]) fprintf(stderr, "R[%d] ", i);
  }
  fprintf(stderr, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
  if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: Received REQUEST from C[%d] for ", childNum);
      int i;
      for(i = 0; i < NUM_RESOURCE_TYPES; i++){
        if(resource[i]) fprintf(logFile, "R[%d] ", i);
      }
      fprintf(logFile, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
    }
  }
}

static void logAllocate(int childNum, int *resource){
  fprintf(stderr, "OSS: Granted REQUEST from C[%d] for ", childNum);
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(resource[i]) fprintf(stderr, "R[%d] ", i);
  }
  fprintf(stderr, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
  
  if(++numAllocations % 20 == 0) printAllocationTable();
  
  if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: Granted REQUEST from C[%d] for ", childNum);
      int i;
      for(i = 0; i < NUM_RESOURCE_TYPES; i++){
        if(resource[i]) fprintf(logFile, "R[%d] ", i);
      }
      fprintf(logFile, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
      if(numAllocations % 20 == 0) printAllocationTable();
      
    }
  }
}

static void logAddToWaitList(int childNum, int *resource){
  fprintf(stderr, "OSS: C[%d] added to ", childNum);
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(resource[i]) fprintf(stderr, "R[%d] ", i);
  }
  fprintf(stderr, " waitlist at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
  if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: C[%d] added to ", childNum);
      int i;
      for(i = 0; i < NUM_RESOURCE_TYPES; i++){
        if(resource[i]) fprintf(logFile, "R[%d] ", i);
      }
      fprintf(logFile, "waitlist at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
    }
  }
}

static void logRelease(int childNum, int *resource){
  fprintf(stderr, "OSS: Received RELEASE from C[%d] for ", childNum);
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(resource[i]) fprintf(stderr, "R[%d] ", i);
  }
  fprintf(stderr, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
  if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: Received RELEASE from C[%d] for ", childNum);
      int i;
      for(i = 0; i < NUM_RESOURCE_TYPES; i++){
        if(resource[i]) fprintf(logFile, "R[%d] ", i);
      }
      fprintf(logFile, "at time [%d:%d]\n", simClock->seconds, simClock->nanoseconds);
    }
  }
}

static void logTerminate(int childNum){
  fprintf(stderr, "OSS: Received TERMINATE from C[%d] @ time [%d:%d]\n", childNum, simClock->seconds, simClock->nanoseconds); 
  if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: Received TERMINATE from C[%d] @ time [%d:%d]\n", childNum, simClock->seconds, simClock->nanoseconds); 
    }
  }
}

static void logFulfillWaitRequest(int childNum, int resourceNum){
  fprintf(stderr, "OSS: Fulfilling REQUEST from C[%d] for R[%d] @ time [%d:%d]\n", childNum, resourceNum, simClock->seconds, simClock->nanoseconds);
  if(++numAllocations % 20 == 0) printAllocationTable();
  if(logEntries++ < logEntryCutoff){
    if(verbose){ 
      fprintf(logFile, "OSS: Fulfilling REQUEST from C[%d] for R[%d] @ time [%d:%d]\n", childNum, resourceNum, simClock->seconds, simClock->nanoseconds); 
      if(numAllocations % 20 == 0) printAllocationTable();
    }
  }
}

static void logDeadlock(){
  fprintf(stderr, "\t\tOSS: Deadlock detected with ");
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(!finished[i]) fprintf(stderr, "C[%d] ", i);
  }
  fprintf(stderr, "\n");
  if(logEntries++ < logEntryCutoff){
    fprintf(logFile, "\t\tOSS: Deadlock detected with ");
    int i;
    for(i = 0; i < PCB_TABLE_SIZE; i++){
      if(!finished[i]) fprintf(logFile, "C[%d] ", i);
    }
    fprintf(logFile, "\n");
  }
}

static void logDeadlockResolution(int childNum){
  fprintf(stderr, "\t\tOSS: Terminated C[%d] to resolve deadlock @ time [%d:%d]\n", childNum, simClock->seconds, simClock->nanoseconds); 
  if(logEntries++ < logEntryCutoff){
    fprintf(logFile, "\t\tOSS: Terminated C[%d] to resolve deadlock @ time [%d:%d]\n", childNum, simClock->seconds, simClock->nanoseconds); 
  }
}

static void resolveDeadlock(){
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(!finished[i]){
      releaseAllResources(resourceDescriptor, &pcb[i], i);
      //clear any messages that the terminating process still has in the queue
      while(msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), REQUEST+i, IPC_NOWAIT) != -1);
      while(msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), RELEASE+i, IPC_NOWAIT) != -1);
      //issue wait for terminating process
      pcb[i].state = EXIT;
      kill(pcb[i].pid, 2);
      waitpid(-1, NULL, 0);
      //clear 'set' bit
      clearBit(i);
      logDeadlockResolution(i);
      numProcessesTerminatedByDeadlockResolver++;
      break;
    }
  }
}

static int requriesLessThanAvailable(const int *work, const int processNum){
  if(pcb[processNum].resourceRequest == -1) return 1;  //has not requested a resource
  if(work[pcb[processNum].resourceRequest] <= 0) return 0;
  return 1;
}

static int deadlockDetection(const pcb_t *pcb, const resource_descriptor_list_t *resourceDescriptor){
  ranDeadlockDetection++;
  int work[NUM_RESOURCE_TYPES];
  
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    work[i] = resourceDescriptor->resources[i].numResources;  //copy available resources of each type into working
  }
  if(childCounter < PCB_TABLE_SIZE) 
    for(i = 0; i < PCB_TABLE_SIZE; i++){
       if(i < childCounter) finished[i] = 0;
       else finished[i] = 1;
    }
  else
    for(i = 0; i < PCB_TABLE_SIZE; finished[i++] = 0);  //set each process to not finished

  int p;
  for(p = 0; p < PCB_TABLE_SIZE; p++){  //for each process, check if its request is less than avaiable
    if(finished[p]){  //process finished
      continue;
    }
    if(requriesLessThanAvailable(work, p)){  //if true
      finished[p] = 1; //set process to finished
      for(i = 0; i < NUM_RESOURCE_TYPES; i++)
        work[i] += pcb[p].aquiredResources[i];  //"release" its resources
      p = -1;  //reset p and repeat
    }
  }
  
  for(p = 0; p < PCB_TABLE_SIZE; p++){
    if(!finished[p]) break;
  }

  return (p == PCB_TABLE_SIZE) ? 0 : 1;
  
}

int main(int argc, char **argv){
  parseOptions(argc, argv);

  children = malloc(sizeof(pid_t) * PCB_TABLE_SIZE);
  logFile = fopen(logFilePath, "w");
  

  if(initAlarmWatcher() == -1) perror("OSS: Failed to init SIGALRM watcher");
  if(initInterruptWatcher() == -1) perror("OSS: Failed to init SIGINT watcher");
  
  if(initMessageQueue() == -1) perror("OSS: Failed to init message queue");
  fprintf(stderr, "OSS: Message Queue ID [%d]\n", messageQueueId); 
  if(initResourceDescriptorMemory() == -1) perror("OSS: Failed to init resource descriptors");
  if(attachResourceDescriptorMemory() == -1) perror("OSS: Failed to attach resource descriptors");
  initResources(resourceDescriptor->resources);

 // if(initMessageSharedMemory() == -1) perror("OSS: Failed to init message shared memory");
 // if(attachMessageSharedMemory() == -1) perror("OSS: Failed to attach message shared memory");
  if(initSemaphoreSharedMemory() == -1) perror("OSS: Failed to init semaphore shared memory");
  if(attachSemaphoreSharedMemory() == -1) perror("OSS: Failed to attach semaphore shared memory");
  if(initSemaphore(semaphore, 1, 1) == -1) fprintf(stderr, "OSS: Failed to create semaphore");
  if(initPCBSharedMemory() == -1) perror("OSS: Failed to init PCB memory");
  if(attachPCBSharedMemory() == -1) perror("OSS: Failed to attach PCB memory");
  if(initClockSharedMemory() == -1) perror("OSS: Failed to init clock memory");
  if(attachClockSharedMemory() == -1) perror("OSS: Failed to attach clock memory");
  if(initBitVector(PCB_TABLE_SIZE, MIN_VALUE) == -1) fprintf(stderr, "OSS: Failed to initialized bit vector\n");
  
  //resetMessage(message);
  resetSimClock(simClock);
  alarm(maxProcessTime);
  pid_t childpid;
  //seed random number generator
  srand(time(0) + getpid());

  //time to create next process  
  sim_clock_t createNext;
  createNext.seconds = 0;
  createNext.nanoseconds = 0;

  //deadlock detection interval
  sim_clock_t deadlockDetectionInterval;
  deadlockDetectionInterval.seconds = 1;
  deadlockDetectionInterval.nanoseconds = 0;
  
  //time to run next deadlock detection
  sim_clock_t nextDeadlockDetection;
  nextDeadlockDetection.seconds = 1;
  nextDeadlockDetection.nanoseconds = 0;


  int aliveChildren = 0;

  while(1){
    //increment clock
    incrementSimClock(simClock);


    //check for messages
    while(1){
      if(msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), -4000, IPC_NOWAIT) == -1){
        break;
      }
      if(messageBuffer.type > 1000 && messageBuffer.type < 2000){ //REQUEST MESSAGE
        logRequest(messageBuffer.pcbIndex, messageBuffer.resources);
        if(allocateResource(resourceDescriptor, &pcb[messageBuffer.pcbIndex], messageBuffer.resources, messageBuffer.pcbIndex)){
          logAllocate(messageBuffer.pcbIndex, messageBuffer.resources);
          if(sendMessage(messageQueueId, ALLOCATE+messageBuffer.pcbIndex, messageBuffer.pcbIndex, messageBuffer.resources) == -1)
            perror("OSS: Failed to send message");
        }
        else logAddToWaitList(messageBuffer.pcbIndex, messageBuffer.resources); 
      }
      else if(messageBuffer.type > 2000 && messageBuffer.type < 3000){ //RELEASE MESSAGE
        logRelease(messageBuffer.pcbIndex, messageBuffer.resources);
        releaseResource(resourceDescriptor, &pcb[messageBuffer.pcbIndex], messageBuffer.resources, messageBuffer.pcbIndex);
      }
      else if(messageBuffer.type > 3000 && messageBuffer.type < 4000){ //TERMINATE MESSAGE
        logTerminate(messageBuffer.pcbIndex);//fprintf(stderr, "OSS: TERMINATE from %d\n", messageBuffer.pcbIndex);
        releaseAllResources(resourceDescriptor, &pcb[messageBuffer.pcbIndex], messageBuffer.pcbIndex);
        //issue wait for terminating process
        pcb[messageBuffer.pcbIndex].state = EXIT;
        kill(pcb[messageBuffer.pcbIndex].pid, 2);
        waitpid(-1, NULL, 0);
        //get PCB index
        //int terminatedIndex = messageBuffer.pcbIndex;
        //clear 'set' bit
        clearBit(messageBuffer.pcbIndex);
        int index = messageBuffer.pcbIndex;

        //clear any messages that the terminating process still has in the queue
        while(msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), REQUEST+index, IPC_NOWAIT) != -1);
        while(msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), RELEASE+index, IPC_NOWAIT) != -1);
        
        if(childCounter < MAX_CHILDREN && !isFull()){  //if room for another child
          if(childCounter < PCB_TABLE_SIZE){  //if still creating initial children
            if(compareSimClocks(simClock, &createNext) != -1){  //if time has passed to create next child 
            //create child
              if((childpid = fork()) > 0){  //parent code
                setBit(childCounter);
                initPCB(&pcb[childCounter], childpid, simClock);
                childCounter++;
                aliveChildren++;
                fprintf(stderr, "OSS: Created child[%d]\n", childCounter);
                createNext.nanoseconds = (rand() % 501) * 1000000;
              }
              else if(childpid == 0){  //child code
                execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-r", itoa(resourceDescriptorMemoryId), "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
              }
              else perror("OSS: Failed to fork");
            }
          }
          else{
            if((childpid = fork()) > 0){  //parent code
              int z;
              for(z = 0; z < PCB_TABLE_SIZE; z++){
                if(pcb[z].state == EXIT) break;
              }
              setBit(z);
              initPCB(&pcb[z], childpid, simClock);
              childCounter++;
              fprintf(stderr, "OSS: Replaced child[%d] with %d\n", z, childpid);
            }
            else if(childpid == 0){
              execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-r", itoa(resourceDescriptorMemoryId), "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
            }
            else perror("OSS: Failed to fork");
          }
        }
      }
    }

   
    if(compareSimClocks(simClock, &nextDeadlockDetection) != -1){  //time to run deadlock detection
      sumSimClocks(&nextDeadlockDetection, &deadlockDetectionInterval);  //compute next deadlock detection time
      
      while(deadlockDetection(pcb, resourceDescriptor)){
        //deadlock detected
        if(!newDeadlock){
          newDeadlock = 1;
          numDeadlocks++;
        }
        logDeadlock();
        resolveDeadlock();
      }
      newDeadlock = 0;
    }

    
    int z;
    for(z = 0; z < NUM_RESOURCE_TYPES; z++){
      if(resourceDescriptor->resources[z].numResources <= 0) continue;  //no resources to fulfill

      int index = fulfillFromWaitList(&resourceDescriptor->resources[z]);  //get the next process to fulfill
      if(index == -1) continue;
      else{
        int pcbIndex = findPCB(pcb, resourceDescriptor->resources[z].waitList[index].pid);
          logFulfillWaitRequest(pcbIndex, z);
          int resourceBuffer[NUM_RESOURCE_TYPES];
          int x;
          for(x = 0; x < NUM_RESOURCE_TYPES; x++) resourceBuffer[x] = (x == z) ? 1 : 0;
          if(allocateResource(resourceDescriptor, &pcb[pcbIndex], resourceBuffer, pcbIndex))
            if(sendMessage(messageQueueId, ALLOCATE+pcbIndex, pcbIndex, resourceBuffer) == -1)
              perror("OSS: Failed to send message");
      }
    }
    
    if(childCounter < MAX_CHILDREN && !isFull()){  //if room for another child
      if(childCounter < PCB_TABLE_SIZE){  //if still creating initial children
        if(compareSimClocks(simClock, &createNext) != -1){  //if time has passed to create next child 
          //create child
          if((childpid = fork()) > 0){  //parent code
            setBit(childCounter);
            initPCB(&pcb[childCounter], childpid, simClock);
            childCounter++;
            aliveChildren++;
            fprintf(stderr, "OSS: Created child[%d]\n", childCounter);
            createNext.nanoseconds = (rand() % 501) * 1000000;
          }
          else if(childpid == 0){  //child code
            execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-r", itoa(resourceDescriptorMemoryId), "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
          }
          else perror("OSS: Failed to fork");
        }
      }
      else{
        if((childpid = fork()) > 0){  //parent code
          int z;
          for(z = 0; z < PCB_TABLE_SIZE; z++){
            if(pcb[z].state == EXIT) break;
          }
          setBit(z);
          initPCB(&pcb[z], childpid, simClock);
          childCounter++;
          fprintf(stderr, "OSS: Replaced child[%d] with %d\n", z, childpid);
        }
        else if(childpid == 0){
            execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-r", itoa(resourceDescriptorMemoryId), "-p", itoa(pcbSharedMemoryId), "-c", itoa(clockSharedMemoryId), "-m", itoa(messageQueueId), NULL);
        }
        else perror("OSS: Failed to fork");
      }
    }

    if(childCounter >= MAX_CHILDREN && isEmpty()) break;  //all children have terminated
  }
  
  raise(SIGINT);

  return 0;
}


