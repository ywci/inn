#ifndef _MIXER_H
#define _MIXER_H

#include <default.h>
#include <pthread.h>
#include "lib/ev.h"
#include "lib/queue.h"
#include "lib/record.h"
#include "lib/candidate.h"

#define MIXER_ADDR "ipc:///tmp/innm"

typedef struct mixer_arg {
	char addr[ADDR_SIZE];
	int id;
} mixer_arg_t;

void refresh();
bool need_balance();
int get_seq(int id);
void resume_mixer();
void create_mixer();
void suspend_mixer();
int next_seq(int id);
int check_step(int id);
void update_seq(int id);
void set_active(int id);
void set_inactive(int id);
int get_matrix_item(int row, int col);
int update_queue(int id, que_item_t *item);
void update_matrix(int row, int col, int step);
void insert_message(int id, int seq, zmsg_t *msg);

#endif
