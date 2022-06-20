#ifndef LSDPROTO_H
#define	LSDPROTO_H 1

#include <stdint.h>

/*
 * This structure is used to send the request to the server by the client.
 */
struct request {
	int64_t		when;		/* time client sent the request */
	uint32_t	req_type;
	int32_t		timer;		/* timer for power commands */
	int16_t		msg_size;
	unsigned char	*msg;		/* optional nul-terminated string */
};

struct sstate {
	int64_t		when;		/* when the power command was scheduled */
	uint32_t	powcmd;		/* type of scheduled power command */
	uint32_t	timer;		/* timer for power command */
};

/*
 * client requests
 */

/* power commands */
#define	REQ_POW_SHUTDOWN	0x00000001
#define	REQ_POW_REBOOT		0x00000002
#define	REQ_POW_STANDBY		0x00000003
#define	REQ_POW_SLEEP		0x00000004
#define	REQ_POW_HIBERNATE	0x00000005
/* send notification to desktop */
#define	REQ_NOTIFY		0x00000006
/* query commands */
#define	REQ_QUERY		0x00000007	/* get shutdown timer on server */

/*
 * server sstates
 */
#define	RES_GRANTED		0x00000008
#define	RES_DENIED		0x00000009
#define RES_DISABLED		0x0000000a	/* request is disabled in server config */

/* parse_request:	Store request code in *$reqtype */
int parse_request(uint32_t *reqtype, char *reqstr);

/*
 * pack_request:
 * 	Pack request structure into a character array and return a pointer to it.
 * 	The character array is dynamically allocated and has to be freed by the caller.
 * 	$size  is set to the size of the array.
 */
unsigned char *pack_request(struct request *req, size_t *size);

/*
 * unpack_request_fixed:
 * 	Unpack fixed part of request structure from character array into the given struct.
 *	NOTE: reqbuf does not include the message buffer. It has to be read from the
 *	connection based on the msg_size field.
 */
void unpack_request_fixed(struct request *req, unsigned char *reqbuf);

/*
 * pack_sstate:
 * 	Pack sstate structure into character array. Since the sstate structure
 * 	contains only fixed size members, the required size of the buffer is known
 * 	and is the sum of the members. If $size is less than this value, resbuf is
 * 	untouched.
 */
void pack_sstate(struct sstate *res, char resbuf[], size_t size);

/*
 * unpack_sstate:
 *	Unpack sstate structure from character array into the given struct.
 */
void unpack_sstate(struct sstate *res, char *resbuf);

/*
 * request_struct_fixedsize:
 * 	Return fixed size of request struct, i.e excluding the msg buffer
 */
size_t request_struct_fixedsize(void);

/*
 * sstate_struct_size:
 * 	Return size of sstate struct
 */
size_t sstate_struct_size(void);

#define	SSTATE_SIZE		sstate_struct_size()
#define REQUEST_FIXED_SIZE	request_struct_fixedsize()

#endif	/* ifndef LSDPROTO_H */
