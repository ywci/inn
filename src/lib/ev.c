/*      ev.c
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

#include "ev.h"

int ev_init(ev_t *ev) 
{
#ifdef TIMEDWAIT
	pthread_condattr_t attr;
	
	pthread_condattr_init(&attr);
	if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC))
		return -1;
	pthread_cond_init(&ev->cond, &attr);
#else
	pthread_cond_init(&ev->cond, NULL);
#endif
	pthread_mutex_init(&ev->mutex, NULL);
	ev->active = 0;
	return 0;
}


void ev_set(ev_t *ev)
{
	pthread_mutex_lock(&ev->mutex);
	if (!ev->active) {
		pthread_cond_signal(&ev->cond);
		ev->active = 1;
	}
	pthread_mutex_unlock(&ev->mutex);
}


void ev_wait(ev_t *ev)
{
#ifdef TIMEDWAIT
	unsigned long tmp;
	struct timespec timeout;
	
	clock_gettime(CLOCK_MONOTONIC, &timeout);
	tmp = timeout.tv_nsec + EV_INTERVAL;
	if (tmp > 999999999) {
		timeout.tv_sec += 1;
		timeout.tv_nsec = tmp - 1000000000;
	} else
		timeout.tv_nsec = tmp;
#endif
	pthread_mutex_lock(&ev->mutex);
	if (!ev->active) {
#ifdef TIMEDWAIT
		pthread_cond_timedwait(&ev->cond, &ev->mutex, &timeout);
#else
		pthread_cond_wait(&ev->cond, &ev->mutex);
#endif
	}
	ev->active = 0;
	pthread_mutex_unlock(&ev->mutex);
}
