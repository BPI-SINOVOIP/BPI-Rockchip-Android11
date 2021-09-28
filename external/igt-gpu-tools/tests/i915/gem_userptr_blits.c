/*
 * Copyright © 2009-2014 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *    Tvrtko Ursulin <tvrtko.ursulin@intel.com>
 *
 */

/** @file gem_userptr_blits.c
 *
 * This is a test of doing many blits using a mixture of normal system pages
 * and uncached linear buffers, with a working set larger than the
 * aperture size.
 *
 * The goal is to simply ensure the basics work.
 */

#include "igt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <glib.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include <linux/memfd.h>

#include "drm.h"
#include "i915_drm.h"

#include "intel_bufmgr.h"

#include "eviction_common.c"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define LOCAL_EXEC_OBJECT_SUPPORTS_48B (1 << 3)

static uint32_t userptr_flags = LOCAL_I915_USERPTR_UNSYNCHRONIZED;

#define WIDTH 512
#define HEIGHT 512

static uint32_t linear[WIDTH*HEIGHT];

static void gem_userptr_test_unsynchronized(void)
{
	userptr_flags = LOCAL_I915_USERPTR_UNSYNCHRONIZED;
}

static void gem_userptr_test_synchronized(void)
{
	userptr_flags = 0;
}

static void gem_userptr_sync(int fd, uint32_t handle)
{
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);
}

static int copy(int fd, uint32_t dst, uint32_t src)
{
	uint32_t batch[12];
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_exec_object2 obj[3];
	struct drm_i915_gem_execbuffer2 exec;
	uint32_t handle;
	int ret, i=0;

	batch[i++] = XY_SRC_COPY_BLT_CMD |
		  XY_SRC_COPY_BLT_WRITE_ALPHA |
		  XY_SRC_COPY_BLT_WRITE_RGB;
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i - 1] |= 8;
	else
		batch[i - 1] |= 6;

	batch[i++] = (3 << 24) | /* 32 bits */
		  (0xcc << 16) | /* copy ROP */
		  WIDTH*4;
	batch[i++] = 0; /* dst x1,y1 */
	batch[i++] = (HEIGHT << 16) | WIDTH; /* dst x2,y2 */
	batch[i++] = 0; /* dst reloc */
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i++] = 0;
	batch[i++] = 0; /* src x1,y1 */
	batch[i++] = WIDTH*4;
	batch[i++] = 0; /* src reloc */
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i++] = 0;
	batch[i++] = MI_BATCH_BUFFER_END;
	batch[i++] = MI_NOOP;

	handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, batch, sizeof(batch));

	reloc[0].target_handle = dst;
	reloc[0].delta = 0;
	reloc[0].offset = 4 * sizeof(batch[0]);
	reloc[0].presumed_offset = 0;
	reloc[0].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[0].write_domain = I915_GEM_DOMAIN_RENDER;

	reloc[1].target_handle = src;
	reloc[1].delta = 0;
	reloc[1].offset = 7 * sizeof(batch[0]);
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		reloc[1].offset += sizeof(batch[0]);
	reloc[1].presumed_offset = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[1].write_domain = 0;

	memset(&exec, 0, sizeof(exec));
	memset(obj, 0, sizeof(obj));

	obj[exec.buffer_count].handle = dst;
	obj[exec.buffer_count].flags = LOCAL_EXEC_OBJECT_SUPPORTS_48B;
	exec.buffer_count++;

	if (src != dst) {
		obj[exec.buffer_count].handle = src;
		obj[exec.buffer_count].flags = LOCAL_EXEC_OBJECT_SUPPORTS_48B;
		exec.buffer_count++;
	}

	obj[exec.buffer_count].handle = handle;
	obj[exec.buffer_count].relocation_count = 2;
	obj[exec.buffer_count].relocs_ptr = to_user_pointer(reloc);
	obj[exec.buffer_count].flags = LOCAL_EXEC_OBJECT_SUPPORTS_48B;
	exec.buffer_count++;
	exec.buffers_ptr = to_user_pointer(obj);
	exec.flags = HAS_BLT_RING(intel_get_drm_devid(fd)) ? I915_EXEC_BLT : 0;

	ret = __gem_execbuf(fd, &exec);
	gem_close(fd, handle);

	return ret;
}

static int
blit(int fd, uint32_t dst, uint32_t src, uint32_t *all_bo, int n_bo)
{
	uint32_t batch[12];
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_exec_object2 *obj;
	struct drm_i915_gem_execbuffer2 exec;
	uint32_t handle;
	int n, ret, i=0;

	batch[i++] = XY_SRC_COPY_BLT_CMD |
		  XY_SRC_COPY_BLT_WRITE_ALPHA |
		  XY_SRC_COPY_BLT_WRITE_RGB;
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i - 1] |= 8;
	else
		batch[i - 1] |= 6;
	batch[i++] = (3 << 24) | /* 32 bits */
		  (0xcc << 16) | /* copy ROP */
		  WIDTH*4;
	batch[i++] = 0; /* dst x1,y1 */
	batch[i++] = (HEIGHT << 16) | WIDTH; /* dst x2,y2 */
	batch[i++] = 0; /* dst reloc */
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i++] = 0;
	batch[i++] = 0; /* src x1,y1 */
	batch[i++] = WIDTH*4;
	batch[i++] = 0; /* src reloc */
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		batch[i++] = 0;
	batch[i++] = MI_BATCH_BUFFER_END;
	batch[i++] = MI_NOOP;

	handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, batch, sizeof(batch));

	reloc[0].target_handle = dst;
	reloc[0].delta = 0;
	reloc[0].offset = 4 * sizeof(batch[0]);
	reloc[0].presumed_offset = 0;
	reloc[0].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[0].write_domain = I915_GEM_DOMAIN_RENDER;

	reloc[1].target_handle = src;
	reloc[1].delta = 0;
	reloc[1].offset = 7 * sizeof(batch[0]);
	if (intel_gen(intel_get_drm_devid(fd)) >= 8)
		reloc[1].offset += sizeof(batch[0]);
	reloc[1].presumed_offset = 0;
	reloc[1].read_domains = I915_GEM_DOMAIN_RENDER;
	reloc[1].write_domain = 0;

	memset(&exec, 0, sizeof(exec));
	obj = calloc(n_bo + 1, sizeof(*obj));
	for (n = 0; n < n_bo; n++) {
		obj[n].handle = all_bo[n];
		obj[n].flags = LOCAL_EXEC_OBJECT_SUPPORTS_48B;
	}
	obj[n].handle = handle;
	obj[n].flags = LOCAL_EXEC_OBJECT_SUPPORTS_48B;
	obj[n].relocation_count = 2;
	obj[n].relocs_ptr = to_user_pointer(reloc);

	exec.buffers_ptr = to_user_pointer(obj);
	exec.buffer_count = n_bo + 1;
	exec.flags = HAS_BLT_RING(intel_get_drm_devid(fd)) ? I915_EXEC_BLT : 0;

	ret = __gem_execbuf(fd, &exec);
	gem_close(fd, handle);
	free(obj);

	return ret;
}

static void store_dword(int fd, uint32_t target,
			uint32_t offset, uint32_t value)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t batch[16];
	int i;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = ARRAY_SIZE(obj);
	execbuf.flags = 0;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(obj, 0, sizeof(obj));
	obj[0].handle = target;
	obj[1].handle = gem_create(fd, 4096);

	memset(&reloc, 0, sizeof(reloc));
	reloc.target_handle = obj[0].handle;
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = offset;
	reloc.read_domains = I915_GEM_DOMAIN_RENDER;
	reloc.write_domain = I915_GEM_DOMAIN_RENDER;
	obj[1].relocs_ptr = to_user_pointer(&reloc);
	obj[1].relocation_count = 1;

	i = 0;
	batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
	if (gen >= 8) {
		batch[++i] = offset;
		batch[++i] = 0;
	} else if (gen >= 4) {
		batch[++i] = 0;
		batch[++i] = offset;
		reloc.offset += sizeof(uint32_t);
	} else {
		batch[i]--;
		batch[++i] = offset;
	}
	batch[++i] = value;
	batch[++i] = MI_BATCH_BUFFER_END;
	gem_write(fd, obj[1].handle, 0, batch, sizeof(batch));
	gem_execbuf(fd, &execbuf);
	gem_close(fd, obj[1].handle);
}

