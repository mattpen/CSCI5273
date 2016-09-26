/**
	* CSCI5273 Network Systems
	* Programming Assignment 1 - udp ftp server
	* Matt Pennington - mape5853
	*
	**/
#include <sys/types.h>
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
#include <string.h>

#define MAXBUFSIZE 100
#define INITRESPSIZE 256

// if ./{filename} exists, write it to the response
char* writegetresponse(char *filename, int *response_len) {	
	FILE *fp;
	char line[100];
	int i;

	int str_size = 256;
	int str_len = 0;
	char *response = calloc(str_size,sizeof(char));

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
		sprintf(response, "FILE NOT FOUND: %s", filename);
		response_len = 0;
		return;
	}

	/* Read the output a line at a time and write to the response. */
  while (fgets(line, sizeof(line) - 1, fp) != NULL) {
  	if ( str_len + sizeof(line) > str_size ) {
  		str_size = str_len + 10*sizeof(line);
  		response = realloc(response, (str_size + 1) * sizeof(char));
  	}
  	str_len += sizeof(line);
  	strcat(response, line);
  }

  /* close file */
  fclose(fp);	
  *response_len = str_len;
  return response;
}

// Write a list of all files in the current working directory to response
char* writelsresponse(int *response_len) {
	// Source for code getting directory data: http://stackoverflow.com/a/646254				
	FILE *fp;        // Holds result of popen
  char path[1035]; // Filename 

	int str_size = 256;
	int str_len = 0;
	char *response = calloc(str_size,sizeof(char));

  /* Open the command for reading. */
  fp = popen("/bin/ls ./", "r");
  if (fp == NULL) {
  	return;
  }
  
  /* Read the output a line at a time and write to the response. */
  while (fgets(path, sizeof(path)-1, fp) != NULL) {
  	printf("reading line: %s\n", path);
  	if ( str_len + sizeof(path) > str_size ) {
  		printf("resizing oldsize: %ld\n", sizeof(response));
  		str_size = str_len + 10*sizeof(path);
  		response = realloc(response, (str_size + 1) * sizeof(char));
  		printf("resizing newsize%ld\n", sizeof(response));
  	}
  	str_len += sizeof(path);
  	strcat(response, path);
  	printf("added to response\n");
  }

  /* close file */
  pclose(fp);	
  printf("inget:%s\n", response);
  *response_len = str_len;
  return response;
}

