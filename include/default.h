#ifndef _DEFAULT_H
#define _DEFAULT_H

#include <stdint.h>

//#define NOWAIT
#define HEARTBEAT
//#define TIMEDWAIT
//#define LIGHT_CLIENT
#define LIGHT_SERVER
#define MIXER_BALANCE
#define SEQUENCER_BALANCE

#define NBIT 8
#define NODE_MAX 7 // (NODE_MAX < 32)
#define ADDR_LEN 16
#define LEN_DIFF 128
#define ADDR_SIZE 128
#define IFACE_SIZE 128
#define TIMESTAMP_SIZE 11
#define BALANCE_TIME 100 // Micro Seconds

#define CONF_INN "./conf/inn.xml"
#define CONF_PORT "./conf/port.xml"
#define CONF_NODE "./conf/node.xml"

#ifdef DEBUG
#define SHOW_VECTOR
#define SHOW_MATRIX
#define SHOW_QUEUES
#define SHOW_RECORD
#define SHOW_TIMESTAMP
#endif

typedef uint32_t bitmap_t;

extern int node_id;
extern int majority;
extern int nr_nodes;
extern int vector_size;
extern bitmap_t bitmap_available;

extern int mixer_port;
extern int client_port;
extern int replayer_port;
extern int collector_port;
extern int sequencer_port;
extern int heartbeat_port;

extern char iface[IFACE_SIZE];
extern bitmap_t node_mask[NODE_MAX];
extern char nodes[NODE_MAX][ADDR_LEN];

#endif
