#!/usr/bin/env bash

echo 'original:'
#xxd .w.jpg
echo 'primary:'
#xxd */.w.jpg.0.p ; xxd */.w.jpg.1.p; xxd */.w.jpg.2.p; xxd */.w.jpg.3.p; echo ''


cat */.w.jpg.0.p > newtestp.jpg
cat */.w.jpg.1.p >> newtestp.jpg
cat */.w.jpg.2.p >> newtestp.jpg
cat */.w.jpg.3.p >> newtestp.jpg
#xxd newtestp.jpg

echo 'secondary:'
#xxd */.w.jpg.0.s ; xxd */.w.jpg.1.s; xxd */.w.jpg.2.s; xxd */.w.jpg.3.s; echo ''

cat */.w.jpg.0.s > newtests.jpg;
cat */.w.jpg.1.s >> newtests.jpg;
cat */.w.jpg.2.s >> newtests.jpg;
cat */.w.jpg.3.s >> newtests.jpg;
#xxd newtests.jpg
