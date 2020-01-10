/*
 * libtopology - a library for discovering Linux system topology
 *
 * Copyright 2008 Nathan Lynch, IBM Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#define _GNU_SOURCE
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <topology.h>
#include "compat.h"

static void bail(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static unsigned int cpuset_to_uint(const cpu_set_t *cpumask, size_t size, int index)
{
	int i;
	unsigned int ret = 0;
	int nbits = sizeof(unsigned int) * 8;

	for (i = 0; i < nbits; i++) {
		if (CPU_ISSET_S(i + (index * nbits), size, cpumask))
			ret += 1 << i;
	}

	return ret;
}

static size_t chars_per_byte = 2;
static size_t bytes_per_word = sizeof(uint32_t);

static void format_cpuset(char *buf, const cpu_set_t *cpumask, size_t cpuset_size, int commas)
{
	size_t nwords = 1;
	uint32_t word;
	int nonzero_seen = 0;

	if (cpuset_size > bytes_per_word)
		nwords = cpuset_size / bytes_per_word;

	while (nwords--) {
		word = cpuset_to_uint(cpumask, cpuset_size, nwords);
		if (!word && !nonzero_seen)
			continue;

		/* first non-zero word */
		if (word && !nonzero_seen) {
			nonzero_seen = 1;
			buf += sprintf(buf, "%x", word);
		} else {
			buf += sprintf(buf, "%08x", word);
		}
		if (nwords && commas)
			buf += sprintf(buf, ",");
	}

	if (!nonzero_seen)
		sprintf(buf, "0");
	return;
}

/* size of a string needed to represent a cpu_set_t of the given size */
static size_t cpuset_size_to_bufsize(size_t cpuset_size)
{
	size_t ncommas = 0;

	/* A comma between every 32-bit word, if more than one word */
	if (cpuset_size > bytes_per_word)
		ncommas = cpuset_size / bytes_per_word - 1;

	return cpuset_size * chars_per_byte + ncommas + 1; /* include nul */
}

static void do_procents(topo_procent_t sys, size_t cpumask_size,
			topo_level_t level, int commas)
{
	topo_procent_t ent = (topo_procent_t)0;
	cpu_set_t *cpumask;
	char *buf;

	cpumask = malloc(cpumask_size);
	buf = malloc(cpuset_size_to_bufsize(cpumask_size));

	if (!cpumask || !buf)
		bail("malloc failure");

	while ((ent = topology_traverse(sys, ent, level))) {
		topology_procent_cpumask(ent, cpumask);
		format_cpuset(buf, cpumask, cpumask_size, commas);
		printf("%s\n", buf);
	}

	free(cpumask);
	free(buf);
}

static void usage(int rc)
{

	fprintf(stdout, "Usage:\n");
	fprintf(stdout, "    cpumasks -n\n");
	fprintf(stdout, "        Get the CPU mask for each available NUMA node on the system, one per line.\n\n");

	fprintf(stdout, "    cpumasks -p\n");
	fprintf(stdout, "        Get the CPU mask for each available package on the system, one per line.\n\n");

	fprintf(stdout, "    cpumasks -c\n");
	fprintf(stdout, "        Get the mask for each available core on the system, one per line.\n\n");

	fprintf(stdout, "    cpumasks -t\n");
	fprintf(stdout, "        Get the mask for each available thread on the system, one per line.\n\n");

	fprintf(stdout, "The output of each option is formatted\n");
	fprintf(stdout, "such that it's compatible with the taskset command so that you can do\n");

	fprintf(stdout, "things like:\n\n");

	fprintf(stdout, "    for m in $(cpumasks -c) ; do taskset $m $my_hpc_job ; done\n");

	exit(rc);
}

int main(int argc, char **argv)
{
	topo_context_t nctx;
	topo_procent_t sys;
	size_t cpuset_size;
	int opt;
	int commas = 0;

	if (topology_init_context(&nctx, &sys))
		bail("could not get topology context");

	if (argc != 2)
		usage(1);

	if (strlen(argv[1]) != 2) /* support only one option at a time */
		usage(1);

	cpuset_size = topology_sizeof_cpumask(nctx);

	while ((opt = getopt(argc, argv, "npcth")) != -1) {
		switch (opt) {
		case 'n':
			do_procents(sys, cpuset_size, TOPOLOGY_NODE, commas);
			break;
		case 'p':
			do_procents(sys, cpuset_size, TOPOLOGY_PACKAGE, commas);
			break;
		case 'c':
			do_procents(sys, cpuset_size, TOPOLOGY_CORE, commas);
			break;
		case 't':
			do_procents(sys, cpuset_size, TOPOLOGY_THREAD, commas);
			break;
		case 'h':
			usage(0);
			break;
		default:
			usage(1);
			break;
		}
	}

	topology_free_context(nctx);

	return 0;
}
