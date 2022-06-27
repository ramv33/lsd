#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "common.h"
#include "protocol.h"
#include "power.h"

int g_powcmd;

void sigalrm_handler(int signum)
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
	alarm(req->timer);
	PDEBUG("[+] alarm set for %d seconds\n", req->timer);
	state->issued_at = time(NULL);
	g_powcmd = req->req_type;

	return 0;
}

void power_abort(void)
{
	PDEBUG("[-] aborting any pending requests\n");
	alarm(0);
}
