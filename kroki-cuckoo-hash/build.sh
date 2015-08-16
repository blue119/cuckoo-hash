gcc -std=c11 -fPIC cuckoo_hash.c -c
gcc -std=c11 -fPIC lookup3.c -c
gcc -shared -o libckckoo.so.1.0.0 cuckoo_hash.o lookup3.o