static uint32_t
create_userptr(int fd, uint32_t val, uint32_t *ptr)
{
	uint32_t handle;
	int i;

	gem_userptr(fd, ptr, sizeof(linear), 0, userptr_flags, &handle);
	igt_assert(handle != 0);

	/* Fill the BO with dwords starting at val */
	for (i = 0; i < WIDTH*HEIGHT; i++)
		ptr[i] = val++;

	return handle;
}

static void **handle_ptr_map;
static unsigned *handle_size_map;
static unsigned int num_handle_map;

static void reset_handle_ptr(void)
{
	if (num_handle_map == 0)
		return;

	free(handle_ptr_map);
	handle_ptr_map = NULL;

	free(handle_size_map);
	handle_size_map = NULL;

	num_handle_map = 0;
}

static void add_handle_ptr(uint32_t handle, void *ptr, int size)
{
	if (handle >= num_handle_map) {
		int max = (4096 + handle) & -4096;

		handle_ptr_map = realloc(handle_ptr_map,
					 max * sizeof(void*));
		igt_assert(handle_ptr_map);
		memset(handle_ptr_map + num_handle_map, 0,
		       (max - num_handle_map) * sizeof(void*));

		handle_size_map = realloc(handle_size_map,
					  max * sizeof(unsigned));
		igt_assert(handle_size_map);
		memset(handle_ptr_map + num_handle_map, 0,
		       (max - num_handle_map) * sizeof(unsigned));

		num_handle_map = max;
	}

	handle_ptr_map[handle] = ptr;
	handle_size_map[handle] = size;
}

static void *get_handle_ptr(uint32_t handle)
{
	igt_assert(handle < num_handle_map);
	return handle_ptr_map[handle];
}

static void free_handle_ptr(uint32_t handle)
{
	igt_assert(handle < num_handle_map);
	igt_assert(handle_ptr_map[handle]);

	munmap(handle_ptr_map[handle], handle_size_map[handle]);
	handle_ptr_map[handle] = NULL;
}

static uint32_t create_userptr_bo(int fd, uint64_t size)
{
	void *ptr;
	uint32_t handle;

	ptr = mmap(NULL, size,
		   PROT_READ | PROT_WRITE,
		   MAP_ANONYMOUS | MAP_SHARED,
		   -1, 0);
	igt_assert(ptr != MAP_FAILED);

	gem_userptr(fd, (uint32_t *)ptr, size, 0, userptr_flags, &handle);
	add_handle_ptr(handle, ptr, size);

	return handle;
}

static void flink_userptr_bo(uint32_t old_handle, uint32_t new_handle)
{
	igt_assert(old_handle < num_handle_map);
	igt_assert(handle_ptr_map[old_handle]);

	add_handle_ptr(new_handle,
		       handle_ptr_map[old_handle],
		       handle_size_map[old_handle]);
}

static void clear(int fd, uint32_t handle, uint64_t size)
{
	void *ptr = get_handle_ptr(handle);

	igt_assert(ptr != NULL);

	memset(ptr, 0, size);
}

static void free_userptr_bo(int fd, uint32_t handle)
{
	gem_close(fd, handle);
	free_handle_ptr(handle);
}

static uint32_t
create_bo(int fd, uint32_t val)
{
	uint32_t handle;
	int i;

	handle = gem_create(fd, sizeof(linear));

	/* Fill the BO with dwords starting at val */
	for (i = 0; i < WIDTH*HEIGHT; i++)
		linear[i] = val++;
	gem_write(fd, handle, 0, linear, sizeof(linear));

	return handle;
}

static void
check_cpu(uint32_t *ptr, uint32_t val)
{
	int i;

	for (i = 0; i < WIDTH*HEIGHT; i++) {
		igt_assert_f(ptr[i] == val,
			     "Expected 0x%08x, found 0x%08x "
			     "at offset 0x%08x\n",
			     val, ptr[i], i * 4);
		val++;
	}
}

static void
check_gpu(int fd, uint32_t handle, uint32_t val)
{
	gem_read(fd, handle, 0, linear, sizeof(linear));
	check_cpu(linear, val);
}

static int has_userptr(int fd)
{
	uint32_t handle = 0;
	void *ptr;
	uint32_t oldflags;
	int ret;

	igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE) == 0);
	oldflags = userptr_flags;
	gem_userptr_test_unsynchronized();
	ret = __gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle);
	userptr_flags = oldflags;
	if (ret != 0) {
		free(ptr);
		return 0;
	}

	gem_close(fd, handle);
	free(ptr);

	return handle != 0;
}

static int test_input_checking(int fd)
{
	struct local_i915_gem_userptr userptr;
	int ret;

	/* Invalid flags. */
	memset(&userptr, 0, sizeof(userptr));
	userptr.user_ptr = 0;
	userptr.user_size = 0;
	userptr.flags = ~0;
	ret = drmIoctl(fd, LOCAL_IOCTL_I915_GEM_USERPTR, &userptr);
	igt_assert_neq(ret, 0);

	/* Too big. */
	memset(&userptr, 0, sizeof(userptr));
	userptr.user_ptr = 0;
	userptr.user_size = ~0;
	userptr.flags = 0;
	ret = drmIoctl(fd, LOCAL_IOCTL_I915_GEM_USERPTR, &userptr);
	igt_assert_neq(ret, 0);

	/* Both wrong. */
	memset(&userptr, 0, sizeof(userptr));
	userptr.user_ptr = 0;
	userptr.user_size = ~0;
	userptr.flags = ~0;
	ret = drmIoctl(fd, LOCAL_IOCTL_I915_GEM_USERPTR, &userptr);
	igt_assert_neq(ret, 0);

	/* Zero user_size. */
	memset(&userptr, 0, sizeof(userptr));
	userptr.user_ptr = 0;
	userptr.user_size = 0;
	userptr.flags = 0;
	ret = drmIoctl(fd, LOCAL_IOCTL_I915_GEM_USERPTR, &userptr);
	igt_assert_neq(ret, 0);

	return 0;
}

static int test_access_control(int fd)
{
	/* CAP_SYS_ADMIN is needed for UNSYNCHRONIZED mappings. */
	gem_userptr_test_unsynchronized();
	igt_require(has_userptr(fd));

	igt_fork(child, 1) {
		void *ptr;
		int ret;
		uint32_t handle;

		igt_drop_root();

		igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE) == 0);

		ret = __gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle);
		if (ret == 0)
			gem_close(fd, handle);
		free(ptr);
		igt_assert_eq(ret, -EPERM);
	}

	igt_waitchildren();

	return 0;
}

static int test_invalid_null_pointer(int fd)
{
	uint32_t handle;

	/* NULL pointer. */
	gem_userptr(fd, NULL, PAGE_SIZE, 0, userptr_flags, &handle);

	igt_assert_neq(copy(fd, handle, handle), 0); /* QQQ Precise errno? */
	gem_close(fd, handle);

	return 0;
}

static int test_invalid_gtt_mapping(int fd)
{
	struct drm_i915_gem_mmap_gtt arg;
	uint32_t handle;
	char *gtt, *map;

	/* Anonymous mapping to find a hole */
	map = mmap(NULL, sizeof(linear) + 2 * PAGE_SIZE,
		   PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS,
		   -1, 0);
	igt_assert(map != MAP_FAILED);

	gem_userptr(fd, map, sizeof(linear) + 2 * PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), 0);
	gem_close(fd, handle);

	gem_userptr(fd, map, PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), 0);
	gem_close(fd, handle);

	gem_userptr(fd, map + sizeof(linear) + PAGE_SIZE, PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), 0);
	gem_close(fd, handle);

	/* GTT mapping */
	memset(&arg, 0, sizeof(arg));
	arg.handle = create_bo(fd, 0);
	do_ioctl(fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &arg);
	gtt = mmap(map + PAGE_SIZE, sizeof(linear),
		   PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_FIXED,
		   fd, arg.offset);
	igt_assert(gtt == map + PAGE_SIZE);
	gem_close(fd, arg.handle);
	igt_assert(((unsigned long)gtt & (PAGE_SIZE - 1)) == 0);
	igt_assert((sizeof(linear) & (PAGE_SIZE - 1)) == 0);

	gem_userptr(fd, gtt, sizeof(linear), 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), -EFAULT);
	gem_close(fd, handle);

	gem_userptr(fd, gtt, PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), -EFAULT);
	gem_close(fd, handle);

	gem_userptr(fd, gtt + sizeof(linear) - PAGE_SIZE, PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), -EFAULT);
	gem_close(fd, handle);

	/* boundaries */
	gem_userptr(fd, map, 2*PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), -EFAULT);
	gem_close(fd, handle);

	gem_userptr(fd, map + sizeof(linear), 2*PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_eq(copy(fd, handle, handle), -EFAULT);
	gem_close(fd, handle);

	munmap(map, sizeof(linear) + 2*PAGE_SIZE);

	return 0;
}

