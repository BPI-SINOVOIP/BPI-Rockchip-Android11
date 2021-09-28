/*
 * Copyright © 2007, 2011, 2013, 2014, 2019 Intel Corporation
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

#include <stdbool.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "igt_core.h"
#include "ioctl_wrappers.h"

#include "gem_mman.h"

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#define VG(x) x
#else
#define VG(x) do {} while (0)
#endif

/**
 * __gem_mmap__gtt:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @size: size of the gem buffer
 * @prot: memory protection bits as used by mmap()
 *
 * This functions wraps up procedure to establish a memory mapping through the
 * GTT.
 *
 * Returns: A pointer to the created memory mapping, NULL on failure.
 */
void *__gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot)
{
	struct drm_i915_gem_mmap_gtt mmap_arg;
	void *ptr;

	memset(&mmap_arg, 0, sizeof(mmap_arg));
	mmap_arg.handle = handle;
	if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_arg))
		return NULL;

	ptr = mmap64(0, size, prot, MAP_SHARED, fd, mmap_arg.offset);
	if (ptr == MAP_FAILED)
		ptr = NULL;
	else
		errno = 0;

	VG(VALGRIND_MAKE_MEM_DEFINED(ptr, size));

	return ptr;
}

/**
 * gem_mmap__gtt:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @size: size of the gem buffer
 * @prot: memory protection bits as used by mmap()
 *
 * Like __gem_mmap__gtt() except we assert on failure.
 *
 * Returns: A pointer to the created memory mapping
 */
void *gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot)
{
	void *ptr = __gem_mmap__gtt(fd, handle, size, prot);
	igt_assert(ptr);
	return ptr;
}

int gem_munmap(void *ptr, uint64_t size)
{
	int ret = munmap(ptr, size);

	if (ret == 0)
		VG(VALGRIND_MAKE_MEM_NOACCESS(ptr, size));

	return ret;
}

bool gem_mmap__has_wc(int fd)
{
	static int has_wc = -1;

	if (has_wc == -1) {
		struct drm_i915_getparam gp;
		int mmap_version = -1;
		int gtt_version = -1;

		has_wc = 0;

		memset(&gp, 0, sizeof(gp));
		gp.param = I915_PARAM_MMAP_GTT_VERSION;
		gp.value = &gtt_version;
		ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);

		memset(&gp, 0, sizeof(gp));
		gp.param = I915_PARAM_MMAP_VERSION;
		gp.value = &mmap_version;
		ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);

		/* Do we have the new mmap_ioctl with DOMAIN_WC? */
		if (mmap_version >= 1 && gtt_version >= 2) {
			struct drm_i915_gem_mmap arg;

			/* Does this device support wc-mmaps ? */
			memset(&arg, 0, sizeof(arg));
			arg.handle = gem_create(fd, 4096);
			arg.offset = 0;
			arg.size = 4096;
			arg.flags = I915_MMAP_WC;
			has_wc = igt_ioctl(fd, DRM_IOCTL_I915_GEM_MMAP, &arg) == 0;
			gem_close(fd, arg.handle);
		}
		errno = 0;
	}

	return has_wc > 0;
}

/**
 * __gem_mmap:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 * @flags: flags used to determine caching
 *
 * This functions wraps up procedure to establish a memory mapping through
 * direct cpu access, bypassing the gpu (valid for wc == false). For wc == true
 * it also bypass cpu caches completely and GTT system agent (i.e. there is no
 * automatic tiling of the mmapping through the fence registers).
 *
 * Returns: A pointer to the created memory mapping, NULL on failure.
 */
static void
*__gem_mmap(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned int prot, uint64_t flags)
{
	struct drm_i915_gem_mmap arg;

	memset(&arg, 0, sizeof(arg));
	arg.handle = handle;
	arg.offset = offset;
	arg.size = size;
	arg.flags = flags;

	if (igt_ioctl(fd, DRM_IOCTL_I915_GEM_MMAP, &arg))
		return NULL;

	VG(VALGRIND_MAKE_MEM_DEFINED(from_user_pointer(arg.addr_ptr), arg.size));

	errno = 0;
	return from_user_pointer(arg.addr_ptr);
}

/**
 * __gem_mmap__wc:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 *
 * This functions wraps up procedure to establish a memory mapping through
 * direct cpu access, bypassing the gpu and cpu caches completely and also
 * bypassing the GTT system agent (i.e. there is no automatic tiling of
 * the mmapping through the fence registers).
 *
 * Returns: A pointer to the created memory mapping, NULL on failure.
 */
void *__gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
	return __gem_mmap(fd, handle, offset, size, prot, I915_MMAP_WC);
}

/**
 * gem_mmap__wc:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 *
 * Like __gem_mmap__wc() except we assert on failure.
 *
 * Returns: A pointer to the created memory mapping
 */
void *gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
	void *ptr = __gem_mmap__wc(fd, handle, offset, size, prot);
	igt_assert(ptr);
	return ptr;
}

/**
 * __gem_mmap__cpu:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 *
 * This functions wraps up procedure to establish a memory mapping through
 * direct cpu access, bypassing the gpu completely.
 *
 * Returns: A pointer to the created memory mapping, NULL on failure.
 */
void *__gem_mmap__cpu(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
	return __gem_mmap(fd, handle, offset, size, prot, 0);
}

/**
 * gem_mmap__cpu:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 *
 * Like __gem_mmap__cpu() except we assert on failure.
 *
 * Returns: A pointer to the created memory mapping
 */
void *gem_mmap__cpu(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
	void *ptr = __gem_mmap__cpu(fd, handle, offset, size, prot);
	igt_assert(ptr);
	return ptr;
}
