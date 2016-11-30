Matt Pennington
CSCI5273 Network Systems
Programming Assignment 3
November 19, 2016

The client and server are compiled with 'make'.
To run, type "./run.sh"
This script shuts down any previously running servers, resets the target directories, starts the servers, and sends server output and stderr to log files.

The implementation is quite slow, I made no attempt to increase speed for transfer of large files.  My hypothesis is that the large amount of logging is to blame.  I am printing the hex values of everything sent and received multiple times, for a 70Kb file transfer there is ~7MB of logs written.
All three extra credits were implemented (subfolders, encryption, and optimization).
The MD5 function depends on a Python helper function: getbin.py