/* ts.c
 *
 * Copyright (C) 2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "ts.h"

ts_tree_t ts_groups[NR_TS_GROUPS];

typedef hid_t ts_key_t;
#define ts_key(timestamp) (*(hid_t *)&timestamp[TIME_SIZE])
#define ts_hash(timestamp) (ts_key(timestamp) % NR_TS_GROUPS)
#define ts_cmp(timestamp1, timestamp2) memcmp(timestamp1, timestamp2, TIME_SIZE)
#define ts_cpy(timestamp1, timestamp2) memcpy(timestamp1, timestamp2, TIME_SIZE)

int ts_compare(char *timestamp1, char *timestamp2)
{
	ts_key_t id1 = ts_key(timestamp1);
	ts_key_t id2 = ts_key(timestamp2);

	if (id1 > id2)
		return 1;
	else if (id1 < id2)
		return -1;
	else
		return 0;
}


inline bool ts_add(ts_tree_t *tree, char *timestamp)
{
	ts_struct_t *ts = malloc(sizeof(ts_struct_t));

	if (!ts) {
		log_err("no memory");
		return false;
	}
	memcpy(ts->timestamp, timestamp, TIMESTAMP_SIZE);
	rbtree_insert(tree->root, ts->timestamp, ts, ts_compare);
	return true;
}


inline ts_node_t ts_lookup_node(ts_tree_t *tree, char *timestamp)
{
	return rbtree_lookup_node(tree->root, timestamp, ts_compare);
}


bool ts_update(char *timestamp)
{
	ts_node_t node;
	ts_tree_t *tree;
	bool ret = true;

	tree = &ts_groups[ts_hash(timestamp)];
	pthread_rwlock_wrlock(&tree->lock);
	node = ts_lookup_node(tree, timestamp);
	if (!node) {
		if (!ts_add(tree, timestamp)) {
			log_err("failed to add");
			show_timestamp("ts", -1, timestamp);
			ret = false;
		}
	} else {
		ts_struct_t *ts = (ts_struct_t *)node->value;

		if (ts_cmp(ts->timestamp, timestamp) < 0)
			ts_cpy(ts->timestamp, timestamp);
		else {
			log_func("expired timestamp");
			show_timestamp("ts", -1, timestamp);
			ret = false;
		}
	}
out:
	pthread_rwlock_unlock(&tree->lock);
	return ret;
}


bool ts_check(char *timestamp)
{
	bool ret;
	ts_node_t node;
	ts_tree_t *tree;

	tree = &ts_groups[ts_hash(timestamp)];
	pthread_rwlock_rdlock(&tree->lock);
	node = ts_lookup_node(tree, timestamp);
	if (!node)
		ret = true;
	else {
		ts_struct_t *ts = (ts_struct_t *)node->value;

		if (ts_cmp(ts->timestamp, timestamp) < 0)
			ret = true;
		else
			ret = false;
	}
	pthread_rwlock_unlock(&tree->lock);
	return ret;
}


void ts_init()
{
	int i;

	for (i = 0; i < NR_TS_GROUPS; i++) {
		ts_groups[i].root = rbtree_create();
		pthread_rwlock_init(&ts_groups[i].lock, NULL);
	}
}
