/**
  * CSCI5273 Network Systems
  * Programming Assignment 3 - distributed file system
  * Matt Pennington - mape5853
  *
  * Provides Distributed File Server
  **/

#include <netinet/in.h>
#include <unistd.h>
#include <memory.h>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <atomic>
#include <thread>
#include <iostream>
#include <vector>
#include <sys/stat.h>

struct Request {
    std::string method;
    std::string username;
    std::string password;
    std::string path;
    std::string filename;
    std::vector<unsigned char> data;
    std::string error;
    int pieceNumber;
};

struct Response {
    std::string header;
    std::vector<unsigned char> body;
};

std::unordered_map<std::string, std::string> userPasswordMap;
std::atomic<int> thread_count;
std::string directory;


#define MSG_SIZE 2000
#define SIZE_MESSAGE_SIZE 17

// clear socket errors
int getSO_ERROR( int fd );

// gracefully close sockets
void closeSocket( int fd );

//the thread function
void *requestCycle( void * );

// Read request string and populate a Request object
Request parseRequest( std::vector<unsigned char> requestString );

// Read request into string from socket
std::vector<unsigned char> getRequest( int sock );

// Write response string to socket
void sendResponse( unsigned char *request, size_t request_size, int sock );

std::vector<unsigned char> loadFileToVector( std::string filename );

void saveVectorToFile( std::vector<unsigned char> data, std::string filename );

void handleListResponse( Request request, int sock );

Response handleGetResponse( Request request );

void handlePutResponse( Request request );

void handleMkdirResponse( Request request );

void initAuthentication();

int main( int argc, char *argv[] ) {
  setbuf( stdout, NULL );
  if ( argc < 3 ) {
    printf( "USAGE: <root_directory> <server_port>\n" );
    exit( 1 );
  }
  directory = argv[ 1 ];
  thread_count = 0;
  initAuthentication();

  // Socket values
  int socket_desc, client_sock, c, *new_sock;
  struct sockaddr_in server, client;

  //Create socket
  socket_desc = socket( AF_INET, SOCK_STREAM, 0 );
  if ( socket_desc == -1 ) {
    printf( "Could not create socket" );
  }

  // Allow server to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( socket_desc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    perror( "setsockopt(SO_REUSEADDR) failed" );
  }

  //Prepare the sockaddr_in structure
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( ( uint16_t ) std::stoi( argv[ 2 ] ) );

  //Bind
  if ( bind( socket_desc, ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) {
    //print the error message
    perror( "bind failed. Error" );
    return 1;
  }

  //Listen
  listen( socket_desc, 3 );

  //Accept and incoming connection
  puts( "Waiting for incoming connections..." );
  c = sizeof( struct sockaddr_in );

  client_sock = accept( socket_desc, ( struct sockaddr * ) &client, ( socklen_t * ) &c );
  while ( client_sock ) {
    if ( client_sock < 0 ) {
      perror( "accept failed" );
      continue;
    }

    pthread_t sniffer_thread;
    new_sock = ( int * ) malloc( 4 );
    *new_sock = client_sock;

    if ( pthread_create( &sniffer_thread, NULL, requestCycle, ( void * ) new_sock ) < 0 ) {
      perror( "could not create thread" );
      return 1;
    }
    client_sock = accept( socket_desc, ( struct sockaddr * ) &client, ( socklen_t * ) &c );
  }

  closeSocket( socket_desc );
  while ( thread_count > 0 ) {
    printf( "waiting for threads to close ...\n" );
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
  }
  return 0;
}

