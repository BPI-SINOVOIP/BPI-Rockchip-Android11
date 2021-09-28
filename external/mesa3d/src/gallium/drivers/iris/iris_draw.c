/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @file iris_draw.c
 *
 * The main driver hooks for drawing and launching compute shaders.
 */

#include <stdio.h>
#include <errno.h>
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "util/u_inlines.h"
#include "util/u_transfer.h"
#include "util/u_upload_mgr.h"
#include "intel/compiler/brw_compiler.h"
#include "intel/compiler/brw_eu_defines.h"
#include "iris_context.h"
#include "iris_defines.h"

static bool
prim_is_points_or_lines(const struct pipe_draw_info *draw)
{
   /* We don't need to worry about adjacency - it can only be used with
    * geometry shaders, and we don't care about this info when GS is on.
    */
   return draw->mode == PIPE_PRIM_POINTS ||
          draw->mode == PIPE_PRIM_LINES ||
          draw->mode == PIPE_PRIM_LINE_LOOP ||
          draw->mode == PIPE_PRIM_LINE_STRIP;
}

/**
 * Record the current primitive mode and restart information, flagging
 * related packets as dirty if necessary.
 *
 * This must be called before updating compiled shaders, because the patch
 * information informs the TCS key.
 */
static void
iris_update_draw_info(struct iris_context *ice,
                      const struct pipe_draw_info *info)
{
   struct iris_screen *screen = (struct iris_screen *)ice->ctx.screen;
   const struct brw_compiler *compiler = screen->compiler;

   if (ice->state.prim_mode != info->mode) {
      ice->state.prim_mode = info->mode;
      ice->state.dirty |= IRIS_DIRTY_VF_TOPOLOGY;


      /* For XY Clip enables */
      bool points_or_lines = prim_is_points_or_lines(info);
      if (points_or_lines != ice->state.prim_is_points_or_lines) {
         ice->state.prim_is_points_or_lines = points_or_lines;
         ice->state.dirty |= IRIS_DIRTY_CLIP;
      }
   }

   if (info->mode == PIPE_PRIM_PATCHES &&
       ice->state.vertices_per_patch != info->vertices_per_patch) {
      ice->state.vertices_per_patch = info->vertices_per_patch;
      ice->state.dirty |= IRIS_DIRTY_VF_TOPOLOGY;

      /* 8_PATCH TCS needs this for key->input_vertices */
      if (compiler->use_tcs_8_patch)
         ice->state.stage_dirty |= IRIS_STAGE_DIRTY_UNCOMPILED_TCS;

      /* Flag constants dirty for gl_PatchVerticesIn if needed. */
      const struct shader_info *tcs_info =
         iris_get_shader_info(ice, MESA_SHADER_TESS_CTRL);
      if (tcs_info &&
          BITSET_TEST(tcs_info->system_values_read, SYSTEM_VALUE_VERTICES_IN)) {
         ice->state.stage_dirty |= IRIS_STAGE_DIRTY_CONSTANTS_TCS;
         ice->state.shaders[MESA_SHADER_TESS_CTRL].sysvals_need_upload = true;
      }
   }

   if (ice->state.primitive_restart != info->primitive_restart ||
       ice->state.cut_index != info->restart_index) {
      ice->state.dirty |= IRIS_DIRTY_VF;
      ice->state.primitive_restart = info->primitive_restart;
      ice->state.cut_index = info->restart_index;
   }
}

/**
 * Update shader draw parameters, flagging VF packets as dirty if necessary.
 */
static void
iris_update_draw_parameters(struct iris_context *ice,
                            const struct pipe_draw_info *info)
{
   bool changed = false;

   if (ice->state.vs_uses_draw_params) {
      struct iris_state_ref *draw_params = &ice->draw.draw_params;

      if (info->indirect) {
         pipe_resource_reference(&draw_params->res, info->indirect->buffer);
         draw_params->offset =
            info->indirect->offset + (info->index_size ? 12 : 8);

         changed = true;
         ice->draw.params_valid = false;
      } else {
         int firstvertex = info->index_size ? info->index_bias : info->start;

         if (!ice->draw.params_valid ||
             ice->draw.params.firstvertex != firstvertex ||
             ice->draw.params.baseinstance != info->start_instance) {

            changed = true;
            ice->draw.params.firstvertex = firstvertex;
            ice->draw.params.baseinstance = info->start_instance;
            ice->draw.params_valid = true;

            u_upload_data(ice->ctx.stream_uploader, 0,
                          sizeof(ice->draw.params), 4, &ice->draw.params,
                          &draw_params->offset, &draw_params->res);
         }
      }
   }

   if (ice->state.vs_uses_derived_draw_params) {
      struct iris_state_ref *derived_params = &ice->draw.derived_draw_params;
      int is_indexed_draw = info->index_size ? -1 : 0;

      if (ice->draw.derived_params.drawid != info->drawid ||
          ice->draw.derived_params.is_indexed_draw != is_indexed_draw) {

         changed = true;
         ice->draw.derived_params.drawid = info->drawid;
         ice->draw.derived_params.is_indexed_draw = is_indexed_draw;

         u_upload_data(ice->ctx.stream_uploader, 0,
                       sizeof(ice->draw.derived_params), 4,
                       &ice->draw.derived_params,
                       &derived_params->offset, &derived_params->res);
      }
   }

   if (changed) {
      ice->state.dirty |= IRIS_DIRTY_VERTEX_BUFFERS |
                          IRIS_DIRTY_VERTEX_ELEMENTS |
                          IRIS_DIRTY_VF_SGVS;
   }
}

