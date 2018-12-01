/* handler.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "handler.h"
#include "callback.h"
#include "evaluator.h"

void handle(char *buf, size_t size)
{
#ifdef EVALUATE
    evaluate(buf, size);
#endif
    callback(buf, size);
}
