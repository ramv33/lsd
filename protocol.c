#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "protocol.h"
#include "auth.h"

static unsigned char *pack_int64(unsigned char buf[8], uint64_t num)
{
	unsigned char *p = buf;

	for (int i = 56; i >= 0; i -= 8) {
		*p = num >> i;
		++p;
	}
	return p;
}

static unsigned char *pack_int32(unsigned char buf[4], uint32_t num)
{
	buf[0] = num >> 24;
	buf[1] = num >> 16;
	buf[2] = num >> 8;
	buf[3] = num;
	return &buf[4];
}

static unsigned char *pack_int16(unsigned char buf[2], uint16_t num)
{
	buf[0] = num >> 8;
	buf[1] = num;
	return &buf[2];
}

static unsigned char *unpack_int64(unsigned char buf[8], uint64_t *num)
{
	*num = 0;
	unsigned char *p = buf;

	for (int i = 56; i >= 0; i -= 8)
		*num |= *p++ << i;
	return p;
}

static unsigned char *unpack_int32(unsigned char buf[4], uint32_t *num)
{
	*num = 0;

	*num |= buf[0] << 24;
	*num |= buf[1] << 16;
	*num |= buf[2] << 8;
	*num |= buf[3];
	return &buf[4];
}

static unsigned char *unpack_int16(unsigned char buf[2], uint16_t *num)
{
	*num = 0;

	*num |= buf[0] << 8;
	*num |= buf[1];
	return &buf[2];
}

/*
 * struct request {
 * 	when;
 * 	timer;
 * 	req_type;
 * 	msg_size;
 * 	*msg;		// variable part
 * 	struct signature sig;
 * };
 */
size_t request_struct_fixedsize(void)
{
	struct request r;

	return sizeof(r.when) + sizeof(r.req_type) + sizeof(r.timer)
		+ sizeof(r.msg_size);
}

/*
 * struct sstate {
 * 	when;
 * 	issued_at;
 * 	timer;
 * 	powcmd;
 * 	ack;
 * }
 */
size_t sstate_struct_size(void)
{
	struct sstate s;

	return sizeof(s.when) + sizeof(s.issued_at) + sizeof(s.powcmd) + sizeof(s.timer)
		+ sizeof(s.ack);
}
/*
 * pack_request:
 * 	Pack request structure into a character array and return a pointer to it.
 * 	The character array is dynamically allocated and has to be freed by the caller.
 * 	$size  is set to the size of the array. The signature is NOT set here, it has
 * 	to be manually set 
 *
 * 	struct request {
 * 		uint64_t	when;
 * 		int32_t		timer;
 * 		uint16_t	req_type;
 * 		int16_t		msg_size;
 * 		char		*msg;
 * 		struct signature sig;
 * 	}
 */
unsigned char *pack_request(struct request *req, size_t *size)
{
	/* size needed for buffer is the fixed size + size of message */
	unsigned char *buf, *ret;
	unsigned char sig[192];
	size_t msg_size, sigsize;

	/* limit on message size */
	msg_size = MIN(req->msg_size, MSG_MAXSIZE);
	*size = request_struct_fixedsize() + msg_size;
	buf = malloc(*size + sizeof(struct signature));
	if (buf == NULL) {
		*size = 0;
		return NULL;
	}
	ret = buf;
	/* pack_* returns the next address in the buffer after packing */
	buf = pack_int64(buf, req->when);
	buf = pack_int32(buf, req->timer);
	buf = pack_int16(buf, req->req_type);
	buf = pack_int16(buf, msg_size);
	if (req->msg_size > 0)
		strncpy(buf, req->msg, req->msg_size+1);

	return ret;
}

unsigned char *sign_request(unsigned char *buf, size_t *bufsize, size_t *sigsize,
		const char *keyfile)
{
	unsigned char sig[192];	// temporarily hold signature in buffer
	int16_t size;

	if (signbuf(keyfile, buf, *bufsize, (unsigned char **)&sig, sigsize) == -1)
		return NULL;
	size = *sigsize;
	unsigned char *p = buf;
	buf = pack_int16(buf+*bufsize, size);
	*bufsize += buf - (p+*bufsize);	// increment size after packing int16, could just add 2
	memcpy(buf, sig, *sigsize);
	*bufsize += *sigsize;

	return buf;
}

