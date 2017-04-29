/* main.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <default.h>
#include "replayer.h"
#include "collector.h"
#include "heartbeat.h"
#include "sampler.h"
#include "client.h"
#include "parser.h"
#include "log.h"

int nr_requests = 0;

int create_server()
{
#ifdef HEARTBEAT
	create_replayer();
	create_heartbeat();
	create_collector();
#endif
	return create_sampler();
}


int main(int argc, char **argv)
{

	int opt;
	bool enable_client = false;

	while ((opt = getopt(argc, argv, "Cc:")) != -1) {
		switch(opt) {
		case 'C':
			enable_client = true;
			break;
		case 'c':
			nr_requests = strtol(optarg, NULL, 0);
			break;
		default:
			printf("Usage: %s [-C] [-c count]\n", argv[0]);
			printf("-C: start client\n");
			exit(-1);
		}
	}

	if (parse()) {
		log_err("failed to load configuration");
		return -1;
	}

	remove(PATH_LOG);
	if (node_id >= 0) {
		if (create_server()) {
			log_err("failed to create server");
			return -1;
		}
	}

	if (enable_client) {
		if (create_client()) {
			log_err("failed to create client");
			return -1;
		}
	}

	pause();
	return 0;
}
