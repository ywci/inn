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

#include "heartbeat.h"

void handle_fault(int id)
{
	int ret;
	coll_arg_t arg;
    void *context = zmq_ctx_new();
    void *socket = zmq_socket(context, ZMQ_PUSH);
	
	log_debug("%s: id=%d", __func__, id);
	suspend_mixer();
	set_inactive(id);
	bitmap_available &= ~node_mask[id];
	arg.bitmap = bitmap_available;
	arg.seq = get_seq(id);
	arg.id = node_id;
	arg.target = id;
    ret = zmq_connect(socket, COLL_FRONTEND_ADDR);
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


void *keep_heartbeat(void *ptr)
{
	int cnt;
	int ret;
	rep_t rep;
	void *socket;
	void *context;
	req_t req = HEARTBEAT_COMMAND;
	heartbeat_arg_t *arg = (heartbeat_arg_t *)ptr;
	
	log_debug("%s: addr=%s", __func__, arg->addr);
	ret = request(arg->addr, &req, &rep);
	if (ret || rep) {
		log_err("failed to start");
		return NULL;

	}
	while (1) {
		context = zmq_ctx_new();
		socket = zmq_socket(context, ZMQ_REQ);
		ret = zmq_connect(socket, arg->addr);
		if (ret) {
			log_err("failed to connect");
			sleep(HEARTBEAT_WAITTIME);
			continue;
		}
		cnt = 0;
		while (1) {
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
				if (bitmap_available & node_mask[arg->id])
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
	int cnt = 0;
	pthread_t thread;
	pthread_attr_t attr;
	heartbeat_arg_t *args;
	
	args = (heartbeat_arg_t *)malloc(sizeof(heartbeat_arg_t) * (nr_nodes - 1));
	if (!args) {
		log_err("no memory");
		return;
	}
	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {			
			sprintf(args[cnt].addr, "tcp://%s:%d", nodes[i], heartbeat_port);
			args[cnt].id = i;
			
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, keep_heartbeat, &args[cnt]);
			pthread_attr_destroy(&attr);
			cnt++;
		}
	}
}


void start_heartbeat()
{
	pthread_t thread;
	pthread_attr_t attr;
	responder_arg_t *arg;
	
	arg = (responder_arg_t *)malloc(sizeof(responder_arg_t));
	if (!arg) {
		log_err("no memeory");
		return;
	}
	
	arg->responder = NULL;
	sprintf(arg->addr, "tcp://*:%d", heartbeat_port);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_responder, arg);
	pthread_attr_destroy(&attr);
}


void create_heartbeat()
{
	connect_targets();
	start_heartbeat();
}
