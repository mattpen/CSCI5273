/**
  * CSCI5273 Network Systems
  * Programming Assignment 2 - web server
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
#include <unordered_map> // Used for Request.headers and Config.filtypes
#include <iostream> 
#include <unistd.h>    
#include <pthread.h> // Used to spawn response handlers
#include <fstream> // Used to retrieve files from the fs
#include <string> // Used to easily read/write small text buffers
#include <sstream> // Used to easily read/write small text buffers
#include <atomic> // Needed to maintain count of threads
#include <chrono> // Needed for sleep to wait for threads to close after ctrl+c
#include <thread> // Needed for sleep to wait for threads to close after ctrl+c
#include <queue>


struct Request {
  std::string method;
  std::string uri;
  std::string version;
  std::unordered_map <std::string, std::string> headers;
  std::string data;
  std::string filetype;
  std::string error;
};

struct Config {
  int port;
  std::string docroot;
  std::string index;
  std::unordered_map <std::string, std::string> filetypes; // key is a file extension and value is a mime-type, ex {".txt":"text/plain"}
  int keepalivetime;
};

Config config;
int *sockets;
bool done;
// std::queue<int> socks;
// std::atomic<bool> socks_lock;
std::atomic<int> thread_count;

bool isWhiteSpace( char c ) { return c == ' ' || c == '\t';}
bool isCRLF( char c ) { return c == '\n'; }

// Parses string into object and validates method and version
Request parseRequest( char *requestString, int requestString_len );

// Reads ./ws.conf and parses out the parameters for configuration.
// Initializes the global config object
void getConfig();

//the thread function
void *connection_handler(void *);

// listen for kill signal, sets done to true after catching signal
void ctrlc_handler(int s);

// clear socket errors
int getSO_ERROR(int fd);

// gracefully close sockets
void closeSocket(int fd);
 
int main(int argc , char *argv[])
{
  getConfig();
  int errno;

  thread_count = 0;

  // Listen for Ctrl + C
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = ctrlc_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Socket values
  int socket_desc, client_sock , c , *new_sock;
  struct sockaddr_in server , client;
   
  //Create socket
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc == -1)
  {
    printf("Could not create socket");
  }
  puts("Socket created");

  int enable = 1;
  if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    perror("setsockopt(SO_REUSEADDR) failed");
  }
   
  //Prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( config.port );

  //Bind
  if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
  {
    //print the error message
    perror("bind failed. Error");
    return 1;
  }
  puts("bind done");
   
  //Listen
  listen(socket_desc , 3);

  //Accept and incoming connection
  puts("Waiting for incoming connections...");
  c = sizeof(struct sockaddr_in);  

  while( !done && (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c) ) )
  {
    if (client_sock < 0) {
      perror("accept failed");
      continue;
    }

    puts("Connection accepted");
       
    pthread_t sniffer_thread;
    new_sock = (int *) malloc(1);
    *new_sock = client_sock;

    if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)    {       
      perror("could not create thread");
      return 1;
    }

    puts("Handler assigned");
  }
  
  closeSocket( socket_desc );
  while( thread_count > 0 ) {
    printf("waiting for threads to close ...\n");
    std::this_thread::sleep_for (std::chrono::seconds(1));
  }
  return 0;
}
 

void ctrlc_handler(int s){
  printf("\n\nCReceived kill signal %d\n",s);
  done = true;
}

/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
  thread_count++;

  //Get the socket descriptor
  int sock = *(int*)socket_desc;
  size_t read_size;
  std::string message;
  const int MSG_SIZE = 2000;
  char client_message[MSG_SIZE];
  Request request;
  

  //Receive a message from client
  bzero(client_message, MSG_SIZE);
  read_size = recv(sock, client_message, MSG_SIZE, 0);

  if(read_size == -1) {
    perror("recv failed");
    thread_count--;
    closeSocket( sock );
    return 0;
  }
  else {
    printf("opened sock: %d\n", sock);
  }

  request = parseRequest( client_message, strlen(client_message) );

  if ( request.error.compare("") != 0 ) {
    message = request.error;
    send(sock , message.c_str(), message.length(), 0);
  }
  else {
    // Get the absolute path and open it
    std::string path = "";
    path.append( config.docroot );
    path.append( request.uri );
    FILE *fp;
    fp = fopen( path.c_str(), "rb" );
    
    if ( fp != NULL ) {
      char buffer[MSG_SIZE];
      size_t filesize;
      std::string line;
      int file_read_size, num;

      fseek(fp, 0, SEEK_END);
      filesize = ftell(fp);
      rewind(fp);

      // Write and send headers
      line = "";
      line.append( request.version );
      line.append( " 200 OK\nContent-Type: " );
      line.append( request.filetype );
      line.append( "\nContent-Length: " );
      line.append( std::to_string( filesize ) ); 
      line.append( "\nConnection: close\n\n" );
      send(sock , line.c_str(), line.length(), 0);

      // Read file and stream to client
      while (filesize > 0) {
        // Read part of the file
        size_t file_read_size = std::min(filesize, sizeof(buffer));
        file_read_size = fread(buffer, 1, file_read_size, fp);
        filesize -= file_read_size;

        // Send packets until buffer is completely sent
        unsigned char *pbuf = (unsigned char *) buffer;
        while (file_read_size > 0) {
            int num = send(sock, pbuf, file_read_size, 0);
            pbuf += num;
            file_read_size -= num;
        }
      }
    }
    else {
      // Server cannot read the requested file, send 404 message
      message = "";
      message.append( request.version );
      message.append( " 404 Not Found\n\n<html><body>404 Not Found Reason URL does not exist: " );
      message.append( request.uri );
      message.append( "</body></html>" );
      send(sock , message.c_str(), message.length(), 0);
    }    
  }
  
  closeSocket(sock); 
  printf("sock closed %d\n", sock);
       
  //Free the socket pointer
  free(socket_desc);
  thread_count--;
  return 0;
}


/**
  *
  **/
