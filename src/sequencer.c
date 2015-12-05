/*      sequencer.c
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

#include "sequencer.h"

byte *seq_vector;
void *seq_publisher;
pthread_mutex_t seq_mutex;
pthread_mutex_t msg_mutex;

rep_t reply(req_t req)
{
	sub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;
	struct in_addr addr;
	
	arg = (sub_arg_t *)malloc(sizeof(sub_arg_t));
	if (!arg) {
		log_err("no memory");
		return 0;
	}
	
	memcpy(&addr, &req, sizeof(struct in_addr));
	sprintf(arg->src, "tcp://%s:%d", inet_ntoa(addr), client_port);
	strcpy(arg->dest, SEQUENCER_ADDR);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_subscriber, arg);
	pthread_attr_destroy(&attr);
	
	return req;
}


void detect_client()
{
	pthread_t thread;
	pthread_attr_t attr;
	responder_arg_t *arg;
	
	arg = (responder_arg_t *)malloc(sizeof(responder_arg_t));
	if (!arg) {
		log_err("no memeory");
		return;
	}
	
	arg->responder = reply;
	sprintf(arg->addr, "tcp://*:%d", sequencer_port);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_responder, arg);
	pthread_attr_destroy(&attr);
}


void update_vector()
{
	int i;
	int bits;
	int pos = 0;
	int cnt = 0;
	byte val = 0;
	
	memset(seq_vector, 0, vector_size);
	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			bits = cnt * NBIT;
			val |= check_step(i) << bits;
			if (bits + NBIT == 8) {
				seq_vector[pos] = val;
				val = 0;
				cnt = 0;
				pos++;
			} else
				cnt++;
		}
	}
	if (cnt)
		seq_vector[pos] = val;
}


zmsg_t *do_update_message(zmsg_t *msg, bool mark)
{
	int ret;
	bool first;
	zframe_t *frame;
	que_item_t *item;
	zmsg_t *newmsg = NULL;
	pthread_mutex_t *mutex = NULL;

	mutex = record_get(node_id, next_seq(node_id), msg, mark, NULL, &first);
	if (mutex) {
		if (!first)
			goto unlock;
		item = (que_item_t *)malloc(sizeof(que_item_t));
		if (!item) {
			log_err("no memory");
			goto unlock;
		}
		memset(item, 0, sizeof(que_item_t));
		newmsg = zmsg_dup(msg);
		if (!newmsg) {
			log_err("failed to duplicate");
			goto out;
		}
		frame = zmsg_first(newmsg);
		item->msg = newmsg;
		item->timestamp = (char *)zframe_data(frame);
	#ifdef SHOW_TIMESTAMP
		show_timestamp("sequencer", node_id, item->timestamp);
	#endif
		if (get_matrix_item(node_id, node_id) != get_seq(node_id)) {
			log_err("failed to update, item=%d, seq=%d", get_matrix_item(node_id, node_id), get_seq(node_id));
			goto out;
		}
		ret = update_queue(node_id, item);
		if (ret) {
			log_err("failed to update queue");
			goto out;
		}
		update_matrix(node_id, node_id, 1);
		update_vector();
		record_put(mutex);
	} else
		goto release;
#ifdef SHOW_VECTOR
	show_vector("sequencer", node_id, seq_vector);
#endif
	frame = zframe_new(seq_vector, vector_size);
	zmsg_prepend(msg, &frame);
	return msg;
out:
	free(item);
	if (newmsg)
		zmsg_destroy(&newmsg);
unlock:
	record_put(mutex);
release:
	zmsg_destroy(&msg);
	return NULL;
}


zmsg_t *update_message(zmsg_t *msg, bool mark)
{
	zmsg_t *ret;

	pthread_mutex_lock(&seq_mutex);
	ret = do_update_message(msg, mark);
	pthread_mutex_unlock(&seq_mutex);
#ifdef SEQUENCER_BALANCE
	if (mark) {
		while (need_balance(node_id))
			wait_timeout(BALANCE_TIME);
	}
#endif
	return ret;
}


zmsg_t *check_message(zmsg_t *msg)
{
	return update_message(msg, true);
}


void do_send_message(zmsg_t **msg, void *socket)
{
	pthread_mutex_lock(&msg_mutex);
	zmsg_send(msg, socket);
	pthread_mutex_unlock(&msg_mutex);
}


void send_message(zmsg_t *msg)
{
	if (seq_publisher)
		do_send_message(&msg, seq_publisher);
}


void do_connect_mixers()
{
	int i;
	sub_arg_t *args;
	
	args = (sub_arg_t *)malloc((nr_nodes - 1) * sizeof(sub_arg_t));
	if (!args) {
		log_err("no memory");
		return;
	}
	
	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			pthread_t thread;
			pthread_attr_t attr;
			
			strcpy(args[i].src, BROADCAST_ADDR);
			sprintf(args[i].dest, "tcp://%s:%d", nodes[i], mixer_port + node_id);
			
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, start_subscriber, &args[i]);
			pthread_attr_destroy(&attr);
		}
	}
}


void connect_mixers()
{
#ifndef LIGHT_SERVER
	do_connect_mixers();
#endif
}


pthread_t start_sequencer(char *src, char *dest)
{
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;

	seq_publisher = NULL;
	pthread_mutex_init(&seq_mutex, NULL);
	pthread_mutex_init(&msg_mutex, NULL);
	seq_vector = malloc(vector_size);
	if (!seq_vector) {
		log_err("no memory");
		return -1;
	}
	
	arg = (pub_arg_t *)malloc(sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memeory");
		return -1;
	}
	
	strcpy(arg->src, src);
	strcpy(arg->dest, dest);
	arg->sender = do_send_message;
	arg->callback = check_message;
	arg->publisher = &seq_publisher;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);
	
	connect_mixers();
	return thread;
}


pthread_t create_sequencer()
{
	char src[ADDR_SIZE];
	char dest[ADDR_SIZE];
	
#ifdef LIGHT_CLIENT
	strcpy(src, SEQUENCER_ADDR);
	detect_client();
#else
	sprintf(src, "tcp://*:%d", sequencer_port);
#endif

#ifdef LIGHT_SERVER
	sprintf(dest, "tcp://*:%d", mixer_port);
#else
	sprintf(dest, BROADCAST_ADDR);
#endif
	return start_sequencer(src, dest);
}
