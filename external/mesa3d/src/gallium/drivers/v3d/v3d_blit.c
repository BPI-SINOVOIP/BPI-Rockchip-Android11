/*
 * Copyright © 2015-2017 Broadcom
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

#include "util/format/u_format.h"
#include "util/u_surface.h"
#include "util/u_blitter.h"
#include "v3d_context.h"
#include "v3d_tiling.h"

#if 0
static struct pipe_surface *
v3d_get_blit_surface(struct pipe_context *pctx,
                     struct pipe_resource *prsc, unsigned level)
{
        struct pipe_surface tmpl;

        memset(&tmpl, 0, sizeof(tmpl));
        tmpl.format = prsc->format;
        tmpl.u.tex.level = level;
        tmpl.u.tex.first_layer = 0;
        tmpl.u.tex.last_layer = 0;

        return pctx->create_surface(pctx, prsc, &tmpl);
}

static bool
is_tile_unaligned(unsigned size, unsigned tile_size)
{
        return size & (tile_size - 1);
}

static bool
v3d_tile_blit(struct pipe_context *pctx, const struct pipe_blit_info *info)
{
        struct v3d_context *v3d = v3d_context(pctx);
        bool msaa = (info->src.resource->nr_samples > 1 ||
                     info->dst.resource->nr_samples > 1);
        int tile_width = msaa ? 32 : 64;
        int tile_height = msaa ? 32 : 64;

        if (util_format_is_depth_or_stencil(info->dst.resource->format))
                return false;

        if (info->scissor_enable)
                return false;

        if ((info->mask & PIPE_MASK_RGBA) == 0)
                return false;

        if (info->dst.box.x != info->src.box.x ||
            info->dst.box.y != info->src.box.y ||
            info->dst.box.width != info->src.box.width ||
            info->dst.box.height != info->src.box.height) {
                return false;
        }

        int dst_surface_width = u_minify(info->dst.resource->width0,
                                         info->dst.level);
        int dst_surface_height = u_minify(info->dst.resource->height0,
                                         info->dst.level);
        if (is_tile_unaligned(info->dst.box.x, tile_width) ||
            is_tile_unaligned(info->dst.box.y, tile_height) ||
            (is_tile_unaligned(info->dst.box.width, tile_width) &&
             info->dst.box.x + info->dst.box.width != dst_surface_width) ||
            (is_tile_unaligned(info->dst.box.height, tile_height) &&
             info->dst.box.y + info->dst.box.height != dst_surface_height)) {
                return false;
        }

        /* VC5_PACKET_LOAD_TILE_BUFFER_GENERAL uses the
         * VC5_PACKET_TILE_RENDERING_MODE_CONFIG's width (determined by our
         * destination surface) to determine the stride.  This may be wrong
         * when reading from texture miplevels > 0, which are stored in
         * POT-sized areas.  For MSAA, the tile addresses are computed
         * explicitly by the RCL, but still use the destination width to
         * determine the stride (which could be fixed by explicitly supplying
         * it in the ABI).
         */
        struct v3d_resource *rsc = v3d_resource(info->src.resource);

        uint32_t stride;

        if (info->src.resource->nr_samples > 1)
                stride = align(dst_surface_width, 32) * 4 * rsc->cpp;
        /* XXX else if (rsc->slices[info->src.level].tiling == VC5_TILING_FORMAT_T)
           stride = align(dst_surface_width * rsc->cpp, 128); */
        else
                stride = align(dst_surface_width * rsc->cpp, 16);

        if (stride != rsc->slices[info->src.level].stride)
                return false;

        if (info->dst.resource->format != info->src.resource->format)
                return false;

        if (false) {
                fprintf(stderr, "RCL blit from %d,%d to %d,%d (%d,%d)\n",
                        info->src.box.x,
                        info->src.box.y,
                        info->dst.box.x,
                        info->dst.box.y,
                        info->dst.box.width,
                        info->dst.box.height);
        }

        struct pipe_surface *dst_surf =
                v3d_get_blit_surface(pctx, info->dst.resource, info->dst.level);
        struct pipe_surface *src_surf =
                v3d_get_blit_surface(pctx, info->src.resource, info->src.level);

        v3d_flush_jobs_reading_resource(v3d, info->src.resource, false);

        struct v3d_job *job = v3d_get_job(v3d, dst_surf, NULL);
        pipe_surface_reference(&job->color_read, src_surf);

        /* If we're resolving from MSAA to single sample, we still need to run
         * the engine in MSAA mode for the load.
         */
        if (!job->msaa && info->src.resource->nr_samples > 1) {
                job->msaa = true;
                job->tile_width = 32;
                job->tile_height = 32;
        }

        job->draw_min_x = info->dst.box.x;
        job->draw_min_y = info->dst.box.y;
        job->draw_max_x = info->dst.box.x + info->dst.box.width;
        job->draw_max_y = info->dst.box.y + info->dst.box.height;
        job->draw_width = dst_surf->width;
        job->draw_height = dst_surf->height;

        job->tile_width = tile_width;
        job->tile_height = tile_height;
        job->msaa = msaa;
        job->needs_flush = true;
        job->resolve |= PIPE_CLEAR_COLOR;

        v3d_job_submit(v3d, job);

        pipe_surface_reference(&dst_surf, NULL);
        pipe_surface_reference(&src_surf, NULL);

        return true;
}
#endif

