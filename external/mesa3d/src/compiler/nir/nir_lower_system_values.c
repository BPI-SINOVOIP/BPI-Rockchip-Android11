/*
 * Copyright © 2014 Intel Corporation
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
 * Authors:
 *    Connor Abbott (cwabbott0@gmail.com)
 *
 */

#include "nir.h"
#include "nir_builder.h"

static nir_ssa_def *
sanitize_32bit_sysval(nir_builder *b, nir_intrinsic_instr *intrin)
{
   assert(intrin->dest.is_ssa);
   const unsigned bit_size = intrin->dest.ssa.bit_size;
   if (bit_size == 32)
      return NULL;

   intrin->dest.ssa.bit_size = 32;
   return nir_u2u(b, &intrin->dest.ssa, bit_size);
}

static nir_ssa_def*
build_global_group_size(nir_builder *b, unsigned bit_size)
{
   nir_ssa_def *group_size = nir_load_local_group_size(b);
   nir_ssa_def *num_work_groups = nir_load_num_work_groups(b, bit_size);
   return nir_imul(b, nir_u2u(b, group_size, bit_size),
                      num_work_groups);
}

static bool
lower_system_value_filter(const nir_instr *instr, const void *_state)
{
   return instr->type == nir_instr_type_intrinsic;
}

