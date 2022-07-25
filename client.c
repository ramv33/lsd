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

#define DEFAULT_PORT	6969	// TODO: move this into a common header file
#define DEFAULT_TIMER	5

struct {
	int		port;		/* port number */
	char		*request;	/* request message */
	int		targets_i;	/* index of argv from which target IPs start */
	int32_t		timer;		/* shutdown timer */
	char		*ifname;	/* interface name */
	char		*msg;		/* notification message to send to server */
	char		*pvtkey;	/* private key */
	int		timeout;	/* timeout while waiting for ack */
	int		ntries;		/* no of times to resend when ack not received */
	int		broadcast;	/* 1 if broadcast, else 0 */
	bool		verbose;	/* talk more */
	bool		ipv6;		/* true if IPv6 */
	bool		force;		/* force action, do not wait for user input */
} argopts;

static void parse_args(int *argc, char *argv[]);
static void usage(char *pgmname);

int create_socket(int domain, bool bcast);
int fill_request(struct request *req);
int send_request(int sockfd, struct request *req, struct sockaddr_in *addrs, size_t num_ips);

int main(int argc, char *argv[])
{
	struct sockaddr_in *addrs;
	size_t addrsize = sizeof(*addrs);
	int sockfd, ret = 0, num_ips;
	char ipstr[INET6_ADDRSTRLEN];
	struct request req;

	parse_args(&argc, argv);

	num_ips = argc - argopts.targets_i;
	PDEBUG("nips = %d\n", num_ips);

	if (argopts.broadcast)
		printfv("broadcast enabled.\n", num_ips);
	addrs = malloc(sizeof(*addrs) * (num_ips + argopts.broadcast));

	if (argopts.broadcast) {
		ret = get_bcast(AF_INET, argopts.ifname, (struct sockaddr *)addrs, &addrsize);
		if (ret == -1) {
			if (!addrsize)
				printf("no bcast for iface: %s\n", argopts.ifname);
			goto out;
		}
	}
	/* If bcast was also specified, the bcast address is in addrs[0], and we start from addrs[1] */
	if (addr_create_array(AF_INET, (struct sockaddr *)addrs + argopts.broadcast, &addrsize,
				&argv[argopts.targets_i], num_ips) == -1) {
		fprintf(stderr, "address creation failed\n");
		ret = 1;
		goto out;
	}

	for (int i = 0; i < num_ips + argopts.broadcast; ++i) {
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
	printfv("timestamp = %ld\n"
		"pvtkey    = '%s'\n",
		req.when, argopts.pvtkey);
	sockfd = create_socket(AF_INET, argopts.broadcast);
	send_request(sockfd, &req, addrs, num_ips + argopts.broadcast);
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
	if (argopts.force) {
		SET_FORCE_BIT(req->req_type);
		printfv("set force bit: req_type = %x\n", req->req_type);
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

int send_request(int sockfd, struct request *req, struct sockaddr_in *addrs, size_t num_ips)
{
	unsigned char *payload;
	size_t payload_size, sigsize;
	ssize_t ret;
	char ipstr[INET6_ADDRSTRLEN];

	payload = pack_request(req, &payload_size);
	if (!payload) {
		perror("allocating payload failed");
		return -1;
	}
	/* sign message */
	if (!sign_request(payload, &payload_size, &sigsize, argopts.pvtkey)) {
		fprintf(stderr, "error signing request\n");
		return -1;
	}

	for (int i = 0; i < num_ips; ++i) {
		ret = sendto(sockfd, payload, payload_size, 0,
				(struct sockaddr *)&addrs[i], sizeof(*addrs));
		if (inet_ntop(AF_INET, &addrs[i].sin_addr, ipstr, sizeof(ipstr)))
			printf("sent payload (%zd bytes) to %s\n", ret, ipstr);

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

	argopts.timer = DEFAULT_TIMER;
	argopts.port = DEFAULT_PORT;
	argopts.pvtkey = DEFAULT_PVTKEY;
	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"key", required_argument, NULL, 'k'},
		{"timer", required_argument, NULL, 't'},
		{"timeout", required_argument, NULL, 'T'},
		{"tries", required_argument, NULL, 'n'},
		{"request", required_argument, NULL, 'r'},
		{"interface", required_argument, NULL, 'i'},
		{"message", required_argument, NULL, 'm'},
		{"timeout", required_argument, NULL, 'T'},
		{"broadcast", no_argument, NULL, 'b'},
		{"ipv6", no_argument, NULL, '6'},
		{NULL, 0, NULL, 0}
	};
	while (1) {
		if ((c = getopt_long(*argc, argv, "vp:k:t:T:n:r:i:m:bf6", long_options, NULL))
				== -1)
			break;
		switch (c) {
		case 'v':
			argopts.verbose = true;
			PDEBUG("verbose\n");
			break;
		case 'p':
			argopts.port = strtol(optarg, NULL, 10);
			if (argopts.port <= 0) {
				fprintf(stderr, "invalid port");
				exit(EXIT_FAILURE);
			}
			PDEBUG("port=%d\n", argopts.port);
			break;
		case 'k':
			argopts.pvtkey = optarg;
			PDEBUG("pvtkey='%s'\n", argopts.pvtkey);
			printfv("pvtkey='%s'\n", argopts.pvtkey);
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
			printfv("request='%s'\n", argopts.request);
			break;
		case 'i':
			argopts.ifname = optarg;
			PDEBUG("ifname='%s'\n", argopts.ifname);
			break;
		case 'm':
			argopts.msg = optarg;
			/* NOTE: not copying */
			PDEBUG("message='%s'\n", argopts.msg);
			printfv("message='%s'\n", argopts.msg);
			break;
		case 'b':
			argopts.broadcast = 1;
			PDEBUG("broadcast\n");
			break;
		case 'f':
			argopts.force = true;
			PDEBUG("force\n");
			break;
		case '6':
			argopts.ipv6 = true;
			PDEBUG("ipv6\n");
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
		argopts.targets_i = optind;
		if (!argopts.broadcast) {
			fprintf(stderr, "usage error: destination address required\n");
			usage(argv[0]);
			/* usage exits from program */
		}
	}
	if (!argopts.request) {
		fprintf(stderr, "usage error: -r argument is mandatory\n");
		usage(argv[0]);
	}
}

void usage(char *pgmname)
{
	printf(
	"\nUsage: %s [options] ip(s)\n\n"
	"-p, --port=PORT           specify port number of daemon on server\n"
	"\n"
	"-t, --timer=SECONDS       when to schedule command\n"
	"\n"
	"-r, --request=REQ         specify the request to send to server; valid options are\n"
	"                          shutdown, reboot, standby, hibernate, sleep, abort, notify, query\n"
	"\n"
	"-b, --broadcast           broadcast request on network out of given interface\n"
	"                          NOTE: interface must be specified (-i) when using this flag\n"
	"\n"
	"-i, --interface=IFNAME    specify network interface to use for sending broadcast message\n"
	"\n"
	"-m, --message=MSG         message to send for notification on server\n\n"
	, pgmname);
	exit(EXIT_FAILURE);
}
