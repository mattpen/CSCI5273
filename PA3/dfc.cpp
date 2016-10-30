/**
  * CSCI5273 Network Systems
  * Programming Assignment 3 - distributed file system
  * Matt Pennington - mape5853
  *
  * Provides Distributed File Client
  **/

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
//#include <signal.h>
#include <memory.h>
//#include <unordered_map>
#include <fstream>
#include <sstream>
//#include <atomic>
//#include <thread>

#define MSG_SIZE 2000

struct Server {
    std::string name;
    std::string ipAddr;
    int32_t port;
};

struct Config {
    std::string username;
    std::string password;
    struct Server servers[4];
} config;

// Get the configuration file and initialize the global config parameter
void initConfig();

void handleList();

void handleGet( std::string filename );

void handlePut( std::string filename );

// clear socket errors
int getSO_ERROR( int fd );

// gracefully close sockets
void closeSocket( int fd );

int main( int argc, char *argv[] ) {
  initConfig();

  char cmd[256];
  std::string command;

  while ( fgets( cmd, sizeof( cmd ), stdin ) ) {
    command.assign( cmd );

    if ( command.find( "LIST" ) == 0 ) {
      handleList();
    }
    else if ( command.find( "GET" ) == 0 ) {
      handleGet( command.substr( 4, command.length() - 4 ) );
    }
    else if ( command.find( "PUT" ) == 0 ) {
      handlePut( command.substr( 4, command.length() - 4 ) );
    }
    else if ( command.find( "EXIT" ) == 0 ) {
      return 0;
    }
  }
}

void initConfig() {
  std::ifstream ifs( "dfc.conf" );
  std::string line;

  // Initialize empty config object
  config.username = "";
  config.password = "";
  for ( int i = 0; i < 4; i++ ) {
    config.servers[ 0 ].name = "";
    config.servers[ 0 ].ipAddr = "";
    config.servers[ 0 ].port = -1;
  }

  while ( std::getline( ifs, line ) ) {
    std::istringstream iss( line );
    try {
      if ( line.find( "#" ) == 0 || line.compare( "" ) == 0 ) {
        // Ignore comments and empty lines
        continue;
      }
      else if ( line.find( "Server" ) == 0 ) {
        std::string name = "";
        std::string ipAddr = "";
        std::string port = "";

        // Parse name, address,  and port
        name = line.substr( line.find( " " ) + 1, line.rfind( " " ) - 7 );
        ipAddr = line.substr( line.rfind( " " ) + 1, line.rfind( ":" ) - line.rfind( " " ) - 1 );
        port = line.substr( line.rfind( ":" ) + 1, line.length() - line.rfind( ":" ) - 1 );

        // Get array index for server
        int socketNum = -1;
        if ( name.compare( "DFS1" ) == 0 ) {
          socketNum = 0;
        }
        else if ( name.compare( "DFS2" ) == 0 ) {
          socketNum = 1;
        }
        else if ( name.compare( "DFS3" ) == 0 ) {
          socketNum = 2;
        }
        else if ( name.compare( "DFS4" ) == 0 ) {
          socketNum = 3;
        }

        config.servers[ socketNum ].name = name;
        config.servers[ socketNum ].ipAddr = ipAddr;
        config.servers[ socketNum ].port = ( uint16_t ) std::stoi( port );

      }
      else if ( line.find( "Username" ) == 0 ) {
        config.username = line.substr( line.rfind( " " ) + 1, line.length() - line.rfind( " " ) - 1 );
      }
      else if ( line.find( "Password" ) == 0 ) {
        config.password = line.substr( line.rfind( " " ) + 1, line.length() - line.rfind( " " ) - 1 );
      }
      else {
        // Add support for additional directives here
      }
    }
    catch ( int e ) {
      // If configuration parse fails then exit immediately
      printf( "Error occurred reading line: %s in config, error message: %d\n", line.c_str(), e );
      exit( 1 );
    }
  }

  if ( ( config.username.compare( "" ) == 0 )
       || ( config.password.compare( "" ) == 0 )
       || ( config.servers[ 0 ].name.compare( "" ) == 0
            || config.servers[ 0 ].ipAddr.compare( "" ) == 0
            || config.servers[ 0 ].port == -1 )
       || ( config.servers[ 1 ].name.compare( "" ) == 0
            || config.servers[ 1 ].ipAddr.compare( "" ) == 0
            || config.servers[ 1 ].port == -1 )
       || ( config.servers[ 2 ].name.compare( "" ) == 0
            || config.servers[ 2 ].ipAddr.compare( "" ) == 0
            || config.servers[ 2 ].port == -1 )
       || ( config.servers[ 3 ].name.compare( "" ) == 0
            || config.servers[ 3 ].ipAddr.compare( "" ) == 0
            || config.servers[ 3 ].port == -1 ) ) {
    printf( "Error reading configuration.\n" );
    exit( 1 );
  }
}

