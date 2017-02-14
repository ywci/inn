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

#define pgmaddr(addr, orig, port) map_addr("pgm", addr, orig, port)
#define epgmaddr(addr, orig, port) map_addr("epgm", addr, orig, port)
#define tcpaddr(addr, orig, port) sprintf(addr, "tcp://%s:%d", orig, port)

#define show_array(str, name, array) do { \
	const int bufsz = 1024; \
	const int reserve = 256; \
	if (strlen(str) + reserve < bufsz) { \
		int i; \
		char buf[bufsz]; \
		char *p = buf; \
		assert(nr_nodes > 1); \
		sprintf(p, "%s: %s=>[", str, name); \
		p += strlen(p); \
		for (i = 0; i < nr_nodes - 1; i++) { \
			sprintf(p, "%d, ", array[i]); \
			p += strlen(p); \
		} \
		sprintf(p, "%d]", array[i]); \
		log_debug("%s", buf); \
	} \
} while (0)

#define show_bitmap(str, bitmap) do {\
	const int bufsz = 1024; \
	const int reserve = 256; \
	if (strlen(str) + reserve < bufsz) { \
		int i; \
		char buf[bufsz]; \
		char *p = buf; \
		sprintf(p, "%s: bitmap=>|", str); \
		p += strlen(p); \
		for (i = 0; i < nr_nodes; i++) { \
			sprintf(p,"%d|", (bitmap & node_mask[i]) != 0); \
			p += strlen(p); \
		} \
		log_debug("%s", buf); \
	} \
} while (0)

#ifdef SHOW_TIMESTAMP
#define show_timestamp(str, id, ts) do { \
	hid_t src; \
	unsigned long sec; \
	unsigned long usec; \
	extract_timestamp(ts, &sec, &usec, &src); \
	if (id >= 0) \
		log_debug("%s: timestamp=%08lx%05lx.%lx (id=%d)", str, sec, usec, (unsigned long)src, id); \
	else \
		log_debug("%s: timestamp=%08lx%05lx.%lx", str, sec, usec, (unsigned long)src); \
} while (0)
#else
#define show_timestamp(str, id, ts) do {} while (0)
#endif

#ifdef SHOW_VECTOR
#define show_vector(str, id, vector) do { \
	int i; \
	int cnt = 0; \
	int pos = 0; \
	char buf[256]; \
 	char *p = buf; \
	byte val = vector[0]; \
	const unsigned int mask = (1 << NBIT) - 1; \
	sprintf(buf, "%s: [vector]=>|", str); \
	p += strlen(p); \
	for (i = 0; i < nr_nodes; i++) { \
		if (i != id) { \
			sprintf(p, "%d|", val & mask); \
			cnt += NBIT; \
			if (cnt == 8) { \
				cnt = 0; \
				pos += 1; \
				if (pos < vector_size) \
					val = vector[pos]; \
			} else \
				val = val >> NBIT; \
		} else \
			sprintf(p, "0|"); \
		p += strlen(p); \
	} \
	if (id >= 0) \
		sprintf(p, " (id=%d)", id); \
	log_debug("%s", buf); \
} while (0)
#else
#define show_vector(str, id, vector) do {} while (0)
#endif

#ifdef SHOW_MATRIX
#define show_matrix(mtx, h, w) do { \
	int i; \
	int j; \
	char *p; \
	char *buf = (char *)malloc(9 * w + 2); \
	log_debug("[matrix]"); \
	for (i = 0; i < h; i++) { \
		strcpy(buf, "|"); \
		p = buf + strlen(buf); \
		for (j = 0; j < w; j++) { \
			sprintf(p, "%08d|", mtx[i][j]); \
			p += strlen(p); \
		} \
		log_debug("%s", buf); \
	} \
	free(buf); \
} while (0)
#else
#define show_matrix(mtx, h, w) do {} while (0)
#endif

#ifdef SHOW_RECORD
#define show_record(id, rec) do { \
	if (rec) { \
		show_timestamp(__func__, id, rec->timestamp); \
		show_bitmap(__func__, rec->bitmap); \
		show_array(__func__, "seq", rec->seq); \
		show_array(__func__, "ready", rec->ready); \
		show_array(__func__, "visible", rec->visible); \
	} \
} while (0)
#else
#define show_record(req, id) do {} while (0)
#endif

#define timestamp_empty(ts) ((ts)[TIMESTAMP_SIZE - 1] == 0)
#define timestamp_clear(ts) memset(ts, 0, TIMESTAMP_SIZE)
#define timestamp_copy(dest, src) memcpy(dest, src, TIMESTAMP_SIZE)

typedef struct timeval timeval_t;
typedef void (*sender_t)(zmsg_t *);
typedef zmsg_t *(*callback_t)(zmsg_t *);

typedef struct sender_desc {
	int total;
	sender_t sender;
	void *desc[NODE_MAX];
} sender_desc_t;

hid_t get_hid();
struct in_addr get_addr();
void evaluate(char *buf, size_t size);
void publish(sender_desc_t *sender, zmsg_t *msg);
void set_timestamp(char *timestamp, struct timeval *tv);
void map_addr(const char *protocol, char *addr, char *orig, int port);
void forward(void *frontend, void *backend, callback_t callback, sender_desc_t *sender);
void extract_timestamp(char *ts, unsigned long *sec, unsigned long *usec, hid_t *src);
int timestamp_compare(char *ts1, char *ts2);

#endif
