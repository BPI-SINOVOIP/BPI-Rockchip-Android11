/*
 * Copyright 2014, 2015 Red Hat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "util/u_memory.h"
#include "util/format/u_format.h"
#include "util/format/u_format_s3tc.h"
#include "util/u_screen.h"
#include "util/u_video.h"
#include "util/u_math.h"
#include "util/os_time.h"
#include "util/xmlconfig.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"

#include "tgsi/tgsi_exec.h"

#include "virgl_screen.h"
#include "virgl_resource.h"
#include "virgl_public.h"
#include "virgl_context.h"
#include "virtio-gpu/virgl_protocol.h"

int virgl_debug = 0;
static const struct debug_named_value debug_options[] = {
   { "verbose",   VIRGL_DEBUG_VERBOSE,             NULL },
   { "tgsi",      VIRGL_DEBUG_TGSI,                NULL },
   { "noemubgra", VIRGL_DEBUG_NO_EMULATE_BGRA,     "Disable tweak to emulate BGRA as RGBA on GLES hosts"},
   { "nobgraswz", VIRGL_DEBUG_NO_BGRA_DEST_SWIZZLE,"Disable tweak to swizzle emulated BGRA on GLES hosts" },
   { "sync",      VIRGL_DEBUG_SYNC,                "Sync after every flush" },
   { "xfer",      VIRGL_DEBUG_XFER,                "Do not optimize for transfers" },
   DEBUG_NAMED_VALUE_END
};
DEBUG_GET_ONCE_FLAGS_OPTION(virgl_debug, "VIRGL_DEBUG", debug_options, 0)

static const char *
virgl_get_vendor(struct pipe_screen *screen)
{
   return "Mesa/X.org";
}


static const char *
virgl_get_name(struct pipe_screen *screen)
{
   return "virgl";
}

static int
virgl_get_param(struct pipe_screen *screen, enum pipe_cap param)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   switch (param) {
   case PIPE_CAP_NPOT_TEXTURES:
      return 1;
   case PIPE_CAP_FRAGMENT_SHADER_TEXTURE_LOD:
   case PIPE_CAP_FRAGMENT_SHADER_DERIVATIVES:
   case PIPE_CAP_VERTEX_SHADER_SATURATE:
      return 1;
   case PIPE_CAP_ANISOTROPIC_FILTER:
      return 1;
   case PIPE_CAP_POINT_SPRITE:
      return 1;
   case PIPE_CAP_MAX_RENDER_TARGETS:
      return vscreen->caps.caps.v1.max_render_targets;
   case PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS:
      return vscreen->caps.caps.v1.max_dual_source_render_targets;
   case PIPE_CAP_OCCLUSION_QUERY:
      return vscreen->caps.caps.v1.bset.occlusion_query;
   case PIPE_CAP_TEXTURE_MIRROR_CLAMP:
   case PIPE_CAP_TEXTURE_MIRROR_CLAMP_TO_EDGE:
      return vscreen->caps.caps.v1.bset.mirror_clamp;
   case PIPE_CAP_TEXTURE_SWIZZLE:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_2D_SIZE:
      if (vscreen->caps.caps.v2.max_texture_2d_size)
         return vscreen->caps.caps.v2.max_texture_2d_size;
      return 16384;
   case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
      if (vscreen->caps.caps.v2.max_texture_3d_size)
         return 1 + util_logbase2(vscreen->caps.caps.v2.max_texture_3d_size);
      return 9; /* 256 x 256 x 256 */
   case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
      if (vscreen->caps.caps.v2.max_texture_cube_size)
         return 1 + util_logbase2(vscreen->caps.caps.v2.max_texture_cube_size);
      return 13; /* 4K x 4K */
   case PIPE_CAP_BLEND_EQUATION_SEPARATE:
      return 1;
   case PIPE_CAP_INDEP_BLEND_ENABLE:
      return vscreen->caps.caps.v1.bset.indep_blend_enable;
   case PIPE_CAP_INDEP_BLEND_FUNC:
      return vscreen->caps.caps.v1.bset.indep_blend_func;
   case PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT:
   case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER:
   case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER:
      return 1;
   case PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT:
      return vscreen->caps.caps.v1.bset.fragment_coord_conventions;
   case PIPE_CAP_DEPTH_CLIP_DISABLE:
      if (vscreen->caps.caps.v1.bset.depth_clip_disable)
         return 1;
      if (vscreen->caps.caps.v2.host_feature_check_version >= 3)
         return 2;
      return 0;
   case PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS:
      return vscreen->caps.caps.v1.max_streamout_buffers;
   case PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS:
   case PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS:
      return 16*4;
   case PIPE_CAP_PRIMITIVE_RESTART:
   case PIPE_CAP_PRIMITIVE_RESTART_FIXED_INDEX:
      return vscreen->caps.caps.v1.bset.primitive_restart;
   case PIPE_CAP_SHADER_STENCIL_EXPORT:
      return vscreen->caps.caps.v1.bset.shader_stencil_export;
   case PIPE_CAP_TGSI_INSTANCEID:
   case PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR:
      return 1;
   case PIPE_CAP_SEAMLESS_CUBE_MAP:
      return vscreen->caps.caps.v1.bset.seamless_cube_map;
   case PIPE_CAP_SEAMLESS_CUBE_MAP_PER_TEXTURE:
      return vscreen->caps.caps.v1.bset.seamless_cube_map_per_texture;
   case PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS:
      return vscreen->caps.caps.v1.max_texture_array_layers;
   case PIPE_CAP_MIN_TEXEL_OFFSET:
      return vscreen->caps.caps.v2.min_texel_offset;
   case PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET:
      return vscreen->caps.caps.v2.min_texture_gather_offset;
   case PIPE_CAP_MAX_TEXEL_OFFSET:
      return vscreen->caps.caps.v2.max_texel_offset;
   case PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET:
      return vscreen->caps.caps.v2.max_texture_gather_offset;
   case PIPE_CAP_CONDITIONAL_RENDER:
      return vscreen->caps.caps.v1.bset.conditional_render;
   case PIPE_CAP_TEXTURE_BARRIER:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_TEXTURE_BARRIER;
   case PIPE_CAP_VERTEX_COLOR_UNCLAMPED:
      return 1;
   case PIPE_CAP_FRAGMENT_COLOR_CLAMPED:
   case PIPE_CAP_VERTEX_COLOR_CLAMPED:
      return vscreen->caps.caps.v1.bset.color_clamping;
   case PIPE_CAP_MIXED_COLORBUFFER_FORMATS:
      return (vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_FBO_MIXED_COLOR_FORMATS) ||
            (vscreen->caps.caps.v2.host_feature_check_version < 1);
   case PIPE_CAP_GLSL_FEATURE_LEVEL:
      return vscreen->caps.caps.v1.glsl_level;
   case PIPE_CAP_GLSL_FEATURE_LEVEL_COMPATIBILITY:
      return MIN2(vscreen->caps.caps.v1.glsl_level, 140);
   case PIPE_CAP_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION:
   case PIPE_CAP_DEPTH_CLIP_DISABLE_SEPARATE:
      return 0;
   case PIPE_CAP_COMPUTE:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_COMPUTE_SHADER;
   case PIPE_CAP_USER_VERTEX_BUFFERS:
      return 0;
   case PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT:
      return vscreen->caps.caps.v2.uniform_buffer_offset_alignment;
   case PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME:
   case PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS:
      return vscreen->caps.caps.v1.bset.streamout_pause_resume;
   case PIPE_CAP_START_INSTANCE:
      return vscreen->caps.caps.v1.bset.start_instance;
   case PIPE_CAP_TGSI_CAN_COMPACT_CONSTANTS:
   case PIPE_CAP_VERTEX_BUFFER_OFFSET_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_VERTEX_BUFFER_STRIDE_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_VERTEX_ELEMENT_SRC_OFFSET_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_PREFER_BLIT_BASED_TEXTURE_TRANSFER:
      return 0;
   case PIPE_CAP_QUERY_TIMESTAMP:
      return 1;
   case PIPE_CAP_QUERY_TIME_ELAPSED:
      return 1;
   case PIPE_CAP_TGSI_TEXCOORD:
      return 0;
   case PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT:
      return VIRGL_MAP_BUFFER_ALIGNMENT;
   case PIPE_CAP_TEXTURE_BUFFER_OBJECTS:
      return vscreen->caps.caps.v1.max_tbo_size > 0;
   case PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT:
      return vscreen->caps.caps.v2.texture_buffer_offset_alignment;
   case PIPE_CAP_BUFFER_SAMPLER_VIEW_RGBA_ONLY:
      return 0;
   case PIPE_CAP_CUBE_MAP_ARRAY:
      return vscreen->caps.caps.v1.bset.cube_map_array;
   case PIPE_CAP_TEXTURE_MULTISAMPLE:
      return vscreen->caps.caps.v1.bset.texture_multisample;
   case PIPE_CAP_MAX_VIEWPORTS:
      return vscreen->caps.caps.v1.max_viewports;
   case PIPE_CAP_MAX_TEXTURE_BUFFER_SIZE:
      return vscreen->caps.caps.v1.max_tbo_size;
   case PIPE_CAP_TEXTURE_BORDER_COLOR_QUIRK:
   case PIPE_CAP_QUERY_PIPELINE_STATISTICS:
   case PIPE_CAP_ENDIANNESS:
      return 0;
   case PIPE_CAP_MIXED_FRAMEBUFFER_SIZES:
   case PIPE_CAP_MIXED_COLOR_DEPTH_BITS:
      return 1;
   case PIPE_CAP_TGSI_VS_LAYER_VIEWPORT:
      return 0;
   case PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES:
      return vscreen->caps.caps.v2.max_geom_output_vertices;
   case PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
      return vscreen->caps.caps.v2.max_geom_total_output_components;
   case PIPE_CAP_TEXTURE_QUERY_LOD:
      return vscreen->caps.caps.v1.bset.texture_query_lod;
   case PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS:
      return vscreen->caps.caps.v1.max_texture_gather_components;
   case PIPE_CAP_DRAW_INDIRECT:
      return vscreen->caps.caps.v1.bset.has_indirect_draw;
   case PIPE_CAP_SAMPLE_SHADING:
   case PIPE_CAP_FORCE_PERSAMPLE_INTERP:
      return vscreen->caps.caps.v1.bset.has_sample_shading;
   case PIPE_CAP_CULL_DISTANCE:
      return vscreen->caps.caps.v1.bset.has_cull;
   case PIPE_CAP_MAX_VERTEX_STREAMS:
      return ((vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_TRANSFORM_FEEDBACK3) ||
              (vscreen->caps.caps.v2.host_feature_check_version < 2)) ? 4 : 1;
   case PIPE_CAP_CONDITIONAL_RENDER_INVERTED:
      return vscreen->caps.caps.v1.bset.conditional_render_inverted;
   case PIPE_CAP_TGSI_FS_FINE_DERIVATIVE:
      return vscreen->caps.caps.v1.bset.derivative_control;
   case PIPE_CAP_POLYGON_OFFSET_CLAMP:
      return vscreen->caps.caps.v1.bset.polygon_offset_clamp;
   case PIPE_CAP_QUERY_SO_OVERFLOW:
      return vscreen->caps.caps.v1.bset.transform_feedback_overflow_query;
   case PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT:
      return vscreen->caps.caps.v2.shader_buffer_offset_alignment;
   case PIPE_CAP_DOUBLES:
      return vscreen->caps.caps.v1.bset.has_fp64 ||
            (vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_FAKE_FP64);
   case PIPE_CAP_MAX_SHADER_PATCH_VARYINGS:
      return vscreen->caps.caps.v2.max_shader_patch_varyings;
   case PIPE_CAP_SAMPLER_VIEW_TARGET:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_TEXTURE_VIEW;
   case PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE:
      return vscreen->caps.caps.v2.max_vertex_attrib_stride;
   case PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_COPY_IMAGE;
   case PIPE_CAP_TGSI_TXQS:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_TXQS;
   case PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_FB_NO_ATTACH;
   case PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_ROBUST_BUFFER_ACCESS;
   case PIPE_CAP_FBFETCH:
      return (vscreen->caps.caps.v2.capability_bits &
              VIRGL_CAP_TGSI_FBFETCH) ? 1 : 0;
   case PIPE_CAP_BLEND_EQUATION_ADVANCED:
      return vscreen->caps.caps.v2.capability_bits_v2 & VIRGL_CAP_V2_BLEND_EQUATION;
   case PIPE_CAP_TGSI_CLOCK:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_SHADER_CLOCK;
   case PIPE_CAP_TGSI_ARRAY_COMPONENTS:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_TGSI_COMPONENTS;
   case PIPE_CAP_MAX_COMBINED_SHADER_BUFFERS:
      return vscreen->caps.caps.v2.max_combined_shader_buffers;
   case PIPE_CAP_MAX_COMBINED_HW_ATOMIC_COUNTERS:
      return vscreen->caps.caps.v2.max_combined_atomic_counters;
   case PIPE_CAP_MAX_COMBINED_HW_ATOMIC_COUNTER_BUFFERS:
      return vscreen->caps.caps.v2.max_combined_atomic_counter_buffers;
   case PIPE_CAP_TEXTURE_FLOAT_LINEAR:
   case PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR:
      return 1; /* TODO: need to introduce a hw-cap for this */
   case PIPE_CAP_QUERY_BUFFER_OBJECT:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_QBO;
   case PIPE_CAP_MAX_VARYINGS:
      if (vscreen->caps.caps.v1.glsl_level < 150)
         return vscreen->caps.caps.v2.max_vertex_attribs;
      return 32;
   case PIPE_CAP_FAKE_SW_MSAA:
      /* If the host supports only one sample (e.g., if it is using softpipe),
       * fake multisampling to able to advertise higher GL versions. */
      return (vscreen->caps.caps.v1.max_samples == 1) ? 1 : 0;
   case PIPE_CAP_MULTI_DRAW_INDIRECT:
      return !!(vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_MULTI_DRAW_INDIRECT);
   case PIPE_CAP_MULTI_DRAW_INDIRECT_PARAMS:
      return !!(vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_INDIRECT_PARAMS);
   case PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT:
      return (vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_ARB_BUFFER_STORAGE) &&
             (vscreen->caps.caps.v2.host_feature_check_version >= 4) &&
              vscreen->vws->supports_coherent;
   case PIPE_CAP_PCI_GROUP:
   case PIPE_CAP_PCI_BUS:
   case PIPE_CAP_PCI_DEVICE:
   case PIPE_CAP_PCI_FUNCTION:
   case PIPE_CAP_GLSL_OPTIMIZE_CONSERVATIVELY:
   case PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION:
      return 0;
   case PIPE_CAP_CLEAR_TEXTURE:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_CLEAR_TEXTURE;
   case PIPE_CAP_CLIP_HALFZ:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_CLIP_HALFZ;
   case PIPE_CAP_MAX_GS_INVOCATIONS:
      return 32;
   case PIPE_CAP_MAX_SHADER_BUFFER_SIZE:
      return 1 << 27;
   case PIPE_CAP_VENDOR_ID:
      return 0x1af4;
   case PIPE_CAP_DEVICE_ID:
      return 0x1010;
   case PIPE_CAP_ACCELERATED:
      return 1;
   case PIPE_CAP_UMA:
   case PIPE_CAP_VIDEO_MEMORY:
      return 0;
   case PIPE_CAP_NATIVE_FENCE_FD:
      return vscreen->vws->supports_fences;
   case PIPE_CAP_DEST_SURFACE_SRGB_CONTROL:
      return (vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_SRGB_WRITE_CONTROL) ||
            (vscreen->caps.caps.v2.host_feature_check_version < 1);
   case PIPE_CAP_TGSI_SKIP_SHRINK_IO_ARRAYS:
      return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_INDIRECT_INPUT_ADDR;
   default:
      return u_pipe_screen_get_param_defaults(screen, param);
   }
}

