#ifndef _EXTSUB_H
#define _EXTSUB_H

#include <zmq.h>
#include <stdlib.h>
#include <default.h>
#include "util.h"
#include "log.h"

typedef struct sub_arg {
	char src[ADDR_SIZE];
	char dest[ADDR_SIZE];
} sub_arg_t;

void *start_subscriber(void *ptr);

#endif
