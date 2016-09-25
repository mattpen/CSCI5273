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



// // Append src at the end of dest, allocating new memory if necessary
// void concat(char **dest, int *dest_len, int dest_size, char *src) {
// 	// If string gets too large, double the size
// 	if (*dest_len + strlen(src) > dest_size) {
// 		char **newstr;
// 		long int new_size = dest_size;
// 		while (*dest_len + strlen(src) > new_size) {
// 			new_size *= 2;
// 		}
// 		printf("%s\n", dest);
// 		printf("%s\n", src);
// 		printf("%ld\n", new_size);
// 		*newstr = calloc(new_size, sizeof(char));
// 		bzero(*newstr, sizeof(*newstr));
// 		strcat(*newstr, *dest);
// 		strcat(*newstr, src);
// 		dest = newstr;
// 	}
// 	else {
// 	  strcat(*dest, src); 
// 	}
// 	*dest_len += strlen(src);
// }

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
	// TODO: replace with bzero?
	for (i=0; i<sizeof(filepath); i++) {
		filepath[i] = '\0';
	}
	strcat(filepath, curdir);
	strcat(filepath, filename);

	// Attempt to open the file
	fp = fopen(filepath, "r");
	if (fp == NULL) {
		sprintf(response, "FILE NOT FOUND: %s", filename);
		return;
	}


	/* Read the output a line at a time and write to the response. */
  while (fgets(line, sizeof(line) - 1, fp) != NULL) {
  	printf("reading line: %s\n", line);
  	if ( str_len + sizeof(line) > str_size ) {
  		printf("resizing oldsize: %ld\n", sizeof(response));
  		str_size = str_len + 10*sizeof(line);
  		response = realloc(response, (str_size + 1) * sizeof(char));
  		printf("resizing newsize%ld\n", sizeof(response));
  	}
  	str_len += sizeof(line);
  	strcat(response, line);
  	printf("added to response\n");
  }

  /* close file */
  fclose(fp);	
  printf("inget:%s\n", response);
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
	char request[MAXBUFSIZE];            //a request to store our received message
	char buffer[MAXBUFSIZE];	// used to get data frames for put
	char filename[MAXBUFSIZE];
	int response_len = 0;								//number of char stored in response
	int response_size = INITRESPSIZE;		//maximum number of char allowed in response
	char *response = calloc(response_size, sizeof(char));											//response to write to client


	// control string, first holds the sequence characters then holds the ack characters
	char seqstring[8];
	long int seq;
	int filenameend;
	int framesize = MAXBUFSIZE-9;
	long int filesize;

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
			printf("inmain:%s\n", response);
		}
		else if ( strncmp(request, "put ", 4) == 0 ) {
			// sizeof "put "
			int cmdsize = 4;

			// Get the ending index for the filename (the index offirst whitespace after "put ")
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

			// data holds the contents of the file to put. It is of length data_len and has max size data_size 
			// int data_len = 0;
			// int data_size = 4;
			char *data = malloc(filesize*(sizeof(char)) + 1);
			bzero(data,sizeof(data));
			printf("filesize:%ld::sizeof(char):%ld\n", sizeof(data),sizeof(char));
			printf("received put cmd: %s\n", request);

			// bzero(buffer,sizeof(buffer));
			// nbytes = recvfrom(sock, buffer, MAXBUFSIZE-1, 0, (struct sockaddr*)&remote, &remote_length);
			// while ( strncmp(buffer, "END", sizeof("END")) != 0 ){
			while ( strlen(data) < filesize ) {
				printf("filesize rcv/tot:%ld/%ld\n", strlen(data), filesize);

				// Get next frame
				bzero(buffer,sizeof(buffer));
				nbytes = recvfrom(sock, buffer, MAXBUFSIZE-1, 0, (struct sockaddr*)&remote, &remote_length);
				
				// Resend original ACK if necessary
				if ( strncmp(request, buffer, 4) == 0 ) {
					printf("resending ack put\n");
					nbytes = sendto(sock, "ACK PUT", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
				}
				// Get data and seqnum from the buffer then send the ACK
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
						printf("errno:%d\n", errno);
					}
					printf("nbytes: %d::\n", nbytes);
				}

			}
			printf("done writing\n");

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
			printf("inmain:%s\n", response);
		}		
		else if ( strncmp(request, "exit", 4) == 0 ) {
			strcat(response, "EXIT");
			nbytes = sendto( sock, response, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
			close(sock);
			return 0;
		}
		else {
			strcat(response, "NOT RECOGNIZED: ");
			strncat(response, request, MAXBUFSIZE - 17);
		}

		printf("this should not be printed after a put\n");
		// send response to client
		// if ( strlen(response) < MAXBUFSIZE ) {
		if ( 1 == 2 ) {
			printf("sending single frame\n");
			nbytes = sendto( sock, response, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
		}
		else {
			printf("sending multi frame\n");
			char *i = response;
			char *const end = response + response_len;
			nbytes = sendto( sock, "START", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
			for(; i < end; i += MAXBUFSIZE-1) {
				nbytes = sendto( sock, i, MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
			}
			nbytes = sendto( sock, "END", MAXBUFSIZE, 0, (struct sockaddr*)&remote, sizeof(remote));
		}		
	}
}

