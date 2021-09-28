/*
 * Copyright (C) 2017 Rob Clark <robclark@freedesktop.org>
 * Copyright © 2018 Google, Inc.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include "util/u_dump.h"
#include "util/half_float.h"

#include "freedreno_blitter.h"
#include "freedreno_fence.h"
#include "freedreno_log.h"
#include "freedreno_resource.h"

#include "fd6_blitter.h"
#include "fd6_format.h"
#include "fd6_emit.h"
#include "fd6_resource.h"

static inline enum a6xx_2d_ifmt
fd6_ifmt(enum a6xx_format fmt)
{
	switch (fmt) {
	case FMT6_A8_UNORM:
	case FMT6_8_UNORM:
	case FMT6_8_SNORM:
	case FMT6_8_8_UNORM:
	case FMT6_8_8_SNORM:
	case FMT6_8_8_8_8_UNORM:
	case FMT6_8_8_8_X8_UNORM:
	case FMT6_8_8_8_8_SNORM:
	case FMT6_4_4_4_4_UNORM:
	case FMT6_5_5_5_1_UNORM:
	case FMT6_5_6_5_UNORM:
		return R2D_UNORM8;

	case FMT6_32_UINT:
	case FMT6_32_SINT:
	case FMT6_32_32_UINT:
	case FMT6_32_32_SINT:
	case FMT6_32_32_32_32_UINT:
	case FMT6_32_32_32_32_SINT:
		return R2D_INT32;

	case FMT6_16_UINT:
	case FMT6_16_SINT:
	case FMT6_16_16_UINT:
	case FMT6_16_16_SINT:
	case FMT6_16_16_16_16_UINT:
	case FMT6_16_16_16_16_SINT:
	case FMT6_10_10_10_2_UINT:
		return R2D_INT16;

	case FMT6_8_UINT:
	case FMT6_8_SINT:
	case FMT6_8_8_UINT:
	case FMT6_8_8_SINT:
	case FMT6_8_8_8_8_UINT:
	case FMT6_8_8_8_8_SINT:
	case FMT6_Z24_UNORM_S8_UINT:
	case FMT6_Z24_UNORM_S8_UINT_AS_R8G8B8A8:
		return R2D_INT8;

	case FMT6_16_UNORM:
	case FMT6_16_SNORM:
	case FMT6_16_16_UNORM:
	case FMT6_16_16_SNORM:
	case FMT6_16_16_16_16_UNORM:
	case FMT6_16_16_16_16_SNORM:
	case FMT6_32_FLOAT:
	case FMT6_32_32_FLOAT:
	case FMT6_32_32_32_32_FLOAT:
		return R2D_FLOAT32;

	case FMT6_16_FLOAT:
	case FMT6_16_16_FLOAT:
	case FMT6_16_16_16_16_FLOAT:
	case FMT6_11_11_10_FLOAT:
	case FMT6_10_10_10_2_UNORM_DEST:
		return R2D_FLOAT16;

	default:
		unreachable("bad format");
		return 0;
	}
}

/* Make sure none of the requested dimensions extend beyond the size of the
 * resource.  Not entirely sure why this happens, but sometimes it does, and
 * w/ 2d blt doesn't have wrap modes like a sampler, so force those cases
 * back to u_blitter
 */
static bool
ok_dims(const struct pipe_resource *r, const struct pipe_box *b, int lvl)
{
	int last_layer =
		r->target == PIPE_TEXTURE_3D ? u_minify(r->depth0, lvl)
		: r->array_size;

	return (b->x >= 0) && (b->x + b->width <= u_minify(r->width0, lvl)) &&
		(b->y >= 0) && (b->y + b->height <= u_minify(r->height0, lvl)) &&
		(b->z >= 0) && (b->z + b->depth <= last_layer);
}

static bool
ok_format(enum pipe_format pfmt)
{
	enum a6xx_format fmt = fd6_pipe2color(pfmt);

	if (util_format_is_compressed(pfmt))
		return true;

	switch (pfmt) {
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
	case PIPE_FORMAT_Z24X8_UNORM:
	case PIPE_FORMAT_Z16_UNORM:
	case PIPE_FORMAT_Z32_UNORM:
	case PIPE_FORMAT_Z32_FLOAT:
	case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
	case PIPE_FORMAT_S8_UINT:
		return true;
	default:
		break;
	}

	if (fmt == FMT6_NONE)
		return false;

	return true;
}

#define DEBUG_BLIT 0
#define DEBUG_BLIT_FALLBACK 0

