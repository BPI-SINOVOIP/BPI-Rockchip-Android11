/*
 * Copyright © 2018 Intel Corporation
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

#include "gpu_cmds.h"

void
gen7_render_flush(struct intel_batchbuffer *batch, uint32_t batch_end)
{
	int ret;

	ret = drm_intel_bo_subdata(batch->bo, 0, 4096, batch->buffer);
	if (ret == 0)
		ret = drm_intel_bo_mrb_exec(batch->bo, batch_end,
					    NULL, 0, 0, 0);
	igt_assert(ret == 0);
}

void
gen7_render_context_flush(struct intel_batchbuffer *batch, uint32_t batch_end)
{
	int ret;

	ret = drm_intel_bo_subdata(batch->bo, 0, 4096, batch->buffer);
	if (ret == 0)
		ret = drm_intel_gem_bo_context_exec(batch->bo, batch->ctx,
				batch_end, 0);
	igt_assert(ret == 0);
}

uint32_t
gen7_fill_curbe_buffer_data(struct intel_batchbuffer *batch,
			    uint8_t color)
{
	uint8_t *curbe_buffer;
	uint32_t offset;

	curbe_buffer = intel_batchbuffer_subdata_alloc(batch,
						       sizeof(uint32_t) * 8,
						       64);
	offset = intel_batchbuffer_subdata_offset(batch, curbe_buffer);
	*curbe_buffer = color;

	return offset;
}

uint32_t
gen11_fill_curbe_buffer_data(struct intel_batchbuffer *batch)
{
	uint32_t *curbe_buffer;
	uint32_t offset;

	curbe_buffer = intel_batchbuffer_subdata_alloc(batch,
						       sizeof(uint32_t) * 8,
						       64);
	offset = intel_batchbuffer_subdata_offset(batch, curbe_buffer);
	*curbe_buffer++ = 0;
	*curbe_buffer   = 1;

	return offset;
}

uint32_t
gen7_fill_surface_state(struct intel_batchbuffer *batch,
			const struct igt_buf *buf,
			uint32_t format,
			int is_dst)
{
	struct gen7_surface_state *ss;
	uint32_t write_domain, read_domain, offset;
	int ret;

	if (is_dst) {
		write_domain = read_domain = I915_GEM_DOMAIN_RENDER;
	} else {
		write_domain = 0;
		read_domain = I915_GEM_DOMAIN_SAMPLER;
	}

	ss = intel_batchbuffer_subdata_alloc(batch, sizeof(*ss), 64);
	offset = intel_batchbuffer_subdata_offset(batch, ss);

	ss->ss0.surface_type = SURFACE_2D;
	ss->ss0.surface_format = format;
	ss->ss0.render_cache_read_write = 1;

	if (buf->tiling == I915_TILING_X)
		ss->ss0.tiled_mode = 2;
	else if (buf->tiling == I915_TILING_Y)
		ss->ss0.tiled_mode = 3;

	ss->ss1.base_addr = buf->bo->offset;
	ret = drm_intel_bo_emit_reloc(batch->bo,
				intel_batchbuffer_subdata_offset(batch, ss) + 4,
				buf->bo, 0,
				read_domain, write_domain);
	igt_assert(ret == 0);

	ss->ss2.height = igt_buf_height(buf) - 1;
	ss->ss2.width  = igt_buf_width(buf) - 1;

	ss->ss3.pitch  = buf->stride - 1;

	ss->ss7.shader_chanel_select_r = 4;
	ss->ss7.shader_chanel_select_g = 5;
	ss->ss7.shader_chanel_select_b = 6;
	ss->ss7.shader_chanel_select_a = 7;

	return offset;
}

uint32_t
gen7_fill_binding_table(struct intel_batchbuffer *batch,
			const struct igt_buf *dst)
{
	uint32_t *binding_table, offset;

	binding_table = intel_batchbuffer_subdata_alloc(batch, 32, 64);
	offset = intel_batchbuffer_subdata_offset(batch, binding_table);
	if (IS_GEN7(batch->devid))
		binding_table[0] = gen7_fill_surface_state(batch, dst,
						SURFACEFORMAT_R8_UNORM, 1);
	else
		binding_table[0] = gen8_fill_surface_state(batch, dst,
						SURFACEFORMAT_R8_UNORM, 1);

	return offset;
}

uint32_t
gen11_fill_binding_table(struct intel_batchbuffer *batch,
			const struct igt_buf *src,const struct igt_buf *dst)
{
	uint32_t *binding_table, offset;

	binding_table = intel_batchbuffer_subdata_alloc(batch, 64, 64);
	offset = intel_batchbuffer_subdata_offset(batch, binding_table);
	binding_table[0] = gen11_fill_surface_state(batch, src,
						SURFACE_1D,SURFACEFORMAT_R32G32B32A32_FLOAT,
						0,0,
						0);
	binding_table[1] = gen11_fill_surface_state(batch, dst,
						SURFACE_BUFFER, SURFACEFORMAT_RAW,
						1,1,
						1);

	return offset;
}

uint32_t
gen7_fill_kernel(struct intel_batchbuffer *batch,
		const uint32_t kernel[][4],
		size_t size)
{
	uint32_t offset;

	offset = intel_batchbuffer_copy_data(batch, kernel, size, 64);

	return offset;
}

uint32_t
gen7_fill_interface_descriptor(struct intel_batchbuffer *batch,
			       const struct igt_buf *dst,
			       const uint32_t kernel[][4],
			       size_t size)
{
	struct gen7_interface_descriptor_data *idd;
	uint32_t offset;
	uint32_t binding_table_offset, kernel_offset;

	binding_table_offset = gen7_fill_binding_table(batch, dst);
	kernel_offset = gen7_fill_kernel(batch, kernel, size);

	idd = intel_batchbuffer_subdata_alloc(batch, sizeof(*idd), 64);
	offset = intel_batchbuffer_subdata_offset(batch, idd);

	idd->desc0.kernel_start_pointer = (kernel_offset >> 6);

	idd->desc1.single_program_flow = 1;
	idd->desc1.floating_point_mode = GEN7_FLOATING_POINT_IEEE_754;

	idd->desc2.sampler_count = 0;      /* 0 samplers used */
	idd->desc2.sampler_state_pointer = 0;

	idd->desc3.binding_table_entry_count = 0;
	idd->desc3.binding_table_pointer = (binding_table_offset >> 5);

	idd->desc4.constant_urb_entry_read_offset = 0;
	idd->desc4.constant_urb_entry_read_length = 1; /* grf 1 */

	return offset;
}

