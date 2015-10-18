#include "dcomm.h"

/* Delay to adjust speed in milliseconds */
#define DELAY 500

/* Threads */
pthread_t tid;
pthread_mutex_t lock;
void* mainThread(void* threadArgs);

/* Making-Connection-related Variables */
const char *hostname;	/* change this to use a different hostname */
const char *fileInput;
int servicePort;
struct sockaddr_in remAddr;
int sockfd,
		remAddrLen = sizeof(remAddr);

/* Global Variables */
int flagXOFF = 0,
		flagXON = 1,
		countSendBytes,
		rcvdBytesLen;
Byte buffer[2],
			controlChar = XON;
FILE* file;

int main(int argc, char const *argv[]) {

	/* Assign hostname, servicePort, and input file */
	hostname = argv[1];
	servicePort = atoi(argv[2]);
	fileInput = argv[3];

	/* Create a Socket */
	if((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		perror("Socket Created");
		return 0;
	}
	else{
		printf("Membuat socket untuk koneksi %s : %d\n",hostname,servicePort);
	}

	/* Bind Socket */
	memset((char *) &remAddr, 0, sizeof(remAddr));
	remAddr.sin_family = AF_INET;
	remAddr.sin_port = htons(servicePort);
	if (inet_aton(hostname, &remAddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	/* Initialize mutex thread */
	if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("\n mutex init failed\n");
    return 1;
	}

	/* Create main thread */
	pthread_create(&tid, NULL, mainThread, NULL); // thread for sending character from file

	while(1){
		rcvdBytesLen = recvfrom(sockfd, buffer, MAXLEN, 0, (struct sockaddr *)&remAddr, &remAddrLen);
		if(rcvdBytesLen > 0){
			controlChar = buffer[0];
			if(controlChar == XOFF && flagXOFF == 1){
				printf("XOFF diterima\n");
				flagXON = 1;
				flagXOFF = 0;
			}
			else if(controlChar == XON && flagXON == 1){
				printf("XON diterima\n");
				flagXON = 0;
				flagXOFF = 1;
			}
		}
		usleep(DELAY*1000); // delay time for Checking-Acknowledgement process
	}

	pthread_join(tid, NULL);
	pthread_mutex_destroy(&lock);

	return 0;
}

void* mainThread(void* threadArgs) {
	pthread_mutex_lock(&lock);

	char ch;
	file = fopen(fileInput, "r");
	if(file == NULL){
		perror("File error\n");
		return 0;
	}

	int i = 0;
	int scanfile = fscanf(file, "%c", &ch);
	while(scanfile != EOF){ // while not EOF
		if(controlChar == XON){
			countSendBytes++;
			printf("Mengirim byte ke-%d: '%c'\n", countSendBytes, ch);
			buffer[0] = ch;
			ssize_t lengthSentBytes = sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&remAddr, remAddrLen);
			if(lengthSentBytes < 0){
				perror("Failed to Send");
				return 0;
			}
			scanfile = fscanf(file, "%c", &ch);
		}
		else{
			printf("Menunggu XON...\n");
		}
		usleep(DELAY*1000); // delay time for Sending-Character process
	}

	if(scanfile == EOF){
		printf("Pembacaan dan pengiriman selesai...\n");
	}

	fclose(file);
	close(sockfd);

	pthread_mutex_unlock(&lock);

	return NULL;
}
