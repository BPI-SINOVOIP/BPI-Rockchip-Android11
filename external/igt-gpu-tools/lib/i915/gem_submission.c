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
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include <i915_drm.h>

#include "i915/gem_engine_topology.h"

#include "igt_core.h"
#include "igt_gt.h"
#include "igt_sysfs.h"
#include "intel_chipset.h"
#include "intel_reg.h"
#include "ioctl_wrappers.h"

#include "i915/gem_submission.h"

/**
 * SECTION:gem_submission
 * @short_description: Helpers for determining submission method
 * @title: GEM Submission
 *
 * This helper library contains functions used for getting information on
 * currently used hardware submission method. Different generations of hardware
 * support different submission backends, currently we're distinguishing 3
 * different methods: legacy ringbuffer submission, execlists, GuC submission.
 * For legacy ringbuffer submission, there's also a variation where we're using
 * semaphores for synchronization between engines.
 */

static bool has_semaphores(int fd, int dir)
{
	int val = 0;
	struct drm_i915_getparam gp = {
		gp.param = I915_PARAM_HAS_SEMAPHORES,
		gp.value = &val,
	};
	if (ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp) < 0)
		val = igt_sysfs_get_boolean(dir, "semaphores");
	return val;
}

/**
 * gem_submission_method:
 * @fd: open i915 drm file descriptor
 *
 * Returns: Submission method bitmap.
 */
unsigned gem_submission_method(int fd)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	unsigned flags = 0;

	int dir;

	dir = igt_sysfs_open_parameters(fd);
	if (dir < 0)
		return 0;

	if (igt_sysfs_get_u32(dir, "enable_guc") & 1) {
		flags |= GEM_SUBMISSION_GUC | GEM_SUBMISSION_EXECLISTS;
		goto out;
	}

	if (gen >= 8) {
		flags |= GEM_SUBMISSION_EXECLISTS;
		goto out;
	}

	if (has_semaphores(fd, dir))
		flags |= GEM_SUBMISSION_SEMAPHORES;

out:
	close(dir);
	return flags;
}

/**
 * gem_submission_print_method:
 * @fd: open i915 drm file descriptor
 *
 * Helper for pretty-printing currently used submission method
 */
void gem_submission_print_method(int fd)
{
	const unsigned flags = gem_submission_method(fd);

	if (flags & GEM_SUBMISSION_GUC) {
		igt_info("Using GuC submission\n");
		return;
	}

	if (flags & GEM_SUBMISSION_EXECLISTS) {
		igt_info("Using Execlists submission\n");
		return;
	}

	igt_info("Using Legacy submission%s\n",
		 flags & GEM_SUBMISSION_SEMAPHORES ? ", with semaphores" : "");
}

/**
 * gem_has_semaphores:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver is using semaphores for
 * synchronization between engines.
 */
bool gem_has_semaphores(int fd)
{
	return gem_submission_method(fd) & GEM_SUBMISSION_SEMAPHORES;
}

/**
 * gem_has_execlists:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver is using execlists as a
 * hardware submission method.
 */
bool gem_has_execlists(int fd)
{
	return gem_submission_method(fd) & GEM_SUBMISSION_EXECLISTS;
}

/**
 * gem_has_guc_submission:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver is using the GuC as a
 * hardware submission method.
 */
bool gem_has_guc_submission(int fd)
{
	return gem_submission_method(fd) & GEM_SUBMISSION_GUC;
}

/**
 * gem_reopen_driver:
 * @fd: re-open the i915 drm file descriptor
 *
 * Re-opens the drm fd which is useful in instances where a clean default
 * context is needed.
 */
int gem_reopen_driver(int fd)
{
	char path[256];

	snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
	fd = open(path, O_RDWR);
	igt_assert_fd(fd);

	return fd;
}

static bool is_wedged(int i915)
{
	int err = 0;
	if (ioctl(i915, DRM_IOCTL_I915_GEM_THROTTLE))
		err = -errno;
	return err == -EIO;
}

/**
 * gem_test_engine:
 * @i915: open i915 drm file descriptor
 * @engine: the engine (I915_EXEC_RING id) to exercise
 *
 * Execute a nop batch on the engine specified, or ALL_ENGINES for all,
 * and check it executes.
 */
void gem_test_engine(int i915, unsigned int engine)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj = { };
	struct drm_i915_gem_execbuffer2 execbuf = {
		.buffers_ptr = to_user_pointer(&obj),
		.buffer_count = 1,
	};

	i915 = gem_reopen_driver(i915);
	igt_assert(!is_wedged(i915));

	obj.handle = gem_create(i915, 4096);
	gem_write(i915, obj.handle, 0, &bbe, sizeof(bbe));

	if (engine == ALL_ENGINES) {
		const struct intel_execution_engine2 *e2;

		__for_each_physical_engine(i915, e2) {
			execbuf.flags = e2->flags;
			gem_execbuf(i915, &execbuf);
		}
	} else {
		execbuf.flags = engine;
		gem_execbuf(i915, &execbuf);
	}
	gem_sync(i915, obj.handle);
	gem_close(i915, obj.handle);

	igt_assert(!is_wedged(i915));
	close(i915);
}