//the thread function
void *requestCycle( void *socket_desc ) {
  thread_count++;

  //Get the socket descriptor
  int sock = *( int * ) socket_desc;

  Request request;
  printf( "Server (%s) received a request.\n", directory.c_str() );
  Response response = Response();

  request = parseRequest( getRequest( sock ) );

  if ( request.error.compare( "" ) == 0 ) {
    printf( "Server(%s) did not error and is handling method(%s)\n", directory.c_str(), request.method.c_str() );
    if ( request.method.compare( "LIST" ) == 0 ) {
      handleListResponse( request, sock );
    }
    else if ( request.method.compare( "GET" ) == 0 ) {
//      handleGetResponse( request, sock );
    }
    else if ( request.method.compare( "PUT" ) == 0 ) {
      handlePutResponse( request );
    }
    else if ( request.method.compare( "MKDIR" ) == 0 ) {
      handleMkdirResponse( request );
    }
    printf( "Server(%s) finished handling method(%s)\n", directory.c_str(), request.method.c_str() );

  }
  else {
    printf( "Server(%s) had an error(%s)", directory.c_str(), request.error.c_str() );
    request.error.insert( 0, "ERROR " );
    response.header = request.error;
  }

//  sendResponse( sock, response );
  closeSocket( sock );
  free( socket_desc );

  thread_count--;
  return 0;
}


/**
 *  Send the char* request to a remote server via socket
 *
 * @param request
 * @param request_size
 * @param sock - File descriptor pointing to a connected socket
 */
void sendResponse( unsigned char *request, size_t request_size, int sock ) {
  std::string sizeString = std::to_string( request_size );
  while ( sizeString.length() < 16 ) {
    sizeString = " " + sizeString;
  }
  send( sock, sizeString.c_str(), 16, 0 );

  unsigned char *send_buffer = request;

  while ( request_size > 0 ) {
    ssize_t send_size = send( sock, send_buffer, request_size, 0 );
    printf( "Client sent size(%ld), send_buffer(%*s)\n", send_size, ( int ) send_size, send_buffer );
    printf( "In hex:\n" );
    for ( int i = 0; i < request_size; i++ ) {
      printf( "%02X", send_buffer[ i ] );
    }
    printf( "\n" );

    if ( send_size < 0 ) {
      perror( "Error sending get body" );
    }
    send_buffer += send_size;
    request_size -= send_size;
  }
}


std::vector<unsigned char> getRequest( int sock ) {
  char sizeMessage[SIZE_MESSAGE_SIZE];
  bzero( sizeMessage, SIZE_MESSAGE_SIZE );

  recv( sock, sizeMessage, SIZE_MESSAGE_SIZE - 1, 0 );
  ssize_t messageSize = std::stoi( std::string( sizeMessage ) );
  printf( "Client getting message of len(%s)(%ld)", sizeMessage, messageSize );

  unsigned char clientMessage[MSG_SIZE];
  bzero( clientMessage, MSG_SIZE );

  ssize_t readSize;
  std::vector<unsigned char> request = std::vector<unsigned char>();

  while ( messageSize > 0 ) {
    bzero( clientMessage, MSG_SIZE );
    readSize = recv( sock, clientMessage, MSG_SIZE - 1, 0 );
    if ( readSize == -1 ) {
      char err[6] = "ERROR";
      printf( "Server (%s) recv error", directory.c_str() );
      request.insert( request.begin(), err, err + 5 );
      perror( "recv failed" );
      return request;
    }

    printf( "Server(%s) Read buffer from sock, size(%ld):\n", directory.c_str(), readSize );
    for ( int i = 0; i < readSize; i++ ) {
      printf( "%02X", clientMessage[ i ] );
    }
    printf( "\n" );

    request.insert( request.end(), clientMessage, clientMessage + readSize );
    messageSize -= readSize;
  }

  printf( "Server(%s) read vector.data: %s\n", directory.c_str(), request.data() );
  return request;
}

std::vector<unsigned char> loadFileToVector( std::string filename ) {
  std::ifstream ifs( filename );
  if ( !ifs ) {
    perror( "Could not open file:" );
    return std::vector<unsigned char>();
  }
  else {
    return std::vector<unsigned char>( std::istreambuf_iterator<char>( ifs ), std::istreambuf_iterator<char>() );
  }
}

