#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "common.h"
#include "protocol.h"
#include "notif.h"
#include "power.h"

uint16_t g_powcmd;

static void doit(uint16_t req_type);

void sigalrm_handler(int signum)
{
	PDEBUG("[-] alarm rang: calling doit()\n");
	doit(g_powcmd);
}

static void doit(uint16_t req_type)
{
	switch (g_powcmd) {
	case REQ_POW_SHUTDOWN:
		PDEBUG("[-] shutting down\n");
		break;
	case REQ_POW_REBOOT:
		PDEBUG("[-] rebooting\n");
		break;
	case REQ_POW_STANDBY:
		PDEBUG("[-] standby\n");
		break;
	case REQ_POW_SLEEP:
		PDEBUG("[-] sleeping\n");
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

	if (GET_FORCE_BIT(req->req_type) == 0) {
		PDEBUG("[-] no force bit\n");
		if (!confirm_shutdown()) {
			PDEBUG("[-] shutdown cancelled by user\n");
			return scheduled;
		}
	} else {
		PDEBUG("[-] force bit set\n");
	}

	act.sa_handler = sigalrm_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	alarm(0);	/* cancel any pending commands */
	PDEBUG("[+] cancelled any pending alarm\n");
	if (sigaction(SIGALRM, &act, NULL) < 0) {
		perror("sigaction: error setting SIGALRM handler");
		return -1;
	}
	PDEBUG("[+] signal handler registered\n");

	/* copy request type and reset force bit for switch case */
	g_powcmd = req->req_type;
	RESET_FORCE_BIT(g_powcmd);

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