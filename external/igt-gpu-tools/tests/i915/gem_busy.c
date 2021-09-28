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

#include <sched.h>
#include <signal.h>
#include <sys/ioctl.h>

#include "igt.h"
#include "igt_rand.h"
#include "igt_vgem.h"
#include "i915/gem_ring.h"

#define LOCAL_EXEC_NO_RELOC (1<<11)
#define PAGE_ALIGN(x) ALIGN(x, 4096)

/* Exercise the busy-ioctl, ensuring the ABI is never broken */
IGT_TEST_DESCRIPTION("Basic check of busy-ioctl ABI.");

enum { TEST = 0, BUSY, BATCH };

static bool gem_busy(int fd, uint32_t handle)
{
	struct drm_i915_gem_busy busy;

	memset(&busy, 0, sizeof(busy));
	busy.handle = handle;

	do_ioctl(fd, DRM_IOCTL_I915_GEM_BUSY, &busy);

	return busy.busy != 0;
}

static void __gem_busy(int fd,
		       uint32_t handle,
		       uint32_t *read,
		       uint32_t *write)
{
	struct drm_i915_gem_busy busy;

	memset(&busy, 0, sizeof(busy));
	busy.handle = handle;

	do_ioctl(fd, DRM_IOCTL_I915_GEM_BUSY, &busy);

	*write = busy.busy & 0xffff;
	*read = busy.busy >> 16;
}

static bool exec_noop(int fd,
		      uint32_t *handles,
		      unsigned flags,
		      bool write)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec[3];

	memset(exec, 0, sizeof(exec));
	exec[0].handle = handles[BUSY];
	exec[1].handle = handles[TEST];
	if (write)
		exec[1].flags |= EXEC_OBJECT_WRITE;
	exec[2].handle = handles[BATCH];

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(exec);
	execbuf.buffer_count = 3;
	execbuf.flags = flags;
	igt_debug("Queuing handle for %s on engine %d\n",
		  write ? "writing" : "reading", flags);
	return __gem_execbuf(fd, &execbuf) == 0;
}

static bool still_busy(int fd, uint32_t handle)
{
	uint32_t read, write;
	__gem_busy(fd, handle, &read, &write);
	return write;
}

static void semaphore(int fd, const struct intel_execution_engine2 *e)
{
	struct intel_execution_engine2 *__e;
	uint32_t bbe = MI_BATCH_BUFFER_END;
	const unsigned uabi = e->class;
	igt_spin_t *spin;
	uint32_t handle[3];
	uint32_t read, write;
	uint32_t active;
	unsigned i;

	handle[TEST] = gem_create(fd, 4096);
	handle[BATCH] = gem_create(fd, 4096);
	gem_write(fd, handle[BATCH], 0, &bbe, sizeof(bbe));

	/* Create a long running batch which we can use to hog the GPU */
	handle[BUSY] = gem_create(fd, 4096);
	spin = igt_spin_new(fd,
			    .engine = e->flags,
			    .dependency = handle[BUSY]);

	/* Queue a batch after the busy, it should block and remain "busy" */
	igt_assert(exec_noop(fd, handle, e->flags, false));
	igt_assert(still_busy(fd, handle[BUSY]));
	__gem_busy(fd, handle[TEST], &read, &write);
	igt_assert_eq(read, 1 << uabi);
	igt_assert_eq(write, 0);

	/* Requeue with a write */
	igt_assert(exec_noop(fd, handle, e->flags, true));
	igt_assert(still_busy(fd, handle[BUSY]));
	__gem_busy(fd, handle[TEST], &read, &write);
	igt_assert_eq(read, 1 << uabi);
	igt_assert_eq(write, 1 + uabi);

	/* Now queue it for a read across all available rings */
	active = 0;
	__for_each_physical_engine(fd, __e) {
		if (exec_noop(fd, handle, __e->flags, false))
			active |= 1 << __e->class;
	}
	igt_assert(still_busy(fd, handle[BUSY]));
	__gem_busy(fd, handle[TEST], &read, &write);
	igt_assert_eq(read, active);
	igt_assert_eq(write, 1 + uabi); /* from the earlier write */

	/* Check that our long batch was long enough */
	igt_assert(still_busy(fd, handle[BUSY]));
	igt_spin_free(fd, spin);

	/* And make sure it becomes idle again */
	gem_sync(fd, handle[TEST]);
	__gem_busy(fd, handle[TEST], &read, &write);
	igt_assert_eq(read, 0);
	igt_assert_eq(write, 0);

	for (i = TEST; i <= BATCH; i++)
		gem_close(fd, handle[i]);
}

