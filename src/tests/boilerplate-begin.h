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

#include <stdlib.h>
#include <stdio.h>
#include <topology.h>
#include "compat.h"

#ifndef NULL_CTX_OK
#define NULL_CTX_OK 0
#endif

size_t count_ents_at_level(topo_procent_t top, topo_level_t level)
{
	topo_procent_t ent = (topo_procent_t)0;
	size_t count = 0;

	while ((ent = topology_traverse(top, ent, level)))
		count++;

	return count;
}


static void __fail_on(int pred, int lineno)
{
	if (!pred)
		return;

	printf("failed! (line %d)\n", lineno);
	exit(0);
}

#define fail_on(pred) \
	__fail_on((pred), __LINE__);

int main(int argc, char **argv)
{
	static topo_context_t ctx;

	topo_procent_t sys;
	int rc;

	printf("%-40s%s", NAME, ": ");

	rc = topology_init_context(&ctx, &sys);
	if (!NULL_CTX_OK)
		fail_on(rc != 0);

