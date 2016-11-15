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
#include <math.h>

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

struct Response {
    std::string header;
    std::vector<char> body;
};

// Get the configuration file and initialize the global config parameter
void initConfig();

void handleList( std::string command );

void handleGet( std::string command );

void handlePut( std::string command );

void encryptFile( std::string filename );

void decryptFile( std::string filename );

//std::vector<char> loadFileToVector( std::string filename );
//std::string loadFileToString( std::string filename );

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
    bzero( cmd, 256 );
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

/**
 *  Send the char* request to a remote server via socket
 *
 * @param request
 * @param request_size
 * @param sock - File descriptor pointing to a connected socket
 */
void sendRequest( unsigned char *request, size_t request_size, int sock ) {
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

Response getResponse( int sock ) {
  char client_message[MSG_SIZE];
  bzero( client_message, MSG_SIZE );
  ssize_t read_size;
  Response response = Response();

  read_size = recv( sock, client_message, MSG_SIZE, 0 );
  if ( read_size == -1 ) {
    perror( "recv failed" );
  }

  response.header = client_message;
  printf( "Client got header: %s\n", response.header.c_str() );

  if ( response.header.find( "ERROR" ) != 0 ) {
    send( sock, "ACK", 4, 0 );
    read_size = recv( sock, client_message, MSG_SIZE, 0 );
    while ( read_size > 0 ) {
      response.body.insert( response.body.end(), client_message, client_message + read_size );
      bzero( client_message, MSG_SIZE );
      read_size = recv( sock, client_message, MSG_SIZE, 0 );
    }

    if ( read_size == -1 ) {
      perror( "recv failed" );
    }
  }

  return response;
}

void handleList( std::string command ) {
  printf( "handling list\n" );

  int sock = -1;
  std::string request;
  Response responses[4];

  request = "LIST " + config.username + ":" + config.password +
            command.substr( command.find( " " ), command.length() - command.find( " " ) - 1 );

  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      printf( "LIST SENT: %s\n", request.c_str() );
      sendRequest( ( unsigned char * ) request.c_str(), request.length(), sock );
      responses[ i ] = getResponse( sock );
      printf( "LIST RECVD: %s\n", responses[ i ].body.data() );
    }
  }
  // TODO: reconstruct full list with incomplete warnings
  closeSocket( sock );
}