#define PE_GTT_MAP 0x1
#define PE_BUSY 0x2
static void test_process_exit(int fd, int flags)
{
	if (flags & PE_GTT_MAP)
		igt_require(gem_has_llc(fd));

	igt_fork(child, 1) {
		uint32_t handle;

		handle = create_userptr_bo(fd, sizeof(linear));

		if (flags & PE_GTT_MAP) {
			uint32_t *ptr = __gem_mmap__gtt(fd, handle, sizeof(linear), PROT_READ | PROT_WRITE);
			if (ptr)
				*ptr = 0;
		}

		if (flags & PE_BUSY)
			igt_assert_eq(copy(fd, handle, handle), 0);
	}
	igt_waitchildren();
}

static void test_forked_access(int fd)
{
	uint32_t handle1 = 0, handle2 = 0;
	void *ptr1 = NULL, *ptr2 = NULL;
	int ret;

	ret = posix_memalign(&ptr1, PAGE_SIZE, sizeof(linear));
#ifdef MADV_DONTFORK
	ret |= madvise(ptr1, sizeof(linear), MADV_DONTFORK);
#endif
	gem_userptr(fd, ptr1, sizeof(linear), 0, userptr_flags, &handle1);
	igt_assert(ptr1);
	igt_assert(handle1);

	ret = posix_memalign(&ptr2, PAGE_SIZE, sizeof(linear));
#ifdef MADV_DONTFORK
	ret |= madvise(ptr2, sizeof(linear), MADV_DONTFORK);
#endif
	gem_userptr(fd, ptr2, sizeof(linear), 0, userptr_flags, &handle2);
	igt_assert(ptr2);
	igt_assert(handle2);

	memset(ptr1, 0x1, sizeof(linear));
	memset(ptr2, 0x2, sizeof(linear));

	igt_fork(child, 1)
		igt_assert_eq(copy(fd, handle1, handle2), 0);
	igt_waitchildren();

	gem_userptr_sync(fd, handle1);
	gem_userptr_sync(fd, handle2);

	gem_close(fd, handle1);
	gem_close(fd, handle2);

	igt_assert(memcmp(ptr1, ptr2, sizeof(linear)) == 0);

#ifdef MADV_DOFORK
	ret = madvise(ptr1, sizeof(linear), MADV_DOFORK);
	igt_assert_eq(ret, 0);
#endif
	free(ptr1);

#ifdef MADV_DOFORK
	ret = madvise(ptr2, sizeof(linear), MADV_DOFORK);
	igt_assert_eq(ret, 0);
#endif
	free(ptr2);
}

#define MAP_FIXED_INVALIDATE_OVERLAP	(1<<0)
#define MAP_FIXED_INVALIDATE_BUSY	(1<<1)
#define MAP_FIXED_INVALIDATE_GET_PAGES	(1<<2)
#define ALL_MAP_FIXED_INVALIDATE (MAP_FIXED_INVALIDATE_OVERLAP | \
				  MAP_FIXED_INVALIDATE_BUSY | \
				  MAP_FIXED_INVALIDATE_GET_PAGES)

static int test_map_fixed_invalidate(int fd, uint32_t flags)
{
	const size_t ptr_size = sizeof(linear) + 2*PAGE_SIZE;
	const int num_handles = (flags & MAP_FIXED_INVALIDATE_OVERLAP) ? 2 : 1;
	uint32_t handle[num_handles];
	uint32_t *ptr;

	ptr = mmap(NULL, ptr_size,
		   PROT_READ | PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS,
		   -1, 0);
	igt_assert(ptr != MAP_FAILED);

	for (int i = 0; i < num_handles; i++)
		handle[i] = create_userptr(fd, 0, ptr + PAGE_SIZE/sizeof(*ptr));

	for (char *fixed = (char *)ptr, *end = fixed + ptr_size;
	     fixed + 2*PAGE_SIZE <= end;
	     fixed += PAGE_SIZE) {
		struct drm_i915_gem_mmap_gtt mmap_gtt;
		uint32_t *map;

		map = mmap(ptr, ptr_size, PROT_READ | PROT_WRITE,
			   MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
			   -1, 0);
		igt_assert(map != MAP_FAILED);
		igt_assert(map == ptr);

		memset(&mmap_gtt, 0, sizeof(mmap_gtt));
		mmap_gtt.handle = gem_create(fd, 2*PAGE_SIZE);
		do_ioctl(fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_gtt);

		if (flags & MAP_FIXED_INVALIDATE_GET_PAGES)
			igt_assert_eq(__gem_set_domain(fd, handle[0],
						       I915_GEM_DOMAIN_GTT,
						       I915_GEM_DOMAIN_GTT),
				      0);

		if (flags & MAP_FIXED_INVALIDATE_BUSY)
			igt_assert_eq(copy(fd, handle[0], handle[num_handles-1]), 0);

		map = mmap(fixed, 2*PAGE_SIZE,
			   PROT_READ | PROT_WRITE,
			   MAP_SHARED | MAP_FIXED,
			   fd, mmap_gtt.offset);
		igt_assert(map != MAP_FAILED);
		igt_assert(map == (uint32_t *)fixed);

		gem_set_tiling(fd, mmap_gtt.handle, I915_TILING_NONE, 0);
		*map = 0xdead;

		if (flags & MAP_FIXED_INVALIDATE_GET_PAGES) {
			igt_assert_eq(__gem_set_domain(fd, handle[0],
						       I915_GEM_DOMAIN_GTT,
						       I915_GEM_DOMAIN_GTT),
				      -EFAULT);

			/* Errors are permanent, so we have to recreate */
			gem_close(fd, handle[0]);
			handle[0] = create_userptr(fd, 0, ptr + PAGE_SIZE/sizeof(*ptr));
		}

		gem_set_tiling(fd, mmap_gtt.handle, I915_TILING_Y, 512 * 4);
		*(uint32_t*)map = 0xbeef;

		gem_close(fd, mmap_gtt.handle);
	}

	for (int i = 0; i < num_handles; i++)
		gem_close(fd, handle[i]);
	munmap(ptr, ptr_size);

	return 0;
}

static int test_forbidden_ops(int fd)
{
	struct drm_i915_gem_pread gem_pread;
	struct drm_i915_gem_pwrite gem_pwrite;
	uint32_t handle;
	void *ptr;

	igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE) == 0);
	gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle);

	/* pread/pwrite are not always forbidden, but when they
	 * are they should fail with EINVAL.
	 */

	memset(&gem_pread, 0, sizeof(gem_pread));
	gem_pread.handle = handle;
	gem_pread.offset = 0;
	gem_pread.size = PAGE_SIZE;
	gem_pread.data_ptr = to_user_pointer(ptr);
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_PREAD, &gem_pread))
		igt_assert_eq(errno, EINVAL);

	memset(&gem_pwrite, 0, sizeof(gem_pwrite));
	gem_pwrite.handle = handle;
	gem_pwrite.offset = 0;
	gem_pwrite.size = PAGE_SIZE;
	gem_pwrite.data_ptr = to_user_pointer(ptr);
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite))
		igt_assert_eq(errno, EINVAL);

	gem_close(fd, handle);
	free(ptr);

	return 0;
}

