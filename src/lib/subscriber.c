/*      subscriber.c
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

#include "subscriber.h"

void *start_subscriber(void *ptr)
{
	int ret;
	void *context;
	void *backend;
	void *frontend;
	sub_arg_t *arg = (sub_arg_t *)ptr;
	
	log_debug("%s: src=%s, dest=%s", __func__, arg->src, arg->dest);
	if (!arg) {
		log_err("invalid argumuent");
		return NULL;
	}
	context = zmq_ctx_new();
	frontend = zmq_socket(context, ZMQ_SUB);
	backend = zmq_socket(context, ZMQ_PUSH);
	ret = zmq_connect(frontend, arg->src);
	if (!ret) {
		ret = zmq_connect(backend, arg->dest);
		if (!ret) {
			ret = zmq_setsockopt(frontend, ZMQ_SUBSCRIBE, "", 0);
			if (ret)
				log_err("failed to subscribe from %s", arg->src);
		} else
			log_err("failed to connect to destination %s", arg->dest);
	} else
		log_err("failed to connect to source %s", arg->src);
	if (!ret)
		forward(frontend, backend, NULL, NULL);
	zmq_close(frontend);
	zmq_close(backend);
	zmq_ctx_destroy(context);
	free(arg);
	return NULL;
}
