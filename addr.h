#ifndef ADDR_H
#define ADDR_H 1

#include <sys/types.h>
#include <sys/socket.h>

/*
 * addr_create:
 * 	Creates an address structure for IP address given in $ip.
 *	On input, *$addrsize specifies the length of the supplied sockaddr structure,
 *	and on output specified the length of the stored address.
 *	Return -1 on error, and 0 on success.
 */
int addr_create(int family, struct sockaddr *addr, size_t *addrsize, char *ip);

int addr_create_array(int family, struct sockaddr *arr, size_t *addrsize,
		char *ips[], size_t count);

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
int get_bcast(int family, char *ifname, struct sockaddr *addr, size_t *addrsize);


#endif /* ifndef ADDR_H */
