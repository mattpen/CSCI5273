#!/bin/bash

kill $(ps aux | grep dfs | awk -F ' ' '{print $2}')

./dfs /DFS1 10001 &
./dfs /DFS2 10002 &
./dfs /DFS3 10003 &
./dfs /DFS4 10004 &
./dfc
