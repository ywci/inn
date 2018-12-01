#include <time.h>
#include <stdlib.h>
#include "banchmark.h"
#include "gen.h"

#define GEN_CLI_MAX 16
#define GEN_CLI_NET "192.168.1."

#ifdef COUNTABLE
#define timestamp_set(timestamp, second, cnt) do { \
    timestamp->sec = second;                       \
    timestamp->usec = cnt;                         \
} while (0)
#else
#define timestamp_set(timestamp, time_val) do {   \
    timestamp->sec = time_val.tv_sec;             \
    timestamp->usec = time_val.tv_usec;           \
} while (0)
#endif

struct {
    hid_t hid[GEN_CLI_MAX];
#ifndef COUNTABLE
    timeval_t t[GEN_CLI_MAX];
#else
    uint32_t sec[GEN_CLI_MAX];
    uint32_t count[GEN_CLI_MAX];
#endif
} gen_status;

void gen_init()
{
    memset(&gen_status, 0, sizeof(gen_status));
    for (int i = 0; i < GEN_CLI_MAX; i++) {
        struct in_addr addr;
        char name[64] = {0};

        sprintf(name, "%s%d", GEN_CLI_NET, i + 1);
        if (!inet_aton(name, &addr)) {
            printf("failed to convert address %s\n", name);
            exit(-1);
        }
        printf("Gen: client=%s, hid=%x\n", name, addr2hid(addr));
        gen_status.hid[i] = addr2hid(addr);
    }
    srand(time(NULL));
}


void gen_ts(timestamp_t *timestamp)
{
    timeval_t curr;
    int n = rand() % GEN_CLI_MAX;

    timestamp->hid = gen_status.hid[n];
#ifdef COUNTABLE
    get_time(curr);
    gen_status.sec[n] = curr.tv_sec;
    gen_status.count[n]++;
    timestamp_set(timestamp, curr.tv_sec, gen_status.count[n]);
#else
    do {
        get_time(curr);
    } while ((curr.tv_sec == gen_status.t[n].tv_sec)
          && (curr.tv_usec == gen_status.t[n].tv_usec));
    gen_status.t[n] = curr;
    timestamp_set(timestamp, curr);
#endif
}