int connectToServer( int i ) {
  // init socket
  struct sockaddr_in server;
  int newSock;

  newSock = socket( AF_INET, SOCK_STREAM, 0 );
  if ( newSock == -1 ) {
    char e[MSG_SIZE];
    sprintf( e, "Sock not created for %d. Error", i );
    perror( e );
  }
  puts( "Socket created" );

  // Allow client to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( newSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    char e[MSG_SIZE];
    sprintf( e, "setsockopt(SO_REUSEADDR) failedfor %d. Error", i );
    perror( e );
  }

  server.sin_addr.s_addr = inet_addr( config.servers[ i ].ipAddr.c_str() );
  server.sin_family = AF_INET;
  server.sin_port = htons( ( uint16_t ) config.servers[ i ].port );

  if ( connect( newSock, ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) {
    char e[MSG_SIZE];
    sprintf( e, "Connect failed for %d. Error", i );
    perror( e );
  }

  return newSock;
}

void sendRequest( std::string request, int sock ) {
  ssize_t send_size;
  size_t file_read_size = sizeof( request.c_str() );
  unsigned char *pbuf = ( unsigned char * ) request.c_str();

  while ( file_read_size > 0 ) {
    send_size = send( sock, pbuf, file_read_size, 0 );
    if ( send_size < 0 ) {
      perror( "Error sending get body" );
    }
    pbuf += send_size;
    file_read_size -= send_size;
  }
}

std::string getResponse( int sock ) {
  char client_message[MSG_SIZE];
  bzero( client_message, MSG_SIZE );
  ssize_t read_size;
  std::string ret = "";

  read_size = recv( sock, client_message, MSG_SIZE, 0 );

  while ( read_size > 0 ) {
    ret.append( client_message );
    bzero( client_message, MSG_SIZE );
    read_size = recv( sock, client_message, MSG_SIZE, 0 );
  }

  if ( read_size == -1 ) {
    perror( "recv failed" );
  }

  return ret;
}

void handleList() {
  printf( "handling list\n" );
//  char client_message[MSG_SIZE];
//    write( config.serverSockets[ i ], "LIST", 4 );
//    bzero( client_message, MSG_SIZE );
//    read( config.serverSockets[ i ], client_message, MSG_SIZE );
//    printf( "%s\n", client_message );

  int sock;
  std::string response;

  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      sendRequest( "LIST", sock );
      response = getResponse( sock );
      printf( "%s\n", response.c_str() );
    }
  }
  // TODO: reconstruct full list with incomplete warnings
  closeSocket( sock );
}

void handleGet( std::string filename ) {
  printf( "handling get\n" );

  int sock;
  std::string response;

  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      // TODO: request should include un:pw, 'p', directory
      // TODO: resend request with 's' if piece not found,
      sendRequest( filename, sock );
      response = getResponse( sock );
      // TODO: decrypt response
      // TODO: reconstruct responses

      // TODO: error if pieces missing
    }
  }
  // TODO: write response to ./{$filename}

  closeSocket( sock );
}

void handlePut( std::string filename ) {
  printf( "handling put\n" );

  int sock;
  std::string request;

  //TODO: get add filename, un/pw, directory to request
  //TODO: get the data into memory and hash it
  //TODO: break the data into 4 pieces, and append it with a p/s to the request
  //TODO: send the appropriate requests to each server
  //TODO: encrypt response
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