#define fail_if(cond)													\
	do {																\
		if (cond) {														\
			if (DEBUG_BLIT_FALLBACK) {									\
				fprintf(stderr, "falling back: %s for blit:\n", #cond);	\
				dump_blit_info(info);									\
			}															\
			return false;												\
		}																\
	} while (0)

static bool
is_ubwc(struct pipe_resource *prsc, unsigned level)
{
	return fd_resource_ubwc_enabled(fd_resource(prsc), level);
}

static void
dump_blit_info(const struct pipe_blit_info *info)
{
	util_dump_blit_info(stderr, info);
	fprintf(stderr, "\ndst resource: ");
	util_dump_resource(stderr, info->dst.resource);
	if (is_ubwc(info->dst.resource, info->dst.level))
		fprintf(stderr, " (ubwc)");
	fprintf(stderr, "\nsrc resource: ");
	util_dump_resource(stderr, info->src.resource);
	if (is_ubwc(info->src.resource, info->src.level))
		fprintf(stderr, " (ubwc)");
	fprintf(stderr, "\n");
}

static bool
can_do_blit(const struct pipe_blit_info *info)
{
	/* I think we can do scaling, but not in z dimension since that would
	 * require blending..
	 */
	fail_if(info->dst.box.depth != info->src.box.depth);

	/* Fail if unsupported format: */
	fail_if(!ok_format(info->src.format));
	fail_if(!ok_format(info->dst.format));

	debug_assert(!util_format_is_compressed(info->src.format));
	debug_assert(!util_format_is_compressed(info->dst.format));

	fail_if(!ok_dims(info->src.resource, &info->src.box, info->src.level));

	fail_if(!ok_dims(info->dst.resource, &info->dst.box, info->dst.level));

	debug_assert(info->dst.box.width >= 0);
	debug_assert(info->dst.box.height >= 0);
	debug_assert(info->dst.box.depth >= 0);

	fail_if(info->dst.resource->nr_samples > 1);

	fail_if(info->window_rectangle_include);

	const struct util_format_description *src_desc =
		util_format_description(info->src.format);
	const struct util_format_description *dst_desc =
		util_format_description(info->dst.format);
	const int common_channels = MIN2(src_desc->nr_channels, dst_desc->nr_channels);

	if (info->mask & PIPE_MASK_RGBA) {
		for (int i = 0; i < common_channels; i++) {
			fail_if(memcmp(&src_desc->channel[i],
						   &dst_desc->channel[i],
						   sizeof(src_desc->channel[0])));
		}
	}

	fail_if(info->alpha_blend);

	return true;
}

static void
emit_setup(struct fd_batch *batch)
{
	struct fd_ringbuffer *ring = batch->draw;
	struct fd_screen *screen = batch->ctx->screen;

	fd6_event_write(batch, ring, PC_CCU_FLUSH_COLOR_TS, true);
	fd6_event_write(batch, ring, PC_CCU_FLUSH_DEPTH_TS, true);
	fd6_event_write(batch, ring, PC_CCU_INVALIDATE_COLOR, false);
	fd6_event_write(batch, ring, PC_CCU_INVALIDATE_DEPTH, false);

	/* normal BLIT_OP_SCALE operation needs bypass RB_CCU_CNTL */
	OUT_WFI5(ring);
	OUT_PKT4(ring, REG_A6XX_RB_CCU_CNTL, 1);
	OUT_RING(ring, A6XX_RB_CCU_CNTL_OFFSET(screen->info.a6xx.ccu_offset_bypass));
}

static void
emit_blit_setup(struct fd_ringbuffer *ring,
		enum pipe_format pfmt, bool scissor_enable, union pipe_color_union *color)
{
	enum a6xx_format fmt = fd6_pipe2color(pfmt);
	bool is_srgb = util_format_is_srgb(pfmt);
	enum a6xx_2d_ifmt ifmt = fd6_ifmt(fmt);

	OUT_PKT7(ring, CP_SET_MARKER, 1);
	OUT_RING(ring, A6XX_CP_SET_MARKER_0_MODE(RM6_BLIT2DSCALE));

	if (is_srgb) {
		assert(ifmt == R2D_UNORM8);
		ifmt = R2D_UNORM8_SRGB;
	}

	uint32_t blit_cntl = A6XX_RB_2D_BLIT_CNTL_MASK(0xf) |
		A6XX_RB_2D_BLIT_CNTL_COLOR_FORMAT(fmt) |
		A6XX_RB_2D_BLIT_CNTL_IFMT(ifmt) |
		COND(color, A6XX_RB_2D_BLIT_CNTL_SOLID_COLOR) |
		COND(scissor_enable, A6XX_RB_2D_BLIT_CNTL_SCISSOR);

	OUT_PKT4(ring, REG_A6XX_RB_2D_BLIT_CNTL, 1);
	OUT_RING(ring, blit_cntl);

	OUT_PKT4(ring, REG_A6XX_GRAS_2D_BLIT_CNTL, 1);
	OUT_RING(ring, blit_cntl);

	if (fmt == FMT6_10_10_10_2_UNORM_DEST)
		fmt = FMT6_16_16_16_16_FLOAT;

	/* This register is probably badly named... it seems that it's
	 * controlling the internal/accumulator format or something like
	 * that. It's certainly not tied to only the src format.
	 */
	OUT_PKT4(ring, REG_A6XX_SP_2D_DST_FORMAT, 1);
	OUT_RING(ring, A6XX_SP_2D_DST_FORMAT_COLOR_FORMAT(fmt) |
			COND(util_format_is_pure_sint(pfmt),
					A6XX_SP_2D_DST_FORMAT_SINT) |
			COND(util_format_is_pure_uint(pfmt),
					A6XX_SP_2D_DST_FORMAT_UINT) |
			COND(util_format_is_snorm(pfmt),
					A6XX_SP_2D_DST_FORMAT_SINT |
						A6XX_SP_2D_DST_FORMAT_NORM) |
			COND(util_format_is_unorm(pfmt),
// TODO sometimes blob uses UINT+NORM but dEQP seems unhappy about that
//						A6XX_SP_2D_DST_FORMAT_UINT |
					A6XX_SP_2D_DST_FORMAT_NORM) |
			COND(is_srgb, A6XX_SP_2D_DST_FORMAT_SRGB) |
			A6XX_SP_2D_DST_FORMAT_MASK(0xf));

	OUT_PKT4(ring, REG_A6XX_RB_2D_UNKNOWN_8C01, 1);
	OUT_RING(ring, 0);
}

/* buffers need to be handled specially since x/width can exceed the bounds
 * supported by hw.. if necessary decompose into (potentially) two 2D blits
 */
static void
emit_blit_buffer(struct fd_context *ctx, struct fd_ringbuffer *ring,
		const struct pipe_blit_info *info)
{
	const struct pipe_box *sbox = &info->src.box;
	const struct pipe_box *dbox = &info->dst.box;
	struct fd_resource *src, *dst;
	unsigned sshift, dshift;

	if (DEBUG_BLIT) {
		fprintf(stderr, "buffer blit: ");
		dump_blit_info(info);
	}

	src = fd_resource(info->src.resource);
	dst = fd_resource(info->dst.resource);

	debug_assert(src->layout.cpp == 1);
	debug_assert(dst->layout.cpp == 1);
	debug_assert(info->src.resource->format == info->dst.resource->format);
	debug_assert((sbox->y == 0) && (sbox->height == 1));
	debug_assert((dbox->y == 0) && (dbox->height == 1));
	debug_assert((sbox->z == 0) && (sbox->depth == 1));
	debug_assert((dbox->z == 0) && (dbox->depth == 1));
	debug_assert(sbox->width == dbox->width);
	debug_assert(info->src.level == 0);
	debug_assert(info->dst.level == 0);

	/*
	 * Buffers can have dimensions bigger than max width, remap into
	 * multiple 1d blits to fit within max dimension
	 *
	 * Note that blob uses .ARRAY_PITCH=128 for blitting buffers, which
	 * seems to prevent overfetch related faults.  Not quite sure what
	 * the deal is there.
	 *
	 * Low 6 bits of SRC/DST addresses need to be zero (ie. address
	 * aligned to 64) so we need to shift src/dst x1/x2 to make up the
	 * difference.  On top of already splitting up the blit so width
	 * isn't > 16k.
	 *
	 * We perhaps could do a bit better, if src and dst are aligned but
	 * in the worst case this means we have to split the copy up into
	 * 16k (0x4000) minus 64 (0x40).
	 */

	sshift = sbox->x & 0x3f;
	dshift = dbox->x & 0x3f;

	emit_blit_setup(ring, PIPE_FORMAT_R8_UNORM, false, NULL);

	for (unsigned off = 0; off < sbox->width; off += (0x4000 - 0x40)) {
		unsigned soff, doff, w, p;

		soff = (sbox->x + off) & ~0x3f;
		doff = (dbox->x + off) & ~0x3f;

		w = MIN2(sbox->width - off, (0x4000 - 0x40));
		p = align(w, 64);

		debug_assert((soff + w) <= fd_bo_size(src->bo));
		debug_assert((doff + w) <= fd_bo_size(dst->bo));

		/*
		 * Emit source:
		 */
		OUT_PKT4(ring, REG_A6XX_SP_PS_2D_SRC_INFO, 10);
		OUT_RING(ring, A6XX_SP_PS_2D_SRC_INFO_COLOR_FORMAT(FMT6_8_UNORM) |
				A6XX_SP_PS_2D_SRC_INFO_TILE_MODE(TILE6_LINEAR) |
				 A6XX_SP_PS_2D_SRC_INFO_COLOR_SWAP(WZYX) |
				 0x500000);
		OUT_RING(ring, A6XX_SP_PS_2D_SRC_SIZE_WIDTH(sshift + w) |
				 A6XX_SP_PS_2D_SRC_SIZE_HEIGHT(1)); /* SP_PS_2D_SRC_SIZE */
		OUT_RELOC(ring, src->bo, soff, 0, 0);    /* SP_PS_2D_SRC_LO/HI */
		OUT_RING(ring, A6XX_SP_PS_2D_SRC_PITCH_PITCH(p));

		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);

		/*
		 * Emit destination:
		 */
		OUT_PKT4(ring, REG_A6XX_RB_2D_DST_INFO, 9);
		OUT_RING(ring, A6XX_RB_2D_DST_INFO_COLOR_FORMAT(FMT6_8_UNORM) |
				 A6XX_RB_2D_DST_INFO_TILE_MODE(TILE6_LINEAR) |
				 A6XX_RB_2D_DST_INFO_COLOR_SWAP(WZYX));
		OUT_RELOC(ring, dst->bo, doff, 0, 0);    /* RB_2D_DST_LO/HI */
		OUT_RING(ring, A6XX_RB_2D_DST_PITCH(p));
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);

		/*
		 * Blit command:
		 */
		OUT_PKT4(ring, REG_A6XX_GRAS_2D_SRC_TL_X, 4);
		OUT_RING(ring, A6XX_GRAS_2D_SRC_TL_X(sshift));
		OUT_RING(ring, A6XX_GRAS_2D_SRC_BR_X(sshift + w - 1));
		OUT_RING(ring, A6XX_GRAS_2D_SRC_TL_Y(0));
		OUT_RING(ring, A6XX_GRAS_2D_SRC_BR_Y(0));

		OUT_PKT4(ring, REG_A6XX_GRAS_2D_DST_TL, 2);
		OUT_RING(ring, A6XX_GRAS_2D_DST_TL_X(dshift) | A6XX_GRAS_2D_DST_TL_Y(0));
		OUT_RING(ring, A6XX_GRAS_2D_DST_BR_X(dshift + w - 1) | A6XX_GRAS_2D_DST_BR_Y(0));

		OUT_PKT7(ring, CP_EVENT_WRITE, 1);
		OUT_RING(ring, 0x3f);
		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, ctx->screen->info.a6xx.magic.RB_UNKNOWN_8E04_blit);

		OUT_PKT7(ring, CP_BLIT, 1);
		OUT_RING(ring, CP_BLIT_0_OP(BLIT_OP_SCALE));

		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, 0);             /* RB_UNKNOWN_8E04 */
	}
}

