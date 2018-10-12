/* handler.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "handler.h"
#include "call.h"

void handle(char *buf, size_t size)
{
    call(buf, size);
}
