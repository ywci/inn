#ifndef _EV_H
#define _EV_H

#include <time.h>
#include <default.h>
#include <pthread.h>

#define EV_INTERVAL 100000000 // nsec (< 1 second)

typedef struct ev {
	int active;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} ev_t;

int ev_init(ev_t *ev);
void ev_set(ev_t *ev);
void ev_wait(ev_t *ev);

#endif
