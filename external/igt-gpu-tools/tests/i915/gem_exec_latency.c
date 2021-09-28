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
 *
 */

#include "igt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <time.h>
#include <sched.h>

#include "drm.h"

#include "igt_sysfs.h"
#include "igt_vgem.h"
#include "igt_dummyload.h"
#include "igt_stats.h"

#include "i915/gem_ring.h"

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_FLAGS  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

#define LIVE 0x1
#define CORK 0x2
#define PREEMPT 0x4

static unsigned int ring_size;
static double rcs_clock;

static void
poll_ring(int fd, unsigned ring, const char *name)
{
	const struct igt_spin_factory opts = {
		.engine = ring,
		.flags = IGT_SPIN_POLL_RUN | IGT_SPIN_FAST,
	};
	struct timespec tv = {};
	unsigned long cycles;
	igt_spin_t *spin[2];
	uint64_t elapsed;

	gem_require_ring(fd, ring);
	igt_require(gem_can_store_dword(fd, ring));

	spin[0] = __igt_spin_factory(fd, &opts);
	igt_assert(igt_spin_has_poll(spin[0]));

	spin[1] = __igt_spin_factory(fd, &opts);
	igt_assert(igt_spin_has_poll(spin[1]));

	igt_spin_end(spin[0]);
	igt_spin_busywait_until_started(spin[1]);

	igt_assert(!gem_bo_busy(fd, spin[0]->handle));

	cycles = 0;
	while ((elapsed = igt_nsec_elapsed(&tv)) < 2ull << 30) {
		const unsigned int idx = cycles++ & 1;

		igt_spin_reset(spin[idx]);

		gem_execbuf(fd, &spin[idx]->execbuf);

		igt_spin_end(spin[!idx]);
		igt_spin_busywait_until_started(spin[idx]);
	}

	igt_info("%s completed %ld cycles: %.3f us\n",
		 name, cycles, elapsed*1e-3/cycles);

	igt_spin_free(fd, spin[1]);
	igt_spin_free(fd, spin[0]);
}

