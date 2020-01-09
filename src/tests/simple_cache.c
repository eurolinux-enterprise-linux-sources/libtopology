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

#define NAME "cache size = 16KB"
#define _GNU_SOURCE
#include <sched.h>
#include <string.h>
#include "boilerplate-begin.h"

{
	topo_procent_t ent;
	topo_device_t dev;
	size_t size;
	cpu_set_t *cpumask;
	const char *cachesize;

	size = topology_sizeof_cpumask(ctx);
	cpumask = malloc(size);

	/* we know there is only one thread, one cache */
	ent = topology_traverse(sys, (topo_procent_t)0, TOPOLOGY_THREAD);
	dev = topology_find_device_by_type(ctx, (topo_device_t)0, "cache");
	topology_device_cpumask(dev, cpumask);
	fail_on(!CPU_ISSET_S(0, size, cpumask));
	fail_on(CPU_COUNT_S(size, cpumask) != 1);

	cachesize = topology_device_get_attribute(dev, "size");
	fail_on(!cachesize);
	fail_on(strcmp(cachesize, "16K"));

	free(cpumask);
}
#include "boilerplate-end.h"
