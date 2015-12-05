#ifndef _COLLECTOR_H
#define _COLLECTOR_H

#include <stdlib.h>
#include <default.h>
#include <pthread.h>
#include "lib/log.h"
#include "lib/publisher.h" 
#include "lib/subscriber.h"
#include "replayer.h"
#include "mixer.h"

#define COLL_FRONTEND_ADDR "ipc:///tmp/inncf"
#define COLL_BACKEND_ADDR "ipc:///tmp/inncb"

typedef struct coll_arg {
	int id;
	int seq;
	int target;
	bitmap_t bitmap;
} coll_arg_t;

void create_collector();
void check_state(coll_arg_t *arg);

#endif