static void
fd6_clear_ubwc(struct fd_batch *batch, struct fd_resource *rsc)
{
	struct fd_ringbuffer *ring = fd_batch_get_prologue(batch);
	union pipe_color_union color = {};

	emit_blit_setup(ring, PIPE_FORMAT_R8_UNORM, false, &color);

	OUT_PKT4(ring, REG_A6XX_SP_PS_2D_SRC_INFO, 13);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A6XX_RB_2D_SRC_SOLID_C0, 4);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A6XX_GRAS_2D_SRC_TL_X, 4);
	OUT_RING(ring, A6XX_GRAS_2D_SRC_TL_X(0));
	OUT_RING(ring, A6XX_GRAS_2D_SRC_BR_X(0));
	OUT_RING(ring, A6XX_GRAS_2D_SRC_TL_Y(0));
	OUT_RING(ring, A6XX_GRAS_2D_SRC_BR_Y(0));

	unsigned size = rsc->layout.slices[0].offset;
	unsigned offset = 0;

	/* We could be more clever here and realize that we could use a
	 * larger width if the size is aligned to something more than a
	 * single page.. or even use a format larger than r8 in those
	 * cases. But for normal sized textures and even up to 16k x 16k
	 * at <= 4byte/pixel, we'll only go thru the loop once
	 */
	const unsigned w = 0x1000;

	/* ubwc size should always be page aligned: */
	assert((size % w) == 0);

	while (size > 0) {
		const unsigned h = MIN2(0x4000, size / w);
		/* width is already aligned to a suitable pitch: */
		const unsigned p = w;

		/*
		 * Emit destination:
		 */
		OUT_PKT4(ring, REG_A6XX_RB_2D_DST_INFO, 9);
		OUT_RING(ring, A6XX_RB_2D_DST_INFO_COLOR_FORMAT(FMT6_8_UNORM) |
				A6XX_RB_2D_DST_INFO_TILE_MODE(TILE6_LINEAR) |
				A6XX_RB_2D_DST_INFO_COLOR_SWAP(WZYX));
		OUT_RELOC(ring, rsc->bo, offset, 0, 0);    /* RB_2D_DST_LO/HI */
		OUT_RING(ring, A6XX_RB_2D_DST_PITCH(p));
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);

		/*
		 * Blit command:
		 */

		OUT_PKT4(ring, REG_A6XX_GRAS_2D_DST_TL, 2);
		OUT_RING(ring, A6XX_GRAS_2D_DST_TL_X(0) | A6XX_GRAS_2D_DST_TL_Y(0));
		OUT_RING(ring, A6XX_GRAS_2D_DST_BR_X(w - 1) | A6XX_GRAS_2D_DST_BR_Y(h - 1));

		OUT_PKT7(ring, CP_EVENT_WRITE, 1);
		OUT_RING(ring, 0x3f);
		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, batch->ctx->screen->info.a6xx.magic.RB_UNKNOWN_8E04_blit);

		OUT_PKT7(ring, CP_BLIT, 1);
		OUT_RING(ring, CP_BLIT_0_OP(BLIT_OP_SCALE));

		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, 0);             /* RB_UNKNOWN_8E04 */

		offset += w * h;
		size -= w * h;
	}

	fd6_event_write(batch, ring, PC_CCU_FLUSH_COLOR_TS, true);
	fd6_event_write(batch, ring, PC_CCU_FLUSH_DEPTH_TS, true);
	fd6_event_write(batch, ring, CACHE_FLUSH_TS, true);
	fd6_cache_inv(batch, ring);
}

