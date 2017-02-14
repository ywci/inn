/* heartbeat.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "heartbeat.h"

void handle_fault(int id)
{
	int ret;
	coll_arg_t arg;
	void *context = zmq_ctx_new();
	void *socket = zmq_socket(context, ZMQ_PUSH);

	log_func("id=%d", id);
	suspend_synthesizer();
	available_nodes &= ~node_mask[id];
	arg.bitmap = available_nodes;
	arg.seq = get_seq(id);
	arg.id = node_id;
	arg.target = id;
	ret = zmq_connect(socket, COLLECTOR_FRONTEND);
	if (ret) {
		log_err("failed to connect");
		goto out;
	}
	ret = zmq_send(socket, &arg, sizeof(coll_arg_t), 0);
	if (ret != sizeof(coll_arg_t)) {
		log_err("failed to send request");
		goto out;
	}
	check_state(&arg);
out:
	zmq_close(socket);
	zmq_ctx_destroy(context);
}


void *do_heartbeat(void *ptr)
{
	int cnt;
	int ret;
	rep_t rep;
	void *socket;
	void *context;
	req_t req = HEARTBEAT_COMMAND;
	heartbeat_arg_t *arg = (heartbeat_arg_t *)ptr;

	ret = request(arg->addr, &req, &rep);
	if (ret || rep) {
		log_err("failed to start");
		return NULL;
	}

	while (true) {
		context = zmq_ctx_new();
		socket = zmq_socket(context, ZMQ_REQ);
		ret = zmq_connect(socket, arg->addr);
		if (ret) {
			log_err("failed to connect");
			sleep(HEARTBEAT_WAITTIME);
			continue;
		}
		cnt = 0;
		while (true) {
			zmq_pollitem_t item = {socket, 0, ZMQ_POLLIN, 0};

			ret = zmq_send(socket, &req, sizeof(req_t), 0);
			if (ret == sizeof(req_t)) {
				zmq_poll(&item, 1, HEARTBEAT_INTERVAL);
				if (item.revents & ZMQ_POLLIN) {
					ret = zmq_recv(socket, &rep, sizeof(rep_t), 0);
					if ((ret == sizeof(rep_t)) && !rep) {
						sleep(HEARTBEAT_INTERVAL / 1000);
						cnt = 0;
						continue;
					}
				}
			}
			cnt += 1;
			if (cnt == HEARTBEAT_RETRY_MAX) {
				if (available_nodes & node_mask[arg->id])
					handle_fault(arg->id);
				break;
			}
		}
		zmq_close(socket);
		zmq_ctx_destroy(context);
	}
	free(arg);
	return NULL;
}


void connect_targets()
{
	int i;
	pthread_t thread;
	pthread_attr_t attr;

	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			heartbeat_arg_t *arg;

			arg = (heartbeat_arg_t *)malloc(sizeof(heartbeat_arg_t));
			if (!arg) {
				log_err("no memory");
				return;
			}
			arg->id = i;
			tcpaddr(arg->addr, nodes[i], heartbeat_port);

			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, do_heartbeat, arg);
			pthread_attr_destroy(&attr);
		}
	}
}


int create_heartbeat()
{
	pthread_t thread;
	pthread_attr_t attr;
	responder_arg_t *arg;

	arg = (responder_arg_t *)calloc(1, sizeof(responder_arg_t));
	if (!arg) {
		log_err("no memeory");
		return -ENOMEM;
	}
	tcpaddr(arg->addr, inet_ntoa(get_addr()), heartbeat_port);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_responder, arg);
	pthread_attr_destroy(&attr);

	connect_targets();
	return 0;
}
