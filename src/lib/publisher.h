#ifndef _PUBLISHER_H
#define _PUBLISHER_H

#include <zmq.h>
#include <stdlib.h>
#include <default.h>
#include "util.h"
#include "log.h"

typedef struct pub_arg {
	int type;
	int total;
	bool bypass;
	sender_t sender;
	sender_desc_t *desc;
	callback_t callback;
	char src[ADDR_SIZE];
	char addr[ADDR_SIZE];
	char dest[NODE_MAX][ADDR_SIZE];
} pub_arg_t;

void *start_publisher(void *ptr);

#endif
