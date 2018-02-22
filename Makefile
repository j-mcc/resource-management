CC = gcc
DEPS = simulatedclock.h pcb.h bitarray.h resourcedescriptor.h oss.h messagequeue.h
CFLAGS = -g -I.

TARGET1 = oss
TARGET1OBJS = oss.o simulatedclock.o pcb.o bitarray.o resourcedescriptor.o messagequeue.o
TARGET1LIBS = -pthread -lm


TARGET2 = child
TARGET2OBJS = child.o simulatedclock.o pcb.o resourcedescriptor.o messagequeue.o
TARGET2LIBS = -pthread -lm


%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $<

$(TARGET1): $(TARGET1OBJS) 
	$(CC) -o $(TARGET1) $(TARGET1OBJS) $(TARGET1LIBS) $(CFLAGS)

$(TARGET2): $(TARGET2OBJS)
	$(CC) -o $(TARGET2) $(TARGET2OBJS) $(TARGET2LIBS) $(CFLAGS)

clean: 
	/bin/rm -f $(TARGET1) $(TARGET2) *.o *.txt
