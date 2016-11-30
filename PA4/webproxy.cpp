/**
  * CSCI5273 Network Systems
  * Programming Assignment 4 - webproxy
  * Matt Pennington - mape5853
  *
  * Provides Web Proxy
  **/

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory.h>
#include <math.h>
#include <openssl/sha.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <unordered_map>

#define MSG_SIZE 8000
#define SIZE_MESSAGE_SIZE 17
#define CACHE_TTL 30

struct Request {
    std::string method;
    std::string uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string data;
    std::string filetype;
    std::string error;
    Request *next;
};

std::unordered_map<unsigned char *, std::vector<unsigned char> > cacheData;
std::unordered_map<unsigned char *, time_t> cacheExpireTime;

bool isWhiteSpace( char c );

bool isCRLF( char *buf, int i );

bool isKeepAlive( std::string connectionString );

Request *parseRequest( char *requestString, int requestString_len );

void requestCycle( int clientSocket );

void closeSocket( int fd );


int main( int argc, char *argv[] ) {
  if ( argc != 2 ) {
    printf( "USAGE: webproxy <server_port>\n" );
    exit( 1 );
  }
  int port = atoi( argv[ 1 ] );

  int serverSocket, clientSocket, c;
  struct sockaddr_in serverSockaddr_in, clientSockaddr_in;

  // Create socket
  serverSocket = socket( AF_INET, SOCK_STREAM, 0 );
  if ( serverSocket == -1 ) {
    perror( "Could not create socket" );
    exit( 1 );
  }

  // Allow server to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    perror( "setsockopt(SO_REUSEADDR) failed" );
    exit( 1 );
  }

  //Prepare the sockaddr_in structure
  serverSockaddr_in.sin_family = AF_INET;
  serverSockaddr_in.sin_addr.s_addr = INADDR_ANY;
  serverSockaddr_in.sin_port = htons( ( uint16_t ) atoi( argv[ 1 ] ) );


  if ( bind( serverSocket, ( struct sockaddr * ) &serverSockaddr_in, sizeof( serverSockaddr_in ) ) < 0 ) {
    perror( "bind failed. Error" );
    exit( 1 );
  }

  listen( serverSocket, 3 );
  c = sizeof( struct sockaddr_in );

  printf( "Listening...\n" );
  clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );

  while ( clientSocket ) {
    printf( "Socket accepted.\n" );

    if ( clientSocket < 0 ) {
      perror( "accept failed" );
      continue;
    }

    requestCycle( clientSocket );
    printf( "Listening...\n" );
    clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );
  }


}

std::vector<unsigned char> getRequest( int sock ) {
  unsigned char clientMessage[MSG_SIZE];
  ssize_t readSize = -1;
  std::vector<unsigned char> request = std::vector<unsigned char>();


  bzero( clientMessage, MSG_SIZE );
  readSize = recv( sock, clientMessage, MSG_SIZE - 1, 0 );
  printf( "Read(%ld): %s\n", readSize, clientMessage );


  if ( readSize == -1 ) {
    char err[6] = "ERROR";
    printf( "recv error" );
    request.insert( request.begin(), err, err + 5 );
    perror( "recv failed" );
    return request;
  }
  request.insert( request.end(), clientMessage, clientMessage + readSize );

  return request;
}

void sendResponse( std::vector<unsigned char> response, int sock ) {

  unsigned char *send_buffer = response.data();
  size_t responseSizeRemaining = response.size();
  while ( responseSizeRemaining > 0 ) {
    ssize_t send_size = send( sock, send_buffer, responseSizeRemaining, 0 );
    printf( "Client sent size(%ld), send_buffer(%*s)\n", send_size, ( int ) send_size, send_buffer );
    printf( "In hex:\n" );
    for ( int i = 0; i < responseSizeRemaining; i++ ) {
      printf( "%02X", send_buffer[ i ] );
    }
    printf( "\n" );

    if ( send_size < 0 ) {
      perror( "Error sending get body" );
    }
    send_buffer += send_size;
    responseSizeRemaining -= send_size;
  }
}

