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

static const int server_port = 6969;

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

int create_client_socket(void)
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("unable to creat socket");
		return -1;
	}
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

void configure_client_context(SSL_CTX *ctx)
{
	/*
	 * Configure the client to abort the handshake if certificate verification
	 * fails
	 */
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	/*
	 * In a real application you would probably just use the default system certificate trust store and call:
	 *     SSL_CTX_set_default_verify_paths(ctx);
	 * In this demo though we are using a self-signed certificate, so the client must trust it directly.
	 */
	if (!SSL_CTX_load_verify_locations(ctx, "cert.pem", NULL)) {
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

	/* used by getline relying on realloc, can't be statically allocated */
	char *txbuf = NULL;
	size_t txcap = 0;
	int txlen;

	char rxbuf[128];
	size_t rxcap = sizeof(rxbuf);
	int rxlen;

	char *rem_server_ip = NULL;

	struct sockaddr_in addr, cliaddr;
	unsigned int addr_len = sizeof(addr);

	/* Splash */
	printf("\nsslecho : Simple Echo Client/Server (OpenSSL 3.0.1-dev) : %s : %s\n\n", __DATE__,
			__TIME__);

	bool isServer = false;
	/* If client get remote server address (could be 127.0.0.1) */
	if (!isServer) {
		if (argc < 2) {
			printf("usage: %s <server_ip>\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		rem_server_ip = argv[1];
	}

	/* Create context used by both client and server */
	ssl_ctx = create_context(isServer);

	printf("We are the client\n\n");
	/* Configure client context so we verify the server correctly */
	configure_client_context(ssl_ctx);

	/* Create "bare" socket */
	sockfd = create_client_socket();
	/* Set up connect address */
	addr.sin_family = AF_INET;
	inet_pton(AF_INET, rem_server_ip, &addr.sin_addr.s_addr);
	addr.sin_port = htons(server_port);

	/* Create client SSL structure using dedicated client socket */
	ssl = SSL_new(ssl_ctx);
	bio = BIO_new_dgram(sockfd, BIO_CLOSE);
	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)))
		perror("connect");
	BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &addr);
	SSL_set_bio(ssl, bio, bio);

	int retval = SSL_connect(ssl);
	if (retval <= 0) {
		switch (SSL_get_error(ssl, retval)) {
			case SSL_ERROR_ZERO_RETURN:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_ZERO_RETURN\n");
				break;
			case SSL_ERROR_WANT_READ:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_WANT_READ\n");
				break;
			case SSL_ERROR_WANT_WRITE:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_WANT_WRITE\n");
				break;
			case SSL_ERROR_WANT_CONNECT:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_WANT_CONNECT\n");
				break;
			case SSL_ERROR_WANT_ACCEPT:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_WANT_ACCEPT\n");
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_WANT_X509_LOOKUP\n");
				break;
			case SSL_ERROR_SYSCALL:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_SYSCALL\n");
				perror("socket i/o");
				break;
			case SSL_ERROR_SSL:
				fprintf(stderr, "SSL_connect failed with SSL_ERROR_SSL\n");
				break;
			default:
				fprintf(stderr, "SSL_connect failed with unknown error\n");
				break;
		}
		exit(EXIT_FAILURE);
	}


	/* Loop to send input from keyboard */
	while (true) {
		/* Get a line of input */
		txlen = getline(&txbuf, &txcap, stdin);
		/* Exit loop on error */
		if (txlen < 0 || txbuf == NULL) {
			break;
		}
		/* Exit loop if just a carriage return */
		if (txbuf[0] == '\n') {
			break;
		}
		/* Send it to the server */
		if ((result = SSL_write(ssl, txbuf, txlen)) <= 0) {
			printf("Server closed connection\n");
			ERR_print_errors_fp(stderr);
			break;
		}

		/* Wait for the echo */
		rxlen = SSL_read(ssl, rxbuf, rxcap);
		if (rxlen <= 0) {
			printf("Server closed connection\n");
			ERR_print_errors_fp(stderr);
			break;
		} else {
			/* Show it */
			rxbuf[rxlen] = 0;
			printf("Received: %s", rxbuf);
		}
	}
	printf("Client exiting...\n");
exit:
	/* Close up */
	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	SSL_CTX_free(ssl_ctx);

	if (sockfd != -1)
		close(sockfd);

	if (txbuf != NULL && txcap > 0)
		free(txbuf);

	return 0;
}