#define PARALLEL 1
#define HANG 2
static void one(int fd, const struct intel_execution_engine2 *e, unsigned test_flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	struct drm_i915_gem_exec_object2 obj[2];
#define SCRATCH 0
#define BATCH 1
	struct drm_i915_gem_relocation_entry store[1024+1];
	struct drm_i915_gem_execbuffer2 execbuf;
	unsigned size = ALIGN(ARRAY_SIZE(store)*16 + 4, 4096);
	const unsigned uabi = e->class;
	uint32_t read[2], write[2];
	struct timespec tv;
	uint32_t *batch, *bbe;
	int i, count, timeout;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.flags = e->flags;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = gem_create(fd, 4096);

	obj[BATCH].handle = gem_create(fd, size);
	obj[BATCH].relocs_ptr = to_user_pointer(store);
	obj[BATCH].relocation_count = ARRAY_SIZE(store);
	memset(store, 0, sizeof(store));

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

	__gem_busy(fd, obj[SCRATCH].handle, &read[SCRATCH], &write[SCRATCH]);
	__gem_busy(fd, obj[BATCH].handle, &read[BATCH], &write[BATCH]);

	if (test_flags & PARALLEL) {
		struct intel_execution_engine2 *e2;

		__for_each_physical_engine(fd, e2) {
			if (e2->class == e->class &&
			    e2->instance == e->instance)
				continue;

			if (!gem_class_can_store_dword(fd, e2->class))
				continue;

			igt_debug("Testing %s in parallel\n", e2->name);
			one(fd, e2, 0);
		}
	}

	timeout = 120;
	if ((test_flags & HANG) == 0) {
		*bbe = MI_BATCH_BUFFER_END;
		__sync_synchronize();
		timeout = 1;
	}

	igt_assert_eq(write[SCRATCH], 1 + uabi);
	igt_assert_eq_u32(read[SCRATCH], 1 << uabi);

	igt_assert_eq(write[BATCH], 0);
	igt_assert_eq_u32(read[BATCH], 1 << uabi);

	/* Calling busy in a loop should be enough to flush the rendering */
	memset(&tv, 0, sizeof(tv));
	while (gem_busy(fd, obj[BATCH].handle))
		igt_assert(igt_seconds_elapsed(&tv) < timeout);
	igt_assert(!gem_busy(fd, obj[SCRATCH].handle));

	munmap(batch, size);
	batch = gem_mmap__wc(fd, obj[SCRATCH].handle, 0, 4096, PROT_READ);
	for (i = 0; i < 1024; i++)
		igt_assert_eq_u32(batch[i], i);
	munmap(batch, 4096);

	gem_close(fd, obj[BATCH].handle);
	gem_close(fd, obj[SCRATCH].handle);
}

static void xchg_u32(void *array, unsigned i, unsigned j)
{
	uint32_t *u32 = array;
	uint32_t tmp = u32[i];
	u32[i] = u32[j];
	u32[j] = tmp;
}

