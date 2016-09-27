/**
	* CSCI5273 Network Systems
	* Programming Assignment 1 - udp ftp server
	* Matt Pennington - mape5853
	*
	**/

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

char* writeputrequest(char* filename, int *request_len) {
	FILE *fp;
	char line[10];
	int i;

	int str_size = 256;
	char *request = calloc(str_size,sizeof(char));

	// Overwrite new line appended to command 
	filename[strlen(filename) - 1] = '\0';
	
	// Clear filepath and set to './' + filename
	char *curdir = "./";
	char *filepath = calloc(sizeof(curdir) + sizeof(filename) + 1, sizeof(char));
	strcat(filepath, curdir);
	strcat(filepath, filename);

	// Attempt to open the file
	fp = fopen(filepath, "r");
	if (fp == NULL) {
		printf("PUT: filename not found\n");	
		request_len = 0;
		return request;
	}

	/* Read the output a line at a time and write to the request. */
	while (fgets(line, sizeof(line) - 1, fp) != NULL) {
  	if ( strlen(request) + strlen(line) > str_size ) {
  		str_size *= 2;
  		request = realloc(request, (str_size + 1) * sizeof(char));
  	}
  	strcat(request, line);
  }

  /* close file */
  fclose(fp);	
  *request_len = strlen(request);
  return request;
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
	char *request = malloc(INITREQSIZE * sizeof(char));	//request to write to client
	char *data;
	int response_received = 0;

	int seq = 0;	//sequence number of the current unACK'd frame
	char seqstring[8];
	char ackstring[8];
	char filesizestring[MAXBUFSIZE];
	char filename[MAXBUFSIZE];
	int filenameend;
	int filesize;
	int framesize = MAXBUFSIZE-9;

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
	
	char command[MAXBUFSIZE];
	char commandresponse[MAXBUFSIZE];
	printf("Enter a command (get {filename}, put {filename}, ls, or exit):\nFilenames with spaces are not supported.\nCommands must be in all lowercase.\n\n");

	while (fgets(command, sizeof(command), stdin)) {
		bzero(request,sizeof(request));
		request_len = 0;

		struct sockaddr_in from_addr;
		int addr_length = sizeof(struct sockaddr);

		if ( strncmp(command, "put ", 4) == 0 ) {
			request = writeputrequest(command + 4, &request_len);

			// Append the size of the file to the put command
			bzero(filesizestring,sizeof(filesizestring));
			sprintf(filesizestring, " %d", request_len);
			strcat(command, filesizestring);
		}

		// If we are sending a put and the filesize is 0 don't do anything.
		if (strncmp( command, "put ", 4 ) != 0  || request_len > 0) {			
			//Resend command after timeout until ACK is received
			response_received = 0;
			while (response_received == 0) {
				// Send command
				nbytes = sendto(sock, command, sizeof(command), 0, (struct sockaddr*)&remote, sizeof(remote));
				if (nbytes < 0) {
					printf("Error sending ACK: errno=%d\n", errno);
					exit(0);
				}
				//Wait for ACK
				bzero(buffer,sizeof(buffer));
				bzero(commandresponse, sizeof(commandresponse));
				sprintf(commandresponse, "%s%s", "ACK ", command);
				nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
				if ( strcmp(buffer, commandresponse) == 0 ) {
					response_received = 1;
				}
			}
				
			// send request string (for put)
			if ( request_len > 0 ) {

				seq = 0;
				char *i = request;
				char *const end = request + request_len;
				
				// break the file into frames and send
				for(; i < end; i += framesize) {
					bzero(seqstring,sizeof(seqstring));
					bzero(ackstring,sizeof(ackstring));
					bzero(frame,sizeof(frame));
					
					// sequence control string
					sprintf(seqstring, "SEQ %4d", seq);
					// expected ack string
					sprintf(ackstring, "ACK %4d", seq);
					// insert control string into the frame
					snprintf(frame, MAXBUFSIZE, "%s%s", seqstring, i);
					
					// send the frame and wait for ACK
					response_received = 0;
					while (response_received == 0) {					
						nbytes = sendto( sock, frame, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
						if (nbytes < 0) {
							printf("Error sending ACK: errno=%d\n", errno);
							exit(0);
						}

						bzero(buffer,sizeof(buffer));
						nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
						
						// if the ACK matches stop listening
						if ( strncmp(buffer, ackstring, 8) == 0 ) {
							response_received = 1;
						}
					}

					// seq must not be more than 4 characters
					if (seq == 9999) {
						seq = 0;
					}
					else {
						seq++;
					}
				}

				printf("file sent successfully\n");
			}

			// receive response for get/ls/exit
			if (strncmp(command, "put ", 4) != 0) {
				if (strncmp(command, "get ", 4) == 0) {
					bzero(filename, sizeof(filename));
					strncpy(filename, command+4, strlen(command) - 5);
				}

				// Parse filesize from initial response
				response_received = 0;
				while( response_received == 0 ) {
					bzero(buffer,sizeof(buffer));
					nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);

					if (strncmp(buffer, "START ", 6) == 0) {
						filesize = strtol(buffer + 6, NULL, 10);
						nbytes = sendto( sock, "ACK START", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
						response_received = 1;
					}
				}

				// buffer holds the contents of the response.
				// free(data);
				data = calloc(filesize*2, sizeof(char));

				// receive data until file is full
				while ( strlen(data) < filesize ) {
					// Get next frame
					bzero(buffer,sizeof(buffer));
					nbytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &addr_length);
				
					// resend initial ACK if neccessary
					if (strncmp(buffer, "START ", 6) == 0) {
						filesize = strtol(buffer + 6, NULL, 10);
						nbytes = sendto( sock, "ACK START", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
						continue;
					}
					else if (strncmp(buffer, "SEQ ", 4) == 0) {

						// Get the sequence number from the buffer
						bzero(seqstring, sizeof(seqstring));
						strncpy(seqstring, buffer, sizeof(seqstring));
						// seq = strtol(seqstring+4, NULL, 10);
						seq = atoi(seqstring+4);

						// Write data from frame to memory
						strcpy(data + seq * framesize, buffer + 8);
						
						// Write the ACK and send
						bzero(seqstring, sizeof(seqstring));
						sprintf(seqstring, "ACK %4d", seq);
						nbytes = sendto(sock, seqstring, sizeof(seqstring), 0, (struct sockaddr*)&remote, sizeof(remote));
						if (nbytes < 0) {
							printf("Error sending ACK: errno=%d\n", errno);
						}
					}
					else {
						printf("PROCESSING ERROR: %s\n", buffer);
					}
				}

				// write get response to file
				if (strncmp( command, "get ", 4) == 0) {
					FILE *fp = fopen(filename, "w");
			    if (fp != NULL) {
		        fputs(data, fp);
		        fclose(fp);
		        printf("Wrote resonse to file: %s\n", filename);
			    }
				}
				else {
					printf("Server says:\n%s\n", data);
				}
			}
		}
	}
}

