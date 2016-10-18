/**
  * CSCI5273 Network Systems
  * Programming Assignment 2 - web server
  * Matt Pennington - mape5853
  *
  **/

#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <memory.h>
#include <unordered_map> // Used for Request.headers and Config.filtypes
#include <fstream> // Used to retrieve files from the fs
#include <sstream> // Used to easily read/write small text buffers
#include <atomic> // Needed to maintain count of threads
#include <thread> // Needed for sleep to wait for threads to close after ctrl+c


struct Request {
  std::string method;
  std::string uri;
  std::string version;
  std::unordered_map <std::string, std::string> headers;
  std::string data;
  std::string filetype;
  std::string error;
  Request* next;
};

struct Config {
  int port;
  std::string docroot;
  std::string index;
  std::unordered_map <std::string, std::string> filetypes; // key is a file extension and value is a mime-type, ex {".txt":"text/plain"}
  int keepalivetime;
};

Config config;
bool done;
std::atomic<int> thread_count;

// Returns true for spaces and tabs
bool isWhiteSpace( char c ) { return c == ' ' || c == '\t'; }

// Returns true for newline and carriage-return-line-feed
bool isCRLF( char *buf, int i ) { return buf[i] == '\n' || (buf[i] == 13 && buf[i+1] == 10); }

bool isKeepAlive( std::string connectionString );

// Parses string into object and validates method and version
Request* parseRequest( char *requestString, int requestString_len );

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

  // Allow server to reuse port/addr if in TIME_WAIT state
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
       
    pthread_t sniffer_thread;
    new_sock = (int *) malloc(4);
    *new_sock = client_sock;

    if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)    {       
      perror("could not create thread");
      return 1;
    }
  }
  
  closeSocket( socket_desc );
  while( thread_count > 0 ) {
    printf("waiting for threads to close ...\n");
    std::this_thread::sleep_for (std::chrono::seconds(1));
  }
  return 0;
}
 

