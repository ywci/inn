#ifndef _COLLECTOR_H
#define _COLLECTOR_H

#include <stdlib.h>
#include <default.h>
#include <pthread.h>
#include "publisher.h"
#include "subscriber.h"
#include "replayer.h"
#include "synth.h"
#include "log.h"

#define COLLECTOR_BACKEND "ipc:///tmp/inncollectorb"
#define COLLECTOR_FRONTEND "ipc:///tmp/inncollectorf"

typedef struct coll_arg {
	int id;
	int seq;
	int target;
	bitmap_t bitmap;
} coll_arg_t;

int create_collector();
void check_state(coll_arg_t *arg);

#endif
