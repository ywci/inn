#ifndef _DEFAULT_H
#define _DEFAULT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef DEBUG
#define SHOW_VECTOR
#define SHOW_MATRIX
//#define SHOW_RECORD
#define SHOW_TIMESTAMP
#endif

enum {
  MULTICAST_PUB=1,
  MULTICAST_SUB,
  MULTICAST_PGM,
  MULTICAST_EPGM,
  MULTICAST_PUSH,
};

typedef uint32_t hid_t;
typedef uint32_t bitmap_t;

#define MULTICAST       MULTICAST_SUB
#define HEARTBEAT

#define NBIT            8       // NBIT=[1, 2, 4, 8]
#define NODE_MAX        7       // NODE_MAX <= 32
#define SKEW_MAX        2       // for balancing

#define SUSPEND_TIMEOUT 0       // nsec
#define DELIVER_TIMEOUT 0       // nsec
#define BALANCE_TIMEOUT 1000000 // nsec

#define ADDR_SIZE       128
#define IFNAME_SIZE     128
#define IPADDR_SIZE     16
#define TIME_SIZE       7
#define TIMESTAMP_SIZE  (TIME_SIZE + sizeof(hid_t))
#define QUEUE_SIZE      65536

#define PATH_LOG        "inn.log"
#define PATH_CONF       "conf/inn.yaml"
#define INN_ADDR        "ipc:///tmp/inndaemon"

extern int node_id;
extern int majority;
extern int nr_nodes;
extern int nr_requests;
extern int vector_size;
extern bitmap_t available_nodes;

extern int client_port;
extern int sampler_port;
extern int notifier_port;
extern int replayer_port;
extern int collector_port;
extern int heartbeat_port;
extern int synthesizer_port;

extern char iface[IFNAME_SIZE];
extern bitmap_t node_mask[NODE_MAX];
extern char nodes[NODE_MAX][IPADDR_SIZE];

#endif
