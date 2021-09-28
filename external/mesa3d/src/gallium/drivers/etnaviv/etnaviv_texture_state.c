/*
 * Copyright (c) 2012-2015 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Wladimir J. van der Laan <laanwj@gmail.com>
 */

#include "etnaviv_texture_state.h"

#include "hw/common.xml.h"

#include "etnaviv_clear_blit.h"
#include "etnaviv_context.h"
#include "etnaviv_emit.h"
#include "etnaviv_format.h"
#include "etnaviv_texture.h"
#include "etnaviv_translate.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"

#include "drm-uapi/drm_fourcc.h"

struct etna_sampler_state {
   struct pipe_sampler_state base;

   /* sampler offset +4*sampler, interleave when committing state */
   uint32_t TE_SAMPLER_CONFIG0;
   uint32_t TE_SAMPLER_CONFIG1;
   uint32_t TE_SAMPLER_LOD_CONFIG;
   uint32_t TE_SAMPLER_3D_CONFIG;
   uint32_t NTE_SAMPLER_BASELOD;
   unsigned min_lod, max_lod, max_lod_min;
};

static inline struct etna_sampler_state *
etna_sampler_state(struct pipe_sampler_state *samp)
{
   return (struct etna_sampler_state *)samp;
}

struct etna_sampler_view {
   struct pipe_sampler_view base;

   /* sampler offset +4*sampler, interleave when committing state */
   uint32_t TE_SAMPLER_CONFIG0;
   uint32_t TE_SAMPLER_CONFIG0_MASK;
   uint32_t TE_SAMPLER_CONFIG1;
   uint32_t TE_SAMPLER_3D_CONFIG;
   uint32_t TE_SAMPLER_SIZE;
   uint32_t TE_SAMPLER_LOG_SIZE;
   uint32_t TE_SAMPLER_ASTC0;
   uint32_t TE_SAMPLER_LINEAR_STRIDE;  /* only LOD0 */
   struct etna_reloc TE_SAMPLER_LOD_ADDR[VIVS_TE_SAMPLER_LOD_ADDR__LEN];
   unsigned min_lod, max_lod; /* 5.5 fixp */

   struct etna_sampler_ts ts;
};

static inline struct etna_sampler_view *
etna_sampler_view(struct pipe_sampler_view *view)
{
   return (struct etna_sampler_view *)view;
}

static void *
etna_create_sampler_state_state(struct pipe_context *pipe,
                          const struct pipe_sampler_state *ss)
{
   struct etna_sampler_state *cs = CALLOC_STRUCT(etna_sampler_state);
   struct etna_context *ctx = etna_context(pipe);
   struct etna_screen *screen = ctx->screen;
   const bool ansio = ss->max_anisotropy > 1;

   if (!cs)
      return NULL;

   cs->base = *ss;

   cs->TE_SAMPLER_CONFIG0 =
      VIVS_TE_SAMPLER_CONFIG0_UWRAP(translate_texture_wrapmode(ss->wrap_s)) |
      VIVS_TE_SAMPLER_CONFIG0_VWRAP(translate_texture_wrapmode(ss->wrap_t)) |
      VIVS_TE_SAMPLER_CONFIG0_MIN(translate_texture_filter(ss->min_img_filter)) |
      VIVS_TE_SAMPLER_CONFIG0_MIP(translate_texture_mipfilter(ss->min_mip_filter)) |
      VIVS_TE_SAMPLER_CONFIG0_MAG(translate_texture_filter(ss->mag_img_filter)) |
      VIVS_TE_SAMPLER_CONFIG0_ANISOTROPY(COND(ansio, etna_log2_fixp55(ss->max_anisotropy)));

   /* ROUND_UV improves precision - but not compatible with NEAREST filter */
   if (ss->min_img_filter != PIPE_TEX_FILTER_NEAREST &&
       ss->mag_img_filter != PIPE_TEX_FILTER_NEAREST) {
      cs->TE_SAMPLER_CONFIG0 |= VIVS_TE_SAMPLER_CONFIG0_ROUND_UV;
   }

   cs->TE_SAMPLER_CONFIG1 = screen->specs.seamless_cube_map ?
      COND(ss->seamless_cube_map, VIVS_TE_SAMPLER_CONFIG1_SEAMLESS_CUBE_MAP) : 0;

   cs->TE_SAMPLER_LOD_CONFIG =
      COND(ss->lod_bias != 0.0, VIVS_TE_SAMPLER_LOD_CONFIG_BIAS_ENABLE) |
      VIVS_TE_SAMPLER_LOD_CONFIG_BIAS(etna_float_to_fixp55(ss->lod_bias));

   cs->TE_SAMPLER_3D_CONFIG =
      VIVS_TE_SAMPLER_3D_CONFIG_WRAP(translate_texture_wrapmode(ss->wrap_r));

   if (ss->min_mip_filter != PIPE_TEX_MIPFILTER_NONE) {
      cs->min_lod = etna_float_to_fixp55(ss->min_lod);
      cs->max_lod = etna_float_to_fixp55(ss->max_lod);
   } else {
      /* when not mipmapping, we need to set max/min lod so that always
       * lowest LOD is selected */
      cs->min_lod = cs->max_lod = etna_float_to_fixp55(ss->min_lod);
   }

   /* if max_lod is 0, MIN filter will never be used (GC3000)
    * when min filter is different from mag filter, we need HW to compute LOD
    * the workaround is to set max_lod to at least 1
    */
   cs->max_lod_min = (ss->min_img_filter != ss->mag_img_filter) ? 1 : 0;

   cs->NTE_SAMPLER_BASELOD =
      COND(ss->compare_mode, VIVS_NTE_SAMPLER_BASELOD_COMPARE_ENABLE) |
      VIVS_NTE_SAMPLER_BASELOD_COMPARE_FUNC(translate_texture_compare(ss->compare_func));

   return cs;
}

