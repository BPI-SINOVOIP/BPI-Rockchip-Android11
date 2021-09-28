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
#include <string.h>
#include <sys/ioctl.h>

#include "igt_core.h"
#include "ioctl_wrappers.h"

#include "i915/gem_scheduler.h"

#define LOCAL_I915_PARAM_HAS_SCHEDULER		41

/**
 * SECTION:gem_scheduler
 * @short_description: Helpers for querying scheduler capabilities
 * @title: GEM Scheduler
 *
 * This helper library contains functions used for getting information on
 * currently used scheduling model.
 */

/**
 * gem_scheduler_capability:
 * @fd: open i915 drm file descriptor
 *
 * Returns: Scheduler capability bitmap.
 */
unsigned gem_scheduler_capability(int fd)
{
	static int caps = -1;

	if (caps < 0) {
		struct drm_i915_getparam gp;

		memset(&gp, 0, sizeof(gp));
		gp.param = LOCAL_I915_PARAM_HAS_SCHEDULER;
		gp.value = &caps;

		caps = 0;
		ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp, sizeof(gp));
		errno = 0;
	}

	return caps;
}

/**
 * gem_scheduler_enabled:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver has scheduling capability.
 */
bool gem_scheduler_enabled(int fd)
{
	return gem_scheduler_capability(fd) &
		I915_SCHEDULER_CAP_ENABLED;
}

/**
 * gem_scheduler_has_ctx_priority:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver supports assigning custom
 * priorities to contexts from userspace.
 */
bool gem_scheduler_has_ctx_priority(int fd)
{
	return gem_scheduler_capability(fd) &
		I915_SCHEDULER_CAP_PRIORITY;
}

/**
 * gem_scheduler_has_preemption:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver supports preempting active
 * (currently executing on HW) workloads.
 */
bool gem_scheduler_has_preemption(int fd)
{
	return gem_scheduler_capability(fd) &
		I915_SCHEDULER_CAP_PREEMPTION;
}

/**
 * gem_scheduler_has_semaphores:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query whether the driver supports using HW semaphores
 * to schedule dependencies in parallel (using the HW to delay execution until
 * ready to reduce latency).
 */
bool gem_scheduler_has_semaphores(int fd)
{
	return gem_scheduler_capability(fd) &
		I915_SCHEDULER_CAP_SEMAPHORES;
}

/**
 * gem_scheduler_print_capability:
 * @fd: open i915 drm file descriptor
 *
 * Helper for pretty-printing scheduler capability.
 */
void gem_scheduler_print_capability(int fd)
{
	unsigned caps = gem_scheduler_capability(fd);

	if (!caps)
		return;

	igt_info("Has kernel scheduler\n");
	if (caps & I915_SCHEDULER_CAP_PRIORITY)
		igt_info(" - With priority sorting\n");
	if (caps & I915_SCHEDULER_CAP_PREEMPTION)
		igt_info(" - With preemption enabled\n");
	if (caps & I915_SCHEDULER_CAP_SEMAPHORES)
		igt_info(" - With HW semaphores enabled\n");
}
