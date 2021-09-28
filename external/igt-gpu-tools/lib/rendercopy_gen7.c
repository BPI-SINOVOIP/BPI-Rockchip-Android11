#include <assert.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "drm.h"
#include "i915_drm.h"
#include "drmtest.h"
#include "intel_bufmgr.h"
#include "intel_batchbuffer.h"
#include "intel_io.h"
#include "intel_chipset.h"
#include "rendercopy.h"
#include "gen7_render.h"
#include "intel_reg.h"


static const uint32_t ps_kernel[][4] = {
	{ 0x0080005a, 0x2e2077bd, 0x000000c0, 0x008d0040 },
	{ 0x0080005a, 0x2e6077bd, 0x000000d0, 0x008d0040 },
	{ 0x02800031, 0x21801fa9, 0x008d0e20, 0x08840001 },
	{ 0x00800001, 0x2e2003bd, 0x008d0180, 0x00000000 },
	{ 0x00800001, 0x2e6003bd, 0x008d01c0, 0x00000000 },
	{ 0x00800001, 0x2ea003bd, 0x008d0200, 0x00000000 },
	{ 0x00800001, 0x2ee003bd, 0x008d0240, 0x00000000 },
	{ 0x05800031, 0x20001fa8, 0x008d0e20, 0x90031000 },
};

static void
gen7_render_flush(struct intel_batchbuffer *batch,
		  drm_intel_context *context, uint32_t batch_end)
{
	int ret;

	ret = drm_intel_bo_subdata(batch->bo, 0, 4096, batch->buffer);
	if (ret == 0)
		ret = drm_intel_gem_bo_context_exec(batch->bo, context,
						    batch_end, 0);
	igt_assert(ret == 0);
}

static uint32_t
gen7_tiling_bits(uint32_t tiling)
{
	switch (tiling) {
	default: igt_assert(0);
	case I915_TILING_NONE: return 0;
	case I915_TILING_X: return GEN7_SURFACE_TILED;
	case I915_TILING_Y: return GEN7_SURFACE_TILED | GEN7_SURFACE_TILED_Y;
	}
}

static uint32_t
gen7_bind_buf(struct intel_batchbuffer *batch,
	      const struct igt_buf *buf,
	      int is_dst)
{
	uint32_t format, *ss;
	uint32_t write_domain, read_domain;
	int ret;

	igt_assert_lte(buf->stride, 256*1024);
	igt_assert_lte(igt_buf_width(buf), 16384);
	igt_assert_lte(igt_buf_height(buf), 16384);

	switch (buf->bpp) {
		case 8: format = SURFACEFORMAT_R8_UNORM; break;
		case 16: format = SURFACEFORMAT_R8G8_UNORM; break;
		case 32: format = SURFACEFORMAT_B8G8R8A8_UNORM; break;
		case 64: format = SURFACEFORMAT_R16G16B16A16_FLOAT; break;
		default: igt_assert(0);
	}

	if (is_dst) {
		write_domain = read_domain = I915_GEM_DOMAIN_RENDER;
	} else {
		write_domain = 0;
		read_domain = I915_GEM_DOMAIN_SAMPLER;
	}

	ss = intel_batchbuffer_subdata_alloc(batch, 8 * sizeof(*ss), 32);

	ss[0] = (SURFACE_2D << GEN7_SURFACE_TYPE_SHIFT |
		 gen7_tiling_bits(buf->tiling) |
		format << GEN7_SURFACE_FORMAT_SHIFT);
	ss[1] = buf->bo->offset;
	ss[2] = ((igt_buf_width(buf) - 1)  << GEN7_SURFACE_WIDTH_SHIFT |
		 (igt_buf_height(buf) - 1) << GEN7_SURFACE_HEIGHT_SHIFT);
	ss[3] = (buf->stride - 1) << GEN7_SURFACE_PITCH_SHIFT;
	ss[4] = 0;
	if (IS_VALLEYVIEW(batch->devid))
		ss[5] = VLV_MOCS_L3 << 16;
	else
		ss[5] = (IVB_MOCS_L3 | IVB_MOCS_PTE) << 16;
	ss[6] = 0;
	ss[7] = 0;
	if (IS_HASWELL(batch->devid))
		ss[7] |= HSW_SURFACE_SWIZZLE(RED, GREEN, BLUE, ALPHA);

	ret = drm_intel_bo_emit_reloc(batch->bo,
				      intel_batchbuffer_subdata_offset(batch, &ss[1]),
				      buf->bo, 0,
				      read_domain, write_domain);
	igt_assert(ret == 0);

	return intel_batchbuffer_subdata_offset(batch, ss);
}

