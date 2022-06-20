#ifndef DTLS_H
#define DTLS_H	1

#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdbool.h>

int create_server_socket(unsigned int port);

int create_client_socket(void);

SSL_CTX *create_ssl_context(bool is_server);

void configure_client_context(SSL_CTX *ctx, const char *cert);

void configure_server_context(SSL_CTX *ctx, const char *cert, const char *pvtkey);

ssize_t readline_ssl(SSL *ssl, char *buf, size_t size, int *err);

#endif	/* ifndef DTLS_H */
