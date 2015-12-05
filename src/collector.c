/*      heartbeat.c
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

#include "collector.h"

bitmap_t coll_targets;
bitmap_t coll_bitmaps[NODE_MAX];
int coll_seq[NODE_MAX][NODE_MAX];

void initialize_collector()
{
	int i, j;
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;
	
	coll_targets = 0;
	for (i = 0; i < NODE_MAX; i++) {
		coll_bitmaps[i] = 0;
		for (j = 0; j < NODE_MAX; j++)
			coll_seq[i][j] = 0;
	}
	
	arg = (pub_arg_t *)malloc(sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memory");
		return;
	}
	
	arg->sender = NULL;
	arg->callback = NULL;
	arg->publisher = NULL;
	strcpy(arg->src, COLL_FRONTEND_ADDR);
	sprintf(arg->dest, "tcp://*:%d", collector_port);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);
}


void connect_collectors()
{
	int i;
	int cnt = 0;
	sub_arg_t *args;
	pthread_t thread;
	pthread_attr_t attr;
	
	args = (sub_arg_t *)malloc((nr_nodes - 1) * sizeof(sub_arg_t));
	if (!args) {
		log_err("failed to create subsbribers");
		return;
	}
	
	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {			
			sprintf(args[cnt].src, "tcp://%s:%d", nodes[i], collector_port);
			strcpy(args[cnt].dest, COLL_BACKEND_ADDR);
			
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, start_subscriber, &args[cnt]);
			pthread_attr_destroy(&attr);
			cnt++;
		}
	}
}


void check_state(coll_arg_t *arg)
{
	int i, j;
	int id = arg->id;
	int seq = arg->seq;
	bool resume = false;
	int target = arg->target;
	bitmap_t bitmap = arg->bitmap;
	
	log_debug("%s: target=%d, id=%d, coll_seq=%d, seq=%d, bitmap=%x", __func__, target, id, coll_seq[target][id], seq, bitmap);
	if (coll_seq[target][id] > seq) {
		log_err("invalid arguments");
		return;
	}
	
	if (target == node_id)
		return;
	
	if (id == node_id) 
		coll_targets |= node_mask[target];
	coll_seq[target][id] = seq;
	coll_bitmaps[id] = bitmap;
	
	for (i = 0; i < nr_nodes; i++)
		if ((i != node_id) && (bitmap_available & node_mask[i]))
			if (coll_bitmaps[i] != bitmap_available)
				return;
	
	for (i = 0; i < nr_nodes; i++) {
		if (coll_targets & node_mask[i]) {
			int pos = 0;
			
			for (j = 1; j < nr_nodes; j++)
				if (coll_seq[i][j] > coll_seq[i][pos])
					pos = j;
			
			if (pos == node_id)
				message_replay(i);
			
			coll_targets &= ~node_mask[i];
			resume = true;
		}
	}
	
	if (resume) {
		log_debug("%s: mixer->resume", __func__);
		resume_mixer();
	}
}


void *collect(void *ptr)
{
	int ret;
	void *context = zmq_ctx_new();
	void *socket = zmq_socket(context, ZMQ_PULL);
	
	ret = zmq_bind(socket, COLL_BACKEND_ADDR);
	if (ret) {
		log_err("failed to bind to %s", COLL_BACKEND_ADDR);
		goto out;
	}
	
	while (1) {
		zmsg_t *msg = zmsg_recv(socket);
		zframe_t *frame = zmsg_first(msg);
		
		if (zframe_size(frame) == sizeof(coll_arg_t))
			check_state((coll_arg_t *)zframe_data(frame));	
		else
			log_err("invalid message");
		zmsg_destroy(&msg);
	}
out:
	zmq_close(socket);
	zmq_ctx_destroy(context);
	return NULL;
}


void start_collector()
{
	pthread_t thread;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, collect, NULL);
	pthread_attr_destroy(&attr);
}


void create_collector()
{
	initialize_collector();
	start_collector();
	connect_collectors();
}
