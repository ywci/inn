#ifndef _CLIENT_H
#define _CLIENT_H

#include <stdlib.h>
#include <default.h>
#include <pthread.h>
#include <sys/time.h>
#include "lib/util.h"
#include "lib/publisher.h"
#include "lib/requester.h"
#include "lib/subscriber.h"

#define INN_ADDR "ipc:///tmp/inn"
#define CLIENT_ADDR "ipc:///tmp/innc"

typedef struct notify_arg {
	struct in_addr src;
	char dest[ADDR_SIZE];
} notify_arg_t;

void create_client();

#endif