static int
virgl_get_shader_param(struct pipe_screen *screen,
                       enum pipe_shader_type shader,
                       enum pipe_shader_cap param)
{
   struct virgl_screen *vscreen = virgl_screen(screen);

   if ((shader == PIPE_SHADER_TESS_CTRL || shader == PIPE_SHADER_TESS_EVAL) &&
       !vscreen->caps.caps.v1.bset.has_tessellation_shaders)
      return 0;

   if (shader == PIPE_SHADER_COMPUTE &&
       !(vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_COMPUTE_SHADER))
     return 0;

   switch(shader)
   {
   case PIPE_SHADER_FRAGMENT:
   case PIPE_SHADER_VERTEX:
   case PIPE_SHADER_GEOMETRY:
   case PIPE_SHADER_TESS_CTRL:
   case PIPE_SHADER_TESS_EVAL:
   case PIPE_SHADER_COMPUTE:
      switch (param) {
      case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
      case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
      case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
      case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
         return INT_MAX;
      case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
      case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
      case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
         return 1;
      case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
      case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
         return vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_INDIRECT_INPUT_ADDR;
      case PIPE_SHADER_CAP_MAX_INPUTS:
         if (vscreen->caps.caps.v1.glsl_level < 150)
            return vscreen->caps.caps.v2.max_vertex_attribs;
         return (shader == PIPE_SHADER_VERTEX ||
                 shader == PIPE_SHADER_GEOMETRY) ? vscreen->caps.caps.v2.max_vertex_attribs : 32;
      case PIPE_SHADER_CAP_MAX_OUTPUTS:
         if (shader == PIPE_SHADER_FRAGMENT)
            return vscreen->caps.caps.v1.max_render_targets;
         return vscreen->caps.caps.v2.max_vertex_outputs;
     // case PIPE_SHADER_CAP_MAX_CONSTS:
     //    return 4096;
      case PIPE_SHADER_CAP_MAX_TEMPS:
         return 256;
      case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
         return vscreen->caps.caps.v1.max_uniform_blocks;
    //  case PIPE_SHADER_CAP_MAX_ADDRS:
     //    return 1;
      case PIPE_SHADER_CAP_SUBROUTINES:
         return 1;
      case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
            return 16;
      case PIPE_SHADER_CAP_INTEGERS:
         return vscreen->caps.caps.v1.glsl_level >= 130;
      case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
         return 32;
      case PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE:
         return 4096 * sizeof(float[4]);
      case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
         if (shader == PIPE_SHADER_FRAGMENT || shader == PIPE_SHADER_COMPUTE)
            return vscreen->caps.caps.v2.max_shader_buffer_frag_compute;
         else
            return vscreen->caps.caps.v2.max_shader_buffer_other_stages;
      case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
         if (shader == PIPE_SHADER_FRAGMENT || shader == PIPE_SHADER_COMPUTE)
            return vscreen->caps.caps.v2.max_shader_image_frag_compute;
         else
            return vscreen->caps.caps.v2.max_shader_image_other_stages;
      case PIPE_SHADER_CAP_SUPPORTED_IRS:
         return (1 << PIPE_SHADER_IR_TGSI);
      case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTERS:
         return vscreen->caps.caps.v2.max_atomic_counters[shader];
      case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTER_BUFFERS:
         return vscreen->caps.caps.v2.max_atomic_counter_buffers[shader];
      case PIPE_SHADER_CAP_LOWER_IF_THRESHOLD:
      case PIPE_SHADER_CAP_TGSI_SKIP_MERGE_REGISTERS:
      case PIPE_SHADER_CAP_INT64_ATOMICS:
      case PIPE_SHADER_CAP_FP16:
      case PIPE_SHADER_CAP_FP16_DERIVATIVES:
      case PIPE_SHADER_CAP_INT16:
      case PIPE_SHADER_CAP_GLSL_16BIT_CONSTS:
         return 0;
      default:
         return 0;
      }
   default:
      return 0;
   }
}

