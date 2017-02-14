#ifndef _HEARTBEAT_H
#define _HEARTBEAT_H

#include <zmq.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "responder.h"
#include "requester.h"
#include "collector.h"
#include "synth.h"
#include "log.h"

#define HEARTBEAT_COMMAND   1
#define HEARTBEAT_WAITTIME  2 // sec
#define HEARTBEAT_INTERVAL  2000 // msec
#define HEARTBEAT_RETRY_MAX	3

typedef struct heartbeat_arg {
	int id;
	char addr[ADDR_SIZE];
} heartbeat_arg_t;

int create_heartbeat();

#endif
