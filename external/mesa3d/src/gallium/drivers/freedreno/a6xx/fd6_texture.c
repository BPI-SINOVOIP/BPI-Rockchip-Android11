/*
 * Copyright (C) 2016 Rob Clark <robclark@freedesktop.org>
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

#include "pipe/p_state.h"
#include "util/u_string.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/format/u_format.h"
#include "util/hash_table.h"

#include "fd6_texture.h"
#include "fd6_resource.h"
#include "fd6_format.h"
#include "fd6_emit.h"

static void fd6_texture_state_destroy(struct fd6_texture_state *state);

static enum a6xx_tex_clamp
tex_clamp(unsigned wrap, bool clamp_to_edge, bool *needs_border)
{
	/* Hardware does not support _CLAMP, but we emulate it: */
	if (wrap == PIPE_TEX_WRAP_CLAMP) {
		wrap = (clamp_to_edge) ?
			PIPE_TEX_WRAP_CLAMP_TO_EDGE : PIPE_TEX_WRAP_CLAMP_TO_BORDER;
	}

	switch (wrap) {
	case PIPE_TEX_WRAP_REPEAT:
		return A6XX_TEX_REPEAT;
	case PIPE_TEX_WRAP_CLAMP_TO_EDGE:
		return A6XX_TEX_CLAMP_TO_EDGE;
	case PIPE_TEX_WRAP_CLAMP_TO_BORDER:
		*needs_border = true;
		return A6XX_TEX_CLAMP_TO_BORDER;
	case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE:
		/* only works for PoT.. need to emulate otherwise! */
		return A6XX_TEX_MIRROR_CLAMP;
	case PIPE_TEX_WRAP_MIRROR_REPEAT:
		return A6XX_TEX_MIRROR_REPEAT;
	case PIPE_TEX_WRAP_MIRROR_CLAMP:
	case PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER:
		/* these two we could perhaps emulate, but we currently
		 * just don't advertise PIPE_CAP_TEXTURE_MIRROR_CLAMP
		 */
	default:
		DBG("invalid wrap: %u", wrap);
		return 0;
	}
}

static enum a6xx_tex_filter
tex_filter(unsigned filter, bool aniso)
{
	switch (filter) {
	case PIPE_TEX_FILTER_NEAREST:
		return A6XX_TEX_NEAREST;
	case PIPE_TEX_FILTER_LINEAR:
		return aniso ? A6XX_TEX_ANISO : A6XX_TEX_LINEAR;
	default:
		DBG("invalid filter: %u", filter);
		return 0;
	}
}

