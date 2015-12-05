/*      publisher.c
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

#include "publisher.h"

void *start_publisher(void *ptr)
{
	int ret;
	void *context;
	void *backend;
	void *frontend;
	sender_t sender;
	void **publisher;
	callback_t callback;
	pub_arg_t *arg = (pub_arg_t *)ptr;
	
	if (!arg) {
		log_err("invalid argument");
		return NULL;
	}
	
	log_debug("%s: src=%s, dest=%s", __func__, arg->src, arg->dest);
	sender = arg->sender;
	callback = arg->callback;
	publisher = arg->publisher;
	
	context = zmq_ctx_new();
	frontend = zmq_socket(context, ZMQ_PULL);
	backend = zmq_socket(context, ZMQ_PUB);
	ret = zmq_bind(frontend, arg->src);
	if (!ret) {
		ret = zmq_bind(backend, arg->dest);
		if (ret)
			log_err("failed to bind to destination %s", arg->dest);
		else if(publisher)
			*publisher = backend;
	} else
		log_err("failed to bind to source %s", arg->src);
	if (!ret)
		forward(frontend, backend, callback, sender);
	zmq_close(frontend);
	zmq_close(backend);
	zmq_ctx_destroy(context);
	free(arg);
	return NULL;
}
