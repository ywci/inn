/* event.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "event.h"

int event_init(event_t *ev)
{
	if (ev->timeout) {
#ifdef TIMEDWAIT
		pthread_condattr_t attr;

		if (ev->timeout >= SECOND) {
			log_err("failed to initialize");
			return -1;
		}
		pthread_condattr_init(&attr);
		if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) {
			log_err("failed to initialize");
			return -1;
		}
		pthread_cond_init(&ev->cond, &attr);
#else
		pthread_cond_init(&ev->cond, NULL);
#endif
	} else
		pthread_cond_init(&ev->cond, NULL);
	pthread_mutex_init(&ev->mutex, NULL);
	ev->active = 0;
	return 0;
}


void event_set(event_t *ev)
{
	pthread_mutex_lock(&ev->mutex);
	if (!ev->active) {
		pthread_cond_signal(&ev->cond);
		ev->active = 1;
	}
	pthread_mutex_unlock(&ev->mutex);
}


int event_wait(event_t *ev)
{
	int ret = 0;

	pthread_mutex_lock(&ev->mutex);
	if (!ev->active) {
		if (ev->timeout) {
#ifdef TIMEDWAIT
			unsigned long tmp;
			struct timespec timeout;

			clock_gettime(ev->timeout, &timeout);
			tmp = timeout.tv_nsec + ev->timeout;
			if (tmp >= SECOND) {
				timeout.tv_sec += 1;
				timeout.tv_nsec = tmp - SECOND;
			} else
				timeout.tv_nsec = tmp;
			ret = pthread_cond_timedwait(&ev->cond, &ev->mutex, &timeout);
#else
			pthread_cond_wait(&ev->cond, &ev->mutex);
#endif
		} else
			pthread_cond_wait(&ev->cond, &ev->mutex);
	}
	ev->active = 0;
	pthread_mutex_unlock(&ev->mutex);
	return ret;
}
