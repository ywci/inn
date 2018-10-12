/* banchmark.c
 *
 * Copyright (C) 2018 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "banchmark.h"

#ifdef CLONE_TS
#include "gen.h"
#endif

typedef struct {
    int hid;
    int cnt;
    struct timeval t;
} hdr_t;

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
    int *p;
    char *buf;
    int cnt = 0;
    int opt = 0;
    void *socket;
    void *context;
    int count = NR_PACKETS;
    int hwm = HIGH_WATER_MARK;
#ifndef CLONE_TS
    size_t size = sizeof(hdr_t);
    hdr_t *hdr;
#else
    size_t size = sizeof(timestamp_t);
    gen_init();
#endif
    if (argc > 0) {
        while ((opt = getopt(argc, argv, "s:r:")) != -1) {
            switch(opt) {
            case 's':
                size = strtol(optarg, NULL, 10);
                break;
            case 'r':
                count = strtol(optarg, NULL, 10);
                break;
            default:
                printf("usage: %s [-s size] [-r requests]\n", argv[0]);
                exit(-1);
            }
        }
    }
#ifndef CLONE_TS
    if (size < sizeof(hdr_t)) {
        printf("Error: the packet size should be greater than %lu bytes\n", sizeof(hdr_t));
        return -1;
    }
#else
    if (size < sizeof(timestamp_t)) {
        printf("Error: the packet size should be greater than %lu bytes\n", sizeof(timestamp_t));
        return -1;
    }
#endif
    printf("banchmark: size=%zu, requests=%d\n", size, count);
    buf = malloc(size);
    if (!buf) {
        printf("Error: no memory\n");
        exit(-1);
    }
#ifndef CLONE_TS
    hdr = (hdr_t *)buf;
    hdr->hid = get_hid();
#endif
    context = zmq_ctx_new();
    socket = zmq_socket(context, ZMQ_PUSH);
    zmq_setsockopt(socket, ZMQ_SNDHWM, &hwm, sizeof(hwm));
    zmq_connect(socket, ADDR);
    for (int i = 0; i < count; i++) {
        zmsg_t *msg;
        zframe_t *frame;
#ifndef CLONE_TS
        hdr->cnt = cnt;
        gettimeofday(&hdr->t, NULL);
#else
        gen_ts((timestamp_t *)buf);
#endif
        msg = zmsg_new();
        frame = zframe_new(buf, size);
        zmsg_append(msg, &frame);
        zmsg_send(&msg, socket);
        cnt++;
    }
    zmq_close(socket);
    zmq_ctx_destroy(context);
    free(buf);
    return 0;
}