static nir_ssa_def *
lower_system_value_instr(nir_builder *b, nir_instr *instr, void *_state)
{
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);

   /* All the intrinsics we care about are loads */
   if (!nir_intrinsic_infos[intrin->intrinsic].has_dest)
      return NULL;

   assert(intrin->dest.is_ssa);
   const unsigned bit_size = intrin->dest.ssa.bit_size;

   switch (intrin->intrinsic) {
   case nir_intrinsic_load_vertex_id:
      if (b->shader->options->vertex_id_zero_based) {
         return nir_iadd(b, nir_load_vertex_id_zero_base(b),
                            nir_load_first_vertex(b));
      } else {
         return NULL;
      }

   case nir_intrinsic_load_base_vertex:
      /**
       * From the OpenGL 4.6 (11.1.3.9 Shader Inputs) specification:
       *
       * "gl_BaseVertex holds the integer value passed to the baseVertex
       * parameter to the command that resulted in the current shader
       * invocation. In the case where the command has no baseVertex
       * parameter, the value of gl_BaseVertex is zero."
       */
      if (b->shader->options->lower_base_vertex) {
         return nir_iand(b, nir_load_is_indexed_draw(b),
                            nir_load_first_vertex(b));
      } else {
         return NULL;
      }

   case nir_intrinsic_load_helper_invocation:
      if (b->shader->options->lower_helper_invocation) {
         nir_ssa_def *tmp;
         tmp = nir_ishl(b, nir_imm_int(b, 1),
                           nir_load_sample_id_no_per_sample(b));
         tmp = nir_iand(b, nir_load_sample_mask_in(b), tmp);
         return nir_inot(b, nir_i2b(b, tmp));
      } else {
         return NULL;
      }

   case nir_intrinsic_load_local_invocation_id:
   case nir_intrinsic_load_local_invocation_index:
   case nir_intrinsic_load_local_group_size:
      return sanitize_32bit_sysval(b, intrin);

   case nir_intrinsic_load_deref: {
      nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);
      if (!nir_deref_mode_is(deref, nir_var_system_value))
         return NULL;

      nir_ssa_def *column = NULL;
      if (deref->deref_type != nir_deref_type_var) {
         /* The only one system values that aren't plane variables are
          * gl_SampleMask which is always an array of one element and a
          * couple of ray-tracing intrinsics which are matrices.
          */
         assert(deref->deref_type == nir_deref_type_array);
         assert(deref->arr.index.is_ssa);
         column = deref->arr.index.ssa;
         deref = nir_deref_instr_parent(deref);
         assert(deref->deref_type == nir_deref_type_var);
         assert(deref->var->data.location == SYSTEM_VALUE_SAMPLE_MASK_IN ||
                deref->var->data.location == SYSTEM_VALUE_RAY_OBJECT_TO_WORLD ||
                deref->var->data.location == SYSTEM_VALUE_RAY_WORLD_TO_OBJECT);
      }
      nir_variable *var = deref->var;

      switch (var->data.location) {
      case SYSTEM_VALUE_INSTANCE_INDEX:
         return nir_iadd(b, nir_load_instance_id(b),
                            nir_load_base_instance(b));

      case SYSTEM_VALUE_SUBGROUP_EQ_MASK:
      case SYSTEM_VALUE_SUBGROUP_GE_MASK:
      case SYSTEM_VALUE_SUBGROUP_GT_MASK:
      case SYSTEM_VALUE_SUBGROUP_LE_MASK:
      case SYSTEM_VALUE_SUBGROUP_LT_MASK: {
         nir_intrinsic_op op =
            nir_intrinsic_from_system_value(var->data.location);
         nir_intrinsic_instr *load = nir_intrinsic_instr_create(b->shader, op);
         nir_ssa_dest_init_for_type(&load->instr, &load->dest,
                                    var->type, NULL);
         load->num_components = load->dest.ssa.num_components;
         nir_builder_instr_insert(b, &load->instr);
         return &load->dest.ssa;
      }

      case SYSTEM_VALUE_DEVICE_INDEX:
         if (b->shader->options->lower_device_index_to_zero)
            return nir_imm_int(b, 0);
         break;

      case SYSTEM_VALUE_GLOBAL_GROUP_SIZE:
         return build_global_group_size(b, bit_size);

      case SYSTEM_VALUE_BARYCENTRIC_LINEAR_PIXEL:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_pixel,
                                     INTERP_MODE_NOPERSPECTIVE);

      case SYSTEM_VALUE_BARYCENTRIC_LINEAR_CENTROID:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_centroid,
                                     INTERP_MODE_NOPERSPECTIVE);

      case SYSTEM_VALUE_BARYCENTRIC_LINEAR_SAMPLE:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_sample,
                                     INTERP_MODE_NOPERSPECTIVE);

      case SYSTEM_VALUE_BARYCENTRIC_PERSP_PIXEL:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_pixel,
                                     INTERP_MODE_SMOOTH);

      case SYSTEM_VALUE_BARYCENTRIC_PERSP_CENTROID:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_centroid,
                                     INTERP_MODE_SMOOTH);

      case SYSTEM_VALUE_BARYCENTRIC_PERSP_SAMPLE:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_sample,
                                     INTERP_MODE_SMOOTH);

      case SYSTEM_VALUE_BARYCENTRIC_PULL_MODEL:
         return nir_load_barycentric(b, nir_intrinsic_load_barycentric_model,
                                     INTERP_MODE_NONE);

      default:
         break;
      }

      nir_intrinsic_op sysval_op =
         nir_intrinsic_from_system_value(var->data.location);
      if (glsl_type_is_matrix(var->type)) {
         assert(nir_intrinsic_infos[sysval_op].index_map[NIR_INTRINSIC_COLUMN] > 0);
         unsigned num_cols = glsl_get_matrix_columns(var->type);
         ASSERTED unsigned num_rows = glsl_get_vector_elements(var->type);
         assert(num_rows == intrin->dest.ssa.num_components);

         nir_ssa_def *cols[4];
         for (unsigned i = 0; i < num_cols; i++) {
            cols[i] = nir_load_system_value(b, sysval_op, i,
                                            intrin->dest.ssa.num_components,
                                            intrin->dest.ssa.bit_size);
            assert(cols[i]->num_components == num_rows);
         }
         return nir_select_from_ssa_def_array(b, cols, num_cols, column);
      } else {
         return nir_load_system_value(b, sysval_op, 0,
                                      intrin->dest.ssa.num_components,
                                      intrin->dest.ssa.bit_size);
      }
   }

   default:
      return NULL;
   }
}

