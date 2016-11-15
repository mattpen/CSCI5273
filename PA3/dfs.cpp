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
    char rank;
    std::string path;
    std::string filename;
    std::vector<unsigned char> data;
    std::string error;
    int pieceNumber;
};

struct Response {
    std::string header;
    std::vector<char> body;
};

std::unordered_map<std::string, std::string> userPasswordMap;
std::atomic<int> thread_count;
std::string directory;


#define MSG_SIZE 2000

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
void sendResponse( int sock, Response response );

std::vector<char> loadFileToVector( std::string filename );

void saveVectorToFile( std::vector<char> data, std::string filename );

Response handleListResponse( Request request );

Response handleGetResponse( Request request );

Response handlePutResponse( Request request );

Response handleMkdirResponse( Request request );

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

//  // Add a 1 second timeout
//  struct timeval timeout;
//  timeout.tv_sec = 1;
//  timeout.tv_usec = 0;
//  if ( setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, ( char * ) &timeout, sizeof( timeout ) ) < 0 ) {
//    char err[MSG_SIZE];
//    sprintf( err, "setsockopt(SO_RCVTIMEO) failed for server(%s). Error", directory.c_str() );
//    perror( err );
//  }

  Request request;
  printf( "Server (%s) received a request.\n", directory.c_str() );
  Response response = Response();

  request = parseRequest( getRequest( sock ) );

  if ( request.error.compare( "" ) == 0 ) {
    printf( "Server(%s) did not error and is handling method(%s)\n", directory.c_str(), request.method.c_str() );
    if ( request.method.compare( "LIST" ) == 0 ) {
      response = handleListResponse( request );
    }
    else if ( request.method.compare( "GET" ) == 0 ) {
      response = handleGetResponse( request );
    }
    else if ( request.method.compare( "PUT" ) == 0 ) {
      response = handlePutResponse( request );
    }
    else if ( request.method.compare( "MKDIR" ) == 0 ) {
      response = handleMkdirResponse( request );
    }
    printf( "Server(%s) finished handling method(%s)\n", directory.c_str(), request.method.c_str() );

  }
  else {
    printf( "Server(%s) had an error(%s)", directory.c_str(), request.error.c_str() );
    request.error.insert( 0, "ERROR " );
    response.header = request.error;
  }

  sendResponse( sock, response );
  closeSocket( sock );
  free( socket_desc );

  thread_count--;
  return 0;
}

Response handleMkdirResponse( Request request ) {
  Response response = Response();
  std::string fullPath = "." + directory + "/" + request.path;
  mkdir( fullPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );

  return response;
}


std::vector<unsigned char> getRequest( int sock ) {
  unsigned char clientMessage[MSG_SIZE];
  bzero( clientMessage, MSG_SIZE );
  ssize_t read_size;
  std::vector<unsigned char> request = std::vector<unsigned char>();

  read_size = recv( sock, clientMessage, MSG_SIZE, 0 );

  if ( read_size == -1 ) {
    char err[6] = "ERROR";
    printf( "Server (%s) recv error", directory.c_str() );
    request.insert( request.begin(), err, err + 5 );
    perror( "recv failed" );
    return request;
  }

  while ( read_size > 0 ) {
    printf( "Server(%s) Read buffer from sock(%d):\n", directory.c_str(), sock );
    for ( int i = 0; i < read_size; i++ ) {
      printf( "%02X", clientMessage[ i ] );
    }
    printf( "\n" );

    request.insert( request.end(), clientMessage, clientMessage + read_size );
    printf( "Server(%s) read_size = %ld\n", directory.c_str(), read_size );

    bzero( clientMessage, MSG_SIZE );
    read_size = recv( sock, clientMessage, MSG_SIZE, 0 );
  }

  printf( "Server(%s) read vector.data: %s\n", directory.c_str(), request.data() );
  return request;
}

void sendResponse( int sock, Response response ) {
  // Send header
  send( sock, response.header.c_str(), response.header.length(), 0 );

  if ( response.header.find( "ERROR" ) != 0 ) {
    char buf[4];
    recv( sock, buf, 4, 0 );

    if ( strcmp( buf, "ACK" ) == 0 ) {
      // Send if we did not send an error and we recvd an ACK
      size_t file_read_size = response.body.size();
      if ( file_read_size > 0 ) {
        unsigned char *pbuf = ( unsigned char * ) response.body.data();

        ssize_t send_size;
        while ( file_read_size > 0 ) {
          send_size = send( sock, pbuf, file_read_size, 0 );
          if ( send_size < 0 ) {
            perror( "Error sending get body" );
          }
          pbuf += send_size;
          file_read_size -= send_size;
        }
      }
    }
  }
}

std::vector<char> loadFileToVector( std::string filename ) {
  std::ifstream ifs( filename );
  if ( !ifs ) {
    perror( "Could not open file:" );
    return *( new std::vector<char>() );
  }
  else {
    return std::vector<char>( std::istreambuf_iterator<char>( ifs ), std::istreambuf_iterator<char>() );
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
  request.rank = '\0';
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
  // Validate rank token
  if ( !( requestString[ start ] == 'p' || requestString[ start ] == 's' ) ) {
    request.error = "BAD_RANK_TOKEN";
    return request;
  }
  request.rank = requestString[ start ];
  printf( "Server %s parsing request. Found rank=:%c:\n", directory.c_str(), request.rank );


  if ( request.method.compare( "GET" ) == 0 ) {
    return request;
  }

  start += 2;
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


Response handleListResponse( Request request ) {
  Response response;
  // ls the directory using system calls
  std::string command = "/bin/ls .";
  command.append( directory );
  command.append( request.path );
  FILE *fp = popen( command.c_str(), "r" );
  if ( fp == NULL ) {
    response.header = "ERROR";
    response.body = *( new std::vector<char>() );
    return response;
  }

  // Store the output of the ls command in a response
  std::string output = "";
  char buf[MSG_SIZE];
  while ( fgets( buf, sizeof( buf ) - 1, fp ) != NULL ) {
    output.append( buf );
  }
  response.body = *( new std::vector<char>( output.begin(), output.end() ) );
  response.header = "SUCCESS";

  /* close file */
  pclose( fp );
  return response;
}

Response handleGetResponse( Request request ) {
  Response response;
  std::string localFilename =
    directory + "/." + request.path + "/" + request.filename
    + "." + std::to_string( request.pieceNumber ) + "." + request.rank;
  response.body = loadFileToVector( localFilename );
  if ( response.body.size() > 0 ) {
    response.header = "SUCCESS " + localFilename + " " + std::to_string( response.body.size() );
  }
  else {
    response.header = "ERROR: FILE NOT FOUND";
  }
  return response;
}

Response handlePutResponse( Request request ) {
  printf( "Server (%s) handling put", directory.c_str() );
  Response response;
  std::string localFilename =
    "." + directory + request.path + "." + request.filename
    + "." + std::to_string( request.pieceNumber ) + "." + request.rank;
  saveVectorToFile( request.data, localFilename );
  response.header = "SUCCESS";
  return response;
}

// TODO: handleMkdirResponse( Request request )

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