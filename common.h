#ifndef COMMON_H
#define COMMON_H 1

#undef PDEBUG
#ifdef DEBUG
#	define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#	define PDEBUG(fmt, args...)	/* not debugging: do nothin */
#endif /* endif DEBUG */

#endif /* ifndef COMMON_H */
