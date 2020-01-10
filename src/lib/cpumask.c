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

#include <assert.h>
#include <ctype.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "helper.h"
#include "compat.h"

static const unsigned char ascii_vals[] = {
	[48] = 0,
	[49] = 1,
	[50] = 2,
	[51] = 3,
	[52] = 4,
	[53] = 5,
	[54] = 6,
	[55] = 7,
	[56] = 8,
	[57] = 9,

	[65] = 0xA,
	[66] = 0xB,
	[67] = 0xC,
	[68] = 0xD,
	[69] = 0xE,
	[70] = 0xF,

	[97]  = 0xa,
	[98]  = 0xb,
	[99]  = 0xc,
	[100] = 0xd,
	[101] = 0xe,
	[102] = 0xf,
};

static uint8_t char_to_val(int c)
{
	assert(isxdigit(c));
	return ascii_vals[c];
}

int topology_cpumask_parse(cpu_set_t *cpumask, size_t size, const char *buf)
{
	const char *pos;
	int cpu = 0;

	CPU_ZERO_S(size, cpumask);

	pos = strlen(buf) + buf;

	/* find the last hex digit */
	while (!isxdigit(*pos) && pos >= buf)
		pos--;
	if (pos < buf)
		goto err;

	for ( ; pos >= buf; pos--) {
		uint8_t val;
		int i;

		if (*pos == ',')
			continue;

		if (!isxdigit(*pos))
			goto err;

		val = char_to_val(*pos);

		for (i = 0; i < 4; i++, val >>= 1) {
			if (val & 1)
				CPU_SET_S(cpu, size, cpumask);
			cpu++;
		}

		assert(val == 0);
	}

	return 0;
err:
	CPU_ZERO_S(size, cpumask);
	return 1;
}