static void *
fd6_sampler_state_create(struct pipe_context *pctx,
		const struct pipe_sampler_state *cso)
{
	struct fd6_sampler_stateobj *so = CALLOC_STRUCT(fd6_sampler_stateobj);
	unsigned aniso = util_last_bit(MIN2(cso->max_anisotropy >> 1, 8));
	bool miplinear = false;
	bool clamp_to_edge;

	if (!so)
		return NULL;

	so->base = *cso;
	so->seqno = ++fd6_context(fd_context(pctx))->tex_seqno;

	if (cso->min_mip_filter == PIPE_TEX_MIPFILTER_LINEAR)
		miplinear = true;

	/*
	 * For nearest filtering, _CLAMP means _CLAMP_TO_EDGE;  for linear
	 * filtering, _CLAMP means _CLAMP_TO_BORDER while additionally
	 * clamping the texture coordinates to [0.0, 1.0].
	 *
	 * The clamping will be taken care of in the shaders.  There are two
	 * filters here, but let the minification one has a say.
	 */
	clamp_to_edge = (cso->min_img_filter == PIPE_TEX_FILTER_NEAREST);
	if (!clamp_to_edge) {
		so->saturate_s = (cso->wrap_s == PIPE_TEX_WRAP_CLAMP);
		so->saturate_t = (cso->wrap_t == PIPE_TEX_WRAP_CLAMP);
		so->saturate_r = (cso->wrap_r == PIPE_TEX_WRAP_CLAMP);
	}

	so->needs_border = false;
	so->texsamp0 =
		COND(miplinear, A6XX_TEX_SAMP_0_MIPFILTER_LINEAR_NEAR) |
		A6XX_TEX_SAMP_0_XY_MAG(tex_filter(cso->mag_img_filter, aniso)) |
		A6XX_TEX_SAMP_0_XY_MIN(tex_filter(cso->min_img_filter, aniso)) |
		A6XX_TEX_SAMP_0_ANISO(aniso) |
		A6XX_TEX_SAMP_0_WRAP_S(tex_clamp(cso->wrap_s, clamp_to_edge, &so->needs_border)) |
		A6XX_TEX_SAMP_0_WRAP_T(tex_clamp(cso->wrap_t, clamp_to_edge, &so->needs_border)) |
		A6XX_TEX_SAMP_0_WRAP_R(tex_clamp(cso->wrap_r, clamp_to_edge, &so->needs_border));

	so->texsamp1 =
		COND(cso->min_mip_filter == PIPE_TEX_MIPFILTER_NONE,
				A6XX_TEX_SAMP_1_MIPFILTER_LINEAR_FAR) |
		COND(!cso->seamless_cube_map, A6XX_TEX_SAMP_1_CUBEMAPSEAMLESSFILTOFF) |
		COND(!cso->normalized_coords, A6XX_TEX_SAMP_1_UNNORM_COORDS);

	so->texsamp0 |= A6XX_TEX_SAMP_0_LOD_BIAS(cso->lod_bias);
	so->texsamp1 |=
		A6XX_TEX_SAMP_1_MIN_LOD(cso->min_lod) |
		A6XX_TEX_SAMP_1_MAX_LOD(cso->max_lod);

	if (cso->compare_mode)
		so->texsamp1 |= A6XX_TEX_SAMP_1_COMPARE_FUNC(cso->compare_func); /* maps 1:1 */

	return so;
}

static void
fd6_sampler_state_delete(struct pipe_context *pctx, void *hwcso)
{
	struct fd6_context *fd6_ctx = fd6_context(fd_context(pctx));
	struct fd6_sampler_stateobj *samp = hwcso;

	hash_table_foreach(fd6_ctx->tex_cache, entry) {
		struct fd6_texture_state *state = entry->data;

		for (unsigned i = 0; i < ARRAY_SIZE(state->key.samp); i++) {
			if (samp->seqno == state->key.samp[i].seqno) {
				fd6_texture_state_destroy(entry->data);
				_mesa_hash_table_remove(fd6_ctx->tex_cache, entry);
				break;
			}
		}
	}

	free(hwcso);
}

static void
fd6_sampler_states_bind(struct pipe_context *pctx,
		enum pipe_shader_type shader, unsigned start,
		unsigned nr, void **hwcso)
{
	struct fd_context *ctx = fd_context(pctx);
	struct fd6_context *fd6_ctx = fd6_context(ctx);
	uint16_t saturate_s = 0, saturate_t = 0, saturate_r = 0;
	unsigned i;

	if (!hwcso)
		nr = 0;

	for (i = 0; i < nr; i++) {
		if (hwcso[i]) {
			struct fd6_sampler_stateobj *sampler =
					fd6_sampler_stateobj(hwcso[i]);
			if (sampler->saturate_s)
				saturate_s |= (1 << i);
			if (sampler->saturate_t)
				saturate_t |= (1 << i);
			if (sampler->saturate_r)
				saturate_r |= (1 << i);
		}
	}

	fd_sampler_states_bind(pctx, shader, start, nr, hwcso);

	if (shader == PIPE_SHADER_FRAGMENT) {
		fd6_ctx->fsaturate =
			(saturate_s != 0) ||
			(saturate_t != 0) ||
			(saturate_r != 0);
		fd6_ctx->fsaturate_s = saturate_s;
		fd6_ctx->fsaturate_t = saturate_t;
		fd6_ctx->fsaturate_r = saturate_r;
	} else if (shader == PIPE_SHADER_VERTEX) {
		fd6_ctx->vsaturate =
			(saturate_s != 0) ||
			(saturate_t != 0) ||
			(saturate_r != 0);
		fd6_ctx->vsaturate_s = saturate_s;
		fd6_ctx->vsaturate_t = saturate_t;
		fd6_ctx->vsaturate_r = saturate_r;
	}
}

