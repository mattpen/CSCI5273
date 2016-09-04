// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <netdb.h>
// #include <unistd.h>
// #include <signal.h>
// #include <fcntl.h>
// #include <errno.h>
// #include <sys/time.h>
// #include <memory.h>
// #include <errno.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

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
      char a[] = "1234";
      char *b = a;
      b += 1;
      printf("%i:%s, %i:%s\n", *a, a, *b, b);

    }