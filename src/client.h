#ifndef _CLIENT_H
#define _CLIENT_H

#include <errno.h>
#include <stdlib.h>
#include <default.h>
#include <pthread.h>
#include <sys/time.h>
#include "publisher.h"
#include "requester.h"
#include "subscriber.h"
#include "util.h"

#define CLIENT_ADDR "ipc:///tmp/innclient"

int create_client();

#endif