static void test_relocations(int fd)
{
	struct drm_i915_gem_relocation_entry *reloc;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 exec;
	unsigned size;
	void *ptr;
	int i;

	size = PAGE_SIZE + ALIGN(sizeof(*reloc)*256, PAGE_SIZE);

	memset(&obj, 0, sizeof(obj));
	igt_assert(posix_memalign(&ptr, PAGE_SIZE, size) == 0);
	gem_userptr(fd, ptr, size, 0, userptr_flags, &obj.handle);
	if (!gem_has_llc(fd))
		gem_set_caching(fd, obj.handle, 0);
	*(uint32_t *)ptr = MI_BATCH_BUFFER_END;

	reloc = (typeof(reloc))((char *)ptr + PAGE_SIZE);
	obj.relocs_ptr = to_user_pointer(reloc);
	obj.relocation_count = 256;

	memset(reloc, 0, 256*sizeof(*reloc));
	for (i = 0; i < 256; i++) {
		reloc[i].offset = 2048 - 4*i;
		reloc[i].target_handle = obj.handle;
		reloc[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	}

	memset(&exec, 0, sizeof(exec));
	exec.buffers_ptr = to_user_pointer(&obj);
	exec.buffer_count = 1;
	gem_execbuf(fd, &exec);

	gem_sync(fd, obj.handle);
	gem_close(fd, obj.handle);
	free(ptr);
}

static unsigned char counter;

static void (* volatile orig_sigbus)(int sig, siginfo_t *info, void *param);
static volatile unsigned long sigbus_start;
static volatile long sigbus_cnt = -1;

static void *umap(int fd, uint32_t handle)
{
	void *ptr;

	if (gem_has_llc(fd)) {
		ptr = gem_mmap__gtt(fd, handle, sizeof(linear),
				    PROT_READ | PROT_WRITE);
	} else {
		uint32_t tmp = gem_create(fd, sizeof(linear));
		igt_assert_eq(copy(fd, tmp, handle), 0);
		ptr = gem_mmap__cpu(fd, tmp, 0, sizeof(linear), PROT_READ);
		gem_close(fd, tmp);
	}

	return ptr;
}

static void
check_bo(int fd1, uint32_t handle1, int is_userptr, int fd2, uint32_t handle2)
{
	unsigned char *ptr1, *ptr2;
	unsigned long size = sizeof(linear);

	ptr2 = umap(fd2, handle2);
	if (is_userptr)
		ptr1 = is_userptr > 0 ? get_handle_ptr(handle1) : ptr2;
	else
		ptr1 = umap(fd1, handle1);

	igt_assert(ptr1);
	igt_assert(ptr2);

	sigbus_start = (unsigned long)ptr2;
	igt_assert(memcmp(ptr1, ptr2, sizeof(linear)) == 0);

	if (gem_has_llc(fd1)) {
		counter++;
		memset(ptr1, counter, size);
		memset(ptr2, counter, size);
	}

	if (!is_userptr)
		munmap(ptr1, sizeof(linear));
	munmap(ptr2, sizeof(linear));
}

static int export_handle(int fd, uint32_t handle, int *outfd)
{
	struct drm_prime_handle args;
	int ret;

	args.handle = handle;
	args.flags = DRM_CLOEXEC;
	args.fd = -1;

	ret = drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &args);
	if (ret)
		ret = errno;
	*outfd = args.fd;

	return ret;
}

