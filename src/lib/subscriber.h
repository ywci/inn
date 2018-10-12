#ifndef _SUBSCRIBER_H
#define _SUBSCRIBER_H

#include <default.h>

typedef struct sub_arg {
    char src[ADDR_SIZE];
    char dest[ADDR_SIZE];
} sub_arg_t;

void *subscriber_start(void *ptr);

#endif