void
v3d_blitter_save(struct v3d_context *v3d)
{
        util_blitter_save_fragment_constant_buffer_slot(v3d->blitter,
                                                        v3d->constbuf[PIPE_SHADER_FRAGMENT].cb);
        util_blitter_save_vertex_buffer_slot(v3d->blitter, v3d->vertexbuf.vb);
        util_blitter_save_vertex_elements(v3d->blitter, v3d->vtx);
        util_blitter_save_vertex_shader(v3d->blitter, v3d->prog.bind_vs);
        util_blitter_save_geometry_shader(v3d->blitter, v3d->prog.bind_gs);
        util_blitter_save_so_targets(v3d->blitter, v3d->streamout.num_targets,
                                     v3d->streamout.targets);
        util_blitter_save_rasterizer(v3d->blitter, v3d->rasterizer);
        util_blitter_save_viewport(v3d->blitter, &v3d->viewport);
        util_blitter_save_scissor(v3d->blitter, &v3d->scissor);
        util_blitter_save_fragment_shader(v3d->blitter, v3d->prog.bind_fs);
        util_blitter_save_blend(v3d->blitter, v3d->blend);
        util_blitter_save_depth_stencil_alpha(v3d->blitter, v3d->zsa);
        util_blitter_save_stencil_ref(v3d->blitter, &v3d->stencil_ref);
        util_blitter_save_sample_mask(v3d->blitter, v3d->sample_mask);
        util_blitter_save_framebuffer(v3d->blitter, &v3d->framebuffer);
        util_blitter_save_fragment_sampler_states(v3d->blitter,
                        v3d->tex[PIPE_SHADER_FRAGMENT].num_samplers,
                        (void **)v3d->tex[PIPE_SHADER_FRAGMENT].samplers);
        util_blitter_save_fragment_sampler_views(v3d->blitter,
                        v3d->tex[PIPE_SHADER_FRAGMENT].num_textures,
                        v3d->tex[PIPE_SHADER_FRAGMENT].textures);
        util_blitter_save_so_targets(v3d->blitter, v3d->streamout.num_targets,
                                     v3d->streamout.targets);
}

static bool
v3d_render_blit(struct pipe_context *ctx, struct pipe_blit_info *info)
{
        struct v3d_context *v3d = v3d_context(ctx);
        struct v3d_resource *src = v3d_resource(info->src.resource);
        struct pipe_resource *tiled = NULL;

        if (!src->tiled) {
                struct pipe_box box = {
                        .x = 0,
                        .y = 0,
                        .width = u_minify(info->src.resource->width0,
                                           info->src.level),
                        .height = u_minify(info->src.resource->height0,
                                           info->src.level),
                        .depth = 1,
                };
                struct pipe_resource tmpl = {
                        .target = info->src.resource->target,
                        .format = info->src.resource->format,
                        .width0 = box.width,
                        .height0 = box.height,
                        .depth0 = 1,
                        .array_size = 1,
                };
                tiled = ctx->screen->resource_create(ctx->screen, &tmpl);
                if (!tiled) {
                        fprintf(stderr, "Failed to create tiled blit temp\n");
                        return false;
                }
                ctx->resource_copy_region(ctx,
                                          tiled, 0,
                                          0, 0, 0,
                                          info->src.resource, info->src.level,
                                          &box);
                info->src.level = 0;
                info->src.resource = tiled;
        }

        if (!util_blitter_is_blit_supported(v3d->blitter, info)) {
                fprintf(stderr, "blit unsupported %s -> %s\n",
                    util_format_short_name(info->src.resource->format),
                    util_format_short_name(info->dst.resource->format));
                return false;
        }

        v3d_blitter_save(v3d);
        util_blitter_blit(v3d->blitter, info);

        pipe_resource_reference(&tiled, NULL);

        return true;
}

