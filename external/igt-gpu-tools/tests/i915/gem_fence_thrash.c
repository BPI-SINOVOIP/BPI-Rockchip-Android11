/*
 * Copyright © 2008-9 Intel Corporation
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
 *
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "drm.h"

#include "igt.h"
#include "igt_x86.h"

#define PAGE_SIZE 4096
#define CACHELINE 64

#define OBJECT_SIZE (128*1024) /* restricted to 1MiB alignment on i915 fences */

/* Before introduction of the LRU list for fences, allocation of a fence for a page
 * fault would use the first inactive fence (i.e. in preference one with no outstanding
 * GPU activity, or it would wait on the first to finish). Given the choice, it would simply
 * reuse the fence that had just been allocated for the previous page-fault - the worst choice
 * when copying between two buffers and thus constantly swapping fences.
 */

struct test {
	int fd;
	int tiling;
	int num_surfaces;
};

static void *
bo_create (int fd, int tiling)
{
	uint32_t handle;
	void *ptr;

	handle = gem_create(fd, OBJECT_SIZE);

	/* dirty cpu caches a bit ... */
	ptr = gem_mmap__cpu(fd, handle, 0, OBJECT_SIZE,
			    PROT_READ | PROT_WRITE);
	memset(ptr, 0, OBJECT_SIZE);
	munmap(ptr, OBJECT_SIZE);

	gem_set_tiling(fd, handle, tiling, 1024);

	ptr = gem_mmap__gtt(fd, handle, OBJECT_SIZE, PROT_READ | PROT_WRITE);

	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	gem_close(fd, handle);

	return ptr;
}

static void *
bo_copy (void *_arg)
{
	struct test *t = (struct test *)_arg;
	int fd = t->fd;
	int n;
	char *a, *b;

	a = bo_create (fd, t->tiling);
	b = bo_create (fd, t->tiling);

	for (n = 0; n < 1000; n++) {
		memcpy (a, b, OBJECT_SIZE);
		sched_yield ();
	}

	munmap(a, OBJECT_SIZE);
	munmap(b, OBJECT_SIZE);

	return NULL;
}

static void copy_wc_page(void *dst, const void *src)
{
	igt_memcpy_from_wc(dst, src, PAGE_SIZE);
}

static void copy_wc_cacheline(void *dst, const void *src)
{
	igt_memcpy_from_wc(dst, src, CACHELINE);
}

static void
_bo_write_verify(struct test *t)
{
	int fd = t->fd;
	int i, k;
	uint32_t **s;
	unsigned int dwords = OBJECT_SIZE >> 2;
	const char *tile_str[] = { "none", "x", "y" };
	uint32_t tmp[PAGE_SIZE/sizeof(uint32_t)];

	igt_assert(t->tiling >= 0 && t->tiling <= I915_TILING_Y);
	igt_assert_lt(0, t->num_surfaces);

	s = calloc(sizeof(*s), t->num_surfaces);
	igt_assert(s);

	for (k = 0; k < t->num_surfaces; k++)
		s[k] = bo_create(fd, t->tiling);

	for (k = 0; k < t->num_surfaces; k++) {
		uint32_t *a = s[k];

		a[0] = 0xdeadbeef;
		igt_assert_f(a[0] == 0xdeadbeef,
			     "tiling %s: write failed at start (%x)\n",
			     tile_str[t->tiling], a[0]);

		a[dwords - 1] = 0xc0ffee;
		igt_assert_f(a[dwords - 1] == 0xc0ffee,
			     "tiling %s: write failed at end (%x)\n",
			     tile_str[t->tiling], a[dwords - 1]);

		for (i = 0; i < dwords; i += CACHELINE/sizeof(uint32_t)) {
			for (int j = 0; j < CACHELINE/sizeof(uint32_t); j++)
				a[i + j] = ~(i + j);

			copy_wc_cacheline(tmp, a + i);
			for (int j = 0; j < CACHELINE/sizeof(uint32_t); j++)
				igt_assert_f(tmp[j] == ~(i+ j),
					     "tiling %s: write failed at %d (%x)\n",
					     tile_str[t->tiling], i + j, tmp[j]);

			for (int j = 0; j < CACHELINE/sizeof(uint32_t); j++)
				a[i + j] = i + j;
		}

		for (i = 0; i < dwords; i += PAGE_SIZE/sizeof(uint32_t)) {
			copy_wc_page(tmp, a + i);
			for (int j = 0; j < PAGE_SIZE/sizeof(uint32_t); j++) {
				igt_assert_f(tmp[j] == i + j,
					     "tiling %s: verify failed at %d (%x)\n",
					     tile_str[t->tiling], i + j, tmp[j]);
			}
		}
	}

	for (k = 0; k < t->num_surfaces; k++)
		munmap(s[k], OBJECT_SIZE);

	free(s);
}

