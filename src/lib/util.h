#ifndef _UTIL_H
#define _UTIL_H

#include <czmq.h>
#include <net/if.h>
#include <stdlib.h>
#include <default.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "queue.h"
#include "log.h"

typedef zmsg_t *(*callback_t)(zmsg_t *);
typedef void (*sender_t)(zmsg_t **, void *);

#define is_empty_timestamp(ts) ((ts)[TIMESTAMP_SIZE - 1] == 0)

void show_queue(que_t *queue);
void show_seq(const char *str, int seq[NODE_MAX]);
void show_bitmap(const char *str, bitmap_t bitmap);
void show_timestamp(const char *role, int id, char *ts);
void show_vector(const char *role, int id, byte *vector);
void show_matrix(int mtx[NODE_MAX][NODE_MAX], int h, int w);

void wait_timeout(int t);
struct in_addr get_addr();
int timestamp_compare(char *ts1, char *ts2);
void set_timestamp(char *timestamp, struct timeval *tv);
void forward(void *frontend, void *backend, callback_t callback, sender_t sender);

#endif