static void
iris_indirect_draw_vbo(struct iris_context *ice,
                       const struct pipe_draw_info *dinfo)
{
   struct iris_batch *batch = &ice->batches[IRIS_BATCH_RENDER];
   struct pipe_draw_info info = *dinfo;

   if (info.indirect->indirect_draw_count &&
       ice->state.predicate == IRIS_PREDICATE_STATE_USE_BIT) {
      /* Upload MI_PREDICATE_RESULT to GPR15.*/
      batch->screen->vtbl.load_register_reg64(batch, CS_GPR(15), MI_PREDICATE_RESULT);
   }

   const uint64_t orig_dirty = ice->state.dirty;
   const uint64_t orig_stage_dirty = ice->state.stage_dirty;

   for (int i = 0; i < info.indirect->draw_count; i++) {
      info.drawid = i;

      iris_batch_maybe_flush(batch, 1500);

      iris_update_draw_parameters(ice, &info);

      batch->screen->vtbl.upload_render_state(ice, batch, &info);

      ice->state.dirty &= ~IRIS_ALL_DIRTY_FOR_RENDER;
      ice->state.stage_dirty &= ~IRIS_ALL_STAGE_DIRTY_FOR_RENDER;

      info.indirect->offset += info.indirect->stride;
   }

   if (info.indirect->indirect_draw_count &&
       ice->state.predicate == IRIS_PREDICATE_STATE_USE_BIT) {
      /* Restore MI_PREDICATE_RESULT. */
      batch->screen->vtbl.load_register_reg64(batch, MI_PREDICATE_RESULT, CS_GPR(15));
   }

   /* Put this back for post-draw resolves, we'll clear it again after. */
   ice->state.dirty = orig_dirty;
   ice->state.stage_dirty = orig_stage_dirty;
}

static void
iris_simple_draw_vbo(struct iris_context *ice,
                     const struct pipe_draw_info *draw)
{
   struct iris_batch *batch = &ice->batches[IRIS_BATCH_RENDER];

   iris_batch_maybe_flush(batch, 1500);

   iris_update_draw_parameters(ice, draw);

   batch->screen->vtbl.upload_render_state(ice, batch, draw);
}

/**
 * The pipe->draw_vbo() driver hook.  Performs a draw on the GPU.
 */
void
iris_draw_vbo(struct pipe_context *ctx, const struct pipe_draw_info *info)
{
   struct iris_context *ice = (struct iris_context *) ctx;
   struct iris_screen *screen = (struct iris_screen*)ice->ctx.screen;
   const struct gen_device_info *devinfo = &screen->devinfo;
   struct iris_batch *batch = &ice->batches[IRIS_BATCH_RENDER];

   if (ice->state.predicate == IRIS_PREDICATE_STATE_DONT_RENDER)
      return;

   /* We can't safely re-emit 3DSTATE_SO_BUFFERS because it may zero the
    * write offsets, changing the behavior.
    */
   if (INTEL_DEBUG & DEBUG_REEMIT) {
      ice->state.dirty |= IRIS_ALL_DIRTY_FOR_RENDER & ~IRIS_DIRTY_SO_BUFFERS;
      ice->state.stage_dirty |= IRIS_ALL_STAGE_DIRTY_FOR_RENDER;
   }

   iris_update_draw_info(ice, info);

   if (devinfo->gen == 9)
      gen9_toggle_preemption(ice, batch, info);

   iris_update_compiled_shaders(ice);

   if (ice->state.dirty & IRIS_DIRTY_RENDER_RESOLVES_AND_FLUSHES) {
      bool draw_aux_buffer_disabled[BRW_MAX_DRAW_BUFFERS] = { };
      for (gl_shader_stage stage = 0; stage < MESA_SHADER_COMPUTE; stage++) {
         if (ice->shaders.prog[stage])
            iris_predraw_resolve_inputs(ice, batch, draw_aux_buffer_disabled,
                                        stage, true);
      }
      iris_predraw_resolve_framebuffer(ice, batch, draw_aux_buffer_disabled);
   }

   iris_binder_reserve_3d(ice);

   batch->screen->vtbl.update_surface_base_address(batch, &ice->state.binder);

   iris_handle_always_flush_cache(batch);

   if (info->indirect)
      iris_indirect_draw_vbo(ice, info);
   else
      iris_simple_draw_vbo(ice, info);

   iris_handle_always_flush_cache(batch);

   iris_postdraw_update_resolve_tracking(ice, batch);

   ice->state.dirty &= ~IRIS_ALL_DIRTY_FOR_RENDER;
   ice->state.stage_dirty &= ~IRIS_ALL_STAGE_DIRTY_FOR_RENDER;
}

