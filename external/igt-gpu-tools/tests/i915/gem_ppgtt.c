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

#include <drm.h>

#include "intel_bufmgr.h"
#include "igt_debugfs.h"

#define WIDTH 512
#define STRIDE (WIDTH*4)
#define HEIGHT 512
#define SIZE (HEIGHT*STRIDE)

static drm_intel_bo *create_bo(drm_intel_bufmgr *bufmgr,
			       uint32_t pixel)
{
	uint64_t value = (uint64_t)pixel << 32 | pixel, *v;
	drm_intel_bo *bo;

	bo = drm_intel_bo_alloc(bufmgr, "surface", SIZE, 4096);
	igt_assert(bo);

	do_or_die(drm_intel_bo_map(bo, 1));
	v = bo->virtual;
	for (int i = 0; i < SIZE / sizeof(value); i += 8) {
		v[i + 0] = value; v[i + 1] = value;
		v[i + 2] = value; v[i + 3] = value;
		v[i + 4] = value; v[i + 5] = value;
		v[i + 6] = value; v[i + 7] = value;
	}
	drm_intel_bo_unmap(bo);

	return bo;
}

static void scratch_buf_init(struct igt_buf *buf,
			     drm_intel_bufmgr *bufmgr,
			     uint32_t pixel)
{
	memset(buf, 0, sizeof(*buf));

	buf->bo = create_bo(bufmgr, pixel);
	buf->stride = STRIDE;
	buf->tiling = I915_TILING_NONE;
	buf->size = SIZE;
	buf->bpp = 32;
}

static void scratch_buf_fini(struct igt_buf *buf)
{
	drm_intel_bo_unreference(buf->bo);
	memset(buf, 0, sizeof(*buf));
}

static void fork_rcs_copy(int timeout, uint32_t final,
			  drm_intel_bo **dst, int count,
			  unsigned flags)
#define CREATE_CONTEXT 0x1
{
	igt_render_copyfunc_t render_copy;
	uint64_t mem_per_child;
	int devid;

	mem_per_child = SIZE;
	if (flags & CREATE_CONTEXT)
		mem_per_child += 2 * 128 * 1024; /* rough context sizes */
	intel_require_memory(count, mem_per_child, CHECK_RAM);

	for (int child = 0; child < count; child++) {
		int fd = drm_open_driver(DRIVER_INTEL);
		drm_intel_bufmgr *bufmgr;

		devid = intel_get_drm_devid(fd);

		bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
		igt_assert(bufmgr);

		dst[child] = create_bo(bufmgr, ~0);

		if (flags & CREATE_CONTEXT) {
			drm_intel_context *ctx;

			ctx = drm_intel_gem_context_create(dst[child]->bufmgr);
			igt_require(ctx);
		}

		render_copy = igt_get_render_copyfunc(devid);
		igt_require_f(render_copy,
			      "no render-copy function\n");
	}

	igt_fork(child, count) {
		struct intel_batchbuffer *batch;
		struct igt_buf buf = {};
		struct igt_buf src;
		unsigned long i;

		batch = intel_batchbuffer_alloc(dst[child]->bufmgr,
						devid);
		igt_assert(batch);

		if (flags & CREATE_CONTEXT) {
			drm_intel_context *ctx;

			ctx = drm_intel_gem_context_create(dst[child]->bufmgr);
			intel_batchbuffer_set_context(batch, ctx);
		}

		buf.bo = dst[child];
		buf.stride = STRIDE;
		buf.tiling = I915_TILING_NONE;
		buf.size = SIZE;
		buf.bpp = 32;

		i = 0;
		igt_until_timeout(timeout) {
			scratch_buf_init(&src, dst[child]->bufmgr,
					 i++ | child << 16);
			render_copy(batch, NULL,
				    &src, 0, 0,
				    WIDTH, HEIGHT,
				    &buf, 0, 0);
			scratch_buf_fini(&src);
		}

		scratch_buf_init(&src, dst[child]->bufmgr,
				 final | child << 16);
		render_copy(batch, NULL,
			    &src, 0, 0,
			    WIDTH, HEIGHT,
			    &buf, 0, 0);
		scratch_buf_fini(&src);
	}
}

