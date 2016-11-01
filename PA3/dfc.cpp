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
#include <memory.h>
#include <fstream>
#include <sstream>
#include <vector>

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

void handleList( std::string command );

void handleGet( std::string command );

void handlePut( std::string command );

void encryptFile( std::string filename );

void decryptFile( std::string filename )

std::vector<char> loadFileToVector( std::string filename );

void saveVectorToFile( std::vector<char> data, std::string filename );

int getBinForFile( std::string filename );

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
      handleList( command );
    }
    else if ( command.find( "GET" ) == 0 ) {
      handleGet( command );
    }
    else if ( command.find( "PUT" ) == 0 ) {
      handlePut( command );
    }
      // TODO: add MKDIR
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
    // TODO: remove this line if not neeted
    //    std::istringstream iss( line );
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
    char err[MSG_SIZE];
    sprintf( err, "Sock not created for %d. Error", i );
    perror( err );
  }
  puts( "Socket created" );

  // Add a 1 second timeout
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  if ( setsockopt( newSock, SOL_SOCKET, SO_RCVTIMEO, ( char * ) &timeout, sizeof( timeout ) ) < 0 ) {
    char err[MSG_SIZE];
    sprintf( err, "setsockopt(SO_RCVTIMEO) failed for server(%d). Error", i );
    perror( err );
  }
  if ( setsockopt( newSock, SOL_SOCKET, SO_SNDTIMEO, ( char * ) &timeout, sizeof( timeout ) ) < 0 ) {
    char err[MSG_SIZE];
    sprintf( err, "setsockopt(SO_SNDTIMEO) failed for server(%d). Error", i );
    perror( err );
  }

  // Allow client to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( newSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    char err[MSG_SIZE];
    sprintf( err, "setsockopt(SO_REUSEADDR) failed for server(%d). Error", i );
    perror( err );
  }

  server.sin_addr.s_addr = inet_addr( config.servers[ i ].ipAddr.c_str() );
  server.sin_family = AF_INET;
  server.sin_port = htons( ( uint16_t ) config.servers[ i ].port );

  if ( connect( newSock, ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) {
    char err[MSG_SIZE];
    sprintf( err, "Connect failed for %d. Error", i );
    perror( err );
  }

  return newSock;
}

void sendRequest( std::string request, int sock ) {
  ssize_t send_size;
  size_t file_read_size = request.length();
  unsigned char *pbuf = ( unsigned char * ) request.c_str();
  printf( "Client sending (%s), sizeof(%ld), strlen(%ld), length(%ld)\n",
          request.c_str(),
          file_read_size,
          strlen( request.c_str() ),
          request.length() );
  while ( file_read_size > 0 ) {
    send_size = send( sock, pbuf, file_read_size, 0 );
    printf( "Client sent size(%ld), pbuf(%*s)\n", send_size, ( int ) send_size, pbuf );
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

void handleList( std::string command ) {
  printf( "handling list\n" );

  int sock;
  std::string request;
  std::string pieces[4];
//  std::string response;

  request = "LIST ";
  request.append( config.username );
  request.append( ":" );
  request.append( config.password );
  request.append( command.substr( command.find( " " ), command.length() - command.find( " " ) ) );

  printf( "LIST SENT: %s\n", request.c_str() );
  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      sendRequest( request, sock );
      pieces[ i ] = getResponse( sock );
      printf( "LIST RECVD: %s\n", pieces[ i ].c_str() );
    }
  }

  // TODO: reconstruct full list with incomplete warnings
  closeSocket( sock );
}

