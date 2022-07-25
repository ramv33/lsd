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
#include "power.h"
#include "daemon.h"
#include "auth.h"
#include "notif.h"

#define BUFFSIZE	2048
#define RXBUF_SIZE	BUFFSIZE
#define TXBUF_SIZE	BUFFSIZE

static struct {
	int port;
	char *pubkey;
	bool ipv6;
} argopts;

/* server state showing info about pending power commands */
struct sstate state;

static void parse_args(int *argc, char *argv[]);

int create_socket(int domain, int port);
int receive_requests(int sockfd);
int handle_request(struct request *req);

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
	receive_requests(sockfd);
	printf("server exiting...\n");
out:
	close(sockfd);
	return 0;
}

int receive_requests(int sockfd)
{
	char rxbuf[RXBUF_SIZE], txbuf[TXBUF_SIZE], *rp;
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
		/* rp points past the fixed part, i.e to the message part */
		rp = unpack_request_fixed(&req, rxbuf);
		PDEBUG("request\n=======\n"
			"when = %ld\ntimer=%d\nreq_type=%x\nmsg_size = %d\n",
			req.when, req.timer, req.req_type, req.msg_size);
		/* receive message */
		if (req.msg_size > 0) {
			req.msg = strdup(rp);
			PDEBUG("msg = '%s'\n", req.msg);
		} else {
			req.msg = NULL;
		}
		rp += req.msg_size;
		unpack_signature(&req.sig, rp);
		size_t sigsize = req.sig.sigsize;
		if (!verifysig(argopts.pubkey, rxbuf, REQUEST_FIXED_SIZE+req.msg_size,
				req.sig.sig, &sigsize)) {
			printf("client verification failed!\n");
			printf("discarding request\n");
			goto end;
		}
		handle_request(&req);
	end:
		free(req.msg);
	}
}

/*
 * handle_request:
 * 	0 on success.
 * 	-1 on invalid request or error scheduling command.
 * 	-2 if request too old.
 */
int handle_request(struct request *req)
{
	int scheduled = 0;
	uint16_t req_type;

	if (req->when <= state.when) {
		fprintf(stderr, "old request... ignoring\n");
		return -2;
	}

	/* unset force bit for switch case */
	req_type = req->req_type;
	if (GET_FORCE_BIT(req_type))
		PDEBUG("[-] force bit set\n");
	RESET_FORCE_BIT(req_type);
	/* call power_schedule if it is a power command, else call notify or send state */
	switch (req_type) {
	case REQ_POW_SHUTDOWN:
	case REQ_POW_REBOOT:
	case REQ_POW_STANDBY:
	case REQ_POW_SLEEP:
	case REQ_POW_HIBERNATE:
		scheduled = power_schedule(req, &state);
		break;
	case REQ_POW_ABORT:
		power_abort();
		break;
	case REQ_NOTIFY:
		PDEBUG("notify\n");
		send_notification(req);
		/* send notification to user (req->msg) */
		return 0;
	case REQ_QUERY:
		PDEBUG("query\n");
		PDEBUG("sstate\n=====\n"
			".when = %ld\n.issued_at = %ld\n"
			".timer = %d\n.powcmd = %x\n\n",
			state.when, state.issued_at, state.timer, state.powcmd);
		/* send state to client */
		break;
	default:
		fprintf(stderr, "invalid request type %x, ignoring...\n", req->req_type);
		return -1;
	}
	if (scheduled) {
		state.when = req->when;
		state.powcmd = req->req_type;	// do this only if power command, in switch. 
		state.timer = req->timer;
		PDEBUG("message: '%s'\n", (req->msg_size > 0 ? req->msg : ""));
		/* schedule power command and return 0 on success
		 * power_schedule(&req, &state).
		 * schedules the command sets server state in state (issued_at). */

		PDEBUG("state\n======\n"
			".when = %ld\n.issued_at = %ld\n.powcmd = %x\n .timer = %d\n", 
			state.when, state.issued_at, state.powcmd, state.timer);
	}
	return 0;
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
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
	argopts.pubkey = DEFAULT_PUBKEY;

	static struct option long_options[] = {
		{"port", required_argument, NULL, 'p'},
		{"pubkey", required_argument, NULL, 'k'},
		{"ipv6", no_argument, NULL, '6'}
	};

	while (1) {
		if ((c = getopt_long(*argc, argv, "p:k:6", long_options, NULL)) == -1)
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
		case 'k':
			argopts.pubkey = optarg;
			printf("pubkey='%s'\n", argopts.pubkey);
			break;
		case '6':
			argopts.ipv6 = true;
			puts("ipv6");
			break;
		}
	}
}
