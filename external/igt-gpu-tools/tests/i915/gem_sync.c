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

#include <time.h>
#include <pthread.h>

#include "igt.h"
#include "igt_sysfs.h"

#define LOCAL_I915_EXEC_NO_RELOC (1<<11)
#define LOCAL_I915_EXEC_HANDLE_LUT (1<<12)

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define MAX_PRIO LOCAL_I915_CONTEXT_MAX_USER_PRIORITY
#define MIN_PRIO LOCAL_I915_CONTEXT_MIN_USER_PRIORITY

#define ENGINE_MASK  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

IGT_TEST_DESCRIPTION("Basic check of ring<->ring write synchronisation.");

/*
 * Testcase: Basic check of sync
 *
 * Extremely efficient at catching missed irqs
 */

static double gettime(void)
{
	static clockid_t clock = -1;
	struct timespec ts;

	/* Stay on the same clock for consistency. */
	if (clock != (clockid_t)-1) {
		if (clock_gettime(clock, &ts))
			goto error;
		goto out;
	}

#ifdef CLOCK_MONOTONIC_RAW
	if (!clock_gettime(clock = CLOCK_MONOTONIC_RAW, &ts))
		goto out;
#endif
#ifdef CLOCK_MONOTONIC_COARSE
	if (!clock_gettime(clock = CLOCK_MONOTONIC_COARSE, &ts))
		goto out;
#endif
	if (!clock_gettime(clock = CLOCK_MONOTONIC, &ts))
		goto out;
error:
	igt_warn("Could not read monotonic time: %s\n",
			strerror(errno));
	igt_assert(0);
	return 0;

out:
	return ts.tv_sec + 1e-9*ts.tv_nsec;
}

static void
sync_ring(int fd, unsigned ring, int num_children, int timeout)
{
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}

		num_children *= num_engines;
	} else {
		gem_require_ring(fd, ring);
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_children) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object;
		struct drm_i915_gem_execbuffer2 execbuf;
		double start, elapsed;
		unsigned long cycles;

		memset(&object, 0, sizeof(object));
		object.handle = gem_create(fd, 4096);
		gem_write(fd, object.handle, 0, &bbe, sizeof(bbe));

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(&object);
		execbuf.buffer_count = 1;
		execbuf.flags = engines[child % num_engines];
		gem_execbuf(fd, &execbuf);
		gem_sync(fd, object.handle);

		start = gettime();
		cycles = 0;
		do {
			do {
				gem_execbuf(fd, &execbuf);
				gem_sync(fd, object.handle);
			} while (++cycles & 1023);
		} while ((elapsed = gettime() - start) < timeout);
		igt_info("%s%sompleted %ld cycles: %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, elapsed*1e6/cycles);

		gem_close(fd, object.handle);
	}
	igt_waitchildren_timeout(timeout+10, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void
idle_ring(int fd, unsigned ring, int timeout)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 object;
	struct drm_i915_gem_execbuffer2 execbuf;
	double start, elapsed;
	unsigned long cycles;

	gem_require_ring(fd, ring);

	memset(&object, 0, sizeof(object));
	object.handle = gem_create(fd, 4096);
	gem_write(fd, object.handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&object);
	execbuf.buffer_count = 1;
	execbuf.flags = ring;
	gem_execbuf(fd, &execbuf);
	gem_sync(fd, object.handle);

	intel_detect_and_clear_missed_interrupts(fd);
	start = gettime();
	cycles = 0;
	do {
		do {
			gem_execbuf(fd, &execbuf);
			gem_quiescent_gpu(fd);
		} while (++cycles & 1023);
	} while ((elapsed = gettime() - start) < timeout);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	igt_info("Completed %ld cycles: %.3f us\n",
		 cycles, elapsed*1e6/cycles);

	gem_close(fd, object.handle);
}