void ctrlc_handler(int s){
  printf("\n\nReceived kill signal %d\n",s);
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

  struct timeval timeout;      
  timeout.tv_sec = config.keepalivetime;
  timeout.tv_usec = 0;

  if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
    perror("setsockopt failed\n");
  }

  if (setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
    perror("setsockopt failed\n");
  }

  size_t read_size;
  std::string message;
  const int MSG_SIZE = 2000;
  char client_message[MSG_SIZE];
  Request *requestptr = new Request;
  requestptr->next = NULL;
  bool keepalive = true;  

  //Receive a message from client
  while ( keepalive ) {
    if ( requestptr->next == NULL ) {
      bzero(client_message, MSG_SIZE);
      read_size = recv(sock, client_message, MSG_SIZE, 0 );
      if(read_size == -1) {
        perror("recv failed");
        thread_count--;
        closeSocket( sock );
        return 0;
      }
      requestptr = parseRequest( client_message, strlen(client_message) ); 
    }
    else {
      // Read/Write pipelined requests before listening again
      requestptr = requestptr->next;
    }

    // Check for keep-alive header
    keepalive = isKeepAlive( requestptr->headers[ "Connection" ] );

    // Log request to stdout
    printf("%s\t%s\t%s\n", requestptr->method.c_str(), requestptr->uri.c_str(), requestptr->version.c_str());

    if ( requestptr->error.compare("") != 0 ) {
      message = requestptr->error;
      int bytes = send(sock , message.c_str(), message.length(), 0);
      if ( bytes < 0 ) {
        perror("Error sending error message");
      }
    }
    else {
      // Get the absolute path and open it
      std::string path = "";
      path.append( config.docroot );
      path.append( requestptr->uri );
      FILE *fp;
      fp = fopen( path.c_str(), "rb" );
      
      if ( fp != NULL ) {
        char buffer[MSG_SIZE];
        size_t filesize;
        std::string response;
        int file_read_size, num;

        fseek(fp, 0, SEEK_END);
        filesize = ftell(fp);
        rewind(fp);

        // Write and send headers
        response = "";
        response.append( requestptr->version );
        response.append( " 200 OK\nContent-Type: " );
        response.append( requestptr->filetype ); 
        if ( keepalive ) {
          response.append( "\nConnection: keep-alive\n" );
        }
        else {
          response.append( "\nConnection: close\n" );
        }

        if ( requestptr->method.compare("POST") == 0 ) {
          if ( requestptr->filetype.compare("text/html") == 0 ) {
            requestptr->data.insert( 0, "<h1>Post Data</h1>\n<pre>" );
            requestptr->data.append( "</pre>\n\n" );

            response.append( "\nContent-Length: " );
            response.append( std::to_string( filesize + requestptr->data.length() * sizeof(char) ) );
            response.append( "\n\n" );
            int bytes = send(sock , response.c_str(), response.length(), 0);
            if ( bytes < 0 ) {
              perror( "Error sending post headers" );
            }

            // Read file and stream to client
            std::string finalline;
            while (filesize > 0) {
              // Read part of the file
              int file_read_size = std::min(filesize, sizeof(buffer));
              bzero( buffer, sizeof(buffer) );
              file_read_size = fread(buffer, 1, file_read_size, fp);
              filesize -= file_read_size;

              // Look for end of html file and insert the POST data
              finalline = buffer;
              int pos = finalline.find( "</body>" );
              unsigned char *pbuf;
              if ( pos != std::string::npos ) {
                finalline.insert( pos, requestptr->data );
                pbuf = (unsigned char *) finalline.c_str();
                file_read_size = finalline.length();
              }
              else {
                pbuf = (unsigned char *) buffer;
              }

              // Send packets until buffer is completely sent
              while (file_read_size > 0) {
                int num = send(sock, pbuf, file_read_size, 0);
                if ( num < 0 ) {
                  perror( "Error sending post body" );
                }
                pbuf += num;
                file_read_size -= num;
              }
            }
            fclose( fp );
          }
          else {
            // Got a POST request for a binary file
            printf("405 for post filetype:%s\n", requestptr->filetype.c_str());
            message = "HTTP/1.0 405 Method Not Allowed\n\n<html><body>405 Method Not Allowed Reason: Invalid Method: ";
            message.append( requestptr->method );
            message.append( " for non-html filetype</body></html>" );
            int bytes = send(sock , message.c_str(), message.length(), 0);
            if ( bytes < 0 ) {
              perror( "Error sending post error message" );
            }
          }
        }
        else {
          response.append( "Content-Length: " );
          response.append( std::to_string( filesize ) );
          response.append( "\n\n" );
          int bytes = send(sock , response.c_str(), response.length(), 0);
          if ( bytes < 0 ) {
            perror( "Error sending get headers" );
          }
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
              if ( num < 0 ) {
                perror( "Error sending get body" );
              }
              pbuf += num;
              file_read_size -= num;
            }
          }
          fclose( fp );
        }
      }
      else {
        // Server cannot read the requested file, send 404 message
        message = "";
        message.append( requestptr->version );
        message.append( " 404 Not Found\n\n<html><body>404 Not Found Reason URL does not exist: " );
        message.append( requestptr->uri );
        message.append( "</body></html>" );
        int bytes = send(sock , message.c_str(), message.length(), 0);
        if ( bytes < 0 ) {
          perror( "Error sending 404" );
        }
      }    
    }
  }

  if(read_size == -1) {
    perror("recv failed");
  }
  closeSocket(sock); 
       
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
Request* parseRequest( char *requestString, int requestString_len ) {
  Request *request = new Request; // return value
  int i = 0; //index in requestString
  request->error = "";
  request->next = NULL;

  std::string key;
  std::string value;
  std::string fileext;

  // get method
  request->method = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString, i ) ) {
    request->method.append(1u, requestString[i]);
    i++;
  }

  // Check for supported methods
  if ( request->method.compare("GET") != 0 && request->method.compare("POST") != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
    request->error.append( request->method );
    request->error.append( "</body></html>" );
    return request;
  }

  // move to URI
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // get uri
  request->uri = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString, i ) ) {
    request->uri.append(1u, requestString[i]);
    i++;
  }

  //Check for directory request
  fileext = "";
  try {
    fileext = request->uri.substr( request->uri.rfind("."), request->uri.length() - request->uri.rfind(".") );
  }
  catch (std::out_of_range& exc) { 
    // No file extension found, assume directory access requested, append / if necessary and append default webpage
    fileext = ".html";
    if ( request->uri[ request->uri.length() - 1 ] != '/' ) {
      request->uri.append("/");
    }
    request->uri.append( config.index );
  }

  // Check filetype
  try {
    request->filetype = config.filetypes.at( fileext );
  }
  catch (std::out_of_range& exc) {
    request->error = "HTTP/1.0 501 Not Implemented\n\n<html><body>501 Not Implemented Reason Filetype not supported: ";
    request->error.append( fileext );
    request->error.append( "</body></html>" );
    return request;   
  }

  // move to version
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // get version
  request->version = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString, i ) && requestString[i] != 13 ) {
    request->version.append(1u, requestString[i]);
    i++;
  }

  // move to end of first line
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // read one new line and check version correctness
  if ( i < requestString_len && isCRLF( requestString, i ) ) {
    if ( requestString[i] == '\n' ){
      i++;
    }
    else if ( requestString[i] == 13 ){
      i += 2;
    }
    else {
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  }
  else if (  request->version.compare( "HTTP/1.0" ) != 0 && request->version.compare( "HTTP/1.1" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
    request->error.append( request->version );
    request->error.append( "</body></html>" );
    return request;
  }

  // get headers if any
  while ( i < requestString_len && !isCRLF( requestString, i ) ) {
    // Read header name
    key = "";
    while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString, i ) ) {
      key.append(1u, requestString[i]);
      i++;
    }

    // move to value
    while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
      i++;
    }

    // Read header value
    value = "";
    while ( i < requestString_len && !isCRLF( requestString, i ) ) {
      value.append(1u, requestString[i]);
      i++;
    }

    // Add key:value to headers map
    request->headers[ key.substr(0,key.length()-1) ] = value;

    // printf("key:value = {%s:%s}\n", key.substr(0,key.length()-1).c_str(), value.c_str());

    // Read the CRLF
    if ( i < requestString_len && isCRLF( requestString, i ) ) {
      if ( requestString[i] == '\n' ){
        i++;
      }
      else if ( requestString[i] == 13 ){
        i += 2;
      }
      else {
        request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
        return request;
      }
    }
  }

  // Read the second CRLF
  if ( i < requestString_len && isCRLF( requestString, i ) ) {
    if ( requestString[i] == '\n' ){
      i++;
    }
    else if ( requestString[i] == 13 ){
      i += 2;
    }
    else {
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  } 

  // get POST data
  if ( request->method.compare( "POST" ) == 0 ) {
    request->data = "";
    while ( i < requestString_len ) {
      request->data.append(1u, requestString[i]);
      i++;
    }
  }
  // get pipelined request
  else if ( request->method.compare( "GET" ) == 0 && request->version.compare( "HTTP/1.1" ) == 0 && isKeepAlive( request->headers[ "Connection" ] ) && strncmp( requestString + i, "GET", 3 ) == 0 ) {
    request->next = parseRequest( requestString + i, requestString_len - i );
  }

  return request;
}

