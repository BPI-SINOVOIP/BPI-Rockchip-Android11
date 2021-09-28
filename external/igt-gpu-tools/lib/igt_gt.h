/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef IGT_GT_H
#define IGT_GT_H

#include "igt_debugfs.h"
#include "igt_dummyload.h"
#include "igt_core.h"

#include "i915_drm.h"

void igt_require_hang_ring(int fd, int ring);

typedef struct igt_hang {
	igt_spin_t *spin;
	unsigned ctx;
	unsigned ban;
	unsigned flags;
} igt_hang_t;

igt_hang_t igt_allow_hang(int fd, unsigned ctx, unsigned flags);
void igt_disallow_hang(int fd, igt_hang_t arg);

#define HANG_POISON 0xc5c5c5c5

igt_hang_t igt_hang_ctx(int fd, uint32_t ctx, int ring, unsigned flags);
#define HANG_ALLOW_BAN 1
#define HANG_ALLOW_CAPTURE 2

igt_hang_t igt_hang_ring(int fd, int ring);
void igt_post_hang_ring(int fd, igt_hang_t arg);

void igt_force_gpu_reset(int fd);

void igt_fork_hang_helper(void);
void igt_stop_hang_helper(void);

int igt_open_forcewake_handle(int fd);

int igt_setup_clflush(void);
void igt_clflush_range(void *addr, int size);

unsigned intel_detect_and_clear_missed_interrupts(int fd);

#define ALL_ENGINES ~0u /* Use in interfaces to iterate all engines */

extern const struct intel_execution_engine {
	const char *name;
	const char *full_name;
	unsigned exec_id;
	unsigned flags;
} intel_execution_engines[];

#define for_if(expr__) if (!(expr__)) {} else

#define for_each_engine(fd__, flags__) \
	for (const struct intel_execution_engine *e__ = intel_execution_engines;\
	     e__->name; \
	     e__++) \
		for_if (gem_has_ring(fd__, flags__ = e__->exec_id | e__->flags))

#define for_each_physical_engine(fd__, flags__) \
	for (const struct intel_execution_engine *e__ = intel_execution_engines;\
	     e__->name; \
	     e__++) \
		for_if (gem_ring_has_physical_engine(fd__, flags__ = e__->exec_id | e__->flags))

bool gem_ring_is_physical_engine(int fd, unsigned int ring);
bool gem_ring_has_physical_engine(int fd, unsigned int ring);

bool gem_can_store_dword(int fd, unsigned int engine);
bool gem_class_can_store_dword(int fd, int class);

extern const struct intel_execution_engine2 {
	const char *name;
	int class;
	int instance;
	uint64_t flags;
	bool is_virtual;
} intel_execution_engines2[];

int gem_execbuf_flags_to_engine_class(unsigned int flags);

#endif /* IGT_GT_H */