/* Implement stencil blits by reinterpreting the stencil data as an RGBA8888
 * or R8 texture.
 */
static void
v3d_stencil_blit(struct pipe_context *ctx, const struct pipe_blit_info *info)
{
        struct v3d_context *v3d = v3d_context(ctx);
        struct v3d_resource *src = v3d_resource(info->src.resource);
        struct v3d_resource *dst = v3d_resource(info->dst.resource);
        enum pipe_format src_format, dst_format;

        if (src->separate_stencil) {
                src = src->separate_stencil;
                src_format = PIPE_FORMAT_R8_UNORM;
        } else {
                src_format = PIPE_FORMAT_RGBA8888_UNORM;
        }

        if (dst->separate_stencil) {
                dst = dst->separate_stencil;
                dst_format = PIPE_FORMAT_R8_UNORM;
        } else {
                dst_format = PIPE_FORMAT_RGBA8888_UNORM;
        }

        /* Initialize the surface. */
        struct pipe_surface dst_tmpl = {
                .u.tex = {
                        .level = info->dst.level,
                        .first_layer = info->dst.box.z,
                        .last_layer = info->dst.box.z,
                },
                .format = dst_format,
        };
        struct pipe_surface *dst_surf =
                ctx->create_surface(ctx, &dst->base, &dst_tmpl);

        /* Initialize the sampler view. */
        struct pipe_sampler_view src_tmpl = {
                .target = src->base.target,
                .format = src_format,
                .u.tex = {
                        .first_level = info->src.level,
                        .last_level = info->src.level,
                        .first_layer = 0,
                        .last_layer = (PIPE_TEXTURE_3D ?
                                       u_minify(src->base.depth0,
                                                info->src.level) - 1 :
                                       src->base.array_size - 1),
                },
                .swizzle_r = PIPE_SWIZZLE_X,
                .swizzle_g = PIPE_SWIZZLE_Y,
                .swizzle_b = PIPE_SWIZZLE_Z,
                .swizzle_a = PIPE_SWIZZLE_W,
        };
        struct pipe_sampler_view *src_view =
                ctx->create_sampler_view(ctx, &src->base, &src_tmpl);

        v3d_blitter_save(v3d);
        util_blitter_blit_generic(v3d->blitter, dst_surf, &info->dst.box,
                                  src_view, &info->src.box,
                                  src->base.width0, src->base.height0,
                                  PIPE_MASK_R,
                                  PIPE_TEX_FILTER_NEAREST,
                                  info->scissor_enable ? &info->scissor : NULL,
                                  info->alpha_blend);

        pipe_surface_reference(&dst_surf, NULL);
        pipe_sampler_view_reference(&src_view, NULL);
}

/* Disable level 0 write, just write following mipmaps */
#define V3D_TFU_IOA_DIMTW (1 << 0)
#define V3D_TFU_IOA_FORMAT_SHIFT 3
#define V3D_TFU_IOA_FORMAT_LINEARTILE 3
#define V3D_TFU_IOA_FORMAT_UBLINEAR_1_COLUMN 4
#define V3D_TFU_IOA_FORMAT_UBLINEAR_2_COLUMN 5
#define V3D_TFU_IOA_FORMAT_UIF_NO_XOR 6
#define V3D_TFU_IOA_FORMAT_UIF_XOR 7

#define V3D_TFU_ICFG_NUMMM_SHIFT 5
#define V3D_TFU_ICFG_TTYPE_SHIFT 9

#define V3D_TFU_ICFG_OPAD_SHIFT 22