static void
emit_blit_dst(struct fd_ringbuffer *ring, struct pipe_resource *prsc, enum pipe_format pfmt, unsigned level, unsigned layer)
{
	struct fd_resource *dst = fd_resource(prsc);
	enum a6xx_format fmt = fd6_pipe2color(pfmt);
	enum a6xx_tile_mode tile = fd_resource_tile_mode(prsc, level);
	enum a3xx_color_swap swap = fd6_resource_swap(dst, pfmt);
	uint32_t pitch = fd_resource_pitch(dst, level);
	bool ubwc_enabled = fd_resource_ubwc_enabled(dst, level);
	unsigned off = fd_resource_offset(dst, level, layer);

	if (fmt == FMT6_Z24_UNORM_S8_UINT)
		fmt = FMT6_Z24_UNORM_S8_UINT_AS_R8G8B8A8;

	OUT_PKT4(ring, REG_A6XX_RB_2D_DST_INFO, 9);
	OUT_RING(ring, A6XX_RB_2D_DST_INFO_COLOR_FORMAT(fmt) |
			A6XX_RB_2D_DST_INFO_TILE_MODE(tile) |
			A6XX_RB_2D_DST_INFO_COLOR_SWAP(swap) |
			COND(util_format_is_srgb(pfmt), A6XX_RB_2D_DST_INFO_SRGB) |
			COND(ubwc_enabled, A6XX_RB_2D_DST_INFO_FLAGS));
	OUT_RELOC(ring, dst->bo, off, 0, 0);    /* RB_2D_DST_LO/HI */
	OUT_RING(ring, A6XX_RB_2D_DST_PITCH(pitch));
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	if (ubwc_enabled) {
		OUT_PKT4(ring, REG_A6XX_RB_2D_DST_FLAGS_LO, 6);
		fd6_emit_flag_reference(ring, dst, level, layer);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
	}
}

