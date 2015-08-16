#!/bin/bash

[ -a cuckoo_hash ] && rm cuckoo_hash
gcc cuckoo_hash.c lookup3.c --std=c11 -g -o cuckoo_hash
[ -x cuckoo_hash ] && valgrind --leak-check=full --show-leak-kinds=all --log-file=/tmp/val.log ./cuckoo_hash
