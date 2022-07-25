#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "common.h"
#include "protocol.h"
#include "notif.h"
#include "power.h"

#define CONFIRM_TIMEOUT	10

uint16_t g_powcmd;

static void doit(uint16_t req_type);

void sigalrm_handler(int signum)
{
	PDEBUG("[-] alarm rang: calling doit()\n");
	doit(g_powcmd);
}

static void doit(uint16_t req_type)
{
	switch (req_type) {
	case REQ_POW_SHUTDOWN:
		PDEBUG("[-] shutting down\n");
		break;
	case REQ_POW_REBOOT:
		PDEBUG("[-] rebooting\n");
		break;
	case REQ_POW_STANDBY:
	case REQ_POW_SLEEP:
		PDEBUG("[-] standby\n");
		break;
	case REQ_POW_HIBERNATE:
		PDEBUG("[-] hibernate\n");
		break;
	}

}

int power_schedule(struct request *req, struct sstate *state)
{
	struct sigaction act;
	int scheduled = 0;		/* return value: 0 if not scheduled */

	/* copy request type and reset force bit for switch case */
	g_powcmd = req->req_type;
	RESET_FORCE_BIT(g_powcmd);

	if (GET_FORCE_BIT(req->req_type) == 0) {
		PDEBUG("[-] no force bit\n");
		if (!confirm_shutdown(req, CONFIRM_TIMEOUT)) {
			PDEBUG("[-] shutdown cancelled by user\n");
			return scheduled;
		}
	} else {
		PDEBUG("[-] force bit set\n");
		send_notification(req);
	}

	act.sa_handler = sigalrm_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	alarm(0);	/* cancel any pending commands */
	PDEBUG("[+] cancelled any pending alarm\n");
	if (sigaction(SIGALRM, &act, NULL) < 0) {
		perror("sigaction: error setting SIGALRM handler");
		return 0;
	}
	PDEBUG("[+] signal handler registered\n");


	/* no timer, do it immediately */
	if (req->timer == 0) {
		doit(g_powcmd);
		/* may not reach here depending on request type */
	} else {
		alarm(req->timer);
		PDEBUG("[+] alarm set for %d seconds\n", req->timer);
	}
	scheduled = 1;
	state->issued_at = time(NULL);

	return scheduled;
}

void power_abort(void)
{
	PDEBUG("[-] aborting any pending requests\n");
	alarm(0);
}
