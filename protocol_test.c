#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "protocol.h"
#include "auth.h"

int sstate_pack_unpack_test(void);
int request_pack_unpack_test(void);

int
main(void)
{
	int ret = 0;

	if (request_pack_unpack_test()) {
		puts("request_pack_unpack: PASSED");
	} else {
		puts("request_pack_unpack: FAILED");
		ret = 1;
	}

	printf("sstate_pack_unpack: ");
	if (sstate_pack_unpack_test()) {
		puts("PASSED");
	} else {
		puts("FAILED");
		ret = 1;
	}

	return ret;
}

int request_pack_unpack_test(void)
{
	char before[256], after[256];
	size_t size;
	time_t t = time(NULL);
	struct request req = {
		.when = t,
		.req_type = 0xdead,
		.timer	= 0x10101010,
		.msg_size = 6,
		.msg = "hello"
	};

	/* signature stuff */
	unsigned char sig[192];
	size_t sigsize = 0;

	sprintf(before, "%lld %x %x %d",
		req.when, req.req_type, req.timer, req.msg_size);
	char *reqbuf = pack_request(&req, &size);
	sign_request(reqbuf, size, &sigsize, "pvtkey.pem");


	PDEBUG("REQUEST\n=======\n");
	PDEBUG("\nwhen = %lld\n"
		"req_type = %x\n"
		"timer = %x\n"
		"msg_size = %d\n",
		req.when, req.req_type, req.timer, req.msg_size);

	printf("SIGNATURE INFO\n===========\n"
		"sigsize = %zd\nsignature:\n", sigsize);
	for (int i = 0; i < sigsize; ++i)
		printf("%02hhx", reqbuf[size+i]);
	puts("");

	for (int i = 0; i < size; ++i)
		PDEBUG("%02hhx ", reqbuf[i]);
	PDEBUG("\n=========\n");

	char *rp = unpack_request_fixed(&req, reqbuf);
	req.msg = strdup(rp);		// copy message string
	rp += strlen(req.msg)+1;	// move past message string
	unpack_signature(&req.sig, rp);
	if (verifysig("pubkey.pem", reqbuf, size, req.sig.sig, &sigsize))
		printf("verification successful!!!\n");
	sprintf(after, "%lld %x %x %d",
		req.when, req.req_type, req.timer, req.msg_size);

	free(reqbuf);

	return !strcmp(before, after);
}

int sstate_pack_unpack_test(void)
{
	char sbuf[SSTATE_SIZE];
	char before[256], after[256];
	struct sstate s = {
		.when = time(NULL),
		.issued_at = time(NULL),
		.powcmd = 0xdead,
		.timer = 0x12345678,
		.ack = ACK_GRANTED
	};

	sprintf(before, "%ld %ld %x %x %x", s.when, s.issued_at, s.powcmd, s.timer, s.ack);
	pack_sstate(&s, sbuf, SSTATE_SIZE);
	for (int i = 0; i < SSTATE_SIZE; ++i)
		PDEBUG("%02hhx ", sbuf[i]);
	unpack_sstate(&s, sbuf);
	sprintf(after, "%ld %ld %x %x %x", s.when, s.issued_at, s.powcmd, s.timer, s.ack);
	return !strcmp(before, after);
}