#define V3D_TFU_ICFG_FORMAT_SHIFT 18
#define V3D_TFU_ICFG_FORMAT_RASTER 0
#define V3D_TFU_ICFG_FORMAT_SAND_128 1
#define V3D_TFU_ICFG_FORMAT_SAND_256 2
#define V3D_TFU_ICFG_FORMAT_LINEARTILE 11
#define V3D_TFU_ICFG_FORMAT_UBLINEAR_1_COLUMN 12
#define V3D_TFU_ICFG_FORMAT_UBLINEAR_2_COLUMN 13
#define V3D_TFU_ICFG_FORMAT_UIF_NO_XOR 14
#define V3D_TFU_ICFG_FORMAT_UIF_XOR 15

static bool
v3d_tfu(struct pipe_context *pctx,
        struct pipe_resource *pdst,
        struct pipe_resource *psrc,
        unsigned int src_level,
        unsigned int base_level,
        unsigned int last_level,
        unsigned int src_layer,
        unsigned int dst_layer)
{
        struct v3d_context *v3d = v3d_context(pctx);
        struct v3d_screen *screen = v3d->screen;
        struct v3d_resource *src = v3d_resource(psrc);
        struct v3d_resource *dst = v3d_resource(pdst);
        struct v3d_resource_slice *src_base_slice = &src->slices[src_level];
        struct v3d_resource_slice *dst_base_slice = &dst->slices[base_level];
        int msaa_scale = pdst->nr_samples > 1 ? 2 : 1;
        int width = u_minify(pdst->width0, base_level) * msaa_scale;
        int height = u_minify(pdst->height0, base_level) * msaa_scale;

        if (psrc->format != pdst->format)
                return false;
        if (psrc->nr_samples != pdst->nr_samples)
                return false;

        uint32_t tex_format = v3d_get_tex_format(&screen->devinfo,
                                                 pdst->format);

        if (!v3d_tfu_supports_tex_format(&screen->devinfo, tex_format))
                return false;

        if (pdst->target != PIPE_TEXTURE_2D || psrc->target != PIPE_TEXTURE_2D)
                return false;

        /* Can't write to raster. */
        if (dst_base_slice->tiling == VC5_TILING_RASTER)
                return false;

        v3d_flush_jobs_writing_resource(v3d, psrc, V3D_FLUSH_DEFAULT, false);
        v3d_flush_jobs_reading_resource(v3d, pdst, V3D_FLUSH_DEFAULT, false);

        struct drm_v3d_submit_tfu tfu = {
                .ios = (height << 16) | width,
                .bo_handles = {
                        dst->bo->handle,
                        src != dst ? src->bo->handle : 0
                },
                .in_sync = v3d->out_sync,
                .out_sync = v3d->out_sync,
        };
        uint32_t src_offset = (src->bo->offset +
                               v3d_layer_offset(psrc, src_level, src_layer));
        tfu.iia |= src_offset;
        if (src_base_slice->tiling == VC5_TILING_RASTER) {
                tfu.icfg |= (V3D_TFU_ICFG_FORMAT_RASTER <<
                             V3D_TFU_ICFG_FORMAT_SHIFT);
        } else {
                tfu.icfg |= ((V3D_TFU_ICFG_FORMAT_LINEARTILE +
                              (src_base_slice->tiling - VC5_TILING_LINEARTILE)) <<
                             V3D_TFU_ICFG_FORMAT_SHIFT);
        }

        uint32_t dst_offset = (dst->bo->offset +
                               v3d_layer_offset(pdst, base_level, dst_layer));
        tfu.ioa |= dst_offset;
        if (last_level != base_level)
                tfu.ioa |= V3D_TFU_IOA_DIMTW;
        tfu.ioa |= ((V3D_TFU_IOA_FORMAT_LINEARTILE +
                     (dst_base_slice->tiling - VC5_TILING_LINEARTILE)) <<
                    V3D_TFU_IOA_FORMAT_SHIFT);

        tfu.icfg |= tex_format << V3D_TFU_ICFG_TTYPE_SHIFT;
        tfu.icfg |= (last_level - base_level) << V3D_TFU_ICFG_NUMMM_SHIFT;

        switch (src_base_slice->tiling) {
        case VC5_TILING_UIF_NO_XOR:
        case VC5_TILING_UIF_XOR:
                tfu.iis |= (src_base_slice->padded_height /
                            (2 * v3d_utile_height(src->cpp)));
                break;
        case VC5_TILING_RASTER:
                tfu.iis |= src_base_slice->stride / src->cpp;
                break;
        case VC5_TILING_LINEARTILE:
        case VC5_TILING_UBLINEAR_1_COLUMN:
        case VC5_TILING_UBLINEAR_2_COLUMN:
                break;
       }

        /* If we're writing level 0 (!IOA_DIMTW), then we need to supply the
         * OPAD field for the destination (how many extra UIF blocks beyond
         * those necessary to cover the height).  When filling mipmaps, the
         * miplevel 1+ tiling state is inferred.
         */
        if (dst_base_slice->tiling == VC5_TILING_UIF_NO_XOR ||
            dst_base_slice->tiling == VC5_TILING_UIF_XOR) {
                int uif_block_h = 2 * v3d_utile_height(dst->cpp);
                int implicit_padded_height = align(height, uif_block_h);

                tfu.icfg |= (((dst_base_slice->padded_height -
                               implicit_padded_height) / uif_block_h) <<
                             V3D_TFU_ICFG_OPAD_SHIFT);
        }

        int ret = v3d_ioctl(screen->fd, DRM_IOCTL_V3D_SUBMIT_TFU, &tfu);
        if (ret != 0) {
                fprintf(stderr, "Failed to submit TFU job: %d\n", ret);
                return false;
        }

        dst->writes++;

        return true;
}

