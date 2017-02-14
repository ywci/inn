#ifndef _REPLAYER_H
#define _REPLAYER_H

#include <zmq.h>
#include <pthread.h>
#include <default.h>
#include "subscriber.h"
#include "publisher.h"
#include "record.h"
#include "synth.h"
#include "log.h"

#define REPLAYER_BACKEND "ipc:///tmp/innreplayerb"
#define REPLAYER_FRONTEND "ipc:///tmp/innreplayerf"

typedef struct reply_arg {
	int id;
	int seq;
} replay_arg_t;

int create_replayer();
void message_replay(int id);

#endif
