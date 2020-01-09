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

#define NAME "caches missing shared_cpu_map"
#include <string.h>
#include "boilerplate-begin.h"

{
	topo_device_t dev;

	topology_for_each_device_of_type(ctx, dev, "cache") {
		fail_on(1); /* there should be no caches! */
	}
}
#include "boilerplate-end.h"