// Reused from http://stackoverflow.com/a/12730776/2496827
int getSO_ERROR(int fd) {
   int err = 1;
   socklen_t len = sizeof err;
   if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len))
      perror("getSO_ERROR");
   if (err)
      errno = err;              // set errno to the socket SO_ERROR
   return err;
}

// Reused from http://stackoverflow.com/a/12730776/2496827
void closeSocket(int fd) {
   if (fd >= 0) {
      getSO_ERROR(fd); // first clear any errors, which can cause close to fail
      if (shutdown(fd, SHUT_RDWR) < 0) // secondly, terminate the 'reliable' delivery
         if (errno != ENOTCONN && errno != EINVAL) // SGI causes EINVAL
            perror("shutdown");
      if (close(fd) < 0) // finally call close()
         perror("close");
   }
}

//  There are apparently a lot of ways to spell keep-alive in common use, so lets check them all.
bool isKeepAlive( std::string connectionString ) {
  return connectionString.compare( "keepalive" ) == 0
      || connectionString.compare( "Keepalive" ) == 0
      || connectionString.compare( "KeepAlive" ) == 0
      || connectionString.compare( "keep-alive" ) == 0
      || connectionString.compare( "Keep-alive" ) == 0
      || connectionString.compare( "Keep-Alive" ) == 0;
}