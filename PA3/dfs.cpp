/**
  * CSCI5273 Network Systems
  * Programming Assignment 3 - distributed file system
  * Matt Pennington - mape5853
  *
  * Provides Distributed File Server
  **/


#include <netinet/in.h>
#include <unistd.h>
//#include <signal.h>
#include <memory.h>
#include <unordered_map> // Used for Request.headers and Config.filtypes
#include <fstream> // Used to retrieve files from the fs
#include <sstream> // Used to easily read/write small text buffers
#include <atomic> // Needed to maintain count of threads
#include <thread> // Needed for sleep to wait for threads to close after ctrl+c

struct Request {
    std::string method;
    std::string username;
    std::string password;
    char rank;
    std::string path;
    std::string filename;
    std::string data;
    std::string error;
    int pieceNumber;
};

std::unordered_map< std::string, std::string > userPasswordMap;
std::atomic< int > thread_count;
std::string directory;
#define MSG_SIZE 2000

// clear socket errors
int getSO_ERROR( int fd );

// gracefully close sockets
void closeSocket( int fd );

//the thread function
void *requestCycle( void * );

// Read request into string from socket
std::string getRequest( int sock );

// Read request string and populate a Request object
Request parseRequest( std::string requestString );

// Write response string to socket
void sendResponse( int sock, std::string response );

std::string handleListResponse( Request request );

std::string handleGetResponse( Request request );

std::string handlePutResponse( Request request );

int main( int argc, char *argv[] ) {
  if ( argc < 3 ) {
    printf( "USAGE: <root_directory> <server_port>\n" );
    exit( 1 );
  }
  directory = argv[ 1 ];

  thread_count = 0;

  // Socket values
  int socket_desc, client_sock, c, *new_sock;
  struct sockaddr_in server, client;

  //Create socket
  socket_desc = socket( AF_INET, SOCK_STREAM, 0 );
  if ( socket_desc == -1 ) {
    printf( "Could not create socket" );
  }
  puts( "Socket created" );

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
  puts( "bind done" );

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
  std::string response;

  request = parseRequest( getRequest( sock ) );

  if ( request.error.compare( "" ) == 0 ) {
    if ( request.method.compare( "LIST" ) == 0 ) {
      response = handleListResponse( request );
    }
    else if ( request.method.compare( "GET" ) == 0 ) {
      response = handleGetResponse( request );
    }
    else if ( request.method.compare( "PUT" ) == 0 ) {
      response = handlePutResponse( request );
    }
  }
  else {
    request.error.insert( 0, "ERR:" );
    response = request.error;
  }

  sendResponse( sock, response );
  closeSocket( sock );
  free( socket_desc );

  thread_count--;
  return 0;
}


std::string getRequest( int sock ) {
  char client_message[MSG_SIZE];
  bzero( client_message, MSG_SIZE );
  ssize_t read_size;
  std::string ret = "";

  read_size = recv( sock, client_message, MSG_SIZE, 0 );

  if ( strncmp( client_message, "PUT", 3 ) == 0 ) {
    while ( read_size > 0 ) {
      ret.append( client_message );
      bzero( client_message, MSG_SIZE );
      read_size = recv( sock, client_message, MSG_SIZE, 0 );
    }
  }

  if ( read_size == -1 ) {
    ret = "EXIT";
    perror( "recv failed" );
  }

  return ret;
}

void sendResponse( int sock, std::string response ) {
  ssize_t send_size;
  size_t file_read_size = sizeof( response.c_str() );
  unsigned char *pbuf = ( unsigned char * ) response.c_str();

  while ( file_read_size > 0 ) {
    send_size = send( sock, pbuf, file_read_size, 0 );
    if ( send_size < 0 ) {
      perror( "Error sending get body" );
    }
    pbuf += send_size;
    file_read_size -= send_size;
  }
}

//TODO: implement "MKDIR" request
Request parseRequest( std::string requestString ) {
  Request request;
  uint64_t start = 0;
  uint64_t end = 0;

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
          || request.method.compare( "PUT" ) == 0 ) ) {

    request.error = "UNSUPPORTED_METHOD";
    return request;
  }

  start = end + 1;
  end = requestString.find( " ", start );
  // validate token
  if ( ( requestString.find( ":", start, end - start ) == -1 )
       || ( end <= start ) ) {
    request.error = "BAD_AUTHORIZATION_TOKEN";
    return request;
  }

  request.username = requestString.substr( start, requestString.find( ":", start ) - start );
  request.password = requestString.substr( requestString.find( ":", start ), end - start );

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
    if ( request.method.compare( "LIST" ) != 0 ) {
      request.error = "BAD_PATH_TOKEN";
    }
    return request;
  }

  request.path = requestString.substr( start, end - start );
  printf( "Server %s parsing request. Found path=:%s:\n", directory.c_str(), request.path.c_str() );

  start = end + 1;
  end = requestString.find( " ", start );
  // validate path token or return list with empty path
  if ( ( end <= start ) ) {
    if ( request.method.compare( "LIST" ) != 0 ) {
      request.error = "BAD_FILENAME_TOKEN";
    }
    return request;
  }

  request.filename = requestString.substr( start, end - start );
  printf( "Server %s parsing request. Found filename=:%s:\n", directory.c_str(), request.filename.c_str() );

  // We have enough info for list, stop processing
  if ( request.method.compare( "LIST" ) == 0 ) {
    return request;
  }

  start = end + 1;
  // Validate rank token
  if ( !( requestString[ start ] == 'p' || requestString[ start ] == 's' ) ) {
    request.error = "BAD_RANK_TOKEN";
    return request;
  }
  request.rank = requestString[ start ] == 'p';
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
  if ( request.pieceNumber < 1 || request.pieceNumber > 4 ) {
    request.error = "BAD_PIECE_TOKEN";
    return request;
  }

  start += 2;
  // Validate data token
  if ( start >= requestString.length() ) {
    request.error = "EMPTY_PUT_REQUEST";
    return request;
  }
  request.data = requestString.substr( start, requestString.length() - start );
  printf( "Server %s parsing request. Found data=:%s:\n", directory.c_str(), request.data.c_str() );
  return request;
}


std::string handleListResponse( Request request ) {
  // TODO: get list of files in request.path and return them
  return request.method;

}

std::string handleGetResponse( Request request ) {
  // TODO: parse filename and directory from request.path. Look for /directory/.filename.N.R, where N is any number 1-4 and R = request.rank
  return request.method;
}

std::string handlePutResponse( Request request ) {
  // TODO:  parse filename and directory from request.path. replace/create a file called /directory/.request.path.N.R with request.data where N = request.pieceNumber and R = request.rank
  return request.method;
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