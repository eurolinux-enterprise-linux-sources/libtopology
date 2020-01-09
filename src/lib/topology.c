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

#define _GNU_SOURCE /* for sched_getaffinity */
#include <assert.h>
#include <dirent.h>
#include <sched.h>
#include <search.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "topology.h"
#include "helper.h"
#include "compat.h"
#include "cpumask.h"


/***********************************************************************/
/* Hashtable API - just a wrapper around the glibc hashtable functions */
/***********************************************************************/

struct htable {
	size_t count;
	size_t maxsize;
	struct hsearch_data tbl;
};

static int htable_init(struct htable *htab, size_t maxsize)
{
	int rc;

	memset(htab, 0, sizeof(*htab));

	rc = hcreate_r(maxsize, &htab->tbl);
	if (rc == 0)
		return 1;

	htab->maxsize = maxsize;
	return 0;
}

static bool htable_full(struct htable *htab)
{
	return htab->maxsize == htab->count;
}

static int htable_insert(struct htable *htab, const char *key, void *data)
{
	ENTRY entry;
	ENTRY *ret;
	int rc;

	assert(htab->count < htab->maxsize);

	entry.key = (char *)key;
	entry.data = data;

	rc = hsearch_r(entry, ENTER, &ret, &htab->tbl);
	if (rc == 0)
		goto err;

	htab->count++;
	return 0;
err:
	return 1;
}

static void *htable_lookup(struct htable *htab, const char *key)
{
	ENTRY entry;
	ENTRY *ret;

	if (!key)
		return NULL;

	entry.key = (char *)key;
	entry.data = NULL;

	hsearch_r(entry, FIND, &ret, &htab->tbl);
	if (!ret)
		goto err;

	return ret->data;
err:
	return NULL;
}

static void htable_release(struct htable *htab)
{
	if (htab->maxsize == 0)
		return;
	hdestroy_r(&htab->tbl);
	memset(htab, 0, sizeof(*htab));
}


/*********************************/
/* core topology data structures */
/*********************************/

/* attribute structure - key/value pair */
struct attr {
	char *name;
	char *value;
	struct attr *next;
};

/* device abstraction */
struct topo_device {
	char *type;
	char *hash_key;
	cpu_set_t *cpumask;
	struct topo_device *next; /* global list of all topo_devices */
	struct attr *attrs;
	struct topo_context *context;
};

/* representation of processor-related entities: nodes, sockets,
 * cores, threads.
 */
struct topo_procent {
	int level;
	int id;
	char *hash_key; /* for cores and packages */
	struct topo_context *context;
	struct topo_procent *parent;
	struct topo_procent *children;
	struct topo_procent *sibling; /* siblings under parent */
	struct topo_procent *next; /* global list of all topo_procents */
	cpu_set_t *cpumask;
	unsigned long long memory;
};

/* topology "context" - this is the handle used by clients to interrogate
 * the system topology.  Between initialization and release this structure
 * and everything reachable from it remains unchanged.
 */
struct topo_context {
	const char *sysfs_root;
	size_t cpu_set_t_size; /* for use with glibc accessors that
				* take a setsize, e.g. CPU_SET_S() */
	struct topo_procent *list; /* global list of entities */
	struct topo_procent *system; /* system entity */
	struct htable core_htable;
	struct htable package_htable; /* temporary hash tables for
				       * enumerating cores and
				       * packages during
				       * initialization */
	struct topo_device *devices;
	struct htable device_htable;
};


/**************************************/
/* traversal of processor entity tree */
/**************************************/

static bool level_valid(enum topology_level l)
{
	switch (l) {
	case TOPOLOGY_THREAD ... TOPOLOGY_SYSTEM:
		return true;
		break;
	default:
		return false;
	}
}

/* is ent a descendant of from? */
static bool
is_descendant(const struct topo_procent *from,
	      const struct topo_procent *ent)
{
	assert(from != NULL);
	assert(ent != NULL);

	if (!ent->parent)
		return false;

	if (ent->parent == from)
		return true;

	return is_descendant(from, ent->parent);
}

static struct topo_procent *
next_at_level(const struct topo_procent *parent,
	      struct topo_procent *iter,
	      enum topology_level to)
{
	struct topo_procent *ent = iter;

	if (!ent)
		ent = parent->context->list;
	else
		ent = ent->next;

