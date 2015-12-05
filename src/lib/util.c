/*      util.c
 *      
 *      Copyright (C) 2015 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "util.h"

void forward(void *src, void *dest, callback_t callback, sender_t sender)
{
	while (true) {
		zmsg_t *msg = zmsg_recv(src);
		
		if (callback)
			msg = callback(msg);
		if (msg) {
			if (sender)
				sender(&msg, dest);
			else
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


int timestamp_compare(char *ts1, char *ts2)
{
	return memcmp(ts1, ts2, TIMESTAMP_SIZE);
}


void extract_timestamp(char *ts, unsigned long *sec, unsigned long *usec, unsigned long *src)
{
	*src = 0;
	*sec = 0;
	*usec = 0;
	memcpy(sec, ts, 4);
	memcpy(usec, &ts[4], 3);
	memcpy(src, &ts[7], 4);
}


void show_timestamp(const char *role, int id, char *ts)
{
	unsigned long src;
	unsigned long sec;
	unsigned long usec;
	
	extract_timestamp(ts, &sec, &usec, &src);
	if (id >= 0)
		log_debug("%s_%d: ts=%08lx%05lx.%lx", role, id, sec, usec, src);
	else
		log_debug("%s: ts=%08lx%05lx.%lx", role, sec, usec, src);
}


void show_vector(const char *role, int id, byte *vector)
{
	int i;
	int cnt = 0;
	int pos = 0;
	char buf[256];
	char *p = buf;
	byte val = vector[0];
	const unsigned int mask = (1 << NBIT) - 1;
	
	if (id >= 0)
		sprintf(buf, "%s_%d: <vector> |", role, id);
	else
		sprintf(buf, "%s: <vector> |", role);
	p += strlen(p);
	for (i = 0; i < nr_nodes; i++) {
		if (i != id) {
			sprintf(p, "%d|", val & mask);
			cnt += NBIT;
			if (cnt == 8) {
				cnt = 0;
				pos += 1;
				if (pos < vector_size)
					val = vector[pos];
			} else
				val = val >> NBIT;
		} else
			sprintf(p, "0|");
		p += strlen(p);
	}
	log_debug("%s", buf);
}


void show_matrix(int mtx[NODE_MAX][NODE_MAX], int h, int w)
{
	int i;
	int j;
	char *p;
	char *buf = (char *)malloc(9 * w + 2);
	
	log_debug("[matrix]");
	for (i = 0; i < h; i++) {
		strcpy(buf, "|");
		p = buf + strlen(buf);
		for (j = 0; j < w; j++) {
			sprintf(p, "%08d|", mtx[i][j]);
			p += strlen(p);
		}
		log_debug("%s", buf);
	}
	free(buf);
}


void show_queue(que_t *queue)
{
	const int blank = 256;
	const int bufsz = 1024;
	const int item_max = 8;
	que_item_t *item;
	char buf[bufsz];
	char *p = buf;
	int len = 0;
	int i = 0;
	
	if (list_empty(&queue->head))
		return;
	
	sprintf(p, "queue_%d: ", queue->id);
	p += strlen(p);
	list_for_each_entry(item, &queue->head, item) {
		size_t l;
		unsigned long src;
		unsigned long sec;
		unsigned long usec;
		int access = (item->flgs & FL_ACCESS) != 0;
		int visible = (item->flgs & FL_VISIBLE) != 0;
		
		extract_timestamp(item->timestamp, &sec, &usec, &src);
		sprintf(p, "(%d)%08lx%05lx.%lx v=%d a=%d seq=%d ", i, sec, usec, src, visible, access, item->seq);
		l = strlen(p);
		len += l;
		p += l;
		i++;
		if ((i == item_max) || (len + blank >= bufsz))
			break;
	}
	log_debug("%s", buf);
}


void show_seq(const char *str, int seq[NODE_MAX])
{
	const int bufsz = 1024;
	const int blank = 256;
	char buf[bufsz];
	char *p = buf;
	int i;
	
	if (strlen(str) + blank >= bufsz)
		return;
	
	sprintf(p, "%s: seq=", str);
	p += strlen(p);
	for (i = 0; i < nr_nodes; i++) {
		sprintf(p, "%d ", seq[i]);
		p += strlen(p);
	}
	log_debug("%s", buf);
}


void show_bitmap(const char *str, bitmap_t bitmap)
{
	const int bufsz = 1024;
	const int blank = 256;
	char buf[bufsz];
	char *p = buf;
	int i;
	
	if (strlen(str) + blank >= bufsz)
		return;
	
	sprintf(p, "%s: bitmap=|", str);
	p += strlen(p);
	for (i = 0; i < nr_nodes; i++) {
		sprintf(p,"%d|", (bitmap & node_mask[i]) != 0);
		p += strlen(p);
	}
	log_debug("%s", buf);
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


void set_timeout(struct timespec *time, int t)
{
	long nsec = (t % 1000000) * 1000;
	
	clock_gettime(CLOCK_REALTIME, time);
	time->tv_sec += t / 1000000;
	if (time->tv_nsec + nsec >= 1000000000) {
		time->tv_nsec += nsec - 1000000000;
		time->tv_sec += 1;
	} else
		time->tv_nsec += nsec;
}


void wait_timeout(int t)
{
	pthread_cond_t cond;
	struct timespec time;
	pthread_mutex_t mutex;

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	set_timeout(&time, t);

	pthread_mutex_lock(&mutex);
	pthread_cond_timedwait(&cond, &mutex, &time);
	pthread_mutex_unlock(&mutex);
}