static void sigbus(int sig, siginfo_t *info, void *param)
{
	unsigned long ptr = (unsigned long)info->si_addr;
	void *addr;

	if (ptr >= sigbus_start &&
	    ptr < sigbus_start + sizeof(linear)) {
		/* replace mapping to allow progress */
		munmap((void *)sigbus_start, sizeof(linear));
		addr = mmap((void *)sigbus_start, sizeof(linear),
			    PROT_READ | PROT_WRITE,
			    MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
		igt_assert((unsigned long)addr == sigbus_start);
		memset(addr, counter, sizeof(linear));

		sigbus_cnt++;
		return;
	}

	if (orig_sigbus)
		orig_sigbus(sig, info, param);
	igt_assert(0);
}

static int test_dmabuf(void)
{
	int fd1, fd2;
	uint32_t handle, handle_import;
	int dma_buf_fd = -1;
	int ret;

	fd1 = drm_open_driver(DRIVER_INTEL);

	handle = create_userptr_bo(fd1, sizeof(linear));
	memset(get_handle_ptr(handle), counter, sizeof(linear));

	ret = export_handle(fd1, handle, &dma_buf_fd);
	if (userptr_flags & LOCAL_I915_USERPTR_UNSYNCHRONIZED && ret) {
		igt_assert(ret == EINVAL || ret == ENODEV);
		free_userptr_bo(fd1, handle);
		close(fd1);
		return 0;
	} else {
		igt_assert_eq(ret, 0);
		igt_assert_lte(0, dma_buf_fd);
	}

	fd2 = drm_open_driver(DRIVER_INTEL);
	handle_import = prime_fd_to_handle(fd2, dma_buf_fd);
	check_bo(fd1, handle, 1, fd2, handle_import);

	/* close dma_buf, check whether nothing disappears. */
	close(dma_buf_fd);
	check_bo(fd1, handle, 1, fd2, handle_import);

	/* destroy userptr object and expect SIGBUS */
	free_userptr_bo(fd1, handle);
	close(fd1);

	if (gem_has_llc(fd2)) {
		struct sigaction sigact, orig_sigact;

		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_sigaction = sigbus;
		sigact.sa_flags = SA_SIGINFO;
		ret = sigaction(SIGBUS, &sigact, &orig_sigact);
		igt_assert_eq(ret, 0);

		orig_sigbus = orig_sigact.sa_sigaction;

		sigbus_cnt = 0;
		check_bo(fd2, handle_import, -1, fd2, handle_import);
		igt_assert(sigbus_cnt > 0);

		ret = sigaction(SIGBUS, &orig_sigact, NULL);
		igt_assert_eq(ret, 0);
	}

	close(fd2);
	reset_handle_ptr();

	return 0;
}

static void store_dword_rand(int i915, unsigned int engine,
			     uint32_t target, uint64_t sz,
			     int count)
{
	const int gen = intel_gen(intel_get_drm_devid(i915));
	struct drm_i915_gem_relocation_entry *reloc;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_execbuffer2 exec;
	unsigned int batchsz;
	uint32_t *batch;
	int i;

	batchsz = count * 16 + 4;
	batchsz = ALIGN(batchsz, 4096);

	reloc = calloc(sizeof(*reloc), count);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = target;
	obj[0].flags = LOCAL_EXEC_OBJECT_SUPPORTS_48B;
	obj[1].handle = gem_create(i915, batchsz);
	obj[1].relocation_count = count;
	obj[1].relocs_ptr = to_user_pointer(reloc);

	batch = gem_mmap__wc(i915, obj[1].handle, 0, batchsz, PROT_WRITE);

	memset(&exec, 0, sizeof(exec));
	exec.buffer_count = 2;
	exec.buffers_ptr = to_user_pointer(obj);
	exec.flags = engine;
	if (gen < 6)
		exec.flags |= I915_EXEC_SECURE;

	i = 0;
	for (int n = 0; n < count; n++) {
		uint64_t offset;

		reloc[n].target_handle = obj[0].handle;
		reloc[n].delta = rand() % (sz / 4) * 4;
		reloc[n].offset = (i + 1) * sizeof(uint32_t);
		reloc[n].presumed_offset = obj[0].offset;
		reloc[n].read_domains = I915_GEM_DOMAIN_RENDER;
		reloc[n].write_domain = I915_GEM_DOMAIN_RENDER;

		offset = reloc[n].presumed_offset + reloc[n].delta;

		batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
		if (gen >= 8) {
			batch[++i] = offset;
			batch[++i] = offset >> 32;
		} else if (gen >= 4) {
			batch[++i] = 0;
			batch[++i] = offset;
			reloc[n].offset += sizeof(uint32_t);
		} else {
			batch[i]--;
			batch[++i] = offset;
		}
		batch[++i] = rand();
		i++;
	}
	batch[i] = MI_BATCH_BUFFER_END;
	igt_assert(i * sizeof(uint32_t) < batchsz);
	munmap(batch, batchsz);

	gem_execbuf(i915, &exec);

	gem_close(i915, obj[1].handle);
	free(reloc);
}

static void test_readonly(int i915)
{
	uint64_t aperture_size;
	uint32_t whandle, rhandle;
	size_t sz, total;
	void *pages, *space;
	int memfd;

	/*
	 * A small batch of pages; small enough to cheaply check for stray
	 * writes but large enough that we don't create too many VMA pointing
	 * back to this set from the large arena. The limit on total number
	 * of VMA for a process is 65,536 (at least on this kernel).
	 *
	 * We then write from the GPU through the large arena into the smaller
	 * backing storage, which we can cheaply check to see if those writes
	 * have landed (using a SHA1sum). Repeating the same random GPU writes
	 * though a read-only handle to confirm that this time the writes are
	 * discarded and the backing store unchanged.
	 */
	sz = 16 << 12;
	memfd = memfd_create("pages", 0);
	igt_require(memfd != -1);
	igt_require(ftruncate(memfd, sz) == 0);

	pages = mmap(NULL, sz, PROT_WRITE, MAP_SHARED, memfd, 0);
	igt_assert(pages != MAP_FAILED);

	igt_require(__gem_userptr(i915, pages, sz, true, userptr_flags, &rhandle) == 0);
	gem_close(i915, rhandle);

	gem_userptr(i915, pages, sz, false, userptr_flags, &whandle);

	/*
	 * We have only a 31bit delta which we use for generating
	 * the target address for MI_STORE_DWORD_IMM, so our maximum
	 * usable object size is only 2GiB. For now.
	 */
	total = 2048ull << 20;
	aperture_size = gem_aperture_size(i915) / 2;
	if (aperture_size < total)
		total = aperture_size;
	total = total / sz * sz;
	igt_info("Using a %'zuB (%'zu pages) arena onto %zu pages\n",
		 total, total >> 12, sz >> 12);

	/* Create an arena all pointing to the same set of pages */
	space = mmap(NULL, total, PROT_READ, MAP_ANON | MAP_SHARED, -1, 0);
	igt_require(space != MAP_FAILED);
	for (size_t offset = 0; offset < total; offset += sz) {
		igt_assert(mmap(space + offset, sz,
				PROT_WRITE, MAP_SHARED | MAP_FIXED,
				memfd, 0) != MAP_FAILED);
		*(uint32_t *)(space + offset) = offset;
	}
	igt_assert_eq_u32(*(uint32_t *)pages, (uint32_t)(total - sz));
	igt_assert(mlock(pages, sz) == 0);
	close(memfd);

	/* Check we can create a normal userptr bo wrapping the wrapper */
	gem_userptr(i915, space, total, false, userptr_flags, &rhandle);
	gem_set_domain(i915, rhandle, I915_GEM_DOMAIN_CPU, 0);
	for (size_t offset = 0; offset < total; offset += sz)
		store_dword(i915, rhandle, offset + 4, offset / sz);
	gem_sync(i915, rhandle);
	igt_assert_eq_u32(*(uint32_t *)(pages + 0), (uint32_t)(total - sz));
	igt_assert_eq_u32(*(uint32_t *)(pages + 4), (uint32_t)(total / sz - 1));
	gem_close(i915, rhandle);

	/* Now enforce read-only henceforth */
	igt_assert(mprotect(space, total, PROT_READ) == 0);

	igt_fork(child, 1) {
		unsigned int engine;
		char *orig;

		orig = g_compute_checksum_for_data(G_CHECKSUM_SHA1, pages, sz);

		gem_userptr(i915, space, total, true, userptr_flags, &rhandle);

		for_each_engine(i915, engine) {
			char *ref, *result;

			/* First tweak the backing store through the write */
			store_dword_rand(i915, engine, whandle, sz, 1024);
			gem_sync(i915, whandle);
			ref = g_compute_checksum_for_data(G_CHECKSUM_SHA1,
							  pages, sz);

			/* Check some writes did land */
			igt_assert(strcmp(ref, orig));

			/* Now try the same through the read-only handle */
			store_dword_rand(i915, engine, rhandle, total, 1024);
			gem_sync(i915, rhandle);
			result = g_compute_checksum_for_data(G_CHECKSUM_SHA1,
							     pages, sz);

			/*
			 * As the writes into the read-only GPU bo should fail,
			 * the SHA1 hash of the backing store should be
			 * unaffected.
			 */
			igt_assert(strcmp(ref, result) == 0);

			g_free(result);
			g_free(orig);
			orig = ref;
		}

		gem_close(i915, rhandle);

		g_free(orig);
	}
	igt_waitchildren();

	munlock(pages, sz);
	munmap(space, total);
	munmap(pages, sz);
}

static jmp_buf sigjmp;
static void sigjmp_handler(int sig)
{
	siglongjmp(sigjmp, sig);
}

static void test_readonly_mmap(int i915)
{
	char *original, *result;
	uint32_t handle;
	uint32_t sz;
	void *pages;
	void *ptr;
	int sig;

	/*
	 * A quick check to ensure that we cannot circumvent the
	 * read-only nature of our memory by creating a GTT mmap into
	 * the pages. Imagine receiving a readonly SHM segment from
	 * another process, or a readonly file mmap, it must remain readonly
	 * on the GPU as well.
	 */

	igt_require(igt_setup_clflush());

	sz = 16 << 12;
	pages = mmap(NULL, sz, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	igt_assert(pages != MAP_FAILED);

	igt_require(__gem_userptr(i915, pages, sz, true, userptr_flags, &handle) == 0);
	gem_set_caching(i915, handle, 0);

	memset(pages, 0xa5, sz);
	igt_clflush_range(pages, sz);
	original = g_compute_checksum_for_data(G_CHECKSUM_SHA1, pages, sz);

	ptr = __gem_mmap__gtt(i915, handle, sz, PROT_WRITE);
	igt_assert(ptr == NULL);

	ptr = gem_mmap__gtt(i915, handle, sz, PROT_READ);
	gem_close(i915, handle);

	/* Check that a write into the GTT readonly map fails */
	if (!(sig = sigsetjmp(sigjmp, 1))) {
		signal(SIGBUS, sigjmp_handler);
		signal(SIGSEGV, sigjmp_handler);
		memset(ptr, 0x5a, sz);
		igt_assert(0);
	}
	igt_assert_eq(sig, SIGSEGV);

	/* Check that we disallow removing the readonly protection */
	igt_assert(mprotect(ptr, sz, PROT_WRITE));
	if (!(sig = sigsetjmp(sigjmp, 1))) {
		signal(SIGBUS, sigjmp_handler);
		signal(SIGSEGV, sigjmp_handler);
		memset(ptr, 0x5a, sz);
		igt_assert(0);
	}
	igt_assert_eq(sig, SIGSEGV);

	/* A single read from the GTT pointer to prove that works */
	igt_assert_eq_u32(*(uint8_t *)ptr, 0xa5);
	munmap(ptr, sz);

	/* Double check that the kernel did indeed not let any writes through */
	igt_clflush_range(pages, sz);
	result = g_compute_checksum_for_data(G_CHECKSUM_SHA1, pages, sz);
	igt_assert(!strcmp(original, result));

	g_free(original);
	g_free(result);

	munmap(pages, sz);
}

static void test_readonly_pwrite(int i915)
{
	char *original, *result;
	uint32_t handle;
	uint32_t sz;
	void *pages;

	/*
	 * Same as for GTT mmapings, we cannot alone ourselves to
	 * circumvent readonly protection on a piece of memory via the
	 * pwrite ioctl.
	 */

	igt_require(igt_setup_clflush());

	sz = 16 << 12;
	pages = mmap(NULL, sz, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	igt_assert(pages != MAP_FAILED);

	igt_require(__gem_userptr(i915, pages, sz, true, userptr_flags, &handle) == 0);
	memset(pages, 0xa5, sz);
	original = g_compute_checksum_for_data(G_CHECKSUM_SHA1, pages, sz);

	for (int page = 0; page < 16; page++) {
		char data[4096];

		memset(data, page, sizeof(data));
		igt_assert_eq(__gem_write(i915, handle, page << 12, data, sizeof(data)), -EINVAL);
	}

	gem_close(i915, handle);

	result = g_compute_checksum_for_data(G_CHECKSUM_SHA1, pages, sz);
	igt_assert(!strcmp(original, result));

	g_free(original);
	g_free(result);

	munmap(pages, sz);
}

static int test_usage_restrictions(int fd)
{
	void *ptr;
	int ret;
	uint32_t handle;

	igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE * 2) == 0);

	/* Address not aligned. */
	ret = __gem_userptr(fd, (char *)ptr + 1, PAGE_SIZE, 0, userptr_flags, &handle);
	igt_assert_neq(ret, 0);

	/* Size not rounded to page size. */
	ret = __gem_userptr(fd, ptr, PAGE_SIZE - 1, 0, userptr_flags, &handle);
	igt_assert_neq(ret, 0);

	/* Both wrong. */
	ret = __gem_userptr(fd, (char *)ptr + 1, PAGE_SIZE - 1, 0, userptr_flags, &handle);
	igt_assert_neq(ret, 0);

	free(ptr);

	return 0;
}

static int test_create_destroy(int fd, int time)
{
	struct timespec start, now;
	uint32_t handle;
	void *ptr;
	int n;

	igt_fork_signal_helper();

	clock_gettime(CLOCK_MONOTONIC, &start);
	do {
		for (n = 0; n < 1000; n++) {
			igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE) == 0);

			gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle);

			gem_close(fd, handle);
			free(ptr);
		}

		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_sec -= time;
	} while (now.tv_sec < start.tv_sec ||
		 (now.tv_sec == start.tv_sec && now.tv_nsec < start.tv_nsec));

	igt_stop_signal_helper();

	return 0;
}