void
gen7_emit_state_base_address(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_STATE_BASE_ADDRESS | (10 - 2));

	/* general */
	OUT_BATCH(0);

	/* surface */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);

	/* dynamic */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);

	/* indirect */
	OUT_BATCH(0);

	/* instruction */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);

	/* general/dynamic/indirect/instruction access Bound */
	OUT_BATCH(0);
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
}

void
gen7_emit_vfe_state(struct intel_batchbuffer *batch, uint32_t threads,
		    uint32_t urb_entries, uint32_t urb_size,
		    uint32_t curbe_size, uint32_t mode)
{
	OUT_BATCH(GEN7_MEDIA_VFE_STATE | (8 - 2));

	/* scratch buffer */
	OUT_BATCH(0);

	/* number of threads & urb entries */
	OUT_BATCH(threads << 16 |
		urb_entries << 8 |
		mode << 2); /* GPGPU vs media mode */

	OUT_BATCH(0);

	/* urb entry size & curbe size */
	OUT_BATCH(urb_size << 16 |	/* in 256 bits unit */
		  curbe_size);		/* in 256 bits unit */

	/* scoreboard */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

void
gen7_emit_curbe_load(struct intel_batchbuffer *batch, uint32_t curbe_buffer)
{
	OUT_BATCH(GEN7_MEDIA_CURBE_LOAD | (4 - 2));
	OUT_BATCH(0);
	/* curbe total data length */
	OUT_BATCH(64);
	/* curbe data start address, is relative to the dynamics base address */
	OUT_BATCH(curbe_buffer);
}

void
gen7_emit_interface_descriptor_load(struct intel_batchbuffer *batch,
				    uint32_t interface_descriptor)
{
	OUT_BATCH(GEN7_MEDIA_INTERFACE_DESCRIPTOR_LOAD | (4 - 2));
	OUT_BATCH(0);
	/* interface descriptor data length */
	if (IS_GEN7(batch->devid))
		OUT_BATCH(sizeof(struct gen7_interface_descriptor_data));
	else
		OUT_BATCH(sizeof(struct gen8_interface_descriptor_data));
	/* interface descriptor address, is relative to the dynamics base
	 * address
	 */
	OUT_BATCH(interface_descriptor);
}

void
gen7_emit_media_objects(struct intel_batchbuffer *batch,
			unsigned int x, unsigned int y,
			unsigned int width, unsigned int height)
{
	int i, j;

	for (i = 0; i < width / 16; i++) {
		for (j = 0; j < height / 16; j++) {
			gen_emit_media_object(batch, x + i * 16, y + j * 16);
		}
	}
}

void
gen7_emit_gpgpu_walk(struct intel_batchbuffer *batch,
		     unsigned int x, unsigned int y,
		     unsigned int width, unsigned int height)
{
	uint32_t x_dim, y_dim, tmp, right_mask;

	/*
	 * Simply do SIMD16 based dispatch, so every thread uses
	 * SIMD16 channels.
	 *
	 * Define our own thread group size, e.g 16x1 for every group, then
	 * will have 1 thread each group in SIMD16 dispatch. So thread
	 * width/height/depth are all 1.
	 *
	 * Then thread group X = width / 16 (aligned to 16)
	 * thread group Y = height;
	 */
	x_dim = (width + 15) / 16;
	y_dim = height;

	tmp = width & 15;
	if (tmp == 0)
		right_mask = (1 << 16) - 1;
	else
		right_mask = (1 << tmp) - 1;

	OUT_BATCH(GEN7_GPGPU_WALKER | 9);

	/* interface descriptor offset */
	OUT_BATCH(0);

	/* SIMD size, thread w/h/d */
	OUT_BATCH(1 << 30 | /* SIMD16 */
		  0 << 16 | /* depth:1 */
		  0 << 8 | /* height:1 */
		  0); /* width:1 */

	/* thread group X */
	OUT_BATCH(0);
	OUT_BATCH(x_dim);

	/* thread group Y */
	OUT_BATCH(0);
	OUT_BATCH(y_dim);

	/* thread group Z */
	OUT_BATCH(0);
	OUT_BATCH(1);

	/* right mask */
	OUT_BATCH(right_mask);

	/* bottom mask, height 1, always 0xffffffff */
	OUT_BATCH(0xffffffff);
}

uint32_t
gen8_spin_curbe_buffer_data(struct intel_batchbuffer *batch,
			    uint32_t iters)
{
	uint32_t *curbe_buffer;
	uint32_t offset;

	curbe_buffer = intel_batchbuffer_subdata_alloc(batch, 64, 64);
	offset = intel_batchbuffer_subdata_offset(batch, curbe_buffer);
	*curbe_buffer = iters;

	return offset;
}

uint32_t
gen8_fill_surface_state(struct intel_batchbuffer *batch,
			const struct igt_buf *buf,
			uint32_t format,
			int is_dst)
{
	struct gen8_surface_state *ss;
	uint32_t write_domain, read_domain, offset;
	int ret;

	if (is_dst) {
		write_domain = read_domain = I915_GEM_DOMAIN_RENDER;
	} else {
		write_domain = 0;
		read_domain = I915_GEM_DOMAIN_SAMPLER;
	}

	ss = intel_batchbuffer_subdata_alloc(batch, sizeof(*ss), 64);
	offset = intel_batchbuffer_subdata_offset(batch, ss);

	ss->ss0.surface_type = SURFACE_2D;
	ss->ss0.surface_format = format;
	ss->ss0.render_cache_read_write = 1;
	ss->ss0.vertical_alignment = 1; /* align 4 */
	ss->ss0.horizontal_alignment = 1; /* align 4 */

	if (buf->tiling == I915_TILING_X)
		ss->ss0.tiled_mode = 2;
	else if (buf->tiling == I915_TILING_Y)
		ss->ss0.tiled_mode = 3;

	ss->ss8.base_addr = buf->bo->offset;

	ret = drm_intel_bo_emit_reloc(batch->bo,
				intel_batchbuffer_subdata_offset(batch, ss) + 8 * 4,
				buf->bo, 0, read_domain, write_domain);
	igt_assert(ret == 0);

	ss->ss2.height = igt_buf_height(buf) - 1;
	ss->ss2.width  = igt_buf_width(buf) - 1;
	ss->ss3.pitch  = buf->stride - 1;

	ss->ss7.shader_chanel_select_r = 4;
	ss->ss7.shader_chanel_select_g = 5;
	ss->ss7.shader_chanel_select_b = 6;
	ss->ss7.shader_chanel_select_a = 7;

	return offset;
}

uint32_t
gen11_fill_surface_state(struct intel_batchbuffer *batch,
			const struct igt_buf *buf,
			uint32_t surface_type,
			uint32_t format,
			uint32_t vertical_alignment,
			uint32_t horizontal_alignment,
			int is_dst)
{
	struct gen8_surface_state *ss;
	uint32_t write_domain, read_domain, offset;
	int ret;

	if (is_dst) {
		write_domain = read_domain = I915_GEM_DOMAIN_RENDER;
	} else {
		write_domain = 0;
		read_domain = I915_GEM_DOMAIN_SAMPLER;
	}

	ss = intel_batchbuffer_subdata_alloc(batch, sizeof(*ss), 64);
	offset = intel_batchbuffer_subdata_offset(batch, ss);

	ss->ss0.surface_type = surface_type;
	ss->ss0.surface_format = format;
	ss->ss0.render_cache_read_write = 1;
	ss->ss0.vertical_alignment = vertical_alignment; /* align 4 */
	ss->ss0.horizontal_alignment = horizontal_alignment; /* align 4 */

	if (buf->tiling == I915_TILING_X)
		ss->ss0.tiled_mode = 2;
	else if (buf->tiling == I915_TILING_Y)
		ss->ss0.tiled_mode = 3;
	else
		ss->ss0.tiled_mode = 0;

	ss->ss8.base_addr = buf->bo->offset;

	ret = drm_intel_bo_emit_reloc(batch->bo,
				intel_batchbuffer_subdata_offset(batch, ss) + 8 * 4,
				buf->bo, 0, read_domain, write_domain);
	igt_assert(ret == 0);

	if (is_dst) {
		ss->ss1.memory_object_control = 2;
		ss->ss2.height = 1;
		ss->ss2.width  = 95;
		ss->ss3.pitch  = 0;
		ss->ss7.shader_chanel_select_r = 4;
		ss->ss7.shader_chanel_select_g = 5;
		ss->ss7.shader_chanel_select_b = 6;
		ss->ss7.shader_chanel_select_a = 7;
	}
	else {
		ss->ss1.qpitch = 4040;
		ss->ss1.base_mip_level = 31;
		ss->ss2.height = 9216;
		ss->ss2.width  = 1019;
		ss->ss3.pitch  = 64;
		ss->ss5.mip_count = 2;
	}

	return offset;
}

uint32_t
gen8_fill_interface_descriptor(struct intel_batchbuffer *batch,
			       const struct igt_buf *dst,
			       const uint32_t kernel[][4],
			       size_t size)
{
	struct gen8_interface_descriptor_data *idd;
	uint32_t offset;
	uint32_t binding_table_offset, kernel_offset;

	binding_table_offset = gen7_fill_binding_table(batch, dst);
	kernel_offset = gen7_fill_kernel(batch, kernel, size);

	idd = intel_batchbuffer_subdata_alloc(batch, sizeof(*idd), 64);
	offset = intel_batchbuffer_subdata_offset(batch, idd);

	idd->desc0.kernel_start_pointer = (kernel_offset >> 6);

	idd->desc2.single_program_flow = 1;
	idd->desc2.floating_point_mode = GEN8_FLOATING_POINT_IEEE_754;

	idd->desc3.sampler_count = 0;      /* 0 samplers used */
	idd->desc3.sampler_state_pointer = 0;

	idd->desc4.binding_table_entry_count = 0;
	idd->desc4.binding_table_pointer = (binding_table_offset >> 5);

	idd->desc5.constant_urb_entry_read_offset = 0;
	idd->desc5.constant_urb_entry_read_length = 1; /* grf 1 */

	idd->desc6.num_threads_in_tg = 1;

	return offset;
}

uint32_t
gen11_fill_interface_descriptor(struct intel_batchbuffer *batch,
			       const struct igt_buf *src,const struct igt_buf *dst,
			       const uint32_t kernel[][4],
			       size_t size)
{
	struct gen8_interface_descriptor_data *idd;
	uint32_t offset;
	uint32_t binding_table_offset, kernel_offset;

	binding_table_offset = gen11_fill_binding_table(batch, src,dst);
	kernel_offset = gen7_fill_kernel(batch, kernel, size);

	idd = intel_batchbuffer_subdata_alloc(batch, sizeof(*idd), 64);
	offset = intel_batchbuffer_subdata_offset(batch, idd);

	idd->desc0.kernel_start_pointer = (kernel_offset >> 6);

	idd->desc2.single_program_flow = 1;
	idd->desc2.floating_point_mode = GEN8_FLOATING_POINT_IEEE_754;

	idd->desc3.sampler_count = 0;      /* 0 samplers used */
	idd->desc3.sampler_state_pointer = 0;

	idd->desc4.binding_table_entry_count = 0;
	idd->desc4.binding_table_pointer = (binding_table_offset >> 5);

	idd->desc5.constant_urb_entry_read_offset = 0;
	idd->desc5.constant_urb_entry_read_length = 1; /* grf 1 */

	idd->desc6.num_threads_in_tg = 1;

	return offset;
}

void
gen8_emit_state_base_address(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN8_STATE_BASE_ADDRESS | (16 - 2));

	/* general */
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);

	/* stateless data port */
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);

	/* surface */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_SAMPLER, 0, BASE_ADDRESS_MODIFY);

	/* dynamic */
	OUT_RELOC(batch->bo,
		  I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_INSTRUCTION,
		  0, BASE_ADDRESS_MODIFY);

	/* indirect */
	OUT_BATCH(0);
	OUT_BATCH(0);

	/* instruction */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);

	/* general state buffer size */
	OUT_BATCH(0xfffff000 | 1);
	/* dynamic state buffer size */
	OUT_BATCH(1 << 12 | 1);
	/* indirect object buffer size */
	OUT_BATCH(0xfffff000 | 1);
	/* instruction buffer size, must set modify enable bit, otherwise it may
	 * result in GPU hang
	 */
	OUT_BATCH(1 << 12 | 1);
}