static void
emit_blit_src(struct fd_ringbuffer *ring, const struct pipe_blit_info *info, unsigned layer, unsigned nr_samples)
{
	struct fd_resource *src = fd_resource(info->src.resource);
	enum a6xx_format sfmt = fd6_pipe2color(info->src.format);
	enum a6xx_tile_mode	stile = fd_resource_tile_mode(info->src.resource, info->src.level);
	enum a3xx_color_swap sswap = fd6_resource_swap(src, info->src.format);
	uint32_t pitch = fd_resource_pitch(src, info->src.level);
	bool subwc_enabled = fd_resource_ubwc_enabled(src, info->src.level);
	unsigned soff = fd_resource_offset(src, info->src.level, layer);
	uint32_t width = u_minify(src->base.width0, info->src.level) * nr_samples;
	uint32_t height = u_minify(src->base.height0, info->src.level);
	uint32_t filter = 0;

	if (info->filter == PIPE_TEX_FILTER_LINEAR)
		filter = A6XX_SP_PS_2D_SRC_INFO_FILTER;

	enum a3xx_msaa_samples samples = fd_msaa_samples(src->base.nr_samples);

	if (sfmt == FMT6_10_10_10_2_UNORM_DEST)
		sfmt = FMT6_10_10_10_2_UNORM;

	OUT_PKT4(ring, REG_A6XX_SP_PS_2D_SRC_INFO, 10);
	OUT_RING(ring, A6XX_SP_PS_2D_SRC_INFO_COLOR_FORMAT(sfmt) |
			A6XX_SP_PS_2D_SRC_INFO_TILE_MODE(stile) |
			A6XX_SP_PS_2D_SRC_INFO_COLOR_SWAP(sswap) |
			A6XX_SP_PS_2D_SRC_INFO_SAMPLES(samples) |
			COND(samples > MSAA_ONE && (info->mask & PIPE_MASK_RGBA),
					A6XX_SP_PS_2D_SRC_INFO_SAMPLES_AVERAGE) |
			COND(subwc_enabled, A6XX_SP_PS_2D_SRC_INFO_FLAGS) |
			COND(util_format_is_srgb(info->src.format), A6XX_SP_PS_2D_SRC_INFO_SRGB) |
			0x500000 | filter);
	OUT_RING(ring, A6XX_SP_PS_2D_SRC_SIZE_WIDTH(width) |
			A6XX_SP_PS_2D_SRC_SIZE_HEIGHT(height)); /* SP_PS_2D_SRC_SIZE */
	OUT_RELOC(ring, src->bo, soff, 0, 0);    /* SP_PS_2D_SRC_LO/HI */
	OUT_RING(ring, A6XX_SP_PS_2D_SRC_PITCH_PITCH(pitch));

	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	if (subwc_enabled) {
		OUT_PKT4(ring, REG_A6XX_SP_PS_2D_SRC_FLAGS_LO, 6);
		fd6_emit_flag_reference(ring, src, info->src.level, layer);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
	}
}

static void
emit_blit_texture(struct fd_context *ctx,
		struct fd_ringbuffer *ring, const struct pipe_blit_info *info)
{
	const struct pipe_box *sbox = &info->src.box;
	const struct pipe_box *dbox = &info->dst.box;
	struct fd_resource *dst;
	int sx1, sy1, sx2, sy2;
	int dx1, dy1, dx2, dy2;

	if (DEBUG_BLIT) {
		fprintf(stderr, "texture blit: ");
		dump_blit_info(info);
	}

	dst = fd_resource(info->dst.resource);

	uint32_t nr_samples = fd_resource_nr_samples(&dst->base);

	sx1 = sbox->x * nr_samples;
	sy1 = sbox->y;
	sx2 = (sbox->x + sbox->width) * nr_samples - 1;
	sy2 = sbox->y + sbox->height - 1;