static float
virgl_get_paramf(struct pipe_screen *screen, enum pipe_capf param)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   switch (param) {
   case PIPE_CAPF_MAX_LINE_WIDTH:
      return vscreen->caps.caps.v2.max_aliased_line_width;
   case PIPE_CAPF_MAX_LINE_WIDTH_AA:
      return vscreen->caps.caps.v2.max_smooth_line_width;
   case PIPE_CAPF_MAX_POINT_WIDTH:
      return vscreen->caps.caps.v2.max_aliased_point_size;
   case PIPE_CAPF_MAX_POINT_WIDTH_AA:
      return vscreen->caps.caps.v2.max_smooth_point_size;
   case PIPE_CAPF_MAX_TEXTURE_ANISOTROPY:
      return 16.0;
   case PIPE_CAPF_MAX_TEXTURE_LOD_BIAS:
      return vscreen->caps.caps.v2.max_texture_lod_bias;
   case PIPE_CAPF_MIN_CONSERVATIVE_RASTER_DILATE:
   case PIPE_CAPF_MAX_CONSERVATIVE_RASTER_DILATE:
   case PIPE_CAPF_CONSERVATIVE_RASTER_DILATE_GRANULARITY:
      return 0.0f;
   }
   /* should only get here on unhandled cases */
   debug_printf("Unexpected PIPE_CAPF %d query\n", param);
   return 0.0;
}

