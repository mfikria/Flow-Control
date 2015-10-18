/*
* File : receiver.cpp
*/
#include "dcomm.h"
/* Delay to adjust speed in milliseconds */
#define DELAY 500
/* Define receive buffer size */
#define RXQSIZE 8
/* Define minimum upperlimit */
#define MIN_UPPERLIMIT 4
/* Define maximum lowerlimit */
#define MAX_LOWERLIMIT 1

Byte rxbuf[RXQSIZE];
QTYPE rcvq = { 0, 0, 0, RXQSIZE, rxbuf };
QTYPE *rxq = &rcvq;

/* Socket */
int sockfd; // listen on sock_fd
struct sockaddr_in thisAddr,	/* our address */
			remAddr;	/* remote address */
socklen_t remAddrLen = sizeof(remAddr);		/* length of addresses */

/* Thread */
pthread_t tid;
pthread_mutex_t lock;

/* Global Variables */
static int countRcvdBytes = 1,   // number of Received Bytes
					countCnsmdBytes = 1;   // number of Consumed Bytes
Byte buffer[2],
		currentByte,
		bufferSend[2],
		controlChar = XON;;
int servicePort;

/* Functions declaration */
void* mainThread(void* threadArgs);
/*
Main Thread for sending Acknowledgement
*/

static Byte *rcvchar(int sockfd, QTYPE *queue);
/*
Read a character from socket and put it to the receive buffer.
If the number of characters in the receive buffer is above certain
level, then send XOFF and set a flag (why?).
Return a pointer to the buffer where data is put.
*/

static Byte *q_get(QTYPE *, Byte *);
/*
Retrieve data from buffer, save it to "current" and "data"
If the number of characters in the receive buffer is below certain
level, then send XON.
Increment front index and check for wraparound.
q_get returns a pointer to the buffer where data is read or NULL if
buffer is empty.
*/

int isValidChar(char inChar);
/*
Check whether character is  a valid character? not ( >32, CR, LF atau end-of-file)
*/

void insertToQueue(QTYPE* Q, Byte input);
/*
Insert an element to Queue Q
*/

void getFromQueue(QTYPE* Q, Byte* output);
/*
Get an element from Queue Q and assign it to output variable
*/

int main(int argc, char *argv[]){

  //check whether argument is valid or not
	if (argc != 2) {
		printf("./receiver <Service Port>\n");
		return 0;
	}

	// define the service port
	servicePort = atoi(argv[1]);

  // create connection with UDP socket
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("cannot create socket\n");
    return 0;
  }

  // bind the socket to a specific port and IP address
  memset((char *)&thisAddr, 0, sizeof(thisAddr));
  thisAddr.sin_family = AF_INET;
  thisAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  thisAddr.sin_port = htons(servicePort);

  if (bind(sockfd, (struct sockaddr *)&thisAddr, sizeof(thisAddr)) < 0) {
    perror("bind failed");
    return 0;
  }
	else {
		printf("Binding pada 127.0.0.1:%d ...\n",servicePort);
	}

	if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("\n mutex init failed\n");
    return 1;
	}

	pthread_create(&tid, NULL, mainThread, NULL);

  while (1) {
    currentByte = *(rcvchar(sockfd, rxq));
    // Quit on end of file
    if (currentByte == Endfile) {
      exit(0);
    }
    usleep(DELAY*1000);
  }

	pthread_join(tid, NULL);
	pthread_mutex_destroy(&lock);

	return 0;
}

void* mainThread(void* threadArgs){
	pthread_mutex_lock(&lock);
	while(1){
		if(q_get(rxq,&currentByte) != NULL){
			if(isValidChar(currentByte)){
				printf("Mengkonsumsi byte ke-%d: ‘%c’\n", countCnsmdBytes++, currentByte);
			}
			else if (currentByte == Endfile){
				printf("end-of-file\n" );
				exit(0);
			}
		}
		usleep(DELAY*3000);
	}
	pthread_mutex_unlock(&lock);

	return NULL;
}

Byte *rcvchar(int sockfd, QTYPE *queue){
  if(controlChar == XON){
    int lengthRcvdBytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&remAddr, &remAddrLen);

    if(lengthRcvdBytes >= 0){
      printf("Menerima byte ke-%d.\n",countRcvdBytes);
      insertToQueue(&*queue,buffer[0]);
      countRcvdBytes++;
    }
    else{ //recvfrom() failed
      perror("recvfrom() does not get data");
    }

    if(queue->count > MIN_UPPERLIMIT) {
      printf("Buffer > minimum upperlimit. Mengirim XOFF.\n");
      controlChar = XOFF;
      bufferSend[0] = controlChar;
      ssize_t lengthSentBytes = sendto(sockfd, bufferSend, sizeof(bufferSend), 4, (struct sockaddr *)&remAddr, remAddrLen);
			if(lengthSentBytes < 0) {
        perror("sendto() failed)\n");
      }
    }
  }
  else{ // Send XOFF
    buffer[0] = 0;
  }
  return &buffer[0];
}

static Byte *q_get(QTYPE *queue, Byte *data){
  // Nothing in the queue
  if (queue->count == 0) return (NULL);
  else {
    do{
      getFromQueue(&*queue, &*data);
    }while(queue->count > 0 && !isValidChar(*data));

    if(queue->count < MAX_LOWERLIMIT){
      controlChar = XON;
      bufferSend[0] = controlChar;
      printf("Buffer < maksimum lowerlimit. Send XON\n");
      ssize_t lengthSentBytes = sendto(sockfd, bufferSend, sizeof(bufferSend), 4, (struct sockaddr *) &remAddr, remAddrLen);
      if(lengthSentBytes < 0){
          perror("sendto() failed\n");
      }
    }
  }
  return data;
}

int isValidChar(char inChar){
  return (inChar >= 32 || inChar == CR || inChar == Endfile || inChar == LF);
}

void insertToQueue(QTYPE *Q, Byte input){
  Q->data[Q->rear] = input;
  (Q->count)++;
  Q->rear = ((Q->rear) + 1) % Q->maxsize;
}

void getFromQueue(QTYPE* Q, Byte* output){
  (*output) = Q->data[Q->front];
  (Q->count)--;
  Q->front = ((Q->front) + 1) % Q->maxsize;
}
