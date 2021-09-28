/*
 * Copyright (c) 2018 Google, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _LTP_EAS_UTIL_H_
#define _LTP_EAS_UTIL_H_

#include <linux/unistd.h>
#include <sched.h>
#include <stdio.h>

#define USEC_PER_SEC 1000000

#define TS_TO_USEC(x) (x.usec + x.sec * USEC_PER_SEC)

#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

#define gettid() syscall(__NR_gettid)

#define ERROR_CHECK(x) \
	if (x) \
		fprintf(stderr, "Error at line %d", __LINE__);

void affine(int cpu);
void burn(unsigned int usec, int sleep);
int find_cpus_with_capacity(int minmax, cpu_set_t * cpuset);

#endif /* _LTP_EAS_UTIL_H_ */
