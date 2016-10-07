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
#include <unistd.h>    //write
#include <pthread.h> //for threading , link with lpthread
#include <fstream>
#include <sstream>

struct Request {
  std::string method;
  std::string uri;
  std::string version;
  std::unordered_map <std::string, std::string> headers;
  std::string data;
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
bool isWhiteSpace( char c ) {
  return c == ' ' || c == '\t';
}

bool isCRLF( char c ) {
  return c == '\n';
}

// validateRequest
  // if !(getMethod returns supported value
  // && getURI has supported filetype
  // && if getVersion returns supported value)
    //respond(400)
  // else
    //return true

// getMethod
  // read charaters until first whitespace and store in string
  // return string and index of whitespace character
// getURI
  // read
// getVersion
// getHeaders -- returns headers in unordered_map if any exist
// getData


// writeResponse
  // validateRequest
  // if invalidRequest
    // respond(400)

  // parse method
  // if method not supported  

// handleRequest
  // validateRequest
  // getHeaders

  // if request contains http1.1 version and keep-alive header
    // set starttime = currenttime
    // loop until currenttime - timeout >= starttime
      // start new thread or process and writeResponse
      // receiveMessage
      
  // else if not keep-alive
    // writeResponse


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
     
    //Send some messages to the client
    message = "HTTP/1.1 200 OK\n\n<html><body>Hello World</body></html>\n";

    //Receive a message from client
    read_size = recv(sock, client_message, MSG_SIZE, 0);

    printf("%s\n", client_message);

    //Send the message back to client
    send(sock , message.c_str(), message.length(), 0);
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
        }
        else if ( line.find( "KeepaliveTime" ) == 0 ) {
          config.keepalivetime = std::stoi( line.substr( line.rfind(" "), line.length() - line.rfind(" ") ) );
        }
        else if ( line.find( "DocumentRoot" ) == 0 ) {
          config.docroot = line.substr( line.rfind(" "), line.length() - line.rfind(" ") );
        }
        else if ( line.find( "DirectoryIndex" ) == 0 ) {
          config.index = line.substr( line.rfind(" "), line.length() - line.rfind(" ") );
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
  std::string filetype;

  // get method
  request.method = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
    request.method.append(1u, requestString[i]);
    i++;
  }

  // Check for supported methods
  if ( request.method.compare("GET") != 0 && request.method.compare("POST") != 0 ) {
    request.error = "400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
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

  // TODO::   check for valid filetype
  filetype = request.uri.substr( request.uri.rfind("."), request.uri.length() - request.uri.rfind(".") );


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
  else if ( !(i == requestString_len) || request.version.compare( "HTTP/1.0") != 0 || request.version.compare( "HTTP/1.1") != 0) {
    request.error = "400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
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