#ifndef _CALLBACK_H
#define _CALLBACK_H

#include "log.h"
#include "util.h"

#ifdef EVAL
#define DEFAULT() evaluate(buf, size)
#else
#define DEFAULT() do {} while (0)
#endif

void callback(char *buf, size_t size);

#endif
