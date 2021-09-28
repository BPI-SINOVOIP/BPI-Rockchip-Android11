/*
 * Copyright © 2012 Intel Corporation
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

/*
 * Testcase: Test the relocations through the CPU domain
 *
 * Attempt to stress test performing relocations whilst the batch is in the
 * CPU domain.
 *
 * A freshly allocated buffer starts in the CPU domain, and the pwrite
 * should also be performed whilst in the CPU domain and so we should
 * execute the relocations within the CPU domain. If for any reason one of
 * those steps should land it in the GTT domain, we take the secondary
 * precaution of filling the mappable portion of the GATT.
 *
 * In order to detect whether a relocation fails, we first fill a target
 * buffer with a sequence of invalid commands that would cause the GPU to
 * immediate hang, and then attempt to overwrite them with a legal, if
 * short, batchbuffer using a BLT. Then we come to execute the bo, if the
 * relocation fail and we either copy across all zeros or garbage, then the
 * GPU will hang.
 */

#include "igt.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <drm.h>

#include "intel_bufmgr.h"

#define MI_INSTR(opcode, flags) ((opcode) << 23 | (flags))

IGT_TEST_DESCRIPTION("Test the relocations through the CPU domain.");

static uint32_t *
gen2_emit_store_addr(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = MI_STORE_DWORD_IMM - 1;
	addr->offset += sizeof(*cs);
	cs += 1; /* addr */
	cs += 1; /* value: implicit 0xffffffff */
	return cs;
}

static uint32_t *
gen4_emit_store_addr(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = MI_STORE_DWORD_IMM;
	*cs++ = 0;
	addr->offset += 2 * sizeof(*cs);
	cs += 1; /* addr */
	cs += 1; /* value: implicit 0xffffffff */
	return cs;
}

static uint32_t *
gen8_emit_store_addr(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = (MI_STORE_DWORD_IMM | 1 << 21) + 1;
	addr->offset += sizeof(*cs);
	igt_assert((addr->delta & 7) == 0);
	cs += 2; /* addr */
	cs += 2; /* value: implicit 0xffffffffffffffff */
	return cs;
}

static uint32_t *
gen2_emit_bb_start(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = MI_BATCH_BUFFER_START | 2 << 6;
	addr->offset += sizeof(*cs);
	addr->delta += 1;
	cs += 1; /* addr */
	return cs;
}

static uint32_t *
gen4_emit_bb_start(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = MI_BATCH_BUFFER_START | 2 << 6 | 1 << 8;
	addr->offset += sizeof(*cs);
	cs += 1; /* addr */
	return cs;
}

static uint32_t *
gen6_emit_bb_start(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = MI_BATCH_BUFFER_START | 1 << 8;
	addr->offset += sizeof(*cs);
	cs += 1; /* addr */
	return cs;
}

static uint32_t *
hsw_emit_bb_start(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	*cs++ = MI_BATCH_BUFFER_START | 2 << 6 | 1 << 8 | 1 << 13;
	addr->offset += sizeof(*cs);
	cs += 1; /* addr */
	return cs;
}

static uint32_t *
gen8_emit_bb_start(uint32_t *cs, struct drm_i915_gem_relocation_entry *addr)
{
	if (((uintptr_t)cs & 7) == 0) {
		*cs++ = MI_NOOP; /* align addr for MI_STORE_DWORD_IMM */
		addr->offset += sizeof(*cs);
	}

	*cs++ = MI_BATCH_BUFFER_START + 1;
	addr->offset += sizeof(*cs);
	cs += 2; /* addr */

	return cs;
}