void handleGet( std::string command ) {
  printf( "handling get\n" );

  int sock;
  Response responses[4];

  std::string request;
  std::string filename;

  request = "GET " + config.username + ":" + config.password;

  uint64_t fnStart, fnLength, pathStart, pathLength;
  fnStart = command.find( " " ) + 1;
  pathStart = command.rfind( " " ) + 1;
  if ( pathStart != fnStart ) {
    fnLength = pathStart - fnStart - 1;
    pathLength = command.length() - pathStart;
    request += " " + command.substr( pathStart, pathLength ) + " ";
    filename = command.substr( fnStart, fnLength );
    request += filename;
  }
  else {
    filename = command.substr( fnStart, command.length() - command.find( " " ) );
    request += filename;
  }

  request += " p";

  // Request primary piece
  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
      sendRequest( ( unsigned char * ) request.c_str(), request.length(), sock );
      responses[ i ] = getResponse( sock );
    }
    closeSocket( sock );
  }

  // Request secondary piece for any missing pieces.
  // Print error and bail if secondary piece is not found.
  request = request.substr( 0, request.length() - 2 ) + " s";
  for ( int i = 0; i < 4; i++ ) {
    if ( responses[ i ].header.find( "ERROR" ) == 0 ) {
      for ( int j = 0; j < 4; j++ ) {
        if ( j != i ) {
          sock = connectToServer( j );
          if ( sock != -1 ) {
            sendRequest( ( unsigned char * ) request.c_str(), request.length(), sock );
            responses[ i ] = getResponse( sock );
          }
          closeSocket( sock );
        }

        // If we found the secondary piece, stop looking
        if ( responses[ i ].header.find( "ERROR" ) != 0 ) {
          j = 5;
        }
      }
    }

    if ( responses[ i ].header.find( "ERROR" ) == 0 ) {
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
  std::string filename;
  std::string path;

  // Write authentication parameter to the request
  std::string request = "PUT " + config.username + ":" + config.password;

  // Extract the filename and path from the command
  uint64_t filenameStartIndex, filenameLength, pathStartIndex, pathLength;

  // Look for starting indices
  // Filename is the first parameter, look for the first whitespace
  filenameStartIndex = command.find( " " ) + 1;
  // Path is the second parameter, we look for the last whitespace
  pathStartIndex = command.rfind( " " ) + 1;

  // We found a path (optional)
  if ( pathStartIndex != filenameStartIndex ) {
    filenameLength = pathStartIndex - filenameStartIndex - 1;
    pathLength = command.length() - pathStartIndex;
    path = command.substr( pathStartIndex, pathLength ) + " ";
    filename = command.substr( filenameStartIndex, filenameLength );
  }
    // We did not find a path
  else {
    path = "/";
    filename = command.substr( filenameStartIndex, command.length() - command.find( " " ) );
  }

  // If the last character of the filename is a newline, remove it.
  if ( filename[ filename.length() - 1 ] == '\n' ) {
    filename.pop_back();
  }

  // Add path and filename to PUT request string
  request += " " + path + " " + filename;

  // Get the starting server index based on MD5 hash and encrypt the file using linux aes
  int bin = getBinForFile( filename );

  // TODO: reimplement this once sending text and binary works
  encryptFile( filename );
  filename = "." + filename;

  // Open the file, if it exists
  FILE *filePointer = NULL;
  if ( ( filePointer = fopen( filename.c_str(), "rb" ) ) == NULL ) {
    perror( "Could not open specified file" );
    return;
  }

  // Get the size of the file and the size of the file pieces
  long startingFilePosition;
  long fileSize;
  size_t filePieceLength;
  fseek( filePointer, 0, SEEK_END );
  fileSize = ftell( filePointer );
  fseek( filePointer, 0, SEEK_SET );
  filePieceLength = ( size_t ) ceil( ( ( double ) fileSize ) / 4.0 );
  printf( "Found fileSize(%ld), fileSize/4(%ld), ceil(%lf) filePieceLength(%ld)\n",
          fileSize, fileSize / 4, ceil( fileSize / 4 ), filePieceLength );

  // Allocate memory for the file and read it
  unsigned char *fileBuffer = new unsigned char[fileSize];
  fread( fileBuffer, fileSize, 1, filePointer );
  unsigned char *filePieceBuffer = fileBuffer;

  // Print file in hex for debugging
  printf( "Read file into memory:\n" );
  for ( int i = 0; i < fileSize; i++ ) {
    printf( "%02X", fileBuffer[ i ] );
  }
  printf( "\n" );

  size_t fileSizeSent = 0;
  for ( int i = 0; i < 4; i++ ) {
    printf( "primaryServer(%d), fileSizeSent(%ld), filePieceLength(%ld), fileSize(%ld)\n", ( i + bin ) % 4,
            fileSizeSent, filePieceLength, fileSize );
    std::string pieceNumberString = std::to_string( i ) + "\n";

    // Send the piece to the first server
    int primarySock = connectToServer( ( i + bin ) % 4 );
    if ( primarySock != -1 ) {
      std::string primaryRequest = request + " p " + pieceNumberString;
      // Send the header
      sendRequest( ( unsigned char * ) primaryRequest.c_str(), primaryRequest.length(), primarySock );
      // Send the data
      sendRequest( filePieceBuffer, filePieceLength, primarySock );
      send( primarySock, "", 0, 0 );
    }
    closeSocket( primarySock );

    // Send the piece to the backup server
    int secondarySock = connectToServer( ( i + bin + 1 ) % 4 );
    if ( secondarySock != -1 ) {
      std::string secondaryRequest = request + " s " + pieceNumberString;
      // Send the header
      sendRequest( ( unsigned char * ) secondaryRequest.c_str(), secondaryRequest.length(), secondarySock );
      // Send the data
      sendRequest( filePieceBuffer, filePieceLength, secondarySock );
      send( secondarySock, "", 0, 0 );
    }
    closeSocket( secondarySock );

    // Move the buffer forward filePieceLength
    fileSizeSent += filePieceLength;
    filePieceBuffer += filePieceLength;

    // Adjust the size for the last piece, if less than filePieceLength
    if ( fileSizeSent + filePieceLength >= fileSize ) {
      filePieceLength = fileSize - fileSizeSent;
    }
  }

//  TODO: readd this after implementing encryption
//  remove( ( filename ).c_str() );

  // Clean up the file
  delete[]fileBuffer;
  fclose( filePointer );
}

//TODO:: add handleMkdir()

//std::vector<char> loadFileToVector( std::string filename ) {
//  std::ifstream ifs( filename.c_str() );
//  if ( !ifs ) {
//    perror( "Could not open file:" );
//    return *( new std::vector<char>() );
//  }
//  else {
//    return std::vector<char>( std::istreambuf_iterator<char>( ifs ), std::istreambuf_iterator<char>() );
//  }
//}

//std::string loadFileToString( std::string filename ) {
//  std::ifstream ifs( filename.c_str(), std::ios::binary );
//  std::string ret( ( std::istreambuf_iterator<char>( ifs ) ), std::istreambuf_iterator<char>() );
//  return ret;
//}

void saveVectorToFile( std::vector<char> data, std::string filename ) {
  std::ofstream ofs( filename, std::ios::out | std::ofstream::binary );
  std::copy( data.begin(), data.end(), std::ostreambuf_iterator<char>( ofs ) );
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
  std::string command = "openssl aes-256-cbc -salt -in " + filename + " -out ." + filename + " -pass file:enc.key";

  // run the command
  FILE *fp = popen( command.c_str(), "r" );
  if ( fp == NULL ) {
    perror( "Problem running encryptFile:" );
  }
  pclose( fp );
}

void decryptFile( std::string filename ) {
  // encrypt the file in tempfile .filename
  std::string command = "openssl aes-256-cbc -d -in ." + filename + " -out " + filename + " -pass file:enc.key";
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