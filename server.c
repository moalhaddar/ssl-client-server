#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <openssl/ssl.h>

#define INTERFACE "0.0.0.0"
#define PORT 4444
#define BACKLOG 1024

int create_listening_socket() {
    int so = socket(AF_INET, SOCK_STREAM, 0);
    if (so == -1) {
        fprintf(stderr, "Failed to create socket\n");
        exit(-1);
    }
    int optval = 1;
    setsockopt(so, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, INTERFACE, &address.sin_addr.s_addr) != 1) {
        printf("Failed to convert binding address to network order\n");
        exit(-1);
    }
    address.sin_port = htons(PORT);
    if (bind(so, (struct sockaddr*) &address, sizeof(address)) == -1) {
        fprintf(stderr, "Failed to bind the socket\n");
        exit(-1);
    }

    if (listen(so, BACKLOG) != 0) {
        printf("Failed to listen\n");
        exit(-1);
    }

    printf("Listening on %s:%d\n", INTERFACE, PORT);

    return so;
}

SSL_CTX* prepare_ssl_context() {
    const SSL_METHOD *server_method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(server_method);
    if (ctx == NULL) {
        printf("Failed to create SSL context\n");
        exit(-2);
    }

    SSL_CTX_use_certificate_file(ctx, "./keys/certificate.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "./keys/private.key", SSL_FILETYPE_PEM);

    return ctx;
}

int main() {

    int listenfd = create_listening_socket();

    SSL_CTX *ctx = prepare_ssl_context();
    while (true) {
        int clientfd = accept(listenfd, NULL, NULL);
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, clientfd);
        int ssl_client = SSL_accept(ssl);
        printf("Accepted connection\n");
        char *buffer[1024];
        int n = SSL_read(ssl, buffer, 1024);
        printf("Read %d bytes\n", n);
        fwrite(buffer, 1, n, stdout);
        SSL_write(ssl, buffer, n);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(clientfd);
    }
}