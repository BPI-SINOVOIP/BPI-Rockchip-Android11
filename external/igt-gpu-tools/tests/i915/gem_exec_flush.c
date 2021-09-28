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

#include "igt.h"
#include "igt_x86.h"

IGT_TEST_DESCRIPTION("Basic check of flushing after batches");

#define UNCACHED 0
#define COHERENT 1
#define WC 2
#define WRITE 4
#define KERNEL 8
#define SET_DOMAIN 16
#define BEFORE 32
#define INTERRUPTIBLE 64
#define CMDPARSER 128
#define BASIC 256
#define MOVNT 512

#if defined(__x86_64__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC target("sse4.1")
#include <smmintrin.h>
__attribute__((noinline))
static uint32_t movnt(uint32_t *map, int i)
{
	__m128i tmp;

	tmp = _mm_stream_load_si128((__m128i *)map + i/4);
	switch (i%4) { /* gcc! */
	default:
	case 0: return _mm_extract_epi32(tmp, 0);
	case 1: return _mm_extract_epi32(tmp, 1);
	case 2: return _mm_extract_epi32(tmp, 2);
	case 3: return _mm_extract_epi32(tmp, 3);
	}
}
static inline unsigned x86_64_features(void)
{
	return igt_x86_features();
}
#pragma GCC pop_options
#else
static inline unsigned x86_64_features(void)
{
	return 0;
}
static uint32_t movnt(uint32_t *map, int i)
{
	igt_assert(!"reached");
}
#endif

