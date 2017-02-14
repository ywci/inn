/* collector.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "collector.h"

bitmap_t coll_targets;
bitmap_t coll_bitmaps[NODE_MAX];
int coll_seq[NODE_MAX][NODE_MAX];

void connect_collectors()
{
	int i;
	pthread_t thread;
	pthread_attr_t attr;

	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			sub_arg_t *arg;

			arg = (sub_arg_t *)malloc(sizeof(sub_arg_t));
			if (!arg) {
				log_err("no memory");
				return;
			}
			tcpaddr(arg->src, nodes[i], collector_port);
			strcpy(arg->dest, COLLECTOR_BACKEND);

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, start_subscriber, arg);
			pthread_attr_destroy(&attr);
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

	log_func("target=%d, id=%d, coll_seq=%d, seq=%d, bitmap=%x", target, id, coll_seq[target][id], seq, bitmap);
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
		if ((i != node_id) && (available_nodes & node_mask[i]))
			if (coll_bitmaps[i] != available_nodes)
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
		log_func("synth->resume");
		resume_synthesizer();
	}
}


void *collect(void *ptr)
{
	int ret;
	void *context = zmq_ctx_new();
	void *socket = zmq_socket(context, ZMQ_PULL);

	ret = zmq_bind(socket, COLLECTOR_BACKEND);
	if (ret) {
		log_err("failed to bind to %s", COLLECTOR_BACKEND);
		goto out;
	}

	while (true) {
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


void init_collector()
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

	arg = (pub_arg_t *)calloc(1, sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memory");
		return;
	}
	arg->type = MULTICAST_PUB;
	strcpy(arg->src, COLLECTOR_FRONTEND);
	tcpaddr(arg->addr, inet_ntoa(get_addr()), collector_port);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);
}


int create_collector()
{
	pthread_t thread;
	pthread_attr_t attr;

	init_collector();
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, collect, NULL);
	pthread_attr_destroy(&attr);

	connect_collectors();
	return 0;
}
