#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "protocol.h"
#include "notif.h"

int confirm_shutdown(void)
{
	FILE *fp;

	// Test if Zenity is installed (get version)
	fp=popen("zenity --question \
		--width 400 --height 100 \
		--title 'Confirm to shutdown' \
		--text 'Confirm shutdown? (yes/no)'","r");
	if (fp==NULL) {
		perror("popen: zenity");
	} else {
		int ret = WEXITSTATUS(pclose(fp));
		PDEBUG("confirmation status code: %d\n", ret);
		/* zenity returns 0 on 'yes' and 1 on 'no'. negate it */
		return !ret;
	}
}

void send_notification(struct request *req)
{
	uint16_t req_type;
	FILE *fp;
	char msg[16];
	char cmd[256];

	req_type = req->req_type;
	RESET_FORCE_BIT(req_type);

	if (reqstr(req_type, msg, sizeof(msg)) == NULL) {
		PDEBUG("[*] invalid request type: %x\n", req_type);
		return;
	}

	snprintf(cmd, sizeof(cmd), "zenity --info --text='%s in %d seconds'",
			msg, req->timer);

	fp = popen(cmd, "r");
	if (fp == NULL)
		perror("popen: zenity");
	else
		PDEBUG("[-] sent notification: '%s in %d seconds'\n",
			msg, req->timer);
}
