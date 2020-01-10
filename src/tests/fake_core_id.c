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

#define NAME "fake core id"
#include "boilerplate-begin.h"
{
	fail_on(count_ents_at_level(sys, TOPOLOGY_NODE) != 2);
	fail_on(count_ents_at_level(sys, TOPOLOGY_PACKAGE) != 2);
	fail_on(count_ents_at_level(sys, TOPOLOGY_CORE) != 2);
	fail_on(count_ents_at_level(sys, TOPOLOGY_THREAD) != 2);
}
#include "boilerplate-end.h"
