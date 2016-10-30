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
//#include <unordered_map> // Used for Request.headers and Config.filtypes
#include <fstream> // Used to retrieve files from the fs
#include <sstream> // Used to easily read/write small text buffers
#include <atomic> // Needed to maintain count of threads
#include <thread> // Needed for sleep to wait for threads to close after ctrl+c

std::atomic< int > thread_count;
std::string name;
#define MSG_SIZE 2000

// clear socket errors
int getSO_ERROR( int fd );

// gracefully close sockets
void closeSocket( int fd );

//the thread function
void *connection_handler( void * );

// Read request into string from socket
std::string getRequest( int sock );

// Write response string to socket
void sendResponse( int sock, std::string response );

int main( int argc, char *argv[] ) {
  if ( argc < 3 ) {
    printf( "USAGE: <root_directory> <server_port>\n" );
    exit( 1 );
  }
  name = argv[ 1 ];

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

    if ( pthread_create( &sniffer_thread, NULL, connection_handler, ( void * ) new_sock ) < 0 ) {
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


//the thread function
void *connection_handler( void *socket_desc ) {
  thread_count++;

  //Get the socket descriptor
  int sock = *( int * ) socket_desc;


  std::string request;
  std::string response;

  request = getRequest( sock );

  while ( request.find( "EXIT" ) != 0 && requestAuthorized( request ) ) {
    printf( "Client (%s) Received message: %s\n", name.c_str(), request );

    if ( request.find( "LIST" ) == 0 ) {
      response = handleListResponse();
    }
    else if ( request.find( "GET" ) == 0 ) {
      response = handleGetResponse( request );
    }
    else if ( request.find( "PUT" ) == 0 ) {
      response = handlePutResponse( request );
    }

    sendResponse( sock, response );
    request = getRequest( sock )
  }

  closeSocket( sock );
  free( socket_desc );

  thread_count--;
  return 0;
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