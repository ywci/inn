/*      requester.c
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

#include "requester.h"

int request(char *addr, req_t *req, rep_t *rep)
{
	int rc;
	int ret = 0;
    void *context = zmq_ctx_new();
    void *socket = zmq_socket(context, ZMQ_REQ);
	
    rc = zmq_connect(socket, addr);
	if (rc) {
		log_err("failed to connect");
		return -EINVAL;
	}
	rc = zmq_send(socket, req, sizeof(req_t), 0);
	if (rc != sizeof(req_t)) {
		log_err("failed to send request");
		ret = -EIO;
		goto out;
	}
	rc = zmq_recv(socket, rep, sizeof(rep_t), 0);
	if (rc != sizeof(rep_t)) {
		log_err("failed to receive reply");
		ret = -EIO;
		goto out;
	}
out:
    zmq_close(socket);
    zmq_ctx_destroy(context);
    return ret;
}
