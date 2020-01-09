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

#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "compat.h"

static long pagesize(void)
{
	long size;

	size = sysconf(_SC_PAGESIZE);
	if (size == -1)
		size = 4096;

	/* It's useful to return 1 here to find overruns etc. */
	return size;
}

void *zmalloc(size_t size)
{
	return calloc(1, size);
}

/* caller is responsible for freeing returned buffer */
/* caller is advised to supply absolute paths */
void *slurp_file(const char *path, size_t *length)
{
	size_t ask;
	size_t bufsize;
	ssize_t total;
	ssize_t slurped;
	char *buf;
	int fd = -1;

	bufsize = pagesize();

	buf = zmalloc(bufsize);
	if (!buf)
		goto err;

	fd = open(path, O_CLOEXEC | O_RDONLY);
	if (fd == -1)
		goto err;

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	ask = bufsize;
	total = 0;
	for (;;) {
		assert(ask > 0);
		errno = 0;
		slurped = read(fd, &buf[total], ask);
		if (slurped < 0 && errno == EINTR)
			continue;
		else if (slurped < 0) /* error */
			goto err;
		else if (slurped == 0) /* EOF */
			break;

		/* We know we did read some data */
		total += slurped;
		assert(total <= bufsize);

		/* Short read, ensure the next one doesn't overrun buf */
		if (slurped < ask) {
			ask -= slurped;
			continue;
		}

		if (total < bufsize)
			continue;

		/* At the end of the buffer, but there's more to read */
		assert(total == bufsize);

		/* No point trying to gracefully handle realloc fail */
		buf = realloc(buf, bufsize + pagesize());
		if (!buf)
			goto err;
		bufsize += pagesize();
		ask = pagesize();
	}

	assert(total <= bufsize);
	close(fd);
	if (length)
		*length = total;
	return buf;
err:
	if (fd != -1) /* valgrind complains on close(-1) */
		close(fd);
	free(buf);
	return NULL;
}

/* Slurp a file and nul-terminate the buffer */
void *slurp_text_file(const char *path)
{
	size_t len;
	char *buf, *newbuf;

	buf = slurp_file(path, &len);
	if (!buf)
		return NULL;

	newbuf = realloc(buf, len + 1);
	if (newbuf)
		newbuf[len] = '\0';
	else
		free(buf);

	return newbuf;
}
