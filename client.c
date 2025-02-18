#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 1024

int get_socket_for_host(char *host, char *port) {
    int so;
    struct addrinfo hints;
    struct addrinfo *results, *result;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    int s = getaddrinfo(host, port, &hints, &results);
    if (s != 0) {
        fprintf(stderr, "Failed to getaddrinfo %s\n", gai_strerror(s));
        return -1;
    }

    for (result = results; result != NULL; result = result->ai_next) {
        so = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (so == -1) {
            continue;
        }

        if (connect(so, result->ai_addr, result->ai_addrlen) != -1) {
            freeaddrinfo(result);
            return so;
        }

        close(so);
    }

    fprintf(stderr, "Failed to connect to all the addresses for %s:%s\n", host, port);
    freeaddrinfo(result);
    return -1;
}

SSL* create_ssl_for_socket(int fd, char *host) {
    const SSL_METHOD *client_method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(client_method);
    if (ctx == NULL) {
        printf("Failed to create SSL CTX\n");
        return NULL;
    }
    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) {
        printf("Failed to create SSL\n");
        return NULL;
    }

    // https://stackoverflow.com/a/59372624
    // This is important in case we a single server running multiple virtual servers
    // And it's using the hostname to multiplex amongst them.
    SSL_set_tlsext_host_name(ssl, host);

    SSL_set_fd(ssl, fd);

    // TLS handshake
    int ret = SSL_connect(ssl);
    if (ret != 1) {
        printf("Failed to establish TLS handshake\n");
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    return ssl;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <host> <port> <path>\n", argv[0]);
        exit(-1);
    }
    char *host = argv[1];
    char *port = argv[2];
    char *path = argv[3];

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    printf("Creating raw tcp socket\n");
    int so = get_socket_for_host(host, port);
    if (so < 0) {
        exit(1);
    }
    printf("Created socket: %d\n", so);

    printf("Starting TLS handshake\n");
    SSL* ssl = create_ssl_for_socket(so, host);
    if (ssl == NULL) {
        exit(-2);
    }
    printf("Completed TLS handshake\n");

    char request[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    snprintf(
        request, BUFFER_SIZE, 
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: AlhaddarTLS/1.0.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n", 
        path,
        host
    );

    printf("Sending request\n");
    SSL_write(ssl, request, strlen(request));

    printf("Reading response\n\n");
    int n = SSL_read(ssl, response, BUFFER_SIZE);
    if (n < 0) {
        char buf[256];
        ERR_error_string(SSL_get_error(ssl, n), buf);
        printf("Failed to read response: %s\n", buf);
        exit(-2);
    }
    while (n > 0) {
        fwrite(response, 1, n, stdout);
        n = SSL_read(ssl, response, BUFFER_SIZE);
    }

    fflush(stdout);
    exit(0);
}