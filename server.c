#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <getopt.h>

#include "dtls.h"

#define	DEFAULT_PORT	6969
#define BUFFSIZE	2048
#define RXBUF_SIZE	BUFFSIZE
#define TXBUF_SIZE	BUFFSIZE

static struct {
	int port;
	bool ipv6;
} argopts;

static void parse_args(int *argc, char *argv[]);

int main(int argc, char *argv[])
{
	char rxbuf[RXBUF_SIZE], txbuf[TXBUF_SIZE];
	char addrstr[INET_ADDRSTRLEN];
	int sockfd = -1, rxlen, txlen;
	struct sockaddr_in servaddr, cliaddr;
	bool is_server = true;
	socklen_t addrsize = sizeof(servaddr);
	SSL_CTX *ssl_ctx = NULL;
	SSL *ssl = NULL;
	BIO *bio = NULL;

	parse_args(&argc, argv);

	sockfd = create_server_socket(argopts.port);
	if (sockfd == -1)
		exit(EXIT_FAILURE);

	ssl_ctx = create_ssl_context(is_server);
	configure_server_context(ssl_ctx, "cert.pem", "key.pem");
	printf("lsdd: listening on port %d\n", argopts.port);

	while (true) {
		ssl = SSL_new(ssl_ctx);
		bio = BIO_new_dgram(sockfd, BIO_NOCLOSE);
		SSL_set_bio(ssl, bio, bio);
		SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE);
		while (DTLSv1_listen(ssl, (BIO_ADDR *)&cliaddr) <= 0)
			;
		/* connection established */
		printf("%s:%d connected\n",
			inet_ntop(AF_INET, &cliaddr.sin_addr, addrstr, sizeof(addrstr)),
			ntohs(cliaddr.sin_port));
		while (true) {
			int err = 0;
			if ((rxlen = readline_ssl(ssl, rxbuf, sizeof(rxbuf)-1, &err)) <= 0) {
				if (rxlen == 0)
					puts("client closed connection\n");
				if (err)
					ERR_print_errors_fp(stderr);
				break;
			}
			rxbuf[rxlen] = '\0';
			printf("received:\n%s\n", rxbuf);
			// process message
			// send response
			if (SSL_write(ssl, rxbuf, rxlen) <= 0)
				ERR_print_errors_fp(stderr);
		}
	}
	printf("server exiting...\n");
exit:
	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	SSL_CTX_free(ssl_ctx);
	if (sockfd != -1)
		close(sockfd);
	return 0;
}

static void parse_args(int *argc, char *argv[])
{
	int c;

	/* setting defaults */
	argopts.port = DEFAULT_PORT;

	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"ipv6", no_argument, NULL, '6'}
	};

	while (1) {
		if ((c = getopt_long(*argc, argv, "p:6", long_options, NULL)) == -1)
			break;
		switch (c) {
		case 'p':
			argopts.port = strtol(optarg, NULL, 10);
			if (argopts.port <= 0) {
				puts("invalid port");
				exit(EXIT_FAILURE);
			}
			printf("port=%d\n", argopts.port);
			break;
		case '6':
			argopts.ipv6 = true;
			puts("ipv6");
			break;
		}
	}
}