static void close_race(int fd)
{
	const unsigned int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	const unsigned int nhandles = gem_measure_ring_inflight(fd, ALL_ENGINES, 0) / 2;
	unsigned int engines[16], nengine;
	unsigned long *control;
	uint32_t *handles;
	int i;

	igt_require(ncpus > 1);
	intel_require_memory(nhandles, 4096, CHECK_RAM);

	/*
	 * One thread spawning work and randomly closing handles.
	 * One background thread per cpu checking busyness.
	 */

	nengine = 0;
	for_each_engine(fd, i)
		engines[nengine++] = i;
	igt_require(nengine);

	control = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(control != MAP_FAILED);

	handles = mmap(NULL, PAGE_ALIGN(nhandles*sizeof(*handles)),
		   PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(handles != MAP_FAILED);

	igt_fork(child, ncpus - 1) {
		struct drm_i915_gem_busy busy;
		uint32_t indirection[nhandles];
		unsigned long count = 0;

		for (i = 0; i < nhandles; i++)
			indirection[i] = i;

		hars_petruska_f54_1_random_perturb(child);

		memset(&busy, 0, sizeof(busy));
		do {
			igt_permute_array(indirection, nhandles, xchg_u32);
			__sync_synchronize();
			for (i = 0; i < nhandles; i++) {
				busy.handle = handles[indirection[i]];
				/* Check that the busy computation doesn't
				 * explode in the face of random gem_close().
				 */
				drmIoctl(fd, DRM_IOCTL_I915_GEM_BUSY, &busy);
			}
			count++;
		} while(*(volatile long *)control == 0);

		igt_debug("child[%d]: count = %lu\n", child, count);
		control[child + 1] = count;
	}

	igt_fork(child, 1) {
		struct sched_param rt = {.sched_priority = 99 };
		igt_spin_t *spin[nhandles];
		unsigned long count = 0;

		igt_assert(sched_setscheduler(getpid(), SCHED_RR, &rt) == 0);

		for (i = 0; i < nhandles; i++) {
			spin[i] = __igt_spin_new(fd,
						 .engine = engines[rand() % nengine]);
			handles[i] = spin[i]->handle;
		}

		igt_until_timeout(20) {
			for (i = 0; i < nhandles; i++) {
				igt_spin_free(fd, spin[i]);
				spin[i] = __igt_spin_new(fd,
							 .engine = engines[rand() % nengine]);
				handles[i] = spin[i]->handle;
				__sync_synchronize();
			}
			count += nhandles;
		}
		control[0] = count;
		__sync_synchronize();

		for (i = 0; i < nhandles; i++)
			igt_spin_free(fd, spin[i]);
	}
	igt_waitchildren();

	for (i = 0; i < ncpus - 1; i++)
		control[ncpus] += control[i + 1];
	igt_info("Total execs %lu, busy-ioctls %lu\n",
		 control[0], control[ncpus] * nhandles);

	munmap(handles, PAGE_ALIGN(nhandles * sizeof(*handles)));
	munmap(control, 4096);

	gem_quiescent_gpu(fd);
}

static bool has_semaphores(int fd)
{
	struct drm_i915_getparam gp;
	int val = -1;

	memset(&gp, 0, sizeof(gp));
	gp.param = I915_PARAM_HAS_SEMAPHORES;
	gp.value = &val;

	drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
	errno = 0;

	return val > 0;
}

static bool has_extended_busy_ioctl(int fd)
{
	igt_spin_t *spin = igt_spin_new(fd, .engine = I915_EXEC_DEFAULT);
	uint32_t read, write;

	__gem_busy(fd, spin->handle, &read, &write);
	igt_spin_free(fd, spin);

	return read != 0;
}

static void basic(int fd, const struct intel_execution_engine2 *e, unsigned flags)
{
	igt_spin_t *spin =
		igt_spin_new(fd,
			     .engine = e->flags,
			     .flags = IGT_SPIN_NO_PREEMPTION);
	struct timespec tv;
	int timeout;
	bool busy;

	busy = gem_bo_busy(fd, spin->handle);

	timeout = 120;
	if ((flags & HANG) == 0) {
		igt_spin_end(spin);
		timeout = 1;
	}

	igt_assert(busy);
	memset(&tv, 0, sizeof(tv));
	while (gem_bo_busy(fd, spin->handle)) {
		if (igt_seconds_elapsed(&tv) > timeout) {
			igt_debugfs_dump(fd, "i915_engine_info");
			igt_debugfs_dump(fd, "i915_hangcheck_info");
			igt_assert_f(igt_seconds_elapsed(&tv) < timeout,
				     "%s batch did not complete within %ds\n",
				     flags & HANG ? "Hanging" : "Normal",
				     timeout);
		}
	}

	igt_spin_free(fd, spin);
}

static void all(int i915)
{
	const struct intel_execution_engine2 *e;

	__for_each_physical_engine(i915, e)
		basic(i915, e, 0);
}

igt_main
{
	const struct intel_execution_engine2 *e;
	int fd = -1;

	igt_fixture {
		fd = drm_open_driver_master(DRIVER_INTEL);
		igt_require_gem(fd);
		igt_require(gem_class_can_store_dword(fd,
						     I915_ENGINE_CLASS_RENDER));
	}

	igt_subtest_group {
		igt_fixture {
			igt_fork_hang_detector(fd);
		}

		igt_subtest("busy-all") {
			gem_quiescent_gpu(fd);
			all(fd);
		}

		__for_each_physical_engine(fd, e) {
			igt_subtest_group {
				igt_subtest_f("busy-%s", e->name) {
					gem_quiescent_gpu(fd);
					basic(fd, e, 0);
				}
			}
		}

		igt_subtest_group {
			igt_fixture {
				igt_require(has_extended_busy_ioctl(fd));
				gem_require_mmap_wc(fd);
			}

			__for_each_physical_engine(fd, e) {
				igt_subtest_f("extended-%s", e->name) {
					igt_require(gem_class_can_store_dword(fd,
						     e->class));
					gem_quiescent_gpu(fd);
					one(fd, e, 0);
					gem_quiescent_gpu(fd);
				}
			}

			__for_each_physical_engine(fd, e) {
				igt_subtest_f("extended-parallel-%s", e->name) {
					igt_require(gem_class_can_store_dword(fd, e->class));

					gem_quiescent_gpu(fd);
					one(fd, e, PARALLEL);
					gem_quiescent_gpu(fd);
				}
			}
		}

		igt_subtest_group {
			igt_fixture {
				igt_require(has_extended_busy_ioctl(fd));
				igt_require(has_semaphores(fd));
			}

			__for_each_physical_engine(fd, e) {
				igt_subtest_f("extended-semaphore-%s", e->name)
					semaphore(fd, e);
			}
		}

		igt_subtest("close-race")
			close_race(fd);

		igt_fixture {
			igt_stop_hang_detector();
		}
	}

	igt_subtest_group {
		igt_hang_t hang;

		igt_fixture {
			hang = igt_allow_hang(fd, 0, 0);
		}

		__for_each_physical_engine(fd, e) {
			igt_subtest_f("%shang-%s",
				      e->class == I915_ENGINE_CLASS_RENDER
				      ? "basic-" : "", e->name) {
				igt_skip_on_simulation();
				gem_quiescent_gpu(fd);
				basic(fd, e, HANG);
			}
		}

		igt_subtest_group {
			igt_fixture {
				igt_require(has_extended_busy_ioctl(fd));
				gem_require_mmap_wc(fd);
			}

			__for_each_physical_engine(fd, e) {
				igt_subtest_f("extended-hang-%s", e->name) {
					igt_skip_on_simulation();
					igt_require(gem_class_can_store_dword(fd, e->class));

					gem_quiescent_gpu(fd);
					one(fd, e, HANG);
					gem_quiescent_gpu(fd);
				}
			}
		}

		igt_fixture {
			igt_disallow_hang(fd, hang);
		}
	}

	igt_fixture {
		close(fd);
	}
}