static void run(int fd, unsigned ring, int nchild, int timeout,
		unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));

	/* The crux of this testing is whether writes by the GPU are coherent
	 * from the CPU.
	 *
	 * For example, using plain clflush (the simplest and most visible
	 * in terms of function calls / syscalls) we have two tests which
	 * perform:
	 *
	 * USER (0):
	 *	execbuf(map[i] = i);
	 *	sync();
	 *	clflush(&map[i]);
	 *	assert(map[i] == i);
	 *
	 *	execbuf(map[i] = i ^ ~0);
	 *	sync();
	 *	clflush(&map[i]);
	 *	assert(map[i] == i ^ ~0);
	 *
	 * BEFORE:
	 *	clflush(&map[i]);
	 *	execbuf(map[i] = i);
	 *	sync();
	 *	assert(map[i] == i);
	 *
	 *	clflush(&map[i]);
	 *	execbuf(map[i] = i ^ ~0);
	 *	sync();
	 *	assert(map[i] == i ^ ~0);
	 *
	 * The assertion here is that the cacheline invalidations are precise
	 * and we have no speculative prefetch that can see the future map[i]
	 * access and bring it ahead of the execution, or accidental cache
	 * pollution by the kernel.
	 */

	igt_fork(child, nchild) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 obj[3];
		struct drm_i915_gem_relocation_entry reloc0[1024];
		struct drm_i915_gem_relocation_entry reloc1[1024];
		struct drm_i915_gem_execbuffer2 execbuf;
		unsigned long cycles = 0;
		bool snoop = false;
		uint32_t *ptr;
		uint32_t *map;
		int i;

		memset(obj, 0, sizeof(obj));
		obj[0].handle = gem_create(fd, 4096);
		obj[0].flags |= EXEC_OBJECT_WRITE;

		if (flags & WC) {
			igt_assert(flags & COHERENT);
			map = gem_mmap__wc(fd, obj[0].handle, 0, 4096, PROT_WRITE);
			gem_set_domain(fd, obj[0].handle,
				       I915_GEM_DOMAIN_WC,
				       I915_GEM_DOMAIN_WC);
		} else {
			snoop = flags & COHERENT;
			gem_set_caching(fd, obj[0].handle, snoop);
			map = gem_mmap__cpu(fd, obj[0].handle, 0, 4096, PROT_WRITE);
			gem_set_domain(fd, obj[0].handle,
				       I915_GEM_DOMAIN_CPU,
				       I915_GEM_DOMAIN_CPU);
		}

		for (i = 0; i < 1024; i++)
			map[i] = 0xabcdabcd;

		gem_set_domain(fd, obj[0].handle,
			       I915_GEM_DOMAIN_WC,
			       I915_GEM_DOMAIN_WC);

		/* Prepara a mappable binding to prevent pread mighrating */
		if (!snoop) {
			ptr = gem_mmap__gtt(fd, obj[0].handle, 4096, PROT_READ);
			igt_assert_eq_u32(ptr[0], 0xabcdabcd);
			munmap(ptr, 4096);
		}

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(obj);
		execbuf.buffer_count = 3;
		execbuf.flags = ring | (1 << 11) | (1<<12);
		if (gen < 6)
			execbuf.flags |= I915_EXEC_SECURE;

		obj[1].handle = gem_create(fd, 1024*64);
		obj[2].handle = gem_create(fd, 1024*64);
		gem_write(fd, obj[2].handle, 0, &bbe, sizeof(bbe));
		igt_require(__gem_execbuf(fd, &execbuf) == 0);

		obj[1].relocation_count = 1;
		obj[2].relocation_count = 1;

		ptr = gem_mmap__wc(fd, obj[1].handle, 0, 64*1024,
				   PROT_WRITE | PROT_READ);
		gem_set_domain(fd, obj[1].handle,
			       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

		memset(reloc0, 0, sizeof(reloc0));
		for (i = 0; i < 1024; i++) {
			uint64_t offset;
			uint32_t *b = &ptr[16 * i];

			reloc0[i].presumed_offset = obj[0].offset;
			reloc0[i].offset = (b - ptr + 1) * sizeof(*ptr);
			reloc0[i].delta = i * sizeof(uint32_t);
			reloc0[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
			reloc0[i].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

			offset = obj[0].offset + reloc0[i].delta;
			*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
			if (gen >= 8) {
				*b++ = offset;
				*b++ = offset >> 32;
			} else if (gen >= 4) {
				*b++ = 0;
				*b++ = offset;
				reloc0[i].offset += sizeof(*ptr);
			} else {
				b[-1] -= 1;
				*b++ = offset;
			}
			*b++ = i;
			*b++ = MI_BATCH_BUFFER_END;
		}
		munmap(ptr, 64*1024);

		ptr = gem_mmap__wc(fd, obj[2].handle, 0, 64*1024,
				   PROT_WRITE | PROT_READ);
		gem_set_domain(fd, obj[2].handle,
			       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);

		memset(reloc1, 0, sizeof(reloc1));
		for (i = 0; i < 1024; i++) {
			uint64_t offset;
			uint32_t *b = &ptr[16 * i];

			reloc1[i].presumed_offset = obj[0].offset;
			reloc1[i].offset = (b - ptr + 1) * sizeof(*ptr);
			reloc1[i].delta = i * sizeof(uint32_t);
			reloc1[i].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
			reloc1[i].write_domain = I915_GEM_DOMAIN_INSTRUCTION;

			offset = obj[0].offset + reloc1[i].delta;
			*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
			if (gen >= 8) {
				*b++ = offset;
				*b++ = offset >> 32;
			} else if (gen >= 4) {
				*b++ = 0;
				*b++ = offset;
				reloc1[i].offset += sizeof(*ptr);
			} else {
				b[-1] -= 1;
				*b++ = offset;
			}
			*b++ = i ^ 0xffffffff;
			*b++ = MI_BATCH_BUFFER_END;
		}
		munmap(ptr, 64*1024);

		igt_until_timeout(timeout) {
			bool xor = false;
			int idx = cycles++ % 1024;

			/* Inspect a different cacheline each iteration */
			i = 16 * (idx % 64) + (idx / 64);
			obj[1].relocs_ptr = to_user_pointer(&reloc0[i]);
			obj[2].relocs_ptr = to_user_pointer(&reloc1[i]);
			igt_assert_eq_u64(reloc0[i].presumed_offset, obj[0].offset);
			igt_assert_eq_u64(reloc1[i].presumed_offset, obj[0].offset);
			execbuf.batch_start_offset =  64*i;

overwrite:
			if ((flags & BEFORE) &&
			    !((flags & COHERENT) || gem_has_llc(fd)))
				igt_clflush_range(&map[i], sizeof(map[i]));

			execbuf.buffer_count = 2 + xor;
			gem_execbuf(fd, &execbuf);

			if (flags & SET_DOMAIN) {
				unsigned domain = flags & WC ? I915_GEM_DOMAIN_WC : I915_GEM_DOMAIN_CPU;
				igt_while_interruptible(flags & INTERRUPTIBLE)
					gem_set_domain(fd, obj[0].handle,
						       domain, (flags & WRITE) ? domain : 0);

				if (xor)
					igt_assert_eq_u32(map[i], i ^ 0xffffffff);
				else
					igt_assert_eq_u32(map[i], i);

				if (flags & WRITE)
					map[i] = 0xdeadbeef;
			} else if (flags & KERNEL) {
				uint32_t val;

				igt_while_interruptible(flags & INTERRUPTIBLE)
					gem_read(fd, obj[0].handle,
						 i*sizeof(uint32_t),
						 &val, sizeof(val));

				if (xor)
					igt_assert_eq_u32(val, i ^ 0xffffffff);
				else
					igt_assert_eq_u32(val, i);

				if (flags & WRITE) {
					val = 0xdeadbeef;
					igt_while_interruptible(flags & INTERRUPTIBLE)
						gem_write(fd, obj[0].handle,
							  i*sizeof(uint32_t),
							  &val, sizeof(val));
				}
			} else if (flags & MOVNT) {
				uint32_t x;

				igt_while_interruptible(flags & INTERRUPTIBLE)
					gem_sync(fd, obj[0].handle);

				x = movnt(map, i);
				if (xor)
					igt_assert_eq_u32(x, i ^ 0xffffffff);
				else
					igt_assert_eq_u32(x, i);

				if (flags & WRITE)
					map[i] = 0xdeadbeef;
			} else {
				igt_while_interruptible(flags & INTERRUPTIBLE)
					gem_sync(fd, obj[0].handle);

				if (!(flags & (BEFORE | COHERENT)) &&
				    !gem_has_llc(fd))
					igt_clflush_range(&map[i], sizeof(map[i]));

				if (xor)
					igt_assert_eq_u32(map[i], i ^ 0xffffffff);
				else
					igt_assert_eq_u32(map[i], i);

				if (flags & WRITE) {
					map[i] = 0xdeadbeef;
					if (!(flags & (COHERENT | BEFORE)))
						igt_clflush_range(&map[i], sizeof(map[i]));
				}
			}

			if (!xor) {
				xor= true;
				goto overwrite;
			}
		}
		igt_info("Child[%d]: %lu cycles\n", child, cycles);

		gem_close(fd, obj[2].handle);
		gem_close(fd, obj[1].handle);

		munmap(map, 4096);
		gem_close(fd, obj[0].handle);
	}
	igt_waitchildren();
}

