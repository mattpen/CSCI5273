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


int main (int argc, char * argv[])
{

	int nbytes;                             // number of bytes send by sendto()
	int sock;                               //this will be our socket
	char buffer[MAXBUFSIZE], buf[MAXBUFSIZE];

	struct sockaddr_in remote;              //"Internet socket address structure"

	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}

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
	printf("Enter a command (get {filename}, put {filename}, ls, or exit):\n");
	while (fgets(command, sizeof(command), stdin)) {
		/******************
		  sendto() sends immediately.  
		  it will report an error if the message fails to leave the computer
		  however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
		 ******************/
		// char command[] = "apple";	
		nbytes = sendto( sock, command, sizeof(command), 0, 
			(struct sockaddr*)&remote, sizeof(remote));

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