void unpack_signature(struct signature *sig, unsigned char *buf)
{
	int16_t sigsize = 0;

	buf = unpack_int16(buf, &sigsize);
	sig->sigsize = sigsize;
	memcpy(sig->sig, buf, sigsize);
}

/*
 * unpack_request_fixed:
 * 	Unpack request from character buffer into request structure.
 * 	Returns a pointer past the end of the fixed part, i.e a pointer to
 * 	where the message part should be.
 *
 * 	NOTE: It only unpacks the fixed part, since we do not know how much
 * 	to read from the connection for the message string. That has to be
 * 	determined from the req->msg_size after unpacking.
 */
unsigned char *unpack_request_fixed(struct request *req, unsigned char *reqbuf)
{
	/* unpack_* returns the next address in the buffer after unpacking */
	reqbuf = unpack_int64(reqbuf, &req->when);
	reqbuf = unpack_int32(reqbuf, &req->timer);
	reqbuf = unpack_int16(reqbuf, &req->req_type);
	reqbuf = unpack_int16(reqbuf, &req->msg_size);
	return reqbuf;
}

/*
 * struct sstate contains the following fields:
 * 	int64_t		when;
 * 	int32_t		timer;
 * 	uint16_t	powcmd;
 */
void pack_sstate(struct sstate *res, char resbuf[], size_t size)
{
	size_t s = sstate_struct_size();
	if (size < s)
		return;
	resbuf = pack_int64(resbuf, res->when);
	resbuf = pack_int64(resbuf, res->issued_at);
	resbuf = pack_int32(resbuf, res->timer);
	resbuf = pack_int16(resbuf, res->powcmd);
	resbuf = pack_int16(resbuf, res->ack);
}

void unpack_sstate(struct sstate *res, char resbuf[])
{
	resbuf = unpack_int64(resbuf, &res->when);
	resbuf = unpack_int64(resbuf, &res->issued_at);
	resbuf = unpack_int32(resbuf, &res->timer);
	resbuf = unpack_int16(resbuf, &res->powcmd);
	resbuf = unpack_int16(resbuf, &res->ack);
}

int parse_request(uint16_t *reqtype, char *reqstr)
{
	if (!strcasecmp("SHUTDOWN", reqstr))
		*reqtype = REQ_POW_SHUTDOWN;
	else if (!strcasecmp("REBOOT", reqstr))
		*reqtype = REQ_POW_REBOOT;
	else if (!strcasecmp("STANDBY", reqstr))
		*reqtype = REQ_POW_STANDBY;
	else if (!strcasecmp("SLEEP", reqstr))
		*reqtype = REQ_POW_SLEEP;
	else if (!strcasecmp("HIBERNATE", reqstr))
		*reqtype = REQ_POW_HIBERNATE;
	else if (!strcasecmp("ABORT", reqstr))
		*reqtype = REQ_POW_ABORT;
	else if (!strcasecmp("NOTIFY", reqstr))
		*reqtype = REQ_NOTIFY;
	else if (!strcasecmp("QUERY", reqstr))
		*reqtype = REQ_QUERY;
	else
		return -1;
}

char *reqstr(uint16_t reqtype, char *str, size_t size)
{
	switch (reqtype) {
	case REQ_POW_SHUTDOWN:
		strncpy(str, "shutdown", size);
		break;
	case REQ_POW_REBOOT:
		strncpy(str, "reboot", size);
		break;
	case REQ_POW_SLEEP:
		strncpy(str, "sleep", size);
		break;
	case REQ_POW_STANDBY:
		strncpy(str, "standby", size);
		break;
	case REQ_POW_HIBERNATE:
		strncpy(str, "hibernate", size);
		break;
	default:
		return NULL;
	}
	return str;
}
