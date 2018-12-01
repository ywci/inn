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
#include "util.h"
#include "log.h"

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
    printf("Usage: %s [-c | --client] [-o filename] [-i interval]\n", name);
    printf("-c or --client   : act as a client\n");
    printf("-o or --output   : the name of log file\n");
    printf("-i or --interval : modify the EVAL_INTV (the output file can be generated after processing EVAL_INTV requests by default)\n");
}


int main(int argc, char **argv)
{
    int i = 1;
    bool enable_client = false;

    // default settings ///////
    eval_intv = EVAL_INTV;
    strcpy(log_name, PATH_LOG);
    ///////////////////////////

    check_settings();
#ifdef FUNC_TIMER
    init_func_timer();
#endif
    while (i < argc) {
        if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--interval")) {
            i++;
            if (i == argc) {
                usage(argv[0]);
                exit(-1);
            } else {
                int n = strtol(argv[i], NULL, 0);

                if (n <= 0) {
                    usage(argv[0]);
                    exit(-1);
                }
                eval_intv = n;
            }
        } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            i++;
            if (i == argc) {
                usage(argv[0]);
                exit(-1);
            } else
                strcpy(log_name, argv[i]);
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
    log_file_remove();
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
