#ifndef _QUEUE_H
#define _QUEUE_H

#include <czmq.h>
#include <stdlib.h>
#include <default.h>
#include <pthread.h>
#include "list.h"
#include "log.h"

#define FL_VISIBLE  0x00000001
#define FL_ACCESS	0x00000002
#define FL_INACTIVE 0x00000004

#define QUEUE_LENGTH 65536

typedef struct que {
	int id;
	int seq;
	int flgs;
	int length;
	struct list_head head;
	pthread_mutex_t mutex;
} que_t;

typedef struct que_item {
	int seq;
	int flgs;
	zmsg_t *msg;
	char *timestamp;
	struct list_head item;
} que_item_t;

int queue_get_seq(que_t *queue);
int queue_next_seq(que_t *queue);
void queue_update_seq(que_t *queue);
void queue_set_active(que_t *queue);
void queue_set_inactive(que_t *queue);
void queue_init(que_t *queue, int id);
int queue_add_item(que_t *queue, que_item_t *item);
void queue_pop_item(que_t *queue, char *ts, zmsg_t **msg);

#endif
