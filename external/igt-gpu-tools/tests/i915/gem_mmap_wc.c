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

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "drm.h"

struct local_i915_gem_mmap_v2 {
	uint32_t handle;
	uint32_t pad;
	uint64_t offset;
	uint64_t size;
	uint64_t addr_ptr;
	uint64_t flags;
#define I915_MMAP_WC 0x1
};
#define LOCAL_IOCTL_I915_GEM_MMAP_v2 DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MMAP, struct local_i915_gem_mmap_v2)

static int OBJECT_SIZE = 16*1024*1024;

/*
 * Local WC mmap wrapper. This is used to make sure we go through
 * the GEM_MMAP IOCTL.
 * */
static void *
local_gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
	void *ptr;

	ptr = __gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ | PROT_WRITE);
	igt_assert(ptr);

	return ptr;
}

static void set_domain(int fd, uint32_t handle)
{
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);
}

static void *
mmap_bo(int fd, uint32_t handle)
{
	void *ptr;

	ptr = local_gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ | PROT_WRITE);

	return ptr;
}

static void *
create_pointer(int fd)
{
	uint32_t handle;
	void *ptr;

	handle = gem_create(fd, OBJECT_SIZE);

	ptr = mmap_bo(fd, handle);
	set_domain(fd, handle);

	gem_close(fd, handle);

	return ptr;
}

static void
test_invalid_flags(int fd)
{
	struct drm_i915_getparam gp;
	struct local_i915_gem_mmap_v2 arg;
	uint64_t flag = I915_MMAP_WC;
	int val = -1;

	memset(&arg, 0, sizeof(arg));
	arg.handle = gem_create(fd, 4096);
	arg.offset = 0;
	arg.size = 4096;

	memset(&gp, 0, sizeof(gp));
	gp.param = 30; /* MMAP_VERSION */
	gp.value = &val;

	/* Do we have the new mmap_ioctl? */
	drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);

	if (val >= 1) {
		/*
		 * Only MMAP_WC flag is supported in version 1, so any other
		 * flag should be rejected.
		 */
		flag <<= 1;
		while (flag) {
			arg.flags = flag;
			igt_assert(drmIoctl(fd,
				   LOCAL_IOCTL_I915_GEM_MMAP_v2,
				   &arg) == -1);
			igt_assert_eq(errno, EINVAL);
			flag <<= 1;
		}
	}

	gem_close(fd, arg.handle);
}

static void
test_copy(int fd)
{
	void *src, *dst;

	/* copy from a fresh src to fresh dst to force pagefault on both */
	src = create_pointer(fd);
	dst = create_pointer(fd);

	memcpy(dst, src, OBJECT_SIZE);
	memcpy(src, dst, OBJECT_SIZE);

	munmap(dst, OBJECT_SIZE);
	munmap(src, OBJECT_SIZE);
}

enum test_read_write {
	READ_BEFORE_WRITE,
	READ_AFTER_WRITE,
};

static void
test_read_write(int fd, enum test_read_write order)
{
	uint32_t handle;
	void *ptr;
	volatile uint32_t val = 0;

	handle = gem_create(fd, OBJECT_SIZE);
	set_domain(fd, handle);

	ptr = mmap_bo(fd, handle);
	igt_assert(ptr != MAP_FAILED);

	if (order == READ_BEFORE_WRITE) {
		val = *(uint32_t *)ptr;
		*(uint32_t *)ptr = val;
	} else {
		*(uint32_t *)ptr = val;
		val = *(uint32_t *)ptr;
	}

	gem_close(fd, handle);
	munmap(ptr, OBJECT_SIZE);
}

