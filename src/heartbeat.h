#ifndef _HEARTBEAT_H
#define _HEARTBEAT_H

#include <zmq.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "lib/log.h"
#include "lib/responder.h"
#include "lib/requester.h"
#include "collector.h"
#include "mixer.h"

#define HEARTBEAT_COMMAND   1
#define HEARTBEAT_WAITTIME  2 // sec
#define HEARTBEAT_INTERVAL  2000 // msec
#define HEARTBEAT_RETRY_MAX	3

typedef struct heartbeat_arg {
	int id;
	char addr[ADDR_SIZE];
} heartbeat_arg_t;

void create_heartbeat();

#endif
