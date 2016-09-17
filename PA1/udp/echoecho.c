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

    // void strOp(char *someString) {
    //   // snprintf( someString, MAX_STRING_SIZE, "0123456789abcdef");
    //   printf("%s\n", someString);
    // }

    void succ(int *n){
      *n = *n + 1;
    }
    void middle(int *j){
      succ(j);
    }
    int main() {
      int num = 34;

      char longstr[MAX_STRING_SIZE], frame[MAX_STRING_SIZE];
      sprintf(longstr, "abcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxyabcdefghijklmnopqrstuvwxy");
      snprintf(frame, MAX_STRING_SIZE, "%8d%s",num , longstr);
      printf("%s\n", frame);

      snprintf(frame, MAX_STRING_SIZE, "%3d%s",num , longstr + 3);
      printf("%s\n", frame);
    }