/* parser.c
 *
 * Copyright (C) 2017 Yi-Wei Ci
 *
 * Distributed under the terms of the MIT license.
 */

#include "parser.h"

int node_id = -1;
int majority = -1;
int nr_nodes = -1;
int vector_size = -1;

int client_port = -1;
int sampler_port = -1;
int notifier_port = -1;
int replayer_port = -1;
int collector_port = -1;
int heartbeat_port = -1;
int synthesizer_port = -1;

char iface[IFNAME_SIZE];
char nodes[NODE_MAX][IPADDR_SIZE];

bitmap_t available_nodes = 0;
bitmap_t node_mask[NODE_MAX];

struct in_addr ifaddr()
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


int parse_port(yaml_node_t *start, yaml_node_t *node, int *port, const char *name)
{
	yaml_node_pair_t *p;

	for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
		yaml_node_t *key = &start[p->key - 1];
		yaml_node_t *val = &start[p->value - 1];
		char *key_str = (char *)key->data.scalar.value;
		char *val_str = (char *)val->data.scalar.value;

		if (!strcmp(key_str, "port")) {
			*port = strtol(val_str, NULL, 10);
			log_debug("%s: port=%d", name, *port);
			break;
		}
	}

	return 0;
}


int parse_sampler(yaml_node_t *start, yaml_node_t *node)
{
	int i;
	int cnt = 0;
	yaml_node_pair_t *p;

	for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
		yaml_node_t *key = &start[p->key - 1];
		yaml_node_t *val = &start[p->value - 1];
		char *key_str = (char *)key->data.scalar.value;
		char *val_str = (char *)val->data.scalar.value;

		if (key->type != YAML_SCALAR_NODE) {
			log_err("failed to parse sampler");
			return -EINVAL;
		}

		if (!strcmp(key_str, "port")) {
			if (val->type != YAML_SCALAR_NODE) {
				log_err("failed to parse sampler");
				return -EINVAL;
			}
			sampler_port = strtol(val_str, NULL, 10);
			log_debug("sampler: port=%d", sampler_port);
		} else if (!strcmp(key_str, "addr")) {
			yaml_node_item_t *item;

			if (val->type != YAML_SEQUENCE_NODE) {
				log_err("failed to parse sampler");
				return -EINVAL;
			}

			for (item = val->data.sequence.items.start; item != val->data.sequence.items.top; item++) {
				if (cnt > NODE_MAX) {
					log_err("too many nodes");
					return -EINVAL;
				}
				strncpy(nodes[cnt], (char *)start[*item - 1].data.scalar.value, IPADDR_SIZE - 1);
				log_debug("sampler: address=%s", nodes[cnt]);
				cnt++;
			}

			if (!cnt) {
				log_err("failed to parse sampler");
				return -EINVAL;
			}
		}
	}

	nr_nodes = cnt;
	majority = (cnt + 1) / 2;

	for (i = 0; i < nr_nodes; i++)
		node_mask[i] = 1 << i;

	available_nodes = (bitmap_t)((1 << nr_nodes) - 1);

	if ((8 % NBIT) != 0) {
		log_err("NBIT is invalidate");
		return -EINVAL;
	}

	vector_size = ((nr_nodes - 1) * NBIT + 7) / 8;
	return 0;
}


int parse_host(yaml_node_t *start, yaml_node_t *node)
{
	bool match = false;
	yaml_node_pair_t *p;

	for (p = node->data.mapping.pairs.start; p < node->data.mapping.pairs.top; p++) {
		int ret;
		yaml_node_t *key = &start[p->key - 1];
		yaml_node_t *val = &start[p->value - 1];
		char *key_str = (char *)key->data.scalar.value;

		if (!strcmp(key_str, "iface")) {
			strcpy(iface, (char *)val->data.scalar.value);
			log_debug("iface: %s", iface);
			match = true;
			break;
		}
	}

	if (!match)
		return -ENOENT;

	return 0;
}


int parse()
{
	FILE *fp;
	int ret = 0;
	yaml_node_t *start;
	yaml_document_t doc;
	yaml_node_pair_t *p;
	yaml_parser_t parser;

	fp = fopen(PATH_CONF, "rb");
	if (!fp) {
		log_err("cannot find %s", PATH_CONF);
		return -1;
	}
	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, fp);
	if (!yaml_parser_load(&parser, &doc)) {
		log_err("failed to parse %s", PATH_CONF);
		ret = -1;
		goto out;
	}
	start = doc.nodes.start;
	if (start->type != YAML_MAPPING_NODE) {
		log_err("faind to load %s", PATH_CONF);
		ret = -1;
		goto out;
	}
	assert((32 % NBIT) == 0);
	for (p = start->data.mapping.pairs.start; p < start->data.mapping.pairs.top; p++) {
		int ret;
		yaml_node_t *key = &start[p->key - 1];
		yaml_node_t *val = &start[p->value - 1];
		char *key_str = (char *)key->data.scalar.value;

		if (!strcmp(key_str, "host"))
			ret = parse_host(start, val);
		else if (!strcmp(key_str, "client"))
			ret = parse_port(start, val, &client_port, "client");
		else if (!strcmp(key_str, "notifier"))
			ret = parse_port(start, val, &notifier_port, "notifier");
		else if (!strcmp(key_str, "replayer"))
			ret = parse_port(start, val, &replayer_port, "replayer");
		else if (!strcmp(key_str, "collector"))
			ret = parse_port(start, val, &collector_port, "collector");
		else if (!strcmp(key_str, "heartbeat"))
			ret = parse_port(start, val, &heartbeat_port, "heartbeat");
		else if (!strcmp(key_str, "synthesizer"))
			ret = parse_port(start, val, &synthesizer_port, "synthesizer");
		else if (!strcmp(key_str, "sampler"))
			ret = parse_sampler(start, val);

		if (ret)
			break;
	}

	if (!ret) {
		int i;

		for (i = 0; i < nr_nodes; i++) {
			if (!strcmp(nodes[i], inet_ntoa(ifaddr()))) {
				log_debug("sampler: id=%d", i);
				node_id = i;
				break;
			}
		}
	}
out:
	yaml_parser_delete(&parser);
	fclose(fp);
	return ret;
}
