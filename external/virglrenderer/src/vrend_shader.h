/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifndef VREND_SHADER_H
#define VREND_SHADER_H

#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"

/* need to store patching info for interpolation */
struct vrend_interp_info {
   int semantic_name;
   int semantic_index;
   int interpolate;
   unsigned location;
};

struct vrend_array {
   int first;
   int array_size;
};

struct vrend_shader_info {
   uint32_t samplers_used_mask;
   uint32_t images_used_mask;
   uint32_t ssbo_used_mask;
   int num_consts;
   int num_inputs;
   int num_interps;
   int num_outputs;
   int num_ubos;
   int ubo_idx[32];
   bool ubo_indirect;
   uint8_t num_indirect_generic_outputs;
   uint8_t num_indirect_patch_outputs;
   uint8_t num_indirect_generic_inputs;
   uint8_t num_indirect_patch_inputs;
   int num_ucp;
   int glsl_ver;
   bool has_pervertex_out;
   bool has_sample_input;
   uint8_t num_clip_out;
   uint8_t num_cull_out;
   uint32_t shadow_samp_mask;
   int gs_out_prim;
   int tes_prim;
   bool tes_point_mode;
   uint32_t attrib_input_mask;

   struct vrend_array *sampler_arrays;
   int num_sampler_arrays;

   struct vrend_array *image_arrays;
   int num_image_arrays;

   struct pipe_stream_output_info so_info;

   struct vrend_interp_info *interpinfo;
   char **so_names;
};

struct vrend_shader_key {
   uint32_t coord_replace;
   bool invert_fs_origin;
   bool pstipple_tex;
   bool add_alpha_test;
   bool color_two_side;
   uint8_t alpha_test;
   uint8_t clip_plane_enable;
   bool gs_present;
   bool tcs_present;
   bool tes_present;
   bool flatshade;
   bool prev_stage_pervertex_out;
   uint8_t prev_stage_num_clip_out;
   uint8_t prev_stage_num_cull_out;
   float alpha_ref_val;
   uint32_t cbufs_are_a8_bitmask;
   uint8_t num_indirect_generic_outputs;
   uint8_t num_indirect_patch_outputs;
   uint8_t num_indirect_generic_inputs;
   uint8_t num_indirect_patch_inputs;
};

struct vrend_shader_cfg {
   int glsl_version;
   int max_draw_buffers;
   bool use_gles;
   bool use_core_profile;
   bool use_explicit_locations;
};

bool vrend_patch_vertex_shader_interpolants(struct vrend_shader_cfg *cfg,
                                            char *program,
                                            struct vrend_shader_info *vs_info,
                                            struct vrend_shader_info *fs_info,
                                            const char *oprefix, bool flatshade);

char *vrend_convert_shader(struct vrend_shader_cfg *cfg,
                           const struct tgsi_token *tokens,
                           uint32_t req_local_mem,
                           struct vrend_shader_key *key,
                           struct vrend_shader_info *sinfo);
const char *vrend_shader_samplertypeconv(int sampler_type, int *is_shad);
char vrend_shader_samplerreturnconv(enum tgsi_return_type type);

int shader_lookup_sampler_array(struct vrend_shader_info *sinfo, int index);
#endif
