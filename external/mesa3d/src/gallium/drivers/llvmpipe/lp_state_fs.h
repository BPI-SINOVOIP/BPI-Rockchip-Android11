/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 **************************************************************************/


#ifndef LP_STATE_FS_H_
#define LP_STATE_FS_H_


#include "pipe/p_compiler.h"
#include "pipe/p_state.h"
#include "tgsi/tgsi_scan.h" /* for tgsi_shader_info */
#include "gallivm/lp_bld_sample.h" /* for struct lp_sampler_static_state */
#include "gallivm/lp_bld_tgsi.h" /* for lp_tgsi_info */
#include "lp_bld_interp.h" /* for struct lp_shader_input */
#include "util/u_inlines.h"

struct tgsi_token;
struct lp_fragment_shader;


/** Indexes into jit_function[] array */
#define RAST_WHOLE 0
#define RAST_EDGE_TEST 1


struct lp_sampler_static_state
{
   /*
    * These attributes are effectively interleaved for more sane key handling.
    * However, there might be lots of null space if the amount of samplers and
    * textures isn't the same.
    */
   struct lp_static_sampler_state sampler_state;
   struct lp_static_texture_state texture_state;
};


struct lp_image_static_state
{
   struct lp_static_texture_state image_state;
};

struct lp_fragment_shader_variant_key
{
   struct pipe_depth_state depth;
   struct pipe_stencil_state stencil[2];
   struct pipe_blend_state blend;

   struct {
      unsigned enabled:1;
      unsigned func:3;
   } alpha;

   unsigned nr_cbufs:8;
   unsigned nr_samplers:8;      /* actually derivable from just the shader */
   unsigned nr_sampler_views:8; /* actually derivable from just the shader */
   unsigned nr_images:8;        /* actually derivable from just the shader */
   unsigned flatshade:1;
   unsigned occlusion_count:1;
   unsigned resource_1d:1;
   unsigned depth_clamp:1;
   unsigned multisample:1;
   unsigned no_ms_sample_mask_out:1;

   enum pipe_format zsbuf_format;
   enum pipe_format cbuf_format[PIPE_MAX_COLOR_BUFS];

   uint8_t cbuf_nr_samples[PIPE_MAX_COLOR_BUFS];
   uint8_t zsbuf_nr_samples;
   uint8_t coverage_samples;
   uint8_t min_samples;

   struct lp_sampler_static_state samplers[1];
   /* followed by variable number of images */
};

#define LP_FS_MAX_VARIANT_KEY_SIZE                                      \
   (sizeof(struct lp_fragment_shader_variant_key) +                     \
    PIPE_MAX_SHADER_SAMPLER_VIEWS * sizeof(struct lp_sampler_static_state) +\
    PIPE_MAX_SHADER_IMAGES * sizeof(struct lp_image_static_state))

static inline size_t
lp_fs_variant_key_size(unsigned nr_samplers, unsigned nr_images)
{
   unsigned samplers = nr_samplers > 1 ? (nr_samplers - 1) : 0;
   return (sizeof(struct lp_fragment_shader_variant_key) +
           samplers * sizeof(struct lp_sampler_static_state) +
           nr_images * sizeof(struct lp_image_static_state));
}

static inline struct lp_image_static_state *
lp_fs_variant_key_images(struct lp_fragment_shader_variant_key *key)
{
   return (struct lp_image_static_state *)
      &key->samplers[key->nr_samplers];
}

/** doubly-linked list item */
struct lp_fs_variant_list_item
{
   struct lp_fragment_shader_variant *base;
   struct lp_fs_variant_list_item *next, *prev;
};


struct lp_fragment_shader_variant
{
   struct pipe_reference reference;
   boolean opaque;

   struct gallivm_state *gallivm;

   LLVMTypeRef jit_context_ptr_type;
   LLVMTypeRef jit_thread_data_ptr_type;
   LLVMTypeRef jit_linear_context_ptr_type;

   LLVMValueRef function[2];

   lp_jit_frag_func jit_function[2];

   /* Total number of LLVM instructions generated */
   unsigned nr_instrs;

   struct lp_fs_variant_list_item list_item_global, list_item_local;
   struct lp_fragment_shader *shader;

   /* For debugging/profiling purposes */
   unsigned no;

   /* key is variable-sized, must be last */
   struct lp_fragment_shader_variant_key key;
};


/** Subclass of pipe_shader_state */
struct lp_fragment_shader
{
   struct pipe_shader_state base;

   struct pipe_reference reference;
   struct lp_tgsi_info info;

   struct lp_fs_variant_list_item variants;

   struct draw_fragment_shader *draw_data;

   /* For debugging/profiling purposes */
   unsigned variant_key_size;
   unsigned no;
   unsigned variants_created;
   unsigned variants_cached;

   /** Fragment shader input interpolation info */
   struct lp_shader_input inputs[PIPE_MAX_SHADER_INPUTS];
};


void
lp_debug_fs_variant(struct lp_fragment_shader_variant *variant);

void
llvmpipe_destroy_fs(struct llvmpipe_context *llvmpipe,
                    struct lp_fragment_shader *shader);

static inline void
lp_fs_reference(struct llvmpipe_context *llvmpipe,
                struct lp_fragment_shader **ptr,
                struct lp_fragment_shader *shader)
{
   struct lp_fragment_shader *old_ptr = *ptr;
   if (pipe_reference(old_ptr ? &(*ptr)->reference : NULL, shader ? &shader->reference : NULL)) {
      llvmpipe_destroy_fs(llvmpipe, old_ptr);
   }
   *ptr = shader;
}

void
llvmpipe_destroy_shader_variant(struct llvmpipe_context *lp,
                                struct lp_fragment_shader_variant *variant);

static inline void
lp_fs_variant_reference(struct llvmpipe_context *llvmpipe,
                        struct lp_fragment_shader_variant **ptr,
                        struct lp_fragment_shader_variant *variant)
{
   struct lp_fragment_shader_variant *old_ptr = *ptr;
   if (pipe_reference(old_ptr ? &(*ptr)->reference : NULL, variant ? &variant->reference : NULL)) {
      llvmpipe_destroy_shader_variant(llvmpipe, old_ptr);
   }
   *ptr = variant;
}

#endif /* LP_STATE_FS_H_ */
