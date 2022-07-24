#ifndef COMMON_H
#define COMMON_H 1

#undef PDEBUG
#ifdef DEBUG
#	define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#	define PDEBUG(fmt, args...)	/* not debugging: do nothin */
#endif /* endif DEBUG */

#define MIN(a, b)	((a) < (b) ? (a) : (b))

#define DEFAULT_PORT	6969

#define DEFAULT_PUBKEY	"/etc/lsd/pubkey.pem"
#define DEFAULT_PVTKEY	"/etc/lsd/pvtkey.pem"

#endif /* ifndef COMMON_H */