bool
v3d_generate_mipmap(struct pipe_context *pctx,
                    struct pipe_resource *prsc,
                    enum pipe_format format,
                    unsigned int base_level,
                    unsigned int last_level,
                    unsigned int first_layer,
                    unsigned int last_layer)
{
        if (format != prsc->format)
                return false;

        /* We could maybe support looping over layers for array textures, but
         * we definitely don't support 3D.
         */
        if (first_layer != last_layer)
                return false;

        return v3d_tfu(pctx,
                       prsc, prsc,
                       base_level,
                       base_level, last_level,
                       first_layer, first_layer);
}

static bool
v3d_tfu_blit(struct pipe_context *pctx, const struct pipe_blit_info *info)
{
        int dst_width = u_minify(info->dst.resource->width0, info->dst.level);
        int dst_height = u_minify(info->dst.resource->height0, info->dst.level);

        if ((info->mask & PIPE_MASK_RGBA) == 0)
                return false;

        if (info->scissor_enable ||
            info->dst.box.x != 0 ||
            info->dst.box.y != 0 ||
            info->dst.box.width != dst_width ||
            info->dst.box.height != dst_height ||
            info->src.box.x != 0 ||
            info->src.box.y != 0 ||
            info->src.box.width != info->dst.box.width ||
            info->src.box.height != info->dst.box.height) {
                return false;
        }

        if (info->dst.format != info->src.format)
                return false;

        return v3d_tfu(pctx, info->dst.resource, info->src.resource,
                       info->src.level,
                       info->dst.level, info->dst.level,
                       info->src.box.z, info->dst.box.z);
}

/* Optimal hardware path for blitting pixels.
 * Scaling, format conversion, up- and downsampling (resolve) are allowed.
 */
void
v3d_blit(struct pipe_context *pctx, const struct pipe_blit_info *blit_info)
{
        struct v3d_context *v3d = v3d_context(pctx);
        struct pipe_blit_info info = *blit_info;

        if (info.mask & PIPE_MASK_S) {
                v3d_stencil_blit(pctx, blit_info);
                info.mask &= ~PIPE_MASK_S;
        }

        if (v3d_tfu_blit(pctx, blit_info))
                info.mask &= ~PIPE_MASK_RGBA;

        if (info.mask)
                v3d_render_blit(pctx, &info);

        /* Flush our blit jobs immediately.  They're unlikely to get reused by
         * normal drawing or other blits, and without flushing we can easily
         * run into unexpected OOMs when blits are used for a large series of
         * texture uploads before using the textures.
         */
        v3d_flush_jobs_writing_resource(v3d, info.dst.resource,
                                        V3D_FLUSH_DEFAULT, false);
}
