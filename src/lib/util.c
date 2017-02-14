/* util.c
 *
 * Copyright (C) 2015-2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "util.h"

uint32_t hid = 0; // host id
bool initialize = false;
unsigned long req_count = 0;

//Multicast Addresses//-------------------------------//
#define MULTICAST_MAX 254
#define MULTICAST_NET "224.0.0."
#if NODE_MAX > MULTICAST_MAX
#error NODE_MAX > MULTICAST_MAX
#endif
//----------------------------------------------------//

int timestamp_compare(char *ts1, char *ts2)
{
	return memcmp(ts1, ts2, TIMESTAMP_SIZE);
}


void map_addr(const char *protocol, char *addr, char *orig, int port)
{
	int i;
	char buf[sizeof(struct in_addr)];

	if (!inet_aton(orig, (struct in_addr *)buf)) {
		log_err("failed to map address %s", orig);
		return;
	}

	i = buf[3];
	if ((i == 0) || (i > MULTICAST_MAX)) {
		log_err("failed to map address %s", orig);
		return;
	}

	if (!strcmp(protocol, "pgm"))
		sprintf(addr, "pgm://%s;%s%d:%d", inet_ntoa(get_addr()), MULTICAST_NET, i, port);
	else if (!strcmp(protocol, "epgm"))
		sprintf(addr, "epgm://%s;%s%d:%d", iface, MULTICAST_NET, i, port);
	else
		log_err("failed to map address, invalid protocol %s, addr=%s", protocol, orig);
}


void publish(sender_desc_t *sender, zmsg_t *msg)
{
	int i;
	zmsg_t *dup;

	for (i = 0; i < sender->total - 1; i++) {
		dup = zmsg_dup(msg);
		zmsg_send(&dup, sender->desc[i]);
	}

	zmsg_send(&msg, sender->desc[i]);
}


void forward(void *src, void *dest, callback_t callback, sender_desc_t *sender)
{
	while (true) {
		zmsg_t *msg = zmsg_recv(src);

		if (callback)
			msg = callback(msg);

		if (msg) {
			if (sender) {
				if (sender->sender)
					sender->sender(msg);
				else
					publish(sender, msg);
			} else
				zmsg_send(&msg, dest);
		}
	}
}


struct in_addr get_addr()
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
}


void extract_timestamp(char *ts, unsigned long *sec, unsigned long *usec, hid_t *src)
{
	*src = 0;
	*sec = 0;
	*usec = 0;
	memcpy(sec, ts, 4);
	memcpy(usec, &ts[4], 3);
	memcpy(src, &ts[7], sizeof(hid_t));
}


void set_timestamp(char *timestamp, struct timeval *tv)
{
	int sec = tv->tv_sec;
	int usec = tv->tv_usec;

	timestamp[0] = (sec >> 24) & 0xff;
	timestamp[1] = (sec >> 16) & 0xff;
	timestamp[2] = (sec >> 8) & 0xff;
	timestamp[3] = sec & 0xff;
	timestamp[4] = (usec >> 16) & 0xff;
	timestamp[5] = (usec >> 8) & 0xff;
	timestamp[6] = usec & 0xff;
}


inline double timediff(timeval_t *start, timeval_t *end)
{
	if (end->tv_usec < start->tv_usec)
	 	return end->tv_sec - 1 - start->tv_sec + (1000000 + end->tv_usec - start->tv_usec) / 1000000.0;
	else
		return end->tv_sec - start->tv_sec + (end->tv_usec - start->tv_usec) / 1000000.0;
}


hid_t get_hid()
{
	struct in_addr addr = get_addr();

	return (hid_t)addr.s_addr;
}


void evaluate(char *buf, size_t size)
{
	static double delay = 0;
	static timeval_t start_time;
	hid_t id = ((hid_t *)buf)[0];

	if (!initialize) {
		hid = get_hid();
		initialize = true;
	}

	if (id == hid) {
		double t;
		timeval_t now;
		timeval_t *p = (timeval_t *)&buf[sizeof(hid_t)];

		gettimeofday(&now, NULL);
		t = timediff(p, &now);
		delay += t;
		log_debug("evaluate: size=%zu, count=%lu, t=%fsec", size, req_count, t);
		req_count++;

		if (1 == req_count)
			start_time = now;

		if (req_count == nr_requests) {
			FILE * fp;
			int rps;

			t = timediff(&start_time, &now);
			rps = nr_requests / t;
			delay /= nr_requests;
			fp = fopen(PATH_LOG, "w");
			fprintf(fp, "rps: %d\ndelay: %f\n", rps, delay);
			fclose(fp);
			log_eval("evaluate: finished, rps=%d, delay=%f", rps, delay);
		}
	}
}
