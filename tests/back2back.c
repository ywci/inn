/* back2back.c
 *
 * Copyright (C) 2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include <zmq.h>
#include <czmq.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <net/if.h>
#include <string.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define NR_PACKETS 10000
#define PACKET_SIZE 32
#define ADDR "ipc:///tmp/inndaemon"
#define IFACE "eth0"

typedef uint32_t hid_t;
typedef struct timeval timeval_t;

struct in_addr get_addr()
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, IFACE, IFNAMSIZ - 1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
}


hid_t get_hid()
{
	struct in_addr addr = get_addr();

	return (hid_t)addr.s_addr;
}


int main(int argc, char **argv)
{
	char *buf;
	int opt = 0;
	timeval_t *p;
	void *socket;
	void *context;
	hid_t id = get_hid();
	int count = NR_PACKETS;
	size_t size = PACKET_SIZE;
	struct in_addr addr = get_addr();
	const int sz_default = sizeof(timeval_t) + sizeof(hid_t);

	if (argc > 0) {
		while ((opt = getopt(argc, argv, "s:c:")) != -1) {
			switch(opt) {
			case 's':
				size = strtol(optarg, NULL, 10);
				break;
			case 'c':
				count = strtol(optarg, NULL, 10);
				break;
			default:
				printf("usage: %s [-s size] [-c count]\n", argv[0]);
				exit(-1);
			}
		}
	}

	if (size < sz_default) {
		printf("Error: the packet size should be greater than %d\n", sz_default);
		free(buf);
		return -1;
	}

	printf("back2back: size=%zu, count=%d\n", size, count);
	buf = malloc(size);
	if (!buf) {
		printf("Error: no memory\n");
		exit(-1);
	}

	context = zmq_ctx_new();
	socket = zmq_socket(context, ZMQ_PUSH);
	zmq_connect(socket, ADDR);
	memcpy(buf, &id, sizeof(hid_t));
	p = (timeval_t *)&buf[sizeof(hid_t)];
	for (int i = 0; i < count; i++) {
		zmsg_t *msg;
		zframe_t *frame;

		gettimeofday(p, NULL);
		msg = zmsg_new();
		frame = zframe_new(buf, PACKET_SIZE);
		zmsg_append(msg, &frame);
		zmsg_send(&msg, socket);
	}

	zmq_close(socket);
	zmq_ctx_destroy(context);
	free(buf);
	return 0;
}
