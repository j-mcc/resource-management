
/*
 * $Author: o1-mccune $
 * $Date: 2017/11/11 01:45:42 $
 * $Revision: 1.1 $
 * $Log: resourcedescriptor.c,v $
 * Revision 1.1  2017/11/11 01:45:42  o1-mccune
 * Initial revision
 *
 */
#include <math.h>
#include <stdio.h>
#include "resourcedescriptor.h"
#include "oss.h"


void initResources(resource_descriptor_t *resources){
  srand(time(0) + getpid());
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    int shared;
    resources[i].shared = ((shared = rand() % 5) == 0) ? 1 : 0;  //~20% of resource types should be shared
    if(!resources[i].shared){  //if resource is not shared, generate 1-10 resources of this type
      int number = (rand() % MAX_RESOURCE_INSTANCES) + 1;  //generate a random number between 1 -> 10
      resources[i].numResources = number;
      fprintf(stderr, "\t\tRESOURCE[%d]: Has %d resources\n", i, resources[i].numResources);
    }
    else{ 
      fprintf(stderr, "\t\tRESOURCE[%d]: Is shared\n", i);
      resources[i].numResources = PCB_TABLE_SIZE;
    }
    resources[i].waitListSize = 0;
    resources[i].waitListIterator = 0;
  }
  return;
}

void releaseAllResources(resource_descriptor_list_t *resourceList, pcb_t *pcb, const int pcbIndex){
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    resourceList->resources[i].numResources += pcb->aquiredResources[i];
    pcb->aquiredResources[i] = 0;
  }
  pcb->resourceRequest = -1;
  removeFromWaitList(resourceList, pcb);
  //fprintf(stderr, "\t\tRELEASED all resources from Child[%d]\n", pcbIndex);
}

void releaseResource(resource_descriptor_list_t *resourceList, pcb_t *pcb, const int *reclaimed, const int pcbIndex){
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(reclaimed[i]){
      resourceList->resources[i].numResources += reclaimed[i];
      pcb->aquiredResources[i] -= reclaimed[i];
    }
  }
  pcb->resourceRequest = -1;
}


//allocates a single resource to a process
int allocateResource(resource_descriptor_list_t *resourceList, pcb_t *pcb, const int *request, const int pcbIndex){
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    if(request[i]){
      int result;
      if((result = resourceList->resources[i].numResources - request[i]) >= 0){  //resource is available
        pcb->resourceRequest = -1;
        pcb->aquiredResources[i] += request[i];
        resourceList->resources[i].numResources -= request[i];
        //fprintf(stderr, "\t\tRESOURCE[%d]: Allocated to Child[%d]. R[%d] has %d remaining\n", i, pcbIndex, i, result);
        return 1;
      }
      else{  //resource is not available, add process to waitlist
        //fprintf(stderr, "\t\tRESOURCE[%d]: Not Available, Child[%d] added to waitlist\n", i, pcbIndex);
        pcb->resourceRequest = i;
        addToWaitList(&resourceList->resources[i], pcb, request[i]);
        return 0;
      }
    }
  }
  return 0;
}

//returns number of resource types that can allocated to waiting processes
int findFulfillableFromWaitList(resource_descriptor_list_t *resourceList, int *fulfillable){
  int allocated = 0;
  int i;
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){  //for each resource
    if(resourceList->resources[i].numResources > 0){
      int j;
      for(j = 0; j < PCB_TABLE_SIZE; j++){  //for each waitList slot
        if(resourceList->resources[i].waitList[j].pid == 0){  //waitList slot open, no more processes are in the list
          if(j != 0){  //at least 1 process can be fulfilled
            allocated++;
            int remaining = (resourceList->resources[i].numResources + 1) - (j + 1);
            if(remaining >= 0) fulfillable[i] = (j + 1); //all waiting processes can be fulfilled
            else  fulfillable[i] = resourceList->resources[i].numResources; //all available resources can be allocated, still have processes waiting 
            break;
          }
        }
      }
    }
  }
  return allocated;
}

int fulfillFromWaitList(resource_descriptor_t *resource){
  if(resource->waitListSize == 0) return -1;
  int iterator = resource->waitListIterator;
  if(resource->waitList[iterator++].pid == 0);  //no item at this spot in waitList
  while((iterator = (iterator % PCB_TABLE_SIZE)) != resource->waitListIterator){  //while iterator hasn't made a cycle
    if(resource->waitList[iterator].pid != 0) break;
    else iterator++;
  }
  resource->waitListIterator = (iterator + 1) % PCB_TABLE_SIZE;
  return iterator;
}

void addToWaitList(resource_descriptor_t *resource, pcb_t *pcb, const int request){
  int i;
  for(i = 0; i < PCB_TABLE_SIZE; i++){
    if(resource->waitList[i].pid == 0){  //found empty waitList slot
      resource->waitList[i].pid = pcb->pid;
      resource->waitList[i].numResources = request; 
      resource->waitListSize++;
      break;
    }
  }
}

void removeFromWaitList(resource_descriptor_list_t *resourceDescriptor, pcb_t *pcb){
  int i, j;
  //fprintf(stderr, "\t\t\t\t\t\t\t\t\tRemoving C[%d] from all waitLists\n", pcb->pid);
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    for(j = 0; j < PCB_TABLE_SIZE; j++){
      if(resourceDescriptor->resources[i].waitList[j].pid == pcb->pid){  //found pcb to remove
        //fprintf(stderr, "\t\t\t\t\t\t\t\t\tFound %d at R[%d]I[%d]\n", pcb->pid, i, j);
        resourceDescriptor->resources[i].waitList[j].pid = 0;
        resourceDescriptor->resources[i].waitListSize--;
        //fprintf(stderr, "\t\t\t\t\t\t\t\t\tR[%d]->waitListSize = %d\n", i, resourceDescriptor->resources[i].waitListSize);
      }
    }
  }
}