static int test_coherency(int fd, int count)
{
	uint32_t *memory;
	uint32_t *cpu, *cpu_val;
	uint32_t *gpu, *gpu_val;
	uint32_t start = 0;
	int i, ret;

	igt_info("Using 2x%d 1MiB buffers\n", count);
	intel_require_memory(2*count, sizeof(linear), CHECK_RAM);

	ret = posix_memalign((void **)&memory, PAGE_SIZE, count*sizeof(linear));
	igt_assert(ret == 0 && memory);

	gpu = malloc(sizeof(uint32_t)*count*4);
	gpu_val = gpu + count;
	cpu = gpu_val + count;
	cpu_val = cpu + count;

	for (i = 0; i < count; i++) {
		gpu[i] = create_bo(fd, start);
		gpu_val[i] = start;
		start += WIDTH*HEIGHT;
	}

	for (i = 0; i < count; i++) {
		cpu[i] = create_userptr(fd, start, memory+i*WIDTH*HEIGHT);
		cpu_val[i] = start;
		start += WIDTH*HEIGHT;
	}

	igt_info("Verifying initialisation...\n");
	for (i = 0; i < count; i++) {
		check_gpu(fd, gpu[i], gpu_val[i]);
		check_cpu(memory+i*WIDTH*HEIGHT, cpu_val[i]);
	}

	igt_info("Cyclic blits cpu->gpu, forward...\n");
	for (i = 0; i < count * 4; i++) {
		int src = i % count;
		int dst = (i + 1) % count;

		igt_assert_eq(copy(fd, gpu[dst], cpu[src]), 0);
		gpu_val[dst] = cpu_val[src];
	}
	for (i = 0; i < count; i++)
		check_gpu(fd, gpu[i], gpu_val[i]);

	igt_info("Cyclic blits gpu->cpu, backward...\n");
	for (i = 0; i < count * 4; i++) {
		int src = (i + 1) % count;
		int dst = i % count;

		igt_assert_eq(copy(fd, cpu[dst], gpu[src]), 0);
		cpu_val[dst] = gpu_val[src];
	}
	for (i = 0; i < count; i++) {
		gem_userptr_sync(fd, cpu[i]);
		check_cpu(memory+i*WIDTH*HEIGHT, cpu_val[i]);
	}

	igt_info("Random blits...\n");
	for (i = 0; i < count * 4; i++) {
		int src = random() % count;
		int dst = random() % count;

		if (random() & 1) {
			igt_assert_eq(copy(fd, gpu[dst], cpu[src]), 0);
			gpu_val[dst] = cpu_val[src];
		} else {
			igt_assert_eq(copy(fd, cpu[dst], gpu[src]), 0);
			cpu_val[dst] = gpu_val[src];
		}
	}
	for (i = 0; i < count; i++) {
		check_gpu(fd, gpu[i], gpu_val[i]);
		gem_close(fd, gpu[i]);

		gem_userptr_sync(fd, cpu[i]);
		check_cpu(memory+i*WIDTH*HEIGHT, cpu_val[i]);
		gem_close(fd, cpu[i]);
	}

	free(gpu);
	free(memory);

	return 0;
}

static struct igt_eviction_test_ops fault_ops = {
	.create = create_userptr_bo,
	.flink = flink_userptr_bo,
	.close = free_userptr_bo,
	.copy = blit,
	.clear = clear,
};

static int can_swap(void)
{
	unsigned long as, ram;

	/* Cannot swap if not enough address space */

	/* FIXME: Improve check criteria. */
	if (sizeof(void*) < 8)
		as = 3 * 1024;
	else
		as = 256 * 1024; /* Just a big number */

	ram = intel_get_total_ram_mb();

	if ((as - 128) < (ram - 256))
		return 0;

	return 1;
}

static void test_forking_evictions(int fd, int size, int count,
			     unsigned flags)
{
	int trash_count;
	int num_threads;

	trash_count = intel_get_total_ram_mb() * 11 / 10;
	/* Use the fact test will spawn a number of child
	 * processes meaning swapping will be triggered system
	 * wide even if one process on it's own can't do it.
	 */
	num_threads = min(sysconf(_SC_NPROCESSORS_ONLN) * 4, 12);
	trash_count /= num_threads;
	if (count > trash_count)
		count = trash_count;

	forking_evictions(fd, &fault_ops, size, count, trash_count, flags);
	reset_handle_ptr();
}

static void test_mlocked_evictions(int fd, int size, int count)
{
	count = min(256, count/2);
	mlocked_evictions(fd, &fault_ops, size, count);
	reset_handle_ptr();
}

static void test_swapping_evictions(int fd, int size, int count)
{
	int trash_count;

	igt_skip_on_f(!can_swap(),
		"Not enough process address space for swapping tests.\n");

	trash_count = intel_get_total_ram_mb() * 11 / 10;

	swapping_evictions(fd, &fault_ops, size, count, trash_count);
	reset_handle_ptr();
}

static void test_minor_evictions(int fd, int size, int count)
{
	minor_evictions(fd, &fault_ops, size, count);
	reset_handle_ptr();
}

static void test_major_evictions(int fd, int size, int count)
{
	major_evictions(fd, &fault_ops, size, count);
	reset_handle_ptr();
}

static void test_overlap(int fd, int expected)
{
	char *ptr;
	int ret;
	uint32_t handle, handle2;

	igt_assert(posix_memalign((void *)&ptr, PAGE_SIZE, PAGE_SIZE * 3) == 0);

	gem_userptr(fd, ptr + PAGE_SIZE, PAGE_SIZE, 0, userptr_flags, &handle);

	/* before, no overlap */
	ret = __gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle2);
	if (ret == 0)
		gem_close(fd, handle2);
	igt_assert_eq(ret, 0);

	/* after, no overlap */
	ret = __gem_userptr(fd, ptr + PAGE_SIZE * 2, PAGE_SIZE, 0, userptr_flags, &handle2);
	if (ret == 0)
		gem_close(fd, handle2);
	igt_assert_eq(ret, 0);

	/* exactly overlapping */
	ret = __gem_userptr(fd, ptr + PAGE_SIZE, PAGE_SIZE, 0, userptr_flags, &handle2);
	if (ret == 0)
		gem_close(fd, handle2);
	igt_assert(ret == 0 || ret == expected);

	/* start overlaps */
	ret = __gem_userptr(fd, ptr, PAGE_SIZE * 2, 0, userptr_flags, &handle2);
	if (ret == 0)
		gem_close(fd, handle2);
	igt_assert(ret == 0 || ret == expected);

	/* end overlaps */
	ret = __gem_userptr(fd, ptr + PAGE_SIZE, PAGE_SIZE * 2, 0, userptr_flags, &handle2);
	if (ret == 0)
		gem_close(fd, handle2);
	igt_assert(ret == 0 || ret == expected);

	/* subsumes */
	ret = __gem_userptr(fd, ptr, PAGE_SIZE * 3, 0, userptr_flags, &handle2);
	if (ret == 0)
		gem_close(fd, handle2);
	igt_assert(ret == 0 || ret == expected);

	gem_close(fd, handle);
	free(ptr);
}

