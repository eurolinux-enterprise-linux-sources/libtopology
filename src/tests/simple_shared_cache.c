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

#define NAME "two cores, shared L2"
#define _GNU_SOURCE
#include <sched.h>
#include <string.h>
#include "boilerplate-begin.h"

{
	topo_device_t dev;
	int nr_l1 = 0, nr_l2 = 0, nr_data = 0, nr_insn = 0, nr_unified = 0;
	cpu_set_t *cpumask;
	size_t cpumask_size;

	cpumask_size = topology_sizeof_cpumask(ctx);
	cpumask = malloc(cpumask_size);

	topology_for_each_device_of_type(ctx, dev, "cache") {
		const char *level;
		const char *type;

		level = topology_device_get_attribute(dev, "level");
		type = topology_device_get_attribute(dev, "type");
		topology_device_cpumask(dev, cpumask);

		if (!strcmp(level, "1"))
			nr_l1++;
		if (!strcmp(level, "2")) {
			nr_l2++;
			/* L2 must have both CPUs in cpumask */
			fail_on(!CPU_ISSET_S(0, cpumask_size, cpumask));
			fail_on(!CPU_ISSET_S(1, cpumask_size, cpumask));
		}
		if (!strcmp(type, "Data"))
			nr_data++;
		if (!strcmp(type, "Instruction"))
			nr_insn++;
		if (!strcmp(type, "Unified"))
			nr_unified++;
	}

	free(cpumask);

	fail_on(nr_l1 != 4); /* 2 data, 2 instruction */
	fail_on(nr_l2 != 1);
	fail_on(nr_data != 2);
	fail_on(nr_insn != 2);
	fail_on(nr_unified != 1);
}
#include "boilerplate-end.h"
