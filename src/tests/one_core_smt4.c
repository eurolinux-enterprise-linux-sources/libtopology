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
#include <sys/types.h>
#include <unistd.h>

#define NAME "single core, smt4"
#include "boilerplate-begin.h"
{
	unsigned int nrcores, nrthreads;
	topo_procent_t ent;
	size_t cpuset_size;
	cpu_set_t *cpumask;
	int i;

	cpuset_size = topology_sizeof_cpumask(ctx);
	cpumask = malloc(cpuset_size);

	nrcores = count_ents_at_level(sys, TOPOLOGY_CORE);
	fail_on(nrcores != 1);

	nrthreads = count_ents_at_level(sys, TOPOLOGY_THREAD);
	fail_on(nrthreads != 4);

	i = 0;
	ent = (topo_procent_t)0;
	while ((ent = topology_traverse(sys, ent, TOPOLOGY_THREAD))) {
		topology_procent_cpumask(ent, cpumask);
		fail_on(CPU_COUNT_S(cpuset_size, cpumask) != 1);
		/* fixme: get linux cpu id, test cpumask for it */
		i++;
	}
	fail_on(i != nrthreads);

	i = 0;
	ent = (topo_procent_t)0;
	while ((ent = topology_traverse(sys, ent, TOPOLOGY_CORE))) {
		int j;
		i++;
		topology_procent_cpumask(ent, cpumask);
		fail_on(CPU_COUNT_S(cpuset_size, cpumask) != 4);
		for (j = 0; j < 4; j++)
			fail_on(!CPU_ISSET_S(j, cpuset_size, cpumask));
	}
	fail_on(i != 1);


	i = 0;
	ent = (topo_procent_t)0;
	while ((ent = topology_traverse(sys, ent, TOPOLOGY_PACKAGE))) {
		int j;
		i++;
		topology_procent_cpumask(ent, cpumask);
		fail_on(CPU_COUNT_S(cpuset_size, cpumask) != 4);
		for (j = 0; j < 4; j++)
			fail_on(!CPU_ISSET_S(j, cpuset_size, cpumask));
	}
	fail_on(i != 1);


	free(cpumask);
}
#include "boilerplate-end.h"