void handleGet( std::string command ) {
  printf( "handling get\n" );

  int sock;
  std::string pieces[4];

  std::string request;
  std::string filename;

  request = "GET ";
  request.append( config.username );
  request.append( ":" );
  request.append( config.password );

  uint64_t fnStart, fnLength, pathStart, pathLength;
  fnStart = command.find( " " ) + 1;
  pathStart = command.rfind( " " ) + 1;
  if ( pathStart != fnStart ) {
    fnLength = pathStart - fnStart - 1;
    pathLength = command.length() - pathStart;
    request.append( " " );
    request.append( command.substr( pathStart, pathLength ) );
    request.append( " " );
    filename = command.substr( fnStart, fnLength );
    request.append( command.substr( fnStart, fnLength ) );
  }
  else {
    filename = command.substr( fnStart, command.length() - command.find( " " ) );
    request.append( filename );
  }

  request.append( " p" );

  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      sendRequest( request, sock );
      pieces[ i ] = getResponse( sock );
    }
    closeSocket( sock );
  }

  for ( int i = 0; i < 4; i++ ) {
    if ( pieces[ i ].find( "ERROR" ) == 0 ) {
      request = request.substr( 0, request.length() - 2 );
      request.append( " s" );
      sock = connectToServer( i );
      if ( sock != -1 ) {
        sendRequest( request, sock );
        pieces[ i ] = getResponse( sock );
      }
      closeSocket( sock );
    }
    if ( pieces[ i ].find( "ERROR" ) == 0 ) {
      printf( "File is incomplete.\n" );
      return;
    }
  }

  for ( int i = 0; i < 4; i++ ) {
    // TODO:  Reconstruct pieces
  }

  // TODO: write response to .filename
  decryptFile( filename );
  // TODO: delete .filename
}

void handlePut( std::string command ) {
  printf( "handling put\n" );

  std::string request;
  std::string filename;

  request = "PUT ";
  request.append( config.username );
  request.append( ":" );
  request.append( config.password );

  uint64_t fnStart, fnLength, pathStart, pathLength;
  fnStart = command.find( " " ) + 1;
  pathStart = command.rfind( " " ) + 1;
  if ( pathStart != fnStart ) {
    fnLength = pathStart - fnStart - 1;
    pathLength = command.length() - pathStart;
    request.append( " " );
    request.append( command.substr( pathStart, pathLength ) );
    request.append( " " );
    filename = command.substr( fnStart, fnLength );
    request.append( command.substr( fnStart, fnLength ) );
  }
  else {
    filename = command.substr( fnStart, command.length() - command.find( " " ) );
    request.append( filename );
  }

  request.append( " p" );

  int bin = getBinForFile( filename );
  encryptFile( filename );
  //TODO: read .filename into memory and break the data into 4 pieces

  int sock;
  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      //TODO: connect to servers and send pieces
    }
    closeSocket( sock );
  }

  // TODO: delete .filename
}

//TODO:: add handleMkdir()

std::vector<char> loadFileToVector( std::string filename ) {
  std::ifstream ifs( filename );
  if ( !ifs ) {
    perror( "Could not open file:" );
  }

  return std::vector<char>( std::istreambuf_iterator<char>( ifs ), std::istreambuf_iterator<char>() );
}

void saveVectorToFile( std::vector<char> data, std::string filename ) {
  //Save data to filename
}

int getBinForFile( std::string filename ) {
  // call the getbin python script
  std::string command = "python getbin.py ";
  command.append( filename );
  FILE *fp = popen( command.c_str(), "r" );
  if ( fp == NULL ) {
    return -1;
  }

  // get the value from stdout
  char buf[4];
  fgets( buf, sizeof( buf ) - 1, fp );

  // close file
  pclose( fp );
  return std::stoi( buf );
}

void encryptFile( std::string filename ) {
  // encrypt the file in tempfile .filename
  std::string command = "openssl aes-256-cbc -salt -in ";
  command.append( filename );
  command.append( " -out ." );
  command.append( filename );
  command.append( " -pass file:enc.key" );

  // run the command
  FILE *fp = popen( command.c_str(), "r" );
  if ( fp == NULL ) {
    perror( "Problem running encryptFile:" );
  }
  pclose( fp );
}

void decryptFile( std::string filename ) {
  // encrypt the file in tempfile .filename
  std::string command = "openssl aes-256-cbc -d -in ."
  command.append( filename );
  command.append( " -out ";
  command.append( filename );
  command.append( " -pass file:enc.key" );
  FILE *fp = popen( command.c_str(), "r" );
  if ( fp == NULL ) {
    perror( "Problem running decryptFile:" );
  }
  // close file
  pclose( fp );
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