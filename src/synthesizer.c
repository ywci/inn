/* synthesizer.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "ev.h"
#include "queue.h"
#include "batch.h"
#include "record.h"
#include "handler.h"
#include "sampler.h"
#include "responder.h"
#include "requester.h"
#include "timestamp.h"
#include "synthesizer.h"

#define SYNTH_CHECK_INTV        5000000 // nsec

#define SYNTH_QUEUE_CHECKER
#define SYNTH_RECHECK_IGNORE

typedef struct synth_arg {
    int id;
    int type;
    char addr[ADDR_SIZE];
} synth_arg_t;

typedef struct synth_entry {
    record_t *record;
    struct list_head list;
} synth_entry_t;

struct {
    bool busy;
    ev_t ev_deliver;
    pthread_mutex_t mutex;
    ev_t ev_live[NODE_MAX];
    struct list_head output;
    pthread_mutex_t deliver_lock;
    liveness_t liveness[NODE_MAX];
    pthread_mutex_t locks[NODE_MAX];
    struct list_head pass[NODE_MAX];
    struct list_head input[NODE_MAX];
    struct list_head req_list[NODE_MAX];
    pthread_mutex_t recv_locks[NODE_MAX];
    struct list_head candidates[NODE_MAX];
} synth_status;

#define synth_list_head_init(phead, pnext) do { \
    phead->next = pnext; \
    phead->prev = pnext; \
    pnext->next = phead; \
    pnext->prev = phead; \
} while (0)

#define synth_list_del(p) do { \
    list_del(p);               \
    set_empty(p);              \
} while (0)

#ifdef SYNTH_RECHECK_IGNORE
#define synth_recheck_ignore(rec) do { \
    if (!(rec)->ignore                 \
        && ((rec)->quorum < majority)  \
        && (((rec)->receivers & available_nodes) == available_nodes)) \
        (rec)->ignore = true;          \
} while (0)
#else
#define synth_recheck_ignore(...) do {} while (0)
#endif

#define synth_lock(id) pthread_mutex_lock(&synth_status.locks[id])
#define synth_unlock(id) pthread_mutex_unlock(&synth_status.locks[id])

#define synth_mutex_lock() pthread_mutex_lock(&synth_status.mutex)
#define synth_mutex_unlock() pthread_mutex_unlock(&synth_status.mutex)

#define synth_recv_lock(id) pthread_mutex_lock(&synth_status.recv_locks[id])
#define synth_recv_unlock(id) pthread_mutex_unlock(&synth_status.recv_locks[id])

#define synth_deliver_lock() pthread_mutex_lock(&synth_status.deliver_lock)
#define synth_deliver_unlock() pthread_mutex_unlock(&synth_status.deliver_lock)

#define synth_list_add assert_list_add
#define synth_list_add_tail assert_list_add_tail
#define synth_can_deliver(rec) (((rec)->quorum >= majority) && !is_delivered(rec))

void synth_deliver(record_t *record)
{
    bool wakeup = false;

    synth_status.busy = true;
    synth_deliver_lock();
    if (!is_delivered(record)) {
        record_deliver(record);
        if (list_empty(&synth_status.output))
            wakeup = true;
        synth_list_add_tail(&record->output, &synth_status.output);
        show_deliver(record);
    }
    synth_deliver_unlock();
    if (wakeup)
        ev_set(&synth_status.ev_deliver);
}


static inline void synth_do_pass(int id, struct list_head *head, struct list_head *pos, bool ignore)
{
    assert(head && is_valid(head));
    if (pos) {
        bool empty = false;
        record_t *rec = NULL;
        struct list_head *pass = NULL;
        struct list_head *candidates = &synth_status.candidates[id];

        while (pos != candidates) {
            rec = list_entry(pos, record_t, cand[id]);
            pass = &rec->pass[id];
            if (!is_delivered(rec)) {
                empty = is_empty(pass);
                if (!ignore)
                    synth_recheck_ignore(rec);
                if (empty && rec->ignore) {
                    synth_list_add(pass, head);
                    if (!rec->prev[id] && !rec->count[id]) {
                        rec->count[id] = true;
                        rec->quorum++;
                        if (synth_can_deliver(rec)) {
                            synth_deliver(rec);
                            return;
                        }
                    }
                } else
                    break;
            }
            if (is_valid(pass))
                head = pass;
            pos = pos->next;
        }
        if ((pos != candidates) && empty && !rec->count[id] && !rec->prev[id]) {
            rec->count[id] = true;
            rec->quorum++;
            if (synth_can_deliver(rec))
                synth_deliver(rec);
        }
    }
}


static inline void synth_pass(record_t *record)
{
    if (!record->ignore && ((record->receivers & available_nodes) == available_nodes)) {
        for (int id = 0; id < nr_nodes; id++) {
            struct list_head *cand = &record->cand[id];

            if (is_valid(cand)) {
                struct list_head *head = NULL;
                struct list_head *prev = cand->prev;
                struct list_head *candidates = &synth_status.candidates[id];

                if (prev != candidates) {
                    record_t *rec = list_entry(prev, record_t, cand[id]);

                    head = &rec->pass[id];
                    if (is_empty(head)) {
                        head = NULL;
                        if (is_delivered(rec)) {
                            prev = prev->prev;
                            while (prev != candidates) {
                                rec = list_entry(prev, record_t, cand[id]);
                                if (is_delivered(rec))
                                    prev = prev->prev;
                                else {
                                    if (is_valid(&rec->pass[id]))
                                        head = &rec->pass[id];
                                    break;
                                }
                            }
                            if (prev == candidates)
                                head = &synth_status.pass[id];
                        }
                    }
                } else
                    head = &synth_status.pass[id];

                if (head) {
                    struct list_head *pos = cand->next;
                    struct list_head *pass = &record->pass[id];

                    assert(is_empty(pass));
                    synth_list_add(pass, head);
                    synth_do_pass(id, pass, pos, true);
                }
            }
        }
        assert(!record->ignore);
        record->ignore = true;
    }
}


void synth_check_quorum(int id, record_t *record)
{
    bool valid = true;
    struct list_head *last;

    if (is_empty(&record->cand[id])) {
        struct list_head *input = &record->input[id];

        assert(is_empty(input));
        synth_list_add_tail(input, &synth_status.input[id]);
        synth_wakeup();
        return;
    }
    synth_mutex_lock();
    if (is_valid(&record->input[id]))
        record->receivers |= node_mask[id];
    last = record->cand[id].prev;
    assert(last);
    if (last != &synth_status.candidates[id]) {
        record_t *rec = list_entry(last, record_t, cand[id]);

        valid = rec->count[id] || is_valid(&rec->pass[id]);
        if (!valid)
            show_blocker(id, rec, record);
    }
    if (valid && !record->count[id]) {
        record->count[id] = true;
        record->quorum++;
        if (record->quorum >= majority)
            if (!is_delivered(record))
                synth_deliver(record);
    }
    if (record->quorum < majority)
        synth_pass(record);
    synth_mutex_unlock();
    show_quorum(id, record);
}


inline bool synth_check_next(int id, record_t *rec_next, record_t *rec_prev)
{
    bool valid = rec_prev ? (is_valid(&rec_prev->pass[id]) || rec_prev->count[id]) : true;

    synth_recheck_ignore(rec_next);
    if (valid && !rec_next->count[id] && !rec_next->prev[id]) {
        rec_next->count[id] = true;
        rec_next->quorum++;
        if (synth_can_deliver(rec_next))
            synth_deliver(rec_next);
        return true;
    } else
        return false;
}


void synth_delete_entry(int id, record_t *record)
{
    record_t *rec_next = NULL;
    record_t *rec_prev = NULL;
    record_t *pprev = record->prev[id];
    struct list_head *req = &record->req[id];
    struct list_head *pass = &record->pass[id];
    struct list_head *cand = &record->cand[id];
    struct list_head *next = &record->next[id];
    struct list_head *input = &record->input[id];
    struct list_head *head = &synth_status.pass[id];
    struct list_head *candidates = &synth_status.candidates[id];

    show_dequeue(id, record->timestamp);
    if (pprev) {
        assert(timestamp_compare(pprev->timestamp, record->timestamp) < 0);
        synth_list_del(&record->link[id]);
    }
    if (is_valid(next) && !list_empty(next)) {
        struct list_head *i;
        struct list_head *j;
        record_t *prev = NULL;

        for (i = next->next, j = i->next; i != next; i = j, j = j->next) {
            record_t *rec = list_entry(i, record_t, link[id]);

            prev = queue_update_prev(id, record, rec);
            if (pprev && !prev) {
                show_prev(id, rec, pprev, record);
                log_err("failed to find prev");
            }
            if (prev) {
                struct list_head *prev_next = &prev->next[id];

                if (is_empty(prev_next))
                    synth_list_head_init(prev_next, i);
                else
                    synth_list_add_tail(i, prev_next);
                rec->prev[id] = prev;
                show_prev(id, rec, prev, record);
            } else {
                set_empty(i);
                rec->prev[id] = NULL;
                show_prev(id, rec, NULL, record);
                synth_check_quorum(id, rec);
            }
        }
    }

    if (is_valid(cand)) {
        if (cand->next != candidates)
            rec_next = list_entry(cand->next, record_t, cand[id]);
        if (cand->prev != candidates)
            rec_prev = list_entry(cand->prev, record_t, cand[id]);
    }
    synth_mutex_lock();
    if (rec_prev)
        head = &rec_prev->pass[id];
    if (is_valid(pass))
        synth_list_del(pass);
    if (rec_next) {
        synth_check_next(id, rec_next, rec_prev);
        if (is_valid(head))
            synth_do_pass(id, head, &rec_next->cand[id], true);
    }
    if (is_valid(cand))
        synth_list_del(cand);
    synth_mutex_unlock();
    queue_pop(id, record);
    if (is_valid(input))
        synth_list_del(input);
    if (is_valid(req))
        synth_list_del(req);
}


void synth_update_queue(int id, record_t *record)
{
    bool earliest = false;

    show_enqueue(id, record->timestamp);
    queue_push(id, record, &earliest);
    synth_list_add_tail(&record->req[id], &synth_status.req_list[id]);
    if (earliest) {
        synth_list_add_tail(&record->input[id],  &synth_status.input[id]);
        synth_wakeup();
    }
}


static inline record_t *synth_do_check_output()
{
    record_t *record = NULL;

    synth_deliver_lock();
    if (!list_empty(&synth_status.output)) {
        struct list_head *head = synth_status.output.next;

        record = list_entry(head, record_t, output);
        synth_list_del(head);
    }
    synth_deliver_unlock();
    return record;
}


bool synth_check_output()
{
    record_t *rec = synth_do_check_output();

    if (rec) {
        hid_t hid;
        timeval_t t;
        zmsg_t *msg = rec->msg;
        zframe_t *frame = zmsg_last(msg);
        size_t size = zframe_size(frame);
        char *buf = (char *)zframe_data(frame);

        for (int i = 0; i < nr_nodes; i++) {
            synth_lock(i);
            if (!is_empty(&rec->item_list[i]))
                synth_delete_entry(i, rec);
            synth_unlock(i);
        }
        handle(buf, size);
        record_release(rec);
        return true;
    } else
        return false;
}


bool synth_check_input()
{
    bool ret = false;

    for (int id = 0; id < nr_nodes; id++) {
        struct list_head *i;
        struct list_head *j;
        bitmap_t mask = node_mask[id];
        struct list_head *pass = &synth_status.pass[id];
        struct list_head *input = &synth_status.input[id];
        struct list_head *req_list = &synth_status.req_list[id];
        struct list_head *candidates = &synth_status.candidates[id];

        synth_lock(id);
        if (!list_empty(req_list)) {
            synth_mutex_lock();
            for (i = req_list->next, j = i->next; i != req_list; i = j, j = j->next) {
                record_t *rec = list_entry(i, record_t, req[id]);

                if (is_empty(&rec->input[id]))
                    rec->receivers |= mask;

                if (!is_delivered(rec)) {
                    synth_list_add_tail(&rec->cand[id], candidates);
                    if (rec->quorum < majority)
                        synth_pass(rec);
                }
                synth_list_del(i);
            }
            synth_mutex_unlock();
            ret = true;
        }
        if (!list_empty(input)) {
            for (i = input->next, j = i->next; i != input; i = j, j = j->next) {
                record_t *rec = list_entry(i, record_t, input[id]);

                if (!is_delivered(rec))
                    synth_check_quorum(id, rec);
                synth_list_del(i);
            }
            ret = true;
        }
        synth_unlock(id);
    }
    return ret;
}


static inline void synth_put(int id, record_t *record)
{
    track_enter();
    if (is_empty(&record->item_list[id]))
        synth_update_queue(id, record);
    track_exit();
}


static inline void synth_do_update(int id, timestamp_t *timestamp, zmsg_t *msg)
{
    track_enter();
    if (timestamp_check(timestamp)) {
        record_t *rec = record_find(id, timestamp, msg);

        if (rec) {
            if (!is_delivered(rec))
                synth_put(id, rec);
            record_put(id, rec);
        }
    }
    track_exit();
}


void synth_update(int id, timestamp_t *timestamp, zmsg_t *msg)
{
    synth_lock(id);
    synth_do_update(id, timestamp, msg);
    synth_unlock(id);
}


void synth_wakeup()
{
    ev_set(&synth_status.ev_deliver);
}


int synth_init()
{
    if (nr_nodes <= 0) {
        log_err("failed to initialize synthesizer");
        return -1;
    }
    queue_init();
    synth_status.busy = false;
    INIT_LIST_HEAD(&synth_status.output);
    ev_init(&synth_status.ev_deliver, DELIVER_TIMEOUT);
    for (int i = 0; i < NODE_MAX; i++) {
        INIT_LIST_HEAD(&synth_status.pass[i]);
        INIT_LIST_HEAD(&synth_status.input[i]);
        INIT_LIST_HEAD(&synth_status.req_list[i]);
        INIT_LIST_HEAD(&synth_status.candidates[i]);
        pthread_mutex_init(&synth_status.locks[i], NULL);
        pthread_mutex_init(&synth_status.recv_locks[i], NULL);
        ev_init(&synth_status.ev_live[i], EV_NOTIMEOUT);
        synth_status.liveness[i] = ALIVE;
    }
    pthread_mutex_init(&synth_status.deliver_lock, NULL);
    pthread_mutex_init(&synth_status.mutex, NULL);
    timestamp_init();
    record_init();
    return 0;
}


void synth_suspect(int id)
{
    crash_details("start (id=%d)", id);
    synth_recv_lock(id);
    synth_status.liveness[id] = SUSPECT;
    synth_recv_unlock(id);
    crash_details("finished (id=%d)", id);
}


void synth_recover(int id)
{
    crash_details("start (id=%d)", id);
    synth_recv_lock(id);
    synth_status.liveness[id] = ALIVE;
    ev_set(&synth_status.ev_live[id]);
    synth_recv_unlock(id);
    crash_details("finished (id=%d)", id);
}


liveness_t synth_get_liveness(int id)
{
    liveness_t ret;

    synth_recv_lock(id);
    ret = synth_status.liveness[id];
    synth_recv_unlock(id);
    return ret;
}


void synth_set_liveness(int id, liveness_t l)
{
    synth_recv_lock(id);
    synth_status.liveness[id] = l;
    synth_recv_unlock(id);
}


void *synth_do_connect(void *ptr)
{
    int id;
    int ret;
    void *socket;
    void *context;
    zmsg_t *msg = NULL;
    synth_arg_t *arg = (synth_arg_t *)ptr;
#ifdef HIGH_WATER_MARK
    int hwm = HIGH_WATER_MARK;
#endif
    if (!arg) {
        log_err("failed to connect synthesizer, invalid argumuent");
        return NULL;
    }
    log_func("addr=%s", arg->addr);
    id = arg->id;
    context = zmq_ctx_new();
    if (MULTICAST_PUSH == arg->type) {
        socket = zmq_socket(context, ZMQ_PULL);
#ifdef HIGH_WATER_MARK
        zmq_setsockopt(socket, ZMQ_RCVHWM, &hwm, sizeof(hwm));
#endif
        ret = zmq_bind(socket, arg->addr);
    } else {
        socket = zmq_socket(context, ZMQ_SUB);
#ifdef HIGH_WATER_MARK
        zmq_setsockopt(socket, ZMQ_RCVHWM, &hwm, sizeof(hwm));
#endif
        ret = zmq_connect(socket, arg->addr);
        if (!ret) {
            if (zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0)) {
                log_err("failed to set socket");
                assert(0);
            }
        }
    }
    if (!ret) {
        while (true) {
            if (!msg)
                msg = zmsg_recv(socket);
            synth_recv_lock(id);
            if (synth_status.liveness[id] == ALIVE) {
                sampler_handle(id, msg);
                synth_recv_unlock(id);
                msg = NULL;
            } else {
                synth_recv_unlock(id);
                ev_wait(&synth_status.ev_live[id]);
            }
        }
    } else
        log_err("failed to connect synthesizer, addr=%s", arg->addr);
    zmq_close(socket);
    zmq_ctx_destroy(context);
    free(arg);
    return NULL;
}


void synth_connect(int id)
{
    pthread_t thread;
    synth_arg_t *arg;
    pthread_attr_t attr;

    arg = (synth_arg_t *)calloc(1, sizeof(synth_arg_t));
    if (!arg) {
        log_err("no memory");
        return;
    }
    arg->id = id;
    arg->type = MULTICAST;
    if (MULTICAST == MULTICAST_PUSH)
        tcpaddr(arg->addr, inet_ntoa(get_addr()), synthesizer_port + id);
    else if (MULTICAST == MULTICAST_PGM)
        pgmaddr(arg->addr, nodes[id], notifier_port);
    else if (MULTICAST == MULTICAST_EPGM)
        epgmaddr(arg->addr, nodes[id], notifier_port);
    else if (MULTICAST == MULTICAST_SUB)
        tcpaddr(arg->addr, nodes[id], notifier_port);
    else
        tcpaddr(arg->addr, nodes[id], synthesizer_port);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, synth_do_connect, arg);
    pthread_attr_destroy(&attr);
}


void synth_connect_peers()
{
    for (int i = 0; i < nr_nodes; i++)
        if (i != node_id)
            synth_connect(i);
}


rep_t synth_responder(req_t req)
{
    int i;
    char *paddr;
    rep_t rep = 0;
    struct in_addr addr;

    memcpy(&addr, &req, sizeof(struct in_addr));
    paddr = inet_ntoa(addr);
    log_func("addr=%s", paddr);
    for (i = 0; i < nr_nodes; i++) {
        if (!strcmp(paddr, nodes[i])) {
            synth_connect(i);
            break;
        }
    }
    if (i == nr_nodes) {
        log_err("failed to connect to synthesizer");
        req = -1;
    }
    return rep;
}


int synth_create_responder()
{
    pthread_t thread;
    pthread_attr_t attr;
    responder_arg_t *arg;

    arg = (responder_arg_t *)calloc(1, sizeof(responder_arg_t));
    if (!arg) {
        log_err("no memeory");
        return -ENOMEM;
    }
    arg->responder = synth_responder;
    tcpaddr(arg->addr, inet_ntoa(get_addr()), synthesizer_port);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, responder_start, arg);
    pthread_attr_destroy(&attr);
    return 0;
}


void *synth_handler(void *arg)
{
    while (true) {
        bool in = synth_check_input();
        bool out = synth_check_output();

        if (!in && !out)
            ev_wait(&synth_status.ev_deliver);
    }
}

void synth_create_handler()
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, synth_handler, NULL);
}


void *synth_queue_checker(void *arg)
{
    while (true) {
        usleep(SYNTH_CHECK_INTV);
        if (synth_status.busy)
            synth_status.busy = false;
        else {
            bool analysis = false;

            for (int id = 0; id < nr_nodes; id++) {
                struct list_head *pos;
                struct list_head *head = &synth_status.pass[id];
                struct list_head *candidates = &synth_status.candidates[id];

                synth_mutex_lock();
                for (pos = candidates->next; pos != candidates; pos = pos->next) {
                    record_t *rec = list_entry(pos, record_t, cand[id]);

                    if (!is_delivered(rec)) {
                        if (!rec->prev[id] && !rec->count[id]) {
                            rec->count[id] = true;
                            rec->quorum++;
                            if (synth_can_deliver(rec)) {
                                synth_deliver(rec);
                                break;
                            }
                        }
                        if (is_empty(&rec->pass[id]))
                            break;
                    } else
                        break;
                }
                if (!list_empty(head)) {
                    record_t *rec = list_entry(head->prev, record_t, pass[id]);

                    pos = rec->cand[id].next;
                    head = head->prev;
                    analysis = true;
                } else
                    pos = candidates->next;
                synth_do_pass(id, head, pos, false);
                synth_mutex_unlock();
            }
            if (analysis)
                show_analysis();
        }
    }
}


void synth_create_queue_checker()
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    pthread_create(&thread, &attr, synth_queue_checker, NULL);
}


int synth_create()
{
    if (synth_init()) {
        log_err("failed to initialize");
        return -EINVAL;
    }
#ifdef SYNTH_QUEUE_CHECKER
    synth_create_queue_checker();
#endif
    synth_create_handler();
    if ((MULTICAST == MULTICAST_SUB) || (MULTICAST == MULTICAST_PGM) || (MULTICAST == MULTICAST_EPGM))
        synth_create_responder();
    else
        synth_connect_peers();
#ifdef EVALUATE
    eval_create();
#endif
    return 0;
}


bool synth_is_empty()
{
    for (int i = 0; i < nr_nodes; i++) {
        synth_lock(i);
        bool empty = (queue_length(i) == 0);
        synth_unlock(i);
        if (!empty)
            return false;
    }
    return true;
}


bool synth_drain()
{
    if (synth_is_empty() && batch_drain())
        return synth_is_empty();
    else
        return false;
}
