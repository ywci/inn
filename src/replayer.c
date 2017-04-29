/* replayer.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "replayer.h"

void *replay(void *ptr)
{
	int ret;
	void *context = zmq_ctx_new();
	void *socket = zmq_socket(context, ZMQ_PULL);

	ret = zmq_bind(socket, REPLAYER_BACKEND);
	if (ret) {
		log_err("failed to bind to %s", REPLAYER_BACKEND);
		goto out;
	}

	while (true) {
		zmsg_t *msg = zmsg_recv(socket);
		zframe_t *frame = zmsg_pop(msg);

		if (zframe_size(frame) == sizeof(replay_arg_t)) {
			replay_arg_t *arg = (replay_arg_t *)zframe_data(frame);

			add_message(arg->id, arg->seq, msg);
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
		log_err("failed to send replay message");
		return;
	}
	context = zmq_ctx_new();
	socket = zmq_socket(context, ZMQ_PUSH);
	frame = zframe_new(&arg, sizeof(replay_arg_t));
	zmsg_prepend(newmsg, &frame);
	ret = zmq_connect(socket, REPLAYER_FRONTEND);
	if (!ret) {
		sndmsg(&newmsg, socket);
		zmq_close(socket);
	} else
		log_err("failed to send replay message");
	zmq_ctx_destroy(context);
	log_func("id=%d, seq=%d", id, seq);
}


void message_replay(int id)
{
	int i;
	zmsg_t *msg;
	int seq_min = record_min(id);
	int seq_max = record_max(id);

	if (seq_max < seq_min) {
		log_err("invalid records");
		return;
	}

	if (!seq_min)
		return;

	log_func("id=%d", id);
	for (i = seq_min; i <= seq_max; i++) {
		msg = record_get_msg(id, i);
		if (!msg) {
			log_err("failed to find record, seq=%d", i);
			return;
		}
		send_replay_message(id, i, msg);
	}
}


int connect_replayers()
{
	int i;
	int cnt = 0;
	sub_arg_t *args;
	pthread_t thread;
	pthread_attr_t attr;

	args = (sub_arg_t *)malloc((nr_nodes - 1) * sizeof(sub_arg_t));
	if (!args) {
		log_err("no memory");
		return -ENOMEM;
	}

	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			tcpaddr(args[cnt].src, nodes[i], replayer_port);
			strcpy(args[cnt].dest, REPLAYER_BACKEND);

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, start_subscriber, &args[cnt]);
			pthread_attr_destroy(&attr);
			cnt++;
		}
	}

	return 0;
}


int create_replayer()
{
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;

	arg = (pub_arg_t *)calloc(1, sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memory");
		return -ENOMEM;
	}
	arg->type = MULTICAST_PUB;
	strcpy(arg->src, REPLAYER_FRONTEND);
	tcpaddr(arg->addr, inet_ntoa(get_addr()), replayer_port);

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

	return connect_replayers();
}