#define RCS_TIMESTAMP (0x2000 + 0x358)
static void latency_on_ring(int fd,
			    unsigned ring, const char *name,
			    unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const int has_64bit_reloc = gen >= 8;
	struct drm_i915_gem_exec_object2 obj[3];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	igt_spin_t *spin = NULL;
	IGT_CORK_HANDLE(c);
	volatile uint32_t *reg;
	unsigned repeats = ring_size;
	uint32_t start, end, *map, *results;
	uint64_t offset;
	double gpu_latency;
	int i, j;

	reg = (volatile uint32_t *)((volatile char *)igt_global_mmio + RCS_TIMESTAMP);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj[1]);
	execbuf.buffer_count = 2;
	execbuf.flags = ring;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC | LOCAL_I915_EXEC_HANDLE_LUT;

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(fd, 4096);
	obj[1].flags = EXEC_OBJECT_WRITE;
	results = gem_mmap__wc(fd, obj[1].handle, 0, 4096, PROT_READ);

	obj[2].handle = gem_create(fd, 64*1024);
	map = gem_mmap__wc(fd, obj[2].handle, 0, 64*1024, PROT_WRITE);
	gem_set_domain(fd, obj[2].handle,
		       I915_GEM_DOMAIN_GTT,
		       I915_GEM_DOMAIN_GTT);
	map[0] = MI_BATCH_BUFFER_END;
	gem_execbuf(fd, &execbuf);

	memset(&reloc,0, sizeof(reloc));
	obj[2].relocation_count = 1;
	obj[2].relocs_ptr = to_user_pointer(&reloc);

	gem_set_domain(fd, obj[2].handle,
		       I915_GEM_DOMAIN_GTT,
		       I915_GEM_DOMAIN_GTT);

	reloc.target_handle = flags & CORK ? 1 : 0;
	reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.write_domain = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.presumed_offset = obj[1].offset;

	for (j = 0; j < repeats; j++) {
		execbuf.batch_start_offset = 64 * j;
		reloc.offset =
			execbuf.batch_start_offset + sizeof(uint32_t);
		reloc.delta = sizeof(uint32_t) * j;

		offset = reloc.presumed_offset;
		offset += reloc.delta;

		i = 16 * j;
		/* MI_STORE_REG_MEM */
		map[i++] = 0x24 << 23 | 1;
		if (has_64bit_reloc)
			map[i-1]++;
		map[i++] = RCS_TIMESTAMP; /* ring local! */
		map[i++] = offset;
		if (has_64bit_reloc)
			map[i++] = offset >> 32;
		map[i++] = MI_BATCH_BUFFER_END;
	}

	if (flags & CORK) {
		obj[0].handle = igt_cork_plug(&c, fd);
		execbuf.buffers_ptr = to_user_pointer(&obj[0]);
		execbuf.buffer_count = 3;
	}

	if (flags & LIVE)
		spin = igt_spin_new(fd, .engine = ring);

	start = *reg;
	for (j = 0; j < repeats; j++) {
		uint64_t presumed_offset = reloc.presumed_offset;

		execbuf.batch_start_offset = 64 * j;
		reloc.offset =
			execbuf.batch_start_offset + sizeof(uint32_t);
		reloc.delta = sizeof(uint32_t) * j;

		gem_execbuf(fd, &execbuf);
		igt_assert(reloc.presumed_offset == presumed_offset);
	}
	end = *reg;
	igt_assert(reloc.presumed_offset == obj[1].offset);

	igt_spin_free(fd, spin);
	if (flags & CORK)
		igt_cork_unplug(&c);

	gem_set_domain(fd, obj[1].handle, I915_GEM_DOMAIN_GTT, 0);
	gpu_latency = (results[repeats-1] - results[0]) / (double)(repeats-1);

	gem_set_domain(fd, obj[2].handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	execbuf.batch_start_offset = 0;
	for (j = 0; j < repeats - 1; j++) {
		offset = obj[2].offset;
		offset += 64 * (j + 1);

		i = 16 * j + (has_64bit_reloc ? 4 : 3);
		map[i] = MI_BATCH_BUFFER_START;
		if (gen >= 8) {
			map[i] |= 1 << 8 | 1;
			map[i + 1] = offset;
			map[i + 2] = offset >> 32;
		} else if (gen >= 6) {
			map[i] |= 1 << 8;
			map[i + 1] = offset;
		} else {
			map[i] |= 2 << 6;
			map[i + 1] = offset;
			if (gen < 4)
				map[i] |= 1;
		}
	}
	offset = obj[2].offset;
	gem_execbuf(fd, &execbuf);
	igt_assert(offset == obj[2].offset);

	gem_set_domain(fd, obj[1].handle, I915_GEM_DOMAIN_GTT, 0);
	igt_info("%s: dispatch latency: %.1fns, execution latency: %.1fns (target %.1fns)\n",
		 name,
		 (end - start) / (double)repeats * rcs_clock,
		 gpu_latency * rcs_clock,
		 (results[repeats - 1] - results[0]) / (double)(repeats - 1) * rcs_clock);

	munmap(map, 64*1024);
	munmap(results, 4096);
	if (flags & CORK)
		gem_close(fd, obj[0].handle);
	gem_close(fd, obj[1].handle);
	gem_close(fd, obj[2].handle);
}