static int
virgl_get_compute_param(struct pipe_screen *screen,
                        enum pipe_shader_ir ir_type,
                        enum pipe_compute_cap param,
                        void *ret)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   if (!(vscreen->caps.caps.v2.capability_bits & VIRGL_CAP_COMPUTE_SHADER))
      return 0;
   switch (param) {
   case PIPE_COMPUTE_CAP_MAX_GRID_SIZE:
      if (ret) {
         uint64_t *grid_size = ret;
         grid_size[0] = vscreen->caps.caps.v2.max_compute_grid_size[0];
         grid_size[1] = vscreen->caps.caps.v2.max_compute_grid_size[1];
         grid_size[2] = vscreen->caps.caps.v2.max_compute_grid_size[2];
      }
      return 3 * sizeof(uint64_t) ;
   case PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE:
      if (ret) {
         uint64_t *block_size = ret;
         block_size[0] = vscreen->caps.caps.v2.max_compute_block_size[0];
         block_size[1] = vscreen->caps.caps.v2.max_compute_block_size[1];
         block_size[2] = vscreen->caps.caps.v2.max_compute_block_size[2];
      }
      return 3 * sizeof(uint64_t);
   case PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK:
      if (ret) {
         uint64_t *max_threads_per_block = ret;
         *max_threads_per_block = vscreen->caps.caps.v2.max_compute_work_group_invocations;
      }
      return sizeof(uint64_t);
   case PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE:
      if (ret) {
         uint64_t *max_local_size = ret;
         /* Value reported by the closed source driver. */
         *max_local_size = vscreen->caps.caps.v2.max_compute_shared_memory_size;
      }
      return sizeof(uint64_t);
   default:
      break;
   }
   return 0;
}

