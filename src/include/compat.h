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

#ifndef _TOPOLOGY_COMPAT_H_
#define _TOPOLOGY_COMPAT_H_

#define _GNU_SOURCE
#include <sched.h>

/* compatibility for glibc older than 2.7 */
#ifndef CPU_ALLOC_SIZE
#define CPU_ALLOC_SIZE(nrbits) \
	({ (void)(nrbits); sizeof(cpu_set_t); })
#endif

#ifndef CPU_ZERO_S
#define CPU_ZERO_S(size, cpusetp) \
	({ (void)(size); CPU_ZERO(cpusetp); })
#endif

#ifndef CPU_SET_S
#define CPU_SET_S(cpu, size, cpusetp) \
	({ (void)(size); CPU_SET(cpu, cpusetp); })
#endif

#ifndef CPU_ISSET_S
#define CPU_ISSET_S(cpu, size, cpusetp) \
	({ (void)(size); CPU_ISSET(cpu, cpusetp); })
#endif

#ifndef CPU_COUNT_S
#define CPU_COUNT_S(size, cpusetp) \
	({ \
		int i, count = 0; \
		(void)(size); \
		for (i = 0; i < CPU_SETSIZE; i++) \
			if (CPU_ISSET(i, (cpusetp))) \
				count++; \
		count; \
	})
#endif

#include <fcntl.h>
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#endif
