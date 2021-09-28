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
 */

#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <errno.h>

#include "igt_vgem.h"
#include "igt_core.h"
#include "ioctl_wrappers.h"

/**
 * SECTION:igt_vgem
 * @short_description: VGEM support library
 * @title: VGEM
 * @include: igt.h
 *
 * This library provides various auxiliary helper functions for writing VGEM
 * tests. VGEM is especially useful as a virtual dma-buf import and exporter and
 * for testing cross driver synchronization (either using epxlicit dma-fences or
 * using implicit fences attached to dma-bufs).
 */

int __vgem_create(int fd, struct vgem_bo *bo)
{
	struct drm_mode_create_dumb arg;

	memset(&arg, 0, sizeof(arg));
	arg.width = bo->width;
	arg.height = bo->height;
	arg.bpp = bo->bpp;

	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg))
		return -errno;

	bo->handle = arg.handle;
	bo->pitch = arg.pitch;
	bo->size = arg.size;

	return 0;
}

void vgem_create(int fd, struct vgem_bo *bo)
{
	igt_assert_eq(__vgem_create(fd, bo), 0);
}

void *__vgem_mmap(int fd, struct vgem_bo *bo, unsigned prot)
{
	struct drm_mode_map_dumb arg;
	void *ptr;

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->handle;
	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &arg))
		return NULL;

	ptr = mmap64(0, bo->size, prot, MAP_SHARED, fd, arg.offset);
	if (ptr == MAP_FAILED)
		return NULL;

	return ptr;
}

void *vgem_mmap(int fd, struct vgem_bo *bo, unsigned prot)
{
	void *ptr;

	igt_assert_f((ptr = __vgem_mmap(fd, bo, prot)),
		     "vgem_map(fd=%d, bo->handle=%d, prot=%x)\n",
		     fd, bo->handle, prot);

	return ptr;
}

#define LOCAL_VGEM_FENCE_ATTACH   0x1
#define LOCAL_VGEM_FENCE_SIGNAL   0x2

#define LOCAL_IOCTL_VGEM_FENCE_ATTACH     DRM_IOWR( DRM_COMMAND_BASE + LOCAL_VGEM_FENCE_ATTACH, struct local_vgem_fence_attach)
#define LOCAL_IOCTL_VGEM_FENCE_SIGNAL     DRM_IOW( DRM_COMMAND_BASE + LOCAL_VGEM_FENCE_SIGNAL, struct local_vgem_fence_signal)

struct local_vgem_fence_attach {
	uint32_t handle;
	uint32_t flags;
	uint32_t out_fence;
	uint32_t pad;
};

struct local_vgem_fence_signal {
	uint32_t fence;
	uint32_t flags;
};

bool vgem_has_fences(int fd)
{
	struct local_vgem_fence_signal arg;
	int err;

	err = 0;
	memset(&arg, 0, sizeof(arg));
	if (drmIoctl(fd, LOCAL_IOCTL_VGEM_FENCE_SIGNAL, &arg))
		err = -errno;
	errno = 0;
	return err == -ENOENT;
}

static int __vgem_fence_attach(int fd, struct local_vgem_fence_attach *arg)
{
	int err = 0;
	if (igt_ioctl(fd, LOCAL_IOCTL_VGEM_FENCE_ATTACH, arg))
		err = -errno;
	errno = 0;
	return err;
}

bool vgem_fence_has_flag(int fd, unsigned flags)
{
	struct local_vgem_fence_attach arg;
	struct vgem_bo bo;
	bool ret = false;

	memset(&bo, 0, sizeof(bo));
	bo.width = 1;
	bo.height = 1;
	bo.bpp = 32;
	vgem_create(fd, &bo);

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo.handle;
	arg.flags = flags;
	if (__vgem_fence_attach(fd, &arg) == 0) {
		vgem_fence_signal(fd, arg.out_fence);
		ret = true;
	}
	gem_close(fd, bo.handle);

	return ret;
}

uint32_t vgem_fence_attach(int fd, struct vgem_bo *bo, unsigned flags)
{
	struct local_vgem_fence_attach arg;

	memset(&arg, 0, sizeof(arg));
	arg.handle = bo->handle;
	arg.flags = flags;
	igt_assert_eq(__vgem_fence_attach(fd, &arg), 0);
	return arg.out_fence;
}

static int ioctl_vgem_fence_signal(int fd, struct local_vgem_fence_signal *arg)
{
	int err = 0;
	if (igt_ioctl(fd, LOCAL_IOCTL_VGEM_FENCE_SIGNAL, arg))
		err = -errno;
	errno = 0;
	return err;
}

int __vgem_fence_signal(int fd, uint32_t fence)
{
	struct local_vgem_fence_signal arg;

	memset(&arg, 0, sizeof(arg));
	arg.fence = fence;

	return ioctl_vgem_fence_signal(fd, &arg);
}

void vgem_fence_signal(int fd, uint32_t fence)
{
	igt_assert_eq(__vgem_fence_signal(fd, fence), 0);
}
