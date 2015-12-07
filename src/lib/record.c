/*      record.c
 *      
 *      Copyright (C) 2015 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "record.h"

record_tree_t records[NR_RECORD_GROUPS];

unsigned long record_hash(char *ts)
{
	unsigned long n = 0;

	memcpy(&n, &ts[4], 2);
	return n & 0xffff;
}


void record_init()
{
	int i;
	
	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		records[i].root = rbtree_create();
		pthread_mutex_init(&records[i].mutex, NULL);
	}
}


void record_add(record_tree_t *tree, int id, int seq, zmsg_t *msg, bool mark)
{
	zmsg_t *newmsg;
	zframe_t *frame;
	const int seq_size = NODE_MAX * sizeof(int);
	record_t *rec = (record_t *)malloc(sizeof(record_t));
	
	if (!rec) {
		log_err("no memory");
		return;
	}
	
	newmsg = zmsg_dup(msg);
	if (!newmsg) {
		log_err("failed to duplicate");
		free(rec);
		return;
	}
	
	frame = zmsg_first(newmsg);
	memset(rec->seq, 0, seq_size);
	rec->msg = newmsg;
	rec->seq[id] = seq;
	rec->expire = false;
	if (mark)
		rec->bitmap = node_mask[id];
	else
		rec->bitmap = 0;
	rec->timestamp = (char *)zframe_data(frame);
	rbtree_insert(tree->root, rec->timestamp, rec, timestamp_compare);
}


record_node_t record_lookup_node(record_tree_t *tree, char *ts)
{
	return rbtree_lookup_node(tree->root, ts, timestamp_compare);
}


void record_remove_node(record_tree_t *tree, record_node_t node)
{
	record_t *rec = (record_t *)node->value;
	
#ifdef SHOW_RECORD
	show_timestamp(__func__, -1, rec->timestamp);
	show_bitmap(__func__, rec->bitmap);
	show_seq(__func__, rec->seq);
#endif
	rbtree_delete_node(tree->root, node);
	zmsg_destroy(&rec->msg);
	free(rec);
}


bool record_check(int id, int seq, zmsg_t *msg, bool mark, bool *create, bool *first)
{
	char *ts;
	unsigned int n;
	zframe_t *frame;
	bool ret = false;
	record_node_t node;
	record_tree_t *tree;

	if (create)
		*create = false;

	if (first)
		*first = false;
	
	if (!msg) {
		log_err("no message");
		return ret;
	}
	
	frame = zmsg_first(msg);
	if (zframe_size(frame) != TIMESTAMP_SIZE) {
		log_err("invalid message");
		return ret;
	}
	
	ts = (char *)zframe_data(frame);
	n = record_hash(ts);
	tree = &records[n];
	pthread_mutex_lock(&tree->mutex);
	node = record_lookup_node(tree, ts);
	if (!node) {
		record_add(tree, id, seq, msg, mark);
		if (create)
			*create = true;
		if (first)
			*first = true;
		ret = true;
	} else {
		record_t *rec = (record_t *)node->value;

		if (mark)
			rec->bitmap |= node_mask[id];

		if (first && !rec->seq[id])
			*first = true;
		
		if (!rec->expire) {
			if (!rec->seq[id])
				rec->seq[id] = seq;
			ret = true;
		}
	}
	pthread_mutex_unlock(&tree->mutex);
	return ret;	
}


void record_update(char *ts)
{
	record_node_t node;
	unsigned int n = record_hash(ts);
	record_tree_t *tree = &records[n];

	pthread_mutex_lock(&tree->mutex);
	node = record_lookup_node(tree, ts);
	if (node) {
		record_t *rec = (record_t *)node->value;
		
		rec->expire = true;
		if ((rec->bitmap & bitmap_available) == bitmap_available)
			record_remove_node(tree, node);
	} else
		log_err("cannot find node");
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


zmsg_t *record_find(int id, int seq)
{
	int i;
	
	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_tree_t *tree = &records[i];
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


int record_get_min(int id)
{
	int i;
	int ret = 0;
#ifdef SHOW_RECORD
	record_t *rec = NULL;
#endif
	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_tree_t *tree = &records[i];
		record_node_t node = record_lookup_min(record_tree_node(tree), id);
		
		if (node) {
			record_t *r = (record_t *)node->value;
			int seq = r->seq[id];
			
			if (!ret || (seq < ret)) {
				ret = seq;
#ifdef SHOW_RECORD
				rec = r;
#endif
			}
		}
	}
	
#ifdef SHOW_RECORD
	if (rec) {
		show_timestamp(__func__, -1, rec->timestamp);
		show_bitmap(__func__, rec->bitmap);
		show_seq(__func__, rec->seq);
	}
#endif
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


int record_get_max(int id)
{
	int i;
	int ret = 0;
#ifdef SHOW_RECORD
	record_t *rec = NULL;
#endif
	for (i = 0; i < NR_RECORD_GROUPS; i++) {
		record_tree_t *tree = &records[i];
		record_node_t node = record_lookup_max(record_tree_node(tree), id);
		
		if (node) {
			record_t *r = (record_t *)node->value;
			int seq = r->seq[id];
			
			if (seq > ret) {
				ret = seq;
#ifdef SHOW_RECORD
				rec = r;
#endif
			}
		}
	}
	
#ifdef SHOW_RECORD
	if (rec) {
		show_timestamp(__func__, -1, rec->timestamp);
		show_bitmap(__func__, rec->bitmap);
		show_seq(__func__, rec->seq);
	}
#endif
	return ret;
}
