#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "protocol.h"
#include "protocol.h"

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
	time_t t = time(NULL);
	struct request req = {
		.when = t,
		.req_type = 0xdead,
		.timer	= 0x10101010,
		.msg_size = 6,
		.msg = "hello"
	};
	size_t size;
	sprintf(before, "%lld %x %x %d",
		req.when, req.req_type, req.timer, req.msg_size);
	char *reqbuf = pack_request(&req, &size);

	PDEBUG("REQUEST\n=======\n");
	PDEBUG("\nwhen = %lld\n"
		"req_type = %x\n"
		"timer = %x\n"
		"msg_size = %d\n",
		req.when, req.req_type, req.timer, req.msg_size);

	for (int i = 0; i < size; ++i)
		PDEBUG("%02hhx ", reqbuf[i]);
	PDEBUG("\n=========\n");

	unpack_request_fixed(&req, reqbuf);
	sprintf(after, "%lld %x %x %d",
		req.when, req.req_type, req.timer, req.msg_size, req.msg);
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
		.timer = 0x12345678
	};

	sprintf(before, "%ld %ld %x %x", s.when, s.issued_at, s.powcmd, s.timer);
	pack_sstate(&s, sbuf, SSTATE_SIZE);
	for (int i = 0; i < SSTATE_SIZE; ++i)
		PDEBUG("%02hhx ", sbuf[i]);
	unpack_sstate(&s, sbuf);
	sprintf(after, "%ld %x %x", s.when, s.issued_at, s.powcmd, s.timer);
	return !strcmp(before, after);
}
