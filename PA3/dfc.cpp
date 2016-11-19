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
#include <set>
#include <math.h>
#include <unordered_map>


#define MSG_SIZE 2000
#define SIZE_MESSAGE_SIZE 17

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
};

// Get the configuration file and initialize the global config parameter
void initConfig();

void handleList( std::string command );

void handleGet( std::string command );

void handlePut( std::string command );

void handleMkDir( std::string command );

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

/**
 * Poll for user input, parse the command and run the appropriate branch
 */
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
    else if ( command.find( "MKDIR" ) == 0 ) {
      handleMkDir( command );
    }
      // TODO: add MKDIR
    else if ( command.find( "EXIT" ) == 0 ) {
      return 0;
    }
    bzero( cmd, 256 );
  }
}

/**
 * Reads the username, password, and remote server address information into memory
 */
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
        uint16_t portNum;
        try {
          portNum = ( uint16_t ) std::stoi( port.c_str() );
        }
        catch ( std::invalid_argument e ) {
          printf( "Client invalid port stoi(%s)\n", port.c_str() );
          return;
        }
        catch ( std::out_of_range e ) {
          printf( "Client out-of-range port stoi(%s)\n", port.c_str() );
          return;
        }
        config.servers[ socketNum ].port = portNum;

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

/**
 * Connects to the server addr and port specified by index i
 * @param i
 * @return socket - file descriptor
 */
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
  std::string sizeString = std::to_string( request_size );
  while ( sizeString.length() < 16 ) {
    sizeString = " " + sizeString;
  }
  send( sock, sizeString.c_str(), 16, 0 );

  unsigned char *send_buffer = request;

  while ( request_size > 0 ) {
    ssize_t send_size = send( sock, send_buffer, request_size, 0 );
    printf( "Client sent size(%ld), ", send_size );
    printf( "data:(" );
    for ( int i = 0; i < request_size; i++ ) {
      printf( "%02X", send_buffer[ i ] );
      if ( ( i + 1 ) % 2 == 0 ) {
        printf( " " );
      }
    }
    printf( ")\n" );

    if ( send_size < 0 ) {
      perror( "Error sending get body" );
    }
    send_buffer += send_size;
    request_size -= send_size;
  }
}

/**
 * TODO:: revise this to read binary data correctly into a vector and wait for empty message
 *
 * Reads data from sock until we receive an empty message
 * @param sock - active and connected socket file descriptor
 * @return Response
 */
std::vector<unsigned char> getResponse( int sock ) {
  char sizeMessage[SIZE_MESSAGE_SIZE];
  bzero( sizeMessage, SIZE_MESSAGE_SIZE );
  size_t bytesRead = 0;

  while ( bytesRead < SIZE_MESSAGE_SIZE - 1 ) {
    bytesRead += recv( sock, sizeMessage, SIZE_MESSAGE_SIZE - 1 - bytesRead, 0 );
    printf( "Client read(%ld) bytes, message(%s)\n", bytesRead, sizeMessage );
    if ( bytesRead == 0 || bytesRead == -1 ) {
      printf( "Error during getResponse\n" );
      return std::vector<unsigned char>();
    }
  }

  ssize_t messageSize;
  try {
    messageSize = std::stoi( std::string( sizeMessage ) );
  }
  catch ( std::invalid_argument e ) {
    printf( "Client invalid messageSize stoi(%s)\n", sizeMessage );
    return std::vector<unsigned char>();
  }
  catch ( std::out_of_range e ) {
    printf( "Client out-of-range messageSize stoi(%s)\n", sizeMessage );
    return std::vector<unsigned char>();
  }
  printf( "Client getting message of len(%s)(%ld)\n", sizeMessage, messageSize );

  unsigned char clientMessage[MSG_SIZE];
  bzero( clientMessage, MSG_SIZE );
  ssize_t readSize;
  std::vector<unsigned char> responseVector = std::vector<unsigned char>();

  while ( messageSize > 0 ) {
    bzero( clientMessage, MSG_SIZE );
    readSize = recv( sock, clientMessage, MSG_SIZE - 1, 0 );
    if ( readSize == -1 ) {
      char err[6] = "ERROR";
      printf( "Client recv error\n" );
      responseVector.insert( responseVector.begin(), err, err + 5 );
      perror( "recv failed" );
      return responseVector;
    }

    printf( "Client read buffer from sock, size(%ld):\n", readSize );
    for ( int i = 0; i < readSize; i++ ) {
      printf( "%02X", clientMessage[ i ] );
    }
    printf( "\n" );

    responseVector.insert( responseVector.end(), clientMessage, clientMessage + readSize );
    messageSize -= readSize;
  }

  printf( "Client read vector.data: %s\n", responseVector.data() );
  return responseVector;
}

