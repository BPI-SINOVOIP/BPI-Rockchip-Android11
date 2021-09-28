/*
 * Copyright © 2016 Intel Corporation
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
 *
 */

#ifndef __IGT_DUMMYLOAD_H__
#define __IGT_DUMMYLOAD_H__

#include <stdint.h>
#include <time.h>

#include "igt_core.h"
#include "igt_list.h"
#include "i915_drm.h"

typedef struct igt_spin {
	unsigned int handle;
	timer_t timer;
	struct igt_list link;

	uint32_t *condition;
	uint32_t cmd_precondition;

	int out_fence;
	struct drm_i915_gem_exec_object2 obj[2];
#define IGT_SPIN_BATCH   1
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t poll_handle;
	uint32_t *poll;
#define SPIN_POLL_START_IDX 0
} igt_spin_t;

struct igt_spin_factory {
	uint32_t ctx;
	uint32_t dependency;
	unsigned int engine;
	unsigned int flags;
};

#define IGT_SPIN_FENCE_OUT     (1 << 0)
#define IGT_SPIN_POLL_RUN      (1 << 1)
#define IGT_SPIN_FAST          (1 << 2)
#define IGT_SPIN_NO_PREEMPTION (1 << 3)

igt_spin_t *
__igt_spin_factory(int fd, const struct igt_spin_factory *opts);
igt_spin_t *
igt_spin_factory(int fd, const struct igt_spin_factory *opts);

#define __igt_spin_new(fd, ...) \
	__igt_spin_factory(fd, &((struct igt_spin_factory){__VA_ARGS__}))
#define igt_spin_new(fd, ...) \
	igt_spin_factory(fd, &((struct igt_spin_factory){__VA_ARGS__}))

void igt_spin_set_timeout(igt_spin_t *spin, int64_t ns);
void igt_spin_reset(igt_spin_t *spin);
void igt_spin_end(igt_spin_t *spin);
void igt_spin_free(int fd, igt_spin_t *spin);

static inline bool igt_spin_has_poll(const igt_spin_t *spin)
{
	return spin->poll;
}

static inline bool igt_spin_has_started(igt_spin_t *spin)
{
	return READ_ONCE(spin->poll[SPIN_POLL_START_IDX]);
}

static inline void igt_spin_busywait_until_started(igt_spin_t *spin)
{
	while (!igt_spin_has_started(spin))
		;
}

void igt_terminate_spins(void);
void igt_unshare_spins(void);

enum igt_cork_type {
	CORK_SYNC_FD = 1,
	CORK_VGEM_HANDLE
};

struct igt_cork_vgem {
	int device;
	uint32_t fence;
};

struct igt_cork_sw_sync {
	int timeline;
};

struct igt_cork {
	enum igt_cork_type type;

	union {
		int fd;

		struct igt_cork_vgem vgem;
		struct igt_cork_sw_sync sw_sync;
	};
};

#define IGT_CORK(name, cork_type) struct igt_cork name = { .type = cork_type, .fd = -1}
#define IGT_CORK_HANDLE(name) IGT_CORK(name, CORK_VGEM_HANDLE)
#define IGT_CORK_FENCE(name) IGT_CORK(name, CORK_SYNC_FD)

uint32_t igt_cork_plug(struct igt_cork *cork, int fd);
void igt_cork_unplug(struct igt_cork *cork);

#endif /* __IGT_DUMMYLOAD_H__ */