static void
test_read_write2(int fd, enum test_read_write order)
{
	uint32_t handle;
	void *r, *w;
	volatile uint32_t val = 0;

	handle = gem_create(fd, OBJECT_SIZE);
	set_domain(fd, handle);

	r = local_gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ);

	w = local_gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ | PROT_WRITE);

	if (order == READ_BEFORE_WRITE) {
		val = *(uint32_t *)r;
		*(uint32_t *)w = val;
	} else {
		*(uint32_t *)w = val;
		val = *(uint32_t *)r;
	}

	gem_close(fd, handle);
	munmap(r, OBJECT_SIZE);
	munmap(w, OBJECT_SIZE);
}

static void
test_write(int fd)
{
	void *src;
	uint32_t dst;

	/* copy from a fresh src to fresh dst to force pagefault on both */
	src = create_pointer(fd);
	dst = gem_create(fd, OBJECT_SIZE);

	gem_write(fd, dst, 0, src, OBJECT_SIZE);

	gem_close(fd, dst);
	munmap(src, OBJECT_SIZE);
}

static void
test_coherency(int fd)
{
	uint32_t handle;
	uint32_t *wc, *cpu;
	int i;

	igt_require(igt_setup_clflush());

	handle = gem_create(fd, OBJECT_SIZE);

	wc = local_gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ | PROT_WRITE);
	cpu = gem_mmap__cpu(fd, handle, 0, OBJECT_SIZE, PROT_READ | PROT_WRITE);
	gem_set_domain(fd, handle, I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

	for (i = 0; i < OBJECT_SIZE / 64; i++) {
		int x = 16*i + (i%16);
		wc[x] = i;
		igt_clflush_range(&cpu[x], sizeof(cpu[x]));
		igt_assert_eq(cpu[x], i);
	}

	munmap(cpu, OBJECT_SIZE);
	munmap(wc, OBJECT_SIZE);
	gem_close(fd, handle);
}

static void
test_write_gtt(int fd)
{
	uint32_t dst;
	char *dst_gtt;
	void *src;

	dst = gem_create(fd, OBJECT_SIZE);
	set_domain(fd, dst);

	/* prefault object into gtt */
	dst_gtt = mmap_bo(fd, dst);
	memset(dst_gtt, 0, OBJECT_SIZE);
	munmap(dst_gtt, OBJECT_SIZE);

	src = create_pointer(fd);

	gem_write(fd, dst, 0, src, OBJECT_SIZE);

	gem_close(fd, dst);
	munmap(src, OBJECT_SIZE);
}

static void
test_read(int fd)
{
	void *dst;
	uint32_t src;

	/* copy from a fresh src to fresh dst to force pagefault on both */
	dst = create_pointer(fd);
	src = gem_create(fd, OBJECT_SIZE);

	gem_read(fd, src, 0, dst, OBJECT_SIZE);

	gem_close(fd, src);
	munmap(dst, OBJECT_SIZE);
}

static void
test_close(int fd)
{
	uint32_t handle = gem_create(fd, OBJECT_SIZE);
	uint8_t *ptr = mmap_bo(fd, handle);
	int i;

	memset(ptr, 0xcc, OBJECT_SIZE);
	gem_close(fd, handle);
	for (i = 0; i < OBJECT_SIZE / 4096; i++)
		igt_assert(ptr[i * 4096 + i] == 0xcc);

	munmap(ptr, OBJECT_SIZE);
}

static void
test_write_cpu_read_wc(int fd, int force_domain)
{
	uint32_t handle;
	uint32_t *src, *dst;

	handle = gem_create(fd, OBJECT_SIZE);

	dst = local_gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ);

	src = gem_mmap__cpu(fd, handle, 0, OBJECT_SIZE, PROT_WRITE);

	memset(src, 0xaa, OBJECT_SIZE);
	if (force_domain)
		set_domain(fd, handle);
	igt_assert(memcmp(dst, src, OBJECT_SIZE) == 0);
	gem_close(fd, handle);

	munmap(src, OBJECT_SIZE);
	munmap(dst, OBJECT_SIZE);
}