static void *
create_tmpl(int i915, struct drm_i915_gem_relocation_entry *reloc)
{
	const uint32_t devid = intel_get_drm_devid(i915);
	const int gen = intel_gen(devid);
	uint32_t *(*emit_store_addr)(uint32_t *cs,
				   struct drm_i915_gem_relocation_entry *addr);
	uint32_t *(*emit_bb_start)(uint32_t *cs,
				   struct drm_i915_gem_relocation_entry *reloc);
	void *tmpl;

	if (gen >= 8)
		emit_store_addr = gen8_emit_store_addr;
	else if (gen >= 4)
		emit_store_addr = gen4_emit_store_addr;
	else
		emit_store_addr = gen2_emit_store_addr;

	if (gen >= 8)
		emit_bb_start = gen8_emit_bb_start;
	else if (IS_HASWELL(devid))
		emit_bb_start = hsw_emit_bb_start;
	else if (gen >= 6)
		emit_bb_start = gen6_emit_bb_start;
	else if (gen >= 4)
		emit_bb_start = gen4_emit_bb_start;
	else
		emit_bb_start = gen2_emit_bb_start;

	tmpl = malloc(4096);
	igt_assert(tmpl);
	memset(tmpl, 0xff, 4096);

	/* Jump over the booby traps to the end */
	reloc[0].delta = 64;
	emit_bb_start(tmpl, &reloc[0]);

	/* Restore the bad address to catch missing relocs */
	reloc[1].offset = 64;
	reloc[1].delta = reloc[0].offset;
	*emit_store_addr(tmpl + 64, &reloc[1]) = MI_BATCH_BUFFER_END;

	return tmpl;
}

static void run_test(int i915, int count)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_relocation_entry reloc[2];
	struct drm_i915_gem_exec_object2 obj;

	uint32_t *handles;
	uint32_t *tmpl;

	handles = malloc(count * sizeof(uint32_t));
	igt_assert(handles);

	memset(reloc, 0, sizeof(reloc));
	tmpl = create_tmpl(i915, reloc);
	for (int i = 0; i < count; i++) {
		handles[i] = gem_create(i915, 4096);
		gem_write(i915, handles[i], 0, tmpl, 4096);
	}
	free(tmpl);

	memset(&obj, 0, sizeof(obj));
	obj.relocs_ptr = to_user_pointer(reloc);
	obj.relocation_count = ARRAY_SIZE(reloc);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;

	/* fill the entire gart with batches and run them */
	for (int i = 0; i < count; i++) {
		obj.handle = handles[i];

		reloc[0].target_handle = obj.handle;
		reloc[0].presumed_offset = -1;
		reloc[1].target_handle = obj.handle;
		reloc[1].presumed_offset = -1;

		gem_execbuf(i915, &execbuf);
	}

	/* And again in reverse to try and catch the relocation code out */
	for (int i = 0; i < count; i++) {
		obj.handle = handles[count - i - 1];

		reloc[0].target_handle = obj.handle;
		reloc[0].presumed_offset = -1;
		reloc[1].target_handle = obj.handle;
		reloc[1].presumed_offset = -1;

		gem_execbuf(i915, &execbuf);
	}

	/* Third time unlucky? */
	for (int i = 0; i < count; i++) {
		obj.handle = handles[i];

		reloc[0].target_handle = obj.handle;
		reloc[0].presumed_offset = -1;
		reloc[1].target_handle = obj.handle;
		reloc[1].presumed_offset = -1;

		gem_set_domain(i915, obj.handle,
			       I915_GEM_DOMAIN_CPU,
			       I915_GEM_DOMAIN_CPU);

		gem_execbuf(i915, &execbuf);
	}

	for (int i = 0; i < count; i++)
		gem_close(i915, handles[i]);
	free(handles);
}

igt_main
{
	int i915;

	igt_fixture {
		i915 = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(i915);

		/* could use BLT_FILL instead for gen2 */
		igt_require(gem_can_store_dword(i915, 0));

		igt_fork_hang_detector(i915);
	}

	igt_subtest("basic")
		run_test(i915, 1);

	igt_subtest("full") {
		uint64_t aper_size = gem_mappable_aperture_size();
		unsigned long count = aper_size / 4096 + 1;

		intel_require_memory(count, 4096, CHECK_RAM);

		run_test(i915, count);
	}

	igt_subtest("forked") {
		uint64_t aper_size = gem_mappable_aperture_size();
		unsigned long count = aper_size / 4096 + 1;
		int ncpus = sysconf(_SC_NPROCESSORS_ONLN);

		intel_require_memory(count, 4096, CHECK_RAM);

		igt_fork(child, ncpus)
			run_test(i915, count / ncpus + 1);
		igt_waitchildren();
	}

	igt_fixture {
		igt_stop_hang_detector();
	}
}
