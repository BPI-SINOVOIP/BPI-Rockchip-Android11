/*
 * Copyright © 2016 Broadcom
 * Copyright © 2019 Collabora, Ltd.
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

#ifndef IGT_PANFROST_H
#define IGT_PANFROST_H

#include "panfrost_drm.h"

struct panfrost_bo {
	int handle;
	uint64_t offset;
	uint32_t size;
	void *map;
};

struct panfrost_submit {
	struct drm_panfrost_submit *args;
	struct panfrost_bo *submit_bo;
	struct panfrost_bo *fb_bo;
	struct panfrost_bo *scratchpad_bo;
	struct panfrost_bo *tiler_scratch_bo;
	struct panfrost_bo *tiler_heap_bo;
	struct panfrost_bo *fbo;
};

struct panfrost_bo *igt_panfrost_gem_new(int fd, size_t size);
void igt_panfrost_free_bo(int fd, struct panfrost_bo *bo);

struct panfrost_submit *igt_panfrost_trivial_job(int fd, bool do_crash, int width, int height, uint32_t color);
void igt_panfrost_free_job(int fd, struct panfrost_submit *submit);

/* IOCTL wrappers */
uint32_t igt_panfrost_get_bo_offset(int fd, uint32_t handle);
uint32_t igt_panfrost_get_param(int fd, int param);
void *igt_panfrost_mmap_bo(int fd, uint32_t handle, uint32_t size, unsigned prot);

void igt_panfrost_bo_mmap(int fd, struct panfrost_bo *bo);

#endif /* IGT_PANFROST_H */
