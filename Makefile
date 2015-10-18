CC=gcc
CFLAGS = -g
# uncomment this for SunOS
# LIBS = -lsocket -lnsl

all: transmitter receiver

transmitter: transmitter.c
	$(CC) -pthread -o transmitter transmitter.c $(LIBS)

receiver: receiver.c
	$(CC) -pthread -o receiver receiver.c $(LIBS)

clean:
	rm -f transmitter receiver transmitter.o receiver.o
