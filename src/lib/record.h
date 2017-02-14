#ifndef _RECORD_H
#define _RECORD_H

#include <zmq.h>
#include <errno.h>
#include <default.h>
#include <pthread.h>
#include "rbtree.h"
#include "util.h"
#include "log.h"

#define NR_RECORD_GROUPS 1024
#define record_tree_node(tree) ((tree)->root->root)

typedef struct record_tree {
	rbtree root;
	pthread_mutex_t mutex;
} record_tree_t;

typedef struct record {
	zmsg_t *msg;
	bool deliver;
	bool release;
	char *timestamp;
	bitmap_t bitmap;
	bitmap_t status;
	int seq[NODE_MAX];
	bool ready[NODE_MAX];
	bool visible[NODE_MAX];
	bitmap_t confirm[NODE_MAX];
	struct record *prev[NODE_MAX];
	struct list_head link[NODE_MAX];
	struct list_head next[NODE_MAX];
	struct list_head list[NODE_MAX];
} record_t;

typedef rbtree_node record_node_t;

int record_min(int id);
int record_max(int id);
zmsg_t *record_get_msg(int id, int seq);
record_t *record_get(int id, zmsg_t *msg);
void record_release(record_t *record);
void record_init();

#endif
