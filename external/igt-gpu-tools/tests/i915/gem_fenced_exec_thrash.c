/*
 * Copyright © 2011 Intel Corporation
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
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "igt.h"

IGT_TEST_DESCRIPTION("Test execbuf fence accounting.");

#define WIDTH 1024
#define HEIGHT 1024
#define OBJECT_SIZE (4*WIDTH*HEIGHT)

#define BATCH_SIZE 4096

#define MAX_FENCES 64

/*
 * Testcase: execbuf fence accounting
 *
 * We had a bug where we were falsely accounting upon reservation already
 * fenced buffers as occupying a fence register even if they did not require
 * one for the batch.
 *
 * We aim to exercise this by performing a sequence of fenced BLT
 * with 2*num_avail_fence buffers, but alternating which half are fenced in
 * each command.
 */

static uint32_t
tiled_bo_create (int fd)
{
	uint32_t handle;

	handle = gem_create(fd, OBJECT_SIZE);

	gem_set_tiling(fd, handle, I915_TILING_X, WIDTH*4);

	return handle;
}

static uint32_t
batch_create (int fd)
{
	uint32_t buf[] = { MI_BATCH_BUFFER_END, 0 };
	uint32_t batch_handle;

	batch_handle = gem_create(fd, BATCH_SIZE);

	gem_write(fd, batch_handle, 0, buf, sizeof(buf));

	return batch_handle;
}

static void fill_reloc(struct drm_i915_gem_relocation_entry *reloc, uint32_t handle)
{
	reloc->offset = 2 * sizeof(uint32_t);
	reloc->target_handle = handle;
	reloc->read_domains = I915_GEM_DOMAIN_RENDER;
	reloc->write_domain = 0;
}

#define BUSY_LOAD (1 << 0)
#define INTERRUPTIBLE (1 << 1)

static void run_test(int fd, int num_fences, int expected_errno,
		     unsigned flags)
{
	struct drm_i915_gem_execbuffer2 execbuf[2];
	struct drm_i915_gem_exec_object2 exec[2][2*MAX_FENCES+1];
	struct drm_i915_gem_relocation_entry reloc[2*MAX_FENCES];

	unsigned long count;
	int i, n;

	igt_assert(2*num_fences+1 <= ARRAY_SIZE(exec[0]));
	igt_assert(2*num_fences <= ARRAY_SIZE(reloc));

	memset(execbuf, 0, sizeof(execbuf));
	memset(exec, 0, sizeof(exec));
	memset(reloc, 0, sizeof(reloc));

	for (n = 0; n < 2*num_fences; n++) {
		uint32_t handle = tiled_bo_create(fd);
		exec[1][2*num_fences - n-1].handle = exec[0][n].handle = handle;
		fill_reloc(&reloc[n], handle);
	}

	for (i = 0; i < 2; i++) {
		for (n = 0; n < num_fences; n++)
			exec[i][n].flags = EXEC_OBJECT_NEEDS_FENCE;

		exec[i][2*num_fences].handle = batch_create(fd);
		exec[i][2*num_fences].relocs_ptr = to_user_pointer(reloc);
		exec[i][2*num_fences].relocation_count = 2*num_fences;

		execbuf[i].buffers_ptr = to_user_pointer(exec[i]);
		execbuf[i].buffer_count = 2*num_fences+1;
		execbuf[i].batch_len = 2*sizeof(uint32_t);
	}

	count = 0;
	igt_until_timeout(2) {
		for (i = 0; i < 2; i++) {
			igt_spin_t *spin = NULL;

			if (flags & BUSY_LOAD)
				spin = __igt_spin_new(fd);

			igt_while_interruptible(flags & INTERRUPTIBLE) {
				igt_assert_eq(__gem_execbuf(fd, &execbuf[i]),
					      -expected_errno);
			}

			igt_spin_free(fd, spin);
			gem_quiescent_gpu(fd);
		}
		count++;
	}
	igt_info("Completed %lu cycles\n", count);

	/* Cleanup */
	for (n = 0; n < 2*num_fences; n++)
		gem_close(fd, exec[0][n].handle);

	for (i = 0; i < 2; i++)
		gem_close(fd, exec[i][2*num_fences].handle);
}

igt_main
{
	uint32_t devid = 0;
	unsigned int num_fences = 0;
	int fd = -1;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		num_fences = gem_available_fences(fd);
		igt_assert(num_fences > 4);
		igt_assert(num_fences <= MAX_FENCES);

		devid = intel_get_drm_devid(fd);
	}

	igt_subtest("2-spare-fences")
		run_test(fd, num_fences - 2, 0, 0);
	for (unsigned flags = 0; flags < 4; flags++) {
		igt_subtest_f("no-spare-fences%s%s",
			      flags & BUSY_LOAD ? "-busy" : "",
			      flags & INTERRUPTIBLE ? "-interruptible" : "")
			run_test(fd, num_fences, 0, flags);
	}
	igt_subtest("too-many-fences")
		run_test(fd, num_fences + 1, intel_gen(devid) >= 4 ? 0 : EDEADLK, 0);

	igt_fixture
		close(fd);
}
