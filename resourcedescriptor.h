/*
 * $Author: o1-mccune $
 * $Date: 2017/11/13 05:11:17 $
 * $Revision: 1.2 $
 * $Log: resourcedescriptor.h,v $
 * Revision 1.2  2017/11/13 05:11:17  o1-mccune
 * Final.
 *
 * Revision 1.1  2017/11/11 01:43:35  o1-mccune
 * Initial revision
 *
 */

#ifndef RESOURCEDESCRIPTOR_H
#define RESOURCEDESCRIPTOR_H

#include "pcb.h"
#include "oss.h"

typedef struct{
  pid_t pid;
  unsigned int numResources;
}wait_list_item_t;

typedef struct{
  unsigned int shared;
  unsigned int numResources;
  wait_list_item_t waitList[PCB_TABLE_SIZE];
  unsigned int waitListSize;
  unsigned int waitListIterator;
} resource_descriptor_t;

typedef struct{
  resource_descriptor_t resources[NUM_RESOURCE_TYPES];
} resource_descriptor_list_t;

void initResources(resource_descriptor_t *resources);

void addToWaitList(resource_descriptor_t *resource, pcb_t *pcb, const int request);

int findFulfillableFromWaitList(resource_descriptor_list_t *resourceList, int *fulfillable);

int allocateResource(resource_descriptor_list_t *resourceList, pcb_t *pcb, const int *request, const int pcbIndex);

void releaseResource(resource_descriptor_list_t *resourceList, pcb_t *pcb, const int *reclaimed, const int pcbIndex);

void releaseAllResources(resource_descriptor_list_t *resourceList, pcb_t *pcb, const int pcbIndex);

void removeFromWaitList(resource_descriptor_list_t *resourceDescriptor, pcb_t *pcb);

int fulfillFromWaitList(resource_descriptor_t *resource);
#endif