	OUT_PKT4(ring, REG_A6XX_GRAS_2D_SRC_TL_X, 4);
	OUT_RING(ring, A6XX_GRAS_2D_SRC_TL_X(sx1));
	OUT_RING(ring, A6XX_GRAS_2D_SRC_BR_X(sx2));
	OUT_RING(ring, A6XX_GRAS_2D_SRC_TL_Y(sy1));
	OUT_RING(ring, A6XX_GRAS_2D_SRC_BR_Y(sy2));

	dx1 = dbox->x * nr_samples;
	dy1 = dbox->y;
	dx2 = (dbox->x + dbox->width) * nr_samples - 1;
	dy2 = dbox->y + dbox->height - 1;

	OUT_PKT4(ring, REG_A6XX_GRAS_2D_DST_TL, 2);
	OUT_RING(ring, A6XX_GRAS_2D_DST_TL_X(dx1) | A6XX_GRAS_2D_DST_TL_Y(dy1));
	OUT_RING(ring, A6XX_GRAS_2D_DST_BR_X(dx2) | A6XX_GRAS_2D_DST_BR_Y(dy2));

	if (info->scissor_enable) {
		OUT_PKT4(ring, REG_A6XX_GRAS_2D_RESOLVE_CNTL_1, 2);
		OUT_RING(ring, A6XX_GRAS_2D_RESOLVE_CNTL_1_X(info->scissor.minx) |
				 A6XX_GRAS_2D_RESOLVE_CNTL_1_Y(info->scissor.miny));
		OUT_RING(ring, A6XX_GRAS_2D_RESOLVE_CNTL_1_X(info->scissor.maxx - 1) |
				 A6XX_GRAS_2D_RESOLVE_CNTL_1_Y(info->scissor.maxy - 1));
	}

	emit_blit_setup(ring, info->dst.format, info->scissor_enable, NULL);

	for (unsigned i = 0; i < info->dst.box.depth; i++) {

		emit_blit_src(ring, info, sbox->z + i, nr_samples);
		emit_blit_dst(ring, info->dst.resource, info->dst.format, info->dst.level, dbox->z + i);

		/*
		 * Blit command:
		 */
		OUT_PKT7(ring, CP_EVENT_WRITE, 1);
		OUT_RING(ring, 0x3f);
		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, ctx->screen->info.a6xx.magic.RB_UNKNOWN_8E04_blit);

		OUT_PKT7(ring, CP_BLIT, 1);
		OUT_RING(ring, CP_BLIT_0_OP(BLIT_OP_SCALE));

		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, 0);             /* RB_UNKNOWN_8E04 */
	}
}

static void
emit_clear_color(struct fd_ringbuffer *ring,
		enum pipe_format pfmt, union pipe_color_union *color)
{
	switch (pfmt) {
	case PIPE_FORMAT_Z24X8_UNORM:
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
	case PIPE_FORMAT_X24S8_UINT: {
		uint32_t depth_unorm24 = color->f[0] * ((1u << 24) - 1);
		uint8_t stencil = color->ui[1];
		color->ui[0] = depth_unorm24 & 0xff;
		color->ui[1] = (depth_unorm24 >> 8) & 0xff;
		color->ui[2] = (depth_unorm24 >> 16) & 0xff;
		color->ui[3] = stencil;
		break;
	}
	default:
		break;
	}

	OUT_PKT4(ring, REG_A6XX_RB_2D_SRC_SOLID_C0, 4);
	switch (fd6_ifmt(fd6_pipe2color(pfmt))) {
	case R2D_UNORM8:
	case R2D_UNORM8_SRGB:
		OUT_RING(ring, float_to_ubyte(color->f[0]));
		OUT_RING(ring, float_to_ubyte(color->f[1]));
		OUT_RING(ring, float_to_ubyte(color->f[2]));
		OUT_RING(ring, float_to_ubyte(color->f[3]));
		break;
	case R2D_FLOAT16:
		OUT_RING(ring, _mesa_float_to_half(color->f[0]));
		OUT_RING(ring, _mesa_float_to_half(color->f[1]));
		OUT_RING(ring, _mesa_float_to_half(color->f[2]));
		OUT_RING(ring, _mesa_float_to_half(color->f[3]));
		break;
	case R2D_FLOAT32:
	case R2D_INT32:
	case R2D_INT16:
	case R2D_INT8:
	default:
		OUT_RING(ring, color->ui[0]);
		OUT_RING(ring, color->ui[1]);
		OUT_RING(ring, color->ui[2]);
		OUT_RING(ring, color->ui[3]);
		break;
	}
}