enum batch_mode {
	BATCH_KERNEL,
	BATCH_USER,
	BATCH_CPU,
	BATCH_GTT,
	BATCH_WC,
};
static void batch(int fd, unsigned ring, int nchild, int timeout,
		  enum batch_mode mode, unsigned flags)
{
	const int gen = intel_gen(intel_get_drm_devid(fd));

	if (flags & CMDPARSER) {
		int cmdparser = -1;
                drm_i915_getparam_t gp;

		gp.param = I915_PARAM_CMD_PARSER_VERSION;
		gp.value = &cmdparser;
		drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
		igt_require(cmdparser > 0);
	}

	intel_detect_and_clear_missed_interrupts(fd);
	igt_fork(child, nchild) {
		const uint32_t bbe = MI_BATCH_BUFFER_END;
		struct drm_i915_gem_exec_object2 obj[2];
		struct drm_i915_gem_relocation_entry reloc;
		struct drm_i915_gem_execbuffer2 execbuf;
		unsigned long cycles = 0;
		uint32_t *ptr;
		uint32_t *map;
		int i;

		memset(obj, 0, sizeof(obj));
		obj[0].handle = gem_create(fd, 4096);
		obj[0].flags |= EXEC_OBJECT_WRITE;

		gem_set_caching(fd, obj[0].handle, !!(flags & COHERENT));
		map = gem_mmap__cpu(fd, obj[0].handle, 0, 4096, PROT_WRITE);

		gem_set_domain(fd, obj[0].handle,
				I915_GEM_DOMAIN_CPU,
				I915_GEM_DOMAIN_CPU);
		for (i = 0; i < 1024; i++)
			map[i] = 0xabcdabcd;

		memset(&execbuf, 0, sizeof(execbuf));
		execbuf.buffers_ptr = to_user_pointer(obj);
		execbuf.buffer_count = 2;
		execbuf.flags = ring | (1 << 11) | (1<<12);
		if (gen < 6)
			execbuf.flags |= I915_EXEC_SECURE;

		obj[1].handle = gem_create(fd, 64<<10);
		gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));
		igt_require(__gem_execbuf(fd, &execbuf) == 0);

		obj[1].relocation_count = 1;
		obj[1].relocs_ptr = to_user_pointer(&reloc);

		switch (mode) {
		case BATCH_CPU:
		case BATCH_USER:
			ptr = gem_mmap__cpu(fd, obj[1].handle, 0, 64<<10,
					    PROT_WRITE);
			break;

		case BATCH_WC:
			ptr = gem_mmap__wc(fd, obj[1].handle, 0, 64<<10,
					    PROT_WRITE);
			break;

		case BATCH_GTT:
			ptr = gem_mmap__gtt(fd, obj[1].handle, 64<<10,
					    PROT_WRITE);
			break;

		case BATCH_KERNEL:
			ptr = mmap(0, 64<<10, PROT_WRITE,
				   MAP_PRIVATE | MAP_ANON, -1, 0);
			break;

		default:
			igt_assert(!"reachable");
			ptr = NULL;
			break;
		}

		memset(&reloc, 0, sizeof(reloc));
		reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
		reloc.write_domain = I915_GEM_DOMAIN_INSTRUCTION;

		igt_until_timeout(timeout) {
			execbuf.batch_start_offset = 0;
			reloc.offset = sizeof(uint32_t);
			if (gen >= 4 && gen < 8)
				reloc.offset += sizeof(uint32_t);

			for (i = 0; i < 1024; i++) {
				uint64_t offset;
				uint32_t *start = &ptr[execbuf.batch_start_offset/sizeof(*start)];
				uint32_t *b = start;

				switch (mode) {
				case BATCH_CPU:
					gem_set_domain(fd, obj[1].handle,
						       I915_GEM_DOMAIN_CPU, I915_GEM_DOMAIN_CPU);
					break;

				case BATCH_WC:
					gem_set_domain(fd, obj[1].handle,
						       I915_GEM_DOMAIN_WC, I915_GEM_DOMAIN_WC);
					break;

				case BATCH_GTT:
					gem_set_domain(fd, obj[1].handle,
						       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
					break;

				case BATCH_USER:
				case BATCH_KERNEL:
					break;
				}

				reloc.presumed_offset = obj[0].offset;
				reloc.delta = i * sizeof(uint32_t);

				offset = reloc.presumed_offset + reloc.delta;
				*b++ = MI_STORE_DWORD_IMM | (gen < 6 ? 1 << 22 : 0);
				if (gen >= 8) {
					*b++ = offset;
					*b++ = offset >> 32;
				} else if (gen >= 4) {
					*b++ = 0;
					*b++ = offset;
				} else {
					b[-1] -= 1;
					*b++ = offset;
				}
				*b++ = cycles + i;
				*b++ = MI_BATCH_BUFFER_END;

				if (flags & CMDPARSER) {
					execbuf.batch_len =
						(b - start) * sizeof(uint32_t);
					if (execbuf.batch_len & 4)
						execbuf.batch_len += 4;
				}

				switch (mode) {
				case BATCH_KERNEL:
					gem_write(fd, obj[1].handle,
						  execbuf.batch_start_offset,
						  start, (b - start) * sizeof(uint32_t));
					break;

				case BATCH_USER:
					if (!gem_has_llc(fd))
						igt_clflush_range(start,
								  (b - start) * sizeof(uint32_t));
					break;

				case BATCH_CPU:
				case BATCH_GTT:
				case BATCH_WC:
					break;
				}
				gem_execbuf(fd, &execbuf);

				execbuf.batch_start_offset += 64;
				reloc.offset += 64;
			}

			if (!(flags & COHERENT)) {
				gem_set_domain(fd, obj[0].handle,
					       I915_GEM_DOMAIN_CPU,
					       I915_GEM_DOMAIN_CPU);
			} else
				gem_sync(fd, obj[0].handle);
			for (i = 0; i < 1024; i++) {
				igt_assert_eq_u32(map[i], cycles + i);
				map[i] = 0xabcdabcd ^ cycles;
			}
			cycles += 1024;

			if (mode == BATCH_USER)
				gem_sync(fd, obj[1].handle);
		}
		igt_info("Child[%d]: %lu cycles\n", child, cycles);

		munmap(ptr, 64<<10);
		gem_close(fd, obj[1].handle);

		munmap(map, 4096);
		gem_close(fd, obj[0].handle);
	}
	igt_waitchildren();
	igt_assert_eq(intel_detect_and_clear_missed_interrupts(fd), 0);
}