static struct pipe_sampler_view *
fd6_sampler_view_create(struct pipe_context *pctx, struct pipe_resource *prsc,
		const struct pipe_sampler_view *cso)
{
	struct fd6_pipe_sampler_view *so = CALLOC_STRUCT(fd6_pipe_sampler_view);
	struct fd_resource *rsc = fd_resource(prsc);
	enum pipe_format format = cso->format;
	bool ubwc_enabled = false;
	unsigned lvl, layers = 0;

	if (!so)
		return NULL;

	fd6_validate_format(fd_context(pctx), rsc, format);

	if (format == PIPE_FORMAT_X32_S8X24_UINT) {
		rsc = rsc->stencil;
		format = rsc->base.format;
	}

	so->base = *cso;
	pipe_reference(NULL, &prsc->reference);
	so->base.texture = prsc;
	so->base.reference.count = 1;
	so->base.context = pctx;
	so->seqno = ++fd6_context(fd_context(pctx))->tex_seqno;
	so->ptr1 = rsc;

	if (cso->target == PIPE_BUFFER) {
		unsigned elements = cso->u.buf.size / util_format_get_blocksize(format);

		lvl = 0;
		so->texconst1 =
			A6XX_TEX_CONST_1_WIDTH(elements & MASK(15)) |
			A6XX_TEX_CONST_1_HEIGHT(elements >> 15);
		so->texconst2 =
			A6XX_TEX_CONST_2_UNK4 |
			A6XX_TEX_CONST_2_UNK31;
		so->offset1 = cso->u.buf.offset;
	} else {
		unsigned miplevels;

		lvl = fd_sampler_first_level(cso);
		miplevels = fd_sampler_last_level(cso) - lvl;
		layers = cso->u.tex.last_layer - cso->u.tex.first_layer + 1;

		so->texconst0 |= A6XX_TEX_CONST_0_MIPLVLS(miplevels);
		so->texconst1 =
			A6XX_TEX_CONST_1_WIDTH(u_minify(prsc->width0, lvl)) |
			A6XX_TEX_CONST_1_HEIGHT(u_minify(prsc->height0, lvl));
		so->texconst2 =
			A6XX_TEX_CONST_2_PITCHALIGN(rsc->layout.pitchalign - 6) |
			A6XX_TEX_CONST_2_PITCH(fd_resource_pitch(rsc, lvl));

		ubwc_enabled = fd_resource_ubwc_enabled(rsc, lvl);

		if (rsc->base.format == PIPE_FORMAT_R8_G8B8_420_UNORM) {
			struct fd_resource *next = fd_resource(rsc->base.next);

			/* In case of biplanar R8_G8B8, the UBWC metadata address in
			 * dwords 7 and 8, is instead the pointer to the second plane.
			 */
			so->ptr2 = next;
			so->texconst6 =
				A6XX_TEX_CONST_6_PLANE_PITCH(fd_resource_pitch(next, lvl));

			if (ubwc_enabled) {
				/* Further, if using UBWC with R8_G8B8, we only point to the
				 * UBWC header and the color data is expected to follow immediately.
				 */
				so->offset1 =
					fd_resource_ubwc_offset(rsc, lvl, cso->u.tex.first_layer);
				so->offset2 =
					fd_resource_ubwc_offset(next, lvl, cso->u.tex.first_layer);
			} else {
				so->offset1 = fd_resource_offset(rsc, lvl, cso->u.tex.first_layer);
				so->offset2 = fd_resource_offset(next, lvl, cso->u.tex.first_layer);
			}
		} else {
			so->offset1 = fd_resource_offset(rsc, lvl, cso->u.tex.first_layer);
			if (ubwc_enabled) {
				so->ptr2 = rsc;
				so->offset2 = fd_resource_ubwc_offset(rsc, lvl, cso->u.tex.first_layer);
			}
		}
	}

	so->texconst0 |= fd6_tex_const_0(prsc, lvl, cso->format,
				cso->swizzle_r, cso->swizzle_g,
				cso->swizzle_b, cso->swizzle_a);

	so->texconst2 |= A6XX_TEX_CONST_2_TYPE(fd6_tex_type(cso->target));

	switch (cso->target) {
	case PIPE_TEXTURE_RECT:
	case PIPE_TEXTURE_1D:
	case PIPE_TEXTURE_2D:
		so->texconst3 =
			A6XX_TEX_CONST_3_ARRAY_PITCH(rsc->layout.layer_size);
		so->texconst5 =
			A6XX_TEX_CONST_5_DEPTH(1);
		break;
	case PIPE_TEXTURE_1D_ARRAY:
	case PIPE_TEXTURE_2D_ARRAY:
		so->texconst3 =
			A6XX_TEX_CONST_3_ARRAY_PITCH(rsc->layout.layer_size);
		so->texconst5 =
			A6XX_TEX_CONST_5_DEPTH(layers);
		break;
	case PIPE_TEXTURE_CUBE:
	case PIPE_TEXTURE_CUBE_ARRAY:
		so->texconst3 =
			A6XX_TEX_CONST_3_ARRAY_PITCH(rsc->layout.layer_size);
		so->texconst5 =
			A6XX_TEX_CONST_5_DEPTH(layers / 6);
		break;
	case PIPE_TEXTURE_3D:
		so->texconst3 =
			A6XX_TEX_CONST_3_MIN_LAYERSZ(
				fd_resource_slice(rsc, prsc->last_level)->size0) |
			A6XX_TEX_CONST_3_ARRAY_PITCH(fd_resource_slice(rsc, lvl)->size0);
		so->texconst5 =
			A6XX_TEX_CONST_5_DEPTH(u_minify(prsc->depth0, lvl));
		break;
	default:
		break;
	}

	if (rsc->layout.tile_all)
		so->texconst3 |= A6XX_TEX_CONST_3_TILE_ALL;

	if (ubwc_enabled) {
		uint32_t block_width, block_height;
		fdl6_get_ubwc_blockwidth(&rsc->layout, &block_width, &block_height);

		so->texconst3 |= A6XX_TEX_CONST_3_FLAG;
		so->texconst9 |= A6XX_TEX_CONST_9_FLAG_BUFFER_ARRAY_PITCH(rsc->layout.ubwc_layer_size >> 2);
		so->texconst10 |=
			A6XX_TEX_CONST_10_FLAG_BUFFER_PITCH(fdl_ubwc_pitch(&rsc->layout, lvl)) |
			A6XX_TEX_CONST_10_FLAG_BUFFER_LOGW(util_logbase2_ceil(DIV_ROUND_UP(u_minify(prsc->width0, lvl), block_width))) |
			A6XX_TEX_CONST_10_FLAG_BUFFER_LOGH(util_logbase2_ceil(DIV_ROUND_UP(u_minify(prsc->height0, lvl), block_height)));
	}

	return &so->base;
}

