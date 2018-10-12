#ifndef _DEFAULT_H
#define _DEFAULT_H

#include <conf.h>
#include <czmq.h>
#include <stdint.h>
#include <stdbool.h>
#include "rbtree.h"

// #define QUIET                // no logging
// #define VERIFY               // check if the program order is followed
#define QUIET_AFTER_RESUME

// #define FUNC_TIMER
// #define SIMU_CRASH
// #define RESET_COUNTER
// #define LOG_AFTER_CRASH
// #define VERIFY_TIMESTAMP
// #define QUIET_AFTER_RECYCLE
// #define SLOW_DOWN_AFTER_CRASH

#define CRASH_DEBUG
#define CRASH_NODES          2
#define CRASH_AFTER_N_REQ    1024

// #define CRASH_DETAILS
// #define CRASH_AFTER_RESUME
// #define CRASH_BEFORE_SUSPEND

#define SHOW_DEP
#define SHOW_PREV
#define SHOW_PACK
#define SHOW_BATCH
#define SHOW_QUEUE
#define SHOW_HEADER
#define SHOW_IGNORE
#define SHOW_RECORD
#define SHOW_RESULT
#define SHOW_QUORUM
#define SHOW_VISIBLE
#define SHOW_BLOCKER
#define SHOW_ENQUEUE
#define SHOW_DEQUEUE
#define SHOW_DELIVER
#define SHOW_CLEANER
#define SHOW_SUSPECT
#define SHOW_REPLAYER
#define SHOW_INVISIBLE
#define SHOW_TIMESTAMP
#define SHOW_HEARTBEAT
#define SHOW_COLLECTOR
#define SHOW_PROVIDERS

// #define SHOW_ANALYSIS
// #define SHOW_PROGRESS

// #define EVAL_SAVE
// #define EVAL_ECHO
// #define EVAL_DELAY

#define FORWARD
#define HEARTBEAT
#define MULTICAST           MULTICAST_SUB

#define NODE_MAX            7              // total servers
#define STAT_INTV           10000          // msg
#define HIGH_WATER_MARK     1000000        // msg
#define DELIVER_TIMEOUT     1000000        // nsec

#define ADDR_SIZE           128
#define IFNAME_SIZE         128
#define IPADDR_SIZE         16

#define PATH_LOG            "log"
#define PATH_CONF           "conf/inn.yaml"
#define PATH_RESULTS        "results"

#define INN_ADDR            "ipc:///tmp/inn"
#define CLIENT_ADDR         "ipc:///tmp/inncli"
#define SAMPLER_ADDR        "ipc:///tmp/innsamp"
#define REPLAYER_BACKEND    "ipc:///tmp/innreplb"
#define REPLAYER_FRONTEND   "ipc:///tmp/innreplf"
#define COLLECTOR_BACKEND   "ipc:///tmp/inncollb"
#define COLLECTOR_FRONTEND  "ipc:///tmp/inncollf"

#define MULTICAST_MAX       254
#define MULTICAST_NET       "224.0.0."

#if NODE_MAX > MULTICAST_MAX
#error NODE_MAX > MULTICAST_MAX
#endif

#define tree_entry list_entry

typedef uint32_t hid_t;
typedef uint32_t seq_t;
typedef uint64_t req_t;
typedef uint64_t rep_t;
typedef uint32_t bitmap_t;
typedef uint32_t session_t;
typedef struct rb_tree rbtree_t;
typedef struct timeval timeval_t;
typedef uint32_t timestamp_sec_t;
typedef uint32_t timestamp_usec_t;
typedef long long unsigned int timeout_t;
typedef struct rb_tree_node rbtree_node_t;

typedef struct __attribute__((__packed__)) {
    timestamp_sec_t sec;
    timestamp_usec_t usec;
    hid_t hid;
} timestamp_t;

enum {
    MULTICAST_PUB=1,
    MULTICAST_SUB,
    MULTICAST_PGM,
    MULTICAST_EPGM,
    MULTICAST_PUSH,
};

typedef enum {
    ALIVE=0,
    SUSPECT,
    STOP,
} liveness_t;

extern bool quiet;
extern int node_id;
extern int majority;
extern int nr_nodes;
extern int nr_requests;
extern int vector_size;
extern bitmap_t available_nodes;

extern int client_port;
extern int sampler_port;
extern int recovery_port;
extern int notifier_port;
extern int replayer_port;
extern int collector_port;
extern int heartbeat_port;
extern int evaluator_port;
extern int synthesizer_port;

extern char iface[IFNAME_SIZE];
extern bool alive_node[NODE_MAX];
extern bitmap_t node_mask[NODE_MAX];
extern char nodes[NODE_MAX][IPADDR_SIZE];

extern session_t get_session(int id);
extern void send_message(zmsg_t *msg);
extern bool get_seq_end(int id, seq_t *seq);
extern bool get_seq_start(int id, seq_t *seq);
extern void set_session(int id, session_t session);
extern void get_timestamps(int id, timestamp_t *timestamps, seq_t start, seq_t end);
extern void add_timestamps(int id, timestamp_t *timestamps, int count, zmsg_t *msg);

#endif