static void
gen7_emit_vertex_elements(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN4_3DSTATE_VERTEX_ELEMENTS |
		  ((2 * (1 + 2)) + 1 - 2));

	OUT_BATCH(0 << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN6_VE0_VALID |
		  SURFACEFORMAT_R32G32B32A32_FLOAT << VE0_FORMAT_SHIFT |
		  0 << VE0_OFFSET_SHIFT);

	OUT_BATCH(GEN4_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_0_SHIFT |
		  GEN4_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_1_SHIFT |
		  GEN4_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_2_SHIFT |
		  GEN4_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_3_SHIFT);

	/* x,y */
	OUT_BATCH(0 << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN6_VE0_VALID |
		  SURFACEFORMAT_R16G16_SSCALED << VE0_FORMAT_SHIFT |
		  0 << VE0_OFFSET_SHIFT); /* offsets vb in bytes */
	OUT_BATCH(GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT |
		  GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT |
		  GEN4_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_2_SHIFT |
		  GEN4_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT);

	/* s,t */
	OUT_BATCH(0 << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT | GEN6_VE0_VALID |
		  SURFACEFORMAT_R16G16_SSCALED << VE0_FORMAT_SHIFT |
		  4 << VE0_OFFSET_SHIFT);  /* offset vb in bytes */
	OUT_BATCH(GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT |
		  GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT |
		  GEN4_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_2_SHIFT |
		  GEN4_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT);
}

static uint32_t
gen7_create_vertex_buffer(struct intel_batchbuffer *batch,
			  uint32_t src_x, uint32_t src_y,
			  uint32_t dst_x, uint32_t dst_y,
			  uint32_t width, uint32_t height)
{
	uint16_t *v;

	v = intel_batchbuffer_subdata_alloc(batch, 12 * sizeof(*v), 8);

	v[0] = dst_x + width;
	v[1] = dst_y + height;
	v[2] = src_x + width;
	v[3] = src_y + height;

	v[4] = dst_x;
	v[5] = dst_y + height;
	v[6] = src_x;
	v[7] = src_y + height;

	v[8] = dst_x;
	v[9] = dst_y;
	v[10] = src_x;
	v[11] = src_y;

	return intel_batchbuffer_subdata_offset(batch, v);
}

static void gen7_emit_vertex_buffer(struct intel_batchbuffer *batch,
				    int src_x, int src_y,
				    int dst_x, int dst_y,
				    int width, int height,
				    uint32_t offset)
{
	OUT_BATCH(GEN4_3DSTATE_VERTEX_BUFFERS | (5 - 2));
	OUT_BATCH(0 << GEN6_VB0_BUFFER_INDEX_SHIFT |
		  GEN6_VB0_VERTEXDATA |
		  GEN7_VB0_ADDRESS_MODIFY_ENABLE |
		  4 * 2 << VB0_BUFFER_PITCH_SHIFT);

	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_VERTEX, 0, offset);
	OUT_BATCH(~0);
	OUT_BATCH(0);
}

static uint32_t
gen7_bind_surfaces(struct intel_batchbuffer *batch,
		   const struct igt_buf *src,
		   const struct igt_buf *dst)
{
	uint32_t *binding_table;

	binding_table = intel_batchbuffer_subdata_alloc(batch, 8, 32);

	binding_table[0] = gen7_bind_buf(batch, dst, 1);
	binding_table[1] = gen7_bind_buf(batch, src, 0);

	return intel_batchbuffer_subdata_offset(batch, binding_table);
}

