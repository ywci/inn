#ifndef _SYNTHESIZER_H
#define _SYNTHESIZER_H

#include "util.h"

int synth_create();
bool synth_drain();
void synth_wakeup();
void synth_recover(int id);
void synth_suspect(int id);
liveness_t synth_get_liveness(int id);
void synth_set_liveness(int id, liveness_t liveness);
void synth_update(int id, timestamp_t *timestamp, zmsg_t *msg);

#endif
