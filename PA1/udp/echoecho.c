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
#include <string.h>

#define MAX_STRING_SIZE 100
#define FRAME_SIZE 3


int main() {      
  char *longstr = malloc( sizeof( char ) * MAX_STRING_SIZE );
  char *i = longstr;
  char a[FRAME_SIZE] = "abc";
  char b[FRAME_SIZE] = "def";
  char c[FRAME_SIZE] = "ghi";
  int filesize = 9;
  

  printf("0 filesize%ld\n", strlen(longstr));

  i = longstr + 2 * FRAME_SIZE;
  strncpy(i, c, FRAME_SIZE);
  printf("c filesize%ld\n", strlen(longstr));

  i = longstr + 1 * FRAME_SIZE;
  strncpy(i, b, FRAME_SIZE);
  printf("b filesize%ld\n", strlen(longstr));
  
  i = longstr + 0 * FRAME_SIZE;
  strncpy(i, a, FRAME_SIZE);
  printf("a filesize%ld\n", strlen(longstr));

  printf("%s\n", longstr);
}