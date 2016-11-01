# Helper function for dfc.cpp
# Prints md5sum modulo 4 of filename to stdout
#
# Matt Pennington
# CSCI5273 Network Systems - Programming Assignment 3
# Distributed File System

import hashlib
import sys

if len(sys.argv) == 2:
    data = open(sys.argv[1], "rb").read()
    hash = hashlib.md5()
    hash.update(data)
    digest = hash.hexdigest()
    sum = int(digest, 16)
    print(sum % 4)