void
gen8_emit_media_state_flush(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN8_MEDIA_STATE_FLUSH | (2 - 2));
	OUT_BATCH(0);
}

void
gen8_emit_vfe_state(struct intel_batchbuffer *batch, uint32_t threads,
		    uint32_t urb_entries, uint32_t urb_size,
		    uint32_t curbe_size)
{
	OUT_BATCH(GEN7_MEDIA_VFE_STATE | (9 - 2));

	/* scratch buffer */
	OUT_BATCH(0);
	OUT_BATCH(0);

	/* number of threads & urb entries */
	OUT_BATCH(threads << 16 |
		urb_entries << 8);

	OUT_BATCH(0);

	/* urb entry size & curbe size */
	OUT_BATCH(urb_size << 16 |
		curbe_size);

	/* scoreboard */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

void
gen8_emit_gpgpu_walk(struct intel_batchbuffer *batch,
		     unsigned int x, unsigned int y,
		     unsigned int width, unsigned int height)
{
	uint32_t x_dim, y_dim, tmp, right_mask;

	/*
	 * Simply do SIMD16 based dispatch, so every thread uses
	 * SIMD16 channels.
	 *
	 * Define our own thread group size, e.g 16x1 for every group, then
	 * will have 1 thread each group in SIMD16 dispatch. So thread
	 * width/height/depth are all 1.
	 *
	 * Then thread group X = width / 16 (aligned to 16)
	 * thread group Y = height;
	 */
	x_dim = (width + 15) / 16;
	y_dim = height;

	tmp = width & 15;
	if (tmp == 0)
		right_mask = (1 << 16) - 1;
	else
		right_mask = (1 << tmp) - 1;

	OUT_BATCH(GEN7_GPGPU_WALKER | 13);

	OUT_BATCH(0); /* kernel offset */
	OUT_BATCH(0); /* indirect data length */
	OUT_BATCH(0); /* indirect data offset */

	/* SIMD size, thread w/h/d */
	OUT_BATCH(1 << 30 | /* SIMD16 */
		  0 << 16 | /* depth:1 */
		  0 << 8 | /* height:1 */
		  0); /* width:1 */

	/* thread group X */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(x_dim);

	/* thread group Y */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(y_dim);

	/* thread group Z */
	OUT_BATCH(0);
	OUT_BATCH(1);

	/* right mask */
	OUT_BATCH(right_mask);

	/* bottom mask, height 1, always 0xffffffff */
	OUT_BATCH(0xffffffff);
}

void
gen_emit_media_object(struct intel_batchbuffer *batch,
		       unsigned int xoffset, unsigned int yoffset)
{
	OUT_BATCH(GEN7_MEDIA_OBJECT | (8 - 2));

	/* interface descriptor offset */
	OUT_BATCH(0);

	/* without indirect data */
	OUT_BATCH(0);
	OUT_BATCH(0);

	/* scoreboard */
	OUT_BATCH(0);
	OUT_BATCH(0);

	/* inline data (xoffset, yoffset) */
	OUT_BATCH(xoffset);
	OUT_BATCH(yoffset);
	if (AT_LEAST_GEN(batch->devid, 8) && !IS_CHERRYVIEW(batch->devid))
		gen8_emit_media_state_flush(batch);
}

void
gen9_emit_state_base_address(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN8_STATE_BASE_ADDRESS | (19 - 2));

	/* general */
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);

	/* stateless data port */
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);

	/* surface */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_SAMPLER, 0, BASE_ADDRESS_MODIFY);

	/* dynamic */
	OUT_RELOC(batch->bo,
		  I915_GEM_DOMAIN_RENDER | I915_GEM_DOMAIN_INSTRUCTION,
		  0, BASE_ADDRESS_MODIFY);

	/* indirect */
	OUT_BATCH(0);
	OUT_BATCH(0);

	/* instruction */
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);

	/* general state buffer size */
	OUT_BATCH(0xfffff000 | 1);
	/* dynamic state buffer size */
	OUT_BATCH(1 << 12 | 1);
	/* indirect object buffer size */
	OUT_BATCH(0xfffff000 | 1);
	/* intruction buffer size, must set modify enable bit, otherwise it may
	 * result in GPU hang
	 */
	OUT_BATCH(1 << 12 | 1);

	/* Bindless surface state base address */
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);
	OUT_BATCH(0xfffff000);
}
