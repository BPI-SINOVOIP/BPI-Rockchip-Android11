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

#include "igt.h"

static void test_many_handles(int fd)
{
	uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	uint32_t clones[1024];
	uint32_t original;

	original = gem_create(fd, 4096);
	gem_write(fd, original, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 1;

	memset(obj, 0, sizeof(obj));
	obj[0].handle = original;
	gem_execbuf(fd, &execbuf);

	for (int i = 0; i < ARRAY_SIZE(clones); i++) {
		clones[i] = gem_open(fd, gem_flink(fd, original));
		obj[0].handle = clones[i];
		gem_execbuf(fd, &execbuf);
	}

	/* We do not allow the sam object to be referenced multiple times
	 * within an execbuf; hence why this practice of cloning a handle
	 * is only found within test cases.
	 */
	execbuf.buffer_count = 2;
	obj[0].handle = original;
	for (int i = 0; i < ARRAY_SIZE(clones); i++) {
		obj[1].handle = clones[i];
		igt_assert_eq(__gem_execbuf(fd, &execbuf), -EINVAL);
	}
	/* Any other clone pair should also be detected */
	obj[1].handle = clones[0];  /* (last, first) */
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -EINVAL);
	execbuf.buffer_count = 1;

	/* Now close the original having used every clone */
	obj[0].handle = original;
	gem_close(fd, original);
	igt_assert_eq(__gem_execbuf(fd, &execbuf), -ENOENT);

	/* All clones should still be operational */
	for (int i = 0; i < ARRAY_SIZE(clones); i++) {
		obj[0].handle = clones[i];
		gem_execbuf(fd, &execbuf);

		/* ... until closed */
		gem_close(fd, clones[i]);
		igt_assert_eq(__gem_execbuf(fd, &execbuf), -ENOENT);
	}
}

igt_main
{
	int fd = -1;

	igt_fixture {
		/* Create an flink requires DRM_AUTH */
		fd = drm_open_driver_master(DRIVER_INTEL);
		igt_require_gem(fd);
	}

	igt_subtest("basic")
		gem_close(fd, gem_create(fd, 4096));

	igt_subtest("many-handles-one-vma")
		test_many_handles(fd);
}
