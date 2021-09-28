/*
 * Copyright © 2017 Broadcom
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
#include "igt_vc4.h"
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "vc4_drm.h"

struct igt_vc4_bo {
	struct igt_list node;
	int handle;
	void *map;
	size_t size;
};

static jmp_buf jmp;

static void __attribute__((noreturn)) sigtrap(int sig)
{
	longjmp(jmp, sig);
}

static void igt_vc4_alloc_mmap_max_bo(int fd, struct igt_list *list,
				      size_t size)
{
	struct igt_vc4_bo *bo;
	struct drm_vc4_create_bo create = {
		.size = size,
	};

	while (true) {
		if (igt_ioctl(fd, DRM_IOCTL_VC4_CREATE_BO, &create))
			break;

		bo = malloc(sizeof(*bo));
		igt_assert(bo);
		bo->handle = create.handle;
		bo->size = create.size;
		bo->map = igt_vc4_mmap_bo(fd, bo->handle, bo->size,
					  PROT_READ | PROT_WRITE);
		igt_list_add_tail(&bo->node, list);
	}
}

static void igt_vc4_unmap_free_bo_pool(int fd, struct igt_list *list)
{
	struct igt_vc4_bo *bo;

	while (!igt_list_empty(list)) {
		bo = igt_list_first_entry(list, bo, node);
		igt_assert(bo);
		igt_list_del(&bo->node);
		munmap(bo->map, bo->size);
		gem_close(fd, bo->handle);
		free(bo);
	}
}

static void igt_vc4_trigger_purge(int fd)
{
	struct igt_list list;

	igt_list_init(&list);

	/* Try to allocate as much as we can to trigger a purge. */
	igt_vc4_alloc_mmap_max_bo(fd, &list, 64 * 1024);
	igt_assert(!igt_list_empty(&list));
	igt_vc4_unmap_free_bo_pool(fd, &list);
}

static void igt_vc4_purgeable_subtest_prepare(int fd, struct igt_list *list)
{
	igt_vc4_unmap_free_bo_pool(fd, list);
	igt_vc4_alloc_mmap_max_bo(fd, list, 64 * 1024);
	igt_assert(!igt_list_empty(list));
}

igt_main
{
	struct igt_vc4_bo *bo;
	struct igt_list list;
	uint32_t *map;
	int fd, ret;

	igt_fixture {
		uint64_t val = 0;

		fd = drm_open_driver(DRIVER_VC4);
		igt_vc4_get_param(fd, DRM_VC4_PARAM_SUPPORTS_MADVISE, &val);
		igt_require(val);
		igt_list_init(&list);
	}

	igt_subtest("mark-willneed") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		igt_list_for_each(bo, &list, node)
			igt_assert(igt_vc4_purgeable_bo(fd, bo->handle,
							false));
	}

	igt_subtest("mark-purgeable") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		igt_list_for_each(bo, &list, node)
			igt_vc4_purgeable_bo(fd, bo->handle, true);

		igt_list_for_each(bo, &list, node)
			igt_vc4_purgeable_bo(fd, bo->handle, false);
	}

	igt_subtest("mark-purgeable-twice") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		bo = igt_list_first_entry(&list, bo, node);
		igt_vc4_purgeable_bo(fd, bo->handle, true);
		igt_vc4_purgeable_bo(fd, bo->handle, true);
		igt_vc4_purgeable_bo(fd, bo->handle, false);
	}

	igt_subtest("mark-unpurgeable-twice") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		bo = igt_list_first_entry(&list, bo, node);
		igt_vc4_purgeable_bo(fd, bo->handle, true);
		igt_vc4_purgeable_bo(fd, bo->handle, false);
		igt_vc4_purgeable_bo(fd, bo->handle, false);
	}

	igt_subtest("access-purgeable-bo-mem") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		bo = igt_list_first_entry(&list, bo, node);
		map = (uint32_t *)bo->map;

		/* Mark the BO as purgeable, but do not try to allocate a new
		 * BO. This should leave the BO in a non-purged state unless
		 * someone else tries to allocated a new BO.
		 */
		igt_vc4_purgeable_bo(fd, bo->handle, true);

		/* Accessing a purgeable BO should generate a SIGBUS event if
		 * the BO has been purged by the system in the meantime.
		 */
		signal(SIGSEGV, sigtrap);
		signal(SIGBUS, sigtrap);
		ret = setjmp(jmp);
		if (!ret)
			*map = 0xdeadbeef;
		else
			igt_assert(ret == SIGBUS);
		signal(SIGBUS, SIG_DFL);
		signal(SIGSEGV, SIG_DFL);
	}

	igt_subtest("access-purged-bo-mem") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);

		/* Mark the first BO in our list as purgeable and try to
		 * allocate a new one. This should trigger a purge and render
		 * the first BO inaccessible.
		 */
		bo = igt_list_first_entry(&list, bo, node);
		map = (uint32_t *)bo->map;
		igt_vc4_purgeable_bo(fd, bo->handle, true);

		/* Trigger a purge. */
		igt_vc4_trigger_purge(fd);

		/* Accessing a purged BO should generate a SIGBUS event. */
		signal(SIGSEGV, sigtrap);
		signal(SIGBUS, sigtrap);
		ret = setjmp(jmp);
		if (!ret)
			*map = 0;
		igt_assert(ret == SIGBUS);
		signal(SIGBUS, SIG_DFL);
		signal(SIGSEGV, SIG_DFL);
		igt_vc4_purgeable_bo(fd, bo->handle, false);
	}

	igt_subtest("mark-unpurgeable-check-retained") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		igt_list_for_each(bo, &list, node) {
			map = (uint32_t *)bo->map;
			*map = 0xdeadbeef;
			igt_vc4_purgeable_bo(fd, bo->handle, true);
		}

		igt_list_for_each(bo, &list, node) {
			map = (uint32_t *)bo->map;
			if (igt_vc4_purgeable_bo(fd, bo->handle, false))
				igt_assert(*map == 0xdeadbeef);
		}
	}

	igt_subtest("mark-unpurgeable-purged") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);

		igt_list_for_each(bo, &list, node)
			igt_vc4_purgeable_bo(fd, bo->handle, true);

		/* Trigger a purge. */
		igt_vc4_trigger_purge(fd);

		bo = igt_list_first_entry(&list, bo, node);
		map = (uint32_t *)bo->map;

		igt_assert(!igt_vc4_purgeable_bo(fd, bo->handle, false));

		/* Purged BOs are unusable and any access to their
		 * mmap-ed region should trigger a SIGBUS.
		 */
		signal(SIGSEGV, sigtrap);
		signal(SIGBUS, sigtrap);
		ret = setjmp(jmp);
		if (!ret)
			*map = 0;
		igt_assert(ret == SIGBUS);
		signal(SIGBUS, SIG_DFL);
		signal(SIGSEGV, SIG_DFL);
	}

	igt_subtest("free-purged-bo") {
		igt_vc4_purgeable_subtest_prepare(fd, &list);
		bo = igt_list_first_entry(&list, bo, node);
		igt_vc4_purgeable_bo(fd, bo->handle, true);

		/* Trigger a purge. */
		igt_vc4_trigger_purge(fd);

		igt_list_del(&bo->node);
		munmap(bo->map, bo->size);
		gem_close(fd, bo->handle);
		free(bo);
	}

	igt_fixture
		close(fd);
}
