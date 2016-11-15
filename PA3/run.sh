#!/bin/bash

kill $(ps aux | grep dfs | awk -F ' ' '{print $2}')
rm -rf .* DFS*/.*

./dfs /DFS1 10001 > server1.log &
./dfs /DFS2 10002 > server2.log &
./dfs /DFS3 10003 > server3.log &
./dfs /DFS4 10004 > server4.log &
./dfc

