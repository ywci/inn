/* main.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <default.h>
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
    heartbeat_create();
    collector_create();
#endif
    return sampler_create();
}


void usage(char *name)
{
    printf("Usage: %s [-c | --client] [-r requests]\n", name);
    printf("-c or --client: start client\n");
    printf("-r: max requests\n");
}


int main(int argc, char **argv)
{
    int i = 1;
    bool enable_client = false;

#ifdef FUNC_TIMER
    init_func_timer();
#endif

    while (i < argc) {
        if (!strcmp(argv[i], "-r")) {
            i++;
            if (i == argc) {
                usage(argv[0]);
                exit(-1);
            } else
                nr_requests = strtol(argv[i], NULL, 0);
        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--client"))
            enable_client = true;
        else {
            usage(argv[0]);
            exit(-1);
        }
        i++;
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
        if (client_create()) {
            log_err("failed to create client");
            return -1;
        }
    }

    pause();
    return 0;
}