static void latency_from_ring(int fd,
			      unsigned ring, const char *name,
			      unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const int has_64bit_reloc = gen >= 8;
	struct drm_i915_gem_exec_object2 obj[3];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	const unsigned int repeats = ring_size / 2;
	unsigned int other;
	uint32_t *map, *results;
	uint32_t ctx[2] = {};
	int i, j;

	if (flags & PREEMPT) {
		ctx[0] = gem_context_create(fd);
		gem_context_set_priority(fd, ctx[0], -1023);

		ctx[1] = gem_context_create(fd);
		gem_context_set_priority(fd, ctx[1], 1023);
	}

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj[1]);
	execbuf.buffer_count = 2;
	execbuf.flags = ring;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC | LOCAL_I915_EXEC_HANDLE_LUT;
	execbuf.rsvd1 = ctx[1];

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(fd, 4096);
	obj[1].flags = EXEC_OBJECT_WRITE;
	results = gem_mmap__wc(fd, obj[1].handle, 0, 4096, PROT_READ);

	obj[2].handle = gem_create(fd, 64*1024);
	map = gem_mmap__wc(fd, obj[2].handle, 0, 64*1024, PROT_WRITE);
	gem_set_domain(fd, obj[2].handle,
		       I915_GEM_DOMAIN_GTT,
		       I915_GEM_DOMAIN_GTT);
	map[0] = MI_BATCH_BUFFER_END;
	gem_execbuf(fd, &execbuf);

	memset(&reloc,0, sizeof(reloc));
	obj[2].relocation_count = 1;
	obj[2].relocs_ptr = to_user_pointer(&reloc);

	gem_set_domain(fd, obj[2].handle,
		       I915_GEM_DOMAIN_GTT,
		       I915_GEM_DOMAIN_GTT);

	reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.write_domain = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.presumed_offset = obj[1].offset;
	reloc.target_handle = flags & CORK ? 1 : 0;

	for_each_physical_engine(fd, other) {
		igt_spin_t *spin = NULL;
		IGT_CORK_HANDLE(c);

		gem_set_domain(fd, obj[2].handle,
			       I915_GEM_DOMAIN_GTT,
			       I915_GEM_DOMAIN_GTT);

		if (flags & PREEMPT)
			spin = __igt_spin_new(fd,
					      .ctx = ctx[0],
					      .engine = ring);

		if (flags & CORK) {
			obj[0].handle = igt_cork_plug(&c, fd);
			execbuf.buffers_ptr = to_user_pointer(&obj[0]);
			execbuf.buffer_count = 3;
		}

		for (j = 0; j < repeats; j++) {
			uint64_t offset;

			execbuf.flags &= ~ENGINE_FLAGS;
			execbuf.flags |= ring;

			execbuf.batch_start_offset = 64 * j;
			reloc.offset =
				execbuf.batch_start_offset + sizeof(uint32_t);
			reloc.delta = sizeof(uint32_t) * j;

			reloc.presumed_offset = obj[1].offset;
			offset = reloc.presumed_offset;
			offset += reloc.delta;

			i = 16 * j;
			/* MI_STORE_REG_MEM */
			map[i++] = 0x24 << 23 | 1;
			if (has_64bit_reloc)
				map[i-1]++;
			map[i++] = RCS_TIMESTAMP; /* ring local! */
			map[i++] = offset;
			if (has_64bit_reloc)
				map[i++] = offset >> 32;
			map[i++] = MI_BATCH_BUFFER_END;

			gem_execbuf(fd, &execbuf);

			execbuf.flags &= ~ENGINE_FLAGS;
			execbuf.flags |= other;

			execbuf.batch_start_offset = 64 * (j + repeats);
			reloc.offset =
				execbuf.batch_start_offset + sizeof(uint32_t);
			reloc.delta = sizeof(uint32_t) * (j + repeats);

			reloc.presumed_offset = obj[1].offset;
			offset = reloc.presumed_offset;
			offset += reloc.delta;

			i = 16 * (j + repeats);
			/* MI_STORE_REG_MEM */
			map[i++] = 0x24 << 23 | 1;
			if (has_64bit_reloc)
				map[i-1]++;
			map[i++] = RCS_TIMESTAMP; /* ring local! */
			map[i++] = offset;
			if (has_64bit_reloc)
				map[i++] = offset >> 32;
			map[i++] = MI_BATCH_BUFFER_END;

			gem_execbuf(fd, &execbuf);
		}

		if (flags & CORK)
			igt_cork_unplug(&c);
		gem_set_domain(fd, obj[1].handle,
			       I915_GEM_DOMAIN_GTT,
			       I915_GEM_DOMAIN_GTT);
		igt_spin_free(fd, spin);

		igt_info("%s-%s delay: %.2fns\n",
			 name, e__->name,
			 (results[2*repeats-1] - results[0]) / (double)repeats * rcs_clock);
	}

	munmap(map, 64*1024);
	munmap(results, 4096);

	if (flags & CORK)
		gem_close(fd, obj[0].handle);
	gem_close(fd, obj[1].handle);
	gem_close(fd, obj[2].handle);

	if (flags & PREEMPT) {
		gem_context_destroy(fd, ctx[1]);
		gem_context_destroy(fd, ctx[0]);
	}
}

static void
__submit_spin(int fd, igt_spin_t *spin, unsigned int flags)
{
	struct drm_i915_gem_execbuffer2 eb = spin->execbuf;

	eb.flags &= ~(0x3f | I915_EXEC_BSD_MASK);
	eb.flags |= flags | I915_EXEC_NO_RELOC;

	gem_execbuf(fd, &eb);
}

struct rt_pkt {
	struct igt_mean mean;
	double min, max;
};

static bool __spin_wait(int fd, igt_spin_t *spin)
{
	while (!igt_spin_has_started(spin)) {
		if (!gem_bo_busy(fd, spin->handle))
			return false;
	}

	return true;
}

/*
 * Test whether RT thread which hogs the CPU a lot can submit work with
 * reasonable latency.
 */
