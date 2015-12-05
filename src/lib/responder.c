/*      responder.c
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

#include "responder.h"

void *start_responder(void *ptr)
{
	int ret;
	void *socket;
    void *context;
	responder_t callback;
	responder_arg_t *arg = (responder_arg_t *)ptr;
	
	if (!arg) {
		log_err("invalid argument");
		return NULL;
	}
	
	callback = arg->responder;
	context = zmq_ctx_new();
	socket = zmq_socket(context, ZMQ_REP);
	ret = zmq_bind(socket, arg->addr);
	free(arg);
	if (ret) {
		log_err("failed to start responder");
		return NULL;
	}
    while (1) {
        req_t req;
		rep_t rep;
		
        zmq_recv(socket, &req, sizeof(req_t), 0);
		if (callback)
			rep = callback(req);
		else
			memset(&rep, 0, sizeof(rep_t));
        zmq_send(socket, &rep, sizeof(rep_t), 0);
    }
	
    return NULL;
}