static void
fd6_sampler_view_destroy(struct pipe_context *pctx,
		struct pipe_sampler_view *_view)
{
	struct fd6_context *fd6_ctx = fd6_context(fd_context(pctx));
	struct fd6_pipe_sampler_view *view = fd6_pipe_sampler_view(_view);

	hash_table_foreach(fd6_ctx->tex_cache, entry) {
		struct fd6_texture_state *state = entry->data;

		for (unsigned i = 0; i < ARRAY_SIZE(state->key.view); i++) {
			if (view->seqno == state->key.view[i].seqno) {
				fd6_texture_state_destroy(entry->data);
				_mesa_hash_table_remove(fd6_ctx->tex_cache, entry);
				break;
			}
		}
	}

	pipe_resource_reference(&view->base.texture, NULL);

	free(view);
}


static uint32_t
key_hash(const void *_key)
{
	const struct fd6_texture_key *key = _key;
	return XXH32(key, sizeof(*key), 0);
}

static bool
key_equals(const void *_a, const void *_b)
{
	const struct fd6_texture_key *a = _a;
	const struct fd6_texture_key *b = _b;
	return memcmp(a, b, sizeof(struct fd6_texture_key)) == 0;
}

struct fd6_texture_state *
fd6_texture_state(struct fd_context *ctx, enum pipe_shader_type type,
		struct fd_texture_stateobj *tex)
{
	struct fd6_context *fd6_ctx = fd6_context(ctx);
	struct fd6_texture_key key;
	bool needs_border = false;

