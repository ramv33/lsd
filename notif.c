#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "protocol.h"
#include "notif.h"

int confirm_shutdown(struct request *req, unsigned int timeout)
{
	FILE *fp;
	char msg[16];
	char cmd[256];
	uint16_t req_type;

	req_type = req->req_type;
	RESET_FORCE_BIT(req_type);
	if (reqstr(req_type, msg, sizeof(msg)) == NULL) {
		PDEBUG("[*] invalid request type: %x\n", req_type);
		return;
	}
	snprintf(cmd, sizeof(cmd),
			"zenity --question \
			--width 400 --height 100 \
			--timeout=%d \
			--title 'Confirm' \
			--text 'Confirm %s in %d seconds? (y/n)'",
			timeout, msg, req->timer);
	// Test if Zenity is installed (get version)
	fp=popen(cmd, "r");
	if (fp==NULL) {
		perror("popen: zenity");
	} else {
		int ret = WEXITSTATUS(pclose(fp));
		/* ret = 0, 1, or 5 depending on whether user pressed OK, Cancel, or
		 * timeout has been reached */
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
