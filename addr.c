#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include "addr.h"

// NOTE: create one for windows as well.

int addr_create_array(int family, struct sockaddr *addr, size_t *addrsize, 
		char *ips[], size_t count)
{
	int flag = 0;
	size_t adsize = *addrsize, a;

	for (int i = 0; i < count; ++i) {
		a = adsize;
		if (addr_create(family, &addr[i], &a, ips[i]) == -1)
			return -1;
	}
	/* update addrsize if any change to address size made */
	*addrsize = a;

	return 0;
}

/*
 * addr_create:
 * 	Creates an address structure for IP address given in $ip.
 *	On input, *$addrsize specifies the length of the supplied sockaddr structure,
 *	and on output specified the length of the stored address.
 *	Return -1 on error, and 0 on success.
 */
int addr_create(int family, struct sockaddr *addr, size_t *addrsize, char *ip)
{
	struct addrinfo hints, *res;
	int ret;

	bzero(&hints, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	if ((ret = getaddrinfo(ip, NULL, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo(%s): %s\n", ip, gai_strerror(ret));
		return -1;
	}

	if (res != NULL) {
		memcpy(addr, res->ai_addr, *addrsize);
		*addrsize = sizeof(res->ai_addr);
	}

	freeaddrinfo(res);
	return 0;
}

/*
 * get_bcast:
 * 	Get the broadcast address for interface given in $ifname belonging to
 * 	address family given in $family (AF_INET or AF_INET6). The broadcast address
 * 	is copied into $addr.
 *
 * 	On input *$addrsize specifies the length of the supplied sockaddr structure,
 * 	and on output specifies the length of the stored address. If no broadcast
 * 	address exists for the $ifname, it is set to 0, and -1 is returned.
 *
 * 	Returns -1 on error and 0 on success.
 */
int get_bcast(int family, char *ifname, struct sockaddr *addr, size_t *addrsize)
{
	struct ifaddrs *ifaddr;
	int fam, ret = -1, exists = 0;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		return -1;
	}
	for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
		fam = ifa->ifa_addr->sa_family;
		if (strcmp(ifa->ifa_name, ifname))
			continue;
		exists = 1;	// If we have reached here, the ifname exists
		if (fam == family) {
			if (fam == AF_INET || AF_INET6) {
				if (!ifa->ifa_broadaddr) {
					*addrsize = 0;	/* no bcast for ifname */
					ret = -1;
					break;
				}
				memcpy(addr, ifa->ifa_broadaddr, *addrsize);
				*addrsize = sizeof(ifa->ifa_broadaddr);
				ret = 0;
				break;
			}
		}
	}
	freeifaddrs(ifaddr);
	if (!exists)
		fprintf(stderr, "invalid interface name: %s\n", ifname);
	return ret;
}