static void
wakeup_ring(int fd, unsigned ring, int timeout, int wlen)
{
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			if (!gem_can_store_dword(fd, ring))
				continue;

			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}
		igt_require(num_engines);
	} else {
		gem_require_ring(fd, ring);
		igt_require(gem_can_store_dword(fd, ring));
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_engines) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object;
		struct drm_i915_gem_execbuffer2 execbuf;
		double end, this, elapsed, now, baseline;
		unsigned long cycles;
		igt_spin_t *spin;

		memset(&object, 0, sizeof(object));
		object.handle = gem_create(fd, 4096);
		gem_write(fd, object.handle, 0, &bbe, sizeof(bbe));

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(&object);
		execbuf.buffer_count = 1;
		execbuf.flags = engines[child % num_engines];

		spin = __igt_spin_new(fd,
				      .engine = execbuf.flags,
				      .flags = (IGT_SPIN_POLL_RUN |
						IGT_SPIN_FAST));
		igt_assert(igt_spin_has_poll(spin));

		gem_execbuf(fd, &execbuf);

		igt_spin_end(spin);
		gem_sync(fd, object.handle);

		for (int warmup = 0; warmup <= 1; warmup++) {
			end = gettime() + timeout/10.;
			elapsed = 0;
			cycles = 0;
			do {
				igt_spin_reset(spin);

				gem_execbuf(fd, &spin->execbuf);
				igt_spin_busywait_until_started(spin);

				this = gettime();
				igt_spin_end(spin);
				gem_sync(fd, spin->handle);
				now = gettime();

				elapsed += now - this;
				cycles++;
			} while (now < end);
			baseline = elapsed / cycles;
		}
		igt_info("%s%saseline %ld cycles: %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " b" : "B",
			 cycles, elapsed*1e6/cycles);

		end = gettime() + timeout;
		elapsed = 0;
		cycles = 0;
		do {
			igt_spin_reset(spin);

			gem_execbuf(fd, &spin->execbuf);
			igt_spin_busywait_until_started(spin);

			for (int n = 0; n < wlen; n++)
				gem_execbuf(fd, &execbuf);

			this = gettime();
			igt_spin_end(spin);
			gem_sync(fd, object.handle);
			now = gettime();

			elapsed += now - this;
			cycles++;
		} while (now < end);
		elapsed -= cycles * baseline;

		igt_info("%s%sompleted %ld cycles: %.3f + %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, 1e6*baseline, elapsed*1e6/cycles);

		igt_spin_free(fd, spin);
		gem_close(fd, object.handle);
	}
	igt_waitchildren_timeout(2*timeout, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void active_ring(int fd, unsigned ring, int timeout)
{
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			if (!gem_can_store_dword(fd, ring))
				continue;

			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}
		igt_require(num_engines);
	} else {
		gem_require_ring(fd, ring);
		igt_require(gem_can_store_dword(fd, ring));
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_engines) {
		double start, end, elapsed;
		unsigned long cycles;
		igt_spin_t *spin[2];

		spin[0] = __igt_spin_new(fd,
					 .engine = ring,
					 .flags = IGT_SPIN_FAST);

		spin[1] = __igt_spin_new(fd,
					 .engine = ring,
					 .flags = IGT_SPIN_FAST);

		start = gettime();
		end = start + timeout;
		cycles = 0;
		do {
			for (int loop = 0; loop < 1024; loop++) {
				igt_spin_t *s = spin[loop & 1];

				igt_spin_end(s);
				gem_sync(fd, s->handle);

				igt_spin_reset(s);

				gem_execbuf(fd, &s->execbuf);
			}
			cycles += 1024;
		} while ((elapsed = gettime()) < end);
		igt_spin_free(fd, spin[1]);
		igt_spin_free(fd, spin[0]);

		igt_info("%s%sompleted %ld cycles: %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, (elapsed - start)*1e6/cycles);
	}
	igt_waitchildren_timeout(2*timeout, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void
active_wakeup_ring(int fd, unsigned ring, int timeout, int wlen)
{
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			if (!gem_can_store_dword(fd, ring))
				continue;

			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}
		igt_require(num_engines);
	} else {
		gem_require_ring(fd, ring);
		igt_require(gem_can_store_dword(fd, ring));
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_engines) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object;
		struct drm_i915_gem_execbuffer2 execbuf;
		double end, this, elapsed, now, baseline;
		unsigned long cycles;
		igt_spin_t *spin[2];

		memset(&object, 0, sizeof(object));
		object.handle = gem_create(fd, 4096);
		gem_write(fd, object.handle, 0, &bbe, sizeof(bbe));

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(&object);
		execbuf.buffer_count = 1;
		execbuf.flags = engines[child % num_engines];

		spin[0] = __igt_spin_new(fd,
					 .engine = execbuf.flags,
					 .flags = (IGT_SPIN_POLL_RUN |
						   IGT_SPIN_FAST));
		igt_assert(igt_spin_has_poll(spin[0]));

		spin[1] = __igt_spin_new(fd,
					 .engine = execbuf.flags,
					 .flags = (IGT_SPIN_POLL_RUN |
						   IGT_SPIN_FAST));

		gem_execbuf(fd, &execbuf);

		igt_spin_end(spin[1]);
		igt_spin_end(spin[0]);
		gem_sync(fd, object.handle);

		for (int warmup = 0; warmup <= 1; warmup++) {
			igt_spin_reset(spin[0]);

			gem_execbuf(fd, &spin[0]->execbuf);

			end = gettime() + timeout/10.;
			elapsed = 0;
			cycles = 0;
			do {
				igt_spin_busywait_until_started(spin[0]);

				igt_spin_reset(spin[1]);

				gem_execbuf(fd, &spin[1]->execbuf);

				this = gettime();
				igt_spin_end(spin[0]);
				gem_sync(fd, spin[0]->handle);
				now = gettime();

				elapsed += now - this;
				cycles++;
				igt_swap(spin[0], spin[1]);
			} while (now < end);
			igt_spin_end(spin[0]);
			baseline = elapsed / cycles;
		}
		igt_info("%s%saseline %ld cycles: %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " b" : "B",
			 cycles, elapsed*1e6/cycles);

		igt_spin_reset(spin[0]);

		gem_execbuf(fd, &spin[0]->execbuf);

		end = gettime() + timeout;
		elapsed = 0;
		cycles = 0;
		do {
			igt_spin_busywait_until_started(spin[0]);

			for (int n = 0; n < wlen; n++)
				gem_execbuf(fd, &execbuf);

			igt_spin_reset(spin[1]);

			gem_execbuf(fd, &spin[1]->execbuf);

			this = gettime();
			igt_spin_end(spin[0]);
			gem_sync(fd, object.handle);
			now = gettime();

			elapsed += now - this;
			cycles++;
			igt_swap(spin[0], spin[1]);
		} while (now < end);
		igt_spin_end(spin[0]);
		elapsed -= cycles * baseline;

		igt_info("%s%sompleted %ld cycles: %.3f + %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, 1e6*baseline, elapsed*1e6/cycles);

		igt_spin_free(fd, spin[1]);
		igt_spin_free(fd, spin[0]);
		gem_close(fd, object.handle);
	}
	igt_waitchildren_timeout(2*timeout, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void
store_ring(int fd, unsigned ring, int num_children, int timeout)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			if (!gem_can_store_dword(fd, ring))
				continue;

			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}

		num_children *= num_engines;
	} else {
		gem_require_ring(fd, ring);
		igt_require(gem_can_store_dword(fd, ring));
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_children) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object[2];
		struct drm_i915_gem_relocation_entry reloc[1024];
		struct drm_i915_gem_execbuffer2 execbuf;
		double start, elapsed;
		unsigned long cycles;
		uint32_t *batch, *b;

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(object);
		execbuf.flags = engines[child % num_engines];
		execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
		execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
		if (gen < 6)
			execbuf.flags |= I915_EXEC_SECURE;

		memset(object, 0, sizeof(object));
		object[0].handle = gem_create(fd, 4096);
		gem_write(fd, object[0].handle, 0, &bbe, sizeof(bbe));
		execbuf.buffer_count = 1;
		gem_execbuf(fd, &execbuf);

		object[0].flags |= EXEC_OBJECT_WRITE;
		object[1].handle = gem_create(fd, 20*1024);

		object[1].relocs_ptr = to_user_pointer(reloc);
		object[1].relocation_count = 1024;

		batch = gem_mmap__cpu(fd, object[1].handle, 0, 20*1024,
				PROT_WRITE | PROT_READ);
		gem_set_domain(fd, object[1].handle,
				I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

		memset(reloc, 0, sizeof(reloc));
		b = batch;
		for (int i = 0; i < 1024; i++) {
			uint64_t offset;

			reloc[i].presumed_offset = object[0].offset;
			reloc[i].offset = (b - batch + 1) * sizeof(*batch);
			reloc[i].delta = i * sizeof(uint32_t);
			reloc[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
			reloc[i].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

			offset = object[0].offset + reloc[i].delta;
			*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
			if (gen >= 8) {
				*b++ = offset;
				*b++ = offset >> 32;
			} else if (gen >= 4) {
				*b++ = 0;
				*b++ = offset;
				reloc[i].offset += sizeof(*batch);
			} else {
				b[-1] -= 1;
				*b++ = offset;
			}
			*b++ = i;
		}
		*b++ = MI_BATCH_BUFFER_END;
		igt_assert((b - batch)*sizeof(uint32_t) < 20*1024);
		munmap(batch, 20*1024);
		execbuf.buffer_count = 2;
		gem_execbuf(fd, &execbuf);
		gem_sync(fd, object[1].handle);

		start = gettime();
		cycles = 0;
		do {
			do {
				gem_execbuf(fd, &execbuf);
				gem_sync(fd, object[1].handle);
			} while (++cycles & 1023);
		} while ((elapsed = gettime() - start) < timeout);
		igt_info("%s%sompleted %ld cycles: %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, elapsed*1e6/cycles);

		gem_close(fd, object[1].handle);
		gem_close(fd, object[0].handle);
	}
	igt_waitchildren_timeout(timeout+10, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void
switch_ring(int fd, unsigned ring, int num_children, int timeout)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;

	gem_require_contexts(fd);

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			if (!gem_can_store_dword(fd, ring))
				continue;

			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}

		num_children *= num_engines;
	} else {
		gem_require_ring(fd, ring);
		igt_require(gem_can_store_dword(fd, ring));
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_children) {
		struct context {
			struct drm_i915_gem_exec_object2 object[2];
			struct drm_i915_gem_relocation_entry reloc[1024];
			struct drm_i915_gem_execbuffer2 execbuf;
		} contexts[2];
		double elapsed, baseline;
		unsigned long cycles;

		for (int i = 0; i < ARRAY_SIZE(contexts); i++) {
			const uint32_t bbe = MI_BATCH_BUFFER_END;
			const uint32_t sz = 32 << 10;
			struct context *c = &contexts[i];
			uint32_t *batch, *b;

			memset(&c->execbuf, 0, sizeof(c->execbuf));
			c->execbuf.buffers_ptr = to_user_pointer(c->object);
			c->execbuf.flags = engines[child % num_engines];
			c->execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
			c->execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
			if (gen < 6)
				c->execbuf.flags |= I915_EXEC_SECURE;
			c->execbuf.rsvd1 = gem_context_create(fd);

			memset(c->object, 0, sizeof(c->object));
			c->object[0].handle = gem_create(fd, 4096);
			gem_write(fd, c->object[0].handle, 0, &bbe, sizeof(bbe));
			c->execbuf.buffer_count = 1;
			gem_execbuf(fd, &c->execbuf);

			c->object[0].flags |= EXEC_OBJECT_WRITE;
			c->object[1].handle = gem_create(fd, sz);

			c->object[1].relocs_ptr = to_user_pointer(c->reloc);
			c->object[1].relocation_count = 1024 * i;

			batch = gem_mmap__cpu(fd, c->object[1].handle, 0, sz,
					PROT_WRITE | PROT_READ);
			gem_set_domain(fd, c->object[1].handle,
					I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

			memset(c->reloc, 0, sizeof(c->reloc));
			b = batch;
			for (int r = 0; r < c->object[1].relocation_count; r++) {
				uint64_t offset;

				c->reloc[r].presumed_offset = c->object[0].offset;
				c->reloc[r].offset = (b - batch + 1) * sizeof(*batch);
				c->reloc[r].delta = r * sizeof(uint32_t);
				c->reloc[r].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
				c->reloc[r].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

				offset = c->object[0].offset + c->reloc[r].delta;
				*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
				if (gen >= 8) {
					*b++ = offset;
					*b++ = offset >> 32;
				} else if (gen >= 4) {
					*b++ = 0;
					*b++ = offset;
					c->reloc[r].offset += sizeof(*batch);
				} else {
					b[-1] -= 1;
					*b++ = offset;
				}
				*b++ = r;
				*b++ = 0x5 << 23;
			}
			*b++ = MI_BATCH_BUFFER_END;
			igt_assert((b - batch)*sizeof(uint32_t) < sz);
			munmap(batch, sz);
			c->execbuf.buffer_count = 2;
			gem_execbuf(fd, &c->execbuf);
			gem_sync(fd, c->object[1].handle);
		}

		cycles = 0;
		baseline = 0;
		igt_until_timeout(timeout) {
			do {
				double this;

				gem_execbuf(fd, &contexts[1].execbuf);
				gem_execbuf(fd, &contexts[0].execbuf);

				this = gettime();
				gem_sync(fd, contexts[1].object[1].handle);
				gem_sync(fd, contexts[0].object[1].handle);
				baseline += gettime() - this;
			} while (++cycles & 1023);
		}
		baseline /= cycles;

		cycles = 0;
		elapsed = 0;
		igt_until_timeout(timeout) {
			do {
				double this;

				gem_execbuf(fd, &contexts[1].execbuf);
				gem_execbuf(fd, &contexts[0].execbuf);

				this = gettime();
				gem_sync(fd, contexts[0].object[1].handle);
				elapsed += gettime() - this;

				gem_sync(fd, contexts[1].object[1].handle);
			} while (++cycles & 1023);
		}
		elapsed /= cycles;

		igt_info("%s%sompleted %ld cycles: %.3f us, baseline %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, elapsed*1e6, baseline*1e6);

		for (int i = 0; i < ARRAY_SIZE(contexts); i++) {
			gem_close(fd, contexts[i].object[1].handle);
			gem_close(fd, contexts[i].object[0].handle);
			gem_context_destroy(fd, contexts[i].execbuf.rsvd1);
		}
	}
	igt_waitchildren_timeout(timeout+10, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void xchg(void *array, unsigned i, unsigned j)
{
	uint32_t *u32 = array;
	uint32_t tmp = u32[i];
	u32[i] = u32[j];
	u32[j] = tmp;
}

struct waiter {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	int ready;
	volatile int *done;

	int fd;
	struct drm_i915_gem_exec_object2 object;
	uint32_t handles[64];
};

static void *waiter(void *arg)
{
	struct waiter *w = arg;

	do {
		pthread_mutex_lock(&w->mutex);
		w->ready = 0;
		pthread_cond_signal(&w->cond);
		while (!w->ready)
			pthread_cond_wait(&w->cond, &w->mutex);
		pthread_mutex_unlock(&w->mutex);
		if (*w->done < 0)
			return NULL;

		gem_sync(w->fd, w->object.handle);
		for (int n = 0;  n < ARRAY_SIZE(w->handles); n++)
			gem_sync(w->fd, w->handles[n]);
	} while (1);
}

static void
__store_many(int fd, unsigned ring, int timeout, unsigned long *cycles)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 object[2];
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_relocation_entry reloc[1024];
	struct waiter threads[64];
	int order[64];
	uint32_t *batch, *b;
	int done;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(object);
	execbuf.flags = ring;
	execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
	execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
	if (gen < 6)
		execbuf.flags |= I915_EXEC_SECURE;

	memset(object, 0, sizeof(object));
	object[0].handle = gem_create(fd, 4096);
	gem_write(fd, object[0].handle, 0, &bbe, sizeof(bbe));
	execbuf.buffer_count = 1;
	gem_execbuf(fd, &execbuf);
	object[0].flags |= EXEC_OBJECT_WRITE;

	object[1].relocs_ptr = to_user_pointer(reloc);
	object[1].relocation_count = 1024;
	execbuf.buffer_count = 2;

	memset(reloc, 0, sizeof(reloc));
	b = batch = malloc(20*1024);
	for (int i = 0; i < 1024; i++) {
		uint64_t offset;

		reloc[i].presumed_offset = object[0].offset;
		reloc[i].offset = (b - batch + 1) * sizeof(*batch);
		reloc[i].delta = i * sizeof(uint32_t);
		reloc[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
		reloc[i].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

		offset = object[0].offset + reloc[i].delta;
		*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
		if (gen >= 8) {
			*b++ = offset;
			*b++ = offset >> 32;
		} else if (gen >= 4) {
			*b++ = 0;
			*b++ = offset;
			reloc[i].offset += sizeof(*batch);
		} else {
			b[-1] -= 1;
			*b++ = offset;
		}
		*b++ = i;
	}
	*b++ = MI_BATCH_BUFFER_END;
	igt_assert((b - batch)*sizeof(uint32_t) < 20*1024);

	done = 0;
	for (int i = 0; i < ARRAY_SIZE(threads); i++) {
		threads[i].fd = fd;
		threads[i].object = object[1];
		threads[i].object.handle = gem_create(fd, 20*1024);
		gem_write(fd, threads[i].object.handle, 0, batch, 20*1024);

		pthread_cond_init(&threads[i].cond, NULL);
		pthread_mutex_init(&threads[i].mutex, NULL);
		threads[i].done = &done;
		threads[i].ready = 0;

		pthread_create(&threads[i].thread, NULL, waiter, &threads[i]);
		order[i] = i;
	}
	free(batch);

	for (int i = 0; i < ARRAY_SIZE(threads); i++) {
		for (int j = 0; j < ARRAY_SIZE(threads); j++)
			threads[i].handles[j] = threads[j].object.handle;
	}

	igt_until_timeout(timeout) {
		for (int i = 0; i < ARRAY_SIZE(threads); i++) {
			pthread_mutex_lock(&threads[i].mutex);
			while (threads[i].ready)
				pthread_cond_wait(&threads[i].cond,
						  &threads[i].mutex);
			pthread_mutex_unlock(&threads[i].mutex);
			igt_permute_array(threads[i].handles,
					  ARRAY_SIZE(threads[i].handles),
					  xchg);
		}

		igt_permute_array(order, ARRAY_SIZE(threads), xchg);
		for (int i = 0; i < ARRAY_SIZE(threads); i++) {
			object[1] = threads[i].object;
			gem_execbuf(fd, &execbuf);
			threads[i].object = object[1];
		}
		++*cycles;

		for (int i = 0; i < ARRAY_SIZE(threads); i++) {
			struct waiter *w = &threads[order[i]];

			w->ready = 1;
			pthread_cond_signal(&w->cond);
		}
	}

	for (int i = 0; i < ARRAY_SIZE(threads); i++) {
		pthread_mutex_lock(&threads[i].mutex);
		while (threads[i].ready)
			pthread_cond_wait(&threads[i].cond, &threads[i].mutex);
		pthread_mutex_unlock(&threads[i].mutex);
	}
	done = -1;
	for (int i = 0; i < ARRAY_SIZE(threads); i++) {
		threads[i].ready = 1;
		pthread_cond_signal(&threads[i].cond);
		pthread_join(threads[i].thread, NULL);
		gem_close(fd, threads[i].object.handle);
	}

	gem_close(fd, object[0].handle);
}

static void
store_many(int fd, unsigned ring, int timeout)
{
	unsigned long *shared;
	const char *names[16];
	int n = 0;

	shared = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(shared != MAP_FAILED);

	intel_detect_and_clear_missed_interrupts(fd);

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			if (!gem_can_store_dword(fd, ring))
				continue;

			igt_fork(child, 1)
				__store_many(fd,
					     ring,
					     timeout,
					     &shared[n]);

			names[n++] = e__->name;
		}
		igt_waitchildren();
	} else {
		gem_require_ring(fd, ring);
		igt_require(gem_can_store_dword(fd, ring));
		__store_many(fd, ring, timeout, &shared[n]);
		names[n++] = NULL;
	}

	for (int i = 0; i < n; i++) {
		igt_info("%s%sompleted %ld cycles\n",
			 names[i] ?: "", names[i] ? " c" : "C", shared[i]);
	}
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
	munmap(shared, 4096);
}

static void
sync_all(int fd, int num_children, int timeout)
{
	unsigned engines[16], engine;
	int num_engines = 0;

	for_each_physical_engine(fd, engine) {
		engines[num_engines++] = engine;
		if (num_engines == ARRAY_SIZE(engines))
			break;
	}
	igt_require(num_engines);

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_children) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object;
		struct drm_i915_gem_execbuffer2 execbuf;
		double start, elapsed;
		unsigned long cycles;

		memset(&object, 0, sizeof(object));
		object.handle = gem_create(fd, 4096);
		gem_write(fd, object.handle, 0, &bbe, sizeof(bbe));

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(&object);
		execbuf.buffer_count = 1;
		gem_execbuf(fd, &execbuf);
		gem_sync(fd, object.handle);

		start = gettime();
		cycles = 0;
		do {
			do {
				for (int n = 0; n < num_engines; n++) {
					execbuf.flags = engines[n];
					gem_execbuf(fd, &execbuf);
				}
				gem_sync(fd, object.handle);
			} while (++cycles & 1023);
		} while ((elapsed = gettime() - start) < timeout);
		igt_info("Completed %ld cycles: %.3f us\n",
			 cycles, elapsed*1e6/cycles);

		gem_close(fd, object.handle);
	}
	igt_waitchildren_timeout(timeout+10, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void
store_all(int fd, int num_children, int timeout)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));
	unsigned engines[16];
	int num_engines = 0;
	unsigned int ring;

	for_each_physical_engine(fd, ring) {
		if (!gem_can_store_dword(fd, ring))
			continue;

		engines[num_engines++] = ring;
		if (num_engines == ARRAY_SIZE(engines))
			break;
	}
	igt_require(num_engines);

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_children) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object[2];
		struct drm_i915_gem_relocation_entry reloc[1024];
		struct drm_i915_gem_execbuffer2 execbuf;
		double start, elapsed;
		unsigned long cycles;
		uint32_t *batch, *b;

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(object);
		execbuf.flags |= LOCAL_I915_EXEC_NO_RELOC;
		execbuf.flags |= LOCAL_I915_EXEC_HANDLE_LUT;
		if (gen < 6)
			execbuf.flags |= I915_EXEC_SECURE;

		memset(object, 0, sizeof(object));
		object[0].handle = gem_create(fd, 4096);
		gem_write(fd, object[0].handle, 0, &bbe, sizeof(bbe));
		execbuf.buffer_count = 1;
		gem_execbuf(fd, &execbuf);

		object[0].flags |= EXEC_OBJECT_WRITE;
		object[1].handle = gem_create(fd, 1024*16 + 4096);

		object[1].relocs_ptr = to_user_pointer(reloc);
		object[1].relocation_count = 1024;

		batch = gem_mmap__cpu(fd, object[1].handle, 0, 16*1024 + 4096,
				PROT_WRITE | PROT_READ);
		gem_set_domain(fd, object[1].handle,
				I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);

		memset(reloc, 0, sizeof(reloc));
		b = batch;
		for (int i = 0; i < 1024; i++) {
			uint64_t offset;

			reloc[i].presumed_offset = object[0].offset;
			reloc[i].offset = (b - batch + 1) * sizeof(*batch);
			reloc[i].delta = i * sizeof(uint32_t);
			reloc[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
			reloc[i].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

			offset = object[0].offset + reloc[i].delta;
			*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
			if (gen >= 8) {
				*b++ = offset;
				*b++ = offset >> 32;
			} else if (gen >= 4) {
				*b++ = 0;
				*b++ = offset;
				reloc[i].offset += sizeof(*batch);
			} else {
				b[-1] -= 1;
				*b++ = offset;
			}
			*b++ = i;
		}
		*b++ = MI_BATCH_BUFFER_END;
		igt_assert((b - batch)*sizeof(uint32_t) < 20*1024);
		munmap(batch, 16*1024+4096);
		execbuf.buffer_count = 2;
		gem_execbuf(fd, &execbuf);
		gem_sync(fd, object[1].handle);

		start = gettime();
		cycles = 0;
		do {
			do {
				igt_permute_array(engines, num_engines, xchg);
				for (int n = 0; n < num_engines; n++) {
					execbuf.flags &= ~ENGINE_MASK;
					execbuf.flags |= engines[n];
					gem_execbuf(fd, &execbuf);
				}
				gem_sync(fd, object[1].handle);
			} while (++cycles & 1023);
		} while ((elapsed = gettime() - start) < timeout);
		igt_info("Completed %ld cycles: %.3f us\n",
			 cycles, elapsed*1e6/cycles);

		gem_close(fd, object[1].handle);
		gem_close(fd, object[0].handle);
	}
	igt_waitchildren_timeout(timeout+10, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static void
preempt(int fd, unsigned ring, int num_children, int timeout)
{
	unsigned engines[16];
	const char *names[16];
	int num_engines = 0;
	uint32_t ctx[2];

	if (ring == ALL_ENGINES) {
		for_each_physical_engine(fd, ring) {
			names[num_engines] = e__->name;
			engines[num_engines++] = ring;
			if (num_engines == ARRAY_SIZE(engines))
				break;
		}

		num_children *= num_engines;
	} else {
		gem_require_ring(fd, ring);
		names[num_engines] = NULL;
		engines[num_engines++] = ring;
	}

	ctx[0] = gem_context_create(fd);
	gem_context_set_priority(fd, ctx[0], MIN_PRIO);

	ctx[1] = gem_context_create(fd);
	gem_context_set_priority(fd, ctx[1], MAX_PRIO);

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, num_children) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 object;
		struct drm_i915_gem_execbuffer2 execbuf;
		double start, elapsed;
		unsigned long cycles;

		memset(&object, 0, sizeof(object));
		object.handle = gem_create(fd, 4096);
		gem_write(fd, object.handle, 0, &bbe, sizeof(bbe));

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(&object);
		execbuf.buffer_count = 1;
		execbuf.flags = engines[child % num_engines];
		execbuf.rsvd1 = ctx[1];
		gem_execbuf(fd, &execbuf);
		gem_sync(fd, object.handle);

		start = gettime();
		cycles = 0;
		do {
			igt_spin_t *spin =
				__igt_spin_new(fd,
					       .ctx = ctx[0],
					       .engine = execbuf.flags);

			do {
				gem_execbuf(fd, &execbuf);
				gem_sync(fd, object.handle);
			} while (++cycles & 1023);

			igt_spin_free(fd, spin);
		} while ((elapsed = gettime() - start) < timeout);
		igt_info("%s%sompleted %ld cycles: %.3f us\n",
			 names[child % num_engines] ?: "",
			 names[child % num_engines] ? " c" : "C",
			 cycles, elapsed*1e6/cycles);

		gem_close(fd, object.handle);
	}
	igt_waitchildren_timeout(timeout+10, NULL);
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);

	gem_context_destroy(fd, ctx[1]);
	gem_context_destroy(fd, ctx[0]);
}

