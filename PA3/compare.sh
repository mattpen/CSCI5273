#!/usr/bin/env bash

echo 'original:'
#xxd .testfile
echo 'primary:'
#xxd */.testfile.0.p ; xxd */.testfile.1.p; xxd */.testfile.2.p; xxd */.testfile.3.p; echo ''


cat */.testfile.0.p > newtestp.jpg
cat */.testfile.1.p >> newtestp.jpg
cat */.testfile.2.p >> newtestp.jpg
cat */.testfile.3.p >> newtestp.jpg
#xxd newtestp.jpg

echo 'secondary:'
#xxd */.testfile.0.s ; xxd */.testfile.1.s; xxd */.testfile.2.s; xxd */.testfile.3.s; echo ''

cat */.testfile.0.s > newtests.jpg;
cat */.testfile.1.s >> newtests.jpg;
cat */.testfile.2.s >> newtests.jpg;
cat */.testfile.3.s >> newtests.jpg;
#xxd newtests.jpg