static void
gen7_emit_binding_table(struct intel_batchbuffer *batch,
			const struct igt_buf *src,
			const struct igt_buf *dst,
			uint32_t bind_surf_off)
{
	OUT_BATCH(GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS | (2 - 2));
	OUT_BATCH(bind_surf_off);
}

static void
gen7_emit_drawing_rectangle(struct intel_batchbuffer *batch, const struct igt_buf *dst)
{
	OUT_BATCH(GEN4_3DSTATE_DRAWING_RECTANGLE | (4 - 2));
	OUT_BATCH(0);
	OUT_BATCH((igt_buf_height(dst) - 1) << 16 | (igt_buf_width(dst) - 1));
	OUT_BATCH(0);
}

static uint32_t
gen7_create_blend_state(struct intel_batchbuffer *batch)
{
	struct gen6_blend_state *blend;

	blend = intel_batchbuffer_subdata_alloc(batch, sizeof(*blend), 64);

	blend->blend0.dest_blend_factor = GEN6_BLENDFACTOR_ZERO;
	blend->blend0.source_blend_factor = GEN6_BLENDFACTOR_ONE;
	blend->blend0.blend_func = GEN6_BLENDFUNCTION_ADD;
	blend->blend1.post_blend_clamp_enable = 1;
	blend->blend1.pre_blend_clamp_enable = 1;

	return intel_batchbuffer_subdata_offset(batch, blend);
}

static void
gen7_emit_state_base_address(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN4_STATE_BASE_ADDRESS | (10 - 2));
	OUT_BATCH(0);
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY);
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);
	OUT_RELOC(batch->bo, I915_GEM_DOMAIN_INSTRUCTION, 0, BASE_ADDRESS_MODIFY);

	OUT_BATCH(0);
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);
	OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
}

static uint32_t
gen7_create_cc_viewport(struct intel_batchbuffer *batch)
{
	struct gen4_cc_viewport *vp;

	vp = intel_batchbuffer_subdata_alloc(batch, sizeof(*vp), 32);
	vp->min_depth = -1.e35;
	vp->max_depth = 1.e35;

	return intel_batchbuffer_subdata_offset(batch, vp);
}

static void
gen7_emit_cc(struct intel_batchbuffer *batch, uint32_t blend_state,
	     uint32_t cc_viewport)
{
	OUT_BATCH(GEN7_3DSTATE_BLEND_STATE_POINTERS | (2 - 2));
	OUT_BATCH(blend_state);

	OUT_BATCH(GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC | (2 - 2));
	OUT_BATCH(cc_viewport);
}

static uint32_t
gen7_create_sampler(struct intel_batchbuffer *batch)
{
	struct gen7_sampler_state *ss;

	ss = intel_batchbuffer_subdata_alloc(batch, sizeof(*ss), 32);

	ss->ss0.min_filter = GEN4_MAPFILTER_NEAREST;
	ss->ss0.mag_filter = GEN4_MAPFILTER_NEAREST;

	ss->ss3.r_wrap_mode = GEN4_TEXCOORDMODE_CLAMP;
	ss->ss3.s_wrap_mode = GEN4_TEXCOORDMODE_CLAMP;
	ss->ss3.t_wrap_mode = GEN4_TEXCOORDMODE_CLAMP;

	ss->ss3.non_normalized_coord = 1;

	return intel_batchbuffer_subdata_offset(batch, ss);
}

static void
gen7_emit_sampler(struct intel_batchbuffer *batch, uint32_t sampler_off)
{
	OUT_BATCH(GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS | (2 - 2));
	OUT_BATCH(sampler_off);
}

static void
gen7_emit_multisample(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN6_3DSTATE_MULTISAMPLE | (4 - 2));
	OUT_BATCH(GEN6_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER |
		  GEN6_3DSTATE_MULTISAMPLE_NUMSAMPLES_1); /* 1 sample/pixel */
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(GEN6_3DSTATE_SAMPLE_MASK | (2 - 2));
	OUT_BATCH(1);
}

