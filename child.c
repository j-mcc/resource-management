 /*
 * $Author: o1-mccune $
 * $Date: 2017/11/13 05:19:11 $
 * $Revison: $
 * $Log: child.c,v $
 * Revision 1.3  2017/11/13 05:19:11  o1-mccune
 * Remove unused #include.
 * Final.
 *
 * Revision 1.2  2017/11/13 05:10:28  o1-mccune
 * Final.
 *
 * Revision 1.1  2017/11/11 01:41:34  o1-mccune
 * Initial revision
 *
 */

#include "oss.h"
#include "messagequeue.h"
#include "resourcedescriptor.h"
#include "pcb.h"
#include "simulatedclock.h"
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>

#define REQUEST_BOUND 500 

static int runCleanUp = 1;
static key_t messageQueueKey;
static int messageQueueId;
static int messageId;
static int resourceDescriptorsId;
static int sharedSemaphoreId;
static int pcbId;
static int sharedClockId;
static resource_descriptor_list_t *resourceDescriptors;
static sem_t *semaphore;
static sim_clock_t *simClock;
static pcb_t *pcb;
static message_t messageBuffer;

static void printOptions(){
  fprintf(stderr, "CHILD:  Command Help\n");
  fprintf(stderr, "\tCHILD:  Optional '-h': Prints Command Usage\n");
  fprintf(stderr, "\tCHILD:  '-r': Shared memory ID for resource descriptors.\n");
  fprintf(stderr, "\tCHILD:  '-p': Shared memory ID for PCB Table.\n");
  fprintf(stderr, "\tCHILD:  '-m': Shared memory ID for message.\n");
  fprintf(stderr, "\tCHILD:  '-s': Shared memory ID for semaphore.\n");
  fprintf(stderr, "\tCHILD:  '-c': Shared memory ID for clock.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  int nCP = 0;
  while ((c = getopt (argc, argv, "hr:p:s:c:m:")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 'c':
        sharedClockId = atoi(optarg);
        break;
      case 'r':
        resourceDescriptorsId = atoi(optarg);
        break;
      case 's':
        sharedSemaphoreId = atoi(optarg);
        break;
      case 'm':
        messageQueueId = atoi(optarg);
        break;
      case 'p':
	pcbId = atoi(optarg);
        break;
      case '?':
	if(isprint (optopt))
          fprintf(stderr, "CHILD: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "CHILD: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  return 0;
}

static int detachResourceDescriptors(){
  return shmdt(resourceDescriptors);
}
/*
static int detachSharedMessage(){
  return shmdt(message);
}
*/
static int detachSharedSemaphore(){
  return shmdt(semaphore);
}

static int detachPCB(){
  return shmdt(pcb);
}

static int detachSharedClock(){
  return shmdt(simClock);
}

static int attachResourceDescriptors(){
  if((resourceDescriptors = shmat(resourceDescriptorsId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}
/*
static int attachSharedMessage(){
  if((message = shmat(messageId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}
*/
static int attachSharedSemaphore(){
  if((semaphore = shmat(sharedSemaphoreId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachPCB(){
  if((pcb = shmat(pcbId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedClock(){
  if((simClock = shmat(sharedClockId, NULL, SHM_RDONLY)) == (void *)-1) return -1;
  return 0;
}

static void cleanUp(int signal){
  if(runCleanUp){
    runCleanUp = 0;
    if(detachSharedSemaphore() == -1) perror("CHILD: Failed to detach shared semaphore");
    if(detachPCB() == -1) perror("CHILD: Failed to detach PCB table");
    if(detachSharedClock() == -1) perror("CHILD: Failed to detach shared clock");
    if(detachResourceDescriptors() == -1) perror("CHILD: Failed to detach resource descriptors");
  }
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(0);
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

static int findResourceForRelease(int index){
  int hasResources[NUM_RESOURCE_TYPES];
  int i;
  int j = 0;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(pcb[index].aquiredResources[i]){
      hasResources[j++] = i;
    }
  }
  if(j == 1) return hasResources[0];
  else if(j) return hasResources[rand() % j];
  else return -1;
}

int main(int argc, char **argv){
  parseOptions(argc, argv);

  if(initAlarmWatcher() == -1){
    perror("CHILD: Failed to init SIGALRM watcher");
    exit(1);
  }
  if(initInterruptWatcher() == -1){
    perror("CHILD: Failed to init SIGINT watcher");
    exit(2);
  }
  if(attachSharedSemaphore() == -1){
    perror("CHILD: Failed to attach semaphore");
    exit(3);
  }
  if(attachPCB() == -1){
    perror("CHILD: Failed to attach PCB table");
    exit(4);
  }
  if(attachSharedClock() == -1){
    perror("CHILD: Failed to attach clock");
    exit(5);
  }
  if(attachResourceDescriptors() == -1){
    perror("CHILD: Failed to attach resource descriptors");
    exit(6);
  }


  srand(time(0) + getpid());  //seed random number
  int i = findPCB(pcb, getpid()); //find pcb index  

  sim_clock_t processEndTime;
  processEndTime.seconds = 2;
  processEndTime.nanoseconds = 0;
  sumSimClocks(&processEndTime, simClock);
   
  sim_clock_t eventTime;
  //eventTime.seconds = 0;
  //eventTime.nanoseconds = (rand() % REQUEST_BOUND) * 1000;
  //sumSimClocks(&eventTime, simClock);
  
  while(1){
    
    
    eventTime.seconds = 0;
    eventTime.nanoseconds = rand() % REQUEST_BOUND;
    sumSimClocks(&eventTime, simClock);
      
    if(compareSimClocks(simClock, &eventTime) == 1){  //time to request or release a resource
      int operation = rand() % 5;
      if(operation != 0){  //request
        int requestType = rand() % NUM_RESOURCE_TYPES;
        
        //create request structure
        int request[NUM_RESOURCE_TYPES];
        int index;
        for(index = 0; index < NUM_RESOURCE_TYPES; index++)
          request[index] = (index == requestType ? 1 : 0);

        //fprintf(stderr, "\t\tCHILD[%d:%d]: Sent REQUEST for R[%d] @ %d:%d\n", i, getpid(), requestType, simClock->seconds, simClock->nanoseconds);
        if(sendMessage(messageQueueId, REQUEST+i, i, request) == -1) perror("CHILD: Failed to send message");

        //wait for allocation
        msgrcv(messageQueueId, &messageBuffer, sizeof(message_t), ALLOCATE+i, 0);
        
      }
      else{  //release
        int releaseType = findResourceForRelease(i);
        if(releaseType != -1){
          //create release structure
          int request[NUM_RESOURCE_TYPES];
          int index;
          for(index = 0; index < NUM_RESOURCE_TYPES; index++)
            request[index] = (index == releaseType ? 1 : 0);
        
          //fprintf(stderr, "\t\tCHILD[%d:%d]: Sent RELEASE for R[%d] @ %d:%d\n", i, getpid(), releaseType, simClock->seconds, simClock->nanoseconds);
          if(sendMessage(messageQueueId, RELEASE+i, i, request) == -1) perror("CHILD: Failed to send message");
        }
      }
    }

    if(compareSimClocks(simClock, &processEndTime) != -1){  //start checking for termination
      int terminate = rand() % 5;
      if(terminate == 0){
        //fprintf(stderr, "\t\tCHILD[%d]: Sent TERMINATE @ %d:%d\n", i, simClock->seconds, simClock->nanoseconds);
        if(sendMessage(messageQueueId, TERMINATE+i, i, pcb->aquiredResources) == -1) perror("CHILD: Failed to send message");
        break;
      }
    }
  }
  cleanUp(2);

  return 0;
}