void *crawlPage( void *hashArg ) {
  unsigned char *hash = ( unsigned char * ) hashArg;
  // parse through cacheData[ hash ] and pre-fetch hrefs
}

std::vector<unsigned char> fetchResponse( std::string host, std::vector<unsigned char> requestData ) {
  std::vector<unsigned char> response = std::vector<unsigned char>();

  // open a new socket to the host/port
  // send the requestData
  // recv the response and pack it in a vector
  
  return response;
}

void requestCycle( int clientSocket ) {
  std::vector<unsigned char> requestData = getRequest( clientSocket );
  Request *request = parseRequest( requestData.data(), requestData.size() );

  if ( request->error.compare( "" ) != 0 ) {
    sendResponse( std::vector<unsigned char>( request->error.begin(), request->error.end() ), clientSocket );
  }

  unsigned char *hash = ( unsigned char * ) malloc( SHA256_DIGEST_LENGTH * sizeof( unsigned char ) );
  SHA256( ( unsigned char * ) request->uri.c_str(), request->uri.length(), hash );

  time_t currentTime;
  time( &currentTime );

  if ( cacheData.find( hash ) == cacheData.end() || cacheExpireTime[ hash ] < currentTime ) {
    std::vector<unsigned char> responseData = fetchResponse( request->headers[ "Host" ], requestData );
    cacheData[ hash ] = responseData;
    cacheExpireTime[ hash ] = currentTime + CACHE_TTL;
    sendResponse( responseData, clientSocket );
    closeSocket( clientSocket );

    pthread_t crawler_thread;
    if ( pthread_create( &crawler_thread, NULL, crawlPage, ( void * ) hash ) < 0 ) {
      perror( "could not create thread" );
      exit( 1 );
    }
  }
  else {
    sendResponse( cacheData[ hash ], clientSocket );
    closeSocket( clientSocket );
  }
}

// Returns true for spaces and tabs
bool isWhiteSpace( char c ) {
  return c == ' ' || c == '\t';
}

// Returns true for newline and carriage-return-line-feed
bool isCRLF( unsigned char *buf, int i ) {
  return buf[ i ] == '\n' || ( buf[ i ] == 13 && buf[ i + 1 ] == 10 );
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


/**
  * This method creates a Request object from an HTTP request string.
  * If the method or version is invalid or unsupported, then it puts an error response in request.error
  *
  * @param requestString - contains entire HTTP request
  * @param requestString_len - length of requestString
  * @returns Request
  **/
Request *parseRequest( unsigned char *requestString, unsigned long requestString_len ) {
  Request *request = new Request; // return value
  int i = 0; //index in requestString
  request->error = "";
  request->next = NULL;

  std::string key;
  std::string value;
  std::string fileext;

  // get method
  request->method = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) ) {
    request->method.append( 1u, requestString[ i ] );
    i++;
  }

  // Check for supported methods
  if ( request->method.compare( "GET" ) != 0 && request->method.compare( "POST" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
    request->error.append( request->method );
    request->error.append( "</body></html>" );
    return request;
  }

  // move to URI
  while ( i < requestString_len && isWhiteSpace( requestString[ i ] ) ) {
    i++;
  }

  // get uri
  request->uri = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) ) {
    request->uri.append( 1u, requestString[ i ] );
    i++;
  }