static bool
has_format_bit(struct virgl_supported_format_mask *mask,
               enum virgl_formats fmt)
{
   assert(fmt < VIRGL_FORMAT_MAX);
   unsigned val = (unsigned)fmt;
   unsigned idx = val / 32;
   unsigned bit = val % 32;
   assert(idx < ARRAY_SIZE(mask->bitmask));
   return (mask->bitmask[idx] & (1u << bit)) != 0;
}

bool
virgl_has_readback_format(struct pipe_screen *screen,
                          enum virgl_formats fmt)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   return has_format_bit(&vscreen->caps.caps.v2.supported_readback_formats,
                         fmt);
}

static bool
virgl_is_vertex_format_supported(struct pipe_screen *screen,
                                 enum pipe_format format)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   const struct util_format_description *format_desc;
   int i;

   format_desc = util_format_description(format);
   if (!format_desc)
      return false;

   if (format == PIPE_FORMAT_R11G11B10_FLOAT) {
      int vformat = VIRGL_FORMAT_R11G11B10_FLOAT;
      int big = vformat / 32;
      int small = vformat % 32;
      if (!(vscreen->caps.caps.v1.vertexbuffer.bitmask[big] & (1 << small)))
         return false;
      return true;
   }

   /* Find the first non-VOID channel. */
   for (i = 0; i < 4; i++) {
      if (format_desc->channel[i].type != UTIL_FORMAT_TYPE_VOID) {
         break;
      }
   }

   if (i == 4)
      return false;

   if (format_desc->layout != UTIL_FORMAT_LAYOUT_PLAIN)
      return false;

   if (format_desc->channel[i].type == UTIL_FORMAT_TYPE_FIXED)
      return false;
   return true;
}

