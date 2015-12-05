#ifndef _SEQUENCER_H
#define _SEQUENCER_H

#include <default.h>
#include <pthread.h>
#include "lib/record.h"
#include "lib/responder.h"
#include "lib/publisher.h"
#include "lib/subscriber.h"
#include "collector.h"
#include "heartbeat.h"
#include "replayer.h"
#include "mixer.h"

#define BROADCAST_ADDR "ipc:///tmp/innb"
#define SEQUENCER_ADDR "ipc:///tmp/inns"

pthread_t create_sequencer();
void send_message(zmsg_t *msg);
zmsg_t *update_message(zmsg_t *msg, bool mark);

#endif
