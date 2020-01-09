#ifndef _TOPOLOGY_CPUMASK_H_
#define _TOPOLOGY_CPUMASK_H_

extern int topology_cpumask_parse(cpu_set_t *cpumask, size_t size, const char *buf);

#endif /* _TOPOLOGY_CPUMASK_H_ */