bool
nir_lower_system_values(nir_shader *shader)
{
   bool progress = nir_shader_lower_instructions(shader,
                                                 lower_system_value_filter,
                                                 lower_system_value_instr,
                                                 NULL);

   /* We're going to delete the variables so we need to clean up all those
    * derefs we left lying around.
    */
   if (progress)
      nir_remove_dead_derefs(shader);

   nir_foreach_variable_with_modes_safe(var, shader, nir_var_system_value)
      exec_node_remove(&var->node);

   return progress;
}

static bool
lower_compute_system_value_filter(const nir_instr *instr, const void *_options)
{
   return instr->type == nir_instr_type_intrinsic;
}

static nir_ssa_def *
lower_compute_system_value_instr(nir_builder *b,
                                 nir_instr *instr, void *_options)
{
   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   const nir_lower_compute_system_values_options *options = _options;

   /* All the intrinsics we care about are loads */
   if (!nir_intrinsic_infos[intrin->intrinsic].has_dest)
      return NULL;

   assert(intrin->dest.is_ssa);
   const unsigned bit_size = intrin->dest.ssa.bit_size;

   switch (intrin->intrinsic) {
   case nir_intrinsic_load_local_invocation_id:
      /* If lower_cs_local_id_from_index is true, then we derive the local
       * index from the local id.
       */
      if (b->shader->options->lower_cs_local_id_from_index) {
         /* We lower gl_LocalInvocationID from gl_LocalInvocationIndex based
          * on this formula:
          *
          *    gl_LocalInvocationID.x =
          *       gl_LocalInvocationIndex % gl_WorkGroupSize.x;
          *    gl_LocalInvocationID.y =
          *       (gl_LocalInvocationIndex / gl_WorkGroupSize.x) %
          *       gl_WorkGroupSize.y;
          *    gl_LocalInvocationID.z =
          *       (gl_LocalInvocationIndex /
          *        (gl_WorkGroupSize.x * gl_WorkGroupSize.y)) %
          *       gl_WorkGroupSize.z;
          *
          * However, the final % gl_WorkGroupSize.z does nothing unless we
          * accidentally end up with a gl_LocalInvocationIndex that is too
          * large so it can safely be omitted.
          */
         nir_ssa_def *local_index = nir_load_local_invocation_index(b);
         nir_ssa_def *local_size = nir_load_local_group_size(b);

         /* Because no hardware supports a local workgroup size greater than
          * about 1K, this calculation can be done in 32-bit and can save some
          * 64-bit arithmetic.
          */
         nir_ssa_def *id_x, *id_y, *id_z;
         id_x = nir_umod(b, local_index,
                            nir_channel(b, local_size, 0));
         id_y = nir_umod(b, nir_udiv(b, local_index,
                                        nir_channel(b, local_size, 0)),
                            nir_channel(b, local_size, 1));
         id_z = nir_udiv(b, local_index,
                            nir_imul(b, nir_channel(b, local_size, 0),
                                        nir_channel(b, local_size, 1)));
         return nir_u2u(b, nir_vec3(b, id_x, id_y, id_z), bit_size);
      } else {
         return NULL;
      }

   case nir_intrinsic_load_local_invocation_index:
      /* If lower_cs_local_index_from_id is true, then we derive the local
       * index from the local id.
       */
      if (b->shader->options->lower_cs_local_index_from_id) {
         /* From the GLSL man page for gl_LocalInvocationIndex:
          *
          *    "The value of gl_LocalInvocationIndex is equal to
          *    gl_LocalInvocationID.z * gl_WorkGroupSize.x *
          *    gl_WorkGroupSize.y + gl_LocalInvocationID.y *
          *    gl_WorkGroupSize.x + gl_LocalInvocationID.x"
          */
         nir_ssa_def *local_id = nir_load_local_invocation_id(b);

         nir_ssa_def *size_x =
            nir_imm_int(b, b->shader->info.cs.local_size[0]);
         nir_ssa_def *size_y =
            nir_imm_int(b, b->shader->info.cs.local_size[1]);

         /* Because no hardware supports a local workgroup size greater than
          * about 1K, this calculation can be done in 32-bit and can save some
          * 64-bit arithmetic.
          */
         nir_ssa_def *index;
         index = nir_imul(b, nir_channel(b, local_id, 2),
                             nir_imul(b, size_x, size_y));
         index = nir_iadd(b, index,
                             nir_imul(b, nir_channel(b, local_id, 1), size_x));
         index = nir_iadd(b, index, nir_channel(b, local_id, 0));
         return nir_u2u(b, index, bit_size);
      } else {
         return NULL;
      }

   case nir_intrinsic_load_local_group_size:
      if (b->shader->info.cs.local_size_variable) {
         /* If the local work group size is variable it can't be lowered at
          * this point.  We do, however, have to make sure that the intrinsic
          * is only 32-bit.
          */
         return NULL;
      } else {
         /* using a 32 bit constant is safe here as no device/driver needs more
          * than 32 bits for the local size */
         nir_const_value local_size_const[3];
         memset(local_size_const, 0, sizeof(local_size_const));
         local_size_const[0].u32 = b->shader->info.cs.local_size[0];
         local_size_const[1].u32 = b->shader->info.cs.local_size[1];
         local_size_const[2].u32 = b->shader->info.cs.local_size[2];
         return nir_u2u(b, nir_build_imm(b, 3, 32, local_size_const), bit_size);
      }

   case nir_intrinsic_load_global_invocation_id_zero_base: {
      if ((options && options->has_base_work_group_id) ||
          !b->shader->options->has_cs_global_id) {
         nir_ssa_def *group_size = nir_load_local_group_size(b);
         nir_ssa_def *group_id = nir_load_work_group_id(b, bit_size);
         nir_ssa_def *local_id = nir_load_local_invocation_id(b);

         return nir_iadd(b, nir_imul(b, group_id,
                                        nir_u2u(b, group_size, bit_size)),
                            nir_u2u(b, local_id, bit_size));
      } else {
         return NULL;
      }
   }

   case nir_intrinsic_load_global_invocation_id: {
      if (options && options->has_base_global_invocation_id)
         return nir_iadd(b, nir_load_global_invocation_id_zero_base(b, bit_size),
                            nir_load_base_global_invocation_id(b, bit_size));
      else if ((options && options->has_base_work_group_id) ||
               !b->shader->options->has_cs_global_id)
         return nir_load_global_invocation_id_zero_base(b, bit_size);
      else
         return NULL;
   }

   case nir_intrinsic_load_global_invocation_index: {
      /* OpenCL's global_linear_id explicitly removes the global offset before computing this */
      assert(b->shader->info.stage == MESA_SHADER_KERNEL);
      nir_ssa_def *global_base_id = nir_load_base_global_invocation_id(b, bit_size);
      nir_ssa_def *global_id = nir_isub(b, nir_load_global_invocation_id(b, bit_size), global_base_id);
      nir_ssa_def *global_size = build_global_group_size(b, bit_size);

      /* index = id.x + ((id.y + (id.z * size.y)) * size.x) */
      nir_ssa_def *index;
      index = nir_imul(b, nir_channel(b, global_id, 2),
                          nir_channel(b, global_size, 1));
      index = nir_iadd(b, nir_channel(b, global_id, 1), index);
      index = nir_imul(b, nir_channel(b, global_size, 0), index);
      index = nir_iadd(b, nir_channel(b, global_id, 0), index);
      return index;
   }

   case nir_intrinsic_load_work_group_id: {
      if (options && options->has_base_work_group_id)
         return nir_iadd(b, nir_u2u(b, nir_load_work_group_id_zero_base(b), bit_size),
                            nir_load_base_work_group_id(b, bit_size));
      else
         return NULL;
   }

   default:
      return NULL;
   }
}

bool
nir_lower_compute_system_values(nir_shader *shader,
                                const nir_lower_compute_system_values_options *options)
{
   if (shader->info.stage != MESA_SHADER_COMPUTE &&
       shader->info.stage != MESA_SHADER_KERNEL)
      return false;

   return nir_shader_lower_instructions(shader,
                                        lower_compute_system_value_filter,
                                        lower_compute_system_value_instr,
                                        (void*)options);
}