static void
iris_update_grid_size_resource(struct iris_context *ice,
                               const struct pipe_grid_info *grid)
{
   const struct iris_screen *screen = (void *) ice->ctx.screen;
   const struct isl_device *isl_dev = &screen->isl_dev;
   struct iris_state_ref *grid_ref = &ice->state.grid_size;
   struct iris_state_ref *state_ref = &ice->state.grid_surf_state;

   const struct iris_compiled_shader *shader = ice->shaders.prog[MESA_SHADER_COMPUTE];
   bool grid_needs_surface = shader->bt.used_mask[IRIS_SURFACE_GROUP_CS_WORK_GROUPS];
   bool grid_updated = false;

   if (grid->indirect) {
      pipe_resource_reference(&grid_ref->res, grid->indirect);
      grid_ref->offset = grid->indirect_offset;

      /* Zero out the grid size so that the next non-indirect grid launch will
       * re-upload it properly.
       */
      memset(ice->state.last_grid, 0, sizeof(ice->state.last_grid));
      grid_updated = true;
   } else if (memcmp(ice->state.last_grid, grid->grid, sizeof(grid->grid)) != 0) {
      memcpy(ice->state.last_grid, grid->grid, sizeof(grid->grid));
      u_upload_data(ice->state.dynamic_uploader, 0, sizeof(grid->grid), 4,
                    grid->grid, &grid_ref->offset, &grid_ref->res);
      grid_updated = true;
   }

   /* If we changed the grid, the old surface state is invalid. */
   if (grid_updated)
      pipe_resource_reference(&state_ref->res, NULL);

   /* Skip surface upload if we don't need it or we already have one */
   if (!grid_needs_surface || state_ref->res)
      return;

   struct iris_bo *grid_bo = iris_resource_bo(grid_ref->res);

   void *surf_map = NULL;
   u_upload_alloc(ice->state.surface_uploader, 0, isl_dev->ss.size,
                  isl_dev->ss.align, &state_ref->offset, &state_ref->res,
                  &surf_map);
   state_ref->offset +=
      iris_bo_offset_from_base_address(iris_resource_bo(state_ref->res));
   isl_buffer_fill_state(&screen->isl_dev, surf_map,
                         .address = grid_ref->offset + grid_bo->gtt_offset,
                         .size_B = sizeof(grid->grid),
                         .format = ISL_FORMAT_RAW,
                         .stride_B = 1,
                         .mocs = iris_mocs(grid_bo, isl_dev,
                                           ISL_SURF_USAGE_CONSTANT_BUFFER_BIT));

   ice->state.stage_dirty |= IRIS_STAGE_DIRTY_BINDINGS_CS;
}

void
iris_launch_grid(struct pipe_context *ctx, const struct pipe_grid_info *grid)
{
   struct iris_context *ice = (struct iris_context *) ctx;
   struct iris_batch *batch = &ice->batches[IRIS_BATCH_COMPUTE];

   if (ice->state.predicate == IRIS_PREDICATE_STATE_DONT_RENDER)
      return;

   if (INTEL_DEBUG & DEBUG_REEMIT) {
      ice->state.dirty |= IRIS_ALL_DIRTY_FOR_COMPUTE;
      ice->state.stage_dirty |= IRIS_ALL_STAGE_DIRTY_FOR_COMPUTE;
   }

   if (ice->state.dirty & IRIS_DIRTY_COMPUTE_RESOLVES_AND_FLUSHES)
      iris_predraw_resolve_inputs(ice, batch, NULL, MESA_SHADER_COMPUTE, false);

   iris_batch_maybe_flush(batch, 1500);

   iris_update_compiled_compute_shader(ice);

   if (memcmp(ice->state.last_block, grid->block, sizeof(grid->block)) != 0) {
      memcpy(ice->state.last_block, grid->block, sizeof(grid->block));
      ice->state.stage_dirty |= IRIS_STAGE_DIRTY_CONSTANTS_CS;
      ice->state.shaders[MESA_SHADER_COMPUTE].sysvals_need_upload = true;
   }

   iris_update_grid_size_resource(ice, grid);

   iris_binder_reserve_compute(ice);
   batch->screen->vtbl.update_surface_base_address(batch, &ice->state.binder);

   if (ice->state.compute_predicate) {
      batch->screen->vtbl.load_register_mem64(batch, MI_PREDICATE_RESULT,
                                    ice->state.compute_predicate, 0);
      ice->state.compute_predicate = NULL;
   }

   iris_handle_always_flush_cache(batch);

   batch->screen->vtbl.upload_compute_state(ice, batch, grid);

   iris_handle_always_flush_cache(batch);

   ice->state.dirty &= ~IRIS_ALL_DIRTY_FOR_COMPUTE;
   ice->state.stage_dirty &= ~IRIS_ALL_STAGE_DIRTY_FOR_COMPUTE;

   /* Note: since compute shaders can't access the framebuffer, there's
    * no need to call iris_postdraw_update_resolve_tracking.
    */
}
