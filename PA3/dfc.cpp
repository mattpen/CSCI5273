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

struct Config {
    std::string username;
    std::string password;
    int serverSockets[4];
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
      for ( int i = 1; i < 4; i++ ) {
        close( config.serverSockets[ i ] );
      }
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
  config.serverSockets[ 0 ] = -1;
  config.serverSockets[ 1 ] = -1;
  config.serverSockets[ 2 ] = -1;
  config.serverSockets[ 3 ] = -1;

  while ( std::getline( ifs, line ) ) {
    std::istringstream iss( line );
    try {
      if ( line.find( "#" ) == 0 || line.compare( "" ) == 0 ) {
        // Ignore comments and empty lines
        continue;
      }
      else if ( line.find( "Server" ) == 0 ) {
        std::string name = "";
        std::string address = "";
        std::string port = "";
        // Parse name, address,  and port
        printf( "Searching string for server: %s\n", line.c_str() );
        printf( "Looking for name in between: %ld,%ld\n", line.find( " " ) + 1, line.rfind( " " ) - 7 );
        name = line.substr( line.find( " " ) + 1, line.rfind( " " ) - 7 );
        printf( "Looking for address in between: %ld,%ld\n", line.rfind( " " ) + 1,
                line.rfind( ":" ) - line.rfind( " " ) - 1 );
        address = line.substr( line.rfind( " " ) + 1, line.rfind( ":" ) - line.rfind( " " ) - 1 );
        printf( "Looking for port in between: %ld,%ld\n", line.rfind( ":" ) + 1,
                line.length() - line.rfind( ":" ) - 1 );
        port = line.substr( line.rfind( ":" ) + 1, line.length() - line.rfind( ":" ) - 1 );
        printf( "Found server:%s:%s:%s:\n", name.c_str(), address.c_str(), port.c_str() );

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

        // init socket
        struct sockaddr_in server;

        config.serverSockets[ socketNum ] = socket( AF_INET, SOCK_STREAM, 0 );
        if ( config.serverSockets[ socketNum ] == -1 ) {
          printf( "Could not create socket" );
        }
        puts( "Socket created" );

        // Allow client to reuse port/addr if in TIME_WAIT state
        int enable = 1;
        if ( setsockopt( config.serverSockets[ socketNum ], SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
          perror( "setsockopt(SO_REUSEADDR) failed" );
        }

        server.sin_addr.s_addr = inet_addr( address.c_str() );
        server.sin_family = AF_INET;
        server.sin_port = htons( ( uint16_t ) std::stoi( port ) );

        if ( connect( config.serverSockets[ socketNum ], ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) {
          perror( "connect failed. Error" );
          exit( 1 );
        }
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
       || ( config.serverSockets[ 0 ] == -1 )
       || ( config.serverSockets[ 1 ] == -1 )
       || ( config.serverSockets[ 2 ] == -1 )
       || ( config.serverSockets[ 3 ] == -1 ) ) {
    printf( "Error reading configuration:\n%s\n%s\n", config.username.c_str(), config.password.c_str() );
    exit( 1 );
  }
}

void handleList() {
  printf( "handling list\n" );
  char client_message[MSG_SIZE];

  for ( int i = 0; i < 4; i++ ) {
    write( config.serverSockets[ i ], "LIST", 4 );
    bzero( client_message, MSG_SIZE );
    read( config.serverSockets[ i ], client_message, MSG_SIZE );
    printf( "%s\n", client_message );
  }
}

void handleGet( std::string filename ) {
  printf( "handling get\n" );
}

void handlePut( std::string filename ) {
  printf( "handling put\n" );
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