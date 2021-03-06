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
#include <openssl/sha.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <unordered_map>
#include <netdb.h>
#include <atomic>
#include <mutex>

#define MSG_SIZE 80000
#define SIZE_MESSAGE_SIZE 17


typedef unsigned char BYTE;
typedef std::vector<BYTE> ByteVector;
typedef std::string String;

struct Request {
    String method;
    String uri;
    String version;
    std::unordered_map<String, String> headers;
    String data;
    String fileext;
    String error;
    Request *next;
};

struct CachedResponse {
    ByteVector data;
    String host;
    String uri;
    time_t expireTime;
    std::mutex lock;
};

int CACHE_TTL = 300;

std::unordered_map<String, CachedResponse *> cache;

bool isWhiteSpace( char c );

bool isCRLF( BYTE *buf, int i );

bool isKeepAlive( String connectionString );

Request *parseRequest( BYTE *requestString, unsigned long requestStringLength );

String getKey( String host, String url );

void *requestCycle( void *incomingSocket );

void *crawlPage( void *hashArg );

void *cleanCache( void * );

void closeSocket( int fd );

void putInCache( String key, CachedResponse *cachedResponse );

ByteVector fetchResponse( String host, String httpVersion, ByteVector requestData );

ByteVector getRequest( int sock );

void sendResponse( ByteVector response, int sock );

int getSO_ERROR( int fd );

void closeSocket( int fd );

//////////////
//////
////// MAIN
//////
//////////////
int main( int argc, char *argv[] ) {
  if ( argc < 2  || argc > 3) {
    printf( "USAGE: webproxy <server_port> [<cache_timeout>]\n" );
    exit( 1 );
  }
  int port = atoi( argv[ 1 ] );

  if ( argc == 3 ) {
    CACHE_TTL = atoi( argv[2] );
    printf("Setting cache ttl to %d\n", CACHE_TTL);
  }

  int serverSocket, clientSocket, c, *tempSocket;
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

  printf( "Listening on port %d ...\n", port );
  clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );

//  pthread_t cacheCleaner;
//  if ( pthread_create( &cacheCleaner, NULL, cleanCache, NULL ) < 0 ) {
//    perror( "could not create thread" );
//    return 1;
//  }

  while ( clientSocket ) {
    if ( clientSocket < 0 ) {
      perror( "accept failed" );
      continue;
    }

    pthread_t snifferThread;
    tempSocket = ( int * ) malloc( 4 );
    *tempSocket = clientSocket;

    if ( pthread_create( &snifferThread, NULL, requestCycle, ( void * ) tempSocket ) < 0 ) {
      perror( "could not create thread" );
      return 1;
    }
    clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );
  }
}

/**
 * Threadsafe method for storing new response to the cache
 *
 * @param key
 * @param cachedResponse
 */
void putInCache( String key, CachedResponse *cachedResponse ) {
  // Update an expired entry in the cache
  if ( cache.find( key ) != cache.end() ) {
    printf( "Updating key(%s) in cache\n", key.c_str() );
    std::lock_guard<std::mutex> newLock( cache[ key ]->lock );
    cache[ key ] = cachedResponse;
  }

    // Add a new entry to the cache
  else {
    printf( "Adding key(%s) to cache\n", key.c_str() );
    cache[ key ] = cachedResponse;
  }
}

/**
 * This thread is usually sleeping
 * It wakes up and clears unused entries from the cache every 2*TTL
 *
 * @param arg - unused, required for pthread
 * @return unused, required for pthread
 */
void *cleanCache( void *arg ) {
  while ( true ) {
    sleep( CACHE_TTL );
    printf( "Clearing cache\n" );
    time_t currentTime;
    time( &currentTime );
    typedef std::unordered_map<String, CachedResponse *>::iterator it_type;
    for ( it_type i = cache.begin(); i != cache.end(); i++ ) {
      if ( i->second->expireTime < currentTime ) {
        printf( "Deleting from cache(%s)", i->first.c_str() );
        delete ( cache[ i->first ] );
      }
    }
  }
}

/**
 * Reads a request from a client specified by sock.
 * TODO: implement overflow checking/handling
 *
 * @param sock
 * @return
 */
