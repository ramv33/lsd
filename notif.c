#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

int confirm_shutdown(void)
{
	FILE *fp;

	// Test if Zenity is installed (get version)
	fp=popen("zenity --question \
		--width 400 --height 100 \
		--title 'Confirm to shutdown' \
		--text 'Confirm shutdown? (yes/no)'","r");
	if (fp==NULL) {
		perror("Pipe returned a error");
	} else {
		int ret = WEXITSTATUS(pclose(fp));
		PDEBUG("confirmation status code: %d\n", ret);
		/* zenity returns 0 on 'yes' and 1 on 'no'. negate it */
		return !ret;
	}
}
