
/*
 * $Author: o1-mccune $
 * $Date: 2017/11/11 01:45:06 $
 * $Revision: 1.1 $
 * $Log: messagequeue.h,v $
 * Revision 1.1  2017/11/11 01:45:06  o1-mccune
 * Initial revision
 *
 */

#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#define ALLOCATE 4001
#define TERMINATE 3001
#define RELEASE 2001
#define REQUEST 1001

typedef struct{
  long type;
  int pcbIndex;
  int resources[NUM_RESOURCE_TYPES];
}message_t;

int getMessageQueue(int key);

int removeMessageQueue(int messageQueueId);

int sendMessage(const int messageQueueId, const long messageType, int pcbIndex, const int *resourceList);

#endif
