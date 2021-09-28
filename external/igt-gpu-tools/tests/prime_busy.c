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

#include "igt.h"

#include <sys/poll.h>

IGT_TEST_DESCRIPTION("Basic check of polling for prime fences.");

static bool prime_busy(struct pollfd *pfd, bool excl)
{
	pfd->events = excl ? POLLOUT : POLLIN;
	return poll(pfd, 1, 0) == 0;
}

#define BEFORE 0x1
#define AFTER 0x2
#define HANG 0x4
#define POLL 0x8

static void busy(int fd, unsigned ring, unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
	struct pollfd pfd[2];
#define SCRATCH 0
#define BATCH 1
	struct drm_i915_gem_relocation_entry store[1024+1];
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned size = ALIGN(ARRAY_SIZE(store)*16 + 4, 4096);
	struct timespec tv;
	uint32_t *batch, *bbe;
	int i, count, timeout;

	gem_quiescent_gpu(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uintptr_t)obj;
	execbuf.buffer_count = 2;
	execbuf.flags = ring;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = gem_create(fd, 4096);

	obj[BATCH].handle = gem_create(fd, size);
	obj[BATCH].relocs_ptr = (uintptr_t)store;
	obj[BATCH].relocation_count = ARRAY_SIZE(store);
	memset(store, 0, sizeof(store));

	if (flags & BEFORE) {
		memset(pfd, 0, sizeof(pfd));
		pfd[SCRATCH].fd = prime_handle_to_fd(fd, obj[SCRATCH].handle);
		pfd[BATCH].fd = prime_handle_to_fd(fd, obj[BATCH].handle);
	}

	batch = gem_mmap__wc(fd, obj[BATCH].handle, 0, size, PROT_WRITE);
	gem_set_domain(fd, obj[BATCH].handle,
			I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	i = 0;
	for (count = 0; count < 1024; count++) {
		store[count].target_handle = obj[SCRATCH].handle;
		store[count].presumed_offset = -1;
		store[count].offset = sizeof(uint32_t) * (i + 1);
		store[count].delta = sizeof(uint32_t) * count;
		store[count].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
		store[count].write_domain = I915_GEM_DOMAIN_INSTRUCTION;
		batch[i] = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
		if (gen >= 8) {
			batch[++i] = 0;
			batch[++i] = 0;
		} else if (gen >= 4) {
			batch[++i] = 0;
			batch[++i] = 0;
			store[count].offset += sizeof(uint32_t);
		} else {
			batch[i]--;
			batch[++i] = 0;
		}
		batch[++i] = count;
		i++;
	}

	bbe = &batch[i];
	store[count].target_handle = obj[BATCH].handle; /* recurse */
	store[count].presumed_offset = 0;
	store[count].offset = sizeof(uint32_t) * (i + 1);
	store[count].delta = 0;
	store[count].read_domains = I915_GEM_DOMAIN_COMMAND;
	store[count].write_domain = 0;
	batch[i] = MI_BATCH_BUFFER_START;
	if (gen >= 8) {
		batch[i] |= 1 << 8 | 1;
		batch[++i] = 0;
		batch[++i] = 0;
	} else if (gen >= 6) {
		batch[i] |= 1 << 8;
		batch[++i] = 0;
	} else {
		batch[i] |= 2 << 6;
		batch[++i] = 0;
		if (gen < 4) {
			batch[i] |= 1;
			store[count].delta = 1;
		}
	}
	i++;

	igt_assert(i < size/sizeof(*batch));
	igt_require(__gem_execbuf(fd, &execbuf) == 0);

	if (flags & AFTER) {
		memset(pfd, 0, sizeof(pfd));
		pfd[SCRATCH].fd = prime_handle_to_fd(fd, obj[SCRATCH].handle);
		pfd[BATCH].fd = prime_handle_to_fd(fd, obj[BATCH].handle);
	}

	igt_assert(prime_busy(&pfd[SCRATCH], false));
	igt_assert(prime_busy(&pfd[SCRATCH], true));

	igt_assert(!prime_busy(&pfd[BATCH], false));
	igt_assert(prime_busy(&pfd[BATCH], true));

	timeout = 120;
	if ((flags & HANG) == 0) {
		*bbe = MI_BATCH_BUFFER_END;
		__sync_synchronize();
		timeout = 1;
	}

	/* Calling busy in a loop should be enough to flush the rendering */
	if (flags & POLL) {
		pfd[BATCH].events = POLLOUT;
		igt_assert(poll(pfd, 1, timeout * 1000) == 1);
	} else {
		memset(&tv, 0, sizeof(tv));
		while (prime_busy(&pfd[BATCH], true))
			igt_assert(igt_seconds_elapsed(&tv) < timeout);
	}
	igt_assert(!prime_busy(&pfd[SCRATCH], true));

	munmap(batch, size);
	batch = gem_mmap__wc(fd, obj[SCRATCH].handle, 0, 4096, PROT_READ);
	for (i = 0; i < 1024; i++)
		igt_assert_eq_u32(batch[i], i);
	munmap(batch, 4096);

	gem_close(fd, obj[BATCH].handle);
	gem_close(fd, obj[SCRATCH].handle);

	close(pfd[BATCH].fd);
	close(pfd[SCRATCH].fd);
}

static void test_engine_mode(int fd,
			     const struct intel_execution_engine *e,
			     const char *name, unsigned int flags)
{
	igt_hang_t hang = {};

	igt_subtest_group {
		igt_fixture {
			gem_require_ring(fd, e->exec_id | e->flags);
			igt_require(gem_can_store_dword(fd, e->exec_id | e->flags));

			if ((flags & HANG) == 0)
			{
				igt_fork_hang_detector(fd);
			}
			else
			{
				igt_skip_on_simulation();
				hang = igt_allow_hang(fd, 0, 0);
			}
		}

		igt_subtest_f("%s%s-%s",
			      !e->exec_id && !(flags & HANG) ? "basic-" : "",
			      name, e->name)
			busy(fd, e->exec_id | e->flags, flags);

		igt_subtest_f("%swait-%s-%s",
			      !e->exec_id && !(flags & HANG) ? "basic-" : "",
			      name, e->name)
			busy(fd, e->exec_id | e->flags, flags | POLL);

		igt_fixture {
			if ((flags & HANG) == 0)
				igt_stop_hang_detector();
			else
				igt_disallow_hang(fd, hang);
		}
	}
}

igt_main
{
	const struct intel_execution_engine *e;
	int fd = -1;

	igt_fixture {
		fd = drm_open_driver_master(DRIVER_INTEL);
		igt_require_gem(fd);
	}

	igt_subtest_group {
		const struct mode {
			const char *name;
			unsigned int flags;
		} modes[] = {
			{ "before", BEFORE },
			{ "after", AFTER },
			{ "hang", BEFORE | HANG },
			{ NULL },
		};

		igt_fixture
			gem_require_mmap_wc(fd);

		for (e = intel_execution_engines; e->name; e++) {
			for (const struct mode *m = modes; m->name; m++)
				test_engine_mode(fd, e, m->name, m->flags);
		}
	}

	igt_fixture
		close(fd);
}
