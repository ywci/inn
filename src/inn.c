/*      inn.c
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

#include "inn.h"

int majority = 0;
int node_id = -1;
int nr_nodes = 0;
int vector_size = 0;
bitmap_t node_mask[NODE_MAX];
bitmap_t bitmap_available = 0;

int mixer_port = 0;
int client_port = 0;
int replayer_port = 0;
int collector_port = 0;
int sequencer_port = 0;
int heartbeat_port = 0;

char iface[IFACE_SIZE];
char nodes[NODE_MAX][ADDR_LEN];

int load_conf()
{
	int i;
	char *ptr;
	char *str;
	int cnt = 0;
	int ret = 0;
	ezxml_t f, node, item;
	struct in_addr hostaddr;
	
	f = ezxml_parse_file(CONF_INN);
	item = ezxml_child(f, "iface");
	str = ezxml_child(item, "name")->txt;
	strcpy(iface, str);
	ezxml_free(f);
	hostaddr = get_addr();
	
	f = ezxml_parse_file(CONF_NODE);
	for (node = ezxml_child(f, "node"); node; node = node->next) {
		if (cnt < NODE_MAX) {
			char *addr = ezxml_child(node, "address")->txt;
			
			if (strlen(addr) >= ADDR_LEN) {
				log_err("invalid address");
				ret = -EINVAL;
				goto out;
			}
			strcpy(nodes[cnt], addr);
			cnt++;
		} else {
			log_err("invalid address");
			ret = -EINVAL;
			goto out;
		}
	}
	ezxml_free(f);
	nr_nodes = cnt;
	bitmap_available = (bitmap_t)((1 << cnt) - 1);
	for (i = 0; i < cnt; i++) 
		node_mask[i] = 1 << i;
	majority = nr_nodes / 2 + 1;
	if ((8 % NBIT) != 0) {
		log_err("NBIT is invalidate");
		ret = -EINVAL;
		goto out;
	}
	vector_size = ((cnt - 1) * NBIT + 7) / 8;
	for (i = 0; i < nr_nodes; i++) {
		struct in_addr tmp;
		
		inet_aton(nodes[i], &tmp);
		if (!memcmp(&hostaddr, &tmp, sizeof(struct in_addr))) {
			node_id = i;
			break;
		}
	}
	log_debug("node_id=%d", node_id);
	
	f = ezxml_parse_file(CONF_PORT);
	item = ezxml_child(f, "sequencer");
	str = ezxml_child(item, "port")->txt;
	sequencer_port = strtol(str, &ptr, 10);
	if (str == ptr) {
		log_err("invalid sequencer port");
		ret = -EINVAL;
		goto out;
	}
	
	item = ezxml_child(f, "mixer");
	str = ezxml_child(item, "port")->txt;
	mixer_port = strtol(str, &ptr, 10);
	if (str == ptr) {
		log_err("invalid mixer port");
		ret = -EINVAL;
		goto out;
	}
	
	item = ezxml_child(f, "heartbeat");
	str = ezxml_child(item, "port")->txt;
	heartbeat_port = strtol(str, &ptr, 10);
	if (str == ptr) {
		log_err("invalid heartbeat port");
		ret = -EINVAL;
		goto out;
	}
	
	item = ezxml_child(f, "collector");
	str = ezxml_child(item, "port")->txt;
	collector_port = strtol(str, &ptr, 10);
	if (str == ptr) {
		log_err("invalid collector port");
		ret = -EINVAL;
		goto out;
	}
	
	item = ezxml_child(f, "replayer");
	str = ezxml_child(item, "port")->txt;
	replayer_port = strtol(str, &ptr, 10);
	if (str == ptr) {
		log_err("invalid replayer port");
		ret = -EINVAL;
		goto out;
	}
	
	item = ezxml_child(f, "client");
	str = ezxml_child(item, "port")->txt;
	client_port = strtol(str, &ptr, 10);
	if (str == ptr) {
		log_err("invalid client port");
		ret = -EINVAL;
		goto out;
	}
out:
	ezxml_free(f);
	return ret;
}


void usage()
{
	printf("inn [-s | -c]\n");
	printf("-s: create server\n");
	printf("-c: create client\n");
}


void create_server()
{
	pthread_t thread = create_sequencer();
	
	if (thread < 0) {
		log_err("failed to create sequencer");
		return;
	}
	create_mixer();
#ifdef HEARTBEAT
	create_replayer();
	create_heartbeat();
	create_collector();
#endif
	pthread_join(thread, NULL);
}


int main(int argc, char **argv)
{
	if (argc != 2) {
		usage();
		return -1;
	}
	
	if (load_conf()) {
		log_err("failed to load configuration");
		return -1;
	}
	
	if (!strcmp(argv[1], "-s"))
		create_server();
	else if (!strcmp(argv[1], "-c"))
		create_client();
	else {
		usage();
		return -1;
	}
	return 0;
}
