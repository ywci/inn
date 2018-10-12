/* sampler.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "batch.h"
#include "sampler.h"
#include "record.h"
#include "responder.h"
#include "collector.h"
#include "heartbeat.h"
#include "publisher.h"
#include "subscriber.h"
#include "synthesizer.h"
#ifdef VERIFY
#include "verify.h"
#endif

#define SAMPLER_QUEUE_LEN 1000000
#define sampler_need_save() (!sampler_status.t_filter.sec || !sampler_status.t_filter.usec)

typedef struct {
    zmsg_t *msg;
    struct list_head list;
} sampler_record_t;

struct {
    int count;
    bool active;
    bool filter;
    sender_desc_t desc;
    pthread_cond_t cond;
    pthread_mutex_t lock;
    host_time_t t_filter;
    pthread_mutex_t mutex;
    struct list_head queue;
} sampler_status;

void send_message(zmsg_t *msg)
{
    publish(&sampler_status.desc, msg);
}


inline void sampler_lock()
{
    pthread_mutex_lock(&sampler_status.lock);
}


inline void sampler_unlock()
{
    pthread_mutex_unlock(&sampler_status.lock);
}


inline int sampler_time_compare(host_time_t *t1, host_time_t *t2)
{
    if (t1->sec > t2->sec)
        return 1;
    else if (t1->sec == t2->sec) {
        if (t1->usec > t2->usec)
            return 1;
        else if (t1->usec == t2->usec)
            return 0;
        else
            return -1;
    } else
        return -1;
}


inline void sampler_do_handle(zmsg_t *msg)
{
    zmsg_t *ret = batch(msg);

    if (ret)
        send_message(ret);
}


void sampler_suspend()
{
    crash_details("start");
    sampler_lock();
    sampler_status.active = false;
    sampler_unlock();
    crash_details("finished!");
}


void sampler_do_suspend()
{
    pthread_cond_t *cond = &sampler_status.cond;
    pthread_mutex_t *mutex = &sampler_status.mutex;

    sampler_unlock();
    pthread_mutex_lock(mutex);
    pthread_cond_wait(cond, mutex);
    pthread_mutex_unlock(mutex);
}


void sampler_wakeup()
{
    pthread_cond_t *cond = &sampler_status.cond;
    pthread_mutex_t *mutex = &sampler_status.mutex;

    pthread_mutex_lock(mutex);
    pthread_cond_signal(cond);
    pthread_mutex_unlock(mutex);
}


void sampler_resume()
{
    sampler_record_t *rec;
    sampler_record_t *next;

    crash_details("start");
    sampler_lock();
    sampler_status.active = true;
    memset(&sampler_status.t_filter, 0, sizeof(host_time_t));
    list_for_each_entry_safe(rec, next, &sampler_status.queue, list) {
        zmsg_t *msg = rec->msg;
        host_time_t *t = (host_time_t *)get_timestamp(msg);

        sampler_do_handle(msg);
        list_del(&rec->list);
        free(rec);
    }
    sampler_status.count = 0;
    sampler_unlock();
    crash_details("finished!");
}


void sampler_start_filter(host_time_t bound)
{
    log_debug("sampler: start filter, bound={sec:%d, usec:%d} (session=%d)", bound.sec, bound.usec, get_session(node_id));
    sampler_lock();
    sampler_status.t_filter = bound;
    sampler_status.filter = true;
    sampler_unlock();
    sampler_wakeup();
}


void sampler_stop_filter()
{
    log_debug("sampler: stop filter (session=%d)", get_session(node_id));
    sampler_lock();
    sampler_status.filter = false;
    sampler_unlock();
    sampler_wakeup();
}


void sampler_drain()
{
    log_func("sampler: drain (session=%d)", get_session(node_id));
    while(!synth_drain());
}


zmsg_t *sampler_do_filter(zmsg_t *msg)
{
    bool active;
    host_time_t *t = (host_time_t *)get_timestamp(msg);

retry:
    sampler_lock();
    active = sampler_status.active;
    if (active || (sampler_status.filter && (sampler_time_compare(&sampler_status.t_filter, t) >= 0)))
        sampler_do_handle(msg);
    else {
        if (sampler_need_save()) {
            sampler_record_t *rec = malloc(sizeof(sampler_record_t));

            assert(rec);
            rec->msg = msg;
            list_add_tail(&rec->list, &sampler_status.queue);
            assert(sampler_status.count < SAMPLER_QUEUE_LEN);
            sampler_status.count++;
        } else if (!active) {
            log_func("suspend (session=%d)", get_session(node_id));
            debug_crash_before_suspend();
            sampler_do_suspend();
            goto retry;
        }
    }
    sampler_unlock();
    return NULL;
}


void sampler_handle(int id, zmsg_t *msg)
{
    if (is_batched(msg))
        batch_update(id, msg);
    else
        sampler_do_handle(msg);
}


zmsg_t *sampler_update_msg(zmsg_t *msg)
{
#ifdef VERIFY
    verify_input(msg);
#endif
    return sampler_do_filter(msg);
}


rep_t sampler_client_responder(req_t req)
{
    sub_arg_t *arg;
    pthread_t thread;
    struct in_addr addr;
    pthread_attr_t attr;

    memcpy(&addr, &req, sizeof(struct in_addr));
    log_func("addr=%s", inet_ntoa(addr));

    arg = (sub_arg_t *)malloc(sizeof(sub_arg_t));
    if (!arg) {
        log_err("no memory");
        return -ENOMEM;
    }
    if (MULTICAST == MULTICAST_PGM)
        pgmaddr(arg->src, inet_ntoa(addr), client_port);
    else if (MULTICAST == MULTICAST_EPGM)
        epgmaddr(arg->src, inet_ntoa(addr), client_port);
    else
        tcpaddr(arg->src, inet_ntoa(addr), client_port);
    strcpy(arg->dest, SAMPLER_ADDR);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, subscriber_start, arg);
    pthread_attr_destroy(&attr);
    return 0;
}


int sampler_create_responder()
{
    pthread_t thread;
    pthread_attr_t attr;
    responder_arg_t *arg;

    arg = (responder_arg_t *)calloc(1, sizeof(responder_arg_t));
    if (!arg) {
        log_err("no memeory");
        return -ENOMEM;
    }
    arg->responder = sampler_client_responder;
    tcpaddr(arg->addr, inet_ntoa(get_addr()), sampler_port);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, responder_start, arg);
    pthread_attr_destroy(&attr);
    return 0;
}


void sampler_init()
{
    batch_init();
    sampler_status.count = 0;
    sampler_status.active = true;
    sampler_status.filter = false;
    INIT_LIST_HEAD(&sampler_status.queue);
    pthread_cond_init(&sampler_status.cond, NULL);
    pthread_mutex_init(&sampler_status.lock, NULL);
    pthread_mutex_init(&sampler_status.mutex, NULL);
    memset(&sampler_status.t_filter, 0, sizeof(host_time_t));
}


int sampler_create()
{
    int i, j;
    pub_arg_t *arg;
    pthread_t thread;
    pthread_attr_t attr;

    sampler_init();
    if (synth_create()) {
        log_err("failed to create synth");
        return -EINVAL;
    }
    arg = (pub_arg_t *)calloc(1, sizeof(pub_arg_t));
    if (!arg) {
        log_err("no memeory");
        return -ENOMEM;
    }
    if (MULTICAST == MULTICAST_SUB) {
        strcpy(arg->src, SAMPLER_ADDR);
        tcpaddr(arg->addr, inet_ntoa(get_addr()), notifier_port);
    } else if (MULTICAST == MULTICAST_PGM) {
        strcpy(arg->src, SAMPLER_ADDR);
        pgmaddr(arg->addr, inet_ntoa(get_addr()), notifier_port);
    }  else if (MULTICAST == MULTICAST_EPGM) {
        strcpy(arg->src, SAMPLER_ADDR);
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
    arg->callback = sampler_update_msg;
    arg->desc = &sampler_status.desc;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, publisher_start, arg);
    pthread_attr_destroy(&attr);

    if ((MULTICAST ==  MULTICAST_SUB) || (MULTICAST == MULTICAST_PGM) || (MULTICAST == MULTICAST_EPGM))
        sampler_create_responder();

    return 0;
}
