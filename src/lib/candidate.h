#ifndef _CANDIDATE_H
#define _CANDIDATE_H

#include <errno.h>
#include <default.h>
#include "rbtree.h"
#include "util.h"

#define NR_CAND_GROUPS 256
#define cand_hash(ts) (((byte *)(ts))[0])

typedef struct candidate {
	int count;
	bitmap_t bitmap;
	char timestamp[TIMESTAMP_SIZE];
} cand_t;

typedef rbtree cand_tree_t;
typedef rbtree_node cand_node_t;

void cand_init();
cand_node_t cand_get_node(char *ts);
cand_node_t cand_add(int id, char *ts);
int cand_check_id(cand_t *cand, int id);
void cand_del_node(cand_node_t node, cand_tree_t tree);

#endif