/**
 * Requests file lists from each server,
 * compiles a list of complete and incomplete files,
 * prints results to stdout
 *
 *
 * @param command
 */
void handleList( std::string command ) {
  printf( "handling list\n" );

  int sock = -1;
  std::string request;
  std::unordered_map<std::string, std::set<int>> fileMap;


  request = "LIST " + config.username + ":" + config.password;
  if ( command.find( " " ) != -1 ) {
    request += command.substr( command.find( " " ), command.length() - command.find( " " ) - 1 ) + "\n";
  }
  else {
    request += " /";
  }

  for ( int i = 0; i < 4; i++ ) {
    sock = connectToServer( i );
    if ( sock != -1 ) {
//      printf( "LIST SENT: %s\n", request.c_str() );
      sendRequest( ( unsigned char * ) request.c_str(), request.length(), sock );
      std::vector<unsigned char> responseVector = getResponse( sock );


//      printf( "LIST RECVD: %s\n", responseVector.data() );

      if ( responseVector.data() != NULL ) {
        std::string listString = ( char * ) responseVector.data();
        while ( listString.compare( "" ) != 0 ) {
          std::string line;
          size_t newlineIndex = listString.find( '\n' );
          if ( newlineIndex != std::string::npos ) {
            line = listString.substr( 0, newlineIndex );
            listString = listString.substr( newlineIndex + 1, listString.length() - newlineIndex );
          }
          else {
            line = listString;
            listString = "";
          }

          printf( "Parsing line(%s), listString(%s)\n", line.c_str(), listString.c_str() );

          if ( line.find( "." ) == 0 && line.rfind( "." ) > line.find( "." ) &&
               line.rfind( "." ) < line.length() - 1 ) {
            std::string filename = line.substr( 1, line.rfind( "." ) - 1 );
            printf( "List found filename(%s)\n", filename.c_str() );
            int pieceNumber;
            try {
              pieceNumber = std::stoi( line.substr( line.rfind( "." ) + 1, 1 ).c_str() );
            }
            catch ( std::invalid_argument e ) {
              printf( "Client invalid pieceNumber stoi(%s)\n", line.substr( line.rfind( "." ) + 1, 1 ).c_str() );
              return;
            }
            catch ( std::out_of_range e ) {
              printf( "Client out-of-range pieceNumber stoi(%s)\n", line.substr( line.rfind( "." ) + 1, 1 ).c_str() );
              return;
            }
            fileMap[ filename ].insert( pieceNumber );
          }
        }
      }
    }
  }

  printf( "PRESENTING LIST:::\n" );
  for ( auto it = fileMap.begin(); it != fileMap.end(); ++it ) {
    if ( it->second.find( 0 ) != it->second.end()
         && it->second.find( 1 ) != it->second.end()
         && it->second.find( 2 ) != it->second.end()
         && it->second.find( 3 ) != it->second.end() ) {
      printf( "%s\n", it->first.c_str() );
    }
    else {
      printf( "%s (incomplete)\n", it->first.c_str() );
    }
  }
  printf( "END LIST:::\n" );
  closeSocket( sock );
}

/**
 * @param pieceMap - mutable adds piece n to pieceMap[n] if found
 * @param request
 * @param serverIndex
 * @return true if pieces were successfully found, false if error
 */
bool
sendGetRequest( std::unordered_map<int, std::vector<unsigned char>> &pieceMap, std::string request, int serverIndex ) {

  // Connect and handle request/response
  int sock = connectToServer( serverIndex );
  sendRequest( ( unsigned char * ) request.c_str(), request.length(), sock );
  std::vector<unsigned char> response = getResponse( sock );
  closeSocket( sock );

//  printf( "Client got GET response\n" );
  size_t readIndex = 0;
  std::string buffer;

  // Get the piece number for the first piece
  buffer = "";
  for ( ; response[ readIndex ] != ' '; readIndex++ ) {
    buffer.push_back( response[ readIndex ] );
  }
//  printf( "Client read buffer during GET firstPieceNum(%s)", buffer.c_str() );
  int firstPieceNumber = std::stoi( buffer );
//  printf( "Client read piece(%d) from server(%d)\n", firstPieceNumber, serverIndex );
  readIndex++;

  // Get the piece number for the second piece
  buffer = "";
  for ( ; response[ readIndex ] != ' '; readIndex++ ) {
    buffer.push_back( response[ readIndex ] );
  }
//  printf( "Client read buffer during GET secondPieceNum(%s)", buffer.c_str() );
  int secondPieceNumber = std::stoi( buffer );
//  printf( "Client read piece(%d) from server(%d)\n", secondPieceNumber, serverIndex );
  readIndex++;

  // Get the index for the first byte of the second piece
  buffer = "";
  for ( ; response[ readIndex ] != ' '; readIndex++ ) {
    buffer.push_back( response[ readIndex ] );
  }
//  printf( "Client read buffer during GET firstPieceSize(%s)", buffer.c_str() );
  readIndex++;
  ssize_t splitIndex = std::stoi( buffer ) + readIndex;
//  printf( "Client got splitIndex(%ld) from server(%d)\n", splitIndex, serverIndex );

  // Read the first piece
  std::vector<unsigned char> firstPiece;
  for ( ; readIndex < splitIndex; readIndex++ ) {
    firstPiece.push_back( response[ readIndex ] );
  }
//  readIndex++;
  pieceMap[ firstPieceNumber ] = firstPiece;

  // Read the second piece
  std::vector<unsigned char> secondPiece;
  for ( ; readIndex < response.size(); readIndex++ ) {
    secondPiece.push_back( response[ readIndex ] );
  }
  pieceMap[ secondPieceNumber ] = secondPiece;

  return true;
}

