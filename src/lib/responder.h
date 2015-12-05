#ifndef _RESPONDER_H
#define _RESPONDER_H

#include <zmq.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <default.h>
#include "requester.h"
#include "log.h"

typedef rep_t (*responder_t)(req_t);

typedef struct reponder_arg {
	char addr[ADDR_SIZE];
	responder_t responder;
} responder_arg_t;

void *start_responder(void *ptr);

#endif