static void test_unmap(int fd, int expected)
{
	char *ptr, *bo_ptr;
	const unsigned int num_obj = 3;
	unsigned int i;
	uint32_t bo[num_obj + 1];
	size_t map_size = sizeof(linear) * num_obj + (PAGE_SIZE - 1);
	int ret;

	ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	igt_assert(ptr != MAP_FAILED);

	bo_ptr = (char *)ALIGN((unsigned long)ptr, PAGE_SIZE);

	for (i = 0; i < num_obj; i++, bo_ptr += sizeof(linear)) {
		gem_userptr(fd, bo_ptr, sizeof(linear), 0, userptr_flags, &bo[i]);
	}

	bo[num_obj] = create_bo(fd, 0);

	for (i = 0; i < num_obj; i++)
		igt_assert_eq(copy(fd, bo[num_obj], bo[i]), 0);

	ret = munmap(ptr, map_size);
	igt_assert_eq(ret, 0);

	for (i = 0; i < num_obj; i++)
		igt_assert_eq(copy(fd, bo[num_obj], bo[i]), -expected);

	for (i = 0; i < (num_obj + 1); i++)
		gem_close(fd, bo[i]);
}

static void test_unmap_after_close(int fd)
{
	char *ptr, *bo_ptr;
	const unsigned int num_obj = 3;
	unsigned int i;
	uint32_t bo[num_obj + 1];
	size_t map_size = sizeof(linear) * num_obj + (PAGE_SIZE - 1);
	int ret;

	ptr = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	igt_assert(ptr != MAP_FAILED);

	bo_ptr = (char *)ALIGN((unsigned long)ptr, PAGE_SIZE);

	for (i = 0; i < num_obj; i++, bo_ptr += sizeof(linear)) {
		gem_userptr(fd, bo_ptr, sizeof(linear), 0, userptr_flags, &bo[i]);
	}

	bo[num_obj] = create_bo(fd, 0);

	for (i = 0; i < num_obj; i++)
		igt_assert_eq(copy(fd, bo[num_obj], bo[i]), 0);

	for (i = 0; i < (num_obj + 1); i++)
		gem_close(fd, bo[i]);

	ret = munmap(ptr, map_size);
	igt_assert_eq(ret, 0);
}

static void test_unmap_cycles(int fd, int expected)
{
	int i;

	for (i = 0; i < 1000; i++)
		test_unmap(fd, expected);
}

#define MM_STRESS_LOOPS 100000

struct stress_thread_data {
	unsigned int stop;
	int exit_code;
};

static void *mm_stress_thread(void *data)
{
	struct stress_thread_data *stdata = (struct stress_thread_data *)data;
	const size_t sz = 2 << 20;
	void *ptr;

	while (!stdata->stop) {
		ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE,
			   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		if (ptr == MAP_FAILED) {
			stdata->exit_code = -EFAULT;
			break;
		}

		madvise(ptr, sz, MADV_HUGEPAGE);
		for (size_t page = 0; page < sz; page += PAGE_SIZE)
			*(volatile uint32_t *)((unsigned char *)ptr + page) = 0;

		if (munmap(ptr, sz)) {
			stdata->exit_code = errno;
			break;
		}
	}

	return NULL;
}

static void test_stress_mm(int fd)
{
	int ret;
	pthread_t t;
	unsigned int loops = MM_STRESS_LOOPS;
	uint32_t handle;
	void *ptr;
	struct stress_thread_data stdata;

	memset(&stdata, 0, sizeof(stdata));

	igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE) == 0);

	ret = pthread_create(&t, NULL, mm_stress_thread, &stdata);
	igt_assert_eq(ret, 0);

	while (loops--) {
		gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle);

		gem_close(fd, handle);
	}

	free(ptr);

	stdata.stop = 1;
	ret = pthread_join(t, NULL);
	igt_assert_eq(ret, 0);

	igt_assert_eq(stdata.exit_code, 0);
}

