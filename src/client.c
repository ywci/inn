/* client.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "client.h"
#include "publisher.h"
#include "requester.h"
#include "subscriber.h"

#if defined(EVALUATE) && defined(EVAL_ECHO)
#include "evaluator.h"

#define client_tree_create rb_tree_new
#define client_node_lookup rb_tree_find
#define client_node_insert rb_tree_insert
#define client_node_remove rb_tree_remove
#define client_lock_init pthread_mutex_init

typedef rbtree_t client_tree_t;
typedef rbtree_node_t client_node_t;
typedef pthread_mutex_t client_lock_t;

typedef struct {
    int id;
    hdr_t hdr;
    client_node_t node;
} client_record_t;

struct {
    bool init;
    int snd_id;
    int rcv_id;
    int updates;
    uint64_t delay;
    timeval_t start;
    client_tree_t tree;
    client_lock_t lock;
} client_status;

int client_node_compare(const void *h1, const void *h2)
{
    assert(h1 && h2);
    return memcmp(h1, h2, sizeof(hdr_t));
}


void client_lock()
{
    pthread_mutex_lock(&client_status.lock);
}


void client_unlock()
{
    pthread_mutex_unlock(&client_status.lock);
}


inline client_record_t *client_lookup(client_tree_t *tree, hdr_t *hdr)
{
    client_node_t *node = NULL;

    if (!client_node_lookup(tree, hdr, &node))
        return tree_entry(node, client_record_t, node);
    else
        return NULL;
}


void client_add_record(zmsg_t *msg)
{
    client_record_t *rec = calloc(1, sizeof(client_record_t));

    assert(rec);
    rec->hdr = *get_hdr(msg);
    rec->id = client_status.snd_id;
    client_status.snd_id++;
    client_lock();
    if (client_node_insert(&client_status.tree, &rec->hdr, &rec->node)) {
        log_err("failed to add record");
        assert(0);
    }
    client_unlock();
}


void client_connect_evaluator()
{
    rep_t rep;
    req_t req;
    char addr[ADDR_SIZE];
    struct in_addr src = get_addr();
    int evaluator = get_evaluator(get_hid());

    memcpy(&req, &src, sizeof(req_t));
    tcpaddr(addr, nodes[evaluator], evaluator_port);
    if (request(addr, &req, &rep))
        log_err("cannot connect to evaluator");
    else
        log_debug("connect to evaluator %s", nodes[evaluator]);
}


void *client_start_listener(void *arg)
{
    void *socket;
    void *context;
    char addr[ADDR_SIZE];
#ifdef HIGH_WATER_MARK
    int hwm = HIGH_WATER_MARK;
#endif
    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_PULL);
#ifdef HIGH_WATER_MARK
    zmq_setsockopt(socket, ZMQ_RCVHWM, &hwm, sizeof(hwm));
#endif
    tcpaddr(addr, inet_ntoa(get_addr()), evaluator_port);
    if (zmq_bind(socket, addr)) {
        log_err("failed to start listener");
        return NULL;
    }

    while (true) {
        int id = -1;
        client_record_t *rec;
        zmsg_t *msg = zmsg_recv(socket);
        hdr_t *hdr = get_hdr(msg);

        client_lock();
        rec = client_lookup(&client_status.tree, hdr);
        if (rec) {
            id = rec->id;
            client_node_remove(&client_status.tree, &rec->node);
        }
        client_unlock();
        log_debug("client: record => %d", hdr->cnt);
        zmsg_destroy(&msg);
        if (rec) {
            timeval_t t;
            timeval_t now;

            if (!client_status.init) {
                get_time(client_status.start);
                client_status.init = true;
            }
            get_time(now);
            assert(id == client_status.rcv_id);
            client_status.rcv_id++;
            client_status.updates++;
            client_status.delay += time_diff(&hdr->t, &now);
            if (client_status.updates == STAT_INTV) {
                float cps; // command per second
                float delay;

                client_status.updates = 0;
                cps = client_status.rcv_id / (time_diff(&client_status.start, &now) / 1000000.0);
                delay = client_status.delay / (float)client_status.rcv_id;
                show_result("delay=%f, cps=%f", delay, cps);
                log_file("delay=%f, cps=%f", delay, cps);
            }
            free(rec);
        } else
            log_err("failed to find record");
    }
}


void client_create_listener()
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, client_start_listener, NULL);
    pthread_attr_destroy(&attr);
}
#endif


static inline zmsg_t *client_add_timestamp(zmsg_t *msg)
{
    timeval_t curr;
    zframe_t *frame;
    static bool init = false;
    static timestamp_t timestamp;
    timestamp_t *ptimestamp = &timestamp;
#ifdef CONTABLE
    static uint32_t count = 0;
    static uint32_t sec = 0;
#else
    static timeval_t t;
#endif
    if (!init) {
        timestamp.hid = get_hid();
        init = true;
    }
#ifdef CONTABLE
    get_time(curr);
    sec = curr.tv_sec;
    count++;
    timestamp_set(ptimestamp, sec, count);
#else
    do {
        get_time(curr);
    } while ((curr.tv_sec == t.tv_sec) && (curr.tv_usec == t.tv_usec));
    t = curr;
    timestamp_set(ptimestamp, curr);
#endif
    frame = zframe_new(ptimestamp, sizeof(timestamp_t));
    zmsg_prepend(msg, &frame);
#if defined(EVALUATE) && defined(EVAL_ECHO)
    client_add_record(msg);
#endif
    return msg;
}


static inline zmsg_t *client_clone_timestamp(zmsg_t *msg)
{
    zframe_t *frame = zframe_dup(zmsg_first(msg));

    zmsg_prepend(msg, &frame);
    return msg;
}


zmsg_t *client_add_index(zmsg_t *msg)
{
#ifndef CLONE_TS
    return client_add_timestamp(msg);
#else
    return client_clone_timestamp(msg);
#endif
}


int client_create()
{
    pub_arg_t *arg;
    pthread_t thread;
    pthread_attr_t attr;

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

    for (int i = 0; i < nr_nodes; i++)
        tcpaddr(arg->dest[i], nodes[i], sampler_port);

    arg->total = nr_nodes;
    arg->callback = client_add_index;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, publisher_start, arg);
    pthread_attr_destroy(&attr);

#if defined(EVALUATE) && defined(EVAL_ECHO)
    client_status.snd_id = 0;
    client_status.rcv_id = 0;
    client_status.updates = 0;
    client_status.init = false;
    if (client_tree_create(&client_status.tree, client_node_compare))
        log_err("failed to create");
    client_lock_init(&client_status.lock, NULL);
    client_create_listener();
    client_connect_evaluator();
#endif
    return 0;
}