static void
etna_delete_sampler_state_state(struct pipe_context *pctx, void *ss)
{
   FREE(ss);
}

static struct pipe_sampler_view *
etna_create_sampler_view_state(struct pipe_context *pctx, struct pipe_resource *prsc,
                         const struct pipe_sampler_view *so)
{
   struct etna_sampler_view *sv = CALLOC_STRUCT(etna_sampler_view);
   struct etna_context *ctx = etna_context(pctx);
   struct etna_screen *screen = ctx->screen;
   const uint32_t format = translate_texture_format(so->format);
   const bool ext = !!(format & EXT_FORMAT);
   const bool astc = !!(format & ASTC_FORMAT);
   const bool srgb = util_format_is_srgb(so->format);
   const uint32_t swiz = get_texture_swiz(so->format, so->swizzle_r,
                                          so->swizzle_g, so->swizzle_b,
                                          so->swizzle_a);

   if (!sv)
      return NULL;

   struct etna_resource *res = etna_texture_handle_incompatible(pctx, prsc);
   if (!res) {
      free(sv);
      return NULL;
   }

   sv->base = *so;
   pipe_reference_init(&sv->base.reference, 1);
   sv->base.texture = NULL;
   pipe_resource_reference(&sv->base.texture, prsc);
   sv->base.context = pctx;

   /* merged with sampler state */
   sv->TE_SAMPLER_CONFIG0 =
      VIVS_TE_SAMPLER_CONFIG0_TYPE(translate_texture_target(sv->base.target)) |
      COND(!ext && !astc, VIVS_TE_SAMPLER_CONFIG0_FORMAT(format));
   sv->TE_SAMPLER_CONFIG0_MASK = 0xffffffff;

   uint32_t base_height = res->base.height0;
   uint32_t base_depth = res->base.depth0;
   bool is_array = false;

   switch (sv->base.target) {
   case PIPE_TEXTURE_1D:
      /* use 2D texture with T wrap to repeat for 1D texture
       * TODO: check if old HW supports 1D texture
       */
      sv->TE_SAMPLER_CONFIG0_MASK = ~VIVS_TE_SAMPLER_CONFIG0_VWRAP__MASK;
      sv->TE_SAMPLER_CONFIG0 &= ~VIVS_TE_SAMPLER_CONFIG0_TYPE__MASK;
      sv->TE_SAMPLER_CONFIG0 |=
         VIVS_TE_SAMPLER_CONFIG0_TYPE(TEXTURE_TYPE_2D) |
         VIVS_TE_SAMPLER_CONFIG0_VWRAP(TEXTURE_WRAPMODE_REPEAT);
      break;
   case PIPE_TEXTURE_1D_ARRAY:
      is_array = true;
      base_height = res->base.array_size;
      break;
   case PIPE_TEXTURE_2D_ARRAY:
      is_array = true;
      base_depth = res->base.array_size;
      break;
   default:
      break;
   }

   if (res->layout == ETNA_LAYOUT_LINEAR && !util_format_is_compressed(so->format)) {
      sv->TE_SAMPLER_CONFIG0 |= VIVS_TE_SAMPLER_CONFIG0_ADDRESSING_MODE(TEXTURE_ADDRESSING_MODE_LINEAR);

      assert(res->base.last_level == 0);
      sv->TE_SAMPLER_LINEAR_STRIDE = res->levels[0].stride;
   } else {
      sv->TE_SAMPLER_CONFIG0 |= VIVS_TE_SAMPLER_CONFIG0_ADDRESSING_MODE(TEXTURE_ADDRESSING_MODE_TILED);
      sv->TE_SAMPLER_LINEAR_STRIDE = 0;
   }

   sv->TE_SAMPLER_CONFIG1 |= COND(ext, VIVS_TE_SAMPLER_CONFIG1_FORMAT_EXT(format)) |
                             COND(astc, VIVS_TE_SAMPLER_CONFIG1_FORMAT_EXT(TEXTURE_FORMAT_EXT_ASTC)) |
                             COND(is_array, VIVS_TE_SAMPLER_CONFIG1_TEXTURE_ARRAY) |
                             VIVS_TE_SAMPLER_CONFIG1_HALIGN(res->halign) | swiz;
   sv->TE_SAMPLER_ASTC0 = COND(astc, VIVS_NTE_SAMPLER_ASTC0_ASTC_FORMAT(format)) |
                          COND(astc && srgb, VIVS_NTE_SAMPLER_ASTC0_ASTC_SRGB) |
                          VIVS_NTE_SAMPLER_ASTC0_UNK8(0xc) |
                          VIVS_NTE_SAMPLER_ASTC0_UNK16(0xc) |
                          VIVS_NTE_SAMPLER_ASTC0_UNK24(0xc);
   sv->TE_SAMPLER_SIZE = VIVS_TE_SAMPLER_SIZE_WIDTH(res->base.width0) |
                         VIVS_TE_SAMPLER_SIZE_HEIGHT(base_height);
   sv->TE_SAMPLER_LOG_SIZE =
      VIVS_TE_SAMPLER_LOG_SIZE_WIDTH(etna_log2_fixp55(res->base.width0)) |
      VIVS_TE_SAMPLER_LOG_SIZE_HEIGHT(etna_log2_fixp55(base_height)) |
      COND(util_format_is_srgb(so->format) && !astc, VIVS_TE_SAMPLER_LOG_SIZE_SRGB) |
      COND(astc, VIVS_TE_SAMPLER_LOG_SIZE_ASTC);
   sv->TE_SAMPLER_3D_CONFIG =
      VIVS_TE_SAMPLER_3D_CONFIG_DEPTH(base_depth) |
      VIVS_TE_SAMPLER_3D_CONFIG_LOG_DEPTH(etna_log2_fixp55(base_depth));

   /* Set up levels-of-detail */
   for (int lod = 0; lod <= res->base.last_level; ++lod) {
      sv->TE_SAMPLER_LOD_ADDR[lod].bo = res->bo;
      sv->TE_SAMPLER_LOD_ADDR[lod].offset = res->levels[lod].offset;
      sv->TE_SAMPLER_LOD_ADDR[lod].flags = ETNA_RELOC_READ;
   }
   sv->min_lod = sv->base.u.tex.first_level << 5;
   sv->max_lod = MIN2(sv->base.u.tex.last_level, res->base.last_level) << 5;

   /* Workaround for npot textures -- it appears that only CLAMP_TO_EDGE is
    * supported when the appropriate capability is not set. */
   if (!screen->specs.npot_tex_any_wrap &&
       (!util_is_power_of_two_or_zero(res->base.width0) ||
        !util_is_power_of_two_or_zero(res->base.height0))) {
      sv->TE_SAMPLER_CONFIG0_MASK = ~(VIVS_TE_SAMPLER_CONFIG0_UWRAP__MASK |
                                      VIVS_TE_SAMPLER_CONFIG0_VWRAP__MASK);
      sv->TE_SAMPLER_CONFIG0 |=
         VIVS_TE_SAMPLER_CONFIG0_UWRAP(TEXTURE_WRAPMODE_CLAMP_TO_EDGE) |
         VIVS_TE_SAMPLER_CONFIG0_VWRAP(TEXTURE_WRAPMODE_CLAMP_TO_EDGE);
   }

   return &sv->base;
}

