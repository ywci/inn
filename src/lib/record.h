#ifndef _RECORD_H
#define _RECORD_H

#include <zmq.h>
#include <errno.h>
#include <default.h>
#include <pthread.h>
#include "rbtree.h"
#include "util.h"
#include "log.h"

#define NR_RECORD_GROUPS 65536
#define record_tree_node(tree) ((tree)->root->root)

typedef struct record_tree {
	rbtree root;
	pthread_mutex_t mutex;
} record_tree_t;

typedef struct record {
	bool expire;
	zmsg_t *msg;
	char *timestamp;
	bitmap_t bitmap;
	int seq[NODE_MAX];
} record_t;

typedef rbtree_node record_node_t;

void record_init();
int record_get_min(int id);
int record_get_max(int id);
void record_update(char *ts);
zmsg_t *record_find(int id, int seq);
pthread_mutex_t *record_lock(char *ts);
void record_put(pthread_mutex_t *mutex);
void record_unlock(pthread_mutex_t *mutex);
pthread_mutex_t *record_get(int id, int seq, zmsg_t *msg, bool mark, bool *create, bool *first);

#endif
