#ifndef _EVENT_H
#define _EVENT_H

#include <time.h>
#include <default.h>
#include <pthread.h>
#include "log.h"

#ifdef LINUX
#define TIMEDWAIT
#endif

#define SECOND 				1000000000

typedef struct event {
	int active;
	int timeout;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
} event_t;

int event_init(event_t *ev);
void event_set(event_t *ev);
void event_wait(event_t *ev);

#endif