static bool
virgl_format_check_bitmask(enum pipe_format format,
                           uint32_t bitmask[16],
                           bool may_emulate_bgra)
{
   enum virgl_formats vformat = pipe_to_virgl_format(format);
   int big = vformat / 32;
   int small = vformat % 32;
   if ((bitmask[big] & (1 << small)))
      return true;

   /* On GLES hosts we don't advertise BGRx_SRGB, but we may be able
    * emulate it by using a swizzled RGBx */
   if (may_emulate_bgra) {
      if (format == PIPE_FORMAT_B8G8R8A8_SRGB)
         format = PIPE_FORMAT_R8G8B8A8_SRGB;
      else if (format == PIPE_FORMAT_B8G8R8X8_SRGB)
         format = PIPE_FORMAT_R8G8B8X8_SRGB;
      else {
         return false;
      }

      vformat = pipe_to_virgl_format(format);
      big = vformat / 32;
      small = vformat % 32;
      if (bitmask[big] & (1 << small))
         return true;
   }
   return false;
}

/**
 * Query format support for creating a texture, drawing surface, etc.
 * \param format  the format to test
 * \param type  one of PIPE_TEXTURE, PIPE_SURFACE
 */
static bool
virgl_is_format_supported( struct pipe_screen *screen,
                                 enum pipe_format format,
                                 enum pipe_texture_target target,
                                 unsigned sample_count,
                                 unsigned storage_sample_count,
                                 unsigned bind)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   const struct util_format_description *format_desc;
   int i;

   union virgl_caps *caps = &vscreen->caps.caps; 
   boolean may_emulate_bgra = (caps->v2.capability_bits &
                               VIRGL_CAP_APP_TWEAK_SUPPORT) &&
                               vscreen->tweak_gles_emulate_bgra;

   if (MAX2(1, sample_count) != MAX2(1, storage_sample_count))
      return false;

   if (!util_is_power_of_two_or_zero(sample_count))
      return false;

   assert(target == PIPE_BUFFER ||
          target == PIPE_TEXTURE_1D ||
          target == PIPE_TEXTURE_1D_ARRAY ||
          target == PIPE_TEXTURE_2D ||
          target == PIPE_TEXTURE_2D_ARRAY ||
          target == PIPE_TEXTURE_RECT ||
          target == PIPE_TEXTURE_3D ||
          target == PIPE_TEXTURE_CUBE ||
          target == PIPE_TEXTURE_CUBE_ARRAY);

   format_desc = util_format_description(format);
   if (!format_desc)
      return false;

   if (util_format_is_intensity(format))
      return false;

   if (sample_count > 1) {
      if (!caps->v1.bset.texture_multisample)
         return false;

      if (bind & PIPE_BIND_SHADER_IMAGE) {
         if (sample_count > caps->v2.max_image_samples)
            return false;
      }

      if (sample_count > caps->v1.max_samples)
         return false;
   }

   if (bind & PIPE_BIND_VERTEX_BUFFER) {
      return virgl_is_vertex_format_supported(screen, format);
   }

   if (util_format_is_compressed(format) && target == PIPE_BUFFER)
      return false;

   /* Allow 3-comp 32 bit textures only for TBOs (needed for ARB_tbo_rgb32) */
   if ((format == PIPE_FORMAT_R32G32B32_FLOAT ||
       format == PIPE_FORMAT_R32G32B32_SINT ||
       format == PIPE_FORMAT_R32G32B32_UINT) &&
       target != PIPE_BUFFER)
      return false;

   if ((format_desc->layout == UTIL_FORMAT_LAYOUT_RGTC ||
        format_desc->layout == UTIL_FORMAT_LAYOUT_ETC ||
        format_desc->layout == UTIL_FORMAT_LAYOUT_S3TC) &&
       target == PIPE_TEXTURE_3D)
      return false;


   if (bind & PIPE_BIND_RENDER_TARGET) {
      /* For ARB_framebuffer_no_attachments. */
      if (format == PIPE_FORMAT_NONE)
         return TRUE;

      if (format_desc->colorspace == UTIL_FORMAT_COLORSPACE_ZS)
         return false;

      /*
       * Although possible, it is unnatural to render into compressed or YUV
       * surfaces. So disable these here to avoid going into weird paths
       * inside gallium frontends.
       */
      if (format_desc->block.width != 1 ||
          format_desc->block.height != 1)
         return false;

      if (!virgl_format_check_bitmask(format,
                                      caps->v1.render.bitmask,
                                      may_emulate_bgra))
         return false;
   }

   if (bind & PIPE_BIND_DEPTH_STENCIL) {
      if (format_desc->colorspace != UTIL_FORMAT_COLORSPACE_ZS)
         return false;
   }

   if (bind & PIPE_BIND_SCANOUT) {
      if (!virgl_format_check_bitmask(format, caps->v2.scanout.bitmask, false))
         return false;
   }

   /*
    * All other operations (sampling, transfer, etc).
    */

   if (format_desc->layout == UTIL_FORMAT_LAYOUT_S3TC) {
      goto out_lookup;
   }
   if (format_desc->layout == UTIL_FORMAT_LAYOUT_RGTC) {
      goto out_lookup;
   }
   if (format_desc->layout == UTIL_FORMAT_LAYOUT_BPTC) {
      goto out_lookup;
   }
   if (format_desc->layout == UTIL_FORMAT_LAYOUT_ETC) {
      goto out_lookup;
   }

   if (format == PIPE_FORMAT_R11G11B10_FLOAT) {
      goto out_lookup;
   } else if (format == PIPE_FORMAT_R9G9B9E5_FLOAT) {
      goto out_lookup;
   }

   /* Find the first non-VOID channel. */
   for (i = 0; i < 4; i++) {
      if (format_desc->channel[i].type != UTIL_FORMAT_TYPE_VOID) {
         break;
      }
   }

   if (i == 4)
      return false;

   /* no L4A4 */
   if (format_desc->nr_channels < 4 && format_desc->channel[i].size == 4)
      return false;

 out_lookup:
   return virgl_format_check_bitmask(format,
                                     caps->v1.sampler.bitmask,
                                     may_emulate_bgra);
}

