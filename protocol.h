#ifndef LSDPROTO_H
#define	LSDPROTO_H 1

#include <stdint.h>

/* signature structure */
struct signature {
	int16_t		sigsize;
	unsigned char	sig[192];
};

/*
 * This structure is used to send the request to the server by the client.
 */
struct request {
	int64_t		when;		/* time client sent the request */
	int32_t		timer;		/* timer for power commands */
	uint16_t	req_type;	/* request type */
	int16_t		msg_size;
	unsigned char	*msg;		/* optional nul-terminated string */
	struct signature sig;
};

struct sstate {
	int64_t		when;		/* when the power command was scheduled */
	int64_t		issued_at;
	int32_t		timer;		/* timer for power command */
	uint16_t	powcmd;		/* type of scheduled power command */
	uint16_t	ack;
};

/*
 * client requests
 */

/* power commands */
#define	REQ_POW_SHUTDOWN	0x0001
#define	REQ_POW_REBOOT		0x0002
#define	REQ_POW_STANDBY		0x0003
#define	REQ_POW_SLEEP		0x0004
#define	REQ_POW_HIBERNATE	0x0005
#define REQ_POW_ABORT		0x0006
/* send notification to desktop */
#define	REQ_NOTIFY		0x0007
/* query commands */
#define	REQ_QUERY		0x0008	/* get shutdown timer on server */

#define SET_FORCE_BIT(reqtype)		((reqtype) = ((1 << 15) | (reqtype)))
#define RESET_FORCE_BIT(reqtype)	((reqtype) = (~(1 << 15) & (reqtype)))
#define GET_FORCE_BIT(reqtype)		((reqtype) & (1 << 15))
/*
 * server sstates
 */
#define	ACK_GRANTED		0x0000
#define	ACK_DENIED		0x0001
#define ACK_DISABLED		0x0002		/* request is disabled in server config */

#define MSG_MAXSIZE		128

/* parse_request:	Store request code in *$reqtype */
int parse_request(uint16_t *reqtype, char *reqstr);

/*
 * pack_request:
 * 	Pack request structure into a character array and return a pointer to it.
 * 	The character array is dynamically allocated and has to be freed by the caller.
 * 	$size  is set to the size of the array.
 */
unsigned char *pack_request(struct request *req, size_t *size);

/*
 * sign_request:
 * 	Sign request packed into $buf using $keyfile as private key, and store the
 * 	signature in $size. Update $sigsize to the size of the signature.
 */
unsigned char *sign_request(unsigned char *buf, size_t *bufsize, size_t *sigsize,
		const char *keyfile);

/*
 * unpack_signature:
 * 	Unpack signature part from $buf into $sig.
 */
void unpack_signature(struct signature *sig, unsigned char *buf);

/*
 * unpack_request_fixed:
 * 	Unpack fixed part of request structure from character array into the given struct.
 *	NOTE: reqbuf does not include the message buffer. It has to be read from the
 *	connection based on the msg_size field.
 */
unsigned char *unpack_request_fixed(struct request *req, unsigned char *reqbuf);

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
