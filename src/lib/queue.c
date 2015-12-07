/*      queue.c
 *      
 *      Copyright (C) 2015 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "queue.h"
#include "util.h"

void queue_init(que_t *queue, int id)
{
	queue->id = id;
	queue->seq = 0;
	queue->flgs = 0;
	queue->length = 0;
	INIT_LIST_HEAD(&queue->head);
	pthread_mutex_init(&queue->mutex, NULL);
}


int queue_add_item(que_t *queue, que_item_t *item)
{
	int ret = 0;
	
	if (queue->length < QUEUE_LENGTH) {
		if (!(queue->flgs & FL_INACTIVE)) {
			queue->seq += 1;
			item->seq = queue->seq;
			list_add_tail(&item->item, &queue->head);
			queue->length += 1;
			log_debug("queue_%d: add item, len=%d, seq=%d", queue->id, queue->length, item->seq);
		}
	} else {
		log_err("queue_%d: no space", queue->id);
		ret = -1;
	}
	return ret;
}


void queue_pop_item(que_t *queue, char *ts, zmsg_t **msg) 
{
	int i;
	que_item_t *item;
	struct list_head *head = &queue->head;
	
	pthread_mutex_lock(&queue->mutex);
	for (i = 0; i < queue->length; i++) {
		head = head->next;
		item = list_entry(head, que_item_t, item);
			
		if (!timestamp_compare(ts, item->timestamp)) {
			list_del(&item->item);
			if (!msg)
				zmsg_destroy(&item->msg);
			else
				*msg = item->msg;
			queue->length -= 1;
			log_debug("queue_%d: delete item, len=%d, seq=%d", queue->id, queue->length, item->seq);
			free(item);
			break;
		}
	}
	pthread_mutex_unlock(&queue->mutex);
}


void queue_set_inactive(que_t *queue)
{
	pthread_mutex_lock(&queue->mutex);
	queue->flgs |= FL_INACTIVE;
	pthread_mutex_unlock(&queue->mutex);
}


void queue_set_active(que_t *queue)
{
	pthread_mutex_lock(&queue->mutex);
	queue->flgs &= ~FL_INACTIVE;
	pthread_mutex_unlock(&queue->mutex);
}


int queue_get_seq(que_t *queue)
{
	return queue->seq;
}


int queue_next_seq(que_t *queue)
{
	return queue->seq + 1;
}


void queue_update_seq(que_t *queue)
{
	queue->seq += 1;
}
