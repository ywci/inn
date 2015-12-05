#ifndef _REPLAYER_H
#define _REPLAYER_H

#include <zmq.h>
#include <default.h>
#include <pthread.h>
#include "lib/log.h"
#include "lib/record.h"
#include "lib/publisher.h"
#include "lib/subscriber.h"
#include "mixer.h"

#define REPLAYER_FRONTEND_ADDR "ipc:///tmp/innrf"
#define REPLAYER_BACKEND_ADDR "ipc:///tmp/innrb"

typedef struct reply_arg {
	int id;
	int seq;
} replay_arg_t;

void create_replayer();
void message_replay(int id);

#endif