	while (ent) {
		if (ent->level == to && is_descendant(parent, ent))
			break;
		ent = ent->next;
	}

	return ent;
}

static struct topo_procent *
traverse(const struct topo_procent *from,
	 struct topo_procent *iter,
	 enum topology_level to)
{
	if (!level_valid(to))
		return NULL;
	if (!from)
		return NULL;

	/* this could be supported later but semantics aren't clear */
	if (to == from->level)
		return NULL;

	/* base cases */

	/* parent */
	if (to == from->level + 1)
		return from->parent;

	/* children */
	if (to == from->level - 1) {
		if (iter)
			return iter->sibling;
		return from->children;
	}

	/* recursive cases */

	/* ancestor */
	if (to > from->level)
		return traverse(from->parent, NULL, to);

	/* descendant, e.g. from is a node, to is TOPOLOGY_THREAD */

	/* traverse global list */
	if (to < from->level)
		return next_at_level(from, iter, to);

	/* should never get here */
	assert(false);
	return NULL;
}


/******************************************/
/* traversal/interrogation of device list */
/******************************************/

static struct topo_device *find_device_by_type(const struct topo_context *ctx,
					       struct topo_device *prev,
					       const char *type)
{
	struct topo_device *dev;

	if (!prev)
		dev = ctx->devices;
	else
		dev = prev->next;

	for ( ; dev; dev = dev->next)
		if (!strcmp(dev->type, type))
			return dev;

	return NULL;
}

#define for_each_device_of_type(ctx, dev, type)			\
	for ((dev) = find_device_by_type((ctx), NULL, (type));	\
	     (dev) != NULL;					\
	     (dev) = find_device_by_type((ctx), (dev), (type)))


/************************/
/* attribute management */
/************************/

static struct attr *new_attr(char *name, char *value)
{
	struct attr *attr;

	attr = zmalloc(sizeof(*attr));
	if (!attr)
		goto err;

	attr->name = name;
	attr->value = value;

	return attr;
err:
	return NULL;
}

static void attr_release(struct attr *attr)
{
	free(attr->name);
	free(attr->value);
	free(attr);
}


/*********************/
/* device management */
/*********************/

/* allocate a device but don't add it to the global list */
static struct topo_device *
alloc_topo_device(struct topo_context *ctx, const char *type)
{
	struct topo_device *dev;

	dev = zmalloc(sizeof(*dev));
	if (!dev)
		goto err;

	dev->cpumask = zmalloc(ctx->cpu_set_t_size);
	if (!dev->cpumask)
		goto err;

	dev->type = strdup(type);
	if (!dev->type)
		goto err;

	dev->context = ctx;

	return dev;
err:
	if (dev) {
		free(dev->cpumask);
		free(dev->type);
	}
	free(dev);
	return NULL;
}

static void topo_device_release(struct topo_device *dev)
{
	struct attr *attr;

	free(dev->type);
	free(dev->cpumask);
	free(dev->hash_key);

	attr = dev->attrs;
	while (attr) {
		struct attr *next;

		next = attr->next;
		attr_release(attr);
		attr = next;
	}

	free(dev);
}

static void topo_device_register(struct topo_context *ctx,
				 struct topo_device *dev)
{
	/* Insert in global list */
	dev->next = ctx->devices;
	ctx->devices = dev;
}


static void topo_device_attach_attr(struct topo_device *dev, struct attr *attr)
{
	attr->next = dev->attrs;
	dev->attrs = attr;
}

static struct attr *topo_device_get_attr(const struct topo_device *dev,
					 const char *name)
{
	struct attr *attr;

	for (attr = dev->attrs; attr; attr = attr->next)
		if (!strcmp(attr->name, name))
			return attr;

	return NULL;
}

static const char *topo_device_get_attr_value(const struct topo_device *dev,
					      const char *name)
{
	struct attr *attr;

	attr = topo_device_get_attr(dev, name);
	if (attr)
		return attr->value;

	return NULL;
}

/* Currently unused */
#if 0
static void topo_device_cpumask_set(struct topo_device *dev, int bit)
{
	size_t cpuset_size = dev->context->cpu_set_t_size;

	CPU_SET_S(bit, cpuset_size, dev->cpumask);
}
#endif