static const char *yesno(bool x)
{
	return x ? "yes" : "no";
}

igt_main
{
	const struct intel_execution_engine *e;
	const int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	const struct batch {
		const char *name;
		unsigned mode;
	} batches[] = {
		{ "kernel", BATCH_KERNEL },
		{ "user", BATCH_USER },
		{ "cpu", BATCH_CPU },
		{ "gtt", BATCH_GTT },
		{ "wc", BATCH_WC },
		{ NULL }
	};
	const struct mode {
		const char *name;
		unsigned flags;
	} modes[] = {
		{ "ro", BASIC },
		{ "rw", BASIC | WRITE },
		{ "ro-before", BEFORE },
		{ "rw-before", BEFORE | WRITE },
		{ "pro", BASIC | KERNEL },
		{ "prw", BASIC | KERNEL | WRITE },
		{ "set", BASIC | SET_DOMAIN | WRITE },
		{ NULL }
	};
	unsigned cpu = x86_64_features();
	int fd = -1;

	igt_fixture {
		igt_require(igt_setup_clflush());
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
		gem_require_mmap_wc(fd);
		igt_require(gem_can_store_dword(fd, 0));
		igt_info("Has LLC? %s\n", yesno(gem_has_llc(fd)));

		if (cpu) {
			char str[1024];

			igt_info("CPU features: %s\n",
				 igt_x86_features_to_string(cpu, str));
		}

		igt_fork_hang_detector(fd);
	}

	for (e = intel_execution_engines; e->name; e++) igt_subtest_group {
		unsigned ring = e->exec_id | e->flags;
		unsigned timeout = 5 + 120*!!e->exec_id;

		igt_fixture {
			gem_require_ring(fd, ring);
			igt_require(gem_can_store_dword(fd, ring));
		}

		for (const struct batch *b = batches; b->name; b++) {
			igt_subtest_f("%sbatch-%s-%s-uc",
				      b == batches && e->exec_id == 0 ? "basic-" : "",
				      b->name,
				      e->name)
				batch(fd, ring, ncpus, timeout, b->mode, 0);
			igt_subtest_f("%sbatch-%s-%s-wb",
				      b == batches && e->exec_id == 0 ? "basic-" : "",
				      b->name,
				      e->name)
				batch(fd, ring, ncpus, timeout, b->mode, COHERENT);
			igt_subtest_f("%sbatch-%s-%s-cmd",
				      b == batches && e->exec_id == 0 ? "basic-" : "",
				      b->name,
				      e->name)
				batch(fd, ring, ncpus, timeout, b->mode,
				      COHERENT | CMDPARSER);
		}

		for (const struct mode *m = modes; m->name; m++) {
			igt_subtest_f("%suc-%s-%s",
				      (m->flags & BASIC && e->exec_id == 0) ? "basic-" : "",
				      m->name,
				      e->name)
				run(fd, ring, ncpus, timeout,
				    UNCACHED | m->flags);

			igt_subtest_f("uc-%s-%s-interruptible",
				      m->name,
				      e->name)
				run(fd, ring, ncpus, timeout,
				    UNCACHED | m->flags | INTERRUPTIBLE);

			igt_subtest_f("%swb-%s-%s",
				      e->exec_id == 0 ? "basic-" : "",
				      m->name,
				      e->name)
				run(fd, ring, ncpus, timeout,
				    COHERENT | m->flags);

			igt_subtest_f("wb-%s-%s-interruptible",
				      m->name,
				      e->name)
				run(fd, ring, ncpus, timeout,
				    COHERENT | m->flags | INTERRUPTIBLE);

			igt_subtest_f("wc-%s-%s",
				      m->name,
				      e->name)
				run(fd, ring, ncpus, timeout,
				    COHERENT | WC | m->flags);

			igt_subtest_f("wc-%s-%s-interruptible",
				      m->name,
				      e->name)
				run(fd, ring, ncpus, timeout,
				    COHERENT | WC | m->flags | INTERRUPTIBLE);

			igt_subtest_f("stream-%s-%s",
				      m->name,
				      e->name) {
				igt_require(cpu & SSE4_1);
				run(fd, ring, ncpus, timeout,
				    MOVNT | COHERENT | WC | m->flags);
			}

			igt_subtest_f("stream-%s-%s-interruptible",
				      m->name,
				      e->name) {
				igt_require(cpu & SSE4_1);
				run(fd, ring, ncpus, timeout,
				    MOVNT | COHERENT | WC | m->flags | INTERRUPTIBLE);
			}
		}
	}

	igt_fixture {
		igt_stop_hang_detector();
		close(fd);
	}
}
