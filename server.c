#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <getopt.h>

#include "common.h"
#include "protocol.h"

#define BUFFSIZE	2048
#define RXBUF_SIZE	BUFFSIZE
#define TXBUF_SIZE	BUFFSIZE

static struct {
	int port;
	bool ipv6;
} argopts;

static void parse_args(int *argc, char *argv[]);

int create_socket(int domain, int port);
int process_loop(int sockfd);

int main(int argc, char *argv[])
{
	int sockfd = -1, rxlen, txlen;
	bool is_server = true;
	ssize_t ret;

	parse_args(&argc, argv);

	sockfd = create_socket(AF_INET, argopts.port);
	if (sockfd == -1)
		exit(EXIT_FAILURE);

	printf("lsdd: listening on port %d\n", argopts.port);
	process_loop(sockfd);
	printf("server exiting...\n");
out:
	close(sockfd);
	return 0;
}

int process_loop(int sockfd)
{
	char rxbuf[RXBUF_SIZE], txbuf[TXBUF_SIZE];
	char addrstr[INET_ADDRSTRLEN];
	ssize_t ret;
	struct sockaddr_in cliaddr;
	struct request req;
	socklen_t addrsize = sizeof(cliaddr);

	while (true) {
		/* receive fixed part of request first */
		ret = recvfrom(sockfd, rxbuf, sizeof(rxbuf), 0,
				(struct sockaddr *)&cliaddr, &addrsize);
		if (ret < 0) {
			perror("recvfrom error");
			continue;
		}
		PDEBUG("received %zd bytes from %s:%d\n", ret,
			inet_ntop(AF_INET, &cliaddr.sin_addr, addrstr, sizeof(addrstr)),
			ntohs(cliaddr.sin_port));
		unpack_request_fixed(&req, rxbuf);
		PDEBUG("request\n=======\n"
			"when = %ld\ntimer=%d\nreq_type=%x\nmsg_size = %d\n",
			req.when, req.timer, req.req_type, req.msg_size);
		/* receive message */
		if (req.msg_size > 0) {
			req.msg = strdup(rxbuf+REQUEST_FIXED_SIZE);
			PDEBUG("msg = '%s'\n", req.msg);
		}
	}
}

int create_socket(int domain, int port)
{
	int s, optval = 1;
	struct sockaddr_in addr;	// supports only ipv4 right now

	s = socket(domain, SOCK_DGRAM, 0);
	if (s == -1) {
		perror("error creating socket");
		return -1;
	}

	addr.sin_family = domain;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
		perror("setsockopt(SO_REUSEPORT) failed");
		fprintf(stderr, "continuing..\n");
	}
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind error");
		close(s);
		return -1;
	}

	return s;
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