static void topo_device_copy_cpumask(const struct topo_device *dev,
				     cpu_set_t *dest)
{
	size_t cpuset_size = dev->context->cpu_set_t_size;

	memcpy(dest, dev->cpumask, cpuset_size);
}


/*****************************************************/
/* processor entity (struct topo_procent) management */
/*****************************************************/

static struct topo_procent *
alloc_init_topo_procent(struct topo_context *ctx, struct topo_procent *parent, int id)
{
	struct topo_procent *ent;
	enum topology_level level = TOPOLOGY_SYSTEM;

	assert(ctx != NULL);

	if (parent) {
		assert(level_valid(parent->level));
		assert(parent->level != TOPOLOGY_THREAD);
		level = parent->level - 1;
	}

	ent = zmalloc(sizeof(*ent));
	if (!ent)
		goto err;

	ent->cpumask = zmalloc(ctx->cpu_set_t_size);
	if (!ent->cpumask)
		goto err;

	ent->context = ctx;
	ent->level = level;
	ent->id = id;

	/* Insert in global list */
	ent->next = ctx->list;
	ctx->list = ent;

	/* Welcome to the family */
	ent->parent = parent;
	if (parent) {
		ent->sibling = parent->children;
		parent->children = ent;
	}

	return ent;
err:
	free(ent);
	return NULL;
}

static void topo_procent_release(struct topo_procent *ent)
{
	free(ent->cpumask);
	free(ent->hash_key);
	free(ent);
}

static void topo_procent_cpumask_set(struct topo_procent *ent, int bit)
{
	size_t cpuset_size = ent->context->cpu_set_t_size;

	CPU_SET_S(bit, cpuset_size, ent->cpumask);

	if (ent->parent)
		topo_procent_cpumask_set(ent->parent, bit);
}

static void topo_procent_copy_cpumask(const struct topo_procent *ent,
				      cpu_set_t *dest)
{
	size_t cpuset_size = ent->context->cpu_set_t_size;

	memcpy(dest, ent->cpumask, cpuset_size);
}


/*********************************************************************/
/* Code for initialization of context and associated data structures */
/*********************************************************************/

/* device should not be in global list yet */
static int hash_device(struct topo_context *ctx, struct topo_device *dev)
{
	struct topo_device *iter;
	size_t newsize;

	if (!htable_full(&ctx->device_htable))
		return htable_insert(&ctx->device_htable, dev->hash_key, dev);

	/* double the hashtable size */
	newsize = ctx->device_htable.maxsize * 2;
	htable_release(&ctx->device_htable);
	if (htable_init(&ctx->device_htable, newsize))
		return 1;

	/* re-hash every device */
	iter = ctx->devices;
	while (iter) {
		if (htable_insert(&ctx->device_htable, iter->hash_key, iter))
			return 1;
		iter = iter->next;
	}

	return htable_insert(&ctx->device_htable, dev->hash_key, dev);
}

/* Return true if:
 * - /sys/devices/system/cpu/cpu$cpu/online exists and has value 1, or
 * - /sys/devices/system/cpu/cpu$cpu/online does not exist (e.g. x86 boot cpu,
 *   or kernel does not support cpu hotplug)
 * Return false if /sys/devices/system/cpu/cpu$cpu/online has value 0
 */
static int sysfs_cpu_is_online(const char *sysfs, unsigned int cpu)
{
	char path[PATH_MAX];
	int online = 1;
	char *buf;

	sprintf(path, "%s/devices/system/cpu/cpu%d/online", sysfs, cpu);

	buf = slurp_text_file(path);
	if (!buf) /* assume online on error */
		goto out;

	if (sscanf(buf, "%d", &online) != 1)
		online = 1;
out:
	free(buf);
	return online;
}

static size_t sysfs_probe_cpumask_size(const char *sysfs)
{
	DIR *dir;
	struct dirent64 *dirent;
	char path[PATH_MAX];
	int max_cpu_id = 0;

	/* This must not be called on systems that don't support
	 * dynamically sized cpumasks.
	 */
	assert(CPU_ALLOC_SIZE(CPU_SETSIZE + 1) > sizeof(cpu_set_t));

	snprintf(path, sizeof(path), "%s/devices/system/cpu", sysfs);

	dir = opendir(path);
	if (!dir)
		goto err;

	while ((dirent = readdir64(dir))) {
		int cpu_id;

		if (dirent->d_type != DT_DIR && dirent->d_type != DT_UNKNOWN)
			continue;

		if (sscanf(dirent->d_name, "cpu%d", &cpu_id) != 1)
			continue;

		if (cpu_id > max_cpu_id)
			max_cpu_id = cpu_id;
	}

	closedir(dir);
	return CPU_ALLOC_SIZE(max_cpu_id + 1);
err:
	if (dir)
		closedir(dir);
	return 0;
}

