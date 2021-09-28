/*
 * Copyright © 2017 Intel Corporation
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

#include "igt.h"
#include "igt_rand.h"
#include "igt_sysfs.h"
#include "igt_vgem.h"
#include "i915/gem_ring.h"

#include <sys/ioctl.h>
#include <sys/signal.h>

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_FLAGS  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

static double elapsed(const struct timespec *start, const struct timespec *end)
{
	return ((end->tv_sec - start->tv_sec) +
		(end->tv_nsec - start->tv_nsec)*1e-9);
}

static void xchg_obj(void *array, unsigned i, unsigned j)
{
	struct drm_i915_gem_exec_object2 *obj = array;
	uint64_t tmp;

	tmp = obj[i].handle;
	obj[i].handle = obj[j].handle;
	obj[j].handle = tmp;

	tmp = obj[i].offset;
	obj[i].offset = obj[j].offset;
	obj[j].offset = tmp;
}

#define CONTEXTS 0x1
static void wide(int fd, int ring_size, int timeout, unsigned int flags)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct {
		struct drm_i915_gem_exec_object2 *obj;
		struct drm_i915_gem_exec_object2 exec[2];
		struct drm_i915_gem_relocation_entry reloc;
		struct drm_i915_gem_execbuffer2 execbuf;
		uint32_t *cmd;
	} *exec;
	struct drm_i915_gem_exec_object2 *obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned engines[16];
	unsigned nengine, engine;
	unsigned long count;
	double time;

	nengine = 0;
	for_each_physical_engine(fd, engine)
		engines[nengine++] = engine;
	igt_require(nengine);

	exec = calloc(nengine, sizeof(*exec));
	igt_assert(exec);

	intel_require_memory(nengine*(2 + ring_size), 4096, CHECK_RAM);
	obj = calloc(nengine*ring_size + 1, sizeof(*obj));
	igt_assert(obj);

	for (unsigned e = 0; e < nengine; e++) {
		exec[e].obj = calloc(ring_size, sizeof(*exec[e].obj));
		igt_assert(exec[e].obj);
		for (unsigned n = 0; n < ring_size; n++)  {
			exec[e].obj[n].handle = gem_create(fd, 4096);
			exec[e].obj[n].flags = EXEC_OBJECT_WRITE;

			obj[e*ring_size + n].handle = exec[e].obj[n].handle;
		}

		exec[e].execbuf.buffers_ptr = to_user_pointer(exec[e].exec);
		exec[e].execbuf.buffer_count = 1;
		exec[e].execbuf.flags = (engines[e] |
					 LOCAL_I915_EXEC_NO_RELOC |
					 LOCAL_I915_EXEC_HANDLE_LUT);

		if (flags & CONTEXTS) {
			exec[e].execbuf.rsvd1 = gem_context_create(fd);
		}

		exec[e].exec[0].handle = gem_create(fd, 4096);
		exec[e].cmd = gem_mmap__wc(fd, exec[e].exec[0].handle,
					   0, 4096, PROT_WRITE);

		gem_set_domain(fd, exec[e].exec[0].handle,
			       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);
		exec[e].cmd[0] = MI_BATCH_BUFFER_END;

		gem_execbuf(fd, &exec[e].execbuf);
		exec[e].exec[1] = exec[e].exec[0];
		exec[e].execbuf.buffer_count = 2;

		exec[e].reloc.target_handle = 1; /* recurse */
		exec[e].reloc.offset = sizeof(uint32_t);
		exec[e].reloc.read_domains = I915_GEM_DOMAIN_COMMAND;
		if (gen < 4)
			exec[e].reloc.delta = 1;

		exec[e].exec[1].relocs_ptr = to_user_pointer(&exec[e].reloc);
		exec[e].exec[1].relocation_count = 1;
	}
	obj[nengine*ring_size].handle = gem_create(fd, 4096);
	gem_write(fd, obj[nengine*ring_size].handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj[nengine*ring_size]);
	execbuf.buffer_count = 1;
	gem_execbuf(fd, &execbuf); /* tag the object as a batch in the GTT */
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = nengine*ring_size + 1;

	intel_detect_and_clear_missed_interrupts(fd);

	time = 0;
	count = 0;
	igt_until_timeout(timeout) {
		struct timespec start, now;
		for (unsigned e = 0; e < nengine; e++) {
			uint64_t address;
			int i;

			if (flags & CONTEXTS) {
				gem_context_destroy(fd, exec[e].execbuf.rsvd1);
				exec[e].execbuf.rsvd1 = gem_context_create(fd);
			}

			exec[e].reloc.presumed_offset = exec[e].exec[1].offset;
			address = (exec[e].reloc.presumed_offset +
				   exec[e].reloc.delta);
			gem_set_domain(fd, exec[e].exec[1].handle,
				       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

			i = 0;
			exec[e].cmd[i] = MI_BATCH_BUFFER_START;
			if (gen >= 8) {
				exec[e].cmd[i] |= 1 << 8 | 1;
				exec[e].cmd[++i] = address;
				exec[e].cmd[++i] = address >> 32;
			} else if (gen >= 6) {
				exec[e].cmd[i] |= 1 << 8;
				exec[e].cmd[++i] = address;
			} else {
				exec[e].cmd[i] |= 2 << 6;
				exec[e].cmd[++i] = address;
			}

			exec[e].exec[0] = obj[nengine*ring_size];
			gem_execbuf(fd, &exec[e].execbuf);

			for (unsigned n = 0; n < ring_size; n++) {
				exec[e].exec[0] = exec[e].obj[n];
				gem_execbuf(fd, &exec[e].execbuf);
				exec[e].obj[n].offset = exec[e].exec[0].offset;
			}
		}

		igt_permute_array(obj, nengine*ring_size, xchg_obj);

		clock_gettime(CLOCK_MONOTONIC, &start);
		for (unsigned e = 0; e < nengine; e++) {
			execbuf.flags = (engines[e] |
					 LOCAL_I915_EXEC_NO_RELOC |
					 LOCAL_I915_EXEC_HANDLE_LUT);
			gem_execbuf(fd, &execbuf);
		}
		clock_gettime(CLOCK_MONOTONIC, &now);
		time += elapsed(&start, &now);
		count += nengine;

		for (unsigned e = 0; e < nengine; e++)
			exec[e].cmd[0] = MI_BATCH_BUFFER_END;
		__sync_synchronize();
	}

	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	igt_info("%s: %'lu cycles: %.3fus\n",
		 __func__, count, time*1e6 / count);

	gem_close(fd, obj[nengine*ring_size].handle);
	free(obj);

	for (unsigned e = 0; e < nengine; e++) {
		if (flags & CONTEXTS)
			gem_context_destroy(fd, exec[e].execbuf.rsvd1);

		for (unsigned n = 0; n < ring_size; n++)
			gem_close(fd, exec[e].obj[n].handle);
		free(exec[e].obj);

		munmap(exec[e].cmd, 4096);
		gem_close(fd, exec[e].exec[1].handle);
	}
	free(exec);
}

igt_main
{
	int ring_size = 0;
	int device = -1;

	igt_fixture {

		device = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(device);
		gem_submission_print_method(device);

		ring_size = gem_measure_ring_inflight(device, ALL_ENGINES, 0) - 10;
		if (!gem_has_execlists(device))
			ring_size /= 2;
		igt_info("Ring size: %d batches\n", ring_size);
		igt_require(ring_size > 0);

		igt_fork_hang_detector(device);
	}

	igt_subtest("wide-all")
		wide(device, ring_size, 20, 0);

	igt_subtest("wide-contexts") {
		gem_require_contexts(device);
		wide(device, ring_size, 20, CONTEXTS);
	}

	igt_fixture {
		igt_stop_hang_detector();
		close(device);
	}
}