void saveVectorToFile( std::vector<unsigned char> data, std::string filename ) {
  printf( "Server (%s) writing data(%s) to file(%s)\n", directory.c_str(), data.data(), filename.c_str() );
  std::ofstream ofs( filename, std::ios::out | std::ofstream::binary );
  std::copy( data.begin(), data.end(), std::ostreambuf_iterator<char>( ofs ) );
  ofs.close();
}

Request parseRequest( std::vector<unsigned char> requestData ) {
  std::string requestString;
  int requestEndIndex = 0;
  while ( requestEndIndex < requestData.size() && requestData[ requestEndIndex ] != '\n' ) {
    requestString.push_back( requestData[ requestEndIndex ] );
    requestEndIndex++;
  }

  Request request = Request();
  request.method = "";
  request.username = "";
  request.password = "";
  request.path = "";
  request.filename = "";
  request.error = "";
  request.pieceNumber = -1;


  uint64_t start = 0;
  uint64_t end = 0;

  printf( "%s Parsing request: %s\n", directory.c_str(), requestString.c_str() );
  end = requestString.find( " " );
  // Validate token
  if ( end == -1 ) {
    request.error = "EMPTY_REQUEST";
    return request;
  }

  request.method = requestString.substr( start, end );
  printf( "Server %s parsing request. Found method=:%s:\n", directory.c_str(), request.method.c_str() );

  // Validate method
  if ( !( request.method.compare( "GET" ) == 0
          || request.method.compare( "LIST" ) == 0
          || request.method.compare( "PUT" ) == 0
          || request.method.compare( "MKDIR" ) == 0 ) ) {

    request.error = "UNSUPPORTED_METHOD";
    return request;
  }

  start = end + 1;
  end = requestString.find( " ", start );
  uint64_t delim = requestString.find( ":", start );
  printf( "(%s) looking in %ld:%ld for un:pw\n", directory.c_str(), start, end );
  // validate token
  if ( delim == -1 || delim >= end || end <= start ) {
    request.error = "BAD_AUTHORIZATION_TOKEN";
    return request;
  }
  request.username = requestString.substr( start, delim - start );
  request.password = requestString.substr( delim + 1, end - delim - 1 );
  printf( "Server %s parsing request. Found username=:%s:\n", directory.c_str(), request.username.c_str() );
  printf( "Server %s parsing request. Found password=:%s:\n", directory.c_str(), request.password.c_str() );

  // Authorize user
  if ( ( request.username.length() <= 0 || request.password.length() <= 0 )
       || ( userPasswordMap.find( request.username ) == userPasswordMap.end() )
       || ( userPasswordMap[ request.username ].compare( request.password ) != 0 ) ) {
    request.error = "BAD_AUTHORIZATION_TOKEN";
    return request;
  }

  start = end + 1;
  end = requestString.find( " ", start );

  // validate path token or return list with empty path
  if ( ( end <= start ) ) {
    request.error = "BAD_PATH_TOKEN";
    return request;
  }
  printf( "Looking for path in (%ld,%ld)\n", end, start );
  request.path = requestString.substr( start, end - start );
  if ( request.path[ request.path.length() - 1 ] == '\n' ) {
    // Strip any tailing newline
    request.path = request.path.substr( 0, request.path.length() - 1 );
  }
  printf( "Server %s parsing request. Found path=:%s:\n", directory.c_str(), request.path.c_str() );

  // User should not be able to view parent directory!
  if ( request.path.find( ".." ) != std::string::npos ) {
    request.error = "BAD_PATH_TOKEN";
    return request;
  }

  // We have enough info for list, stop processing
  if ( request.method.compare( "LIST" ) == 0 || request.method.compare( "MKDIR" ) == 0 ) {
    return request;
  }

  start = end + 1;
  end = requestString.find( " ", start );
  // validate path token or return list with empty path
  if ( end <= start ) {
    request.error = "BAD_FILENAME_TOKEN";
    return request;
  }
  request.filename = requestString.substr( start, end - start );

  printf( "Server %s parsing request. Found filename=:%s:\n", directory.c_str(), request.filename.c_str() );

  // User should not be able to view parent directory!
  if ( request.filename.find( ".." ) != std::string::npos ) {
    request.error = "BAD_FILENAME_TOKEN";
    return request;
  }

  start = end + 1;
  if ( start >= requestString.length() ) {
    request.error = "MISSING_PIECE_TOKEN";
    return request;
  }

  request.pieceNumber = std::stoi( requestString.substr( start, 1 ) );
  printf( "Server %s parsing request. Found piece#=:%d:\n", directory.c_str(), request.pieceNumber );
  if ( request.pieceNumber < 0 || request.pieceNumber > 3 ) {
    request.error = "BAD_PIECE_TOKEN";
    return request;
  }

  if ( request.method.compare( "PUT" ) == 0 ) {
    for ( int i = 0; i < requestData.size() - requestEndIndex - 1; i++ ) {
      request.data.push_back( requestData[ i + requestEndIndex + 1 ] );
    }
  }

  return request;
}


