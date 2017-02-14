/* record.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "record.h"
#include "ts.h"

record_tree_t record_groups[NR_RECORD_GROUPS];

void record_init()
{
	int i;

	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_groups[i].root = rbtree_create();
		pthread_mutex_init(&record_groups[i].mutex, NULL);
	}
}


unsigned long record_hash(char *ts)
{
	unsigned long n = 0;

	memcpy(&n, &ts[4], 2);
	return n % NR_RECORD_GROUPS;
}


record_t *record_add(record_tree_t *tree, zmsg_t *msg)
{
	zmsg_t *dup;
	zframe_t *frame;
	record_t *rec = (record_t *)calloc(1, sizeof(record_t));

	if (!rec) {
		log_err("no memory");
		return NULL;
	}

	dup = zmsg_dup(msg);
	if (!dup) {
		log_err("failed to add");
		free(rec);
		return NULL;
	}

	frame = zmsg_first(dup);
	rec->msg = dup;
	rec->timestamp = (char *)zframe_data(frame);
	rbtree_insert(tree->root, rec->timestamp, rec, timestamp_compare);
	return rec;
}


inline record_node_t record_lookup_node(record_tree_t *tree, char *ts)
{
	return rbtree_lookup_node(tree->root, ts, timestamp_compare);
}


inline void record_remove_node(record_tree_t *tree, record_node_t node)
{
	record_t *rec = (record_t *)node->value;

	show_record(-1, rec);
	ts_update(rec->timestamp);
	rbtree_delete_node(tree->root, node);
	zmsg_destroy(&rec->msg);
	free(rec);
}


record_t *record_get(int id, zmsg_t *msg)
{
	char *timestamp;
	zframe_t *frame;
	record_node_t node;
	record_tree_t *tree;
	record_t *rec = NULL;

	frame = zmsg_first(msg);
	assert(zframe_size(frame) == TIMESTAMP_SIZE);
	timestamp = (char *)zframe_data(frame);
	tree = &record_groups[record_hash(timestamp)];
	pthread_mutex_lock(&tree->mutex);
	node = record_lookup_node(tree, timestamp);
	if (!node) {
		rec = record_add(tree, msg);
		if (!rec) {
			log_err("failed to add record");
			goto out;
		}
	} else
		rec = (record_t *)node->value;
#ifdef FASTMODE
	if ((zmsg_size(rec->msg) == 1) && (zmsg_size(msg) == 2)) {
		assert(id == node_id);
		frame = zframe_dup(zmsg_last(msg));
		zmsg_append(rec->msg, &frame);
	}
#endif
	if (!rec->seq[id])
		rec->bitmap |= node_mask[id];

	if (rec->release) {
		if ((rec->bitmap & available_nodes) == available_nodes) {
			assert(node);
			record_remove_node(tree, node);
			rec = NULL;
		}
	}
out:
	pthread_mutex_unlock(&tree->mutex);
	show_record(id, rec);
	return rec;
}


void record_release(record_t *record)
{
	char *timestamp = record->timestamp;
	record_tree_t *tree = &record_groups[record_hash(timestamp)];

	pthread_mutex_lock(&tree->mutex);
	if ((record->bitmap & available_nodes) == available_nodes) {
		record_node_t node = record_lookup_node(tree, timestamp);

		assert(node);
		record_remove_node(tree, node);
	} else
		record->release = true;
	pthread_mutex_unlock(&tree->mutex);
}


record_node_t record_lookup(record_node_t node, int id, int seq)
{
	record_t *rec;
	record_node_t ret = NULL;

	if (!node || !seq)
		return NULL;

	if (node->right) {
		ret = record_lookup(node->right, id, seq);
		if (ret)
			return ret;
	}

	rec = (record_t *)node->value;
	if (rec->seq[id] == seq)
		return node;

	if (node->left)
		ret = record_lookup(node->left, id, seq);

	return ret;
}


zmsg_t *record_get_msg(int id, int seq)
{
	int i;

	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_tree_t *tree = &record_groups[i];
		record_node_t node = record_lookup(record_tree_node(tree), id, seq);

		if (node) {
			record_t *rec = (record_t *)node->value;

			return rec->msg;
		}
	}
	return NULL;
}


record_node_t record_lookup_min(record_node_t node, int id)
{
	int seq = 0;
	record_t *rec;
	record_node_t ret = NULL;

	if (!node)
		return ret;

	if (node->right) {
		ret = record_lookup_min(node->right, id);
		if (ret) {
			rec = (record_t *)ret->value;
			seq = rec->seq[id];
		}
	}

	rec = (record_t *)node->value;
	if (rec->seq[id] && (!ret || (rec->seq[id] < seq))) {
		ret = node;
		seq = rec->seq[id];
	}

	if (node->left) {
		record_node_t tmp = record_lookup_min(node->left, id);

		if (tmp) {
			rec = (record_t *)tmp->value;
			if (rec->seq[id] && (!ret || (rec->seq[id] < seq)))
				ret = node;
		}
	}

	return ret;
}


int record_min(int id)
{
	int i;
	int ret = 0;
	record_t *rec = NULL;

	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_tree_t *tree = &record_groups[i];
		record_node_t node = record_lookup_min(record_tree_node(tree), id);

		if (node) {
			record_t *r = (record_t *)node->value;
			int seq = r->seq[id];

			if (!ret || (seq < ret)) {
				ret = seq;
				rec = r;
			}
		}
	}

	show_record(id, rec);
	return ret;
}


record_node_t record_lookup_max(record_node_t node, int id)
{
	int seq = 0;
	record_t *rec;
	record_node_t ret = NULL;

	if (!node)
		return ret;

	if (node->right) {
		ret = record_lookup_max(node->right, id);
		if (ret) {
			rec = (record_t *)ret->value;
			seq = rec->seq[id];
		}
	}

	rec = (record_t *)node->value;
	if (rec->seq[id] && (!ret || (rec->seq[id] > seq))) {
		ret = node;
		seq = rec->seq[id];
	}

	if (node->left) {
		record_node_t tmp = record_lookup_max(node->left, id);

		if (tmp) {
			rec = (record_t *)tmp->value;
			if (rec->seq[id] && (!ret || (rec->seq[id] > seq)))
				ret = node;
		}
	}

	return ret;
}


int record_max(int id)
{
	int i;
	int ret = 0;
	record_t *rec = NULL;

	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_tree_t *tree = &record_groups[i];
		record_node_t node = record_lookup_max(record_tree_node(tree), id);

		if (node) {
			record_t *r = (record_t *)node->value;
			int seq = r->seq[id];

			if (seq > ret) {
				ret = seq;
				rec = r;
			}
		}
	}

	show_record(id, rec);
	return ret;
}
