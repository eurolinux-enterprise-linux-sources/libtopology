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

#define NAME "traversal test"
#include <string.h>
#include "boilerplate-begin.h"

{
	topo_context_t ctx;
	topo_procent_t sys;
	topo_procent_t node;
	size_t nodes = 0;
	int rc;

	rc = topology_init_context(&ctx, &sys);
	fail_on(rc != 0);

	node = (topo_procent_t)0;
	while ((node = topology_traverse(sys, node, TOPOLOGY_NODE))) {
		topo_procent_t pkg = (topo_procent_t)0;
		size_t pkgs = 0;

		fail_on(count_ents_at_level(node, TOPOLOGY_THREAD) != 8);
		fail_on(count_ents_at_level(node, TOPOLOGY_CORE) != 4);
		fail_on(count_ents_at_level(node, TOPOLOGY_PACKAGE) != 2);

		while ((pkg = topology_traverse(node, pkg, TOPOLOGY_PACKAGE))) {
			topo_procent_t core = (topo_procent_t)0;
			size_t cores = 0;

			fail_on(count_ents_at_level(pkg, TOPOLOGY_THREAD) != 4);
			fail_on(count_ents_at_level(pkg, TOPOLOGY_CORE) != 2);

			while ((core = topology_traverse(pkg, core, TOPOLOGY_CORE))) {
				topo_procent_t thr = (topo_procent_t)0;
				size_t threads = 0;

				fail_on(count_ents_at_level(core, TOPOLOGY_THREAD) != 2);

				while ((thr = topology_traverse(core, thr,
								TOPOLOGY_THREAD))) {
					threads++;
					fail_on(core != topology_traverse(thr, 0,
									  TOPOLOGY_CORE));
					fail_on(pkg != topology_traverse(thr, 0,
									 TOPOLOGY_PACKAGE));
					fail_on(node != topology_traverse(thr, 0,
									  TOPOLOGY_NODE));
				}
				fail_on(threads != 2);
				cores++;
				fail_on(pkg != topology_traverse(core, 0,
								 TOPOLOGY_PACKAGE));
				fail_on(node != topology_traverse(core, 0,
								  TOPOLOGY_NODE));
			}
			fail_on(cores != 2);
			pkgs++;
			fail_on(node != topology_traverse(pkg, 0, TOPOLOGY_NODE));
		}
		fail_on(pkgs != 2);
		nodes++;
	}
	fail_on(nodes != 2);

	topology_free_context(ctx);
}
#include "boilerplate-end.h"
