#ifndef NOTIF_H
#define NOTIF_H 1

#include "protocol.h"

int confirm_shutdown(void);
void send_notification(struct request *req_type);

#endif /* ifndef NOTIF_H */
