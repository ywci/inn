#ifndef _SYNTH_H
#define _SYNTH_H

#include <errno.h>
#include <default.h>
#include <pthread.h>
#include "callback.h"
#include "record.h"
#include "queue.h"
#include "event.h"

typedef struct synth_arg {
	int id;
	int type;
	char addr[ADDR_SIZE];
} synth_arg_t;

typedef struct synth_entry {
	record_t *record;
	struct list_head list;
} synth_entry_t;

int get_seq(int id);
int create_synthesizer();
void resume_synthesizer();
void suspend_synthesizer();
void add_message(int id, int seq, zmsg_t *msg);
zmsg_t *update_message(zmsg_t *msg);

#endif
