#ifndef _PARSER_H
#define _PARSER_H

#include <yaml.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <default.h>
#include "log.h"

int parse();

#endif
