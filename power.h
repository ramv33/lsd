#ifndef POWER_H
#define POWER_H 1

#include "protocol.h"	/* get definition of struct request and state */

int power_schedule(struct request *req, struct sstate *state);

void power_abort(void);

#endif /* #ifndef POWER_H */
