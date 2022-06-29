#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common.h"
#include "daemon.h"

void daemonize(void)
{
	pid_t pid = fork();

	switch (pid) {
	case -1:
		perror("daemonize: fork");
		exit(EXIT_FAILURE);
	case 0:
		PDEBUG("[-] hello from child\n");
		break;
	default:
		/* let parent die :( */
		printf("[-] created child with pid=%d\n", pid);
		printf("[-] exiting...\n");
		exit(EXIT_SUCCESS);
	}

	/* create new session */
	if (setsid() < 0) {
		perror("daemonize: setsid");
		exit(EXIT_FAILURE);
	}

	umask(0);
	chdir("/");

}
