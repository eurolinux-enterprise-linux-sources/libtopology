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

/** \file topology.h
 * \brief libtopology - a library for discovering Linux system topology
 *
 * The libtopology API allows one to formulate scheduling and NUMA
 * policies in terms that are abstract and portable.
 */
#ifndef _TOPOLOGY_H_
#define _TOPOLOGY_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBTOPOLOGY_DOXYGEN_SKIP

#include <sched.h> /* for cpu_set_t */

#endif /* LIBTOPOLOGY_DOXYGEN_SKIP */

typedef unsigned long topo_context_t;
typedef unsigned long topo_procent_t;
typedef unsigned long topo_device_t;

typedef enum topology_level {
	TOPOLOGY_THREAD = 1,
	TOPOLOGY_CORE,
	TOPOLOGY_PACKAGE,
	TOPOLOGY_NODE,
	TOPOLOGY_SYSTEM,
} topo_level_t;

/**
 * topology_init_context() - Allocate a topology context
 *
 * @param ctx - address of topo_context_t
 * @param system - address of system topo_procent_t
 *
 * @return 0 on success, non-zero on failure.
 *
 * On success ctx will contain a valid handle for a topology context
 * which can be used with other topology APIs.  If the system pointer
 * is non-NULL, on success it contains a handle corresponding to the
 * top-level system object, from which all other topo_procent_t
 * objects in the system (nodes, packages, cores, and threads) can be
 * reached.
 *
 * On failure the contents of ctx and system are unspecified.
 */
extern int topology_init_context(topo_context_t *ctx, topo_procent_t *system);

/**
 * topology_free_context() - Release a topology context
 *
 * @param ctx - context to release
 *
 * Releases the resources associated with a valid topology context
 * provided by topology_init_context().  After a call to
 * topology_free_context(), ctx is considered invalid and must not be
 * passed to other topology APIs.  This function should be called only
 * after all other operations on this context or its derived objects
 * have completed.  In other words, topology_free_context() should not
 * be called until the user is done making libtopology calls.
 */
extern void topology_free_context(topo_context_t ctx);

/**
 * topology_sizeof_cpumask() - get the size of cpumasks used by
 *                             libtopology
 *
 * @param ctx - a valid context returned from topology_init_context()
 *
 * @return The size in bytes of cpumask objects used by libtopology
 * for this context.
 *
 * During context initialization, libtopology may dynamically
 * determine the size of the cpumask data structure (a bitfield
 * representing a set of logical CPU ids), which is used to indicate
 * the set of logical CPUs with which an object is associated.  The
 * value returned by this function is the size which must be used to
 * allocate cpu_set_t objects which are passed to the libtopology API
 * (e.g. topology_device_cpumask()).
 *
 * \see topology_device_cpumask
 * \see topology_procent_cpumask
 */
extern size_t topology_sizeof_cpumask(topo_context_t ctx);

/**
 * topology_traverse() - traverse from one processor entity to others
 *
 * @param start - starting point in procent hierarchy
 * @param iter - value returned from previous call to
 * topology_traverse(), or 0
 * @param to_level - the topology_level value which returned objects
 * must match
 *
 * @return The next topo_procent_t object at the level specified,
 * relative to start and iter.
 *
 * This function can be used to interrogate the topo_procent_t
 * hierarchy.  The iter argument is consulted when traversing an
 * object's constituents or "children", i.e. when to_level is less
 * than the starting point's level.
 */
extern topo_procent_t
topology_traverse(topo_procent_t start,
		  topo_procent_t iter,
		  topo_level_t to_level);
/**
 * topology_procent_cpumask() - copy a topo_procent_t's cpumask
 *
 * @param ent - topo_procent_t to query
 * @param dest - pointer to cpu_set_t allocated by the caller
 *
 * Copies ent's cpumask to the cpumask buffer provided by the caller.
 * The size of the cpumask buffer must be equal to the size returned
 * by topology_sizeof_cpumask().
 *
 * \see topology_sizeof_cpumask
 */
extern void topology_procent_cpumask(topo_procent_t ent, cpu_set_t *dest);

/**
 * topology_find_device_by_type() - Iterate from one device to the next
 *                                  of the given type
 * @param ctx - Topology context in use
 * @param prev - 0, or a topo_device_t returned by a previous call
 *               to topology_find_device_by_type()
 * @param type - type string, e.g. "cache"
 * @return The next device matching the given type
 *
 * This interface is useful primarily in combination with
 * topology_for_each_device_of_type() to iterate over a the devices
 * matching the given type.  The order in which devices are returned
 * is unspecified.
 *
 * \see topology_for_each_device_of_type
 */
extern topo_device_t
topology_find_device_by_type(topo_context_t ctx,
			     topo_device_t prev,
			     const char *type);


/**
 * topology_device_cpumask() - copy a topo_device_t's cpumask
 *
 * @param dev - topo_device_t to query
 * @param dest - pointer to cpu_set_t allocated by the caller
 *
 * Copies the given device's cpumask to the cpumask buffer provided by
 * the caller.  The size of the cpumask buffer must be equal to the
 * size returned by topology_sizeof_cpumask().
 *
 * \see topology_sizeof_cpumask
 */
extern void topology_device_cpumask(topo_device_t dev, cpu_set_t *dest);

/**
 * topology_device_get_attribute - query the properties of a device
 *
 * @param dev - topo_device *
 * @param name - string identifying the attribute
 * @return The value of the attribute for the given device.
 *
 * If the device has no attribute matching the given name, returns
 * NULL.  The pointer returned refers to memory allocated by
 * libtopology; if the user intends to use this value after releasing
 * the context associated with dev, a copy should be made.
 */
extern const char *
topology_device_get_attribute(topo_device_t dev, const char *name);

/**
 * topology_for_each_device_of_type() - Iterate over all devices matching type
 *
 * @param ctx - Topology context in use
 * @param dev - struct topo_device * iterator
 * @param type - type string, e.g. "cache"
 *
 * The order in which devices are returned is unspecified.
 */
#define topology_for_each_device_of_type(ctx, dev, type)                            \
	for ((dev) = topology_find_device_by_type((ctx), (topo_device_t)0, (type)); \
	     (dev) != 0;                                                            \
	     (dev) = topology_find_device_by_type((ctx), (dev), (type)))

#ifdef __cplusplus
}
#endif

#endif /* _TOPOLOGY_H_ */
