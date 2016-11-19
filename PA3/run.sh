#!/bin/bash

# Kill any dfs servers currently running
kill $(ps aux | grep dfs | awk -F ' ' '{print $2}') 2>/dev/null

# Purge any temporary encrypted files and the DFS# directories
rm -rf .* DFS*/.* DFS*/* 2>/dev/null

# Start the servers and redirect output to the logs
./dfs /DFS1 10001 > server1.log 2>&1 &
./dfs /DFS2 10002 > server2.log 2>&1 &
./dfs /DFS3 10003 > server3.log 2>&1 &
./dfs /DFS4 10004 > server4.log 2>&1 &

# Start the client and wait for commands from stdin
#valgrind -v --leak-check=full --show-leak-kinds=all ./dfc
./dfc