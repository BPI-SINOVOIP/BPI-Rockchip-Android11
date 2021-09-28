/*
 * Copyright © 2015 Intel Corporation
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
 * 	Jeff McGee <jeff.mcgee@intel.com>
 */

#include <intel_bufmgr.h>
#include <i915_drm.h>
#include "intel_reg.h"
#include "drmtest.h"
#include "intel_batchbuffer.h"
#include "gen8_media.h"
#include "media_spin.h"
#include "gpu_cmds.h"

static const uint32_t spin_kernel[][4] = {
	{ 0x00600001, 0x20800208, 0x008d0000, 0x00000000 }, /* mov (8)r4.0<1>:ud r0.0<8;8;1>:ud */
	{ 0x00200001, 0x20800208, 0x00450040, 0x00000000 }, /* mov (2)r4.0<1>.ud r2.0<2;2;1>:ud */
	{ 0x00000001, 0x20880608, 0x00000000, 0x00000003 }, /* mov (1)r4.8<1>:ud 0x3 */
	{ 0x00000001, 0x20a00608, 0x00000000, 0x00000000 }, /* mov (1)r5.0<1>:ud 0 */
	{ 0x00000040, 0x20a00208, 0x060000a0, 0x00000001 }, /* add (1)r5.0<1>:ud r5.0<0;1;0>:ud 1 */
	{ 0x01000010, 0x20000200, 0x02000020, 0x000000a0 }, /* cmp.e.f0.0 (1)null<1> r1<0;1;0> r5<0;1;0> */
	{ 0x00110027, 0x00000000, 0x00000000, 0xffffffe0 }, /* ~f0.0 while (1) -32 */
	{ 0x0c800031, 0x20000a00, 0x0e000080, 0x040a8000 }, /* send.dcdp1 (16)null<1> r4.0<0;1;0> 0x040a8000 */
	{ 0x00600001, 0x2e000208, 0x008d0000, 0x00000000 }, /* mov (8)r112<1>:ud r0.0<8;8;1>:ud */
	{ 0x07800031, 0x20000a40, 0x0e000e00, 0x82000010 }, /* send.ts (16)null<1> r112<0;1;0>:d 0x82000010 */
};

/*
 * This sets up the media pipeline,
 *
 * +---------------+ <---- 4096
 * |       ^       |
 * |       |       |
 * |    various    |
 * |      state    |
 * |       |       |
 * |_______|_______| <---- 2048 + ?
 * |       ^       |
 * |       |       |
 * |   batch       |
 * |    commands   |
 * |       |       |
 * |       |       |
 * +---------------+ <---- 0 + ?
 *
 */

#define BATCH_STATE_SPLIT 2048
/* VFE STATE params */
#define THREADS 0
#define MEDIA_URB_ENTRIES 2
#define MEDIA_URB_SIZE 2
#define MEDIA_CURBE_SIZE 2

/* Offsets needed in gen_emit_media_object. In media_spin library this
 * values do not matter.
 */
#define xoffset 0
#define yoffset 0

void
gen8_media_spinfunc(struct intel_batchbuffer *batch,
		    const struct igt_buf *dst, uint32_t spins)
{
	uint32_t curbe_buffer, interface_descriptor;
	uint32_t batch_end;

	intel_batchbuffer_flush_with_context(batch, NULL);

	/* setup states */
	batch->ptr = &batch->buffer[BATCH_STATE_SPLIT];

	curbe_buffer = gen8_spin_curbe_buffer_data(batch, spins);
	interface_descriptor = gen8_fill_interface_descriptor(batch, dst,
					      spin_kernel, sizeof(spin_kernel));
	igt_assert(batch->ptr < &batch->buffer[4095]);

	/* media pipeline */
	batch->ptr = batch->buffer;
	OUT_BATCH(GEN8_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA);
	gen8_emit_state_base_address(batch);

	gen8_emit_vfe_state(batch, THREADS, MEDIA_URB_ENTRIES,
			    MEDIA_URB_SIZE, MEDIA_CURBE_SIZE);

	gen7_emit_curbe_load(batch, curbe_buffer);

	gen7_emit_interface_descriptor_load(batch, interface_descriptor);

	gen_emit_media_object(batch, xoffset, yoffset);

	OUT_BATCH(MI_BATCH_BUFFER_END);

	batch_end = intel_batchbuffer_align(batch, 8);
	igt_assert(batch_end < BATCH_STATE_SPLIT);

	gen7_render_flush(batch, batch_end);
	intel_batchbuffer_reset(batch);
}

void
gen9_media_spinfunc(struct intel_batchbuffer *batch,
		    const struct igt_buf *dst, uint32_t spins)
{
	uint32_t curbe_buffer, interface_descriptor;
	uint32_t batch_end;

	intel_batchbuffer_flush_with_context(batch, NULL);

	/* setup states */
	batch->ptr = &batch->buffer[BATCH_STATE_SPLIT];

	curbe_buffer = gen8_spin_curbe_buffer_data(batch, spins);
	interface_descriptor = gen8_fill_interface_descriptor(batch, dst,
					      spin_kernel, sizeof(spin_kernel));
	igt_assert(batch->ptr < &batch->buffer[4095]);

	/* media pipeline */
	batch->ptr = batch->buffer;
	OUT_BATCH(GEN8_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA |
		  GEN9_FORCE_MEDIA_AWAKE_ENABLE |
		  GEN9_SAMPLER_DOP_GATE_DISABLE |
		  GEN9_PIPELINE_SELECTION_MASK |
		  GEN9_SAMPLER_DOP_GATE_MASK |
		  GEN9_FORCE_MEDIA_AWAKE_MASK);
	gen9_emit_state_base_address(batch);

	gen8_emit_vfe_state(batch, THREADS, MEDIA_URB_ENTRIES,
			    MEDIA_URB_SIZE, MEDIA_CURBE_SIZE);

	gen7_emit_curbe_load(batch, curbe_buffer);

	gen7_emit_interface_descriptor_load(batch, interface_descriptor);

	gen_emit_media_object(batch, xoffset, yoffset);

	OUT_BATCH(GEN8_PIPELINE_SELECT | PIPELINE_SELECT_MEDIA |
		  GEN9_FORCE_MEDIA_AWAKE_DISABLE |
		  GEN9_SAMPLER_DOP_GATE_ENABLE |
		  GEN9_PIPELINE_SELECTION_MASK |
		  GEN9_SAMPLER_DOP_GATE_MASK |
		  GEN9_FORCE_MEDIA_AWAKE_MASK);

	OUT_BATCH(MI_BATCH_BUFFER_END);

	batch_end = intel_batchbuffer_align(batch, 8);
	igt_assert(batch_end < BATCH_STATE_SPLIT);

	gen7_render_flush(batch, batch_end);
	intel_batchbuffer_reset(batch);
}
