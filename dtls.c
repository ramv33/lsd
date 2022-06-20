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
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static int
app_verify_cookie_cb(SSL *ssl, const unsigned char *cookie, unsigned int cookie_len)
{
	// TODO
	return 1;
}

static int
app_gen_cookie_cb(SSL *ssl, unsigned char *cookie, unsigned int *cookie_len)
{
	// TODO
	for (int i = 0; i < 32; ++i)
		cookie[i] = i;
	*cookie_len = 32;
	return 1;
}

int create_server_socket(unsigned int port)
{
	int s;
	int optval = 1;
	struct sockaddr_in addr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("unable to create socket");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");
	if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror("unable to bind");
		return -1;
	}

	return s;
}

int create_client_socket(bool bcast)
{
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("unable to creat socket");
		return -1;
	}
}

SSL_CTX* create_ssl_context(bool is_server)
{
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	if (is_server)
		method = DTLS_server_method();
	else
		method = DTLS_client_method();

	ctx = SSL_CTX_new(method);
	if (ctx == NULL) {
		perror("Unable to create SSL context");
		ERR_print_errors_fp(stderr);
	}

	/* set cookie callbacks */
	SSL_CTX_set_cookie_generate_cb(ctx, app_gen_cookie_cb);
	SSL_CTX_set_cookie_verify_cb(ctx, app_verify_cookie_cb);

	return ctx;
}

void configure_client_context(SSL_CTX *ctx)
{
	/* abort the handshake if certificate verification fails */
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

void configure_server_context(SSL_CTX *ctx, const char *cert, const char *pvtkey)
{
	/* Set the key and cert */
	if (SSL_CTX_use_certificate_chain_file(ctx, cert) <= 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, pvtkey, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		exit(EXIT_FAILURE);
	}
}

/*
 * readline_ssl:
 * 	Read a line from SSL connection, i.e stops on reading '\n'.
 * 	Reads at most size-1 characters and nul-terminates the string.
 * 	Returns the no: of characters read up to and including the '\n'.
 */
ssize_t readline_ssl(SSL *ssl, char *buf, size_t size, int *err)
{
	ssize_t i;
	char *p = buf;
	int ret;

	for (i = 0; i < (size-1); ++i) {
		ret = SSL_read(ssl, p, 1);
		switch (SSL_get_error(ssl, ret)) {
		case SSL_ERROR_NONE:
		case SSL_ERROR_WANT_READ:
			break;
		case SSL_ERROR_ZERO_RETURN:
			goto out;
		default:
			*err = 1;
			fprintf(stderr, "SSL_read error:");
			ERR_print_errors_fp(stderr);
			goto out;
		}
		if (*p == '\n')
			break;
		++p;
	}
out:
	*p = '\0';
	return p - buf;
}
