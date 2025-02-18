set -xe

gcc -o ./build/client client.c -lssl -lcrypto -ggdb 
gcc -o ./build/server server.c -lssl -lcrypto -ggdb 