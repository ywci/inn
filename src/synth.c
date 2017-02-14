/* synth.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "synth.h"
#include "sampler.h"
#include "ts.h"

// SYNTH SETTINGS --------//
#define RECONFIRM
//#define INTENSIVE
//------------------------//

event_t suspend_event;
event_t deliver_event;
int node_step[NODE_MAX];
queue_t queues[NODE_MAX];
pthread_rwlock_t synth_lock;
pthread_mutex_t global_lock;
struct list_head deliver_queue;
pthread_mutex_t locks[NODE_MAX];
int dep_matrix[NODE_MAX][NODE_MAX];
char timestamps[NODE_MAX][TIMESTAMP_SIZE];
pthread_rwlock_t timestamp_locks[NODE_MAX];

bool node_active = true;
int deliver_length = 0;

#ifdef BALANCE
int curr_id = -1;
int curr_seq = 0;
event_t balance_event;
bool balance_wait = false;
#endif

#define STATUS available_nodes
#define VECTORSZ (((NODE_MAX - 1) * NBIT + 7) / 8)

#define lock_acquire(id) do { \
	pthread_rwlock_rdlock(&synth_lock); \
	pthread_mutex_lock(&locks[id]); \
} while (0)

#define lock_release(id) do { \
	pthread_mutex_unlock(&locks[id]); \
	pthread_rwlock_unlock(&synth_lock); \
} while (0)

#define is_empty(list) ((list)->next == NULL)
#define deliver_lock() pthread_mutex_lock(&global_lock)
#define deliver_unlock() pthread_mutex_unlock(&global_lock)
#define timestamp_rdlock(id) pthread_rwlock_rdlock(&timestamp_locks[id])
#define timestamp_wrlock(id) pthread_rwlock_wrlock(&timestamp_locks[id])
#define timestamp_unlock(id) pthread_rwlock_unlock(&timestamp_locks[id])

int get_seq(int id)
{
	return queues[id].seq;
}


void delete_entry(int id, record_t *record)
{
	struct list_head *i;
	struct list_head *j;
	struct record *prev = record->prev[id];
	struct list_head *next = &record->next[id];
	struct list_head *entry = &record->list[id];

	log_func("start=>{id:%d, seq:%d}", id, record->seq[id]);
	if (!is_empty(next) && !list_empty(next)) {
		if (prev) {
			struct list_head *head = prev->list[id].prev;

			for (i = next->next, j = i->next; i != next; i = j, j = j->next) {
				struct list_head *k;
				record_t *rec = list_entry(i, record_t, link[id]);
				char *ts = rec->timestamp;

				for (k = entry->prev; k != head; k = k->prev) {
					record_t *curr = list_entry(k, record_t, list[id]);

					if (timestamp_compare(curr->timestamp, ts) < 0) {
						log_func("prev({id:%d, seq:%d})=>{id:%d, seq:%d}", id, rec->seq[id], id, curr->seq[id]);
						if (is_empty(curr->next))
							INIT_LIST_HEAD(curr->next);
						list_add_tail(i, curr->next);
						rec->prev[id] = curr;
						break;
					}
				}
				assert(k != head);
			}
			list_del(&record->link[id]);
		} else {
			int seq = -1;
			char *ts = timestamps[id];

			timestamp_wrlock(id);
			timestamp_clear(ts);
			for (i = next->next, j = i->next; i != next; i = j, j = j->next) {
				record_t *rec = list_entry(i, record_t, link[id]);

				rec->status = 0;
				rec->prev[id] = NULL;
				rec->ready[id] = true;
				if(timestamp_empty(ts) || (timestamp_compare(rec->timestamp, ts) < 0)) {
					timestamp_copy(ts, rec->timestamp);
					seq = rec->seq[id];
				}
				list_del(i);
			}
			timestamp_unlock(id);
			assert(seq != -1);
			log_debug("==-- early bird --== {id:%d, seq:%d}", id, seq);
			show_timestamp(">>refresh<<", id, ts);
		}
	} else{
		if (prev)
			list_del(&record->link[id]);
		else {
			timestamp_wrlock(id);
			timestamp_clear(timestamps[id]);
			timestamp_unlock(id);
		}
	}
	queue_pop(&queues[id], entry);
	log_func("finish=>{id:%d, seq:%d}", id, record->seq[id]);
}


inline int check_step(int id)
{
	const int step = (1 << NBIT) - 1;
	int ret = dep_matrix[id][id] - node_step[id];

	if (ret > step)
		ret = step;

	node_step[id] += ret;
	return ret;
}


void get_vector(byte *vector)
{
	int i;
	int bits;
	int pos = 0;
	int cnt = 0;
	byte val = 0;

	for (i = 0; i < nr_nodes; i++) {
		if (i != node_id) {
			bits = cnt * NBIT;
			val |= check_step(i) << bits;
			if (bits + NBIT == 8) {
				vector[pos] = val;
				val = 0;
				cnt = 0;
				pos++;
			} else
				cnt++;
		}
	}

	if (cnt)
		vector[pos] = val;
}


#ifdef BALANCE
inline void check_balance(int id, int seq)
{
	if (balance_wait) {
		if ((id == curr_id) && (curr_seq - seq <= SKEW_MAX)) {
			balance_wait = false;
			event_set(&balance_event);
		}
	} else {
		if (curr_seq - seq > SKEW_MAX) {
			curr_id = id;
			balance_wait = true;
		}
	}
}


inline void balance()
{
	if (balance_wait) {
		event_wait(&balance_event);
		balance_wait = false;
	}
}
#endif



inline bool is_visible(int id, record_t *record)
{
	int i;
	int cnt = 0;
 	int seq = record->seq[id];

	for (i = 0; i < nr_nodes; i++) {
		if (dep_matrix[i][id] >= seq) {
			cnt++;
			if (cnt >= majority) {
				log_debug("<visible>------------------->{id:%d, seq:%d}", id, seq);
				return true;
			}
		}
	}

	return false;
}


int update_seq(int id, byte *vector)
{
	int seq;

	if (vector) {
		int i;
		int val;
		int cnt = 0;
		int pos = 0;
		const unsigned int mask = (1 << NBIT) - 1;

		val = vector[0];
		dep_matrix[id][id]++;
		dep_matrix[node_id][id]++;
		for (i = 0; i < nr_nodes; i++) {
			if (i != id) {
				dep_matrix[id][i] += val & mask;
				cnt += NBIT;
				if (cnt == 8) {
					pos++;
					cnt = 0;
					val = vector[pos];
				} else
					val >>= NBIT;
			}
		}
	}

	seq = queue_update_seq(&queues[id]);
	if (dep_matrix[node_id][id] != seq) {
		log_err("failed to update seq, current=%d, seq=%d, id=%d", dep_matrix[node_id][id], seq, id);
		return -1;
	}
	show_matrix(dep_matrix, nr_nodes, nr_nodes);
	return seq;
}


int check_seq(int id, int seq)
{
	if (seq == queues[id].seq + 1) {
		queues[id].seq = seq;
		dep_matrix[id][id]++;
		dep_matrix[node_id][id]++;
		return 0;
	} else {
		log_err("failed to check seq, current=%d, seq=%d, id=%d", queues[id].seq + 1, seq, id);
		return -1;
	}
}


void deliver(int id, record_t *record)
{
	deliver_lock();
	if (!record->deliver) {
		synth_entry_t *entry;

		log_debug(">>deliver<<------------------{id:%d, seq:%d}", id, record->seq[id]);
		record->deliver = true;
		if (deliver_length >= QUEUE_SIZE) {
			deliver_unlock();
			lock_release(id);
			log_debug("wait for delivering ...");
			event_wait(&suspend_event);
			lock_acquire(id);
			deliver_lock();
		}

		entry = calloc(1, sizeof(synth_entry_t));
		if (!entry) {
			log_err("failed to deliver, no memory");
			goto out;
		}

		entry->record = record;
		list_add_tail(&entry->list, &deliver_queue);
		if (!deliver_length)
			event_set(&deliver_event);
		deliver_length++;
	}
out:
	deliver_unlock();
}


bool do_update_queue(int id, bool reveal)
{
	static int total = 0;
	static int status = 0;
	static bool available[NODE_MAX];
	struct list_head *head = &queues[id].head;
	struct list_head *pos;

	if (status != STATUS) {
		int i;
		int tmp = STATUS;

		total = 0;
		status = STATUS;
		for (i = 0; i < nr_nodes; i++) {
			if (tmp & 1) {
				available[i] = true;
				total++;
			} else
				available[i] = false;
			tmp >>= 1;
		}
		log_func("total=%d", total);
	}

	if (reveal) {
		bool visible = false;

		for (pos = head->prev; pos != head; pos = pos->prev) {
			record_t *rec = list_entry(pos, record_t, list[id]);

			if (!rec->visible[id]) {
				if (is_visible(id, rec)) {
					rec->visible[id] = true;
					visible = true;
				} else
					assert(!visible);
			} else
				break;
		}
	}

	list_for_each(pos, head) {
		record_t *rec = list_entry(pos, record_t, list[id]);

		if (!rec->visible[id]) {
			break;
#ifdef RECONFIRM
		} else if (rec->confirm[id]) {
			log_debug("reconfirm=>{id:%d, seq:%d}", id, rec->seq[id]);
			rec->confirm[id] = STATUS;
			break;
#endif
		} else {
			int i;
			int ready = 0;
			int guess = 0;
			int block = 0;
			bitmap_t bitmap;

#ifndef RECONFIRM
			if (!rec->confirm[id])
#endif
				if (rec->status == STATUS) // this record is blocked
						continue;

			bitmap = rec->bitmap & STATUS;
			for (i = 0; i < nr_nodes; i++) {
				if (bitmap & node_mask[i]) {
					if (rec->ready[i]) {
						if (rec->visible[i])
							ready++;
					} else
						block++;
				} else if (available[i]) {
					timestamp_rdlock(i);
					if (!timestamp_empty(timestamps[i])
					&& timestamp_compare(rec->timestamp, timestamps[i]) > 0)
						guess++;
					timestamp_unlock(i);
				}
			}

			if (ready >= majority) {
				int cnt = 1;

				for (i = 0; i < nr_nodes; i++)
					if ((bitmap & node_mask[i]) && (rec->confirm[i] == STATUS))
						cnt++;

				if (cnt >= majority) {
#ifdef FASTMODE
					if (rec->seq[node_id])
#endif
					{
						delete_entry(id, rec);
						deliver(id, rec);
						return true;
					}
				}

				log_debug("confirm=>{id:%d, seq:%d}, cnt=%d (ready=%d)", id, rec->seq[id], cnt, ready);
				rec->confirm[id] = STATUS;
				break;
			} else {
				int cnt = total - block;

				if (cnt < majority) {
					log_debug("block=>{id:%d, seq:%d}, cnt=%d (ready=%d)", id, rec->seq[id], block, ready);
					rec->status = STATUS;
				} else {
					rec->status = 0;
					cnt -= guess;
					if (cnt < majority)
						log_debug("guess=>{id:%d, seq:%d}, cnt=%d (ready=%d)", id, rec->seq[id], block + guess, ready);
					else {
						log_debug("recheck=>{id:%d, seq:%d}, cnt=%d (ready=%d)", id, rec->seq[id], block + guess, ready);
						break;
					}
				}
			}
		}
	}

	return false;
}


void update_queue(int id, int seq, record_t *record)
{
	bool pass = false;
	char *ts = record->timestamp;

	log_func("{id:%d, seq:%d}", id, seq);
	timestamp_wrlock(id);
	if (timestamp_empty(timestamps[id]) || (timestamp_compare(ts, timestamps[id]) < 0)) {
		timestamp_copy(timestamps[id], ts);
		pass = true;
	}
	timestamp_unlock(id);

	if (pass) {
		record->ready[id] = true;
		log_debug("==-- early bird --== {id:%d, seq:%d}", id, seq);
		show_timestamp(">>update<<", id, ts);
	} else {
		bool match = false;
		bool visible = false;
		struct list_head *pos;
		struct list_head *head = &queues[id].head;

		if (is_visible(id, record)) {
			record->visible[id] = true;
			visible = true;
		}

		for (pos = head->prev; pos != head; pos = pos->prev) {
			record_t *rec = list_entry(pos, record_t, list[id]);

			if (!match) {
				if (timestamp_compare(rec->timestamp, ts) < 0) {
					if (is_empty(&rec->next[id]))
						INIT_LIST_HEAD(&rec->next[id]);
					list_add_tail(&record->link[id], &rec->next[id]);
					record->prev[id] = rec;
					match = true;
				}
			}

			if (!rec->visible[id]) {
				if (is_visible(id, rec)) {
					rec->visible[id] = true;
					visible = true;
				} else
					assert(!visible);
			} else if (match)
				break;
		}
		assert(match);
	}
	queue_push(&queues[id], &record->list[id]);
	do_update_queue(id, pass);
}


void *do_deliver(void *arg)
{
	while (true) {
		int i;
		record_t *record = NULL;

		deliver_lock();
		if (!deliver_length) {
			deliver_unlock();
			for (i = 0; i < nr_nodes; i++) {
				if (available_nodes & node_mask[i]) {
					bool update;

					lock_acquire(i);
					update = do_update_queue(i, true);
					lock_release(i);
					if (update)
						break;
				}
			}

			deliver_lock();
			if (!deliver_length) {
				deliver_unlock();
				log_debug("wait for incoming request ...");
				event_wait(&deliver_event);
				deliver_lock();
			}
		}
		if (!list_empty(&deliver_queue)) {
			struct list_head *head = deliver_queue.next;
			synth_entry_t *entry = list_entry(head, synth_entry_t, list);

			record = entry->record;
			list_del(head);
			free(entry);
			if (deliver_length >= QUEUE_SIZE) {
				log_debug("wake up ...");
				event_set(&suspend_event);
			}
			deliver_length--;
		}
		deliver_unlock();

		if (record) {
			zmsg_t *msg = record->msg;
			zframe_t *frame = zmsg_last(msg);

			for (i = 0; i < nr_nodes; i++) {
				lock_acquire(i);
				if (!is_empty(&record->list[i]))
					delete_entry(i, record);
				lock_release(i);
			}
			callback((char *)zframe_data(frame), zframe_size(frame));
			show_timestamp(">>deliver<<", -1, record->timestamp);
			record_release(record);
		}
	}
}


zmsg_t *do_update_message(zmsg_t *msg)
{
	int seq;
#ifdef FASTMODE
	zmsg_t *tmp;
#endif
	record_t *record;
	byte vecotr[VECTORSZ] = {0};
	zframe_t *frame = zmsg_first(msg);

	assert(zframe_size(frame) == TIMESTAMP_SIZE);
	lock_acquire(node_id);
	if (!ts_check((char *)zframe_data(frame))) {
		log_func("find an expired message, id=%d", node_id);
		zmsg_destroy(&msg);
		lock_release(node_id);
		return NULL;
	}

	record = record_get(node_id, msg);
	if (record) {
		if (!record->seq[node_id]) {
			dep_matrix[node_id][node_id]++;
			seq = update_seq(node_id, NULL);
	#ifdef BALANCE
			curr_seq = seq;
	#endif
			record->seq[node_id] = seq;
			if (!record->deliver) {
				update_queue(node_id, seq, record);
	#ifdef INTENSIVE
				event_set(&deliver_event);
	#endif
			}
		}

		get_vector(vecotr);
	#ifdef FASTMODE
		tmp = zmsg_new();
		assert(tmp);
		frame = zmsg_pop(msg);
		zmsg_prepend(tmp, &frame);
		zmsg_destroy(&msg);
		msg = tmp;
	#endif
		frame = zframe_new(vecotr, vector_size);
		assert(frame);
		zmsg_prepend(msg, &frame);
		send_message(msg);
	} else
		zmsg_destroy(&msg);

	lock_release(node_id);
	return NULL;
}


zmsg_t *update_message(zmsg_t *msg)
{
#ifdef BALANCE
	balance();
#endif
	return do_update_message(msg);
}


void add_message(int id, int seq, zmsg_t *msg)
{
	zframe_t *p;
	zframe_t *frame = NULL;
	record_t *record = NULL;
#ifdef REUSE
	bool reuse = false;
#endif
	if (seq < 0) {
		frame = zmsg_pop(msg);
		assert(zframe_size(frame) == vector_size);
		show_vector("synth", id, zframe_data(frame));
	}

	p = zmsg_first(msg);
	assert(zframe_size(p) == TIMESTAMP_SIZE);
	lock_acquire(id);
	if (!ts_check((char *)zframe_data(p))) {
		log_func("find an expired message, id=%d", id);
		goto out;
	}

	if (!(node_mask[id] & available_nodes)) {
		log_func("receive a message from unavailable node %d", id);
		goto out;
	}

	record = record_get(id, msg);
	if (record) {
		if (!record->seq[id]) {
			if (seq < 0) {
				seq = update_seq(id, zframe_data(frame));
				if (seq < 0)
					goto out;
			} else if (check_seq(id, seq))
				goto out;
#ifdef BALANCE
			check_balance(id, seq);
#endif
			record->seq[id] = seq;
			if(!record->deliver) {
				update_queue(id, seq, record);
				event_set(&deliver_event);
			}
		}
#ifdef REUSE
		if (!record->seq[node_id])
			reuse = true;
#endif
	}
out:
	lock_release(id);
	zframe_destroy(&frame);
#ifdef REUSE
	if (reuse)
		do_update_message(msg);
	else
#endif
		if (msg)
			zmsg_destroy(&msg);
}


void suspend_synthesizer()
{
	if (node_active) {
		pthread_rwlock_wrlock(&synth_lock);
		node_active = false;
	}
}


void resume_synthesizer()
{
	if (!node_active) {
		node_active = true;
		pthread_rwlock_unlock(&synth_lock);
	}
}


int init_synthesizer()
{
	int i, j;

	if (nr_nodes <= 0) {
		log_err("failed to initialize");
		return -1;
	}

#ifdef BALANCE
	balance_event.timeout = BALANCE_TIMEOUT;
	if (event_init(&balance_event)) {
		log_err("failed to initialize");
		return -1;
	}
#endif
	suspend_event.timeout = SUSPEND_TIMEOUT;
	if (event_init(&suspend_event)) {
		log_err("failed to initialize");
		return -1;
	}

	deliver_event.timeout = DELIVER_TIMEOUT;
	if (event_init(&deliver_event)) {
		log_err("failed to initialize");
		return -1;
	}

	for (i = 0; i < nr_nodes; i++) {
		queue_init(&queues[i], i);
		pthread_mutex_init(&locks[i], NULL);
		pthread_rwlock_init(&timestamp_locks[i], NULL);
	}

	for (i = 0; i < NODE_MAX; i++) {
		node_step[i] = 0;
		for (j = 0; j < NODE_MAX; j++)
			dep_matrix[i][j] = 0;
		timestamp_clear(timestamps[i]);
	}

	pthread_rwlock_init(&synth_lock, NULL);
	pthread_mutex_init(&global_lock, NULL);
	INIT_LIST_HEAD(&deliver_queue);
	record_init();
	ts_init();
	return 0;
}


void *do_connect_synthesizer(void *ptr)
{
	int id;
	int ret;
	zmsg_t *msg;
	void *socket;
	void *context;
	synth_arg_t *arg = (synth_arg_t *)ptr;

	if (!arg) {
		log_err("invalid argumuent");
		return NULL;
	}

	log_func("addr=%s", arg->addr);
	id = arg->id;
	context = zmq_ctx_new();
	if (MULTICAST_PUSH == arg->type) {
		socket = zmq_socket(context, ZMQ_PULL);
		ret = zmq_bind(socket, arg->addr);
	} else {
		socket = zmq_socket(context, ZMQ_SUB);
		ret = zmq_connect(socket, arg->addr);
		if (!ret)
			ret = zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0);
	}

	if (!ret) {
		while (true) {
			msg = zmsg_recv(socket);
			add_message(id, -1, msg);
		}
	} else
		log_err("failed to connect to %s", arg->addr);

	zmq_close(socket);
	zmq_ctx_destroy(context);
	free(arg);
	return NULL;
}


void connect_synthesizer(int id)
{
	pthread_t thread;
	synth_arg_t *arg;
	pthread_attr_t attr;

	log_debug("connect_synthesizer: id=%d", id);
	arg = (synth_arg_t *)calloc(1, sizeof(synth_arg_t));
	if (!arg) {
		log_err("no memory");
		return;
	}

	arg->id = id;
	arg->type = MULTICAST;
	if (MULTICAST == MULTICAST_PUSH)
		tcpaddr(arg->addr, inet_ntoa(get_addr()), synthesizer_port + id);
	else if (MULTICAST == MULTICAST_PGM)
		pgmaddr(arg->addr, nodes[id], notifier_port);
	else if (MULTICAST == MULTICAST_EPGM)
		epgmaddr(arg->addr, nodes[id], notifier_port);
	else if (MULTICAST == MULTICAST_SUB)
		tcpaddr(arg->addr, nodes[id], notifier_port);
	else
		tcpaddr(arg->addr, nodes[id], synthesizer_port);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, do_connect_synthesizer, arg);
	pthread_attr_destroy(&attr);
}


void connect_synthesizers()
{
	int i;

	for (i = 0; i < nr_nodes; i++)
		if (i != node_id)
			connect_synthesizer(i);
}


rep_t synth_responder(req_t req)
{
	int i;
	char *paddr;
	rep_t rep = 0;
	struct in_addr addr;

	memcpy(&addr, &req, sizeof(struct in_addr));
	paddr = inet_ntoa(addr);
	log_debug("synth_responder: addr=%s", paddr);

	for (i = 0; i < nr_nodes; i++) {
		if (!strcmp(paddr, nodes[i])) {
			connect_synthesizer(i);
			break;
		}
	}

	if (i == nr_nodes) {
		log_err("failed to connect to synthesizer");
		req = -1;
	}

	return rep;
}


int create_synth_responder()
{
	pthread_t thread;
	pthread_attr_t attr;
	responder_arg_t *arg;

	arg = (responder_arg_t *)calloc(1, sizeof(responder_arg_t));
	if (!arg) {
		log_err("no memeory");
		return -ENOMEM;
	}
	arg->responder = synth_responder;
	tcpaddr(arg->addr, inet_ntoa(get_addr()), synthesizer_port);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, start_responder, arg);
	pthread_attr_destroy(&attr);

	return 0;
}


int create_synthesizer()
{
	pthread_t thread;
	pthread_attr_t attr;

	if (init_synthesizer()) {
		log_err("failed to initialize");
		return -EINVAL;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_create(&thread, &attr, do_deliver, NULL);

	if ((MULTICAST == MULTICAST_SUB) || (MULTICAST == MULTICAST_PGM) || (MULTICAST == MULTICAST_EPGM))
		create_synth_responder();
	else
		connect_synthesizers();

	return 0;
}
