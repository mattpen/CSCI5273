Matt Pennington
CSCI 5273 Network Systems
Programming Assignment 4: webproxy


The program compiles with 'make'.
The program runs with './webproxy PORT_NUMBER [CACHE_TIMEOUT]'
The timeout value is optional.

I implemented all of the mandatory requirements for grad students plus the prefetching extra credit.  I did not attempt to implement the HTTP/1.1 keep-alive extra credit.

Here are the grading requirements from the recent email message:
pages_from_server - PASS
pages_from_proxy  - PASS
pages_from_server_after_cache_timeout - PASS
prefetching       - PASS
multithreading  - PASS
connection keepalive    -  FAIL

I was not able to get the test.py script to work.  When ./webproxy is executed from within a python script on my machine, the outgoing http packets are blocked.  I recorded a video and repeated all of the tests from the script using Apache Bench (ab) instead (http://httpd.apache.org/docs/2.4/programs/ab.html).

The video of the tests are available at https://youtu.be/-f6jwiGFHJk.

The cache is implemented using unordered_map, so I did not explicitly use my own hashing scheme as unordered_map hashes keys internally.