static void test_stress_purge(int fd)
{
	struct stress_thread_data stdata;
	uint32_t handle;
	pthread_t t;
	void *ptr;

	memset(&stdata, 0, sizeof(stdata));

	igt_assert(posix_memalign(&ptr, PAGE_SIZE, PAGE_SIZE) == 0);
	igt_assert(!pthread_create(&t, NULL, mm_stress_thread, &stdata));

	igt_until_timeout(150) {
		gem_userptr(fd, ptr, PAGE_SIZE, 0, userptr_flags, &handle);

		gem_set_domain(fd, handle,
			       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
		intel_purge_vm_caches(fd);

		gem_close(fd, handle);
	}

	free(ptr);

	stdata.stop = 1;
	igt_assert(!pthread_join(t, NULL));
	igt_assert_eq(stdata.exit_code, 0);
}

struct userptr_close_thread_data {
	int fd;
	void *ptr;
	bool overlap;
	bool stop;
	pthread_mutex_t mutex;
};

static void *mm_userptr_close_thread(void *data)
{
	struct userptr_close_thread_data *t = (struct userptr_close_thread_data *)data;
	int num_handles = t->overlap ? 2 : 1;

	uint32_t handle[num_handles];

	/* Be pedantic and enforce the required memory barriers */
	pthread_mutex_lock(&t->mutex);
	while (!t->stop) {
		pthread_mutex_unlock(&t->mutex);
		for (int i = 0; i < num_handles; i++)
			gem_userptr(t->fd, t->ptr, PAGE_SIZE, 0, userptr_flags, &handle[i]);
		for (int i = 0; i < num_handles; i++)
			gem_close(t->fd, handle[i]);
		pthread_mutex_lock(&t->mutex);
	}
	pthread_mutex_unlock(&t->mutex);

	return NULL;
}

static void test_invalidate_close_race(int fd, bool overlap)
{
	pthread_t t;
	unsigned int loops = MM_STRESS_LOOPS;
	struct userptr_close_thread_data t_data;

	memset(&t_data, 0, sizeof(t_data));
	t_data.fd = fd;
	t_data.overlap = overlap;
	igt_assert(posix_memalign(&t_data.ptr, PAGE_SIZE, PAGE_SIZE) == 0);
	pthread_mutex_init(&t_data.mutex, NULL);

	igt_assert(pthread_create(&t, NULL, mm_userptr_close_thread, &t_data) == 0);

	while (loops--) {
		mprotect(t_data.ptr, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
		mprotect(t_data.ptr, PAGE_SIZE, PROT_READ | PROT_WRITE);
	}

	pthread_mutex_lock(&t_data.mutex);
	t_data.stop = 1;
	pthread_mutex_unlock(&t_data.mutex);

	pthread_join(t, NULL);

	pthread_mutex_destroy(&t_data.mutex);
	free(t_data.ptr);
}

uint64_t total_ram;
uint64_t aperture_size;
int fd, count;

static int opt_handler(int opt, int opt_index, void *data)
{
	switch (opt) {
	case 'c':
		count = atoi(optarg);
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

const char *help_str = "  -c\tBuffer count\n";

igt_main_args("c:", NULL, help_str, opt_handler, NULL)
{
	int size = sizeof(linear);

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_assert(fd >= 0);
		igt_require_gem(fd);

		size = sizeof(linear);

		aperture_size = gem_aperture_size(fd);
		igt_info("Aperture size is %lu MiB\n", (long)(aperture_size / (1024*1024)));

		if (count == 0)
			count = 2 * aperture_size / (1024*1024) / 3;

		total_ram = intel_get_total_ram_mb();
		igt_info("Total RAM is %'llu MiB\n", (long long)total_ram);

		if (count > total_ram * 3 / 4) {
			count = intel_get_total_ram_mb() * 3 / 4;
			igt_info("Not enough RAM to run test, reducing buffer count.\n");
		}
	}

	igt_subtest_group {
		igt_fixture {
			igt_require(has_userptr(fd));
		}

		igt_subtest("input-checking")
			test_input_checking(fd);

		igt_subtest("usage-restrictions")
			test_usage_restrictions(fd);

		igt_subtest("invalid-null-pointer")
			test_invalid_null_pointer(fd);

		igt_subtest("invalid-gtt-mapping")
			test_invalid_gtt_mapping(fd);

		igt_subtest("forked-access")
			test_forked_access(fd);

		igt_subtest("forbidden-operations")
			test_forbidden_ops(fd);

		igt_subtest("relocations")
			test_relocations(fd);
	}

	igt_subtest_group {
		gem_userptr_test_unsynchronized();

		igt_fixture {
			igt_require(has_userptr(fd));
		}

		igt_subtest("create-destroy-unsync")
			test_create_destroy(fd, 5);

		igt_subtest("unsync-overlap")
			test_overlap(fd, 0);

		igt_subtest("unsync-unmap")
			test_unmap(fd, 0);

		igt_subtest("unsync-unmap-cycles")
			test_unmap_cycles(fd, 0);

		igt_subtest("unsync-unmap-after-close")
			test_unmap_after_close(fd);

		igt_subtest("coherency-unsync")
			test_coherency(fd, count);

		igt_subtest("dmabuf-unsync")
			test_dmabuf();

		igt_subtest("readonly-unsync")
			test_readonly(fd);

		igt_subtest("readonly-mmap-unsync")
			test_readonly_mmap(fd);

		igt_subtest("readonly-pwrite-unsync")
			test_readonly_pwrite(fd);

		for (unsigned flags = 0; flags < ALL_FORKING_EVICTIONS + 1; flags++) {
			igt_subtest_f("forked-unsync%s%s%s-%s",
					flags & FORKING_EVICTIONS_SWAPPING ? "-swapping" : "",
					flags & FORKING_EVICTIONS_DUP_DRMFD ? "-multifd" : "",
					flags & FORKING_EVICTIONS_MEMORY_PRESSURE ?
					"-mempressure" : "",
					flags & FORKING_EVICTIONS_INTERRUPTIBLE ?
					"interruptible" : "normal") {
				test_forking_evictions(fd, size, count, flags);
			}
		}

		igt_subtest("mlocked-unsync-normal")
			test_mlocked_evictions(fd, size, count);

		igt_subtest("swapping-unsync-normal")
			test_swapping_evictions(fd, size, count);

		igt_subtest("minor-unsync-normal")
			test_minor_evictions(fd, size, count);

		igt_subtest("major-unsync-normal") {
			size = 200 * 1024 * 1024;
			count = (gem_aperture_size(fd) / size) + 2;
			test_major_evictions(fd, size, count);
		}

		igt_fixture {
			size = sizeof(linear);
			count = 2 * gem_aperture_size(fd) / (1024*1024) / 3;
			if (count > total_ram * 3 / 4)
				count = intel_get_total_ram_mb() * 3 / 4;
		}

		igt_fork_signal_helper();

		igt_subtest("mlocked-unsync-interruptible")
			test_mlocked_evictions(fd, size, count);

		igt_subtest("swapping-unsync-interruptible")
			test_swapping_evictions(fd, size, count);

		igt_subtest("minor-unsync-interruptible")
			test_minor_evictions(fd, size, count);

		igt_subtest("major-unsync-interruptible") {
			size = 200 * 1024 * 1024;
			count = (gem_aperture_size(fd) / size) + 2;
			test_major_evictions(fd, size, count);
		}

		igt_stop_signal_helper();
	}

	igt_subtest_group {
		gem_userptr_test_synchronized();

		igt_fixture {
			igt_require(has_userptr(fd));
			size = sizeof(linear);
			count = 2 * gem_aperture_size(fd) / (1024*1024) / 3;
			if (count > total_ram * 3 / 4)
				count = intel_get_total_ram_mb() * 3 / 4;
		}

		igt_subtest("process-exit")
			test_process_exit(fd, 0);

		igt_subtest("process-exit-gtt")
			test_process_exit(fd, PE_GTT_MAP);

		igt_subtest("process-exit-busy")
			test_process_exit(fd, PE_BUSY);

		igt_subtest("process-exit-gtt-busy")
			test_process_exit(fd, PE_GTT_MAP | PE_BUSY);

		igt_subtest("create-destroy-sync")
			test_create_destroy(fd, 5);

		igt_subtest("sync-overlap")
			test_overlap(fd, EINVAL);

		igt_subtest("sync-unmap")
			test_unmap(fd, EFAULT);

		igt_subtest("sync-unmap-cycles")
			test_unmap_cycles(fd, EFAULT);

		igt_subtest("sync-unmap-after-close")
			test_unmap_after_close(fd);

		igt_subtest("stress-mm")
			test_stress_mm(fd);
		igt_subtest("stress-purge")
			test_stress_purge(fd);

		igt_subtest("stress-mm-invalidate-close")
			test_invalidate_close_race(fd, false);

		igt_subtest("stress-mm-invalidate-close-overlap")
			test_invalidate_close_race(fd, true);

		for (unsigned flags = 0; flags < ALL_MAP_FIXED_INVALIDATE + 1; flags++) {
			igt_subtest_f("map-fixed-invalidate%s%s%s",
				      flags & MAP_FIXED_INVALIDATE_OVERLAP ? "-overlap" : "",
				      flags & MAP_FIXED_INVALIDATE_BUSY ? "-busy" : "",
				      flags & MAP_FIXED_INVALIDATE_GET_PAGES ? "-gup" : "") {
				test_map_fixed_invalidate(fd, flags);
			}
		}

		igt_subtest("coherency-sync")
			test_coherency(fd, count);

		igt_subtest("dmabuf-sync")
			test_dmabuf();

		for (unsigned flags = 0; flags < ALL_FORKING_EVICTIONS + 1; flags++) {
			igt_subtest_f("forked-sync%s%s%s-%s",
					flags & FORKING_EVICTIONS_SWAPPING ? "-swapping" : "",
					flags & FORKING_EVICTIONS_DUP_DRMFD ? "-multifd" : "",
					flags & FORKING_EVICTIONS_MEMORY_PRESSURE ?
					"-mempressure" : "",
					flags & FORKING_EVICTIONS_INTERRUPTIBLE ?
					"interruptible" : "normal") {
				test_forking_evictions(fd, size, count, flags);
			}
		}

		igt_subtest("mlocked-normal-sync")
			test_mlocked_evictions(fd, size, count);

		igt_subtest("swapping-normal-sync")
			test_swapping_evictions(fd, size, count);

		igt_subtest("minor-normal-sync")
			test_minor_evictions(fd, size, count);

		igt_subtest("major-normal-sync") {
			size = 200 * 1024 * 1024;
			count = (gem_aperture_size(fd) / size) + 2;
			test_major_evictions(fd, size, count);
		}

		igt_fixture {
			size = 1024 * 1024;
			count = 2 * gem_aperture_size(fd) / (1024*1024) / 3;
			if (count > total_ram * 3 / 4)
				count = intel_get_total_ram_mb() * 3 / 4;
		}

		igt_fork_signal_helper();

		igt_subtest("mlocked-sync-interruptible")
			test_mlocked_evictions(fd, size, count);

		igt_subtest("swapping-sync-interruptible")
			test_swapping_evictions(fd, size, count);

		igt_subtest("minor-sync-interruptible")
			test_minor_evictions(fd, size, count);

		igt_subtest("major-sync-interruptible") {
			size = 200 * 1024 * 1024;
			count = (gem_aperture_size(fd) / size) + 2;
			test_major_evictions(fd, size, count);
		}

		igt_stop_signal_helper();
	}


	igt_subtest("access-control")
		test_access_control(fd);
}
