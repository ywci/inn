#ifndef _SAMPLER_H
#define _SAMPLER_H

#include <errno.h>
#include <pthread.h>
#include <default.h>
#include "responder.h"
#include "publisher.h"
#include "subscriber.h"
#include "collector.h"
#include "heartbeat.h"
#include "replayer.h"
#include "record.h"
#include "synth.h"

#define INDEXER_ADDR "ipc:///tmp/innsampler"

int create_sampler();
void send_message(zmsg_t *msg);

#endif
