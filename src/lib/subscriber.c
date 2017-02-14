/* subscriber.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "subscriber.h"

void *start_subscriber(void *ptr)
{
	int ret;
	void *context;
	void *backend;
	void *frontend;
	sub_arg_t *arg = (sub_arg_t *)ptr;

	if (!arg) {
		log_err("invalid argumuent");
		return NULL;
	}

	log_func("src=%s, dest=%s", arg->src, arg->dest);
	context = zmq_ctx_new();
	frontend = zmq_socket(context, ZMQ_SUB);
	backend = zmq_socket(context, ZMQ_PUSH);

	ret = zmq_connect(frontend, arg->src);
	if (!ret) {
		ret = zmq_setsockopt(frontend, ZMQ_SUBSCRIBE, "", 0);
		if (!ret) {
			ret = zmq_connect(backend, arg->dest);
			if (!ret)
				forward(frontend, backend, NULL, NULL);
		}
	}

	if (ret)
		log_err("failed to start subscriber");

	zmq_close(frontend);
	zmq_close(backend);
	zmq_ctx_destroy(context);

	free(arg);
	return NULL;
}