static void
etna_sampler_view_state_destroy(struct pipe_context *pctx,
                          struct pipe_sampler_view *view)
{
   pipe_resource_reference(&view->texture, NULL);
   FREE(view);
}

#define EMIT_STATE(state_name, src_value) \
   etna_coalsence_emit(stream, &coalesce, VIVS_##state_name, src_value)

#define EMIT_STATE_FIXP(state_name, src_value) \
   etna_coalsence_emit_fixp(stream, &coalesce, VIVS_##state_name, src_value)

#define EMIT_STATE_RELOC(state_name, src_value) \
   etna_coalsence_emit_reloc(stream, &coalesce, VIVS_##state_name, src_value)

/* Emit plain (non-descriptor) texture state */
static void
etna_emit_texture_state(struct etna_context *ctx)
{
   struct etna_cmd_stream *stream = ctx->stream;
   struct etna_screen *screen = ctx->screen;
   uint32_t active_samplers = active_samplers_bits(ctx);
   uint32_t dirty = ctx->dirty;
   struct etna_coalesce coalesce;

   etna_coalesce_start(stream, &coalesce);

   if (unlikely(dirty & ETNA_DIRTY_SAMPLER_VIEWS)) {
      for (int x = 0; x < VIVS_TS_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
            /*01720*/ EMIT_STATE(TS_SAMPLER_CONFIG(x), sv->ts.TS_SAMPLER_CONFIG);
         }
      }
      for (int x = 0; x < VIVS_TS_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
            /*01740*/ EMIT_STATE_RELOC(TS_SAMPLER_STATUS_BASE(x), &sv->ts.TS_SAMPLER_STATUS_BASE);
         }
      }
      for (int x = 0; x < VIVS_TS_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
            /*01760*/ EMIT_STATE(TS_SAMPLER_CLEAR_VALUE(x), sv->ts.TS_SAMPLER_CLEAR_VALUE);
         }
      }
      for (int x = 0; x < VIVS_TS_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
            /*01780*/ EMIT_STATE(TS_SAMPLER_CLEAR_VALUE2(x), sv->ts.TS_SAMPLER_CLEAR_VALUE2);
         }
      }
   }
   if (unlikely(dirty & (ETNA_DIRTY_SAMPLER_VIEWS | ETNA_DIRTY_SAMPLERS))) {
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         uint32_t val = 0; /* 0 == sampler inactive */

         /* set active samplers to their configuration value (determined by both
          * the sampler state and sampler view) */
         if ((1 << x) & active_samplers) {
            struct etna_sampler_state *ss = etna_sampler_state(ctx->sampler[x]);
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);

            val = (ss->TE_SAMPLER_CONFIG0 & sv->TE_SAMPLER_CONFIG0_MASK) |
                  sv->TE_SAMPLER_CONFIG0;
         }

         /*02000*/ EMIT_STATE(TE_SAMPLER_CONFIG0(x), val);
      }
   }
   if (unlikely(dirty & (ETNA_DIRTY_SAMPLER_VIEWS))) {
      struct etna_sampler_state *ss;
      struct etna_sampler_view *sv;

      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            sv = etna_sampler_view(ctx->sampler_view[x]);
            /*02040*/ EMIT_STATE(TE_SAMPLER_SIZE(x), sv->TE_SAMPLER_SIZE);
         }
      }
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            ss = etna_sampler_state(ctx->sampler[x]);
            sv = etna_sampler_view(ctx->sampler_view[x]);
            uint32_t TE_SAMPLER_LOG_SIZE = sv->TE_SAMPLER_LOG_SIZE;

            if (texture_use_int_filter(&sv->base, &ss->base, false))
               TE_SAMPLER_LOG_SIZE |= VIVS_TE_SAMPLER_LOG_SIZE_INT_FILTER;

            /*02080*/ EMIT_STATE(TE_SAMPLER_LOG_SIZE(x), TE_SAMPLER_LOG_SIZE);
         }
      }
   }
   if (unlikely(dirty & (ETNA_DIRTY_SAMPLER_VIEWS | ETNA_DIRTY_SAMPLERS))) {
      struct etna_sampler_state *ss;
      struct etna_sampler_view *sv;

      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            ss = etna_sampler_state(ctx->sampler[x]);
            sv = etna_sampler_view(ctx->sampler_view[x]);

            unsigned max_lod = MAX2(MIN2(ss->max_lod, sv->max_lod), ss->max_lod_min);

            /* min and max lod is determined both by the sampler and the view */
            /*020C0*/ EMIT_STATE(TE_SAMPLER_LOD_CONFIG(x),
                                 ss->TE_SAMPLER_LOD_CONFIG |
                                 VIVS_TE_SAMPLER_LOD_CONFIG_MAX(max_lod) |
                                 VIVS_TE_SAMPLER_LOD_CONFIG_MIN(MAX2(ss->min_lod, sv->min_lod)));
         }
      }
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            ss = etna_sampler_state(ctx->sampler[x]);
            sv = etna_sampler_view(ctx->sampler_view[x]);

            /*02180*/ EMIT_STATE(TE_SAMPLER_3D_CONFIG(x), ss->TE_SAMPLER_3D_CONFIG |
                                                          sv->TE_SAMPLER_3D_CONFIG);
         }
      }
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            ss = etna_sampler_state(ctx->sampler[x]);
            sv = etna_sampler_view(ctx->sampler_view[x]);

            /*021C0*/ EMIT_STATE(TE_SAMPLER_CONFIG1(x), ss->TE_SAMPLER_CONFIG1 |
                                                        sv->TE_SAMPLER_CONFIG1 |
                                                        COND(sv->ts.enable, VIVS_TE_SAMPLER_CONFIG1_USE_TS));
         }
      }
   }
   if (unlikely(dirty & (ETNA_DIRTY_SAMPLER_VIEWS))) {
      for (int y = 0; y < VIVS_TE_SAMPLER_LOD_ADDR__LEN; ++y) {
         for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
            if ((1 << x) & active_samplers) {
               struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
               /*02400*/ EMIT_STATE_RELOC(TE_SAMPLER_LOD_ADDR(x, y),&sv->TE_SAMPLER_LOD_ADDR[y]);
            }
         }
      }
   }
   if (unlikely(dirty & (ETNA_DIRTY_SAMPLER_VIEWS))) {
      /* only LOD0 is valid for this register */
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
            /*02C00*/ EMIT_STATE(TE_SAMPLER_LINEAR_STRIDE(0, x), sv->TE_SAMPLER_LINEAR_STRIDE);
         }
      }
   }
   if (unlikely(screen->specs.tex_astc && (dirty & (ETNA_DIRTY_SAMPLER_VIEWS)))) {
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_view *sv = etna_sampler_view(ctx->sampler_view[x]);
            /*10500*/ EMIT_STATE(NTE_SAMPLER_ASTC0(x), sv->TE_SAMPLER_ASTC0);
         }
      }
   }
   if (unlikely(screen->specs.halti >= 1 && (dirty & (ETNA_DIRTY_SAMPLER_VIEWS)))) {
      for (int x = 0; x < VIVS_TE_SAMPLER__LEN; ++x) {
         if ((1 << x) & active_samplers) {
            struct etna_sampler_state *ss = etna_sampler_state(ctx->sampler[x]);
            /*10700*/ EMIT_STATE(NTE_SAMPLER_BASELOD(x), ss->NTE_SAMPLER_BASELOD);
         }
      }
   }
   etna_coalesce_end(stream, &coalesce);
}

#undef EMIT_STATE
#undef EMIT_STATE_FIXP
#undef EMIT_STATE_RELOC

static struct etna_sampler_ts*
etna_ts_for_sampler_view_state(struct pipe_sampler_view *pview)
{
   struct etna_sampler_view *sv = etna_sampler_view(pview);
   return &sv->ts;
}

void
etna_texture_state_init(struct pipe_context *pctx)
{
   struct etna_context *ctx = etna_context(pctx);
   DBG("etnaviv: Using state-based texturing");
   ctx->base.create_sampler_state = etna_create_sampler_state_state;
   ctx->base.delete_sampler_state = etna_delete_sampler_state_state;
   ctx->base.create_sampler_view = etna_create_sampler_view_state;
   ctx->base.sampler_view_destroy = etna_sampler_view_state_destroy;
   ctx->emit_texture_state = etna_emit_texture_state;
   ctx->ts_for_sampler_view = etna_ts_for_sampler_view_state;
}
