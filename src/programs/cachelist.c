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

#define DO_SELFTEST 1

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

int main(int argc, char **argv)
{
	topo_context_t ctx;
	topo_procent_t sys;
	topo_device_t cache;
	size_t cpumask_size;
	cpu_set_t *cpumask;
	char *buf;

	if (topology_init_context(&ctx, &sys))
		bail("could not get topology context");

	cpumask_size = topology_sizeof_cpumask(ctx);
	cpumask = malloc(cpumask_size);
	if (!cpumask)
		bail("could not allocate cpumask");

	buf = malloc(cpuset_size_to_bufsize(cpumask_size));
	if (!buf)
		bail("could not allocate cpumask format buffer");

	topology_for_each_device_of_type(ctx, cache, "cache") {
		const char *level;
		const char *size;
		const char *type;

		level = topology_device_get_attribute(cache, "level");
		size = topology_device_get_attribute(cache, "size");
		type = topology_device_get_attribute(cache, "type");
		printf("cache : level = %s, type = %s, size = %s\n",
		       level, type, size);
		topology_device_cpumask(cache, cpumask);
		format_cpuset(buf, cpumask, cpumask_size, 0);
		printf("        cpus = 0x%s\n", buf);
	}

	free(buf);
	free(cpumask);
	topology_free_context(ctx);

	return 0;
}