void handleListResponse( Request request, int sock ) {

  // ls the directory using system calls
  std::string command = "/bin/ls -A .";
  command.append( directory );
  command.append( request.path );
  FILE *fp = popen( command.c_str(), "r" );
  if ( fp == NULL ) {
    sendResponse( ( unsigned char * ) "ERROR", 6, sock );
    return;
  }

  // Store the output of the ls command in a response
  std::string output = "";
  char buf[MSG_SIZE];
  printf( "Server(%s) ls buffer(", directory.c_str() );
  while ( fgets( buf, sizeof( buf ) - 1, fp ) != NULL ) {
    printf( "%s", buf );
    output.append( buf );
  }
  sendResponse( ( unsigned char * ) output.c_str(), output.length(), sock );

  /* close file */
  pclose( fp );
}

Response handleGetResponse( Request request ) {
  // TODO: dis shit so broke
  Response response;
  std::string localFilename =
    directory + "/." + request.path + "/" + request.filename
    + "." + std::to_string( request.pieceNumber );
  response.body = loadFileToVector( localFilename );
  if ( response.body.size() > 0 ) {
    response.header = "SUCCESS " + localFilename + " " + std::to_string( response.body.size() );
  }
  else {
    response.header = "ERROR: FILE NOT FOUND";
  }
  return response;
}

void handlePutResponse( Request request ) {
  printf( "Server (%s) handling put", directory.c_str() );
  std::string localFilename =
    "." + directory + request.path + "." + request.filename
    + "." + std::to_string( request.pieceNumber );
  saveVectorToFile( request.data, localFilename );
}

void handleMkdirResponse( Request request ) {
  std::string fullPath = "." + directory + "/" + request.path;
  mkdir( fullPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
}

void initAuthentication() {
  std::ifstream ifs( "dfs.conf" );
  std::string line;

  while ( std::getline( ifs, line ) ) {
    // TODO: remove this line if not neeted
    // std::istringstream iss( line );
    try {
      if ( line.find( "#" ) == 0 || line.compare( "" ) == 0 ) {
        // Don't try to parse comments
        continue;
      }
      else if ( line.find( " " ) >= 0 ) {
        // Username Password is the only supported directive
        userPasswordMap[ line.substr( 0, line.find( " " ) ) ] =
          line.substr( line.find( " " ) + 1, line.length() - line.find( " " ) - 1 );
        printf( "Added un:pw: (%s:%s)\n",
                line.substr( 0, line.find( " " ) ).c_str(),
                userPasswordMap[ line.substr( 0, line.find( " " ) ) ].c_str() );
      }
      else {
        // Add support for additional directives here
      }
    }
    catch ( int e ) {
      // If configuration parse fails then exit immediately
      printf( "Error occurred reading line: %s in dfs.conf, error message: %d\n", line.c_str(), e );
      exit( 1 );
    }
  }

  return;
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
