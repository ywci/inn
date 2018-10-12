#ifndef _SAMPLER_H
#define _SAMPLER_H

#include "util.h"

int sampler_create();
void sampler_drain();
void sampler_resume();
void sampler_suspend();
void sampler_stop_filter();
void sampler_handle(int id, zmsg_t *msg);
void sampler_start_filter(host_time_t bound);

#endif
