/*      client.c
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

#include "client.h"

struct timeval t_prev;
char timestamp[TIMESTAMP_SIZE];

zmsg_t *add_index(zmsg_t *msg)
{
	zframe_t *frame;
	struct timeval tv;
	
	while (true) {
		gettimeofday(&tv, NULL);
		if (memcmp(&tv, &t_prev, sizeof(struct timeval))) {
			t_prev = tv;
			break;
		}
	}
	set_timestamp(timestamp, &tv);
	frame = zframe_new(timestamp, TIMESTAMP_SIZE);
#ifdef SHOW_TIMESTAMP
	show_timestamp("client", -1, (char *)zframe_data(frame));
#endif
	zmsg_prepend(msg, &frame);
	return msg;
}


void init_timestamp()
{
	struct in_addr addr = get_addr();
	
	gettimeofday(&t_prev, NULL);
	memset(timestamp, 0, TIMESTAMP_SIZE);
	memcpy(&timestamp[7], &addr, 4);
}


pthread_t start_client(char *addr)
{
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;
	
	init_timestamp();
	arg = (pub_arg_t *)malloc(sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memory");
		return -1;
	}

	arg->sender =NULL;
	arg->publisher = NULL;
	arg->callback = add_index;
	strcpy(arg->src, CLIENT_ADDR);
	strcpy(arg->dest, addr);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);
	
	return thread;
}


void connect_sequencers()
{
	int i;
	sub_arg_t *args;
	
	args = (sub_arg_t *)malloc(nr_nodes * sizeof(sub_arg_t));
	if (!args) {
		log_err("no memory");
		return;
	}
	
	for (i = 0; i < nr_nodes; i++) {
		pthread_t thread;
		pthread_attr_t attr;
		
		strcpy(args[i].src, INN_ADDR);
		sprintf(args[i].dest, "tcp://%s:%d", nodes[i], sequencer_port);
		
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		pthread_create(&thread, &attr, start_subscriber, &args[i]);
		pthread_attr_destroy(&attr);
	}
}


void *notify(void *ptr)
{
	int ret;
	req_t req;
	rep_t rep;
	notify_arg_t *arg = (notify_arg_t *)ptr;
	
	memcpy(&req, &arg->src, sizeof(req_t));
	ret = request(arg->dest, &req, &rep);
	if (ret)
		log_err("failed to notify %s", arg->dest);
	return NULL;
}


void notify_sequencers()
{
	int i;
	notify_arg_t *args;
	
	args = (notify_arg_t *)malloc(nr_nodes * sizeof(notify_arg_t));
	if (!args) {
		log_err("no memory");
		return;
	}
	
	for (i = 0; i < nr_nodes; i++) {
		pthread_t thread;
		pthread_attr_t attr;
		
		args[i].src = get_addr();
		sprintf(args[i].dest, "tcp://%s:%d", nodes[i], sequencer_port);
		
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		pthread_create(&thread, &attr, notify, &args[i]);
		pthread_attr_destroy(&attr);
	}
}


void create_client()
{
	pthread_t thread;
	char addr[ADDR_SIZE];
	
#ifdef LIGHT_CLIENT
	sprintf(addr, "tcp://*:%d", client_port);
#else
	strcpy(addr, INN_ADDR);
#endif
	thread = start_client(addr);
	if (thread < 0) {
		log_err("failed to create client");
		return;
	}
#ifdef LIGHT_CLIENT
	notify_sequencers();
#else
	connect_sequencers();
#endif
	pthread_join(thread, NULL);
}