igt_main
{
	const struct intel_execution_engine *e;
	const int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	int fd = -1;

	igt_skip_on_simulation();

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
		gem_submission_print_method(fd);
		gem_scheduler_print_capability(fd);

		igt_fork_hang_detector(fd);
	}

	for (e = intel_execution_engines; e->name; e++) {
		igt_subtest_f("%s", e->name)
			sync_ring(fd, e->exec_id | e->flags, 1, 150);
		igt_subtest_f("idle-%s", e->name)
			idle_ring(fd, e->exec_id | e->flags, 150);
		igt_subtest_f("active-%s", e->name)
			active_ring(fd, e->exec_id | e->flags, 150);
		igt_subtest_f("wakeup-%s", e->name)
			wakeup_ring(fd, e->exec_id | e->flags, 150, 1);
		igt_subtest_f("active-wakeup-%s", e->name)
			active_wakeup_ring(fd, e->exec_id | e->flags, 150, 1);
		igt_subtest_f("double-wakeup-%s", e->name)
			wakeup_ring(fd, e->exec_id | e->flags, 150, 2);
		igt_subtest_f("store-%s", e->name)
			store_ring(fd, e->exec_id | e->flags, 1, 150);
		igt_subtest_f("switch-%s", e->name)
			switch_ring(fd, e->exec_id | e->flags, 1, 150);
		igt_subtest_f("forked-switch-%s", e->name)
			switch_ring(fd, e->exec_id | e->flags, ncpus, 150);
		igt_subtest_f("many-%s", e->name)
			store_many(fd, e->exec_id | e->flags, 150);
		igt_subtest_f("forked-%s", e->name)
			sync_ring(fd, e->exec_id | e->flags, ncpus, 150);
		igt_subtest_f("forked-store-%s", e->name)
			store_ring(fd, e->exec_id | e->flags, ncpus, 150);
	}

	igt_subtest("basic-each")
		sync_ring(fd, ALL_ENGINES, 1, 5);
	igt_subtest("basic-store-each")
		store_ring(fd, ALL_ENGINES, 1, 5);
	igt_subtest("basic-many-each")
		store_many(fd, ALL_ENGINES, 5);
	igt_subtest("switch-each")
		switch_ring(fd, ALL_ENGINES, 1, 150);
	igt_subtest("forked-switch-each")
		switch_ring(fd, ALL_ENGINES, ncpus, 150);
	igt_subtest("forked-each")
		sync_ring(fd, ALL_ENGINES, ncpus, 150);
	igt_subtest("forked-store-each")
		store_ring(fd, ALL_ENGINES, ncpus, 150);
	igt_subtest("active-each")
		active_ring(fd, ALL_ENGINES, 150);
	igt_subtest("wakeup-each")
		wakeup_ring(fd, ALL_ENGINES, 150, 1);
	igt_subtest("active-wakeup-each")
		active_wakeup_ring(fd, ALL_ENGINES, 150, 1);
	igt_subtest("double-wakeup-each")
		wakeup_ring(fd, ALL_ENGINES, 150, 2);

	igt_subtest("basic-all")
		sync_all(fd, 1, 5);
	igt_subtest("basic-store-all")
		store_all(fd, 1, 5);

	igt_subtest("all")
		sync_all(fd, 1, 150);
	igt_subtest("store-all")
		store_all(fd, 1, 150);
	igt_subtest("forked-all")
		sync_all(fd, ncpus, 150);
	igt_subtest("forked-store-all")
		store_all(fd, ncpus, 150);

	igt_subtest_group {
		igt_fixture {
			gem_require_contexts(fd);
			igt_require(gem_scheduler_has_ctx_priority(fd));
			igt_require(gem_scheduler_has_preemption(fd));
		}

		igt_subtest("preempt-all")
			preempt(fd, ALL_ENGINES, 1, 20);

		for (e = intel_execution_engines; e->name; e++) {
			igt_subtest_f("preempt-%s", e->name)
				preempt(fd, e->exec_id | e->flags, ncpus, 150);
		}
	}

	igt_fixture {
		igt_stop_hang_detector();
		close(fd);
	}
}