static size_t sched_probe_cpumask_size(void)
{
	cpu_set_t *cpus;
	size_t size;
	int rc;

	/* Start with the minimum size supported by glibc */
	size = CPU_ALLOC_SIZE(1);

	cpus = zmalloc(size);
	if (!cpus)
		goto err;

	rc = sched_getaffinity(getpid(), size, cpus);

	/* Assumes we don't get any error besides EINVAL. */
	while (rc) {
		free(cpus);
		size *= 2;
		cpus = zmalloc(size);
		if (!cpus)
			goto err;
		rc = sched_getaffinity(getpid(), size, cpus);
	}

	free(cpus);
	return size;
err:
	free(cpus);
	return 0;
}

/* probe_cpumask_size:
 * - Return sizeof(cpu_set_t) if this system does not support
 *   dynamically sized cpumasks (tests of more than 1024 cpus will
 *   fail or be skipped)
 * Otherwise, return the greater of:
 * - the minimum cpumask size supported by sched_getaffinity if this
 *   system supports dynamically sized cpumasks
 * - the number of CPUs in sysfs if LIBTOPOLOGY_CPUMASK_OVERRIDE is set
 *   (intended for testing only - passing oversize cpumasks to
 *   sched_setaffinity will not work correctly)
 * If the number of CPUs in sysfs is greater than the
 * sched_getaffinity size, and LIBTOPOLOGY_CPUMASK_OVERRIDE is not
 * set, return error (zero).
 */
static size_t probe_cpumask_size(const char *sysfs)
{
	size_t sched_size;
	size_t sysfs_size;
	size_t ret = 0;

	/* Can we do dynamic cpumasks? */
	if (CPU_ALLOC_SIZE(CPU_SETSIZE + 1) == sizeof(cpu_set_t))
		return sizeof(cpu_set_t);

	sched_size = sched_probe_cpumask_size();
	if (sched_size == 0)
		goto err;
	ret = sched_size;

	sysfs_size = sysfs_probe_cpumask_size(sysfs);
	if (sysfs_size > sched_size) {
		if (!getenv("LIBTOPOLOGY_CPUMASK_OVERRIDE"))
			goto err;
		ret = sysfs_size;
	}

	return ret;
err:
	return 0;
}

static int sysfs_count_caches(const char *sysfs, int cpu)
{
	struct dirent64 *dirent;
	char path[PATH_MAX];
	int count = 0;
	DIR *dir;

	snprintf(path, sizeof(path), "%s/devices/system/cpu/cpu%d/cache",
		 sysfs, cpu);

	dir = opendir(path);
	if (!dir)
		goto out;

	while ((dirent = readdir64(dir))) {
		int index;

		if (dirent->d_type != DT_DIR && dirent->d_type != DT_UNKNOWN)
			continue;

		if (sscanf(dirent->d_name, "index%d", &index) != 1)
			continue;

		count++;
	}

	closedir(dir);
out:
	return count;
}

static void buf_remove_newline(char *buf)
{
	char *newline;

	newline = strchr(buf, '\n');

	if (newline)
		*newline = '\0';
}

static char *sysfs_get_attr_value(int dirfd, const char *attr_path)
{
	char *line = NULL;
	FILE *file = NULL;
	size_t size;
	int fd;
	int rc;

	fd = openat(dirfd, attr_path, O_CLOEXEC | O_RDONLY);
	if (fd == -1)
		goto err;

	fcntl(fd, F_SETFD, FD_CLOEXEC);

	file = fdopen(fd, "r");
	if (!file)
		goto err;

	rc = getline(&line, &size, file);
	if (rc == -1)
		line = NULL;
	else
		buf_remove_newline(line);
err:
	if (file)
		fclose(file);
	else if (fd != -1)
		close(fd);
	return line;
}

static struct attr *sysfs_get_attr(int dfd, const char *name)
{
	struct attr *attr;
	char *value = NULL;
	char *newname;

	newname = strdup(name);
	if (!name)
		goto err;

