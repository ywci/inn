#ifndef _QUEUE_H
#define _QUEUE_H

#include <czmq.h>
#include <stdlib.h>
#include <default.h>
#include <pthread.h>
#include "list.h"
#include "log.h"

#define QUEUE_LENGTH 65536

typedef struct queue {
	int id;
	int seq;
	int length;
	struct list_head head;
} queue_t;

int queue_update_seq(queue_t *queue);
void queue_init(queue_t *queue, int id);
int queue_push(queue_t *queue, struct list_head *entry);
void queue_pop(queue_t *queue, struct list_head *entry);

#endif