void getConfig() {
  std::ifstream infile("ws.conf");
  std::string line; // Used to read a line of the configuration file
  std::string key;
  std::string val;
  std::size_t first_whitespace = 0; // Used to parse extension from ContentType
  std::size_t second_whitespace = 0; // Used to parse mime-type from ContentType

  config.port = 0;
  config.keepalivetime = 0;
  config.docroot = "";
  config.index = "";

  while (std::getline(infile, line))
  {
      std::istringstream iss(line);
      try {
        if ( line.find( "#" ) == 0 ) {
          // Don't try to parse comments
          continue;
        }
        else if ( line.find( "ListenPort" ) == 0 ) {
          config.port = std::stoi( line.substr( line.rfind(" ") + 1, line.length() - line.rfind(" ") - 1 ) );
          if ( config.port <= 1024 ) {
            perror("Invalid port number");
            exit(1);
          }
        }
        else if ( line.find( "KeepaliveTime" ) == 0 ) {
          config.keepalivetime = std::stoi( line.substr( line.rfind(" ") + 1, line.length() - line.rfind(" ") - 1 ) );
        }
        else if ( line.find( "DocumentRoot" ) == 0 ) {
          config.docroot = line.substr( line.rfind(" ") + 1, line.length() - line.rfind(" ") - 1 );
        }
        else if ( line.find( "DirectoryIndex" ) == 0 ) {
          config.index.append( line.substr( line.rfind(" ") + 1, line.length() - line.rfind(" ") - 1) );
        }
        else if ( line.find( "ContentType" ) == 0 ) {
          first_whitespace = line.find(" ");
          second_whitespace = line.find(" ", first_whitespace + 1);
          key = line.substr( first_whitespace + 1, second_whitespace - first_whitespace - 1);
          val = line.substr( second_whitespace + 1, line.length() - second_whitespace - 1 );
          config.filetypes[ key ] = val;
        }
        else {
          // Add support for additional directives here
        }
      }
      catch (int e) {
        // If configuration parse fails then exit immediately
        printf("Error occurred reading line: %s in config, error message: %d\n", line.c_str(), e);
        exit(1);
      }
  }
  
  if ( config.port == 0 ) {
    perror( "Port missing from ws.conf" );
    exit(1);
  }
  return;
}