//  //This shouldn't be needed for proxy
//  //Check for directory request
//  fileext = "";
//  try {
//    fileext = request->uri.substr( request->uri.rfind( "." ), request->uri.length() - request->uri.rfind( "." ) );
//  }
//  catch ( std::out_of_range &exc ) {
//    // No file extension found, assume directory access requested, append / if necessary and append default webpage
//    fileext = ".html";
//    if ( request->uri[ request->uri.length() - 1 ] != '/' ) {
//      request->uri.append( "/" );
//    }
//    request->uri.append( config.index );
//  }
//
//  // Check filetype
//  try {
//    request->filetype = config.filetypes.at( fileext );
//  }
//  catch ( std::out_of_range &exc ) {
//    request->error = "HTTP/1.0 501 Not Implemented\n\n<html><body>501 Not Implemented Reason Filetype not supported: ";
//    request->error.append( fileext );
//    request->error.append( "</body></html>" );
//    return request;
//  }

  // move to version
  while ( i < requestString_len && isWhiteSpace( requestString[ i ] ) ) {
    i++;
  }

  // get version
  request->version = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) &&
          requestString[ i ] != 13 ) {
    request->version.append( 1u, requestString[ i ] );
    i++;
  }

  // move to end of first line
  while ( i < requestString_len && isWhiteSpace( requestString[ i ] ) ) {
    i++;
  }

  // read one new line, if next character is not a newline, then this request is invalid
  if ( i < requestString_len && isCRLF( requestString, i ) ) {
    if ( requestString[ i ] == '\n' ) {
      i++;
    }
    else if ( requestString[ i ] == 13 ) {
      i += 2;
    }
    else {
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  }

  printf( "request.version: {%s}\n", request->version.c_str() );

  if ( request->version.compare( "HTTP/1.0" ) != 0 ) {
//  if ( request->version.compare( "HTTP/1.0" ) != 0 && request->version.compare( "HTTP/1.1" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
    request->error.append( request->version );
    request->error.append( "</body></html>" );
    return request;
  }

  // get headers if any
  while ( i < requestString_len && !isCRLF( requestString, i ) ) {
    // Read header name
    key = "";
    while ( i < requestString_len && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) ) {
      key.append( 1u, requestString[ i ] );
      i++;
    }

    // move to value
    while ( i < requestString_len && isWhiteSpace( requestString[ i ] ) ) {
      i++;
    }

    // Read header value
    value = "";
    while ( i < requestString_len && !isCRLF( requestString, i ) ) {
      value.append( 1u, requestString[ i ] );
      i++;
    }

    // Add key:value to headers map
    request->headers[ key.substr( 0, key.length() - 1 ) ] = value;

    // printf("key:value = {%s:%s}\n", key.substr(0,key.length()-1).c_str(), value.c_str());

    // Read the CRLF
    if ( i < requestString_len && isCRLF( requestString, i ) ) {
      if ( requestString[ i ] == '\n' ) {
        i++;
      }
      else if ( requestString[ i ] == 13 ) {
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
    if ( requestString[ i ] == '\n' ) {
      i++;
    }
    else if ( requestString[ i ] == 13 ) {
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
      request->data.append( 1u, requestString[ i ] );
      i++;
    }
  }
    // get pipelined request
  else if ( request->method.compare( "GET" ) == 0 && request->version.compare( "HTTP/1.1" ) == 0 &&
            isKeepAlive( request->headers[ "Connection" ] ) &&
            strncmp( ( ( char * ) requestString ) + i, "GET", 3 ) == 0 ) {
    request->next = parseRequest( requestString + i, requestString_len - i );
  }

  return request;
}


// Reused from http://stackoverflow.com/a/12730776/2496827
int getSO_ERROR( int fd ) {
  int err = 1;
  socklen_t len = sizeof err;
  if ( -1 == getsockopt( fd, SOL_SOCKET, SO_ERROR, ( char * ) &err, &len ) ) {
    perror( "getSO_ERROR" );
  }
  if ( err )
    errno = err;              // set errno to the socket SO_ERROR
  return err;
}

// Reused from http://stackoverflow.com/a/12730776/2496827
void closeSocket( int fd ) {
  if ( fd >= 0 ) {
    getSO_ERROR( fd ); // first clear any errors, which can cause close to fail
    if ( shutdown( fd, SHUT_RDWR ) < 0 ) { // secondly, terminate the 'reliable' delivery
      if ( errno != ENOTCONN && errno != EINVAL ) { // SGI causes EINVAL
        perror( "shutdown" );
      }
    }
    if ( close( fd ) < 0 ) { // finally call close()
      perror( "close" );
    }
  }
}