int main (int argc, char * argv[]) {
	if (argc != 2) {
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	int sock;                           //This will be our socket
	int errno;
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	int nbytes;                         //number of bytes we receive in our message
	char request[MAXBUFSIZE];           //a request to store our received message
	char buffer[MAXBUFSIZE];						// used to get data frames for put
	char frame[MAXBUFSIZE];							// used to send frames to client
	char filename[MAXBUFSIZE];
	int response_len = 0;								//number of char stored in response
	char *response = calloc(INITRESPSIZE, sizeof(char));	//response to write to client


	// control string, first holds the sequence characters then holds the ack characters
	char seqstring[8];
	char ackstring[8];
	long int seq = 0;
	int filenameend = 0;
	int framesize = MAXBUFSIZE-9;
	long int filesize = 0;

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine


	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}

	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("unable to bind socket\n");
	}

	remote_length = sizeof(remote);
	
	// Loop that waits for incoming message and sends a response
	while(1) {
		//clear memory
		bzero(request,sizeof(request));
		response_len = 0;
		bzero(response,sizeof(response));


		//waits for an incoming message
		nbytes = recvfrom(sock, request, sizeof(request), 0, (struct sockaddr*)&remote, &remote_length);
		printf("server: The client says %s", request);

		// Parse command from beginning of the request
		if ( strncmp(request, "get ", 4) == 0 ) {
			free(response);
			response = writegetresponse(request + 4, &response_len);
		}
		else if ( strncmp(request, "put ", 4) == 0 ) {
			// sizeof "put "
			int cmdsize = 4;

			// Get the ending index for the filename from the request string (the index offirst whitespace after "put ")
			filenameend;
			for(filenameend=cmdsize; request[filenameend] != ' '; filenameend++) {
				if ( request[filenameend] == '\0' ) {
					printf("filename or size error, recovery not implemented\n");
					return 0;
				} 
			}

			// Parse the filename and filesize from the put command
			// char *filename = malloc(sizeof(char) * (filenameend-cmdsize) + 1);
			bzero(filename,sizeof(filename));
			strncpy(filename, request+cmdsize, filenameend-cmdsize);
			filesize = strtol(request + filenameend, NULL, 10);

			// Send ACK for put command to client
			nbytes = sendto( sock, "ACK PUT", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));

			// Data holds the contents of the file to put.
			char *data = malloc(filesize*(sizeof(char)) + 1);
			bzero(data,sizeof(data));

			// receive data until file is full
			while ( strlen(data) < filesize ) {
				printf("filesize rcv/tot:%ld/%ld\n", strlen(data), filesize);

				// Get next frame
				bzero(buffer,sizeof(buffer));
				nbytes = recvfrom(sock, buffer, MAXBUFSIZE-1, 0, (struct sockaddr*)&remote, &remote_length);
				
				if ( strncmp(request, buffer, 4) == 0 ) {
					// Resend original ACK if necessary
					printf("resending ack put\n");
					nbytes = sendto(sock, "ACK PUT", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
				}
				else {
					// Get the sequence number from the buffer
					bzero(seqstring, sizeof(seqstring));
					strncpy(seqstring, buffer, sizeof(seqstring));
					printf("seqstring:%s\n", seqstring);
					seq = strtol(seqstring+4, NULL, 10);
					printf("rcvd seqString: %s\nparsed seq:%ld\n", seqstring, seq);

					// Write data from frame to memory
					strcpy(data + seq * framesize, buffer + 8);
					
					// Write the ACK and send
					bzero(seqstring, sizeof(seqstring));
					sprintf(seqstring, "ACK %4ld", seq);
					printf("acked: %s::\nseq: %ld::\n", seqstring, seq);
					nbytes = sendto(sock, seqstring, sizeof(seqstring), 0, (struct sockaddr*)&remote, sizeof(remote));
					if (nbytes < 0) {
						printf("Error sending ACK: errno=%d\n", errno);
					}
				}

			}

			// Write data to file
			FILE *fp = fopen(filename, "w");
	    if (fp != NULL)
	    {
        fputs(data, fp);
        fclose(fp);
	    }
	    
	    continue;
		}
		else if ( strncmp(request, "ls", 2) == 0 ) {
			free( response );
			response = writelsresponse(&response_len);
		}		
		// Tell client command was received, close sock and exit gracefully
		else if ( strncmp(request, "exit", 4) == 0 ) {
			nbytes = sendto( sock, "ACK EXIT", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
			close(sock);
			return 0;
		}
		// Send response for command not recognized
		else {
			strcat(response, "NOT RECOGNIZED: ");
			strncat(response, request, MAXBUFSIZE - 17);
			response_len = sizeof(response);
		}	

		
		// send response to client

		// start response and send filesize
		bzero(buffer, sizeof(buffer));
		strcat(buffer, "START ");
		strcat(buffer, response_len);
		response_received = 0;
		while (response_received == 0) {
			nbytes = sendto( sock, buffer, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));

			bzero(buffer,sizeof(buffer));
			nbytes = recvfrom(sock, buffer, sizeof(seqString), 0, (struct sockaddr*)&from_addr, &addr_length);

			// if the ACK matches stop listening
			if ( strncmp(buffer, "ACK START", 9) == 0 ) {
				response_received = 1;
			}
		}

		// send response string
		if ( response_len > 0 ) {}
			seq = 0;
			char *i = response;
			char *const end = i + response_len;

			// break the file into frames and send
			for(; i < end; i += MAXBUFSIZE-9) {
				bzero(seqString,sizeof(seqString));
				bzero(ackString,sizeof(ackString));
				bzero(frame,sizeof(frame));

				// sequence control string
				sprintf(seqString, "SEQ %4d", seq);
				// expected ack string
				sprintf(ackString, "ACK %4d", seq);
				// insert control string into the frame
				snprintf(frame, MAXBUFSIZE, "%s%s", seqString, i);
				
				// send the frame and wait for ACK
				response_received = 0;
				while (response_received == 0) {
					nbytes = sendto( sock, frame, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));

					bzero(buffer,sizeof(buffer));
					nbytes = recvfrom(sock, buffer, sizeof(seqString), 0, (struct sockaddr*)&from_addr, &addr_length);

					// if the ACK matches stop listening
					if ( strncmp(buffer, ackString, 8) == 0 ) {
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
		}

	}
}

