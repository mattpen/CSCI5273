#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

#define MAXBUFSIZE 100
#define INITREQSIZE 256

// Append src at the end of dest, allocating new memory if necessary
void concat(char **dest, int *dest_len, int *dest_size, char *src) {
	// If string gets too large, double the size
  	while (*dest_len + strlen(src) > *dest_size) {
  		*dest_size *= 2;
  		*dest = realloc(*dest, *dest_size * sizeof(char));
  	}
    strncat(*dest, src, strlen(src));
    *dest_len += strlen(src);
}

int writeputrequest(char **request, int *request_len, int *request_size, char* filename) {
	FILE *fp;
	char line[10];
	int i;

	// Overwrite new line appended to command 
	filename[strlen(filename) - 1] = '\0';
	
	// Clear filepath and set to './' + filename
	char *curdir = "./";
	char *filepath = malloc(strlen(curdir) + strlen(filename) + 1);
	for (i=0; i<sizeof(filepath); i++) {
		filepath[i] = '\0';
	}
	strcat(filepath, curdir);
	strcat(filepath, filename);

	// Attempt to open the file
	fp = fopen(filepath, "r");
	if (fp == NULL) {
		printf("PUT: FILE NOT FOUND\n");	
		return 0;
	}


	/* Read the output a line at a time and write to the request. */
  while (fgets(line, sizeof(line) - 1, fp) != NULL) {
  	concat(request, request_len, request_size, line);
  }

  /* close file */
  fclose(fp);
  return 1;
}

int main (int argc, char * argv[])
{

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

	int nbytes;                             							// number of bytes send by sendto()
	int sock;                               							//this will be our socket
	char buffer[MAXBUFSIZE];
	char buf[MAXBUFSIZE];
	char frame[MAXBUFSIZE];
	struct sockaddr_in remote;              							//"Internet socket address structure"
	int request_len = 0;																	//number of char stored in request
	int request_size = INITREQSIZE;												//maximum number of char allowed in request
	char *request = malloc(request_size * sizeof(char));	//request to write to client
	int put_success = 0;
	int response_received = 0;

	int seq;	//sequence number of the current unACK'd frame
	char seqString[8];
	char ackString[8];
	char filesize[MAXBUFSIZE];

	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&remote,sizeof(remote));               //zero the struct
	remote.sin_family = AF_INET;                 //address family
	remote.sin_port = htons(atoi(argv[2]));      //sets port to network byte order
	remote.sin_addr.s_addr = inet_addr(argv[1]); //sets remote IP address

	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}

	// Set timeout for socket to 100ms
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
	    perror("Error");
	}
	
	char command[MAXBUFSIZE];
	printf("Enter a command (get {filename}, put {filename}, ls, or exit):\nfilenames with spaces are not supported");

	while (fgets(command, sizeof(command), stdin)) {

		if ( strncmp(command, "put ", 4) == 0 ) {
			bzero(request,sizeof(request));
			request_len = 0;

			struct sockaddr_in from_addr;
			int addr_length = sizeof(struct sockaddr);

			put_success = writeputrequest(&request, &request_len, &request_size, command + 4);

			// Append the size of the file to the put command
			bzero(filesize,sizeof(filesize));
			sprintf(filesize, " %ld", request_len * sizeof(char));
			strcat(command, filesize);

			if (put_success == 1) {
				
				//Resend command after timeout until ACK is received
				response_received = 0;
				while (response_received == 0) {
					bzero(buffer,sizeof(buffer));

					// Send put command
					printf("sending put request\n");
					nbytes = sendto( sock, command, sizeof(command), 0, (struct sockaddr*)&remote, sizeof(remote));

					//Wait for ACK
					nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
					if ( strncmp(buffer, "ACK PUT", 9) == 0 ) {
						printf("ack put rcvd\n");
						response_received = 1;
					}
				}

				// Break the message into frames and use stop-and-wait to send the message reliably
				seq = 0;
				char *i = request;
				char *const end = request + request_len;
				for(; i < end; i += MAXBUFSIZE-9) {
					bzero(seqString,sizeof(seqString));
					bzero(ackString,sizeof(ackString));
					bzero(frame,sizeof(frame));

					fprintf(stderr, "zeroed frame\n");

					sprintf(seqString, "SEQ %4d", seq);
					sprintf(ackString, "ACK %4d", seq);
					snprintf(frame, MAXBUFSIZE, "%s%s", seqString, i);
					

					response_received = 0;
					while (response_received == 0) {
						nbytes = sendto( sock, frame, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
						bzero(buffer,sizeof(buffer));
						nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
						printf("sending data: %s\n", seqString);
						if ( strncmp(buffer, ackString, strlen(seqString)) == 0 ) {
							printf("ACK received: %s\n",buffer);
							response_received = 1;
						}
						else {
							printf("got this when waiting: %s\n", buffer);
						}
					}

					// seq must not be more than 4 characters
					// TODO: evaluate this limit, is it too low or high?
					if ( seq == 9999 ) {
						seq = 0;
					}
					else {
						seq++;
					}
				}

				response_received = 0;
				while (response_received == 0) {
					bzero(buffer,sizeof(buffer));
					nbytes = sendto( sock, "END", sizeof("END"), 0, (struct sockaddr*)&remote, sizeof(remote));	
					nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
					printf("sent end\n");
					if ( strncmp(buffer, "ACK END", 9) == 0 ) {
						printf("recvd ack end\n");
						response_received = 1;
					}
				}
										
			}
			else {
				continue;
			}
		}
		else {
			nbytes = sendto( sock, command, sizeof(command), 0, (struct sockaddr*)&remote, sizeof(remote));
			/******************
			  sendto() sends immediately.  
			  it will report an error if the message fails to leave the computer
			  however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
			 ******************/

			// Blocks till bytes are received
			struct sockaddr_in from_addr;
			int addr_length = sizeof(struct sockaddr);
			bzero(buffer,sizeof(buffer));
			nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
			printf("Server says %s\n", buffer);

			if ( strncmp(buffer, "EXIT", 4) == 0 ) {
				close(sock);
				return 0;
			}
			else if( strncmp(buffer, "START", sizeof("START")) == 0 ) {
				int len = 0;
				int size = 4;
				char *longbuffer = malloc(size * sizeof(char));

				bzero(buffer,sizeof(buffer));
				nbytes = recvfrom(sock, buffer, MAXBUFSIZE-1, 0, (struct sockaddr*)&from_addr, &addr_length);
				while ( strncmp(buffer, "END", sizeof("END")) != 0 ){
					concat(&longbuffer, &len, &size, buffer);
					bzero(buffer,sizeof(buffer));
					nbytes = recvfrom(sock, buffer, MAXBUFSIZE-1, 0, (struct sockaddr*)&from_addr, &addr_length);
				}
				printf("Server says %s\n", longbuffer);
			}
		}
		
  }
}

