/**
  * CSCI5273 Network Systems
  * Programming Assignment 2 - web server
  * Matt Pennington - mape5853
  *
  **/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string>
#include <unordered_map>

struct Request {
  std::string method;
  std::string uri;
  std::string version;
  std::unordered_map <std::string, std::string> headers;
  std::string data;
  std::string error;
};

bool isWhiteSpace( char c ) {
  return c == ' ' || c == '\t';
}

bool isCRLF( char c ) {
  return c == '\n';
}

Request parseRequest( char *requestString, int requestString_len ) {
  Request request; // return value
  int i = 0; //index in requestString
  request.error = "";

  std::string key;
  std::string value;

  // get method
  request.method = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
    request.method.append(1u, requestString[i]);
    i++;
  }

  // Check for supported methods
  if ( request.method.compare("GET") != 0 && request.method.compare("POST") != 0 ) {
    request.error = "400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
    request.error.append( request.method );
    request.error.append( "</body></html>" );
    return request;
  }

  // move to URI
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // get uri
  request.uri = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
    request.uri.append(1u, requestString[i]);
    i++;
  }

  // move to version
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // get version
  request.version = "";
  while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
    request.version.append(1u, requestString[i]);
    i++;
  }

  // move to end of first line
  while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
    i++;
  }

  // read one new line
  if ( i < requestString_len && isCRLF( requestString[i] ) ) {
    i++;
  }
  else {
    // more data after version has been read, error!
    // request.error = "400 something"
    // return request;
  }

  // get headers if any
  while ( i < requestString_len && !isCRLF( requestString[i] ) ) {
    // Read header name
    key = "";
    while ( i < requestString_len && !isWhiteSpace( requestString[i] ) && !isCRLF( requestString[i] ) ) {
      key.append(1u, requestString[i]);
      i++;
    }

    // move to value
    while( i < requestString_len && isWhiteSpace( requestString[i] ) ) {
      i++;
    }

    // Read header value
    value = "";
    while ( i < requestString_len && !isCRLF( requestString[i] ) ) {
      value.append(1u, requestString[i]);
      i++;
    }

    request.headers[key] = value;

    // read one new line
    if ( i < requestString_len && isCRLF( requestString[i] ) ) {
      i++;
    }
  }



  // read an empty line, or error
  if ( i < requestString_len && isCRLF( requestString[i] ) ) {
    i++;
  }

  // get data
  request.data = "";
  while ( i < requestString_len ) {
    request.data.append(1u, requestString[i]);
  }

  return request;
}

std::string validateMethod( Request request );

// validateRequest
  // if !(getMethod returns supported value
  // && getURI has supported filetype
  // && if getVersion returns supported value)
    //respond(400)
  // else
    //return true

// getMethod
  // read charaters until first whitespace and store in string
  // return string and index of whitespace character
// getURI
  // read
// getVersion
// getHeaders -- returns headers in unordered_map if any exist
// getData


// writeResponse
  // validateRequest
  // if invalidRequest
    // respond(400)

  // parse method
  // if method not supported  

// handleRequest
  // validateRequest
  // getHeaders

  // if request contains http1.1 version and keep-alive header
    // set starttime = currenttime
    // loop until currenttime - timeout >= starttime
      // start new thread or process and writeResponse
      // receiveMessage
      
  // else if not keep-alive
    // writeResponse



int main (int argc, char * argv[]) {

  char a[19] = "GET /home HTTP/1.0";
  Request ar = parseRequest( a, 19 );

  printf("%s::%s::%s::%s\n", ar.method.c_str(), ar.uri.c_str(), ar.version.c_str(), ar.error.c_str());

  char b[19] = "EGT /home HTTP/1.0";
  Request br = parseRequest( b, 19 );

  printf("%s::%s::%s::%s\n", br.method.c_str(), br.uri.c_str(), br.version.c_str(), br.error.c_str());

  // initialize variables

  // read conf file and initialize

  // setup sock

  // loop for incoming messages
    // read the message
    // start a new thread or process and handleRequest
}