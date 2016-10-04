Matt Pennington - mape5853
Programming Assignment 1
CSCI5273 Network Systems

To compile the programs type "make"
To run the client, type "client {SERVER_IP} {PORT}"
To run the server, type "server {PORT}"

I have implemented reliable UDP transfer with the stop-and-wait protocol. The stop-and-wait protocol is implemented for both sending files (put) and retrieving files (get, ls).  The program is known to fail if the filename contains a period or space. The get function for a non-existing file is not implemented quite correctly, it will send an empty file to the client.
  