void
fd6_clear_surface(struct fd_context *ctx,
		struct fd_ringbuffer *ring, struct pipe_surface *psurf,
		uint32_t width, uint32_t height, union pipe_color_union *color)
{
	if (DEBUG_BLIT) {
		fprintf(stderr, "surface clear:\ndst resource: ");
		util_dump_resource(stderr, psurf->texture);
		fprintf(stderr, "\n");
	}

	uint32_t nr_samples = fd_resource_nr_samples(psurf->texture);
	OUT_PKT4(ring, REG_A6XX_GRAS_2D_DST_TL, 2);
	OUT_RING(ring, A6XX_GRAS_2D_DST_TL_X(0) | A6XX_GRAS_2D_DST_TL_Y(0));
	OUT_RING(ring, A6XX_GRAS_2D_DST_BR_X(width * nr_samples - 1) |
			A6XX_GRAS_2D_DST_BR_Y(height - 1));

	emit_clear_color(ring, psurf->format, color);
	emit_blit_setup(ring, psurf->format, false, color);

	for (unsigned i = psurf->u.tex.first_layer; i <= psurf->u.tex.last_layer; i++) {
		emit_blit_dst(ring, psurf->texture, psurf->format, psurf->u.tex.level, i);

		/*
		 * Blit command:
		 */
		OUT_PKT7(ring, CP_EVENT_WRITE, 1);
		OUT_RING(ring, 0x3f);
		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, ctx->screen->info.a6xx.magic.RB_UNKNOWN_8E04_blit);

		OUT_PKT7(ring, CP_BLIT, 1);
		OUT_RING(ring, CP_BLIT_0_OP(BLIT_OP_SCALE));

		OUT_WFI5(ring);

		OUT_PKT4(ring, REG_A6XX_RB_UNKNOWN_8E04, 1);
		OUT_RING(ring, 0);             /* RB_UNKNOWN_8E04 */
	}
}

static bool
handle_rgba_blit(struct fd_context *ctx, const struct pipe_blit_info *info)
{
	struct fd_batch *batch;

	debug_assert(!(info->mask & PIPE_MASK_ZS));

	if (!can_do_blit(info))
		return false;

	batch = fd_bc_alloc_batch(&ctx->screen->batch_cache, ctx, true);

	fd_screen_lock(ctx->screen);

	fd_batch_resource_read(batch, fd_resource(info->src.resource));
	fd_batch_resource_write(batch, fd_resource(info->dst.resource));

	fd_screen_unlock(ctx->screen);

	/* Clearing last_fence must come after the batch dependency tracking
	 * (resource_read()/resource_write()), as that can trigger a flush,
	 * re-populating last_fence
	 */
	fd_fence_ref(&ctx->last_fence, NULL);

	fd_batch_set_stage(batch, FD_STAGE_BLIT);

	fd_log_stream(batch, stream, util_dump_blit_info(stream, info));

	emit_setup(batch);

	if ((info->src.resource->target == PIPE_BUFFER) &&
			(info->dst.resource->target == PIPE_BUFFER)) {
		assert(fd_resource(info->src.resource)->layout.tile_mode == TILE6_LINEAR);
		assert(fd_resource(info->dst.resource)->layout.tile_mode == TILE6_LINEAR);
		fd_log(batch, "START BLIT (BUFFER)");
		emit_blit_buffer(ctx, batch->draw, info);
		fd_log(batch, "END BLIT (BUFFER)");
	} else {
		/* I don't *think* we need to handle blits between buffer <-> !buffer */
		debug_assert(info->src.resource->target != PIPE_BUFFER);
		debug_assert(info->dst.resource->target != PIPE_BUFFER);
		fd_log(batch, "START BLIT (TEXTURE)");
		emit_blit_texture(ctx, batch->draw, info);
		fd_log(batch, "END BLIT (TEXTURE)");
	}

	fd6_event_write(batch, batch->draw, PC_CCU_FLUSH_COLOR_TS, true);
	fd6_event_write(batch, batch->draw, PC_CCU_FLUSH_DEPTH_TS, true);
	fd6_event_write(batch, batch->draw, CACHE_FLUSH_TS, true);
	fd6_cache_inv(batch, batch->draw);

	fd_resource(info->dst.resource)->valid = true;
	batch->needs_flush = true;

	fd_batch_flush(batch);
	fd_batch_reference(&batch, NULL);

	return true;
}

/**
 * Re-written z/s blits can still fail for various reasons (for example MSAA).
 * But we want to do the fallback blit with the re-written pipe_blit_info,
 * in particular as u_blitter cannot blit stencil.  So handle the fallback
 * ourself and never "fail".
 */
static bool
do_rewritten_blit(struct fd_context *ctx, const struct pipe_blit_info *info)
{
	bool success = handle_rgba_blit(ctx, info);
	if (!success)
		success = fd_blitter_blit(ctx, info);
	debug_assert(success);  /* fallback should never fail! */
	return success;
}

/**
 * Handle depth/stencil blits either via u_blitter and/or re-writing the
 * blit into an equivilant format that we can handle
 */
