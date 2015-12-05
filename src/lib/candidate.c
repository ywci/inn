/*      candidate.c
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

#include "candidate.h"

cand_tree_t candidates[NR_CAND_GROUPS];

void cand_init()
{
	int i;
	
	for (i = 0; i < NR_CAND_GROUPS; i++)
		candidates[i] = rbtree_create();
}


int cand_check_id(cand_t *cand, int id) 
{
	return cand->bitmap & node_mask[id];
}


cand_node_t cand_get_node(char *ts)
{
	unsigned int n = cand_hash(ts);
	
	rbtree tree = candidates[n];
	return rbtree_lookup_node(tree, ts, timestamp_compare);
}


cand_node_t cand_add(int id, char *ts)
{
	cand_t *cand;
	cand_node_t node;
	unsigned int n = cand_hash(ts);
	rbtree tree = candidates[n];
	
	node = rbtree_lookup_node(tree, ts, timestamp_compare);
	if (node) {
		cand = (cand_t *)node->value;
		if (cand->count >= nr_nodes) {
			log_err("invalid candidate");
			return NULL;
		}
		cand->count += 1;
		cand->bitmap |= node_mask[id];
	} else {
		cand = (cand_t *)malloc(sizeof(cand_t));
		if (!cand) {
			log_err("no memory");
			return NULL;
		}
		cand->count = 1;
		cand->bitmap = node_mask[id];
		memcpy(cand->timestamp, ts, TIMESTAMP_SIZE);
		rbtree_insert(tree, cand->timestamp, cand, timestamp_compare);
	}
	return node;
}


void cand_del_node(cand_node_t node, cand_tree_t tree)
{	
	cand_t *cand = node->value;
	
	if (!tree) {
		unsigned int n = cand_hash(cand->timestamp);
		
		tree = candidates[n];
	}
	rbtree_delete_node(tree, node);
	free(cand);
}
