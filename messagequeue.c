 /*
 * $Author: o1-mccune $
 * $Date: 2017/11/11 01:42:36 $
 * $Revison: $
 * $Log: messagequeue.c,v $
 * Revision 1.1  2017/11/11 01:42:36  o1-mccune
 * Initial revision
 *
 */


#include "oss.h"
#include "messagequeue.h"
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int messageQueueId;

int getMessageQueue(int key){
  if((messageQueueId = msgget(key, IPC_CREAT | 0644)) == -1){
    perror("CHILD failed to get message queue");
    return -1;
  }
  return messageQueueId; 
}

int sendMessage(const int messageQueueId, const long messageType, int pcbIndex, const int *resourceList){
  int error = 0;
  message_t *message;
  if((message = malloc(sizeof(message_t))) == NULL) return -1;

  //memcpy(message->resources, resourceList, sizeof(int) * NUM_RESOURCE_TYPES);
  
  int i;
 // fprintf(stderr, "\t\t\t\t\t\tBUILDING MESSAGE: ");
  for(i = 0; i < NUM_RESOURCE_TYPES; i++){
    message->resources[i] = resourceList[i];
   // fprintf(stderr, "%d[%d] ", i, message->resources[i]);
  }

  
  message->type = messageType;
  //fprintf(stderr, "\n\t\t\t\t\t\tMESSAGE TYPE: %d\n", message->type);
  message->pcbIndex = pcbIndex;
  if(msgsnd(messageQueueId, message, sizeof(message_t), 0) == -1) error = errno;
  free(message);
  if(error){
    errno = error;
    return -1;
  }
  return 0;
}

int removeMessageQueue(int messageQueueId){
  return msgctl(messageQueueId, IPC_RMID, NULL);
}
