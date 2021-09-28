/*
 * Copyright © 2017 Intel Corporation
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

#include "igt.h"

#define MAX_ERROR 5 /* % */

#define assert_within_epsilon(x, ref, tolerance) \
	igt_assert_f(100 * x <= (100 + tolerance) * ref && \
		     100 * x >= (100 - tolerance) * ref, \
		     "'%s' != '%s' (%lld not within %d%% tolerance of %lld)\n",\
		     #x, #ref, (long long)x, tolerance, (long long)ref)

static void spin(int fd, const struct intel_execution_engine2 *e2,
		 unsigned int timeout_sec)
{
	const uint64_t timeout_100ms = 100000000LL;
	unsigned long loops = 0;
	igt_spin_t *spin;
	struct timespec tv = { };
	struct timespec itv = { };
	uint64_t elapsed;

	spin = __igt_spin_new(fd, .engine = e2->flags);
	while ((elapsed = igt_nsec_elapsed(&tv)) >> 30 < timeout_sec) {
		igt_spin_t *next = __igt_spin_new(fd, .engine = e2->flags);

		igt_spin_set_timeout(spin,
				     timeout_100ms - igt_nsec_elapsed(&itv));
		gem_sync(fd, spin->handle);
		igt_debug("loop %lu: interval=%fms (target 100ms), elapsed %fms\n",
			  loops,
			  igt_nsec_elapsed(&itv) * 1e-6,
			  igt_nsec_elapsed(&tv) * 1e-6);
		memset(&itv, 0, sizeof(itv));

		igt_spin_free(fd, spin);
		spin = next;
		loops++;
	}
	igt_spin_free(fd, spin);

	igt_info("Completed %ld loops in %lld ns, target %ld\n",
		 loops, (long long)elapsed, (long)(elapsed / timeout_100ms));

	assert_within_epsilon(timeout_100ms * loops, elapsed, MAX_ERROR);
}

#define RESUBMIT_NEW_CTX     (1 << 0)
#define RESUBMIT_ALL_ENGINES (1 << 1)

static void spin_resubmit(int fd, const struct intel_execution_engine2 *e2,
			  unsigned int flags)
{
	const uint32_t ctx0 = gem_context_create(fd);
	const uint32_t ctx1 = (flags & RESUBMIT_NEW_CTX) ?
		gem_context_create(fd) : ctx0;
	igt_spin_t *spin = __igt_spin_new(fd, .ctx = ctx0, .engine = e2->flags);
	const struct intel_execution_engine2 *other;

	struct drm_i915_gem_execbuffer2 eb = {
		.buffer_count = 1,
		.buffers_ptr = to_user_pointer(&spin->obj[IGT_SPIN_BATCH]),
		.rsvd1 = ctx1,
	};

	igt_assert(gem_context_has_engine_map(fd, 0) ||
		   !(flags & RESUBMIT_ALL_ENGINES));

	if (flags & RESUBMIT_ALL_ENGINES) {
		gem_context_set_all_engines(fd, ctx0);
		if (ctx0 != ctx1)
			gem_context_set_all_engines(fd, ctx1);

		for_each_context_engine(fd, ctx1, other) {
			if (gem_engine_is_equal(other, e2))
				continue;

			eb.flags = other->flags;
			gem_execbuf(fd, &eb);
		}
	} else {
		eb.flags = e2->flags;
		gem_execbuf(fd, &eb);
	}

	igt_spin_end(spin);

	gem_sync(fd, spin->handle);

	igt_spin_free(fd, spin);

	if (ctx1 != ctx0)
		gem_context_destroy(fd, ctx1);

	gem_context_destroy(fd, ctx0);
}

static void spin_exit_handler(int sig)
{
	igt_terminate_spins();
}

static void spin_on_all_engines(int fd, unsigned int timeout_sec)
{
	const struct intel_execution_engine2 *e2;

	__for_each_physical_engine(fd, e2) {
		igt_fork(child, 1) {
			igt_install_exit_handler(spin_exit_handler);
			spin(fd, e2, timeout_sec);
		}
	}

	igt_waitchildren();
}

igt_main
{
	const struct intel_execution_engine2 *e2;
	const struct intel_execution_engine *e;
	struct intel_execution_engine2 e2__;
	int fd = -1;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
		igt_fork_hang_detector(fd);
	}

	for (e = intel_execution_engines; e->name; e++) {
		e2__ = gem_eb_flags_to_engine(e->exec_id | e->flags);
		if (e2__.flags == -1)
			continue;
		e2 = &e2__;

		igt_subtest_f("legacy-%s", e->name)
			spin(fd, e2, 3);

		igt_subtest_f("legacy-resubmit-%s", e->name)
			spin_resubmit(fd, e2, 0);

		igt_subtest_f("legacy-resubmit-new-%s", e->name)
			spin_resubmit(fd, e2, RESUBMIT_NEW_CTX);
	}

	__for_each_physical_engine(fd, e2) {
		igt_subtest_f("%s", e2->name)
			spin(fd, e2, 3);

		igt_subtest_f("resubmit-%s", e2->name)
			spin_resubmit(fd, e2, 0);

		igt_subtest_f("resubmit-new-%s", e2->name)
			spin_resubmit(fd, e2, RESUBMIT_NEW_CTX);

		igt_subtest_f("resubmit-all-%s", e2->name)
			spin_resubmit(fd, e2, RESUBMIT_ALL_ENGINES);

		igt_subtest_f("resubmit-new-all-%s", e2->name)
			spin_resubmit(fd, e2,
				      RESUBMIT_NEW_CTX |
				      RESUBMIT_ALL_ENGINES);
	}

	igt_subtest("spin-each")
		spin_on_all_engines(fd, 3);

	igt_fixture {
		igt_stop_hang_detector();
		close(fd);
	}
}
