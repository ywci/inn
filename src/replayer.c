/*      replayer.c
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

#include "replayer.h"
#include "mixer.h"

void *replay(void *ptr)
{
	int ret;
	void *context = zmq_ctx_new();
	void *socket = zmq_socket(context, ZMQ_PULL);
	
	ret = zmq_bind(socket, REPLAYER_BACKEND_ADDR);
	if (ret) {
		log_err("failed to bind to %s", REPLAYER_BACKEND_ADDR);
		goto out;
	}
	
	while (1) {
		zmsg_t *msg = zmsg_recv(socket);
		zframe_t *frame = zmsg_pop(msg);
		
		if (zframe_size(frame) == sizeof(replay_arg_t)) {
			replay_arg_t *arg = (replay_arg_t *)zframe_data(frame);
			
			insert_message(arg->id, arg->seq, msg);
		} else {
			log_err("invalid message");
			zmsg_destroy(&msg);
		}
		zframe_destroy(&frame);
	}
out:
	zmq_close(socket);
	zmq_ctx_destroy(context);
	return NULL;
}


void start_replayer()
{
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;
	
	arg = (pub_arg_t *)malloc(sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memory");
		return;
	}
	
	arg->sender = NULL;
	arg->callback = NULL;
	arg->publisher = NULL;
	strcpy(arg->src, REPLAYER_FRONTEND_ADDR);
	sprintf(arg->dest, "tcp://*:%d", replayer_port);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, replay, NULL);
	pthread_attr_destroy(&attr);
}


void connect_replayers()
{
	int i;
	int cnt = 0;
	sub_arg_t *args;
	pthread_t thread;
	pthread_attr_t attr;
	
	args = (sub_arg_t *)malloc((nr_nodes - 1) * sizeof(sub_arg_t));
	if (!args) {
		log_err("no memory");
		return;
	}
	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			sprintf(args[cnt].src, "tcp://%s:%d", nodes[i], replayer_port);
			strcpy(args[cnt].dest, REPLAYER_BACKEND_ADDR);
			
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, start_subscriber, &args[cnt]);
			pthread_attr_destroy(&attr);
			cnt++;
		}
	}
}


void create_replayer()
{
	connect_replayers();
	start_replayer();
}


void send_replay_message(int id, int seq, zmsg_t *msg)
{
	int ret;
	void *socket;
	void *context;
	zmsg_t *newmsg;
	zframe_t *frame;
	replay_arg_t arg;
	
	arg.id = id;
	arg.seq = seq;
	newmsg = zmsg_dup(msg);
	if (!newmsg) {
		log_err("failed to duplicate");
		return;
	}
	context = zmq_ctx_new();
	socket = zmq_socket(context, ZMQ_PUSH);
	frame = zframe_new(&arg, sizeof(replay_arg_t));
	zmsg_prepend(newmsg, &frame);
	ret = zmq_connect(socket, REPLAYER_FRONTEND_ADDR);
	if (!ret) {
		zmsg_send(&newmsg, socket);
		zmq_close(socket);
	} else
		log_err("failed to send replay message");
	zmq_ctx_destroy(context);
	log_debug("%s: id=%d, seq=%d", __func__, id, seq);
}


void message_replay(int id)
{
	int i;
	zmsg_t *msg;
	int seq_min = record_get_min(id);
	int seq_max = record_get_max(id);
	
	if (seq_max < seq_min) {
		log_err("invalid records");
		return;
	}
	
	if (!seq_min)
		return;
	
	log_debug("%s: id=%d", __func__, id);
	for (i = seq_min; i <= seq_max; i++) {
		msg = record_find(id, i);
		if (!msg) {
			log_err("failed to find record");
			return;
		}
		send_replay_message(id, i, msg);
	}
}