/**
  * This method creates a Request object from an HTTP request string.
  * If the method or version is invalid or unsupported, then it puts an error response in request.error
  *
  * @param requestString - contains entire HTTP request
  * @param requestString_len - length of requestString
  * @returns Request
  **/
Request parseRequest( char *requestString, int requestString_len ) {
  Request request; // return value
  int i = 0; //index in requestString
  request.error = "";

  std::string key;
  std::string value;
  std::string fileext;

  // get method
  request.method = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
    request.method.append(1u, requestString[i]);
    i++;
  }

  // Check for supported methods
  if ( request.method.compare("GET") != 0 && request.method.compare("POST") != 0 ) {
    request.error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
    request.error.append( request.method );
    request.error.append( "</body></html>" );
    return request;
  }

  // move to URI
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // get uri
  request.uri = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
    request.uri.append(1u, requestString[i]);
    i++;
  }

  //Check for directory request
  fileext = "";
  try {
    fileext = request.uri.substr( request.uri.rfind("."), request.uri.length() - request.uri.rfind(".") );
  }
  catch (std::out_of_range& exc) { 
    // No file extension found, assume directory access requested, append / if necessary and append default webpage
    fileext = ".html";
    if ( request.uri[ request.uri.length() - 1 ] != '/' ) {
      request.uri.append("/");
    }
    request.uri.append( config.index );
  }

  // Check filetype
  try {
    request.filetype = config.filetypes.at( fileext );
  }
  catch (std::out_of_range& exc) {
    request.error = "HTTP/1.0 501 Not Implemented\n\n<html><body>501 Not Implemented Reason Filetype not supported: ";
    request.error.append( fileext );
    request.error.append( "</body></html>" );
    return request;   
  }

  // move to version
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // get version
  request.version = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) && requestString[i] != 13 ) {
    request.version.append(1u, requestString[i]);
    i++;
  }

  // move to end of first line
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // read one new line and check version correctness
  if ( i < requestString_len && isCRLF( requestString[i] ) ) {
    i++;
  }
  // else if ( !(i == requestString_len) || request.version.compare( "HTTP/1.0") != 0 || request.version.compare( "HTTP/1.1") != 0) {
  else if (  request.version.compare( "HTTP/1.0" ) != 0 && request.version.compare( "HTTP/1.1" ) != 0 ) {
    request.error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
    request.error.append( request.version );
    request.error.append( "</body></html>" );
    return request;
  }

  // get headers if any
  while ( i < requestString_len && !isCRLF( requestString[i] ) ) {
    // Read header name
    key = "";
    while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
      key.append(1u, requestString[i]);
      i++;
    }

    // move to value
    while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
      i++;
    }

    // Read header value
    value = "";
    while ( i < requestString_len && !isCRLF( requestString[i] ) ) {
      value.append(1u, requestString[i]);
      i++;
    }

    // Add key:value to headers map
    request.headers[key] = value;

    // read one new line
    if ( i < requestString_len && isCRLF( requestString[i] ) ) {
      i++;
    }
  }

  // read an empty line
  if ( i < requestString_len && isCRLF( requestString[i] ) ) {
    i++;
  }

  // get data
  request.data = "";
  while ( i < requestString_len ) {
    request.data.append(1u, requestString[i]);
  }

  return request;
}

int getSO_ERROR(int fd) {
   int err = 1;
   socklen_t len = sizeof err;
   if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
      perror("getSO_ERROR");
   if (err)
      errno = err;              // set errno to the socket SO_ERROR
   return err;
}

void closeSocket(int fd) {      // *not* the Windows closesocket()
   if (fd >= 0) {
      getSO_ERROR(fd); // first clear any errors, which can cause close to fail
      if (shutdown(fd, SHUT_RDWR) < 0) // secondly, terminate the 'reliable' delivery
         if (errno != ENOTCONN && errno != EINVAL) // SGI causes EINVAL
            perror("shutdown");
      if (close(fd) < 0) // finally call close()
         perror("close");
   }
}