ByteVector getRequest( int sock ) {
  BYTE clientMessage[MSG_SIZE];
  ByteVector request = ByteVector();

  bzero( clientMessage, MSG_SIZE );
  ssize_t readSize = recv( sock, clientMessage, MSG_SIZE - 1, 0 );

  // Log the first line of the request
  printf( "Read(%ld): ", readSize );
  for ( int i = 0; clientMessage[ i ] != '\n' && i < readSize; i++ ) { printf( "%c", clientMessage[ i ] ); }
  printf( "\n" );

  // Handle errors
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


/**
 * Sends an http response to a client specified by sock
 *
 * @param response
 * @param sock
 */
void sendResponse( ByteVector response, int sock ) {
  BYTE *sendBuffer = response.data();
  size_t responseSizeRemaining = response.size();
  while ( responseSizeRemaining > 0 ) {
    ssize_t bytesSent = send( sock, sendBuffer, responseSizeRemaining, 0 );
    if ( bytesSent < 0 ) {
      perror( "Error sending get body" );
    }
    sendBuffer += bytesSent;
    responseSizeRemaining -= bytesSent;
  }
}

/**
 * Gets thread-safe data from cache for non-time-sensitive crawling
 *
 * @param hash - key for the cache
 * @param page - this function writes the cached response data to this string
 * @param host - this function writes the cache host to this string
 */
void getHostUriAndDataFromCacheForCrawling( String key, String *page, String *host, String *uri ) {
  std::lock_guard<std::mutex> lock( cache[ key ]->lock );
  CachedResponse *cachedResponse = cache[ key ];
  *page = ( char * ) cachedResponse->data.data();
  *host = cachedResponse->host;
  *uri = cachedResponse->uri;
}


/**
 * Main thread function for prefetching links in a page
 *
 * @param hashArg - points to a SHA-256 BYTE[]
 */
void *crawlPage( void *hashArg ) {
  String key = ( char * ) hashArg;

  // Get the cached entry
  String page;
  String host;
  String originalHost;
  String originalUri;
  getHostUriAndDataFromCacheForCrawling( key, &page, &originalHost, &originalUri );

  // Invariant for progressing through the crawled page
  size_t minIndex = 0;

  // Parse through the files, looking for URI data within tags (href and src attributes)
  while ( minIndex < page.length() ) {

    // Look for the next html tag, enclosed by <>
    // Stop processing if no more tags found
    size_t nextLAngle = page.find( "<", minIndex );
    if ( nextLAngle == String::npos ) { break; }
    size_t nextRAngle = page.find( ">", nextLAngle );
    if ( nextRAngle == String::npos ) { break; }
    String tag = page.substr( nextLAngle, nextRAngle - nextLAngle + 1 );

    // Fast forward the loop variable to past the closing tag
    minIndex = nextRAngle + 1;

    // Get the content of href or src attributes, if present
    size_t nextItem = std::min( tag.find( "href=\"" ), tag.find( "src=\"" ) );
    size_t hrefBegin = tag.find( "\"", nextItem );
    size_t hrefEnd = tag.find( "\"", hrefBegin + 1 );

    // If the item found is within the current tag, process it
    if ( hrefEnd >= hrefBegin
         && hrefBegin != String::npos
         && hrefEnd != String::npos ) {

      // Parse the url from the tag
      String url = tag.substr( hrefBegin + 1, hrefEnd - hrefBegin - 1 );

      // Don't prefetch same page anchors
      if ( url.find( "#" ) == 0 ) {

        continue;
      }

        // If we found a relative path, prepend the current location (using the original url as the best guess)
      else if ( url[ 0 ] != '/' && url.find( "http://" ) != 0 && url.find( "https://" ) != 0 ) {

        originalUri = originalUri.substr( 0, originalUri.rfind( "/" ) + 1 );
        url = originalUri + url;
      }

      // Construct the HTTP request, we use HTTP/1.0 by default for simplicity's sake
      String requestString = "GET " + url + " HTTP/1.0\n\n";
      ByteVector requestData = ByteVector( requestString.begin(), requestString.end() );

      // If the uri is external to the page, find the host from the uri,
      // otherwise assume the link is relative
      if ( url.find( "http://" ) == 0 || url.find( "https://" ) == 0 ) {
        size_t hostStart = url.find( "//" ) + 2;
        size_t hostEnd = url.find( "/", hostStart );
        hostEnd = ( hostEnd == std::string::npos ) ? url.length() - 1 : hostEnd;
        host = url.substr( hostStart, hostEnd - hostStart );
      }
      else {
        host = originalHost;
      }

      // Create a new key value
      BYTE *newHash = ( BYTE * ) malloc( SHA256_DIGEST_LENGTH * sizeof( BYTE ) );
      String newKey = getKey( host, url );
      SHA256( ( BYTE * ) key.c_str(), key.length(), newHash );


      // If the uri is not cached or the cache is expired, add to cache
      time_t currentTime;
      time( &currentTime );
      if ( cache.find( newKey ) == cache.end() || cache[ newKey ]->expireTime < currentTime ) {

        // Get the response
        ByteVector response = fetchResponse( host, "HTTP/1.0", requestData );

        // If we got a response cache it
        if ( response.size() > 0 ) {

          // Create new cached entry
          CachedResponse *newResponse = new CachedResponse();
          newResponse->data = response;
          newResponse->expireTime = currentTime + CACHE_TTL;
          newResponse->host = host;
          newResponse->uri = url;

          putInCache( newKey, newResponse );
        }
      }
    }
  }

  return 0;
}

/**
 * Get an HTTP response from a remote server
 *
 * @param host - Remote hostname or ip addr
 * @param httpVersion - Request version, used to determine tcp disconnect method
 * @param requestData - original request to send
 * @return HTTP response
 */
ByteVector fetchResponse( String host, String httpVersion, ByteVector requestData ) {
  ByteVector response = ByteVector();

  // Create the socket for the remote connection
  struct sockaddr_in server;
  int newSock = socket( AF_INET, SOCK_STREAM, 0 );
  if ( newSock == -1 ) {
    printf( "Could not connect to socket for host(%s)", host.c_str() );
    return response;
  }

//  // Add a 1 second timeout
//  struct timeval timeout;
//  timeout.tv_sec = 1;
//  timeout.tv_usec = 0;
//  if ( setsockopt( newSock, SOL_SOCKET, SO_RCVTIMEO, ( char * ) &timeout, sizeof( timeout ) ) < 0 ) {
//    printf( "Could not set socket timeout for host(%s)\n", host.c_str() );
//    return response;
//  }
//  if ( setsockopt( newSock, SOL_SOCKET, SO_SNDTIMEO, ( char * ) &timeout, sizeof( timeout ) ) < 0 ) {
//    printf( "Could not set socket timeout for host(%s)\n", host.c_str() );
//    return response;
//  }

  // Allow client to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( newSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    printf( "Could not reuse socket for host(%s)\n", host.c_str() );
    return response;
  }

  // Find port, if specified in the host string
  // Assume 80 otherwise
  int port = 80;
  std::string temp = host;
  if ( host.find( "http" ) == 0 ) {
    temp = host.substr( 6, host.length() - 6 );
  }
  size_t delimiterPosition = temp.find( ":" );
  if ( delimiterPosition != String::npos && delimiterPosition < host.length() ) {
    port = std::stoi( host.substr( delimiterPosition + 1, host.length() - delimiterPosition + 1 ) );
    host = host.substr( 0, delimiterPosition );
  }

  // Host was not provided in headers, parse through the requestData for the host
  if ( host.compare("") == 0 ) {
    String requestString = (char *) requestData.data();
    size_t beginHost = requestString.find("http");
    if (beginHost != String::npos ) {
      beginHost = requestString.find( "://", beginHost ) + 3;
      size_t endHost = std::min( requestString.find( "/", beginHost ), requestString.find(" ", beginHost) );
      if ( endHost > beginHost && endHost != String::npos ) {
        host = requestString.substr( beginHost, endHost - beginHost );
      }
    }
  }

  printf("Looking for host(%s)\n", host.c_str());
  // Find the remote address information
  struct hostent *newHostent = gethostbyname( host.c_str() );
  if ( newHostent == NULL ) {
    printf( "Could not find host from hostname(%s)\n", host.c_str() );
    return response;
  }
  memcpy( &server.sin_addr, newHostent->h_addr_list[ 0 ], ( size_t ) newHostent->h_length );
  server.sin_family = AF_INET;
  server.sin_port = htons( ( uint16_t ) port );

  // Connect to the remote host, bail if connection fails
  if ( connect( newSock, ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) {
    printf( "Connect failed for host(%s). Error: %d\n", host.c_str(), errno );
    return response;
  }

  // Prepare the outgoing buffer
  ssize_t bytesToSend = requestData.size();
  BYTE *sendBuffer = requestData.data();

  // Send the request to the remote server
  while ( bytesToSend > 0 ) {
    ssize_t bytesSent = send( newSock, sendBuffer, requestData.size(), 0 );
    bytesToSend -= bytesSent;
    sendBuffer += bytesSent;

    if ( bytesSent < 0 ) {
      printf( "Error sending get request to host(%s)\n", host.c_str() );
    }
  }

  // Prepare the incoming buffer
  unsigned char readBuffer[MSG_SIZE];
  bzero( readBuffer, MSG_SIZE );
  ssize_t bytesReceived;

  // If request is HTTP/1.0 just read data until the client closes the stream
  if ( httpVersion.compare( "HTTP/1.0" ) == 0 ) {
    bytesReceived = recv( newSock, readBuffer, MSG_SIZE - 1, 0 );
    while ( bytesReceived > 0 ) {
      response.insert( response.end(), readBuffer, readBuffer + bytesReceived );
      bzero( readBuffer, MSG_SIZE );
      bytesReceived = recv( newSock, readBuffer, MSG_SIZE - 1, 0 );
    }
  }
    // If request is HTTP/1.1 we need to find out if it uses Content-Length or chunked encoding by parsing the headers
  else {
    // Read the header a byte at a time so we don't read any of the body
    std::string header = "";
    while ( ( bytesReceived = recv( newSock, readBuffer, 1, 0 ) ) > 0 ) {
      header.push_back( readBuffer[ 0 ] );
      bzero( readBuffer, MSG_SIZE );
      if ( header.length() > 2 && header.find( "\r\n\r\n" ) != std::string::npos ) {
        break;
      }
    }

    // If response uses Content-Length, read that many bytes
    if ( header.find( "Content-Length:" ) != std::string::npos ) {
      printf( "Getting body using contentLength\n" );
      size_t begin = header.find( ":", header.find( "Content-Length:" ) );
      size_t end = header.find( "\n", begin );
      ssize_t contentLength = std::stoi( header.substr( begin + 1, end - begin ) );
      printf( "Found contentLength(%ld), ", contentLength );
      while ( response.size() < contentLength ) {
        bzero( readBuffer, MSG_SIZE );
        bytesReceived = recv( newSock, readBuffer, MSG_SIZE - 1, 0 );
        printf( "bytesReceived(%ld)\n", bytesReceived );
        response.insert( response.end(), readBuffer, readBuffer + bytesReceived );
      }
    }

      //If response is using chunked encoding, read bytes until we receive a 0 chunk
    else if ( header.find( "Transfer-Encoding: chunked" ) != std::string::npos ) {
      std::string chunkDescription = "";
      ssize_t chunkSize = 1;

      while ( chunkSize > 0 ) {
        // Read the chunk string
        chunkDescription = "";
        bzero( readBuffer, MSG_SIZE );
        while ( recv( newSock, readBuffer, 1, 0 ) > 0 ) {
          chunkDescription.push_back( readBuffer[ 0 ] );
          response.push_back( readBuffer[ 0 ] );
          bzero( readBuffer, MSG_SIZE );
          if ( chunkDescription.length() > 2 && chunkDescription.find( "\r\n" ) != std::string::npos ) {
            break;
          }
        }

        // Get the decimal value for the chunk size
        chunkSize = strtol( chunkDescription.c_str(), NULL, 16 );;

        // Read chunkSize bytes from the sockeet
        bytesReceived = 0;
        while ( bytesReceived < chunkSize ) {
          bzero( readBuffer, MSG_SIZE );
          bytesReceived += recv( newSock, readBuffer, 1, 0 );
          response.push_back( readBuffer[ 0 ] );
        }
      }
    }

    else {
      // Bad things have happened.  We could assume things like the server will close the connection
      // or that we can close the connection after a timeout, but this isn't specified in RFC2616
      printf( "No Content-Length or Transfer-Encoding header received.\n" );
      return response;
    }

    // Put the headers back in the response
    response.insert( response.begin(), header.begin(), header.end() );
  }

  // Handle errors, ignore timeouts (errno == 11)
  if ( bytesReceived == -1 && errno != 11 ) {
    printf( "recv failed for host(%s), errno(%d)\n", host.c_str(), errno );
  }

  return response;
}


/**
 * Thread for handling an incoming request and outgoing response
 *
 * @param incomingSocket
 * @return unused
 */
void *requestCycle( void *incomingSocket ) {
  //Get the socket descriptor
  int clientSocket = *( int * ) incomingSocket;

  // Get and validate the incoming request
  ByteVector requestData = getRequest( clientSocket );
  Request *request = parseRequest( requestData.data(), requestData.size() );

  // Handle validation errors
  if ( request->error.compare( "" ) != 0 ) {
    sendResponse( ByteVector( request->error.begin(), request->error.end() ), clientSocket );
    return 0;
  }

  // Create a new key for the hash
  BYTE *hash = ( BYTE * ) malloc( SHA256_DIGEST_LENGTH * sizeof( BYTE ) );
  String key = getKey( request->headers[ "Host" ], request->uri );
  SHA256( ( BYTE * ) key.c_str(), key.length(), hash );

  time_t currentTime;
  time( &currentTime );

  // Log why the page was not retrieved from the cache
  if ( cache.find( key ) == cache.end() ) { printf( "Cached response not found\n" ); }
  else if ( cache[ key ]->expireTime < currentTime ) { printf( "Cached response expired\n" ); }

  // Handle a page not found in the cache or with an expired entry
  if ( cache.find( key ) == cache.end() || cache[ key ]->expireTime < currentTime ) {

    // Get the request form the remote server
    ByteVector responseData =
      fetchResponse( request->headers[ "Host" ], request->version, requestData );

    // Send it back to the client
    sendResponse( responseData, clientSocket );
    closeSocket( clientSocket );

    // Create new cache entry
    CachedResponse *newCacheEntry = new CachedResponse();
    newCacheEntry->data = responseData;
    newCacheEntry->expireTime = currentTime + CACHE_TTL;
    newCacheEntry->host = request->headers[ "Host" ];
    newCacheEntry->uri = request->uri;

    putInCache( key, newCacheEntry );

    // Prefetch links from this page
    // If the response wasn't an html page, there probably aren't any crawlable links
    if ( request->fileext.compare( ".html" ) == 0 ) {
      pthread_t crawlerThread;
      if ( pthread_create( &crawlerThread, NULL, crawlPage, ( void * ) key.c_str() ) < 0 ) {
        perror( "could not create thread" );
        exit( 1 );
      }
    }
  }
  else {
    // Log a successfully retrieved response from cache
    printf( "Found cached response for key(%s)\n", key.c_str() );
    sendResponse( cache[ key ]->data, clientSocket );
    closeSocket( clientSocket );
  }

  return 0;
}

// Returns true for spaces and tabs
bool isWhiteSpace( char c ) {
  return c == ' ' || c == '\t';
}

// Returns true for newline and carriage-return-line-feed
bool isCRLF( BYTE *buf, int i ) {
  return buf[ i ] == '\n' || ( buf[ i ] == 13 && buf[ i + 1 ] == 10 );
}

//  There are apparently a lot of ways to spell keep-alive in common use, so lets check them all.
bool isKeepAlive( String connectionString ) {
  return connectionString.compare( "keepalive" ) == 0
         || connectionString.compare( "Keepalive" ) == 0
         || connectionString.compare( "KeepAlive" ) == 0
         || connectionString.compare( "keep-alive" ) == 0
         || connectionString.compare( "Keep-alive" ) == 0
         || connectionString.compare( "Keep-Alive" ) == 0;
}


/**
 * This creates a uniform key for the cache, to be consistent between client requests and crawl requests
 *
 * @param host - domain name for the resource, with or without protocol
 * @param url - path to the resource, can be absolute or relative
 * @return key for creating a hash value for the cache
 */
String getKey( String host, String url ) {
  // If url already has the host, just strip the protocol off and return it
  if ( url.find( "http://" ) == 0 || url.find( "https://" ) == 0 ) {
    size_t start = url.find( "//" ) + 2;
    return url.substr( start, url.length() - start );
  }

  // If the host string has a protocol, strip it off
  if ( host.find( "http://" ) == 0 || host.find( "https://" ) == 0 ) {
    size_t start = host.find( "//" ) + 2;
    host = host.substr( start, host.length() - start );
  }

  // Strip any trailing or leading '/' chars
  while ( host.find( "/" ) == host.length() - 1 ) {
    host = host.substr( 0, host.length() - 2 );
  }
  while ( url.find( "/" ) == 0 ) {
    url = url.substr( 1, url.length() - 2 );
  }

  // Don't include anchors in key
  size_t anchorPosition = url.find( "#" );
  if ( anchorPosition != std::string::npos ) {
    url = url.substr( anchorPosition, url.length() - anchorPosition );
  }

  return host + "/" + url;
}

/**
  * This method creates a Request object from an HTTP request string.
  * If the method or version is invalid or unsupported, then it puts an error response in request.error
  *
  * (Mostly reused from PA2)
  *
  * @param requestString - contains entire HTTP request
  * @param requestStringLength - length of requestString
  * @returns Request
  **/
Request *parseRequest( BYTE *requestString, unsigned long requestStringLength ) {
  Request *request = new Request; // return value
  int i = 0; //index in requestString
  request->error = "";
  request->next = NULL;

  String key;
  String value;
  String fileext;

  // get method
  request->method = "";
  while ( i < requestStringLength && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) ) {
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
  while ( i < requestStringLength && isWhiteSpace( requestString[ i ] ) ) {
    i++;
  }

  // get uri
  request->uri = "";
  while ( i < requestStringLength && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) ) {
    request->uri.append( 1u, requestString[ i ] );
    i++;
  }

  //This shouldn't be needed for proxy
  //Check for directory request
  request->fileext = "";
  try {
    request->fileext = request->uri.substr( request->uri.rfind( "." ),
                                            request->uri.length() - request->uri.rfind( "." ) );
  }
  catch ( std::out_of_range &exc ) {
    request->fileext = ".html";
  }



  // move to version
  while ( i < requestStringLength && isWhiteSpace( requestString[ i ] ) ) {
    i++;
  }

  // get version
  request->version = "";
  while ( i < requestStringLength && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) &&
          requestString[ i ] != 13 ) {
    request->version.append( 1u, requestString[ i ] );
    i++;
  }

  // move to end of first line
  while ( i < requestStringLength && isWhiteSpace( requestString[ i ] ) ) {
    i++;
  }

  // read one new line, if next character is not a newline, then this request is invalid
  if ( i < requestStringLength && isCRLF( requestString, i ) ) {
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

//  printf( "request.version: {%s}\n", request->version.c_str() );

  if ( request->version.compare( "HTTP/1.0" ) != 0 ) {
//  if ( request->version.compare( "HTTP/1.0" ) != 0 && request->version.compare( "HTTP/1.1" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
    request->error.append( request->version );
    request->error.append( "</body></html>" );
    return request;
  }

  // get headers if any
  while ( i < requestStringLength && !isCRLF( requestString, i ) ) {
    // Read header name
    key = "";
    while ( i < requestStringLength && !isWhiteSpace( requestString[ i ] ) && !isCRLF( requestString, i ) ) {
      key.append( 1u, requestString[ i ] );
      i++;
    }

    // move to value
    while ( i < requestStringLength && isWhiteSpace( requestString[ i ] ) ) {
      i++;
    }

    // Read header value
    value = "";
    while ( i < requestStringLength && !isCRLF( requestString, i ) ) {
      value.append( 1u, requestString[ i ] );
      i++;
    }

    // Add key:value to headers map
    request->headers[ key.substr( 0, key.length() - 1 ) ] = value;

    // printf("key:value = {%s:%s}\n", key.substr(0,key.length()-1).c_str(), value.c_str());

    // Read the CRLF
    if ( i < requestStringLength && isCRLF( requestString, i ) ) {
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
  if ( i < requestStringLength && isCRLF( requestString, i ) ) {
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
    while ( i < requestStringLength ) {
      request->data.append( 1u, requestString[ i ] );
      i++;
    }
  }
    // get pipelined request
  else if ( request->method.compare( "GET" ) == 0 && request->version.compare( "HTTP/1.1" ) == 0 &&
            isKeepAlive( request->headers[ "Connection" ] ) &&
            strncmp( ( ( char * ) requestString ) + i, "GET", 3 ) == 0 ) {
    request->next = parseRequest( requestString + i, requestStringLength - i );
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