	value = sysfs_get_attr_value(dfd, newname);
	if (!value)
		goto err;

	attr = new_attr(newname, value);
	if (!attr)
		goto err;

	return attr;
err:
	free(newname);
	if (value)
		free(value);
	return NULL;
}

static int __get_cache_info(struct topo_device *cache, int cpu, int index)
{
	size_t cpumask_size;
	char path[PATH_MAX];
	const char *sysfs;
	struct attr *attr;
	DIR *dir;
	int fd;

	cpumask_size = cache->context->cpu_set_t_size;

	sysfs = cache->context->sysfs_root;

	snprintf(path, sizeof(path),
		 "%s/devices/system/cpu/cpu%d/cache/index%d",
		 sysfs, cpu, index);

	dir = opendir(path);
	if (!dir)
		goto err;

	fd = dirfd(dir);
	if (fd == -1)
		goto err;

	attr = sysfs_get_attr(fd, "size");
	if (!attr)
		goto err;

	topo_device_attach_attr(cache, attr);

	attr = sysfs_get_attr(fd, "type");
	if (!attr)
		goto err;

	topo_device_attach_attr(cache, attr);

	attr = sysfs_get_attr(fd, "level");
	if (!attr)
		goto err;

	topo_device_attach_attr(cache, attr);

	attr = sysfs_get_attr(fd, "shared_cpu_map");
	if (!attr)
		goto err;

	topo_device_attach_attr(cache, attr);

	if (topology_cpumask_parse(cache->cpumask, cpumask_size, attr->value))
		goto err;

	/* Ensure this cpu is set in the shared map */
	if (!CPU_ISSET_S(cpu, cpumask_size, cache->cpumask))
		goto err;

	closedir(dir);

	return 0;
err:
	if (dir)
		closedir(dir);
	return 1;
}

static int cache_build_hash(struct topo_device *cache)
{
	const char *level;
	const char *type;
	const char *cpumask;
	int rc;

	level = topo_device_get_attr_value(cache, "level");
	type = topo_device_get_attr_value(cache, "type");
	cpumask = topo_device_get_attr_value(cache, "shared_cpu_map");

	if (!level || !type || !cpumask)
		goto err;

	rc = asprintf(&cache->hash_key, "cache-L%s-%s-%s",
		      level, type, cpumask);
	if (rc == -1)
		cache->hash_key = NULL;

	if (!cache->hash_key)
		goto err;

	return 0;
err:
	return 1;
}

static int get_cache_info(struct topo_procent *thread)
{
	struct topo_context *ctx;
	struct topo_device *cache;
	int nr_caches;
	int index;

	ctx = thread->context;

	nr_caches = sysfs_count_caches(ctx->sysfs_root, thread->id);
	if (nr_caches == 0)
		return 0;

	/* Need to keep track of which caches have been seen.  We can
	 * uniquely identify a cache by its level, type, and
	 * shared_cpu_map, so hash that combination.
	 */

	for (index = 0; index < nr_caches; index++) {
		cache = alloc_topo_device(ctx, "cache");
		if (!cache)
			goto err;
		if (__get_cache_info(cache, thread->id, index))
			goto err;
		if (cache_build_hash(cache))
			goto err;

		/* If we've seen this cache already, release it */
		if (htable_lookup(&ctx->device_htable, cache->hash_key)) {
			topo_device_release(cache);
			continue;
		}

		if (hash_device(ctx, cache))
			goto err;

		topo_device_register(ctx, cache);
	}

	return 0;

err:
	topo_device_release(cache);
	return 1;
}

static char *sysfs_cpu_thread_siblings(const char *sysfs, int cpu_id)
{
	char path[PATH_MAX];
	char *buf;
	int rc;

	snprintf(path, sizeof(path),
		 "%s/devices/system/cpu/cpu%d/topology/thread_siblings",
		 sysfs, cpu_id);

	buf = slurp_text_file(path);
	if (buf)
		return buf;

	rc = asprintf(&buf, "%d", cpu_id);
	if (rc == -1)
		buf = NULL;

	return buf;
}