static void
test_write_gtt_read_wc(int fd)
{
	uint32_t handle;
	uint32_t *src, *dst;

	handle = gem_create(fd, OBJECT_SIZE);
	set_domain(fd, handle);

	dst = local_gem_mmap__wc(fd, handle, 0, OBJECT_SIZE, PROT_READ);

	src = gem_mmap__gtt(fd, handle, OBJECT_SIZE, PROT_WRITE);

	memset(src, 0xaa, OBJECT_SIZE);
	igt_assert(memcmp(dst, src, OBJECT_SIZE) == 0);
	gem_close(fd, handle);

	munmap(src, OBJECT_SIZE);
	munmap(dst, OBJECT_SIZE);
}

static void
test_set_cache_level(int fd)
{
	struct drm_mode_cursor arg;
	struct drm_mode_card_res res;
	uint32_t crtc[32];
	int active_crtc = 0;
	int n;

	/* We want to trigger an old WARN in set-cache-level when
	 * it sees an unbound object in the GTT domain, following
	 * the introduction of mmap(wc).
	 */

	memset(&arg, 0, sizeof(arg));
	arg.flags = DRM_MODE_CURSOR_BO;
	arg.width = arg.height = 64;
	arg.handle = gem_create(fd, 64*64*4);
	set_domain(fd, arg.handle);

	/* Bind the object to the cursor to force set-cache-level(DISPLAY) */
	memset(&res, 0, sizeof(res));
	res.count_crtcs = 32;
	res.crtc_id_ptr = to_user_pointer(crtc);
	do_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
	for (n = 0; n < res.count_crtcs; n++) {
		struct drm_mode_crtc mode;

		memset(&mode, 0, sizeof(mode));
		mode.crtc_id = crtc[n];
		do_ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &mode);

		if (!mode.mode_valid)
			continue;

		active_crtc++;

		arg.crtc_id = crtc[n];
		do_ioctl(fd, DRM_IOCTL_MODE_CURSOR, &arg);
	}

	gem_close(fd, arg.handle);
	igt_require(active_crtc);
}

struct thread_fault_concurrent {
	pthread_t thread;
	int id;
	uint32_t **ptr;
};

static void *
thread_fault_concurrent(void *closure)
{
	struct thread_fault_concurrent *t = closure;
	uint32_t val = 0;
	int n;

	for (n = 0; n < 32; n++) {
		if (n & 1)
			*t->ptr[(n + t->id) % 32] = val;
		else
			val = *t->ptr[(n + t->id) % 32];
	}

	return NULL;
}

static void
test_fault_concurrent(int fd)
{
	uint32_t *ptr[32];
	struct thread_fault_concurrent thread[64];
	int n;

	for (n = 0; n < 32; n++) {
		ptr[n] = create_pointer(fd);
	}

	for (n = 0; n < 64; n++) {
		thread[n].ptr = ptr;
		thread[n].id = n;
		pthread_create(&thread[n].thread, NULL, thread_fault_concurrent, &thread[n]);
	}

	for (n = 0; n < 64; n++)
		pthread_join(thread[n].thread, NULL);

	for (n = 0; n < 32; n++) {
		munmap(ptr[n], OBJECT_SIZE);
	}
}

static void
test_pf_nonblock(int i915)
{
	igt_spin_t *spin;
	uint32_t *ptr;

	spin = igt_spin_new(i915);

	igt_set_timeout(1, "initial pagefaulting did not complete within 1s");

	ptr = gem_mmap__wc(i915, spin->handle, 0, 4096, PROT_WRITE);
	ptr[256] = 0;
	munmap(ptr, 4096);

	igt_reset_timeout();

	igt_spin_free(i915, spin);
}

static void
run_without_prefault(int fd,
			void (*func)(int fd))
{
	igt_disable_prefault();
	func(fd);
	igt_enable_prefault();
}

static int mmap_ioctl(int i915, struct drm_i915_gem_mmap *arg)
{
	int err = 0;

	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_MMAP, arg))
		err = -errno;

	errno = 0;
	return err;
}

int fd;

