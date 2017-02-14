/* queue.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "queue.h"
#include "util.h"

void queue_init(queue_t *queue, int id)
{
	queue->id = id;
	queue->seq = 0;
	queue->length = 0;
	INIT_LIST_HEAD(&queue->head);
}


int queue_push(queue_t *queue, struct list_head *entry)
{
	int ret = 0;

	if (queue->length < QUEUE_LENGTH) {
		list_add_tail(entry, &queue->head);
		queue->length += 1;
	} else {
		log_err("queue: failed to push, len=%d, seq=%d (id=%d)", queue->length, queue->seq, queue->id);
		ret = -1;
	}
	return ret;
}


void queue_pop(queue_t *queue, struct list_head *entry)
{
	if (queue->length > 0) {
		list_del(entry);
		memset(entry, 0, sizeof(struct list_head));
		queue->length--;
	} else
		log_err("queue: failed to pop, len=%d, seq=%d (id=%d)", queue->length, queue->seq, queue->id);
}


int queue_update_seq(queue_t *queue)
{
	queue->seq++;
	return queue->seq;
}