static char *sysfs_cpu_core_siblings(const char *sysfs, int cpu_id)
{
	char path[PATH_MAX];
	char *buf;

	snprintf(path, sizeof(path),
		 "%s/devices/system/cpu/cpu%d/topology/core_siblings",
		 sysfs, cpu_id);

	buf = slurp_text_file(path);
	if (buf)
		return buf;

	/* thread_siblings must be a subset of core_siblings so assume
	 * one core per package */
	return sysfs_cpu_thread_siblings(sysfs, cpu_id);
}

static int do_one_cpu(struct topo_procent *node, int cpu_id)
{
	struct topo_context *ctx;
	struct topo_procent *pkg;
	struct topo_procent *core;
	struct topo_procent *thread;
	char *siblings;

	ctx = node->context;

	/* package */
	siblings = sysfs_cpu_core_siblings(ctx->sysfs_root, cpu_id);
	if (!siblings)
		goto err;

	pkg = htable_lookup(&ctx->package_htable, siblings);
	if (!pkg) {
		int rc;

		pkg = alloc_init_topo_procent(ctx, node, cpu_id);
		if (!pkg)
			goto err;

		pkg->hash_key = strdup(siblings);
		if (!pkg->hash_key)
			goto err;

		rc = htable_insert(&ctx->package_htable, pkg->hash_key, pkg);
		if (rc)
			goto err;
	}

	free(siblings);

	/* core */
	siblings = sysfs_cpu_thread_siblings(ctx->sysfs_root, cpu_id);
	if (!siblings)
		goto err;

	core = htable_lookup(&ctx->core_htable, siblings);
	if (!core) {
		int rc;

		core = alloc_init_topo_procent(ctx, pkg, cpu_id);
		if (!core)
			goto err;

		core->hash_key = strdup(siblings);
		if (!core->hash_key)
			goto err;

		rc = htable_insert(&ctx->core_htable, core->hash_key, core);
		if (rc)
			goto err;
	}

	free(siblings);
	siblings = NULL;

	thread = alloc_init_topo_procent(ctx, core, cpu_id);
	if (!thread)
		goto err;

	topo_procent_cpumask_set(thread, cpu_id);

	/* Collecting cache info is best-effort */
	get_cache_info(thread);

	return 0;
err:
	free(siblings);
	return 1;
}

static int do_node_cpus(struct topo_procent *node)
{
	struct topo_context *ctx;
	struct dirent64 *dirent;
	char path[PATH_MAX];
	DIR *dir;

	ctx = node->context;

	snprintf(path, sizeof(path), "%s/devices/system/node/node%d",
		 ctx->sysfs_root, node->id);

	/* If we're "faking" node 0, use the cpu sysfs hierarchy */
	dir = opendir(path);
	if (!dir && node->id != 0) {
		goto err;
	} else if (!dir) {
		snprintf(path, sizeof(path), "%s/devices/system/cpu",
			 ctx->sysfs_root);
		dir = opendir(path);
		if (!dir)
			goto err;
	}

	while ((dirent = readdir64(dir))) {
		int cpu_id;

		if (sscanf(dirent->d_name, "cpu%d", &cpu_id) != 1)
			continue;

		if (!sysfs_cpu_is_online(ctx->sysfs_root, cpu_id))
			continue;

		if (do_one_cpu(node, cpu_id))
			goto err;
	}

	closedir(dir);
	return 0;
err:
	if (dir)
		closedir(dir);
	return 1;
}

static int do_one_node(struct topo_procent *sys, int nid)
{
	struct topo_procent *node;

	node = alloc_init_topo_procent(sys->context, sys, nid);
	if (!node)
		goto err;

	if (do_node_cpus(node))
		goto err;

	return 0;
err:
	return 1;
}

static int build_context(struct topo_context *ctx)
{
	struct topo_procent *system;
	struct dirent64 *dirent;
	char path[PATH_MAX];
	DIR *dir = NULL;

	system = alloc_init_topo_procent(ctx, NULL, 0);
	if (!system)
		goto err;

	ctx->system = system;

	snprintf(path, sizeof(path), "%s/devices/system/node", ctx->sysfs_root);
	dir = opendir(path);
	if (!dir) /* Non-numa system, treat as single node */
		return do_one_node(system, 0);

	while ((dirent = readdir64(dir))) {
		int node_id;

		if (dirent->d_type != DT_DIR && dirent->d_type != DT_UNKNOWN)
			continue;

		if (sscanf(dirent->d_name, "node%d", &node_id) != 1)
			continue;

		if (do_one_node(system, node_id))
			goto err;
	}

	closedir(dir);
	return 0;

err:
	if (dir)
		closedir(dir);
	return 1;
}