static void *
bo_write_verify(void *_arg)
{
	struct test *t = (struct test *)_arg;
	int i;

	for (i = 0; i < 10; i++)
		_bo_write_verify(t);

	return 0;
}

static int run_test(int threads_per_fence, void *f, int tiling,
		    int surfaces_per_thread)
{
	struct test t;
	pthread_t *threads;
	int n, num_fences, num_threads;

	t.fd = drm_open_driver(DRIVER_INTEL);
	t.tiling = tiling;
	t.num_surfaces = surfaces_per_thread;

	num_fences = gem_available_fences(t.fd);
	igt_assert_lt(0, num_fences);

	num_threads = threads_per_fence * num_fences;

	igt_info("%s: threads %d, fences %d, tiling %d, surfaces per thread %d\n",
		 f == bo_copy ? "copy" : "write-verify", num_threads,
		 num_fences, tiling, surfaces_per_thread);

	if (threads_per_fence) {
		threads = calloc(sizeof(*threads), num_threads);
		igt_assert(threads != NULL);

		for (n = 0; n < num_threads; n++)
			pthread_create (&threads[n], NULL, f, &t);

		for (n = 0; n < num_threads; n++)
			pthread_join (threads[n], NULL);

		free(threads);
	} else {
		void *(*func)(void *) = f;
		igt_assert(func(&t) == (void *)0);
	}

	close(t.fd);

	return 0;
}

igt_main
{
	igt_skip_on_simulation();

	igt_subtest("bo-write-verify-none")
		igt_assert(run_test(0, bo_write_verify, I915_TILING_NONE, 80) == 0);

	igt_subtest("bo-write-verify-x")
		igt_assert(run_test(0, bo_write_verify, I915_TILING_X, 80) == 0);

	igt_subtest("bo-write-verify-y")
		igt_assert(run_test(0, bo_write_verify, I915_TILING_Y, 80) == 0);

	igt_subtest("bo-write-verify-threaded-none")
		igt_assert(run_test(5, bo_write_verify, I915_TILING_NONE, 2) == 0);

	igt_subtest("bo-write-verify-threaded-x") {
		igt_assert(run_test(2, bo_write_verify, I915_TILING_X, 2) == 0);
		igt_assert(run_test(5, bo_write_verify, I915_TILING_X, 2) == 0);
		igt_assert(run_test(10, bo_write_verify, I915_TILING_X, 2) == 0);
		igt_assert(run_test(20, bo_write_verify, I915_TILING_X, 2) == 0);
	}

	igt_subtest("bo-write-verify-threaded-y") {
		igt_assert(run_test(2, bo_write_verify, I915_TILING_Y, 2) == 0);
		igt_assert(run_test(5, bo_write_verify, I915_TILING_Y, 2) == 0);
		igt_assert(run_test(10, bo_write_verify, I915_TILING_Y, 2) == 0);
		igt_assert(run_test(20, bo_write_verify, I915_TILING_Y, 2) == 0);
	}

	igt_subtest("bo-copy")
		igt_assert(run_test(1, bo_copy, I915_TILING_X, 1) == 0);
}