static bool
handle_zs_blit(struct fd_context *ctx, const struct pipe_blit_info *info)
{
	struct pipe_blit_info blit = *info;

	if (DEBUG_BLIT) {
		fprintf(stderr, "---- handle_zs_blit: ");
		dump_blit_info(info);
	}

	switch (info->dst.format) {
	case PIPE_FORMAT_S8_UINT:
		debug_assert(info->mask == PIPE_MASK_S);
		blit.mask = PIPE_MASK_R;
		blit.src.format = PIPE_FORMAT_R8_UINT;
		blit.dst.format = PIPE_FORMAT_R8_UINT;
		return do_rewritten_blit(ctx, &blit);

	case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
		if (info->mask & PIPE_MASK_Z) {
			blit.mask = PIPE_MASK_R;
			blit.src.format = PIPE_FORMAT_R32_FLOAT;
			blit.dst.format = PIPE_FORMAT_R32_FLOAT;
			do_rewritten_blit(ctx, &blit);
		}

		if (info->mask & PIPE_MASK_S) {
			blit.mask = PIPE_MASK_R;
			blit.src.format = PIPE_FORMAT_R8_UINT;
			blit.dst.format = PIPE_FORMAT_R8_UINT;
			blit.src.resource = &fd_resource(info->src.resource)->stencil->base;
			blit.dst.resource = &fd_resource(info->dst.resource)->stencil->base;
			do_rewritten_blit(ctx, &blit);
		}

		return true;

	case PIPE_FORMAT_Z16_UNORM:
		blit.mask = PIPE_MASK_R;
		blit.src.format = PIPE_FORMAT_R16_UNORM;
		blit.dst.format = PIPE_FORMAT_R16_UNORM;
		return do_rewritten_blit(ctx, &blit);

	case PIPE_FORMAT_Z32_UNORM:
	case PIPE_FORMAT_Z32_FLOAT:
		debug_assert(info->mask == PIPE_MASK_Z);
		blit.mask = PIPE_MASK_R;
		blit.src.format = PIPE_FORMAT_R32_UINT;
		blit.dst.format = PIPE_FORMAT_R32_UINT;
		return do_rewritten_blit(ctx, &blit);

	case PIPE_FORMAT_Z24X8_UNORM:
	case PIPE_FORMAT_Z24_UNORM_S8_UINT:
		blit.mask = 0;
		if (info->mask & PIPE_MASK_Z)
			blit.mask |= PIPE_MASK_R | PIPE_MASK_G | PIPE_MASK_B;
		if (info->mask & PIPE_MASK_S)
			blit.mask |= PIPE_MASK_A;
		blit.src.format = PIPE_FORMAT_Z24_UNORM_S8_UINT_AS_R8G8B8A8;
		blit.dst.format = PIPE_FORMAT_Z24_UNORM_S8_UINT_AS_R8G8B8A8;
		return fd_blitter_blit(ctx, &blit);

	default:
		return false;
	}
}

static bool
handle_compressed_blit(struct fd_context *ctx, const struct pipe_blit_info *info)
{
	struct pipe_blit_info blit = *info;

	if (DEBUG_BLIT) {
		fprintf(stderr, "---- handle_compressed_blit: ");
		dump_blit_info(info);
	}

	if (info->src.format != info->dst.format)
		return fd_blitter_blit(ctx, info);

	if (util_format_get_blocksize(info->src.format) == 8) {
		blit.src.format = blit.dst.format = PIPE_FORMAT_R16G16B16A16_UINT;
	} else {
		debug_assert(util_format_get_blocksize(info->src.format) == 16);
		blit.src.format = blit.dst.format = PIPE_FORMAT_R32G32B32A32_UINT;
	}

	int bw = util_format_get_blockwidth(info->src.format);
	int bh = util_format_get_blockheight(info->src.format);

	/* NOTE: x/y *must* be aligned to block boundary (ie. in
	 * glCompressedTexSubImage2D()) but width/height may not
	 * be:
	 */

	debug_assert((blit.src.box.x % bw) == 0);
	debug_assert((blit.src.box.y % bh) == 0);

	blit.src.box.x /= bw;
	blit.src.box.y /= bh;
	blit.src.box.width  = DIV_ROUND_UP(blit.src.box.width, bw);
	blit.src.box.height = DIV_ROUND_UP(blit.src.box.height, bh);

	debug_assert((blit.dst.box.x % bw) == 0);
	debug_assert((blit.dst.box.y % bh) == 0);

	blit.dst.box.x /= bw;
	blit.dst.box.y /= bh;
	blit.dst.box.width  = DIV_ROUND_UP(blit.dst.box.width, bw);
	blit.dst.box.height = DIV_ROUND_UP(blit.dst.box.height, bh);

	return do_rewritten_blit(ctx, &blit);
}

static bool
fd6_blit(struct fd_context *ctx, const struct pipe_blit_info *info)
{
	if (info->mask & PIPE_MASK_ZS)
		return handle_zs_blit(ctx, info);
	if (util_format_is_compressed(info->src.format) ||
			util_format_is_compressed(info->dst.format))
		return handle_compressed_blit(ctx, info);

	return handle_rgba_blit(ctx, info);
}

void
fd6_blitter_init(struct pipe_context *pctx)
{
	fd_context(pctx)->clear_ubwc = fd6_clear_ubwc;

	if (fd_mesa_debug & FD_DBG_NOBLIT)
		return;

	fd_context(pctx)->blit = fd6_blit;
}

unsigned
fd6_tile_mode(const struct pipe_resource *tmpl)
{
	/* if the mipmap level 0 is still too small to be tiled, then don't
	 * bother pretending:
	 */
	if (fd_resource_level_linear(tmpl, 0))
		return TILE6_LINEAR;

	/* basically just has to be a format we can blit, so uploads/downloads
	 * via linear staging buffer works:
	 */
	if (ok_format(tmpl->format))
		return TILE6_3;

	return TILE6_LINEAR;
}
