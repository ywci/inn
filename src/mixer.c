/*      mixer.c
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

#include "mixer.h"
#include "sequencer.h"

#ifndef NOWAIT
ev_t mixer_event;
#endif
bool mixer_active;
int mixer_ts_size;
int mixer_step[NODE_MAX];
pthread_mutex_t mixer_mutex;
que_t mixer_queues[NODE_MAX];
int mixer_matrix[NODE_MAX][NODE_MAX];
static char mixer_ts[NODE_MAX][TIMESTAMP_SIZE];


bool need_balance(int id)
{
	int i;
	int len = 0;
	
	for (i = 0; i < nr_nodes; i++) {
		if (bitmap_available & node_mask[i]) {
			int tmp = mixer_queues[i].length;

			if (!len || (tmp < len))
				len = tmp;
		}
	}
	
	if (mixer_queues[id].length - len >= LEN_DIFF)
		return true;
	else
		return false;
}


inline int update_queue(int id, que_item_t *item)
{
	return queue_add_item(&mixer_queues[id], item);
}


inline void update_matrix(int row, int col, int step)
{
	mixer_matrix[row][col] += step;
}


int get_matrix_item(int row, int col)
{
	return mixer_matrix[row][col];
}


int check_visible(int id, int seq)
{
	int i;
	int cnt = 0;
	
	for (i = 0; i < nr_nodes; i++) {
		if (mixer_matrix[i][id] >= seq) {
			cnt++;
			if (cnt >= majority)
				return 1;
		}
	}
	return 0;
}


int check_step(int id)
{
	const int step = (1 << NBIT) - 1;
	int ret = mixer_matrix[id][id] - mixer_step[id];
	
	if (ret > step)
		ret = step;
	mixer_step[id] += ret;
	return ret;
}


bool check_candidate(char *ts, int space)
{
	cand_node_t node = cand_get_node(ts);
	
	if (node) {
		cand_t *cand = (cand_t *)node->value;

		if (cand->count + space >= majority) {
			int i;
			int cnt = 0;
			
			for (i = 0; i < nr_nodes; i++) {
				if (!cand_check_id(cand, i) && !(mixer_queues[i].flgs & FL_INACTIVE)) {
					if (is_empty_timestamp(mixer_ts[i]) || (timestamp_compare(mixer_ts[i], cand->timestamp) > 0)) {
						cnt++;
						if (cand->count + cnt >= majority)
							return true;
					}
				}
			}
		}
	}
	return false;
}


bool verify(cand_node_t node)
{
	int i;
	cand_t *cand = (cand_t *)node->value;
	
	for (i = 0; i < nr_nodes; i++) {
		if (cand_check_id(cand, i)) {
			int ret;
			int cnt = 0;
			que_item_t *item;
			int len = mixer_queues[i].length;
			struct list_head *head = &mixer_queues[i].head;
			
			while (true) {
				head = head->next;
				item = list_entry(head, que_item_t, item);
				ret = timestamp_compare(item->timestamp, cand->timestamp);
				if (ret > 0) {
					if (check_candidate(item->timestamp, nr_nodes - cand->count))
						return false;
				} else 
					break;
				cnt++;
				if (cnt == len) {
					log_err("invalid candidate");
					return false;
				}
			}
		}
	}
	return true;
}


zmsg_t *scan()
{
	int i;
	int len = 0;
	bool next = true;
	zmsg_t *msg = NULL;
	int que_len[NODE_MAX];
	cand_node_t candidate = NULL;
	struct list_head *cursors[NODE_MAX];
	
	pthread_mutex_lock(&mixer_mutex);
	for (i = 0; i < nr_nodes; i++) {
		cursors[i] = &mixer_queues[i].head;
		que_len[i] = mixer_queues[i].length;
	}
	memset(mixer_ts, 0, mixer_ts_size);
	while (next) {
		len++;
		next = false;
		for (i = 0; i < nr_nodes; i++) {
			if (que_len[i] >= len) {
				cand_node_t node;
				bool visible = false;
				struct list_head *cursor = cursors[i]->next;
				que_item_t *item = list_entry(cursor, que_item_t, item);

				if (&mixer_queues[i].head == cursor) {
					log_err("invalid queue");
					goto out;
				}
				cursors[i] = cursor;
				if (!(item->flgs & FL_VISIBLE)) {
					if (check_visible(i, item->seq)) {
						item->flgs |= FL_VISIBLE;
						visible = true;
					}
				} else
					visible = true;
				
				if (visible) {
					if (is_empty_timestamp(mixer_ts[i]) || (timestamp_compare(mixer_ts[i], item->timestamp) > 0)) {
						memcpy(mixer_ts[i], item->timestamp, TIMESTAMP_SIZE);
						if (!(item->flgs & FL_ACCESS)) {
							item->flgs |= FL_ACCESS;
							node = cand_add(i, item->timestamp);
						} else
							node = cand_get_node(item->timestamp);
						
						if (node) {
							cand_t *cand = (cand_t *)node->value;
								
							if (cand->count >= majority) {
								if (!candidate)
									candidate = node;
								else {
									cand_t *curr = (cand_t *)candidate->value;
										
									if (timestamp_compare(cand->timestamp, curr->timestamp) > 0)
										candidate = node;
								}
							}
						}
					}
					next = true;
				} else
					que_len[i] = 0;
			}
		}
	}
	
	if (candidate && verify(candidate)) {
		cand_t *cand = (cand_t *)candidate->value;
		char *ts = cand->timestamp;
		
		record_update(ts);
		for (i = 0; i < nr_nodes; i++) {
			if (!msg)
				queue_pop_item(&mixer_queues[i], ts, &msg);
			else
				queue_pop_item(&mixer_queues[i], ts, NULL);
		}
		cand_del_node(candidate, NULL);
		if (!msg)
			log_err("failed to get messgae");
	}
out:
	pthread_mutex_unlock(&mixer_mutex);
	return msg;
}


void wait_event()
{
#ifndef NOWAIT
	ev_wait(&mixer_event);
#endif
}


void *mix(void *ptr)
{
	zmsg_t *msg;
	void *socket;
	void *context;
	
	context = zmq_ctx_new();
	socket = zmq_socket(context, ZMQ_PUB);
	if (zmq_bind(socket, MIXER_ADDR)) {
		log_err("failed to bind");
		return NULL;
	}
	
	while (true) {
		wait_event();
		while (true) {
			msg = scan();
			if (msg) {
				zframe_t *frame = zmsg_pop(msg);
#ifdef SHOW_TIMESTAMP
				show_timestamp("mix", -1, (char *)zframe_data(frame));
#endif
				zmsg_send(&msg, socket);
				zframe_destroy(&frame);
			} else {
#ifdef SHOW_QUEUES
				int i;
				
				for (i = 0; i < nr_nodes; i++)
					show_queue(&mixer_queues[i]);
#endif
				break;
			}
		}
	}
}


void forward_message(zmsg_t *msg)
{
	zmsg_t *res = update_message(msg, false);

	if (res)
		send_message(res);
}


void add_message(int id, zmsg_t *msg)
{
	int i;
	int val;
	byte *buf;
	bool create;
	int cnt = 0;
	int pos = 0;
	zframe_t *frame;
	que_item_t *item;
	pthread_mutex_t *mutex;
	const unsigned int mask = (1 << NBIT) - 1;
	
	frame = zmsg_pop(msg);
	if (zframe_size(frame) != vector_size) {
		log_err("invalid message");
		goto release;
	}
	buf = zframe_data(frame);
#ifdef SHOW_VECTOR
	show_vector("mixer", id, buf);
#endif
	val = buf[0];
	update_matrix(id, id, 1);
	update_matrix(node_id, id, 1);
	for (i = 0; i < nr_nodes; i++) {
		if (i != id) {
			update_matrix(id, i, val & mask);
			cnt += NBIT;
			if (cnt == 8) {
				pos++;
				cnt = 0;
				val = buf[pos];
			} else
				val >>= NBIT;
		}
	}
#ifdef SHOW_MATRIX
	show_matrix(mixer_matrix, nr_nodes, nr_nodes);
#endif
	zframe_destroy(&frame);
	mutex = record_get(id, next_seq(id), msg, true, &create, NULL);
	if (mutex) {
		zmsg_t *newmsg = NULL;
		
		item = (que_item_t *)malloc(sizeof(que_item_t));
		if (!item) {
			log_err("no memory");
			goto unlock;
		}
		memset(item, 0, sizeof(que_item_t));
		frame = zmsg_first(msg);
		item->msg = msg;
		item->timestamp = (char *)zframe_data(frame);
#ifdef SHOW_TIMESTAMP
		show_timestamp("mixer", id, item->timestamp);
#endif
		if (get_matrix_item(node_id, id) != next_seq(id)) {
			log_err("failed to update, item=%d, seq=%d", get_matrix_item(node_id, id), get_seq(id));
			goto out;
		}

		if (create) {
			newmsg = zmsg_dup(msg);
			if (!newmsg)
				log_err("failed to duplicate");
		}
		
		if (update_queue(id, item)) {
			log_err("failed to update queue");
			if (newmsg)
				zmsg_destroy(&newmsg);
			goto out;
		}
		record_put(mutex);
		if (newmsg)
			forward_message(newmsg);
	} else {
		update_seq(id);
		goto release;
	}
	refresh();
	return;
out:
	free(item);
unlock:
	record_put(mutex);
release:
	zmsg_destroy(&msg);
}


int initialize_mixer()
{
	int i, j;
	
	if (nr_nodes <= 0) {
		log_err("failed to initialize");
		return -1;
	}

#ifndef NOWAIT
	if (ev_init(&mixer_event)) {
		log_err("invalid event");
		return -1;
	}
#endif
	
	for (i = 0; i < nr_nodes; i++)
		queue_init(&mixer_queues[i], i);
	
	for (i = 0; i < NODE_MAX; i++) {
		mixer_step[i] = 0;
		for (j = 0; j < NODE_MAX; j++)
			mixer_matrix[i][j] = 0;
	}
	
	pthread_mutex_init(&mixer_mutex, NULL);
	mixer_ts_size = nr_nodes * TIMESTAMP_SIZE;
	record_init();
	cand_init();
	mixer_active = true;
	return 0;
}


void refresh()
{
#ifndef NOWAIT
	ev_set(&mixer_event);
#endif
}


void *bind_mixer(void *ptr)
{
	int id;
	int ret;
	zmsg_t *msg;
	void *socket;
	void *context;
	mixer_arg_t *arg = (mixer_arg_t *)ptr;
#ifndef LIGHT_SERVER
	char addr[ADDR_SIZE];
#endif
	
	if (!arg) {
		log_err("invalid argumuent");
		return NULL;
	}
	log_debug("%s: addr=%s, id=%d", __func__, arg->addr, arg->id);
	id = arg->id;
	context = zmq_ctx_new();
#ifdef LIGHT_SERVER
	socket = zmq_socket(context, ZMQ_SUB);
	ret = zmq_connect(socket, arg->addr);
	if (!ret)
		ret = zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0);
#else
	sprintf(addr, "tcp://*:%d", mixer_port + arg->id);
	socket = zmq_socket(context, ZMQ_PULL);
	ret = zmq_bind(socket, addr);
#endif
	if (!ret) {
		while (true) {
#ifdef MIXER_BALANCE
			while (need_balance(id))
				wait_timeout(BALANCE_TIME);
#endif
			msg = zmsg_recv(socket);
			add_message(id, msg);
		}
	} else
		log_err("failed to connect to %s", arg->addr);
	zmq_close(socket);
	zmq_ctx_destroy(context);
	free(arg);
	return NULL;
}


void bind_mixers()
{
	int i;
	int cnt = 0;
	pthread_t thread;
	mixer_arg_t *args;
	pthread_attr_t attr;
	
	args = (mixer_arg_t *)malloc((nr_nodes - 1) * sizeof(mixer_arg_t));
	if (!args) {
		log_err("no memory");
		return;
	}
	
	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			args[cnt].id = i;
			sprintf(args[cnt].addr, "tcp://%s:%d", nodes[i], mixer_port);
			
			pthread_attr_init(&attr);
			pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
			pthread_create(&thread, &attr, bind_mixer, &args[cnt]);
			pthread_attr_destroy(&attr);
			cnt++;
		}
	}
}


void start_mixer()
{
	pthread_t thread;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, mix, NULL);
}


void suspend_mixer()
{
	if (mixer_active) {
		pthread_mutex_trylock(&mixer_mutex);
		mixer_active = false;
	}
}


void resume_mixer()
{
	if (!mixer_active) {
		mixer_active = true;
		pthread_mutex_unlock(&mixer_mutex);
	}
}


void set_active(int id)
{
	queue_set_active(&mixer_queues[id]);
}


void set_inactive(int id)
{
	queue_set_inactive(&mixer_queues[id]);
}


int get_seq(int id)
{
	return queue_get_seq(&mixer_queues[id]);
}


int next_seq(int id)
{
	return queue_next_seq(&mixer_queues[id]);
}


void update_seq(int id)
{
	queue_update_seq(&mixer_queues[id]);
}


void create_mixer()
{	
	if (initialize_mixer()) {
		log_err("failed to initialize");
		return;
	}
	bind_mixers();
	start_mixer();
}


void insert_message(int id, int seq, zmsg_t *msg)
{
	que_item_t *item;
	pthread_mutex_t *mutex;
	zframe_t *frame = zmsg_first(msg);
	
	log_debug("%s: id=%d, seq=%d", __func__, id, seq);
	if (id == node_id) {
		log_err("invalid id");
		return;
	}
	
	if (zframe_size(frame) != TIMESTAMP_SIZE) {
		log_err("invalid message");
		return;
	}
	
	if (seq <= get_seq(id))
		return;
	
	if (seq != next_seq(id)) {
		log_err("invalid seq");
		return;
	}
	
	update_matrix(id, id, 1);
	update_matrix(node_id, id, 1);
	mutex = record_get(id, seq, msg, false, NULL, NULL);
	if (!mutex) {
		update_seq(id);
		return;
	}
	
	item = (que_item_t *)malloc(sizeof(que_item_t));
	if (!item) {
		log_err("no memory");
		goto out;
	}
	memset(item, 0, sizeof(que_item_t));
	item->msg = msg;
	item->timestamp = (char *)zframe_data(frame);
#ifdef SHOW_TIMESTAMP
	show_timestamp("mixer", id, item->timestamp);
#endif
	if (update_queue(id, item)) {
		log_err("failed to update queue");
		goto out;
	}
	refresh();
out:
	record_put(mutex);
}
