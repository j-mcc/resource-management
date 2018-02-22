/*
 * $Author: o1-mccune $
 * $Date: 2017/11/13 05:11:06 $
 * $Revision: 1.2 $
 * $Log: pcb.c,v $
 * Revision 1.2  2017/11/13 05:11:06  o1-mccune
 * Final.
 *
 * Revision 1.1  2017/11/11 01:43:04  o1-mccune
 * Initial revision
 *
 */
#include "oss.h"
#include "pcb.h"
#include "simulatedclock.h"
#include <stdio.h>

void initPCB(pcb_t *pcb, pid_t pid, sim_clock_t *time){
  pcb->pid = pid;
  pcb->priority = 0;
  pcb->state = CREATED;
  pcb->runMode = 5;
  
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++)
    pcb->aquiredResources[i] = 0;

  pcb->resourceRequest = -1;
  
  pcb->eventEnd.seconds = 0;
  pcb->eventEnd.nanoseconds = 0;

  pcb->totalWaitTime.seconds = 0;
  pcb->totalWaitTime.nanoseconds = 0;

  pcb->timeLastDispatch.seconds = 0;
  pcb->timeLastDispatch.nanoseconds = 0;
  
  pcb->timeCreated.seconds = time->seconds;
  pcb->timeCreated.nanoseconds = time->nanoseconds;
 
  pcb->timeInSystem.seconds = 0;
  pcb->timeInSystem.nanoseconds = 0;
  
  pcb->totalCPUTime.seconds = 0;
  pcb->totalCPUTime.nanoseconds = 0;
  
  pcb->lastBurstTime.seconds = 0;
  pcb->lastBurstTime.nanoseconds = 0;

  pcb->lastWaitTime.seconds = 0;
  pcb->lastWaitTime.nanoseconds = 0;
} 

void addToTimeInSystem(pcb_t *pcb, sim_clock_t *time){
  sumSimClocks(&pcb->timeInSystem, time);  
}

void addToTotalCPUTime(pcb_t *pcb, sim_clock_t *time){
  sumSimClocks(&pcb->totalCPUTime, time);  
}

void setLastBurstTime(pcb_t *pcb, sim_clock_t *time){
  pcb->lastBurstTime.seconds = time->seconds;
  pcb->lastBurstTime.nanoseconds = time->nanoseconds;
}

void setLastWaitTime(pcb_t *pcb, sim_clock_t *time){
  if(pcb->timeLastDispatch.seconds == 0 && pcb->timeLastDispatch.nanoseconds == 0){
    pcb->lastWaitTime = findDifference(time, &pcb->timeCreated); 
  }
  else{
    pcb->lastWaitTime = findDifference(time, &pcb->timeLastDispatch);
  }
}

void setLastDispatchTime(pcb_t *pcb, sim_clock_t *time){
  setLastWaitTime(pcb, time);
  pcb->timeLastDispatch.seconds = time->seconds;
  pcb->timeLastDispatch.nanoseconds = time->nanoseconds;
}

pid_t getPID(pcb_t *pcb){
  return pcb->pid;
}

void incrementAquiredResource(int *aquiredResources, int resourceNumber){
  if(resourceNumber < NUM_RESOURCE_TYPES){
    aquiredResources[resourceNumber]++;
    //fprintf(stderr, "\t\tR[%d] was increased to %d\n", resourceNumber, aquiredResources[resourceNumber]);
  }
}

void decrementAquiredResource(int *aquiredResources, int resourceNumber){
  if(resourceNumber < NUM_RESOURCE_TYPES){
    if(aquiredResources[resourceNumber] == 0);
    else aquiredResources[resourceNumber]--;
    //fprintf(stderr, "\t\tR[%d] was decreased to %d\n", resourceNumber, aquiredResources[resourceNumber]);
  }
}

int findPCB(pcb_t *pcb, pid_t pid){
  if(pid == 0) return -2;
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(pcb[i].pid == pid) return i;
  }
  return -1;
}
