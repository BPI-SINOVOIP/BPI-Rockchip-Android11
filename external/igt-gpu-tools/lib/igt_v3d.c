/*
 * Copyright © 2016 Broadcom
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

#include <assert.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "drmtest.h"
#include "igt_aux.h"
#include "igt_core.h"
#include "igt_v3d.h"
#include "ioctl_wrappers.h"
#include "intel_reg.h"
#include "intel_chipset.h"
#include "v3d_drm.h"

/**
 * SECTION:igt_v3d
 * @short_description: V3D support library
 * @title: V3D
 * @include: igt.h
 *
 * This library provides various auxiliary helper functions for writing V3D
 * tests.
 */

struct v3d_bo *
igt_v3d_create_bo(int fd, size_t size)
{
	struct v3d_bo *bo = calloc(1, sizeof(*bo));

	struct drm_v3d_create_bo create = {
		.size = size,
	};

	do_ioctl(fd, DRM_IOCTL_V3D_CREATE_BO, &create);

	bo->handle = create.handle;
	bo->offset = create.offset;
	bo->size = size;

	return bo;
}

void
igt_v3d_free_bo(int fd, struct v3d_bo *bo)
{
	if (bo->map)
		munmap(bo->map, bo->size);
	gem_close(fd, bo->handle);
	free(bo);
}

uint32_t
igt_v3d_get_bo_offset(int fd, uint32_t handle)
{
	struct drm_v3d_get_bo_offset get = {
		.handle = handle,
	};

	do_ioctl(fd, DRM_IOCTL_V3D_GET_BO_OFFSET, &get);

	return get.offset;
}

uint32_t
igt_v3d_get_param(int fd, enum drm_v3d_param param)
{
	struct drm_v3d_get_param get = {
		.param = param,
	};

	do_ioctl(fd, DRM_IOCTL_V3D_GET_PARAM, &get);

	return get.value;
}

void *
igt_v3d_mmap_bo(int fd, uint32_t handle, uint32_t size, unsigned prot)
{
	struct drm_v3d_mmap_bo mmap_bo = {
		.handle = handle,
	};
	void *ptr;

	do_ioctl(fd, DRM_IOCTL_V3D_MMAP_BO, &mmap_bo);

	ptr = mmap(0, size, prot, MAP_SHARED, fd, mmap_bo.offset);
	if (ptr == MAP_FAILED)
		return NULL;
	else
		return ptr;
}

void igt_v3d_bo_mmap(int fd, struct v3d_bo *bo)
{
	bo->map = igt_v3d_mmap_bo(fd, bo->handle, bo->size,
				  PROT_READ | PROT_WRITE);
	igt_assert(bo->map);
}
