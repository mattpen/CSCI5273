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
#include <string>
#include <unordered_map>
#include <iostream>
#include <unistd.h>    
#include <pthread.h> 
#include <fstream>
#include <sstream>

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

bool isWhiteSpace( char c ) { return c == ' ' || c == '\t';}
bool isCRLF( char c ) { return c == '\n'; }

// Parses string into object and validates method and version
Request parseRequest( char *requestString, int requestString_len );

// Reads ./ws.conf and parses out the parameters for configuration.
// Initializes the global config object
void getConfig();

//the thread function
void *connection_handler(void *);
 
int main(int argc , char *argv[])
{
  getConfig();

  int socket_desc , client_sock , c , *new_sock;
  struct sockaddr_in server , client;
   
  //Create socket
  socket_desc = socket(AF_INET , SOCK_STREAM , 0);
  if (socket_desc == -1)
  {
    printf("Could not create socket");
  }
  puts("Socket created");
   
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

  while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
  {
    puts("Connection accepted");
       
    pthread_t sniffer_thread;
    new_sock = (int *) malloc(1);
    *new_sock = client_sock;
     
    if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
    {       
      perror("could not create thread");
      return 1;
    }
     
    //Now join the thread , so that we dont terminate before the thread
    //pthread_join( sniffer_thread , NULL);
    puts("Handler assigned");
  }
   
  if (client_sock < 0)
  {
    perror("accept failed");
    return 1;
  }
   
  return 0;
}
 
/*
 * This will handle connection for each client
 * */
void *connection_handler(void *socket_desc)
{
  //Get the socket descriptor
  int sock = *(int*)socket_desc;
  int read_size;
  std::string message;
  const int MSG_SIZE = 2000;
  char client_message[MSG_SIZE];
  char buffer[MSG_SIZE];
  Request request;
  std::string path;
  int bytes_read;
  int total_bytes;
  std:: string response;

  //Receive a message from client
  read_size = recv(sock, client_message, MSG_SIZE, 0);
  request = parseRequest( client_message, strlen(client_message) );

  if ( request.error.compare("") != 0 ) {
    message = request.error;
    send(sock , message.c_str(), message.length(), 0);
  }
  else {
    // Get the absolute path and open it
    path = "";
    path.append( config.docroot );
    path.append( request.uri );
    printf("request for path:%s\n", path.c_str() );
    FILE *fp;
    fp = fopen(path.c_str(), "r");
    
    total_bytes = 0;
    bytes_read = 0;
    message = "";
    while ( bytes_read >= 0 ) {
        bytes_read = read(fp, buffer, sizeof(buffer));
        if (bytes_read == 0) // We're done reading from the file
            break;
        total_bytes += bytes_read;
        message.append( buffer );
    }

    fclose(fp);

    if ( total_bytes > 0 ) {
      response = "";
      response.append( request.version );
      response.append( " 200 OK\nContent-Type: " );
      response.append( request.filetype );
      response.append( "\nContent-Length: " );
      response.append( std::to_string( total_bytes ) );
      response.append( "\nConnection: close\n\n" )
      message.insert( 0, response );

      //send file
      void *p = message.c_str();
      while (total_bytes > 0) {
          int bytes_written = write(sock, p, total_bytes);
          if (bytes_written <= 0) {
              perror( "bytes_written < 0??" );
          }
          total_bytes -= bytes_written;
          p += bytes_written;
      }
    }
    else {
      // Server cannot read the requested file, send 404 message
      message = "";
      message.append( request.version );
      message.append( "404 Not Found\n\n<html><body>404 Not Found Reason URL does not exist: " );
      message.append( request.uri );
      message.append( "</body></html>" );
      send(sock , message.c_str(), message.length(), 0);
    }    
  }
  
  close(sock);
   
  if(read_size == -1)
  {
    perror("recv failed");
  }
       
  //Free the socket pointer
  free(socket_desc);
   
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
          config.index = line.substr( line.rfind(" ") + 1, line.length() - line.rfind(" ") -1);
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
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
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
  else if ( !(i == requestString_len) || request.version.compare( "HTTP/1.0" ) != 0 ) {
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