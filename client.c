#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdbool.h>
#include <getopt.h>

#include "common.h"
#include "protocol.h"
#include "addr.h"

#define	DEFAULT_PORT	6969	// TODO: move this into a common header file

struct {
	int		port;		/* port number */
	char		*request;	/* request message */
	int		targets_i;	/* index of argv from which target IPs start */
	int32_t		timer;		/* shutdown timer */
	char		*ifname;	/* interface name */
	char		*msg;		/* notification message to send to server */
	int		timeout;	/* timeout while waiting for ack */
	int		ntries;		/* no of times to resend when ack not received */
	bool		ipv6;		/* true if IPv6 */
	bool		broadcast;	/* true if broadcast */
} argopts;

static void parse_args(int *argc, char *argv[]);
static void usage(void);

int create_socket(int domain, bool bcast);
int fill_request(struct request *req);
int send_request(int sockfd, struct request *req, struct sockaddr_in *addrs, size_t count);

int main(int argc, char *argv[])
{
	struct sockaddr_in *addrs;
	size_t addrsize = sizeof(*addrs);
	int sockfd, ret = 0, count;
	char ipstr[INET6_ADDRSTRLEN];
	struct request req;

	parse_args(&argc, argv);
	count = argc - argopts.targets_i;
	PDEBUG("nips = %d\n", count);
	addrs = malloc(sizeof(*addrs) * count);
	if (addr_create_array(AF_INET, (struct sockaddr *)addrs, &addrsize,
			&argv[argopts.targets_i], count) == -1) {
		fprintf(stderr, "address creation failed\n");
		ret = 1;
		goto out;
	}

	for (int i = 0; i < count; ++i) {
		addrs[i].sin_port = htons(argopts.port);
#ifdef DEBUG
		if (inet_ntop(AF_INET, &addrs[i].sin_addr, ipstr, sizeof(ipstr)))
			puts(ipstr);
#endif
	}

	if (fill_request(&req) == -1) {
		ret = 1;
		goto out;
	}
	PDEBUG("struct request\n"
		"==============\n"
		".when = %ld\n.req_type = %x\n"
		".timer = %d\n.msg_size = %d\n"
		".msg = '%s'\n",
		req.when, req.req_type, req.timer, req.msg_size,
		(req.msg_size != 0) ? req.msg : "");
	sockfd = create_socket(AF_INET, argopts.broadcast);
	send_request(sockfd, &req, addrs, count);
out:
	free(addrs);
	return ret;
}

int fill_request(struct request *req)
{
	/* argopts.request is mandatory and is checked in parse_args() */
	if (parse_request(&req->req_type, argopts.request) == -1) {
		fprintf(stderr, "invalid request '%s'", argopts.request);
		return -1;
	}
	if (argopts.timer < 0) {
		fprintf(stderr, "invalid timer value %d\n", argopts.timer);
		return -1;
	}
	req->timer = argopts.timer;
	req->msg_size = 0;
	if (argopts.msg) {
		req->msg_size = strlen(argopts.msg);
		req->msg = argopts.msg;	/* NOTE: not copying */ 
	}
	req->when = time(NULL);
}

int send_request(int sockfd, struct request *req, struct sockaddr_in *addrs, size_t count)
{
	unsigned char *payload;
	size_t payload_size;
	ssize_t ret;
	char ipstr[INET6_ADDRSTRLEN];

	payload = pack_request(req, &payload_size);
	if (!payload) {
		perror("allocating payload failed");
		return -1;
	}
	for (int i = 0; i < count; ++i) {
		ret = sendto(sockfd, payload, payload_size, 0,
				(struct sockaddr *)&addrs[i], sizeof(*addrs));
#		ifdef DEBUG
		if (inet_ntop(AF_INET, &addrs[i].sin_addr, ipstr, sizeof(ipstr)))
			printf("sent payload (%zd bytes) to %s\n", ret, ipstr);

#		endif
		if (ret == -1) {
			perror("error sending payload");
			continue;
		}
	}
	return 0;
}

int create_socket(int domain, bool bcast)
{
	int sockfd;

	sockfd = socket(domain, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("error creating socket");
		return -1;
	}
	if (bcast) {
		int optval = 1, ret;
		ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
		if (ret == -1) {
			perror("setsockopt: cannot set socket as broadcast");
			close(sockfd);
			return -1;
		}
	}
	return sockfd;
}

static void parse_args(int *argc, char *argv[])
{
	int c;

	argopts.port = DEFAULT_PORT;
	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"timeout", required_argument, NULL, 't'},
		{"timer", required_argument, NULL, 'T'},
		{"tries", required_argument, NULL, 'n'},
		{"request", required_argument, NULL, 'r'},
		{"interface", required_argument, NULL, 'i'},
		{"message", required_argument, NULL, 'm'},
		{"broadcast", no_argument, NULL, 'b'},
		{"ipv6", no_argument, NULL, '6'},
		{NULL, 0, NULL, 0}
	};
	while (1) {
		if ((c = getopt_long(*argc, argv, "p:t:T:n:r:i:m:b6", long_options, NULL))
				== -1)
			break;
		switch (c) {
		case 'p':
			argopts.port = strtol(optarg, NULL, 10);
			if (argopts.port <= 0) {
				fprintf(stderr, "invalid port");
				exit(EXIT_FAILURE);
			}
			PDEBUG("port=%d\n", argopts.port);
			break;
		case 't':
			argopts.timer = strtol(optarg, NULL, 10);
			PDEBUG("timer=%d\n", argopts.timer);
			if (argopts.timer < 0) {
				fprintf(stderr, "invalid timer value, should be >= 0\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'T':
			argopts.timeout = strtol(optarg, NULL, 10);
			PDEBUG("timeout=%d\n", argopts.timeout);
			if (argopts.timeout <= 0) {
				fprintf(stderr, "invalid timeout value, should be > 0\n");
				exit(EXIT_FAILURE);
			}
			break;
		case 'n':
			argopts.ntries = strtol(optarg, NULL, 10);
			PDEBUG("ntries=%d\n", argopts.ntries);
			break;
		case 'r':
			argopts.request = optarg;
			PDEBUG("request='%s'\n", argopts.request);
			break;
		case 'i':
			argopts.ifname = optarg;
			PDEBUG("ifname='%s'\n", argopts.ifname);
			break;
		case 'm':
			argopts.msg = optarg;
			/* NOTE: not copying */
			PDEBUG("message='%s'\n", argopts.msg);
			break;
		case 'b':
			argopts.broadcast = true;
			PDEBUG("broadcast");
			break;
		case '6':
			argopts.ipv6 = true;
			PDEBUG("ipv6");
			break;
		}
	}
	if (argopts.broadcast && !argopts.ifname) {
		fprintf(stderr, "ifname required if broadcast\n");
		exit(EXIT_FAILURE);
	}
	/* get IP(s) */
	if (optind < *argc) {
		argopts.targets_i = optind;	// optind is index of first non-option argument
	} else {
		fprintf(stderr, "usage error: destination address required\n");
		usage();
		/* usage exits from program */
	}
	if (!argopts.request) {
		fprintf(stderr, "usage error: -r argument is mandatory\n");
		usage();
	}
}

void usage(void)
{
	puts("TODO: usage");
	exit(EXIT_FAILURE);
}
