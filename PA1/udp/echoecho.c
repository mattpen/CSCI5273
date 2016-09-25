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

    #define MAX_STRING_SIZE 100
    #define FRAME_SIZE 3

int main() {
  char seqstring[8] = "seq    0";
  long int seq = strtol(seqstring + 4, NULL, 10);
  bzero(seqstring, sizeof(seqstring));
  sprintf(seqstring, "ACK %4ld", seq);
  printf("acked: %s\nseq: %ld\n", seqstring, seq);


  
  

      
  // char *longstr = malloc( sizeof( char ) * MAX_STRING_SIZE );
  // char *i = longstr;
  // char a[FRAME_SIZE] = "abc";
  // char b[FRAME_SIZE] = "def";
  // char c[FRAME_SIZE] = "ghi";
  
  // i = longstr + 1 * FRAME_SIZE;
  // strncpy(i, b, FRAME_SIZE);
  // i = longstr + 0 * FRAME_SIZE;
  // strncpy(i, a, FRAME_SIZE);
  // i = longstr + 2 * FRAME_SIZE;
  // strncpy(i, c, FRAME_SIZE);
  // printf("%s\n", longstr);
}