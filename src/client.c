/* client.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
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
	zmsg_prepend(msg, &frame);
	return msg;
}


void init_timestamp()
{
	hid_t id = get_hid();

	gettimeofday(&t_prev, NULL);
	memset(timestamp, 0, TIMESTAMP_SIZE);
	memcpy(&timestamp[TIME_SIZE], &id, sizeof(hid_t));
}


int create_client()
{
	int i;
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;

	init_timestamp();

	arg = (pub_arg_t *)calloc(1, sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memory");
		return -ENOMEM;
	}

	strcpy(arg->src, INN_ADDR);
	if (MULTICAST == MULTICAST_PUB)
		strcpy(arg->addr, CLIENT_ADDR);
	else if (MULTICAST == MULTICAST_SUB)
		tcpaddr(arg->addr, inet_ntoa(get_addr()), client_port);
	else if (MULTICAST == MULTICAST_PGM)
		pgmaddr(arg->addr, inet_ntoa(get_addr()), client_port);
	else if (MULTICAST == MULTICAST_EPGM)
		epgmaddr(arg->addr, inet_ntoa(get_addr()), client_port);

	for (i = 0; i < nr_nodes; i++)
		tcpaddr(arg->dest[i], nodes[i], sampler_port);

	arg->total = nr_nodes;
	arg->callback = add_index;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);

	return 0;
}
