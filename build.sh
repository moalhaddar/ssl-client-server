set -xe

gcc -o ./build/client client.c -lssl -lcrypto -ggdb 