	memset(&key, 0, sizeof(key));

	for (unsigned i = 0; i < tex->num_textures; i++) {
		if (!tex->textures[i])
			continue;

		struct fd6_pipe_sampler_view *view =
			fd6_pipe_sampler_view(tex->textures[i]);

		key.view[i].rsc_seqno = fd_resource(view->base.texture)->seqno;
		key.view[i].seqno = view->seqno;
	}

	for (unsigned i = 0; i < tex->num_samplers; i++) {
		if (!tex->samplers[i])
			continue;

		struct fd6_sampler_stateobj *sampler =
			fd6_sampler_stateobj(tex->samplers[i]);

		key.samp[i].seqno = sampler->seqno;

		needs_border |= sampler->needs_border;
	}

	key.type = type;
	key.bcolor_offset = fd6_border_color_offset(ctx, type, tex);

	uint32_t hash = key_hash(&key);
	struct hash_entry *entry =
		_mesa_hash_table_search_pre_hashed(fd6_ctx->tex_cache, hash, &key);

	if (entry) {
		return entry->data;
	}

	struct fd6_texture_state *state = CALLOC_STRUCT(fd6_texture_state);

	state->key = key;
	state->stateobj = fd_ringbuffer_new_object(ctx->pipe, 0x1000);
	state->needs_border = needs_border;

	fd6_emit_textures(ctx->pipe, state->stateobj, type, tex, key.bcolor_offset,
			NULL, NULL);

	/* NOTE: uses copy of key in state obj, because pointer passed by caller
	 * is probably on the stack
	 */
	_mesa_hash_table_insert_pre_hashed(fd6_ctx->tex_cache, hash,
			&state->key, state);

	return state;
}

static void
fd6_texture_state_destroy(struct fd6_texture_state *state)
{
	fd_ringbuffer_del(state->stateobj);
	free(state);
}

static void
fd6_rebind_resource(struct fd_context *ctx, struct fd_resource *rsc)
{
	if (!(rsc->dirty & FD_DIRTY_TEX))
		return;

	struct fd6_context *fd6_ctx = fd6_context(ctx);

	hash_table_foreach (fd6_ctx->tex_cache, entry) {
		struct fd6_texture_state *state = entry->data;

		for (unsigned i = 0; i < ARRAY_SIZE(state->key.view); i++) {
			if (rsc->seqno == state->key.view[i].rsc_seqno) {
				fd6_texture_state_destroy(entry->data);
				_mesa_hash_table_remove(fd6_ctx->tex_cache, entry);
				break;
			}
		}
	}
}

void
fd6_texture_init(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);
	struct fd6_context *fd6_ctx = fd6_context(ctx);

	pctx->create_sampler_state = fd6_sampler_state_create;
	pctx->delete_sampler_state = fd6_sampler_state_delete;
	pctx->bind_sampler_states = fd6_sampler_states_bind;

	pctx->create_sampler_view = fd6_sampler_view_create;
	pctx->sampler_view_destroy = fd6_sampler_view_destroy;
	pctx->set_sampler_views = fd_set_sampler_views;

	ctx->rebind_resource = fd6_rebind_resource;

	fd6_ctx->tex_cache = _mesa_hash_table_create(NULL, key_hash, key_equals);
}

void
fd6_texture_fini(struct pipe_context *pctx)
{
	struct fd6_context *fd6_ctx = fd6_context(fd_context(pctx));

	hash_table_foreach(fd6_ctx->tex_cache, entry) {
		fd6_texture_state_destroy(entry->data);
	}
	ralloc_free(fd6_ctx->tex_cache);
}
