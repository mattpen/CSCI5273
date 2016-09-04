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

// if ./{filename} exists, write it to the response
void writegetresponse(char **response, int *response_len, int *response_size, char* filename) {	
	FILE *fp;
	char line[10];

	// Attempt to open the file
	// TODO: fix path problem....
	fp = fopen("/home/phet/CSCI5273/PA1/udp/lf", "r");
	if (fp == NULL) {
		concat(response, response_len, response_size, "FILE NOT FOUND: ");
		concat(response, response_len, response_size, filename);	
		return;
	}


	/* Read the output a line at a time and write to the response. */
  while (fgets(line, sizeof(line) - 1, fp) != NULL) {
  	concat(response, response_len, response_size, line);
  }

  /* close file */
  fclose(fp);	
}

// Write a list of all files in the current working directory to response
void writelsresponse(char **response, int *response_len, int *response_size) {
	// Source for code getting directory data: http://stackoverflow.com/a/646254				
	FILE *fp;        // Holds result of popen
  char path[1035]; // Filename 

  /* Open the command for reading. */
  fp = popen("/bin/ls ./", "r");
  if (fp == NULL) {
  	return;
  }
  
  /* Read the output a line at a time and write to the response. */
  while (fgets(path, sizeof(path)-1, fp) != NULL) {
  	concat(response, response_len, response_size, path);
  }

  /* close fileprotocol */
  pclose(fp);
}

int main (int argc, char * argv[]) {
	if (argc != 2) {
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	int exitcmd_received = 0;           //Flag to signal end of loop
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	int nbytes;                         //number of bytes we receive in our message
	char request[MAXBUFSIZE];            //a request to store our received message
	int response_len = 0;								//number of char stored in response
	int response_size = INITRESPSIZE;		//maximum number of char allowed in response
	char *response = malloc(response_size * sizeof(char));											//response to write to client

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
		nbytes = recvfrom(sock, request, sizeof(request), 0, 
			(struct sockaddr*)&remote, &remote_length);
		printf("server: The client says %s", request);

		

		// Parse command from beginning of the request
		if ( strncmp(request, "get ", 4) == 0 ) {
			writegetresponse(&response, &response_len, &response_size, request + 4);
		}
		else if ( strncmp(request, "put ", 4) == 0 ) {
			snprintf(response, sizeof(response), "PUT NOT IMPLEMENTED YET");
		}
		else if ( strncmp(request, "ls", 2) == 0 ) {
			writelsresponse(&response, &response_len, &response_size);
		}		
		else if ( strncmp(request, "exit", 4) == 0 ) {
			concat(&response, &response_len, &response_size, "EXIT");
			exitcmd_received = 1;
		}
		else {
			concat(&response, &response_len, &response_size, "NOT RECOGNIZED: ");
			concat(&response, &response_len, &response_size, request);
		}

		// send response to client
		if ( response_len < MAXBUFSIZE ) {
			nbytes = sendto( sock, response, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
		}
		else{
			char *i = response;
			char *const end = response + response_len;
			nbytes = sendto( sock, "START", sizeof("START"), 0, (struct sockaddr*)&remote, sizeof(remote));
			for(; i < end; i += MAXBUFSIZE-1) {
				printf("%.*s\n", MAXBUFSIZE, i);
				nbytes = sendto( sock, i, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
			}
			nbytes = sendto( sock, "END", sizeof("END"), 0, (struct sockaddr*)&remote, sizeof(remote));
		}		
		
		if ( exitcmd_received ) {
			close(sock);
			return 0;
		}
	}
}