static void context_release(struct topo_context *ctx)
{
	struct topo_procent *ent;
	struct topo_device *dev;

	if (!ctx)
		return;

	ent = ctx->list;
	while (ent) {
		struct topo_procent *next;
		next = ent->next;
		topo_procent_release(ent);
		ent = next;
	}

	dev = ctx->devices;
	while (dev) {
		struct topo_device *next;
		next = dev->next;
		topo_device_release(dev);
		dev = next;
	}

	htable_release(&ctx->package_htable);
	htable_release(&ctx->core_htable);
	htable_release(&ctx->device_htable);
	free(ctx);
}

static struct topo_context *new_context(void)
{
	struct topo_context *ctx;

	ctx = zmalloc(sizeof(*ctx));
	if (!ctx)
		goto err;

	ctx->sysfs_root = getenv("LIBTOPOLOGY_SYSFS_ROOT");
	if (!ctx->sysfs_root)
		ctx->sysfs_root = "/sys";

	ctx->cpu_set_t_size = probe_cpumask_size(ctx->sysfs_root);
	if (ctx->cpu_set_t_size == 0)
		goto err;

	if (htable_init(&ctx->package_htable, ctx->cpu_set_t_size * 8))
		goto err;
	if (htable_init(&ctx->core_htable, ctx->cpu_set_t_size * 8))
		goto err;

	/* The device hash table will grow if it gets full */
	if (htable_init(&ctx->device_htable, 2))
		goto err;


	if (build_context(ctx))
		goto err;

	return ctx;

err:
	context_release(ctx);
	return NULL;
}


/**********************************************************************/
/* Utility functions for converting between user-visible typedefs and */
/* internal types                                                     */
/**********************************************************************/

static struct topo_context *context_extract(topo_context_t ctx_t)
{
	return (struct topo_context *)ctx_t;
}

static topo_context_t context_wrap(struct topo_context *ctx)
{
	return (topo_context_t)ctx;
}

static struct topo_device *device_extract(topo_device_t dev_t)
{
	return (struct topo_device *)dev_t;
}

static topo_device_t device_wrap(struct topo_device *dev)
{
	return (topo_device_t)dev;
}

static struct topo_procent *procent_extract(topo_procent_t ent_t)
{
	return (struct topo_procent *)ent_t;
}

static topo_procent_t procent_wrap(struct topo_procent *ent)
{
	return (topo_procent_t)ent;
}



/*******************/
/*  Exported APIs  */
/*******************/

int topology_init_context(topo_context_t *ctx_t, topo_procent_t *system_t)
{
	struct topo_context *ctx;

	ctx = new_context();

	if (!ctx)
		return 1;

	*ctx_t = context_wrap(ctx);
	if (system_t)
		*system_t = procent_wrap(ctx->system);

	return 0;
}

void topology_free_context(topo_context_t ctx_t)
{
	context_release(context_extract(ctx_t));
}

size_t topology_sizeof_cpumask(topo_context_t ctx_t)
{
	struct topo_context *ctx = context_extract(ctx_t);

	return ctx->cpu_set_t_size;
}

topo_device_t
topology_find_device_by_type(topo_context_t ctx_t,
			     topo_device_t prev_t,
			     const char *type)
{
	struct topo_context *ctx = context_extract(ctx_t);
	struct topo_device *prev = device_extract(prev_t);

	return device_wrap(find_device_by_type(ctx, prev, type));
}

const char *
topology_device_get_attribute(topo_device_t dev,
			      const char *name)
{
	return topo_device_get_attr_value(device_extract(dev), name);
}

void topology_device_cpumask(topo_device_t dev_t, cpu_set_t *cpumask)
{
	struct topo_device *dev = device_extract(dev_t);

	topo_device_copy_cpumask(dev, cpumask);
}

void topology_procent_cpumask(topo_procent_t ent_t, cpu_set_t *cpumask)
{
	struct topo_procent *ent = procent_extract(ent_t);

	topo_procent_copy_cpumask(ent, cpumask);
}

topo_procent_t topology_traverse(topo_procent_t from,
				 topo_procent_t iter,
				 topo_level_t to)
{
	return procent_wrap(traverse(procent_extract(from),
				     procent_extract(iter), to));
}

