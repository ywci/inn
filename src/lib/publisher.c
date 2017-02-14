/* publisher.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "publisher.h"
#include "responder.h"
#include "subscriber.h"

void init_pub(char *src, char *dest)
{
	sub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;

	arg = (sub_arg_t *)malloc(sizeof(sub_arg_t));
	if (!arg) {
		log_err("no memory");
		return;
	}
	strcpy(arg->src, src);
	strcpy(arg->dest, dest);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_subscriber, arg);
	pthread_attr_destroy(&attr);
}


void *init_push(char *addr)
{
	int ret;
	void *socket = NULL;
	void *context = NULL;

	context = zmq_ctx_new();
	socket = zmq_socket(context, ZMQ_PUSH);
	ret = zmq_connect(socket, addr);
	if (ret) {
		log_err("failed to connect to %s", addr);
		zmq_ctx_destroy(context);
		return NULL;
	}

	return socket;
}


void init_req(char *addr)
{
	rep_t rep;
	req_t req;
	struct in_addr src = get_addr();

	memcpy(&req, &src, sizeof(req_t));
	if (request(addr, &req, &rep))
		log_err("failed to initialize %s", addr);
}


void connect_subscribers(pub_arg_t *arg, void *endpoints[NODE_MAX])
{
	int i;

	for (i = 0; i < arg->total; i++) {
		switch(arg->type) {
		case MULTICAST_SUB:
		case MULTICAST_PGM:
		case MULTICAST_EPGM:
			init_req(arg->dest[i]);
			break;
		case MULTICAST_PUB:
			init_pub(arg->addr, arg->dest[i]);
			break;
		case MULTICAST_PUSH:
			endpoints[i] = init_push(arg->dest[i]);
			break;
		default:
			log_err("failed to connect to subscribers, invalid multicast type");
			break;
		}
	}
}


void *start_publisher(void *ptr)
{
	int i;
	int ret;
	callback_t callback;
	sender_desc_t sender;
	void *context = NULL;
	void *backend = NULL;
	void *frontend = NULL;
	void *endpoints[NODE_MAX];
	pub_arg_t *arg = (pub_arg_t *)ptr;

	if (!arg) {
		log_err("invalid argument");
		return NULL;
	}

	log_func("src=%s", arg->src);
	if (strlen(arg->addr) > 0)
		log_func("addr=%s", arg->addr);
	for (i = 0; i < arg->total; i++)
		log_func("dest=%s", arg->dest[i]);

	if (!arg->type)
		arg->type = MULTICAST;

	memset(&sender, 0, sizeof(sender_desc_t));
	sender.sender = arg->sender;
	callback = arg->callback;
	context = zmq_ctx_new();
	frontend = zmq_socket(context, ZMQ_PULL);
	if (strlen(arg->addr) > 0)
		backend = zmq_socket(context, ZMQ_PUB);
	ret = zmq_bind(frontend, arg->src);
	if (!ret) {
		if (strlen(arg->addr) > 0)
			ret = zmq_bind(backend, arg->addr);
		if (!ret) {
			connect_subscribers(arg, endpoints);
			if (arg->type != MULTICAST_PUSH) {
				sender.desc[0] = backend;
				sender.total = 1;
			} else {
				for (i = 0; i < arg->total; i++)
					sender.desc[i] = endpoints[i];
				sender.total = arg->total;
			}

			if (arg->desc)
				memcpy(arg->desc, &sender, sizeof(sender_desc_t));
			
			if (!arg->bypass)
				forward(frontend, backend, callback, &sender);
			else {
				if (callback) {
					while (true) {
						zmsg_t *msg = zmsg_recv(frontend);

						msg = callback(msg);
						if (msg)
							zmsg_destroy(&msg);
					}
				} else
					ret = -EINVAL;
			}
		}
	}

	if (ret)
		log_err("failed to start publisher");

	zmq_close(frontend);
	zmq_close(backend);
	zmq_ctx_destroy(context);
	free(arg);
	return NULL;
}