static void virgl_flush_frontbuffer(struct pipe_screen *screen,
                                      struct pipe_resource *res,
                                      unsigned level, unsigned layer,
                                    void *winsys_drawable_handle, struct pipe_box *sub_box)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   struct virgl_winsys *vws = vscreen->vws;
   struct virgl_resource *vres = virgl_resource(res);

   if (vws->flush_frontbuffer)
      vws->flush_frontbuffer(vws, vres->hw_res, level, layer, winsys_drawable_handle,
                             sub_box);
}

static void virgl_fence_reference(struct pipe_screen *screen,
                                  struct pipe_fence_handle **ptr,
                                  struct pipe_fence_handle *fence)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   struct virgl_winsys *vws = vscreen->vws;

   vws->fence_reference(vws, ptr, fence);
}

static bool virgl_fence_finish(struct pipe_screen *screen,
                               struct pipe_context *ctx,
                               struct pipe_fence_handle *fence,
                               uint64_t timeout)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   struct virgl_winsys *vws = vscreen->vws;

   return vws->fence_wait(vws, fence, timeout);
}

static int virgl_fence_get_fd(struct pipe_screen *screen,
            struct pipe_fence_handle *fence)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   struct virgl_winsys *vws = vscreen->vws;

   return vws->fence_get_fd(vws, fence);
}

