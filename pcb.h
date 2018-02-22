
/*
 * $Author: o1-mccune $
 * $Date: 2017/11/13 05:12:08 $
 * $Revision: 1.2 $
 * $Log: pcb.h,v $
 * Revision 1.2  2017/11/13 05:12:08  o1-mccune
 * Final
 *
 * Revision 1.1  2017/11/11 01:45:32  o1-mccune
 * Initial revision
 *
 */
#ifndef PCB_H
#define PCB_H

#include "oss.h"
#include <sys/types.h>
#include "simulatedclock.h"
#include "pcb.h"

typedef struct{
  pid_t pid;                        //process id
  int priority;                     //process priority
  enum {CREATED, READY, WAIT, EXIT} state;   //ready/wait/exit
  unsigned char runMode;           //run mode
  int aquiredResources[NUM_RESOURCE_TYPES];
  int resourceRequest;
  sim_clock_t eventEnd;             //time event will end
  sim_clock_t totalWaitTime;
  sim_clock_t timeCreated;          //time when process entered system
  sim_clock_t timeLastDispatch;     //time when last dispatched
  sim_clock_t timeInSystem;         //elapsed time since process entered system
  sim_clock_t totalCPUTime;         //total time using cpu
  sim_clock_t lastBurstTime;        //duration of last cpu use
  sim_clock_t lastWaitTime;         //duration of last wait
} pcb_t;

void incrementAquiredResource(int *aquiredResources, const int resourceNumber);

void decrementAquiredResource(int *aquiredResources, const int resourceNumber);

void initPCB(pcb_t *pcb, pid_t pid, sim_clock_t *time);

void addToTimeInSystem(pcb_t *pcb, sim_clock_t *time);

void addToTotalCPUTime(pcb_t *pcb, sim_clock_t *time);

void setLastBurstTime(pcb_t *pcb, sim_clock_t *time);

void setLastDispatchTime(pcb_t *pcb, sim_clock_t *time);

void changePriority(pcb_t *pcb, unsigned int priority);

pid_t getPID(pcb_t *pcb);

void setLastWaitTime(pcb_t *pcb, sim_clock_t *time);

int findPCB(pcb_t *pcb, pid_t pid);

#endif
