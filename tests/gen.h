#ifndef _GEN_H
#define _GEN_H

#include <netinet/in.h>

#define get_time(t) gettimeofday(&(t), NULL)
#define addr2hid(addr) ((hid_t)(addr).s_addr)

typedef uint32_t hid_t;
typedef uint32_t timestamp_sec_t;
typedef uint32_t timestamp_usec_t;

typedef struct __attribute__((__packed__)) {
    timestamp_sec_t sec;
    timestamp_usec_t usec;
    hid_t hid;
} timestamp_t;

void gen_init();
void gen_ts(timestamp_t *timestamp);

#endif