static uint64_t
virgl_get_timestamp(struct pipe_screen *_screen)
{
   return os_time_get_nano();
}

static void
virgl_destroy_screen(struct pipe_screen *screen)
{
   struct virgl_screen *vscreen = virgl_screen(screen);
   struct virgl_winsys *vws = vscreen->vws;

   slab_destroy_parent(&vscreen->transfer_pool);

   if (vws)
      vws->destroy(vws);
   FREE(vscreen);
}

static void
fixup_formats(union virgl_caps *caps, struct virgl_supported_format_mask *mask)
{
   const size_t size = ARRAY_SIZE(mask->bitmask);
   for (int i = 0; i < size; ++i) {
      if (mask->bitmask[i] != 0)
         return; /* we got some formats, we definately have a new protocol */
   }

   /* old protocol used; fall back to considering all sampleable formats valid
    * readback-formats
    */
   for (int i = 0; i < size; ++i)
      mask->bitmask[i] = caps->v1.sampler.bitmask[i];
}

struct pipe_screen *
virgl_create_screen(struct virgl_winsys *vws, const struct pipe_screen_config *config)
{
   struct virgl_screen *screen = CALLOC_STRUCT(virgl_screen);

   const char *VIRGL_GLES_EMULATE_BGRA = "gles_emulate_bgra";
   const char *VIRGL_GLES_APPLY_BGRA_DEST_SWIZZLE = "gles_apply_bgra_dest_swizzle";
   const char *VIRGL_GLES_SAMPLES_PASSED_VALUE = "gles_samples_passed_value";

   if (!screen)
      return NULL;

   virgl_debug = debug_get_option_virgl_debug();

   if (config && config->options) {
      screen->tweak_gles_emulate_bgra =
            driQueryOptionb(config->options, VIRGL_GLES_EMULATE_BGRA);
      screen->tweak_gles_apply_bgra_dest_swizzle =
            driQueryOptionb(config->options, VIRGL_GLES_APPLY_BGRA_DEST_SWIZZLE);
      screen->tweak_gles_tf3_value =
            driQueryOptioni(config->options, VIRGL_GLES_SAMPLES_PASSED_VALUE);
   }
   screen->tweak_gles_emulate_bgra &= !(virgl_debug & VIRGL_DEBUG_NO_EMULATE_BGRA);
   screen->tweak_gles_apply_bgra_dest_swizzle &= !(virgl_debug & VIRGL_DEBUG_NO_BGRA_DEST_SWIZZLE);

   screen->vws = vws;
   screen->base.get_name = virgl_get_name;
   screen->base.get_vendor = virgl_get_vendor;
   screen->base.get_param = virgl_get_param;
   screen->base.get_shader_param = virgl_get_shader_param;
   screen->base.get_compute_param = virgl_get_compute_param;
   screen->base.get_paramf = virgl_get_paramf;
   screen->base.is_format_supported = virgl_is_format_supported;
   screen->base.destroy = virgl_destroy_screen;
   screen->base.context_create = virgl_context_create;
   screen->base.flush_frontbuffer = virgl_flush_frontbuffer;
   screen->base.get_timestamp = virgl_get_timestamp;
   screen->base.fence_reference = virgl_fence_reference;
   //screen->base.fence_signalled = virgl_fence_signalled;
   screen->base.fence_finish = virgl_fence_finish;
   screen->base.fence_get_fd = virgl_fence_get_fd;

   virgl_init_screen_resource_functions(&screen->base);

   vws->get_caps(vws, &screen->caps);
   fixup_formats(&screen->caps.caps,
                 &screen->caps.caps.v2.supported_readback_formats);
   fixup_formats(&screen->caps.caps, &screen->caps.caps.v2.scanout);

   union virgl_caps *caps = &screen->caps.caps;
   bool may_emulate_bgra = (caps->v2.capability_bits & VIRGL_CAP_APP_TWEAK_SUPPORT);
   screen->tweak_gles_emulate_bgra &= !virgl_format_check_bitmask(
       PIPE_FORMAT_B8G8R8A8_SRGB, caps->v1.render.bitmask, may_emulate_bgra);
   screen->refcnt = 1;

   slab_create_parent(&screen->transfer_pool, sizeof(struct virgl_transfer), 16);

   return &screen->base;
}
