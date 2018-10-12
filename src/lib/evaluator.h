#ifndef _EVALUATOR_H
#define _EVALUATOR_H

#include "util.h"

#define get_evaluator(hid) (hid % nr_nodes)

void eval_create();
void evaluate(char *buf, size_t size);

#endif
