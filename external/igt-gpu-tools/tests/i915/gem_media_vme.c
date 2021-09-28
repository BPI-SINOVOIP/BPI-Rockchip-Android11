/*
 * Copyright © 2018 Intel Corporation
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
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "drm.h"
#include "intel_bufmgr.h"

IGT_TEST_DESCRIPTION("A very simple workload for the VME media block.");

#define WIDTH	64
#define STRIDE	(WIDTH)
#define HEIGHT	64

#define INPUT_SIZE	(WIDTH * HEIGHT * sizeof(char) * 1.5)
#define OUTPUT_SIZE	(56*sizeof(int))

static void
scratch_buf_init(drm_intel_bufmgr *bufmgr,
		 struct igt_buf *buf,
		 unsigned int size)
{
	drm_intel_bo *bo;

	bo = drm_intel_bo_alloc(bufmgr, "", size, 4096);
	igt_assert(bo);

	memset(buf, 0, sizeof(*buf));

	buf->bo = bo;
	buf->tiling = I915_TILING_NONE;
	buf->size = size;
}

static void scratch_buf_init_src(drm_intel_bufmgr *bufmgr, struct igt_buf *buf)
{
	scratch_buf_init(bufmgr, buf, INPUT_SIZE);

	/*
	 * Ideally we would read src surface from file "SourceFrameI.yu12".
	 * But even without it, we can still triger the rcs0 resetting
	 * with this vme kernel.
	 */

	buf->stride = STRIDE;
}

static void scratch_buf_init_dst(drm_intel_bufmgr *bufmgr, struct igt_buf *buf)
{
	scratch_buf_init(bufmgr, buf, OUTPUT_SIZE);

	buf->stride = 1;
}

static uint64_t switch_off_n_bits(uint64_t mask, unsigned int n)
{
	unsigned int i;

	igt_assert(n > 0 && n <= (sizeof(mask) * 8));
	igt_assert(n <= __builtin_popcount(mask));

	for (i = 0; n && i < (sizeof(mask) * 8); i++) {
		uint64_t bit = 1ULL << i;

		if (bit & mask) {
			mask &= ~bit;
			n--;
		}
	}

	return mask;
}

static void shut_non_vme_subslices(int drm_fd, uint32_t ctx)
{
	struct drm_i915_gem_context_param_sseu sseu = { };
	struct drm_i915_gem_context_param arg = {
		.param = I915_CONTEXT_PARAM_SSEU,
		.ctx_id = ctx,
		.size = sizeof(sseu),
		.value = to_user_pointer(&sseu),
	};
	int ret;

	if (__gem_context_get_param(drm_fd, &arg))
		return; /* no sseu support */

	ret = __gem_context_set_param(drm_fd, &arg);
	igt_assert(ret == 0 || ret == -ENODEV || ret == -EINVAL);
	if (ret)
		return; /* no sseu support */

	/* shutdown half subslices */
	sseu.subslice_mask =
		switch_off_n_bits(sseu.subslice_mask,
				  __builtin_popcount(sseu.subslice_mask) / 2);

	gem_context_set_param(drm_fd, &arg);
}

igt_simple_main
{
	int drm_fd;
	uint32_t devid;
	drm_intel_bufmgr *bufmgr;
	igt_vme_func_t media_vme;
	struct intel_batchbuffer *batch;
	struct igt_buf src, dst;

	drm_fd = drm_open_driver(DRIVER_INTEL);
	igt_require_gem(drm_fd);

	devid = intel_get_drm_devid(drm_fd);

	media_vme = igt_get_media_vme_func(devid);
	igt_require_f(media_vme, "no media-vme function\n");

	bufmgr = drm_intel_bufmgr_gem_init(drm_fd, 4096);
	igt_assert(bufmgr);

	batch = intel_batchbuffer_alloc(bufmgr, devid);
	igt_assert(batch);

	scratch_buf_init_src(bufmgr, &src);
	scratch_buf_init_dst(bufmgr, &dst);

	batch->ctx = drm_intel_gem_context_create(bufmgr);
	igt_assert(batch->ctx);

	/* ICL hangs if non-VME enabled slices are enabled with a VME kernel. */
	if (intel_gen(devid) == 11) {
		uint32_t ctx_id;
		int ret;

		ret = drm_intel_gem_context_get_id(batch->ctx, &ctx_id);
		igt_assert_eq(ret, 0);

		shut_non_vme_subslices(drm_fd, ctx_id);
	}

	igt_fork_hang_detector(drm_fd);

	media_vme(batch, &src, WIDTH, HEIGHT, &dst);

	gem_sync(drm_fd, dst.bo->handle);

	igt_stop_hang_detector();
}