void handleGet( std::string command ) {
  printf( "handling get\n" );
  std::string filename;
  std::string path;

  // Write authentication parameter to the request
  std::string request = "GET " + config.username + ":" + config.password;

  // Extract the filename and path from the command
  uint64_t filenameStartIndex, filenameLength, pathStartIndex, pathLength;

  // Look for starting indices
  // Filename is the first parameter, look for the first whitespace
  filenameStartIndex = command.find( " " ) + 1;
  // Path is the second parameter, we look for the last whitespace
  pathStartIndex = command.rfind( " " ) + 1;

  if ( pathStartIndex != filenameStartIndex ) {
    // We found a path (optional)
    filenameLength = pathStartIndex - filenameStartIndex - 1;
    pathLength = command.length() - pathStartIndex;
    path = command.substr( pathStartIndex, pathLength );
    filename = command.substr( filenameStartIndex, filenameLength );
  }

  else {
    // We did not find a path
    path = "/";
    filename = command.substr( filenameStartIndex, command.length() - command.find( " " ) );
  }

  // If the last character of the filename is a newline, remove it.
  if ( filename[ filename.length() - 1 ] == '\n' ) {
    filename.pop_back();
  }
  if ( path[ path.length() - 1 ] == '\n' ) {
    path.pop_back();
  }

//  printf( "GET found path(%s), filename(%s)\n", path.c_str(), filename.c_str() );
  // Add path and filename to PUT request string
  request += " " + path + " " + filename;


  std::unordered_map<int, std::vector<unsigned char>> pieces;
  // Get the file pieces.  Try servers 0&2 first, then try 1&3 if unsuccessful.
  // If any two consecutive servers are down, the file is incomplete and the function returns early.
  // Get pieces from server 0
  if ( !sendGetRequest( pieces, request, 0 ) ) {
    //If server 0 didn't respond get pieces from server 1
    if ( !sendGetRequest( pieces, request, 1 ) ) { return; }
    else {
      //get piece from server 3
      if ( !sendGetRequest( pieces, request, 3 ) ) { return; }
    }
  }
  else {
    //Get pieces from server 2
    if ( !sendGetRequest( pieces, request, 2 ) ) {
      //If server 2 didn't respond get pieces from server 1&3
      if ( !sendGetRequest( pieces, request, 1 ) ) { return; }
      else {
        // get piece from server 3
        if ( !sendGetRequest( pieces, request, 3 ) ) { return; }
      }
    }
  }

  // Write the pieces to a temporary encrypted file
  std::ofstream ofs( ".NEW" + filename, std::ios::out | std::ofstream::binary );
  for ( int i = 0; i < 4; i++ ) {
    printf( "Concatenating piece(%d):(", i );
    for ( int j = 0; j < pieces[ i ].size(); j++ ) {
      printf( "%02X", pieces[ i ][ j ] );
      if ( ( j + 1 ) % 2 == 0 ) {
        printf( " " );
      }
    }
    printf( ")\n" );

    std::copy( pieces[ i ].begin(), pieces[ i ].end(), std::ostreambuf_iterator<char>( ofs ) );
  }
  ofs.close();

  decryptFile( filename );
  // Delete the temporary file
//  remove( ( ".NEW" + filename ).c_str() );

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

  if ( pathStartIndex != filenameStartIndex ) {
    // We found a path (optional)
    filenameLength = pathStartIndex - filenameStartIndex - 1;
    pathLength = command.length() - pathStartIndex;
    path = command.substr( pathStartIndex, pathLength );
    filename = command.substr( filenameStartIndex, filenameLength );
  }

  else {
    // We did not find a path
    path = "/";
    filename = command.substr( filenameStartIndex, command.length() - command.find( " " ) );
  }

  // If the last character of the filename is a newline, remove it.
  if ( filename[ filename.length() - 1 ] == '\n' ) {
    filename.pop_back();
  }
  if ( path[ path.length() - 1 ] == '\n' ) {
    path.pop_back();
  }

//  printf( "PUT found path(%s), filename(%s)\n", path.c_str(), filename.c_str() );
  // Add path and filename to PUT request string
  request += " " + path + " " + filename;

  // Get the starting server index based on MD5 hash and encrypt the file using linux aes
  int bin = getBinForFile( filename );

  // Encrypt the file and use the temporary filename
  encryptFile( filename );
  filename = "." + filename;

  // Open the file, if it exists
  FILE *filePointer = NULL;
  if ( ( filePointer = fopen( filename.c_str(), "rb" ) ) == NULL ) {
    perror( "Could not open specified file" );
    return;
  }

  // Get the size of the file and the size of the file pieces
  long fileSize;
  size_t filePieceLength;
  fseek( filePointer, 0, SEEK_END );
  fileSize = ftell( filePointer );
  fseek( filePointer, 0, SEEK_SET );
  filePieceLength = ( size_t ) ceil( ( ( double ) fileSize ) / 4.0 );
  printf( "Found fileSize(%ld), fileSize/4(%ld), ceil(%lf) filePieceLength(%ld)\n", fileSize, fileSize / 4,
          ceil( fileSize / 4 ), filePieceLength );

  // Allocate memory for the file and read it
  unsigned char fileBuffer[fileSize];
  fread( fileBuffer, fileSize, 1, filePointer );
  unsigned char *filePieceBuffer = fileBuffer;

  // Print file in hex for debugging
  printf( "Read file into memory(" );
  for ( int i = 0; i < fileSize; i++ ) {
    printf( "%02X", fileBuffer[ i ] );
  }
  printf( ")\n" );

  size_t fileSizeSent = 0;
  for ( int i = 0; i < 4; i++ ) {
    //printf( "primaryServer(%d), fileSizeSent(%ld), filePieceLength(%ld), fileSize(%ld)\n", ( i + bin ) % 4, fileSizeSent, filePieceLength, fileSize );

    // Connect to the appropriate servers
    int primarySock = connectToServer( ( i + bin ) % 4 );
    int secondarySock = connectToServer( ( i + bin + 1 ) % 4 );

    if ( primarySock != -1 ) {
      // Add the piece number to the request
      std::string pieceRequestString = request + " " + std::to_string( i ) + "\n";

      printf( "Sending piece(%i)(", i );
      for ( int j = 0; j < filePieceLength; j++ ) {
        printf( "%02X", filePieceBuffer[ j ] );
        if ( ( j + 1 ) % 2 == 0 ) {
          printf( " " );
        }
      }
      printf( ")\n" );

      // Concatenate the request and the data
      unsigned char message[pieceRequestString.length() + filePieceLength];
      std::copy( ( unsigned char * ) pieceRequestString.c_str(),
                 ( unsigned char * ) pieceRequestString.c_str() + pieceRequestString.length(), message );
      std::copy( filePieceBuffer, filePieceBuffer + filePieceLength, message + pieceRequestString.length() );

      // Send the full message
      sendRequest( message, pieceRequestString.length() + filePieceLength, primarySock );
      sendRequest( message, pieceRequestString.length() + filePieceLength, secondarySock );
    }

    // Close the connections, no response needed
    closeSocket( primarySock );
    closeSocket( secondarySock );

    // Move the buffer forward filePieceLength
    fileSizeSent += filePieceLength;
    filePieceBuffer += filePieceLength;

    // Adjust the size for the last piece, if less than filePieceLength
    if ( fileSizeSent + filePieceLength >= fileSize ) {
      filePieceLength = fileSize - fileSizeSent;
    }
  }

  // Delete the temporary file
//  remove( ( filename ).c_str() );

  // Close the file
  fclose( filePointer );
}

void handleMkDir( std::string command ) {
  printf( "Handling mkdir\n" );
  // Write authentication parameter to the request
  command.insert( command.find( ' ' ), " " + config.username + ":" + config.password );

  // Send command to each server
  for ( int i = 0; i < 4; i++ ) {
    int sock = connectToServer( i );
    sendRequest( ( unsigned char * ) command.c_str(), command.length(), sock );
    closeSocket( sock );
  }
}

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
  int ret;
  try {
    ret = std::stoi( buf );
  }
  catch ( std::invalid_argument e ) {
    printf( "Client invalid of buf stoi(%s)\n", buf );
    return -1;
  }
  catch ( std::out_of_range e ) {
    printf( "Client out-of-range of buf stoi(%s)\n", buf );
    return -1;
  }
  return ret;
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
  std::string command = "openssl aes-256-cbc -d -in .NEW" + filename + " -out NEW." + filename + " -pass file:enc.key";
  printf( "Client running decrypt(%s)\n", command.c_str() );
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