static void
rthog_latency_on_ring(int fd, unsigned int engine, const char *name, unsigned int flags)
#define RTIDLE 0x1
{
	const char *passname[] = {
		"warmup",
		"normal",
		"rt[0]",
		"rt[1]",
		"rt[2]",
		"rt[3]",
		"rt[4]",
		"rt[5]",
		"rt[6]",
	};
#define NPASS ARRAY_SIZE(passname)
#define MMAP_SZ (64 << 10)
	const struct igt_spin_factory opts = {
		.engine = engine,
		.flags = IGT_SPIN_POLL_RUN | IGT_SPIN_FAST,
	};
	struct rt_pkt *results;
	unsigned int engines[16];
	const char *names[16];
	unsigned int nengine;
	int ret;

	igt_assert(ARRAY_SIZE(engines) * NPASS * sizeof(*results) <= MMAP_SZ);
	results = mmap(NULL, MMAP_SZ, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(results != MAP_FAILED);

	nengine = 0;
	if (engine == ALL_ENGINES) {
		for_each_physical_engine(fd, engine) {
			if (!gem_can_store_dword(fd, engine))
				continue;

			engines[nengine] = engine;
			names[nengine] = e__->name;
			nengine++;
		}
		igt_require(nengine > 1);
	} else {
		igt_require(gem_can_store_dword(fd, engine));
		engines[nengine] = engine;
		names[nengine] = name;
		nengine++;
	}

	gem_quiescent_gpu(fd);

	igt_fork(child, nengine) {
		unsigned int pass = 0; /* Three phases: warmup, normal, rt. */

		engine = engines[child];
		do {
			struct igt_mean mean;
			double min = HUGE_VAL;
			double max = -HUGE_VAL;
			igt_spin_t *spin;

			igt_mean_init(&mean);

			if (pass == 2) {
				struct sched_param rt =
				{ .sched_priority = 99 };

				ret = sched_setscheduler(0,
							 SCHED_FIFO | SCHED_RESET_ON_FORK,
							 &rt);
				if (ret) {
					igt_warn("Failed to set scheduling policy!\n");
					break;
				}
			}

			usleep(250);

			spin = __igt_spin_factory(fd, &opts);
			if (!spin) {
				igt_warn("Failed to create spinner! (%s)\n",
					 passname[pass]);
				break;
			}
			igt_spin_busywait_until_started(spin);

			igt_until_timeout(pass > 0 ? 5 : 2) {
				struct timespec ts = { };
				double t;

				igt_spin_end(spin);
				gem_sync(fd, spin->handle);
				if (flags & RTIDLE)
					igt_drop_caches_set(fd, DROP_IDLE);

				/*
				 * If we are oversubscribed (more RT hogs than
				 * cpus) give the others a change to run;
				 * otherwise, they will interrupt us in the
				 * middle of the measurement.
				 */
				if (nengine > 1)
					usleep(10*nengine);

				igt_spin_reset(spin);

				igt_nsec_elapsed(&ts);
				__submit_spin(fd, spin, engine);
				if (!__spin_wait(fd, spin)) {
					igt_warn("Wait timeout! (%s)\n",
						 passname[pass]);
					break;
				}

				t = igt_nsec_elapsed(&ts) * 1e-9;
				if (t > max)
					max = t;
				if (t < min)
					min = t;

				igt_mean_add(&mean, t);
			}

			igt_spin_free(fd, spin);

			igt_info("%8s %10s: mean=%.2fus stddev=%.3fus [%.2fus, %.2fus] (n=%lu)\n",
				 names[child],
				 passname[pass],
				 igt_mean_get(&mean) * 1e6,
				 sqrt(igt_mean_get_variance(&mean)) * 1e6,
				 min * 1e6, max * 1e6,
				 mean.count);

			results[NPASS * child + pass].mean = mean;
			results[NPASS * child + pass].min = min;
			results[NPASS * child + pass].max = max;
		} while (++pass < NPASS);
	}

	igt_waitchildren();

	for (unsigned int child = 0; child < nengine; child++) {
		struct rt_pkt normal = results[NPASS * child + 1];
		igt_stats_t stats;
		double variance;

		igt_stats_init_with_size(&stats, NPASS);

		variance = 0;
		for (unsigned int pass = 2; pass < NPASS; pass++) {
			struct rt_pkt *rt = &results[NPASS * child + pass];

			igt_assert(rt->max);

			igt_stats_push_float(&stats, igt_mean_get(&rt->mean));
			variance += igt_mean_get_variance(&rt->mean);
		}
		variance /= NPASS - 2;

		igt_info("%8s: normal latency=%.2f±%.3fus, rt latency=%.2f±%.3fus\n",
			 names[child],
			 igt_mean_get(&normal.mean) * 1e6,
			 sqrt(igt_mean_get_variance(&normal.mean)) * 1e6,
			 igt_stats_get_median(&stats) * 1e6,
			 sqrt(variance) * 1e6);

		igt_assert(igt_stats_get_median(&stats) <
			   igt_mean_get(&normal.mean) * 2);

		/* The system is noisy; be conservative when declaring fail. */
		igt_assert(variance < igt_mean_get_variance(&normal.mean) * 10);
	}

	munmap(results, MMAP_SZ);
}

static double clockrate(int i915, int reg)
{
	volatile uint32_t *mmio;
	uint32_t r_start, r_end;
	struct timespec tv;
	uint64_t t_start, t_end;
	uint64_t elapsed;
	int cs_timestamp_freq;
	drm_i915_getparam_t gp = {
		.value = &cs_timestamp_freq,
		.param = I915_PARAM_CS_TIMESTAMP_FREQUENCY,
	};

	if (igt_ioctl(i915, DRM_IOCTL_I915_GETPARAM, &gp) == 0)
		return cs_timestamp_freq;

	mmio = (volatile uint32_t *)((volatile char *)igt_global_mmio + reg);

	t_start = igt_nsec_elapsed(&tv);
	r_start = *mmio;
	elapsed = igt_nsec_elapsed(&tv) - t_start;

	usleep(1000);

	t_end = igt_nsec_elapsed(&tv);
	r_end = *mmio;
	elapsed += igt_nsec_elapsed(&tv) - t_end;

	elapsed = (t_end - t_start) + elapsed / 2;
	return (r_end - r_start) * 1e9 / elapsed;
}

igt_main
{
	const struct intel_execution_engine *e;
	int device = -1;

	igt_fixture {
		device = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(device);
		gem_require_mmap_wc(device);

		gem_submission_print_method(device);

		ring_size = gem_measure_ring_inflight(device, ALL_ENGINES, 0);
		igt_info("Ring size: %d batches\n", ring_size);
		igt_require(ring_size > 8);
		ring_size -= 8; /* leave some spare */
		if (ring_size > 1024)
			ring_size = 1024;

		intel_register_access_init(intel_get_pci_device(), false, device);
		rcs_clock = clockrate(device, RCS_TIMESTAMP);
		igt_info("RCS timestamp clock: %.0fKHz, %.1fns\n",
			 rcs_clock / 1e3, 1e9 / rcs_clock);
		rcs_clock = 1e9 / rcs_clock;
	}

	igt_subtest("all-rtidle-submit")
		rthog_latency_on_ring(device, ALL_ENGINES, "all", RTIDLE);

	igt_subtest("all-rthog-submit")
		rthog_latency_on_ring(device, ALL_ENGINES, "all", 0);

	igt_subtest_group {
		igt_fixture
			igt_require(intel_gen(intel_get_drm_devid(device)) >= 7);

		for (e = intel_execution_engines; e->name; e++) {
			if (e->exec_id == 0)
				continue;

			igt_subtest_group {
				igt_fixture {
					igt_require(gem_ring_has_physical_engine(device, e->exec_id | e->flags));
				}

				igt_subtest_f("%s-dispatch", e->name)
					latency_on_ring(device,
							e->exec_id | e->flags,
							e->name, 0);

				igt_subtest_f("%s-live-dispatch", e->name)
					latency_on_ring(device,
							e->exec_id | e->flags,
							e->name, LIVE);

				igt_subtest_f("%s-poll", e->name)
					poll_ring(device,
						  e->exec_id | e->flags,
						  e->name);

				igt_subtest_f("%s-rtidle-submit", e->name)
					rthog_latency_on_ring(device,
							      e->exec_id |
							      e->flags,
							      e->name,
							      RTIDLE);

				igt_subtest_f("%s-rthog-submit", e->name)
					rthog_latency_on_ring(device,
							      e->exec_id |
							      e->flags,
							      e->name,
							      0);

				igt_subtest_f("%s-live-dispatch-queued", e->name)
					latency_on_ring(device,
							e->exec_id | e->flags,
							e->name, LIVE | CORK);
				igt_subtest_f("%s-dispatch-queued", e->name)
					latency_on_ring(device,
							e->exec_id | e->flags,
							e->name, CORK);

				igt_subtest_f("%s-synchronisation", e->name)
					latency_from_ring(device,
							  e->exec_id | e->flags,
							  e->name, 0);

				igt_subtest_f("%s-synchronisation-queued", e->name)
					latency_from_ring(device,
							  e->exec_id | e->flags,
							  e->name, CORK);

				igt_subtest_group {
					igt_fixture {
						gem_require_contexts(device);
						igt_require(gem_scheduler_has_preemption(device));
					}

					igt_subtest_f("%s-preemption", e->name)
						latency_from_ring(device,
								  e->exec_id | e->flags,
								  e->name, PREEMPT);
				}
			}
		}
	}

	igt_fixture {
		close(device);
	}
}