static void
gen7_emit_urb(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS | (2 - 2));
	OUT_BATCH(8); /* in 1KBs */

	/* num of VS entries must be divisible by 8 if size < 9 */
	OUT_BATCH(GEN7_3DSTATE_URB_VS | (2 - 2));
	OUT_BATCH((64 << GEN7_URB_ENTRY_NUMBER_SHIFT) |
		  (2 - 1) << GEN7_URB_ENTRY_SIZE_SHIFT |
		  (1 << GEN7_URB_STARTING_ADDRESS_SHIFT));

	OUT_BATCH(GEN7_3DSTATE_URB_HS | (2 - 2));
	OUT_BATCH((0 << GEN7_URB_ENTRY_SIZE_SHIFT) |
		  (2 << GEN7_URB_STARTING_ADDRESS_SHIFT));

	OUT_BATCH(GEN7_3DSTATE_URB_DS | (2 - 2));
	OUT_BATCH((0 << GEN7_URB_ENTRY_SIZE_SHIFT) |
		  (2 << GEN7_URB_STARTING_ADDRESS_SHIFT));

	OUT_BATCH(GEN7_3DSTATE_URB_GS | (2 - 2));
	OUT_BATCH((0 << GEN7_URB_ENTRY_SIZE_SHIFT) |
		  (1 << GEN7_URB_STARTING_ADDRESS_SHIFT));
}

static void
gen7_emit_vs(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN6_3DSTATE_VS | (6 - 2));
	OUT_BATCH(0); /* no VS kernel */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through */
}

static void
gen7_emit_hs(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_HS | (7 - 2));
	OUT_BATCH(0); /* no HS kernel */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through */
}