igt_main
{
	if (igt_run_in_simulation())
		OBJECT_SIZE = 1 * 1024 * 1024;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		gem_require_mmap_wc(fd);
	}

	igt_subtest("bad-object") {
		uint32_t real_handle = gem_create(fd, 4096);
		uint32_t handles[20];
		size_t i = 0, len;

		handles[i++] = 0xdeadbeef;
		for(int bit = 0; bit < 16; bit++)
			handles[i++] = real_handle | (1 << (bit + 16));
		handles[i++] = real_handle + 1;
		len = i;

		for (i = 0; i < len; ++i) {
			struct drm_i915_gem_mmap arg = {
				.handle = handles[i],
				.size = 4096,
				.flags = I915_MMAP_WC,
			};
			igt_assert_eq(mmap_ioctl(fd, &arg), -ENOENT);
		}

		gem_close(fd, real_handle);
	}

	igt_subtest("bad-offset") {
		struct bad_offset {
			uint64_t size;
			uint64_t offset;
		} bad_offsets[] = {
			{4096, 4096 + 1},
			{4096, -4096},
			{ 2 * 4096, -4096},
			{ 4096, ~0},
			{}
		};

		for (int i = 0; i < ARRAY_SIZE(bad_offsets); i++) {
			struct drm_i915_gem_mmap arg = {
				.handle = gem_create(fd, 4096),

				.offset = bad_offsets[i].offset,
				.size = bad_offsets[i].size,

				.flags = I915_MMAP_WC,
			};

			igt_assert_eq(mmap_ioctl(fd, &arg), -EINVAL);
			gem_close(fd, arg.handle);
		}
	}

	igt_subtest("bad-size") {
		uint64_t bad_size[] = {
			0,
			-4096,
			4096 + 1,
			2 * 4096,
			~0,
		};

		for (int i = 0; i < ARRAY_SIZE(bad_size); i++) {
			struct drm_i915_gem_mmap arg = {
				.handle = gem_create(fd, 4096),
				.offset = 4096,
				.size = bad_size[i],
				.flags = I915_MMAP_WC,
			};

			igt_assert_eq(mmap_ioctl(fd, &arg), -EINVAL);
			gem_close(fd, arg.handle);
		}
	}

	igt_subtest("invalid-flags")
		test_invalid_flags(fd);
	igt_subtest("close")
		test_close(fd);
	igt_subtest("copy")
		test_copy(fd);
	igt_subtest("read")
		test_read(fd);
	igt_subtest("write")
		test_write(fd);
	igt_subtest("coherency")
		test_coherency(fd);
	igt_subtest("write-gtt")
		test_write_gtt(fd);
	igt_subtest("read-write")
		test_read_write(fd, READ_BEFORE_WRITE);
	igt_subtest("write-read")
		test_read_write(fd, READ_AFTER_WRITE);
	igt_subtest("read-write-distinct")
		test_read_write2(fd, READ_BEFORE_WRITE);
	igt_subtest("write-read-distinct")
		test_read_write2(fd, READ_AFTER_WRITE);
	igt_subtest("fault-concurrent")
		test_fault_concurrent(fd);
	igt_subtest("read-no-prefault")
		run_without_prefault(fd, test_read);
	igt_subtest("write-no-prefault")
		run_without_prefault(fd, test_write);
	igt_subtest("write-gtt-no-prefault")
		run_without_prefault(fd, test_write_gtt);
	igt_subtest("write-cpu-read-wc")
		test_write_cpu_read_wc(fd, 1);
	igt_subtest("write-cpu-read-wc-unflushed")
		test_write_cpu_read_wc(fd, 0);
	igt_subtest("write-gtt-read-wc")
		test_write_gtt_read_wc(fd);
	igt_subtest("pf-nonblock")
		test_pf_nonblock(fd);
	igt_subtest("set-cache-level")
		test_set_cache_level(fd);

	igt_fixture
		close(fd);
}