static void fork_bcs_copy(int timeout, uint32_t final,
			  drm_intel_bo **dst, int count)
{
	int devid;

	for (int child = 0; child < count; child++) {
		drm_intel_bufmgr *bufmgr;
		int fd = drm_open_driver(DRIVER_INTEL);

		devid = intel_get_drm_devid(fd);

		bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
		igt_assert(bufmgr);

		dst[child] = create_bo(bufmgr, ~0);
	}

	igt_fork(child, count) {
		struct intel_batchbuffer *batch;
		drm_intel_bo *src[2];
		unsigned long i;


		batch = intel_batchbuffer_alloc(dst[child]->bufmgr,
						devid);
		igt_assert(batch);

		i = 0;
		igt_until_timeout(timeout) {
			src[0] = create_bo(dst[child]->bufmgr,
					   ~0);
			src[1] = create_bo(dst[child]->bufmgr,
					   i++ | child << 16);

			intel_copy_bo(batch, src[0], src[1], SIZE);
			intel_copy_bo(batch, dst[child], src[0], SIZE);

			drm_intel_bo_unreference(src[1]);
			drm_intel_bo_unreference(src[0]);
		}

		src[0] = create_bo(dst[child]->bufmgr,
				   ~0);
		src[1] = create_bo(dst[child]->bufmgr,
				   final | child << 16);

		intel_copy_bo(batch, src[0], src[1], SIZE);
		intel_copy_bo(batch, dst[child], src[0], SIZE);

		drm_intel_bo_unreference(src[1]);
		drm_intel_bo_unreference(src[0]);
	}
}

static void surfaces_check(drm_intel_bo **bo, int count, uint32_t expected)
{
	for (int child = 0; child < count; child++) {
		uint32_t *ptr;

		do_or_die(drm_intel_bo_map(bo[child], 0));
		ptr = bo[child]->virtual;
		for (int j = 0; j < SIZE/4; j++)
			igt_assert_eq(ptr[j], expected | child << 16);
		drm_intel_bo_unmap(bo[child]);
	}
}

static uint64_t exec_and_get_offset(int fd, uint32_t batch)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec[1];
	uint32_t batch_data[2] = { MI_BATCH_BUFFER_END };

	gem_write(fd, batch, 0, batch_data, sizeof(batch_data));

	memset(exec, 0, sizeof(exec));
	exec[0].handle = batch;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(exec);
	execbuf.buffer_count = 1;

	gem_execbuf(fd, &execbuf);
	igt_assert_neq(exec[0].offset, -1);

	return exec[0].offset;
}

static void flink_and_close(void)
{
	uint32_t fd, fd2;
	uint32_t bo, flinked_bo, new_bo, name;
	uint64_t offset, offset_new;

	fd = drm_open_driver(DRIVER_INTEL);
	igt_require(gem_uses_full_ppgtt(fd));

	bo = gem_create(fd, 4096);
	name = gem_flink(fd, bo);

	fd2 = drm_open_driver(DRIVER_INTEL);

	flinked_bo = gem_open(fd2, name);
	offset = exec_and_get_offset(fd2, flinked_bo);
	gem_sync(fd2, flinked_bo);
	gem_close(fd2, flinked_bo);

	igt_drop_caches_set(fd, DROP_RETIRE | DROP_IDLE);

	/* the flinked bo VMA should have been cleared now, so a new bo of the
	 * same size should get the same offset
	 */
	new_bo = gem_create(fd2, 4096);
	offset_new = exec_and_get_offset(fd2, new_bo);
	gem_close(fd2, new_bo);

	igt_assert_eq(offset, offset_new);

	gem_close(fd, bo);
	close(fd);
	close(fd2);
}

#define N_CHILD 8
igt_main
{
	igt_fixture {
		int fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
		close(fd);
	}

	igt_subtest("blt-vs-render-ctx0") {
		drm_intel_bo *bcs[1], *rcs[N_CHILD];

		fork_bcs_copy(30, 0x4000, bcs, 1);
		fork_rcs_copy(30, 0x8000 / N_CHILD, rcs, N_CHILD, 0);

		igt_waitchildren();

		surfaces_check(bcs, 1, 0x4000);
		surfaces_check(rcs, N_CHILD, 0x8000 / N_CHILD);
	}

	igt_subtest("blt-vs-render-ctxN") {
		drm_intel_bo *bcs[1], *rcs[N_CHILD];

		fork_rcs_copy(30, 0x8000 / N_CHILD, rcs, N_CHILD, CREATE_CONTEXT);
		fork_bcs_copy(30, 0x4000, bcs, 1);

		igt_waitchildren();

		surfaces_check(bcs, 1, 0x4000);
		surfaces_check(rcs, N_CHILD, 0x8000 / N_CHILD);
	}

	igt_subtest("flink-and-close-vma-leak")
		flink_and_close();
}