static void
gen7_emit_te(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_TE | (4 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen7_emit_ds(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_DS | (6 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen7_emit_gs(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN6_3DSTATE_GS | (7 - 2));
	OUT_BATCH(0); /* no GS kernel */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through  */
}

static void
gen7_emit_streamout(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_STREAMOUT | (3 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen7_emit_sf(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN6_3DSTATE_SF | (7 - 2));
	OUT_BATCH(0);
	OUT_BATCH(GEN6_3DSTATE_SF_CULL_NONE);
	OUT_BATCH(2 << GEN6_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen7_emit_sbe(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_SBE | (14 - 2));
	OUT_BATCH(1 << GEN7_SBE_NUM_OUTPUTS_SHIFT |
		  1 << GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT |
		  1 << GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT);
	OUT_BATCH(0);
	OUT_BATCH(0); /* dw4 */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* dw8 */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* dw12 */
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen7_emit_ps(struct intel_batchbuffer *batch, uint32_t kernel_off)
{
	int threads;

	if (IS_HASWELL(batch->devid))
		threads = 40 << HSW_PS_MAX_THREADS_SHIFT | 1 << HSW_PS_SAMPLE_MASK_SHIFT;
	else
		threads = 40 << IVB_PS_MAX_THREADS_SHIFT;

	OUT_BATCH(GEN7_3DSTATE_PS | (8 - 2));
	OUT_BATCH(kernel_off);
	OUT_BATCH(1 << GEN7_PS_SAMPLER_COUNT_SHIFT |
		  2 << GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT);
	OUT_BATCH(0); /* scratch address */
	OUT_BATCH(threads |
		  GEN7_PS_16_DISPATCH_ENABLE |
		  GEN7_PS_ATTRIBUTE_ENABLE);
	OUT_BATCH(6 << GEN7_PS_DISPATCH_START_GRF_SHIFT_0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen7_emit_clip(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN6_3DSTATE_CLIP | (4 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through */
	OUT_BATCH(0);

	OUT_BATCH(GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CL | (2 - 2));
	OUT_BATCH(0);
}

static void
gen7_emit_wm(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN6_3DSTATE_WM | (3 - 2));
	OUT_BATCH(GEN7_WM_DISPATCH_ENABLE |
		GEN7_WM_PERSPECTIVE_PIXEL_BARYCENTRIC);
	OUT_BATCH(0);
}

static void
gen7_emit_null_depth_buffer(struct intel_batchbuffer *batch)
{
	OUT_BATCH(GEN7_3DSTATE_DEPTH_BUFFER | (7 - 2));
	OUT_BATCH(SURFACE_NULL << GEN4_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT |
		  GEN4_DEPTHFORMAT_D32_FLOAT << GEN4_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT);
	OUT_BATCH(0); /* disable depth, stencil and hiz */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(GEN7_3DSTATE_CLEAR_PARAMS | (3 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
}

#define BATCH_STATE_SPLIT 2048
void gen7_render_copyfunc(struct intel_batchbuffer *batch,
			  drm_intel_context *context,
			  const struct igt_buf *src, unsigned src_x, unsigned src_y,
			  unsigned width, unsigned height,
			  const struct igt_buf *dst, unsigned dst_x, unsigned dst_y)
{
	uint32_t ps_binding_table, ps_sampler_off, ps_kernel_off;
	uint32_t blend_state, cc_viewport;
	uint32_t vertex_buffer;
	uint32_t batch_end;

	igt_assert(src->bpp == dst->bpp);
	intel_batchbuffer_flush_with_context(batch, context);

	batch->ptr = &batch->buffer[BATCH_STATE_SPLIT];


	blend_state = gen7_create_blend_state(batch);
	cc_viewport = gen7_create_cc_viewport(batch);
	ps_sampler_off = gen7_create_sampler(batch);
	ps_kernel_off = intel_batchbuffer_copy_data(batch, ps_kernel,
						    sizeof(ps_kernel), 64);
	vertex_buffer = gen7_create_vertex_buffer(batch,
						  src_x, src_y,
						  dst_x, dst_y,
						  width, height);
	ps_binding_table = gen7_bind_surfaces(batch, src, dst);

	igt_assert(batch->ptr < &batch->buffer[4095]);

	batch->ptr = batch->buffer;
	OUT_BATCH(G4X_PIPELINE_SELECT | PIPELINE_SELECT_3D);

	gen7_emit_state_base_address(batch);
	gen7_emit_multisample(batch);
	gen7_emit_urb(batch);
	gen7_emit_vs(batch);
	gen7_emit_hs(batch);
	gen7_emit_te(batch);
	gen7_emit_ds(batch);
	gen7_emit_gs(batch);
	gen7_emit_clip(batch);
	gen7_emit_sf(batch);
	gen7_emit_wm(batch);
	gen7_emit_streamout(batch);
	gen7_emit_null_depth_buffer(batch);
	gen7_emit_cc(batch, blend_state, cc_viewport);
	gen7_emit_sampler(batch, ps_sampler_off);
	gen7_emit_sbe(batch);
	gen7_emit_ps(batch, ps_kernel_off);
	gen7_emit_vertex_elements(batch);
	gen7_emit_vertex_buffer(batch, src_x, src_y,
				dst_x, dst_y, width,
				height, vertex_buffer);
	gen7_emit_binding_table(batch, src, dst, ps_binding_table);
	gen7_emit_drawing_rectangle(batch, dst);

	OUT_BATCH(GEN4_3DPRIMITIVE | (7 - 2));
	OUT_BATCH(GEN4_3DPRIMITIVE_VERTEX_SEQUENTIAL | _3DPRIM_RECTLIST);
	OUT_BATCH(3);
	OUT_BATCH(0);
	OUT_BATCH(1);   /* single instance */
	OUT_BATCH(0);   /* start instance location */
	OUT_BATCH(0);   /* index buffer offset, ignored */

	OUT_BATCH(MI_BATCH_BUFFER_END);

	batch_end = batch->ptr - batch->buffer;
	batch_end = ALIGN(batch_end, 8);
	igt_assert(batch_end < BATCH_STATE_SPLIT);

	gen7_render_flush(batch, context, batch_end);
	intel_batchbuffer_reset(batch);
}
