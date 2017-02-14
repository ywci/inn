/* samper.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "sampler.h"

sender_desc_t sampler_desc;

void send_message(zmsg_t *msg)
{
	publish(&sampler_desc, msg);
}


rep_t client_responder(req_t req)
{
	rep_t rep = 0;
	sub_arg_t *arg;
	pthread_t thread;
	struct in_addr addr;
	pthread_attr_t attr;

	memcpy(&addr, &req, sizeof(struct in_addr));
	log_debug("client_responder: addr=%s", inet_ntoa(addr));

	arg = (sub_arg_t *)malloc(sizeof(sub_arg_t));
	if (!arg) {
		log_err("no memory");
		req = -ENOMEM;
		return rep;
	}
	if (MULTICAST == MULTICAST_PGM)
		pgmaddr(arg->src, inet_ntoa(addr), client_port);
	else if (MULTICAST == MULTICAST_EPGM)
		epgmaddr(arg->src, inet_ntoa(addr), client_port);
	else
		tcpaddr(arg->src, inet_ntoa(addr), client_port);
	strcpy(arg->dest, INDEXER_ADDR);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_subscriber, arg);
	pthread_attr_destroy(&attr);

	return rep;
}


int create_client_responder()
{
	pthread_t thread;
	pthread_attr_t attr;
	responder_arg_t *arg;

	arg = (responder_arg_t *)calloc(1, sizeof(responder_arg_t));
	if (!arg) {
		log_err("no memeory");
		return -ENOMEM;
	}
	arg->responder = client_responder;
	tcpaddr(arg->addr, inet_ntoa(get_addr()), sampler_port);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_responder, arg);
	pthread_attr_destroy(&attr);

	return 0;
}


int create_sampler()
{
	int i, j;
	pub_arg_t *arg;
	pthread_t thread;
	pthread_attr_t attr;

	if (create_synthesizer()) {
		log_err("failed to create synth");
		return -EINVAL;
	}

	arg = (pub_arg_t *)calloc(1, sizeof(pub_arg_t));
	if (!arg) {
		log_err("no memeory");
		return -ENOMEM;
	}

	if (MULTICAST == MULTICAST_SUB) {
		strcpy(arg->src, INDEXER_ADDR);
		tcpaddr(arg->addr, inet_ntoa(get_addr()), notifier_port);
	} else if (MULTICAST == MULTICAST_PGM) {
		strcpy(arg->src, INDEXER_ADDR);
		pgmaddr(arg->addr, inet_ntoa(get_addr()), notifier_port);
	}  else if (MULTICAST == MULTICAST_EPGM) {
		strcpy(arg->src, INDEXER_ADDR);
		epgmaddr(arg->addr, inet_ntoa(get_addr()), notifier_port);
	} else if (MULTICAST == MULTICAST_PUB) {
		tcpaddr(arg->src, inet_ntoa(get_addr()), sampler_port);
		tcpaddr(arg->addr, inet_ntoa(get_addr()), synthesizer_port);
	} else if (MULTICAST == MULTICAST_PUSH)
		tcpaddr(arg->src, inet_ntoa(get_addr()), sampler_port);

	for (i = 0, j = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			if (MULTICAST == MULTICAST_PUSH) {
				tcpaddr(arg->dest[j], nodes[i], synthesizer_port + node_id);
				j++;
			} else if ((MULTICAST == MULTICAST_SUB) || (MULTICAST == MULTICAST_PGM) || (MULTICAST == MULTICAST_EPGM)) {
				tcpaddr(arg->dest[j], nodes[i], synthesizer_port);
				j++;
			}
		}
	}

	arg->total = j;
	arg->bypass = true;
	arg->desc = &sampler_desc;
	arg->callback = update_message;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_publisher, arg);
	pthread_attr_destroy(&attr);

	if ((MULTICAST ==  MULTICAST_SUB) || (MULTICAST == MULTICAST_PGM) || (MULTICAST == MULTICAST_EPGM))
		create_client_responder();

	return 0;
}
