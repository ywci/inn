#ifndef _TS_H
#define _TS_H

#include <default.h>
#include <pthread.h>
#include "rbtree.h"
#include "util.h"
#include "log.h"

#define NR_TS_GROUPS 1024
#define ts_tree_node(tree) ((tree)->root->root)

typedef struct ts_tree {
	rbtree root;
	pthread_rwlock_t lock;
} ts_tree_t;

typedef struct ts_struct {
  char timestamp[TIMESTAMP_SIZE];
} ts_struct_t;

typedef rbtree_node ts_node_t;

void ts_init();
bool ts_check(char *timestamp);
bool ts_update(char *timestamp);

#endif
