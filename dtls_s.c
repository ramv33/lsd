/*
 *  Copyright 2022 The OpenSSL Project Authors. All Rights Reserved.
 *
 *  Licensed under the Apache License 2.0 (the "License").  You may not use
 *  this file except in compliance with the License.  You can obtain a copy
 *  in the file LICENSE in the source distribution or at
 *  https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdbool.h>

static const int server_port = 4433;

/*
 * This flag won't be useful until both accept/read (TCP & SSL) methods
 * can be called with a timeout. TBD.
 */
static volatile bool    server_running = true;

int app_verify_cookie_cb(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len)
{
	return 1;
}

int app_gen_cookie_cb(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)
{
	for (int i = 0; i < 32; ++i)
		cookie[i] = i;
	*cookie_len = 32;
	return 1;
}

int create_server_socket(int port)
{
	int s;
	int optval = 1;
	struct sockaddr_in addr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("unable to create socket");
		exit(EXIT_FAILURE);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(server_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* Reuse the address; good for quick restarts */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
			< 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
	}
	if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror("unable to bind");
		return -1;
	}

	return s;
}

SSL_CTX* create_context(bool isServer)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	if (isServer)
		method = DTLS_server_method();
	else
		method = DTLS_client_method();

	ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	SSL_CTX_set_cookie_generate_cb(ctx, app_gen_cookie_cb);
	SSL_CTX_set_cookie_verify_cb(ctx, app_verify_cookie_cb);

	return ctx;
}

void configure_server_context(SSL_CTX *ctx)
{
	/* Set the key and cert */
	if (SSL_CTX_use_certificate_chain_file(ctx, "cert.pem") <= 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	int result;

	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	BIO *bio = NULL;

	int sockfd = -1;

	char rxbuf[128];
	size_t rxcap = sizeof(rxbuf);
	int rxlen;

	struct sockaddr_in addr, cliaddr;
	socklen_t addr_len = sizeof(addr);
	char addrstr[INET_ADDRSTRLEN];

	/* Splash */
	printf("\ndtlsecho : Simple Echo Client/Server (OpenSSL 3.0.1-dev) : %s : %s\n\n", __DATE__,
			__TIME__);

	bool isServer = true;

	/* Create context used by both client and server */
	ssl_ctx = create_context(isServer);
	/* If server */
	printf("We are the server on port: %d\n\n", server_port);

	/* Configure server context with appropriate key files */
	configure_server_context(ssl_ctx);

	/* Create server socket; will bind with server port and listen */
	sockfd = create_server_socket(server_port);

	/*
	 * Loop to accept clients.
	 * Need to implement timeouts on TCP & SSL connect/read functions
	 * before we can catch a CTRL-C and kill the server.
	 */
	while (server_running) {
		/* Create server SSL structure using newly accepted client socket */
		ssl = SSL_new(ssl_ctx);
		bio = BIO_new_dgram(sockfd, BIO_NOCLOSE);
		SSL_set_bio(ssl, bio, bio);
		SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);
		while (DTLSv1_listen(ssl, (BIO_ADDR *)&cliaddr) <= 0)
			;
		printf("%s:%d connected\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, addrstr, sizeof(addrstr)),
			ntohs(cliaddr.sin_port));
		/* Echo loop */
		while (true) {
			/* Get message from client; will fail if client closes connection */
			if ((rxlen = SSL_read(ssl, rxbuf, rxcap)) <= 0) {
				if (rxlen == 0) {
					printf("Client closed connection\n");
				}
				ERR_print_errors_fp(stderr);
				break;
			}
			/* Insure null terminated input */
			rxbuf[rxlen] = 0;
			/* Look for kill switch */
			if (strcmp(rxbuf, "kill\n") == 0) {
				/* Terminate...with extreme prejudice */
				printf("Server received 'kill' command\n");
				server_running = false;
				break;
			}
			/* Show received message */
			printf("Received: %s", rxbuf);
			/* Echo it back */
			if (SSL_write(ssl, rxbuf, rxlen) <= 0) {
				ERR_print_errors_fp(stderr);
			}
		}
		if (server_running) {
			/* Cleanup for next client */
			SSL_shutdown(ssl);
			SSL_free(ssl);
		}
	}
	printf("Server exiting...\n");
exit:
	/* Close up */
	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	SSL_CTX_free(ssl_ctx);

	if (sockfd != -1)
		close(sockfd);

	return 0;
}
