#ifndef _PUBLISHER_H
#define _PUBLISHER_H

#include <zmq.h>
#include <stdlib.h>
#include <default.h>
#include "util.h"
#include "log.h"

typedef struct pub_arg {
	char src[ADDR_SIZE];
	char dest[ADDR_SIZE];
	callback_t callback;
	sender_t sender;
	void **publisher;
} pub_arg_t;

void *start_publisher(void *ptr);

#endif
