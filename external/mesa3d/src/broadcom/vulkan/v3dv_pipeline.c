/*
 * Copyright © 2019 Raspberry Pi
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

#include "vk_util.h"

#include "v3dv_debug.h"
#include "v3dv_private.h"

#include "vk_format_info.h"

#include "common/v3d_debug.h"

#include "compiler/nir/nir_builder.h"
#include "nir/nir_serialize.h"

#include "util/u_atomic.h"

#include "vulkan/util/vk_format.h"

#include "broadcom/cle/v3dx_pack.h"

void
v3dv_print_v3d_key(struct v3d_key *key,
                   uint32_t v3d_key_size)
{
   struct mesa_sha1 ctx;
   unsigned char sha1[20];
   char sha1buf[41];

   _mesa_sha1_init(&ctx);

   _mesa_sha1_update(&ctx, key, v3d_key_size);

   _mesa_sha1_final(&ctx, sha1);
   _mesa_sha1_format(sha1buf, sha1);

   fprintf(stderr, "key %p: %s\n", key, sha1buf);
}

VkResult
v3dv_CreateShaderModule(VkDevice _device,
                        const VkShaderModuleCreateInfo *pCreateInfo,
                        const VkAllocationCallbacks *pAllocator,
                        VkShaderModule *pShaderModule)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   struct v3dv_shader_module *module;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
   assert(pCreateInfo->flags == 0);

   module = vk_alloc2(&device->alloc, pAllocator,
                      sizeof(*module) + pCreateInfo->codeSize, 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (module == NULL)
      return vk_error(NULL, VK_ERROR_OUT_OF_HOST_MEMORY);

   module->nir = NULL;

   module->size = pCreateInfo->codeSize;
   memcpy(module->data, pCreateInfo->pCode, module->size);

   _mesa_sha1_compute(module->data, module->size, module->sha1);

   *pShaderModule = v3dv_shader_module_to_handle(module);

   return VK_SUCCESS;
}

void
v3dv_shader_module_internal_init(struct v3dv_shader_module *module,
                                 nir_shader *nir)
{
   module->nir = nir;
   module->size = 0;

   if (nir != NULL) {
      struct blob blob;
      blob_init(&blob);

      nir_serialize(&blob, nir, false);
      if (!blob.out_of_memory)
         _mesa_sha1_compute(blob.data, blob.size, module->sha1);

      blob_finish(&blob);
   }
}

void
v3dv_DestroyShaderModule(VkDevice _device,
                         VkShaderModule _module,
                         const VkAllocationCallbacks *pAllocator)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   V3DV_FROM_HANDLE(v3dv_shader_module, module, _module);

   if (!module)
      return;

   /* NIR modules (which are only created internally by the driver) are not
    * dynamically allocated so we should never call this for them.
    * Instead the driver is responsible for freeing the NIR code when it is
    * no longer needed.
    */
   assert(module->nir == NULL);

   vk_free2(&device->alloc, pAllocator, module);
}

void
v3dv_shader_variant_destroy(struct v3dv_device *device,
                            struct v3dv_shader_variant *variant)
{
   if (variant->assembly_bo)
      v3dv_bo_free(device, variant->assembly_bo);
   ralloc_free(variant->prog_data.base);
   vk_free(&device->alloc, variant);
}

static void
destroy_pipeline_stage(struct v3dv_device *device,
                       struct v3dv_pipeline_stage *p_stage,
                       const VkAllocationCallbacks *pAllocator)
{
   if (!p_stage)
      return;

   ralloc_free(p_stage->nir);
   if (p_stage->current_variant)
      v3dv_shader_variant_unref(device, p_stage->current_variant);
   vk_free2(&device->alloc, pAllocator, p_stage);
}

static void
v3dv_destroy_pipeline(struct v3dv_pipeline *pipeline,
                      struct v3dv_device *device,
                      const VkAllocationCallbacks *pAllocator)
{
   if (!pipeline)
      return;

   /* FIXME: we can't just use a loop over mesa stage due the bin, would be
    * good to find an alternative.
    */
   destroy_pipeline_stage(device, pipeline->vs, pAllocator);
   destroy_pipeline_stage(device, pipeline->vs_bin, pAllocator);
   destroy_pipeline_stage(device, pipeline->fs, pAllocator);
   destroy_pipeline_stage(device, pipeline->cs, pAllocator);

   if (pipeline->spill.bo) {
      assert(pipeline->spill.size_per_thread > 0);
      v3dv_bo_free(device, pipeline->spill.bo);
   }

   if (pipeline->default_attribute_values) {
      v3dv_bo_free(device, pipeline->default_attribute_values);
      pipeline->default_attribute_values = NULL;
   }

   if (pipeline->combined_index_map)
      _mesa_hash_table_destroy(pipeline->combined_index_map, NULL);

   if (pipeline->default_attribute_values)
      v3dv_bo_free(device, pipeline->default_attribute_values);

   vk_free2(&device->alloc, pAllocator, pipeline);
}

void
v3dv_DestroyPipeline(VkDevice _device,
                     VkPipeline _pipeline,
                     const VkAllocationCallbacks *pAllocator)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   V3DV_FROM_HANDLE(v3dv_pipeline, pipeline, _pipeline);

   if (!pipeline)
      return;

   v3dv_destroy_pipeline(pipeline, device, pAllocator);
}

static const struct spirv_to_nir_options default_spirv_options =  {
   .caps = { false },
   .ubo_addr_format = nir_address_format_32bit_index_offset,
   .ssbo_addr_format = nir_address_format_32bit_index_offset,
   .phys_ssbo_addr_format = nir_address_format_64bit_global,
   .push_const_addr_format = nir_address_format_logical,
   .shared_addr_format = nir_address_format_32bit_offset,
   .frag_coord_is_sysval = false,
};

const nir_shader_compiler_options v3dv_nir_options = {
   .lower_all_io_to_temps = true,
   .lower_extract_byte = true,
   .lower_extract_word = true,
   .lower_bitfield_insert_to_shifts = true,
   .lower_bitfield_extract_to_shifts = true,
   .lower_bitfield_reverse = true,
   .lower_bit_count = true,
   .lower_cs_local_id_from_index = true,
   .lower_ffract = true,
   .lower_fmod = true,
   .lower_pack_unorm_2x16 = true,
   .lower_pack_snorm_2x16 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_pack_unorm_4x8 = true,
   .lower_pack_snorm_4x8 = true,
   .lower_unpack_unorm_4x8 = true,
   .lower_unpack_snorm_4x8 = true,
   .lower_pack_half_2x16 = true,
   .lower_unpack_half_2x16 = true,
   /* FIXME: see if we can avoid the uadd_carry and usub_borrow lowering and
    * get the tests to pass since it might produce slightly better code.
    */
   .lower_uadd_carry = true,
   .lower_usub_borrow = true,
   /* FIXME: check if we can use multop + umul24 to implement mul2x32_64
    * without lowering.
    */
   .lower_mul_2x32_64 = true,
   .lower_fdiv = true,
   .lower_find_lsb = true,
   .lower_ffma16 = true,
   .lower_ffma32 = true,
   .lower_ffma64 = true,
   .lower_flrp32 = true,
   .lower_fpow = true,
   .lower_fsat = true,
   .lower_fsqrt = true,
   .lower_ifind_msb = true,
   .lower_isign = true,
   .lower_ldexp = true,
   .lower_mul_high = true,
   .lower_wpos_pntc = true,
   .lower_rotate = true,
   .lower_to_scalar = true,
   .vertex_id_zero_based = false, /* FIXME: to set this to true, the intrinsic
                                   * needs to be supported */
   .lower_interpolate_at = true,
};

const nir_shader_compiler_options *
v3dv_pipeline_get_nir_options(void)
{
   return &v3dv_nir_options;
}

#define OPT(pass, ...) ({                                  \
   bool this_progress = false;                             \
   NIR_PASS(this_progress, nir, pass, ##__VA_ARGS__);      \
   if (this_progress)                                      \
      progress = true;                                     \
   this_progress;                                          \
})

static void
nir_optimize(nir_shader *nir,
             struct v3dv_pipeline_stage *stage,
             bool allow_copies)
{
   bool progress;

   do {
      progress = false;
      OPT(nir_split_array_vars, nir_var_function_temp);
      OPT(nir_shrink_vec_array_vars, nir_var_function_temp);
      OPT(nir_opt_deref);
      OPT(nir_lower_vars_to_ssa);
      if (allow_copies) {
         /* Only run this pass in the first call to nir_optimize.  Later calls
          * assume that we've lowered away any copy_deref instructions and we
          * don't want to introduce any more.
          */
         OPT(nir_opt_find_array_copies);
      }
      OPT(nir_opt_copy_prop_vars);
      OPT(nir_opt_dead_write_vars);
      OPT(nir_opt_combine_stores, nir_var_all);

      OPT(nir_lower_alu_to_scalar, NULL, NULL);

      OPT(nir_copy_prop);
      OPT(nir_lower_phis_to_scalar);

      OPT(nir_copy_prop);
      OPT(nir_opt_dce);
      OPT(nir_opt_cse);
      OPT(nir_opt_combine_stores, nir_var_all);

      /* Passing 0 to the peephole select pass causes it to convert
       * if-statements that contain only move instructions in the branches
       * regardless of the count.
       *
       * Passing 1 to the peephole select pass causes it to convert
       * if-statements that contain at most a single ALU instruction (total)
       * in both branches.
       */
      OPT(nir_opt_peephole_select, 0, false, false);
      OPT(nir_opt_peephole_select, 8, false, true);

      OPT(nir_opt_intrinsics);
      OPT(nir_opt_idiv_const, 32);
      OPT(nir_opt_algebraic);
      OPT(nir_opt_constant_folding);

      OPT(nir_opt_dead_cf);

      OPT(nir_opt_if, false);
      OPT(nir_opt_conditional_discard);

      OPT(nir_opt_remove_phis);
      OPT(nir_opt_undef);
      OPT(nir_lower_pack);
   } while (progress);

   OPT(nir_remove_dead_variables, nir_var_function_temp, NULL);
}

static void
preprocess_nir(nir_shader *nir,
               struct v3dv_pipeline_stage *stage)
{
   /* Make sure we lower variable initializers on output variables so that
    * nir_remove_dead_variables below sees the corresponding stores
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, nir_var_shader_out);

   /* Now that we've deleted all but the main function, we can go ahead and
    * lower the rest of the variable initializers.
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, ~0);

   /* Split member structs.  We do this before lower_io_to_temporaries so that
    * it doesn't lower system values to temporaries by accident.
    */
   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_split_per_member_structs);

   if (nir->info.stage == MESA_SHADER_FRAGMENT)
      NIR_PASS_V(nir, nir_lower_io_to_vector, nir_var_shader_out);
   if (nir->info.stage == MESA_SHADER_FRAGMENT) {
      NIR_PASS_V(nir, nir_lower_input_attachments,
                 &(nir_input_attachment_options) {
                    .use_fragcoord_sysval = false,
                       });
   }

   NIR_PASS_V(nir, nir_lower_explicit_io,
              nir_var_mem_push_const,
              nir_address_format_32bit_offset);

   NIR_PASS_V(nir, nir_lower_explicit_io,
              nir_var_mem_ubo | nir_var_mem_ssbo,
              nir_address_format_32bit_index_offset);

   NIR_PASS_V(nir, nir_remove_dead_variables, nir_var_shader_in |
              nir_var_shader_out | nir_var_system_value | nir_var_mem_shared,
              NULL);

   NIR_PASS_V(nir, nir_propagate_invariant);
   NIR_PASS_V(nir, nir_lower_io_to_temporaries,
              nir_shader_get_entrypoint(nir), true, false);

   NIR_PASS_V(nir, nir_lower_system_values);
   NIR_PASS_V(nir, nir_lower_clip_cull_distance_arrays);

   NIR_PASS_V(nir, nir_lower_alu_to_scalar, NULL, NULL);

   NIR_PASS_V(nir, nir_normalize_cubemap_coords);

   NIR_PASS_V(nir, nir_lower_global_vars_to_local);

   NIR_PASS_V(nir, nir_split_var_copies);
   NIR_PASS_V(nir, nir_split_struct_vars, nir_var_function_temp);

   nir_optimize(nir, stage, true);

   NIR_PASS_V(nir, nir_lower_load_const_to_scalar);

   /* Lower a bunch of stuff */
   NIR_PASS_V(nir, nir_lower_var_copies);

   NIR_PASS_V(nir, nir_lower_indirect_derefs, nir_var_shader_in |
              nir_var_shader_out |
              nir_var_function_temp, UINT32_MAX);

   NIR_PASS_V(nir, nir_lower_array_deref_of_vec,
              nir_var_mem_ubo | nir_var_mem_ssbo,
              nir_lower_direct_array_deref_of_vec_load);

   NIR_PASS_V(nir, nir_lower_frexp);

   /* Get rid of split copies */
   nir_optimize(nir, stage, false);
}

/* FIXME: This is basically the same code at anv, tu and radv. Move to common
 * place?
 */
static struct nir_spirv_specialization*
vk_spec_info_to_nir_spirv(const VkSpecializationInfo *spec_info,
                          uint32_t *out_num_spec_entries)
{
   if (spec_info == NULL || spec_info->mapEntryCount == 0)
      return NULL;

   uint32_t num_spec_entries = spec_info->mapEntryCount;
   struct nir_spirv_specialization *spec_entries = calloc(num_spec_entries, sizeof(*spec_entries));

   for (uint32_t i = 0; i < num_spec_entries; i++) {
      VkSpecializationMapEntry entry = spec_info->pMapEntries[i];
      const void *data = spec_info->pData + entry.offset;
      assert(data + entry.size <= spec_info->pData + spec_info->dataSize);

      spec_entries[i].id = spec_info->pMapEntries[i].constantID;
      switch (entry.size) {
      case 8:
         spec_entries[i].value.u64 = *(const uint64_t *)data;
         break;
      case 4:
         spec_entries[i].value.u32 = *(const uint32_t *)data;
         break;
      case 2:
         spec_entries[i].value.u16 = *(const uint16_t *)data;
         break;
      case 1:
         spec_entries[i].value.u8 = *(const uint8_t *)data;
         break;
      default:
         assert(!"Invalid spec constant size");
         break;
      }
   }

   *out_num_spec_entries = num_spec_entries;
   return spec_entries;
}

static nir_shader *
shader_module_compile_to_nir(struct v3dv_device *device,
                             struct v3dv_pipeline_stage *stage)
{
   nir_shader *nir;
   const nir_shader_compiler_options *nir_options = &v3dv_nir_options;

   if (!stage->module->nir) {
      uint32_t *spirv = (uint32_t *) stage->module->data;
      assert(stage->module->size % 4 == 0);

      if (V3D_DEBUG & V3D_DEBUG_DUMP_SPIRV)
         v3dv_print_spirv(stage->module->data, stage->module->size, stderr);

      uint32_t num_spec_entries = 0;
      struct nir_spirv_specialization *spec_entries =
         vk_spec_info_to_nir_spirv(stage->spec_info, &num_spec_entries);
      const struct spirv_to_nir_options spirv_options = default_spirv_options;
      nir = spirv_to_nir(spirv, stage->module->size / 4,
                         spec_entries, num_spec_entries,
                         stage->stage, stage->entrypoint,
                         &spirv_options, nir_options);
      nir_validate_shader(nir, "after spirv_to_nir");
      free(spec_entries);
   } else {
      /* For NIR modules created by the driver we can't consume the NIR
       * directly, we need to clone it first, since ownership of the NIR code
       * (as with SPIR-V code for SPIR-V shaders), belongs to the creator
       * of the module and modules can be destroyed immediately after been used
       * to create pipelines.
       */
      nir = nir_shader_clone(NULL, stage->module->nir);
      nir_validate_shader(nir, "nir module");
   }
   assert(nir->info.stage == stage->stage);

   if (V3D_DEBUG & (V3D_DEBUG_NIR |
                    v3d_debug_flag_for_shader_stage(stage->stage))) {
      fprintf(stderr, "Initial form: %s prog %d NIR:\n",
              gl_shader_stage_name(stage->stage),
              stage->program_id);
      nir_print_shader(nir, stderr);
      fprintf(stderr, "\n");
   }

   /* We have to lower away local variable initializers right before we
    * inline functions.  That way they get properly initialized at the top
    * of the function and not at the top of its caller.
    */
   NIR_PASS_V(nir, nir_lower_variable_initializers, nir_var_function_temp);
   NIR_PASS_V(nir, nir_lower_returns);
   NIR_PASS_V(nir, nir_inline_functions);
   NIR_PASS_V(nir, nir_opt_deref);

   /* Pick off the single entrypoint that we want */
   foreach_list_typed_safe(nir_function, func, node, &nir->functions) {
      if (func->is_entrypoint)
         func->name = ralloc_strdup(func, "main");
      else
         exec_node_remove(&func->node);
   }
   assert(exec_list_length(&nir->functions) == 1);

   /* Vulkan uses the separate-shader linking model */
   nir->info.separate_shader = true;

   preprocess_nir(nir, stage);

   return nir;
}

static int
type_size_vec4(const struct glsl_type *type, bool bindless)
{
   return glsl_count_attribute_slots(type, false);
}

static unsigned
descriptor_map_add(struct v3dv_descriptor_map *map,
                   int set,
                   int binding,
                   int array_index,
                   int array_size,
                   bool is_shadow)
{
   assert(array_index < array_size);

   unsigned index = 0;
   for (unsigned i = 0; i < map->num_desc; i++) {
      if (set == map->set[i] &&
          binding == map->binding[i] &&
          array_index == map->array_index[i]) {
         assert(array_size == map->array_size[i]);
         return index;
      }
      index++;
   }

   assert(index == map->num_desc);

   map->set[map->num_desc] = set;
   map->binding[map->num_desc] = binding;
   map->array_index[map->num_desc] = array_index;
   map->array_size[map->num_desc] = array_size;
   map->is_shadow[map->num_desc] = is_shadow;
   map->num_desc++;

   return index;
}


static void
lower_load_push_constant(nir_builder *b, nir_intrinsic_instr *instr,
                         struct v3dv_pipeline *pipeline)
{
   assert(instr->intrinsic == nir_intrinsic_load_push_constant);
   instr->intrinsic = nir_intrinsic_load_uniform;
}

/* Gathers info from the intrinsic (set and binding) and then lowers it so it
 * could be used by the v3d_compiler */
static void
lower_vulkan_resource_index(nir_builder *b,
                            nir_intrinsic_instr *instr,
                            struct v3dv_pipeline *pipeline,
                            const struct v3dv_pipeline_layout *layout)
{
   assert(instr->intrinsic == nir_intrinsic_vulkan_resource_index);

   nir_const_value *const_val = nir_src_as_const_value(instr->src[0]);

   unsigned set = nir_intrinsic_desc_set(instr);
   unsigned binding = nir_intrinsic_binding(instr);
   struct v3dv_descriptor_set_layout *set_layout = layout->set[set].layout;
   struct v3dv_descriptor_set_binding_layout *binding_layout =
      &set_layout->binding[binding];
   unsigned index = 0;

   switch (nir_intrinsic_desc_type(instr)) {
   case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
   case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
      struct v3dv_descriptor_map *descriptor_map =
         nir_intrinsic_desc_type(instr) == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ?
         &pipeline->ubo_map : &pipeline->ssbo_map;

      if (!const_val)
         unreachable("non-constant vulkan_resource_index array index");

      index = descriptor_map_add(descriptor_map, set, binding,
                                 const_val->u32,
                                 binding_layout->array_size,
                                 false /* is_shadow: Doesn't really matter in this case */);

      if (nir_intrinsic_desc_type(instr) == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
         /* skip index 0 which is used for push constants */
         index++;
      }
      break;
   }

   default:
      unreachable("unsupported desc_type for vulkan_resource_index");
      break;
   }

   /* Since we use the deref pass, both vulkan_resource_index and
    * vulkan_load_descriptor returns a vec2. But for the index the backend
    * expect just one scalar (like with get_ssbo_size), so lets return here
    * just it. Then on load_descriptor we would recreate the vec2, keeping the
    * second component (unused right now) to zero.
    */
   nir_ssa_def_rewrite_uses(&instr->dest.ssa,
                            nir_src_for_ssa(nir_imm_int(b, index)));
   nir_instr_remove(&instr->instr);
}

static struct hash_table *
pipeline_ensure_combined_index_map(struct v3dv_pipeline *pipeline)
{
   if (pipeline->combined_index_map == NULL) {
      pipeline->combined_index_map =
         _mesa_hash_table_create(NULL, _mesa_hash_u32, _mesa_key_u32_equal);
      pipeline->next_combined_index = 0;
   }

   assert(pipeline->combined_index_map);

   return pipeline->combined_index_map;
}

static uint32_t
get_combined_index(struct v3dv_pipeline *pipeline,
                   uint32_t texture_index,
                   uint32_t sampler_index)
{
   struct hash_table *ht = pipeline_ensure_combined_index_map(pipeline);
   uint32_t key = v3dv_pipeline_combined_index_key_create(texture_index, sampler_index);
   struct hash_entry *entry = _mesa_hash_table_search(ht, &key);

   if (entry)
      return (uint32_t)(uintptr_t) (entry->data);

   uint32_t new_index = pipeline->next_combined_index;
   pipeline->next_combined_index++;

   pipeline->combined_index_to_key_map[new_index] = key;
   _mesa_hash_table_insert(ht, &pipeline->combined_index_to_key_map[new_index],
                           (void *)(uintptr_t) (new_index));

   return new_index;
}

static void
lower_tex_src_to_offset(nir_builder *b, nir_tex_instr *instr, unsigned src_idx,
                        struct v3dv_pipeline *pipeline,
                        const struct v3dv_pipeline_layout *layout)
{
   nir_ssa_def *index = NULL;
   unsigned base_index = 0;
   unsigned array_elements = 1;
   nir_tex_src *src = &instr->src[src_idx];
   bool is_sampler = src->src_type == nir_tex_src_sampler_deref;

   /* We compute first the offsets */
   nir_deref_instr *deref = nir_instr_as_deref(src->src.ssa->parent_instr);
   while (deref->deref_type != nir_deref_type_var) {
      assert(deref->parent.is_ssa);
      nir_deref_instr *parent =
         nir_instr_as_deref(deref->parent.ssa->parent_instr);

      assert(deref->deref_type == nir_deref_type_array);

      if (nir_src_is_const(deref->arr.index) && index == NULL) {
         /* We're still building a direct index */
         base_index += nir_src_as_uint(deref->arr.index) * array_elements;
      } else {
         if (index == NULL) {
            /* We used to be direct but not anymore */
            index = nir_imm_int(b, base_index);
            base_index = 0;
         }

         index = nir_iadd(b, index,
                          nir_imul(b, nir_imm_int(b, array_elements),
                                   nir_ssa_for_src(b, deref->arr.index, 1)));
      }

      array_elements *= glsl_get_length(parent->type);

      deref = parent;
   }

   if (index)
      index = nir_umin(b, index, nir_imm_int(b, array_elements - 1));

   /* We have the offsets, we apply them, rewriting the source or removing
    * instr if needed
    */
   if (index) {
      nir_instr_rewrite_src(&instr->instr, &src->src,
                            nir_src_for_ssa(index));

      src->src_type = is_sampler ?
         nir_tex_src_sampler_offset :
         nir_tex_src_texture_offset;
   } else {
      nir_tex_instr_remove_src(instr, src_idx);
   }

   uint32_t set = deref->var->data.descriptor_set;
   uint32_t binding = deref->var->data.binding;
   struct v3dv_descriptor_set_layout *set_layout = layout->set[set].layout;
   struct v3dv_descriptor_set_binding_layout *binding_layout =
      &set_layout->binding[binding];

   /* For input attachments, the shader includes the attachment_idx. As we are
    * treating them as a texture, we only want the base_index
    */
   uint32_t array_index = binding_layout->type != VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT ?
      deref->var->data.index + base_index :
      base_index;

   int desc_index =
      descriptor_map_add(is_sampler ?
                         &pipeline->sampler_map : &pipeline->texture_map,
                         deref->var->data.descriptor_set,
                         deref->var->data.binding,
                         array_index,
                         binding_layout->array_size,
                         instr->is_shadow);

   if (is_sampler)
      instr->sampler_index = desc_index;
   else
      instr->texture_index = desc_index;
}

static bool
lower_sampler(nir_builder *b, nir_tex_instr *instr,
              struct v3dv_pipeline *pipeline,
              const struct v3dv_pipeline_layout *layout)
{
   int texture_idx =
      nir_tex_instr_src_index(instr, nir_tex_src_texture_deref);

   if (texture_idx >= 0)
      lower_tex_src_to_offset(b, instr, texture_idx, pipeline, layout);

   int sampler_idx =
      nir_tex_instr_src_index(instr, nir_tex_src_sampler_deref);

   if (sampler_idx >= 0)
      lower_tex_src_to_offset(b, instr, sampler_idx, pipeline, layout);

   if (texture_idx < 0 && sampler_idx < 0)
      return false;

   int combined_index =
      get_combined_index(pipeline,
                         instr->texture_index,
                         sampler_idx < 0 ? V3DV_NO_SAMPLER_IDX : instr->sampler_index);

   instr->texture_index = combined_index;
   instr->sampler_index = combined_index;

   return true;
}

/* FIXME: really similar to lower_tex_src_to_offset, perhaps refactor? */
static void
lower_image_deref(nir_builder *b,
                  nir_intrinsic_instr *instr,
                  struct v3dv_pipeline *pipeline,
                  const struct v3dv_pipeline_layout *layout)
{
   nir_deref_instr *deref = nir_src_as_deref(instr->src[0]);
   nir_ssa_def *index = NULL;
   unsigned array_elements = 1;
   unsigned base_index = 0;

   while (deref->deref_type != nir_deref_type_var) {
      assert(deref->parent.is_ssa);
      nir_deref_instr *parent =
         nir_instr_as_deref(deref->parent.ssa->parent_instr);

      assert(deref->deref_type == nir_deref_type_array);

      if (nir_src_is_const(deref->arr.index) && index == NULL) {
         /* We're still building a direct index */
         base_index += nir_src_as_uint(deref->arr.index) * array_elements;
      } else {
         if (index == NULL) {
            /* We used to be direct but not anymore */
            index = nir_imm_int(b, base_index);
            base_index = 0;
         }

         index = nir_iadd(b, index,
                          nir_imul(b, nir_imm_int(b, array_elements),
                                   nir_ssa_for_src(b, deref->arr.index, 1)));
      }

      array_elements *= glsl_get_length(parent->type);

      deref = parent;
   }

   if (index)
      index = nir_umin(b, index, nir_imm_int(b, array_elements - 1));

   uint32_t set = deref->var->data.descriptor_set;
   uint32_t binding = deref->var->data.binding;
   struct v3dv_descriptor_set_layout *set_layout = layout->set[set].layout;
   struct v3dv_descriptor_set_binding_layout *binding_layout =
      &set_layout->binding[binding];

   uint32_t array_index = deref->var->data.index + base_index;

   assert(binding_layout->type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
          binding_layout->type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER);

   int desc_index =
      descriptor_map_add(&pipeline->texture_map,
                         deref->var->data.descriptor_set,
                         deref->var->data.binding,
                         array_index,
                         binding_layout->array_size,
                         false /* is_shadow: Doesn't really matter in this case */);

   /* We still need to get a combined_index, as we are integrating images with
    * the rest of the texture/sampler support
    */
   int combined_index =
      get_combined_index(pipeline, desc_index, V3DV_NO_SAMPLER_IDX);

   index = nir_imm_int(b, combined_index);

   nir_rewrite_image_intrinsic(instr, index, false);
}

static bool
lower_intrinsic(nir_builder *b, nir_intrinsic_instr *instr,
                struct v3dv_pipeline *pipeline,
                const struct v3dv_pipeline_layout *layout)
{
   switch (instr->intrinsic) {
   case nir_intrinsic_load_layer_id:
      /* FIXME: if layered rendering gets supported, this would need a real
       * lowering
       */
      nir_ssa_def_rewrite_uses(&instr->dest.ssa,
                               nir_src_for_ssa(nir_imm_int(b, 0)));
      nir_instr_remove(&instr->instr);
      return true;

   case nir_intrinsic_load_push_constant:
      lower_load_push_constant(b, instr, pipeline);
      pipeline->use_push_constants = true;
      return true;

   case nir_intrinsic_vulkan_resource_index:
      lower_vulkan_resource_index(b, instr, pipeline, layout);
      return true;

   case nir_intrinsic_load_vulkan_descriptor: {
      /* We are not using it, as loading the descriptor happens as part of the
       * load/store instruction, so the simpler is just doing a no-op. We just
       * lower the desc back to a vec2, as it is what load_ssbo/ubo expects.
       */
      nir_ssa_def *desc = nir_vec2(b, instr->src[0].ssa, nir_imm_int(b, 0));
      nir_ssa_def_rewrite_uses(&instr->dest.ssa, nir_src_for_ssa(desc));
      nir_instr_remove(&instr->instr);
      return true;
   }

   case nir_intrinsic_image_deref_load:
   case nir_intrinsic_image_deref_store:
   case nir_intrinsic_image_deref_atomic_add:
   case nir_intrinsic_image_deref_atomic_imin:
   case nir_intrinsic_image_deref_atomic_umin:
   case nir_intrinsic_image_deref_atomic_imax:
   case nir_intrinsic_image_deref_atomic_umax:
   case nir_intrinsic_image_deref_atomic_and:
   case nir_intrinsic_image_deref_atomic_or:
   case nir_intrinsic_image_deref_atomic_xor:
   case nir_intrinsic_image_deref_atomic_exchange:
   case nir_intrinsic_image_deref_atomic_comp_swap:
   case nir_intrinsic_image_deref_size:
   case nir_intrinsic_image_deref_samples:
      lower_image_deref(b, instr, pipeline, layout);
      return true;

   default:
      return false;
   }
}

static bool
lower_impl(nir_function_impl *impl,
           struct v3dv_pipeline *pipeline,
           const struct v3dv_pipeline_layout *layout)
{
   nir_builder b;
   nir_builder_init(&b, impl);
   bool progress = false;

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         b.cursor = nir_before_instr(instr);
         switch (instr->type) {
         case nir_instr_type_tex:
            progress |=
               lower_sampler(&b, nir_instr_as_tex(instr), pipeline, layout);
            break;
         case nir_instr_type_intrinsic:
            progress |=
               lower_intrinsic(&b, nir_instr_as_intrinsic(instr), pipeline, layout);
            break;
         default:
            break;
         }
      }
   }

   return progress;
}

static bool
lower_pipeline_layout_info(nir_shader *shader,
                           struct v3dv_pipeline *pipeline,
                           const struct v3dv_pipeline_layout *layout)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= lower_impl(function->impl, pipeline, layout);
   }

   return progress;
}


static void
lower_fs_io(nir_shader *nir)
{
   /* Our backend doesn't handle array fragment shader outputs */
   NIR_PASS_V(nir, nir_lower_io_arrays_to_elements_no_indirects, false);
   NIR_PASS_V(nir, nir_remove_dead_variables, nir_var_shader_out, NULL);

   nir_assign_io_var_locations(nir, nir_var_shader_in, &nir->num_inputs,
                               MESA_SHADER_FRAGMENT);

   nir_assign_io_var_locations(nir, nir_var_shader_out, &nir->num_outputs,
                               MESA_SHADER_FRAGMENT);

   NIR_PASS_V(nir, nir_lower_io, nir_var_shader_in | nir_var_shader_out,
              type_size_vec4, 0);
}

static void
lower_vs_io(struct nir_shader *nir)
{
   NIR_PASS_V(nir, nir_lower_io_arrays_to_elements_no_indirects, false);

   nir_assign_io_var_locations(nir, nir_var_shader_in, &nir->num_inputs,
                               MESA_SHADER_VERTEX);

   nir_assign_io_var_locations(nir, nir_var_shader_out, &nir->num_outputs,
                               MESA_SHADER_VERTEX);

   /* FIXME: if we call nir_lower_io, we get a crash later. Likely because it
    * overlaps with v3d_nir_lower_io. Need further research though.
    */
}

static void
shader_debug_output(const char *message, void *data)
{
   /* FIXME: We probably don't want to debug anything extra here, and in fact
    * the compiler is not using this callback too much, only as an alternative
    * way to debug out the shaderdb stats, that you can already get using
    * V3D_DEBUG=shaderdb. Perhaps it would make sense to revisit the v3d
    * compiler to remove that callback.
    */
}

static void
pipeline_populate_v3d_key(struct v3d_key *key,
                          const struct v3dv_pipeline_stage *p_stage,
                          uint32_t ucp_enables,
                          bool robust_buffer_access)
{
   /* The following values are default values used at pipeline create. We use
    * there 16 bit as default return size.
    */

   /* We don't use the nir shader info.num_textures because that doesn't take
    * into account input attachments, even after calling
    * nir_lower_input_attachments. As a general rule that makes sense, but on
    * our case we are handling them mostly as textures. We iterate through the
    * combined_index_map that was filled with the textures sused on th sader.
    */
   uint32_t tex_idx = 0;
   if (p_stage->pipeline->combined_index_map) {
      hash_table_foreach(p_stage->pipeline->combined_index_map, entry) {
         key->tex[tex_idx].swizzle[0] = PIPE_SWIZZLE_X;
         key->tex[tex_idx].swizzle[1] = PIPE_SWIZZLE_Y;
         key->tex[tex_idx].swizzle[2] = PIPE_SWIZZLE_Z;
         key->tex[tex_idx].swizzle[3] = PIPE_SWIZZLE_W;

         key->tex[tex_idx].return_size = 16;
         key->tex[tex_idx].return_channels = 2;

         tex_idx++;
      }
   }
   key->num_tex_used = tex_idx;
   assert(key->num_tex_used <= V3D_MAX_TEXTURE_SAMPLERS);

   /* default value. Would be override on the vs/gs populate methods when GS
    * gets supported
    */
   key->is_last_geometry_stage = true;

   /* Vulkan doesn't have fixed function state for user clip planes. Instead,
    * shaders can write to gl_ClipDistance[], in which case the SPIR-V compiler
    * takes care of adding a single compact array variable at
    * VARYING_SLOT_CLIP_DIST0, so we don't need any user clip plane lowering.
    *
    * The only lowering we are interested is specific to the fragment shader,
    * where we want to emit discards to honor writes to gl_ClipDistance[] in
    * previous stages. This is done via nir_lower_clip_fs() so we only set up
    * the ucp enable mask for that stage.
    */
   key->ucp_enables = ucp_enables;

   key->robust_buffer_access = robust_buffer_access;

   key->environment = V3D_ENVIRONMENT_VULKAN;
}

/* FIXME: anv maps to hw primitive type. Perhaps eventually we would do the
 * same. For not using prim_mode that is the one already used on v3d
 */
static const enum pipe_prim_type vk_to_pipe_prim_type[] = {
   [VK_PRIMITIVE_TOPOLOGY_POINT_LIST] = PIPE_PRIM_POINTS,
   [VK_PRIMITIVE_TOPOLOGY_LINE_LIST] = PIPE_PRIM_LINES,
   [VK_PRIMITIVE_TOPOLOGY_LINE_STRIP] = PIPE_PRIM_LINE_STRIP,
   [VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST] = PIPE_PRIM_TRIANGLES,
   [VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP] = PIPE_PRIM_TRIANGLE_STRIP,
   [VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN] = PIPE_PRIM_TRIANGLE_FAN,
   [VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY] = PIPE_PRIM_LINES_ADJACENCY,
   [VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY] = PIPE_PRIM_LINE_STRIP_ADJACENCY,
   [VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY] = PIPE_PRIM_TRIANGLES_ADJACENCY,
   [VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY] = PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY,
};

static const enum pipe_logicop vk_to_pipe_logicop[] = {
   [VK_LOGIC_OP_CLEAR] = PIPE_LOGICOP_CLEAR,
   [VK_LOGIC_OP_AND] = PIPE_LOGICOP_AND,
   [VK_LOGIC_OP_AND_REVERSE] = PIPE_LOGICOP_AND_REVERSE,
   [VK_LOGIC_OP_COPY] = PIPE_LOGICOP_COPY,
   [VK_LOGIC_OP_AND_INVERTED] = PIPE_LOGICOP_AND_INVERTED,
   [VK_LOGIC_OP_NO_OP] = PIPE_LOGICOP_NOOP,
   [VK_LOGIC_OP_XOR] = PIPE_LOGICOP_XOR,
   [VK_LOGIC_OP_OR] = PIPE_LOGICOP_OR,
   [VK_LOGIC_OP_NOR] = PIPE_LOGICOP_NOR,
   [VK_LOGIC_OP_EQUIVALENT] = PIPE_LOGICOP_EQUIV,
   [VK_LOGIC_OP_INVERT] = PIPE_LOGICOP_INVERT,
   [VK_LOGIC_OP_OR_REVERSE] = PIPE_LOGICOP_OR_REVERSE,
   [VK_LOGIC_OP_COPY_INVERTED] = PIPE_LOGICOP_COPY_INVERTED,
   [VK_LOGIC_OP_OR_INVERTED] = PIPE_LOGICOP_OR_INVERTED,
   [VK_LOGIC_OP_NAND] = PIPE_LOGICOP_NAND,
   [VK_LOGIC_OP_SET] = PIPE_LOGICOP_SET,
};

static void
pipeline_populate_v3d_fs_key(struct v3d_fs_key *key,
                             const VkGraphicsPipelineCreateInfo *pCreateInfo,
                             const struct v3dv_pipeline_stage *p_stage,
                             uint32_t ucp_enables)
{
   memset(key, 0, sizeof(*key));

   const bool rba = p_stage->pipeline->device->features.robustBufferAccess;
   pipeline_populate_v3d_key(&key->base, p_stage, ucp_enables, rba);

   const VkPipelineInputAssemblyStateCreateInfo *ia_info =
      pCreateInfo->pInputAssemblyState;
   uint8_t topology = vk_to_pipe_prim_type[ia_info->topology];

   key->is_points = (topology == PIPE_PRIM_POINTS);
   key->is_lines = (topology >= PIPE_PRIM_LINES &&
                    topology <= PIPE_PRIM_LINE_STRIP);

   /* Vulkan doesn't appear to specify (anv does the same) */
   key->clamp_color = false;

   const VkPipelineColorBlendStateCreateInfo *cb_info =
      pCreateInfo->pColorBlendState;

   key->logicop_func = cb_info && cb_info->logicOpEnable == VK_TRUE ?
                       vk_to_pipe_logicop[cb_info->logicOp] :
                       PIPE_LOGICOP_COPY;

   const bool raster_enabled =
      !pCreateInfo->pRasterizationState->rasterizerDiscardEnable;

   /* Multisample rasterization state must be ignored if rasterization
    * is disabled.
    */
   const VkPipelineMultisampleStateCreateInfo *ms_info =
      raster_enabled ? pCreateInfo->pMultisampleState : NULL;
   if (ms_info) {
      assert(ms_info->rasterizationSamples == VK_SAMPLE_COUNT_1_BIT ||
             ms_info->rasterizationSamples == VK_SAMPLE_COUNT_4_BIT);
      key->msaa = ms_info->rasterizationSamples > VK_SAMPLE_COUNT_1_BIT;

      if (key->msaa) {
         key->sample_coverage =
            p_stage->pipeline->sample_mask != (1 << V3D_MAX_SAMPLES) - 1;
         key->sample_alpha_to_coverage = ms_info->alphaToCoverageEnable;
         key->sample_alpha_to_one = ms_info->alphaToOneEnable;
      }
   }

   /* Vulkan doesn't support alpha test */
   key->alpha_test = false;
   key->alpha_test_func = COMPARE_FUNC_NEVER;

   /* This is intended for V3D versions before 4.1, otherwise we just use the
    * tile buffer load/store swap R/B bit.
    */
   key->swap_color_rb = 0;

   const struct v3dv_render_pass *pass =
      v3dv_render_pass_from_handle(pCreateInfo->renderPass);
   const struct v3dv_subpass *subpass = p_stage->pipeline->subpass;
   for (uint32_t i = 0; i < subpass->color_count; i++) {
      const uint32_t att_idx = subpass->color_attachments[i].attachment;
      if (att_idx == VK_ATTACHMENT_UNUSED)
         continue;

      key->cbufs |= 1 << i;

      VkFormat fb_format = pass->attachments[att_idx].desc.format;
      enum pipe_format fb_pipe_format = vk_format_to_pipe_format(fb_format);

      /* If logic operations are enabled then we might emit color reads and we
       * need to know the color buffer format and swizzle for that
       */
      if (key->logicop_func != PIPE_LOGICOP_COPY) {
         key->color_fmt[i].format = fb_pipe_format;
         key->color_fmt[i].swizzle = v3dv_get_format_swizzle(fb_format);
      }

      const struct util_format_description *desc =
         vk_format_description(fb_format);

      if (desc->channel[0].type == UTIL_FORMAT_TYPE_FLOAT &&
          desc->channel[0].size == 32) {
         key->f32_color_rb |= 1 << i;
      }

      if (p_stage->nir->info.fs.untyped_color_outputs) {
         if (util_format_is_pure_uint(fb_pipe_format))
            key->uint_color_rb |= 1 << i;
         else if (util_format_is_pure_sint(fb_pipe_format))
            key->int_color_rb |= 1 << i;
      }

      if (key->is_points) {
         /* FIXME: The mask would need to be computed based on the shader
          * inputs. On gallium it is done at st_atom_rasterizer
          * (sprite_coord_enable). anv seems (need to confirm) to do that on
          * genX_pipeline (PointSpriteTextureCoordinateEnable). Would be also
          * better to have tests to guide filling the mask.
          */
         key->point_sprite_mask = 0;

         /* Vulkan mandates upper left. */
         key->point_coord_upper_left = true;
      }
   }

   /* FIXME: we understand that this is used on GL to configure fixed-function
    * two side lighting support, and not make sense for Vulkan. Need to
    * confirm though.
    */
   key->light_twoside = false;

   /* FIXME: ditto, although for flat lighting. Again, neet to confirm.*/
   key->shade_model_flat = false;
}

static void
pipeline_populate_v3d_vs_key(struct v3d_vs_key *key,
                             const VkGraphicsPipelineCreateInfo *pCreateInfo,
                             const struct v3dv_pipeline_stage *p_stage)
{
   memset(key, 0, sizeof(*key));

   const bool rba = p_stage->pipeline->device->features.robustBufferAccess;
   pipeline_populate_v3d_key(&key->base, p_stage, 0, rba);

   /* Vulkan doesn't appear to specify (anv does the same) */
   key->clamp_color = false;

   /* Vulkan specifies a point size per vertex, so true for if the prim are
    * points, like on ES2)
    */
   const VkPipelineInputAssemblyStateCreateInfo *ia_info =
      pCreateInfo->pInputAssemblyState;
   uint8_t topology = vk_to_pipe_prim_type[ia_info->topology];

   /* FIXME: not enough to being PRIM_POINTS, on gallium the full check is
    * PIPE_PRIM_POINTS && v3d->rasterizer->base.point_size_per_vertex */
   key->per_vertex_point_size = (topology == PIPE_PRIM_POINTS);

   key->is_coord = p_stage->is_coord;
   if (p_stage->is_coord) {
      /* The only output varying on coord shaders are for transform
       * feedback. Set to 0 as VK_EXT_transform_feedback is not supported.
       */
      key->num_used_outputs = 0;
   } else {
      struct v3dv_pipeline *pipeline = p_stage->pipeline;
      struct v3dv_shader_variant *fs_variant = pipeline->fs->current_variant;

      key->num_used_outputs = fs_variant->prog_data.fs->num_inputs;

      STATIC_ASSERT(sizeof(key->used_outputs) ==
                    sizeof(fs_variant->prog_data.fs->input_slots));
      memcpy(key->used_outputs, fs_variant->prog_data.fs->input_slots,
             sizeof(key->used_outputs));
   }

   const VkPipelineVertexInputStateCreateInfo *vi_info =
      pCreateInfo->pVertexInputState;
   for (uint32_t i = 0; i < vi_info->vertexAttributeDescriptionCount; i++) {
      const VkVertexInputAttributeDescription *desc =
         &vi_info->pVertexAttributeDescriptions[i];
      assert(desc->location < MAX_VERTEX_ATTRIBS);
      if (desc->format == VK_FORMAT_B8G8R8A8_UNORM)
         key->va_swap_rb_mask |= 1 << (VERT_ATTRIB_GENERIC0 + desc->location);
   }
}

/*
 * Creates the pipeline_stage for the coordinate shader. Initially a clone of
 * the vs pipeline_stage, with is_coord to true
 *
 * Returns NULL if it was not able to allocate the object, so it should be
 * handled as a VK_ERROR_OUT_OF_HOST_MEMORY error.
 */
static struct v3dv_pipeline_stage*
pipeline_stage_create_vs_bin(const struct v3dv_pipeline_stage *src,
                             const VkAllocationCallbacks *pAllocator)
{
   struct v3dv_device *device = src->pipeline->device;

   struct v3dv_pipeline_stage *p_stage =
      vk_zalloc2(&device->alloc, pAllocator, sizeof(*p_stage), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (p_stage == NULL)
      return NULL;

   p_stage->pipeline = src->pipeline;
   assert(src->stage == MESA_SHADER_VERTEX);
   p_stage->stage = src->stage;
   p_stage->entrypoint = src->entrypoint;
   p_stage->module = src->module;
   p_stage->nir = nir_shader_clone(NULL, src->nir);
   p_stage->spec_info = src->spec_info;
   memcpy(p_stage->shader_sha1, src->shader_sha1, 20);

   p_stage->is_coord = true;

   return p_stage;
}

/* FIXME: right now this just asks for an bo for the exact size of the qpu
 * assembly. It would be good to be able to re-use bos to avoid bo
 * fragmentation. This could be tricky though, as right now we are uploading
 * the assembly from two paths, when compiling a shader, or when deserializing
 * from the pipeline cache. This also means that the same variant can be
 * shared by different objects. So with the current approach it is clear who
 * owns the assembly bo, but if shared, who owns the shared bo?
 *
 * For now one-bo per-assembly would work.
 *
 * Returns false if it was not able to allocate or map the assembly bo memory.
 */
static bool
upload_assembly(struct v3dv_device *device,
                struct v3dv_shader_variant *variant,
                gl_shader_stage stage,
                bool is_coord,
                const void *data,
                uint32_t size)
{
   const char *name = NULL;
   /* We are uploading the assembly just once, so at this point we shouldn't
    * have any bo
    */
   assert(variant->assembly_bo == NULL);

   switch (stage) {
   case MESA_SHADER_VERTEX:
      name = (is_coord == true) ? "coord_shader_assembly" :
         "vertex_shader_assembly";
      break;
   case MESA_SHADER_FRAGMENT:
      name = "fragment_shader_assembly";
      break;
   case MESA_SHADER_COMPUTE:
      name = "compute_shader_assembly";
      break;
   default:
      unreachable("Stage not supported\n");
      break;
   };

   struct v3dv_bo *bo = v3dv_bo_alloc(device, size, name, true);
   if (!bo) {
      fprintf(stderr, "failed to allocate memory for shader\n");
      return false;
   }

   bool ok = v3dv_bo_map(device, bo, size);
   if (!ok) {
      fprintf(stderr, "failed to map source shader buffer\n");
      return false;
   }

   memcpy(bo->map, data, size);

   /* We don't unmap the assembly bo, as we would use to gather the assembly
    * when serializing the variant.
    */
   variant->assembly_bo = bo;

   return true;
}

static void
pipeline_hash_variant(const struct v3dv_pipeline_stage *p_stage,
                      struct v3d_key *key,
                      size_t key_size,
                      unsigned char *sha1_out)
{
   struct mesa_sha1 ctx;
   struct v3dv_pipeline *pipeline = p_stage->pipeline;
   _mesa_sha1_init(&ctx);

   if (p_stage->stage == MESA_SHADER_COMPUTE) {
      _mesa_sha1_update(&ctx, p_stage->shader_sha1, sizeof(p_stage->shader_sha1));
   } else {
      /* We need to include both on the sha1 key as one could affect the other
       * during linking (like if vertex output are constants, then the
       * fragment shader would load_const intead of load_input). An
       * alternative would be to use the serialized nir, but that seems like
       * an overkill
       */
      _mesa_sha1_update(&ctx, pipeline->vs->shader_sha1,
                        sizeof(pipeline->vs->shader_sha1));
      _mesa_sha1_update(&ctx, pipeline->fs->shader_sha1,
                        sizeof(pipeline->fs->shader_sha1));
   }
   _mesa_sha1_update(&ctx, key, key_size);

   _mesa_sha1_final(&ctx, sha1_out);
}

/* Checks that the pipeline has enough spill size to use a specific variant */
static void
pipeline_check_spill_size(struct v3dv_pipeline *pipeline,
                          struct v3dv_shader_variant *variant)
{
   if (variant->prog_data.base->spill_size > pipeline->spill.size_per_thread) {
      struct v3dv_device *device = pipeline->device;

      /* The TIDX register we use for choosing the area to access
       * for scratch space is: (core << 6) | (qpu << 2) | thread.
       * Even at minimum threadcount in a particular shader, that
       * means we still multiply by qpus by 4.
       */
      const uint32_t total_spill_size =
         4 * device->devinfo.qpu_count * variant->prog_data.base->spill_size;
      if (pipeline->spill.bo) {
         assert(pipeline->spill.size_per_thread > 0);
         v3dv_bo_free(device, pipeline->spill.bo);
      }
      pipeline->spill.bo =
         v3dv_bo_alloc(device, total_spill_size, "spill", true);
      pipeline->spill.size_per_thread = variant->prog_data.base->spill_size;
   }
}

/*
 * Creates a new shader_variant_create. Note that for prog_data is const, so
 * it is used only to copy to their own prog_data
 *
 * Creation includes allocating a shader source bo, and filling it up.
 */
struct v3dv_shader_variant *
v3dv_shader_variant_create(struct v3dv_device *device,
                           gl_shader_stage stage,
                           bool is_coord,
                           const unsigned char *variant_sha1,
                           const struct v3d_key *key,
                           uint32_t key_size,
                           struct v3d_prog_data *prog_data,
                           uint32_t prog_data_size,
                           const uint64_t *qpu_insts,
                           uint32_t qpu_insts_size,
                           VkResult *out_vk_result)
{
   struct v3dv_shader_variant *variant =
      vk_zalloc(&device->alloc, sizeof(*variant), 8,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

   if (variant == NULL) {
      *out_vk_result = VK_ERROR_OUT_OF_HOST_MEMORY;
      return NULL;
   }

   variant->ref_cnt = 1;
   variant->stage = stage;
   variant->is_coord = is_coord;
   memcpy(&variant->key, key, key_size);
   variant->v3d_key_size = key_size;
   memcpy(variant->variant_sha1, variant_sha1, sizeof(variant->variant_sha1));
   variant->prog_data_size = prog_data_size;
   variant->prog_data.base = prog_data;

   if (qpu_insts) {
      if (!upload_assembly(device, variant, stage, is_coord,
                           qpu_insts, qpu_insts_size)) {
         ralloc_free(variant->prog_data.base);
         vk_free(&device->alloc, variant);

         *out_vk_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
         return NULL;
      }
      variant->qpu_insts_size = qpu_insts_size;
   }

   *out_vk_result = VK_SUCCESS;

   return variant;
}

/* For a given key, it returns the compiled version of the shader. If it was
 * already compiled, it gets it from the p_stage cache, if not it compiles is
 * through the v3d compiler
 *
 * If the method returns NULL it means that it was not able to allocate the
 * resources for the variant. out_vk_result would return which OOM applies.
 *
 * Returns a new reference of the shader_variant to the caller.
 */
struct v3dv_shader_variant*
v3dv_get_shader_variant(struct v3dv_pipeline_stage *p_stage,
                        struct v3dv_pipeline_cache *cache,
                        struct v3d_key *key,
                        size_t key_size,
                        const VkAllocationCallbacks *pAllocator,
                        VkResult *out_vk_result)
{
   /* First we check if the current pipeline variant is such variant. For this
    * we can just use the v3d_key
    */

   if (p_stage->current_variant &&
       memcmp(key, &p_stage->current_variant->key, key_size) == 0) {
      *out_vk_result = VK_SUCCESS;
      return p_stage->current_variant;
   }

   /* We search on the pipeline cache if provided by the user, or the default
    * one
    */
   unsigned char variant_sha1[20];
   pipeline_hash_variant(p_stage, key, key_size, variant_sha1);

   struct v3dv_pipeline *pipeline = p_stage->pipeline;
   struct v3dv_device *device = pipeline->device;
   if (cache == NULL && device->instance->default_pipeline_cache_enabled)
       cache = &device->default_pipeline_cache;

   struct v3dv_shader_variant *variant =
      v3dv_pipeline_cache_search_for_variant(pipeline,
                                             cache,
                                             variant_sha1);

   if (variant) {
      pipeline_check_spill_size(pipeline, variant);
      *out_vk_result = VK_SUCCESS;
      return variant;
   }
   /* If we don't find the variant in any cache, we compile one and add the
    * variant to the cache
    */
   struct v3dv_physical_device *physical_device =
      &pipeline->device->instance->physicalDevice;
   const struct v3d_compiler *compiler = physical_device->compiler;

   uint32_t variant_id = p_atomic_inc_return(&p_stage->compiled_variant_count);

   if (V3D_DEBUG & (V3D_DEBUG_NIR |
                    v3d_debug_flag_for_shader_stage(p_stage->stage))) {
      fprintf(stderr, "Just before v3d_compile: %s prog %d variant %d NIR:\n",
              gl_shader_stage_name(p_stage->stage),
              p_stage->program_id,
              variant_id);
      nir_print_shader(p_stage->nir, stderr);
      fprintf(stderr, "\n");
   }

   uint64_t *qpu_insts;
   uint32_t qpu_insts_size;
   struct v3d_prog_data *prog_data;

   qpu_insts = v3d_compile(compiler,
                           key, &prog_data,
                           p_stage->nir,
                           shader_debug_output, NULL,
                           p_stage->program_id,
                           variant_id,
                           &qpu_insts_size);

   if (!qpu_insts) {
      fprintf(stderr, "Failed to compile %s prog %d NIR to VIR\n",
              gl_shader_stage_name(p_stage->stage),
              p_stage->program_id);
   }

   variant = v3dv_shader_variant_create(device, p_stage->stage, p_stage->is_coord,
                                        variant_sha1,
                                        key, key_size,
                                        prog_data, v3d_prog_data_size(p_stage->stage),
                                        qpu_insts, qpu_insts_size,
                                        out_vk_result);
   if (qpu_insts)
      free(qpu_insts);

   if (variant)
      pipeline_check_spill_size(pipeline, variant);

   if (*out_vk_result == VK_SUCCESS) {
      struct v3dv_pipeline_cache *default_cache =
         &pipeline->device->default_pipeline_cache;

      v3dv_pipeline_cache_upload_variant(pipeline, cache, variant);

      /* Ensure that the NIR shader is on the default cache, as cmd_buffer could
       * need to change the current variant.
       */
      if (default_cache != cache) {
         v3dv_pipeline_cache_upload_variant(pipeline, default_cache, variant);
      }
   }

   return variant;
}

/* This methods updates the return size for a given key. It assumes that it
 * was already properly populated. So for example values for key->num_tex_used
 * should be correct at this point
 *
 * Note that even the @return_size to set is 32bit, it could be overriden to
 * 16bit, like for shadow textures, that we know in advance that they are
 * always 16bit.
 */
void
v3d_key_update_return_size(struct v3dv_pipeline *pipeline,
                           struct v3d_key *key,
                           uint32_t return_size)
{
   assert(return_size == 32 || return_size == 16);
   struct v3dv_descriptor_map *texture_map = &pipeline->texture_map;

   for (uint32_t tex_idx = 0; tex_idx < key->num_tex_used; tex_idx++) {
      key->tex[tex_idx].return_size =
         texture_map->is_shadow[tex_idx] ? 16 : return_size;

      key->tex[tex_idx].return_channels =
         key->tex[tex_idx].return_size == 16 ? 2 : 4;
   }
}

/*
 * To avoid needed too many shader re-compilation after pipeline creation
 * time, we pre-generate several options, so they are available on the default
 * cache. The poster boy here is return size for texture acceses, as the real
 * value needed would depend on the texture format used.
 */
static struct v3dv_shader_variant*
pregenerate_shader_variants(struct v3dv_pipeline_stage *p_stage,
                            struct v3dv_pipeline_cache *cache,
                            struct v3d_key *key,
                            size_t key_size,
                            const VkAllocationCallbacks *pAllocator,
                            VkResult *out_vk_result)
{
   /* We assume that we receive the default 16 return size*/
   struct v3dv_shader_variant *variant_16 =
      v3dv_get_shader_variant(p_stage, cache, key, key_size,
                              pAllocator, out_vk_result);

   if (*out_vk_result != VK_SUCCESS)
      return variant_16;

   if (!p_stage->pipeline->device->instance->default_pipeline_cache_enabled) {
      /* If pipeline cache is disabled it doesn't make sense to pre-generate,
       * as we are relying on the default pipeline cache to save the different
       * pre-compiled variants
       */
      return variant_16;
   }

   v3d_key_update_return_size(p_stage->pipeline, key, 32);

   struct v3dv_shader_variant *variant_32 =
      v3dv_get_shader_variant(p_stage, cache, key, key_size,
                              pAllocator, out_vk_result);

   /* get_shader_variant returns a new ref, so as we are going to use
    * variant_16, we need to unref this.
    */
   v3dv_shader_variant_unref(p_stage->pipeline->device, variant_32);

   return variant_16;
}

/* FIXME: C&P from st, common place? */
static void
st_nir_opts(nir_shader *nir)
{
   bool progress;

   do {
      progress = false;

      NIR_PASS_V(nir, nir_lower_vars_to_ssa);

      /* Linking deals with unused inputs/outputs, but here we can remove
       * things local to the shader in the hopes that we can cleanup other
       * things. This pass will also remove variables with only stores, so we
       * might be able to make progress after it.
       */
      NIR_PASS(progress, nir, nir_remove_dead_variables,
               (nir_variable_mode)(nir_var_function_temp |
                                   nir_var_shader_temp |
                                   nir_var_mem_shared),
               NULL);

      NIR_PASS(progress, nir, nir_opt_copy_prop_vars);
      NIR_PASS(progress, nir, nir_opt_dead_write_vars);

      if (nir->options->lower_to_scalar) {
         NIR_PASS_V(nir, nir_lower_alu_to_scalar, NULL, NULL);
         NIR_PASS_V(nir, nir_lower_phis_to_scalar);
      }

      NIR_PASS_V(nir, nir_lower_alu);
      NIR_PASS_V(nir, nir_lower_pack);
      NIR_PASS(progress, nir, nir_copy_prop);
      NIR_PASS(progress, nir, nir_opt_remove_phis);
      NIR_PASS(progress, nir, nir_opt_dce);
      if (nir_opt_trivial_continues(nir)) {
         progress = true;
         NIR_PASS(progress, nir, nir_copy_prop);
         NIR_PASS(progress, nir, nir_opt_dce);
      }
      NIR_PASS(progress, nir, nir_opt_if, false);
      NIR_PASS(progress, nir, nir_opt_dead_cf);
      NIR_PASS(progress, nir, nir_opt_cse);
      NIR_PASS(progress, nir, nir_opt_peephole_select, 8, true, true);

      NIR_PASS(progress, nir, nir_opt_algebraic);
      NIR_PASS(progress, nir, nir_opt_constant_folding);

      NIR_PASS(progress, nir, nir_opt_undef);
      NIR_PASS(progress, nir, nir_opt_conditional_discard);
   } while (progress);
}

static void
link_shaders(nir_shader *producer, nir_shader *consumer)
{
   assert(producer);
   assert(consumer);

   if (producer->options->lower_to_scalar) {
      NIR_PASS_V(producer, nir_lower_io_to_scalar_early, nir_var_shader_out);
      NIR_PASS_V(consumer, nir_lower_io_to_scalar_early, nir_var_shader_in);
   }

   nir_lower_io_arrays_to_elements(producer, consumer);

   st_nir_opts(producer);
   st_nir_opts(consumer);

   if (nir_link_opt_varyings(producer, consumer))
      st_nir_opts(consumer);

   NIR_PASS_V(producer, nir_remove_dead_variables, nir_var_shader_out, NULL);
   NIR_PASS_V(consumer, nir_remove_dead_variables, nir_var_shader_in, NULL);

   if (nir_remove_unused_varyings(producer, consumer)) {
      NIR_PASS_V(producer, nir_lower_global_vars_to_local);
      NIR_PASS_V(consumer, nir_lower_global_vars_to_local);

      st_nir_opts(producer);
      st_nir_opts(consumer);

      /* Optimizations can cause varyings to become unused.
       * nir_compact_varyings() depends on all dead varyings being removed so
       * we need to call nir_remove_dead_variables() again here.
       */
      NIR_PASS_V(producer, nir_remove_dead_variables, nir_var_shader_out, NULL);
      NIR_PASS_V(consumer, nir_remove_dead_variables, nir_var_shader_in, NULL);
   }
}

static void
pipeline_lower_nir(struct v3dv_pipeline *pipeline,
                   struct v3dv_pipeline_stage *p_stage,
                   struct v3dv_pipeline_layout *layout)
{
   nir_shader_gather_info(p_stage->nir, nir_shader_get_entrypoint(p_stage->nir));

   /* Apply the actual pipeline layout to UBOs, SSBOs, and textures */
   NIR_PASS_V(p_stage->nir, lower_pipeline_layout_info, pipeline, layout);
}

/**
 * The SPIR-V compiler will insert a sized compact array for
 * VARYING_SLOT_CLIP_DIST0 if the vertex shader writes to gl_ClipDistance[],
 * where the size of the array determines the number of active clip planes.
 */
static uint32_t
get_ucp_enable_mask(struct v3dv_pipeline_stage *p_stage)
{
   assert(p_stage->stage == MESA_SHADER_VERTEX);
   const nir_shader *shader = p_stage->nir;
   assert(shader);

   nir_foreach_variable_with_modes(var, shader, nir_var_shader_out) {
      if (var->data.location == VARYING_SLOT_CLIP_DIST0) {
         assert(var->data.compact);
         return (1 << glsl_get_length(var->type)) - 1;
      }
   }
   return 0;
}

static nir_shader*
pipeline_stage_get_nir(struct v3dv_pipeline_stage *p_stage,
                       struct v3dv_pipeline *pipeline,
                       struct v3dv_pipeline_cache *cache)
{
   nir_shader *nir = NULL;

   nir = v3dv_pipeline_cache_search_for_nir(pipeline, cache,
                                            &v3dv_nir_options,
                                            p_stage->shader_sha1);

   if (nir) {
      assert(nir->info.stage == p_stage->stage);
      return nir;
   }

   nir = shader_module_compile_to_nir(pipeline->device, p_stage);

   if (nir) {
      struct v3dv_pipeline_cache *default_cache =
         &pipeline->device->default_pipeline_cache;

      v3dv_pipeline_cache_upload_nir(pipeline, cache, nir,
                                     p_stage->shader_sha1);

      /* Ensure that the variant is on the default cache, as cmd_buffer could
       * need to change the current variant
       */
      if (default_cache != cache) {
         v3dv_pipeline_cache_upload_nir(pipeline, default_cache, nir,
                                        p_stage->shader_sha1);
      }
      return nir;
   }

   /* FIXME: this shouldn't happen, raise error? */
   return NULL;
}

static void
pipeline_hash_shader(const struct v3dv_shader_module *module,
                     const char *entrypoint,
                     gl_shader_stage stage,
                     const VkSpecializationInfo *spec_info,
                     unsigned char *sha1_out)
{
   struct mesa_sha1 ctx;
   _mesa_sha1_init(&ctx);

   _mesa_sha1_update(&ctx, module->sha1, sizeof(module->sha1));
   _mesa_sha1_update(&ctx, entrypoint, strlen(entrypoint));
   _mesa_sha1_update(&ctx, &stage, sizeof(stage));
   if (spec_info) {
      _mesa_sha1_update(&ctx, spec_info->pMapEntries,
                        spec_info->mapEntryCount *
                        sizeof(*spec_info->pMapEntries));
      _mesa_sha1_update(&ctx, spec_info->pData,
                        spec_info->dataSize);
   }

   _mesa_sha1_final(&ctx, sha1_out);
}


static VkResult
pipeline_compile_vertex_shader(struct v3dv_pipeline *pipeline,
                               struct v3dv_pipeline_cache *cache,
                               const VkGraphicsPipelineCreateInfo *pCreateInfo,
                               const VkAllocationCallbacks *pAllocator)
{
   struct v3dv_pipeline_stage *p_stage = pipeline->vs;

   pipeline_lower_nir(pipeline, p_stage, pipeline->layout);
   /* Right now we only support pipelines with both vertex and fragment
    * shader.
    */
   assert(pipeline->fs);

   /* Make sure we do all our common lowering *before* we create the vs
    * and vs_bin pipeline stages, since from that point forward we need to
    * run lowerings for both of them separately, since each stage will
    * own its NIR code.
    */
   lower_vs_io(p_stage->nir);

   pipeline->vs_bin = pipeline_stage_create_vs_bin(pipeline->vs, pAllocator);
   if (pipeline->vs_bin == NULL)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   /* FIXME: likely this to be moved to a gather info method to a full
    * struct inside pipeline_stage
    */
   const VkPipelineInputAssemblyStateCreateInfo *ia_info =
      pCreateInfo->pInputAssemblyState;
   pipeline->vs->topology = vk_to_pipe_prim_type[ia_info->topology];

   struct v3d_vs_key *key = &pipeline->vs->key.vs;
   pipeline_populate_v3d_vs_key(key, pCreateInfo, pipeline->vs);
   VkResult vk_result;
   pipeline->vs->current_variant =
      pregenerate_shader_variants(pipeline->vs, cache, &key->base, sizeof(*key),
                                  pAllocator, &vk_result);
   if (vk_result != VK_SUCCESS)
      return vk_result;

   key = &pipeline->vs_bin->key.vs;
   pipeline_populate_v3d_vs_key(key, pCreateInfo, pipeline->vs_bin);
   pipeline->vs_bin->current_variant =
      pregenerate_shader_variants(pipeline->vs_bin, cache, &key->base, sizeof(*key),
                                  pAllocator, &vk_result);

   return vk_result;
}

static VkResult
pipeline_compile_fragment_shader(struct v3dv_pipeline *pipeline,
                                 struct v3dv_pipeline_cache *cache,
                                 const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator)
{
   struct v3dv_pipeline_stage *p_stage = pipeline->vs;

   p_stage = pipeline->fs;
   pipeline_lower_nir(pipeline, p_stage, pipeline->layout);

   struct v3d_fs_key *key = &p_stage->key.fs;

   pipeline_populate_v3d_fs_key(key, pCreateInfo, p_stage,
                                get_ucp_enable_mask(pipeline->vs));

   lower_fs_io(p_stage->nir);

   VkResult vk_result;
   p_stage->current_variant =
      pregenerate_shader_variants(p_stage, cache, &key->base, sizeof(*key),
                                  pAllocator, &vk_result);

   return vk_result;
}

/*
 * It compiles a pipeline. Note that it also allocate internal object, but if
 * some allocations success, but other fails, the method is not freeing the
 * successful ones.
 *
 * This is done to simplify the code, as what we do in this case is just call
 * the pipeline destroy method, and this would handle freeing the internal
 * objects allocated. We just need to be careful setting to NULL the objects
 * not allocated.
 */
static VkResult
pipeline_compile_graphics(struct v3dv_pipeline *pipeline,
                          struct v3dv_pipeline_cache *cache,
                          const VkGraphicsPipelineCreateInfo *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator)
{
   struct v3dv_device *device = pipeline->device;
   struct v3dv_physical_device *physical_device =
      &device->instance->physicalDevice;

   /* First pass to get the the common info from the shader and the nir
    * shader. We don't care of the coord shader for now.
    */
   for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
      const VkPipelineShaderStageCreateInfo *sinfo = &pCreateInfo->pStages[i];
      gl_shader_stage stage = vk_to_mesa_shader_stage(sinfo->stage);

      struct v3dv_pipeline_stage *p_stage =
         vk_zalloc2(&device->alloc, pAllocator, sizeof(*p_stage), 8,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

      if (p_stage == NULL)
         return VK_ERROR_OUT_OF_HOST_MEMORY;

      /* Note that we are assigning program_id slightly differently that
       * v3d. Here we are assigning one per pipeline stage, so vs and vs_bin
       * would have a different program_id, while v3d would have the same for
       * both. For the case of v3dv, it is more natural to have an id this way,
       * as right now we are using it for debugging, not for shader-db.
       */
      p_stage->program_id =
         p_atomic_inc_return(&physical_device->next_program_id);
      p_stage->compiled_variant_count = 0;

      p_stage->pipeline = pipeline;
      p_stage->stage = stage;
      if (stage == MESA_SHADER_VERTEX)
         p_stage->is_coord = false;
      p_stage->entrypoint = sinfo->pName;
      p_stage->module = v3dv_shader_module_from_handle(sinfo->module);
      p_stage->spec_info = sinfo->pSpecializationInfo;

      pipeline_hash_shader(p_stage->module,
                           p_stage->entrypoint,
                           stage,
                           p_stage->spec_info,
                           p_stage->shader_sha1);

      pipeline->active_stages |= sinfo->stage;

      p_stage->nir = pipeline_stage_get_nir(p_stage, pipeline, cache);

      switch(stage) {
      case MESA_SHADER_VERTEX:
         pipeline->vs = p_stage;
         break;
      case MESA_SHADER_FRAGMENT:
         pipeline->fs = p_stage;
         break;
      default:
         unreachable("not supported shader stage");
      }
   }

   /* Add a no-op fragment shader if needed */
   if (!pipeline->fs) {
      nir_builder b;
      nir_builder_init_simple_shader(&b, NULL, MESA_SHADER_FRAGMENT,
                                     &v3dv_nir_options);
      b.shader->info.name = ralloc_strdup(b.shader, "noop_fs");

      struct v3dv_pipeline_stage *p_stage =
         vk_zalloc2(&device->alloc, pAllocator, sizeof(*p_stage), 8,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

      if (p_stage == NULL)
         return VK_ERROR_OUT_OF_HOST_MEMORY;

      p_stage->pipeline = pipeline;
      p_stage->stage = MESA_SHADER_FRAGMENT;
      p_stage->entrypoint = "main";
      p_stage->module = 0;
      p_stage->nir = b.shader;
      /* The no-op shader is always the same, so we can just create the sha1
       * using the name
       */
      _mesa_sha1_compute(b.shader->info.name, strlen(b.shader->info.name),
                         p_stage->shader_sha1);

      p_stage->program_id =
         p_atomic_inc_return(&physical_device->next_program_id);
      p_stage->compiled_variant_count = 0;

      pipeline->fs = p_stage;
      pipeline->active_stages |= MESA_SHADER_FRAGMENT;
   }

   /* Linking */
   link_shaders(pipeline->vs->nir, pipeline->fs->nir);

   /* Compiling to vir (or getting it from a cache);
    */
   VkResult vk_result;
   vk_result = pipeline_compile_fragment_shader(pipeline, cache,
                                                pCreateInfo, pAllocator);
   if (vk_result != VK_SUCCESS)
      return vk_result;

   vk_result = pipeline_compile_vertex_shader(pipeline, cache,
                                              pCreateInfo, pAllocator);
   if (vk_result != VK_SUCCESS)
      return vk_result;

   /* FIXME: values below are default when non-GS is available. Would need to
    * provide real values if GS gets supported
    */
   pipeline->vpm_cfg_bin.As = 1;
   pipeline->vpm_cfg_bin.Ve = 0;
   pipeline->vpm_cfg_bin.Vc =
      pipeline->vs_bin->current_variant->prog_data.vs->vcm_cache_size;

   pipeline->vpm_cfg.As = 1;
   pipeline->vpm_cfg.Ve = 0;
   pipeline->vpm_cfg.Vc =
      pipeline->vs->current_variant->prog_data.vs->vcm_cache_size;

   return VK_SUCCESS;
}

static unsigned
v3dv_dynamic_state_mask(VkDynamicState state)
{
   switch(state) {
   case VK_DYNAMIC_STATE_VIEWPORT:
      return V3DV_DYNAMIC_VIEWPORT;
   case VK_DYNAMIC_STATE_SCISSOR:
      return V3DV_DYNAMIC_SCISSOR;
   case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
      return V3DV_DYNAMIC_STENCIL_COMPARE_MASK;
   case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
      return V3DV_DYNAMIC_STENCIL_WRITE_MASK;
   case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
      return V3DV_DYNAMIC_STENCIL_REFERENCE;
   case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
      return V3DV_DYNAMIC_BLEND_CONSTANTS;
   case VK_DYNAMIC_STATE_DEPTH_BIAS:
      return V3DV_DYNAMIC_DEPTH_BIAS;
   case VK_DYNAMIC_STATE_LINE_WIDTH:
      return V3DV_DYNAMIC_LINE_WIDTH;

   /* Depth bounds testing is not available in in V3D 4.2 so here we are just
    * ignoring this dynamic state. We are already asserting at pipeline creation
    * time that depth bounds testing is not enabled.
    */
   case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
      return 0;

   default:
      unreachable("Unhandled dynamic state");
   }
}

static void
pipeline_init_dynamic_state(
   struct v3dv_pipeline *pipeline,
   const VkPipelineDynamicStateCreateInfo *pDynamicState,
   const VkPipelineViewportStateCreateInfo *pViewportState,
   const VkPipelineDepthStencilStateCreateInfo *pDepthStencilState,
   const VkPipelineColorBlendStateCreateInfo *pColorBlendState,
   const VkPipelineRasterizationStateCreateInfo *pRasterizationState)
{
   pipeline->dynamic_state = default_dynamic_state;
   struct v3dv_dynamic_state *dynamic = &pipeline->dynamic_state;

   /* Create a mask of enabled dynamic states */
   uint32_t dynamic_states = 0;
   if (pDynamicState) {
      uint32_t count = pDynamicState->dynamicStateCount;
      for (uint32_t s = 0; s < count; s++) {
         dynamic_states |=
            v3dv_dynamic_state_mask(pDynamicState->pDynamicStates[s]);
      }
   }

   /* For any pipeline states that are not dynamic, set the dynamic state
    * from the static pipeline state.
    */
   if (pViewportState) {
      if (!(dynamic_states & V3DV_DYNAMIC_VIEWPORT)) {
         dynamic->viewport.count = pViewportState->viewportCount;
         typed_memcpy(dynamic->viewport.viewports, pViewportState->pViewports,
                      pViewportState->viewportCount);

         for (uint32_t i = 0; i < dynamic->viewport.count; i++) {
            v3dv_viewport_compute_xform(&dynamic->viewport.viewports[i],
                                        dynamic->viewport.scale[i],
                                        dynamic->viewport.translate[i]);
         }
      }

      if (!(dynamic_states & V3DV_DYNAMIC_SCISSOR)) {
         dynamic->scissor.count = pViewportState->scissorCount;
         typed_memcpy(dynamic->scissor.scissors, pViewportState->pScissors,
                      pViewportState->scissorCount);
      }
   }

   if (pDepthStencilState) {
      if (!(dynamic_states & V3DV_DYNAMIC_STENCIL_COMPARE_MASK)) {
         dynamic->stencil_compare_mask.front =
            pDepthStencilState->front.compareMask;
         dynamic->stencil_compare_mask.back =
            pDepthStencilState->back.compareMask;
      }

      if (!(dynamic_states & V3DV_DYNAMIC_STENCIL_WRITE_MASK)) {
         dynamic->stencil_write_mask.front = pDepthStencilState->front.writeMask;
         dynamic->stencil_write_mask.back = pDepthStencilState->back.writeMask;
      }

      if (!(dynamic_states & V3DV_DYNAMIC_STENCIL_REFERENCE)) {
         dynamic->stencil_reference.front = pDepthStencilState->front.reference;
         dynamic->stencil_reference.back = pDepthStencilState->back.reference;
      }
   }

   if (pColorBlendState && !(dynamic_states & V3DV_DYNAMIC_BLEND_CONSTANTS)) {
      memcpy(dynamic->blend_constants, pColorBlendState->blendConstants,
             sizeof(dynamic->blend_constants));
   }

   if (pRasterizationState) {
      if (pRasterizationState->depthBiasEnable &&
          !(dynamic_states & V3DV_DYNAMIC_DEPTH_BIAS)) {
         dynamic->depth_bias.constant_factor =
            pRasterizationState->depthBiasConstantFactor;
         dynamic->depth_bias.slope_factor =
            pRasterizationState->depthBiasSlopeFactor;
      }
      if (!(dynamic_states & V3DV_DYNAMIC_LINE_WIDTH))
         dynamic->line_width = pRasterizationState->lineWidth;
   }

   pipeline->dynamic_state.mask = dynamic_states;
}

static uint8_t
blend_factor(VkBlendFactor factor, bool dst_alpha_one, bool *needs_constants)
{
   switch (factor) {
   case VK_BLEND_FACTOR_ZERO:
   case VK_BLEND_FACTOR_ONE:
   case VK_BLEND_FACTOR_SRC_COLOR:
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
   case VK_BLEND_FACTOR_DST_COLOR:
   case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
   case VK_BLEND_FACTOR_SRC_ALPHA:
   case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
   case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
      return factor;
   case VK_BLEND_FACTOR_CONSTANT_COLOR:
   case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
   case VK_BLEND_FACTOR_CONSTANT_ALPHA:
   case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
      *needs_constants = true;
      return factor;
   case VK_BLEND_FACTOR_DST_ALPHA:
      return dst_alpha_one ? V3D_BLEND_FACTOR_ONE :
                             V3D_BLEND_FACTOR_DST_ALPHA;
   case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
      return dst_alpha_one ? V3D_BLEND_FACTOR_ZERO :
                             V3D_BLEND_FACTOR_INV_DST_ALPHA;
   case VK_BLEND_FACTOR_SRC1_COLOR:
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
   case VK_BLEND_FACTOR_SRC1_ALPHA:
   case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
      assert(!"Invalid blend factor: dual source blending not supported.");
   default:
      assert(!"Unknown blend factor.");
   }

   /* Should be handled by the switch, added to avoid a "end of non-void
    * function" error
    */
   unreachable("Unknown blend factor.");
}

static void
pack_blend(struct v3dv_pipeline *pipeline,
           const VkPipelineColorBlendStateCreateInfo *cb_info)
{
   /* By default, we are not enabling blending and all color channel writes are
    * enabled. Color write enables are independent of whether blending is
    * enabled or not.
    *
    * Vulkan specifies color write masks so that bits set correspond to
    * enabled channels. Our hardware does it the other way around.
    */
   pipeline->blend.enables = 0;
   pipeline->blend.color_write_masks = 0; /* All channels enabled */

   if (!cb_info)
      return;

   assert(pipeline->subpass);
   if (pipeline->subpass->color_count == 0)
      return;

   assert(pipeline->subpass->color_count == cb_info->attachmentCount);

   pipeline->blend.needs_color_constants = false;
   uint32_t color_write_masks = 0;
   for (uint32_t i = 0; i < pipeline->subpass->color_count; i++) {
      const VkPipelineColorBlendAttachmentState *b_state =
         &cb_info->pAttachments[i];

      uint32_t attachment_idx =
         pipeline->subpass->color_attachments[i].attachment;
      if (attachment_idx == VK_ATTACHMENT_UNUSED)
         continue;

      color_write_masks |= (~b_state->colorWriteMask & 0xf) << (4 * i);

      if (!b_state->blendEnable)
         continue;

      VkAttachmentDescription *desc =
         &pipeline->pass->attachments[attachment_idx].desc;
      const struct v3dv_format *format = v3dv_get_format(desc->format);
      bool dst_alpha_one = (format->swizzle[3] == PIPE_SWIZZLE_1);

      uint8_t rt_mask = 1 << i;
      pipeline->blend.enables |= rt_mask;

      v3dv_pack(pipeline->blend.cfg[i], BLEND_CFG, config) {
         config.render_target_mask = rt_mask;

         config.color_blend_mode = b_state->colorBlendOp;
         config.color_blend_dst_factor =
            blend_factor(b_state->dstColorBlendFactor, dst_alpha_one,
                         &pipeline->blend.needs_color_constants);
         config.color_blend_src_factor =
            blend_factor(b_state->srcColorBlendFactor, dst_alpha_one,
                         &pipeline->blend.needs_color_constants);

         config.alpha_blend_mode = b_state->alphaBlendOp;
         config.alpha_blend_dst_factor =
            blend_factor(b_state->dstAlphaBlendFactor, dst_alpha_one,
                         &pipeline->blend.needs_color_constants);
         config.alpha_blend_src_factor =
            blend_factor(b_state->srcAlphaBlendFactor, dst_alpha_one,
                         &pipeline->blend.needs_color_constants);
      }
   }

   pipeline->blend.color_write_masks = color_write_masks;
}

/* This requires that pack_blend() had been called before so we can set
 * the overall blend enable bit in the CFG_BITS packet.
 */
static void
pack_cfg_bits(struct v3dv_pipeline *pipeline,
              const VkPipelineDepthStencilStateCreateInfo *ds_info,
              const VkPipelineRasterizationStateCreateInfo *rs_info,
              const VkPipelineMultisampleStateCreateInfo *ms_info)
{
   assert(sizeof(pipeline->cfg_bits) == cl_packet_length(CFG_BITS));

   pipeline->msaa =
      ms_info && ms_info->rasterizationSamples > VK_SAMPLE_COUNT_1_BIT;

   v3dv_pack(pipeline->cfg_bits, CFG_BITS, config) {
      config.enable_forward_facing_primitive =
         rs_info ? !(rs_info->cullMode & VK_CULL_MODE_FRONT_BIT) : false;

      config.enable_reverse_facing_primitive =
         rs_info ? !(rs_info->cullMode & VK_CULL_MODE_BACK_BIT) : false;

      /* Seems like the hardware is backwards regarding this setting... */
      config.clockwise_primitives =
         rs_info ? rs_info->frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE : false;

      config.enable_depth_offset = rs_info ? rs_info->depthBiasEnable: false;

      /* This is required to pass line rasterization tests in CTS while
       * exposing, at least, a minimum of 4-bits of subpixel precision
       * (the minimum requirement).
       */
      config.line_rasterization = 1; /* perp end caps */

      if (rs_info && rs_info->polygonMode != VK_POLYGON_MODE_FILL) {
         config.direct3d_wireframe_triangles_mode = true;
         config.direct3d_point_fill_mode =
            rs_info->polygonMode == VK_POLYGON_MODE_POINT;
      }

      config.rasterizer_oversample_mode = pipeline->msaa ? 1 : 0;

      /* From the Vulkan spec:
       *
       *   "Provoking Vertex:
       *
       *       The vertex in a primitive from which flat shaded attribute
       *       values are taken. This is generally the “first” vertex in the
       *       primitive, and depends on the primitive topology."
       *
       * First vertex is the Direct3D style for provoking vertex. OpenGL uses
       * the last vertex by default.
       */
      config.direct3d_provoking_vertex = true;

      config.blend_enable = pipeline->blend.enables != 0;

      /* Disable depth/stencil if we don't have a D/S attachment */
      bool has_ds_attachment =
         pipeline->subpass->ds_attachment.attachment != VK_ATTACHMENT_UNUSED;

      if (ds_info && ds_info->depthTestEnable && has_ds_attachment) {
         config.z_updates_enable = ds_info->depthWriteEnable;
         config.depth_test_function = ds_info->depthCompareOp;
      } else {
         config.depth_test_function = VK_COMPARE_OP_ALWAYS;
      }

      /* EZ state will be updated at draw time based on bound pipeline state */
      config.early_z_updates_enable = false;
      config.early_z_enable = false;

      config.stencil_enable =
         ds_info ? ds_info->stencilTestEnable && has_ds_attachment: false;
   };
}

static uint32_t
translate_stencil_op(enum pipe_stencil_op op)
{
   switch (op) {
   case VK_STENCIL_OP_KEEP:
      return V3D_STENCIL_OP_KEEP;
   case VK_STENCIL_OP_ZERO:
      return V3D_STENCIL_OP_ZERO;
   case VK_STENCIL_OP_REPLACE:
      return V3D_STENCIL_OP_REPLACE;
   case VK_STENCIL_OP_INCREMENT_AND_CLAMP:
      return V3D_STENCIL_OP_INCR;
   case VK_STENCIL_OP_DECREMENT_AND_CLAMP:
      return V3D_STENCIL_OP_DECR;
   case VK_STENCIL_OP_INVERT:
      return V3D_STENCIL_OP_INVERT;
   case VK_STENCIL_OP_INCREMENT_AND_WRAP:
      return V3D_STENCIL_OP_INCWRAP;
   case VK_STENCIL_OP_DECREMENT_AND_WRAP:
      return V3D_STENCIL_OP_DECWRAP;
   default:
      unreachable("bad stencil op");
   }
}

static void
pack_single_stencil_cfg(struct v3dv_pipeline *pipeline,
                        uint8_t *stencil_cfg,
                        bool is_front,
                        bool is_back,
                        const VkStencilOpState *stencil_state)
{
   /* From the Vulkan spec:
    *
    *   "Reference is an integer reference value that is used in the unsigned
    *    stencil comparison. The reference value used by stencil comparison
    *    must be within the range [0,2^s-1] , where s is the number of bits in
    *    the stencil framebuffer attachment, otherwise the reference value is
    *    considered undefined."
    *
    * In our case, 's' is always 8, so we clamp to that to prevent our packing
    * functions to assert in debug mode if they see larger values.
    *
    * If we have dynamic state we need to make sure we set the corresponding
    * state bits to 0, since cl_emit_with_prepacked ORs the new value with
    * the old.
    */
   const uint8_t write_mask =
      pipeline->dynamic_state.mask & V3DV_DYNAMIC_STENCIL_WRITE_MASK ?
         0 : stencil_state->writeMask & 0xff;

   const uint8_t compare_mask =
      pipeline->dynamic_state.mask & V3DV_DYNAMIC_STENCIL_COMPARE_MASK ?
         0 : stencil_state->compareMask & 0xff;

   const uint8_t reference =
      pipeline->dynamic_state.mask & V3DV_DYNAMIC_STENCIL_COMPARE_MASK ?
         0 : stencil_state->reference & 0xff;

   v3dv_pack(stencil_cfg, STENCIL_CFG, config) {
      config.front_config = is_front;
      config.back_config = is_back;
      config.stencil_write_mask = write_mask;
      config.stencil_test_mask = compare_mask;
      config.stencil_test_function = stencil_state->compareOp;
      config.stencil_pass_op = translate_stencil_op(stencil_state->passOp);
      config.depth_test_fail_op = translate_stencil_op(stencil_state->depthFailOp);
      config.stencil_test_fail_op = translate_stencil_op(stencil_state->failOp);
      config.stencil_ref_value = reference;
   }
}

static void
pack_stencil_cfg(struct v3dv_pipeline *pipeline,
                 const VkPipelineDepthStencilStateCreateInfo *ds_info)
{
   assert(sizeof(pipeline->stencil_cfg) == 2 * cl_packet_length(STENCIL_CFG));

   if (!ds_info || !ds_info->stencilTestEnable)
      return;

   if (pipeline->subpass->ds_attachment.attachment == VK_ATTACHMENT_UNUSED)
      return;

   const uint32_t dynamic_stencil_states = V3DV_DYNAMIC_STENCIL_COMPARE_MASK |
                                           V3DV_DYNAMIC_STENCIL_WRITE_MASK |
                                           V3DV_DYNAMIC_STENCIL_REFERENCE;


   /* If front != back or we have dynamic stencil state we can't emit a single
    * packet for both faces.
    */
   bool needs_front_and_back = false;
   if ((pipeline->dynamic_state.mask & dynamic_stencil_states) ||
       memcmp(&ds_info->front, &ds_info->back, sizeof(ds_info->front)))
      needs_front_and_back = true;

   /* If the front and back configurations are the same we can emit both with
    * a single packet.
    */
   pipeline->emit_stencil_cfg[0] = true;
   if (!needs_front_and_back) {
      pack_single_stencil_cfg(pipeline, pipeline->stencil_cfg[0],
                              true, true, &ds_info->front);
   } else {
      pipeline->emit_stencil_cfg[1] = true;
      pack_single_stencil_cfg(pipeline, pipeline->stencil_cfg[0],
                              true, false, &ds_info->front);
      pack_single_stencil_cfg(pipeline, pipeline->stencil_cfg[1],
                              false, true, &ds_info->back);
   }
}

static bool
stencil_op_is_no_op(const VkStencilOpState *stencil)
{
   return stencil->depthFailOp == VK_STENCIL_OP_KEEP &&
          stencil->compareOp == VK_COMPARE_OP_ALWAYS;
}

static void
enable_depth_bias(struct v3dv_pipeline *pipeline,
                  const VkPipelineRasterizationStateCreateInfo *rs_info)
{
   pipeline->depth_bias.enabled = false;
   pipeline->depth_bias.is_z16 = false;

   if (!rs_info || !rs_info->depthBiasEnable)
      return;

   /* Check the depth/stencil attachment description for the subpass used with
    * this pipeline.
    */
   assert(pipeline->pass && pipeline->subpass);
   struct v3dv_render_pass *pass = pipeline->pass;
   struct v3dv_subpass *subpass = pipeline->subpass;

   if (subpass->ds_attachment.attachment == VK_ATTACHMENT_UNUSED)
      return;

   assert(subpass->ds_attachment.attachment < pass->attachment_count);
   struct v3dv_render_pass_attachment *att =
      &pass->attachments[subpass->ds_attachment.attachment];

   if (att->desc.format == VK_FORMAT_D16_UNORM)
      pipeline->depth_bias.is_z16 = true;

   pipeline->depth_bias.enabled = true;
}

static void
pipeline_set_ez_state(struct v3dv_pipeline *pipeline,
                      const VkPipelineDepthStencilStateCreateInfo *ds_info)
{
   if (!ds_info || !ds_info->depthTestEnable) {
      pipeline->ez_state = VC5_EZ_DISABLED;
      return;
   }

   switch (ds_info->depthCompareOp) {
   case VK_COMPARE_OP_LESS:
   case VK_COMPARE_OP_LESS_OR_EQUAL:
      pipeline->ez_state = VC5_EZ_LT_LE;
      break;
   case VK_COMPARE_OP_GREATER:
   case VK_COMPARE_OP_GREATER_OR_EQUAL:
      pipeline->ez_state = VC5_EZ_GT_GE;
      break;
   case VK_COMPARE_OP_NEVER:
   case VK_COMPARE_OP_EQUAL:
      pipeline->ez_state = VC5_EZ_UNDECIDED;
      break;
   default:
      pipeline->ez_state = VC5_EZ_DISABLED;
      break;
   }

   /* If stencil is enabled and is not a no-op, we need to disable EZ */
   if (ds_info->stencilTestEnable &&
       (!stencil_op_is_no_op(&ds_info->front) ||
        !stencil_op_is_no_op(&ds_info->back))) {
         pipeline->ez_state = VC5_EZ_DISABLED;
   }
}

static void
pack_shader_state_record(struct v3dv_pipeline *pipeline)
{
   assert(sizeof(pipeline->shader_state_record) ==
          cl_packet_length(GL_SHADER_STATE_RECORD));

   struct v3d_fs_prog_data *prog_data_fs =
      pipeline->fs->current_variant->prog_data.fs;

   struct v3d_vs_prog_data *prog_data_vs =
      pipeline->vs->current_variant->prog_data.vs;

   struct v3d_vs_prog_data *prog_data_vs_bin =
      pipeline->vs_bin->current_variant->prog_data.vs;


   /* Note: we are not packing addresses, as we need the job (see
    * cl_pack_emit_reloc). Additionally uniforms can't be filled up at this
    * point as they depend on dynamic info that can be set after create the
    * pipeline (like viewport), . Would need to be filled later, so we are
    * doing a partial prepacking.
    */
   v3dv_pack(pipeline->shader_state_record, GL_SHADER_STATE_RECORD, shader) {
      shader.enable_clipping = true;

      shader.point_size_in_shaded_vertex_data =
         pipeline->vs->topology == PIPE_PRIM_POINTS;

      /* Must be set if the shader modifies Z, discards, or modifies
       * the sample mask.  For any of these cases, the fragment
       * shader needs to write the Z value (even just discards).
       */
      shader.fragment_shader_does_z_writes = prog_data_fs->writes_z;
      /* Set if the EZ test must be disabled (due to shader side
       * effects and the early_z flag not being present in the
       * shader).
       */
      shader.turn_off_early_z_test = prog_data_fs->disable_ez;

      shader.fragment_shader_uses_real_pixel_centre_w_in_addition_to_centroid_w2 =
         prog_data_fs->uses_center_w;

      /* The description for gl_SampleID states that if a fragment shader reads
       * it, then we should automatically activate per-sample shading. However,
       * the Vulkan spec also states that if a framebuffer has no attachments:
       *
       *    "The subpass continues to use the width, height, and layers of the
       *     framebuffer to define the dimensions of the rendering area, and the
       *     rasterizationSamples from each pipeline’s
       *     VkPipelineMultisampleStateCreateInfo to define the number of
       *     samples used in rasterization multisample rasterization."
       *
       * So in this scenario, if the pipeline doesn't enable multiple samples
       * but the fragment shader accesses gl_SampleID we would be requested
       * to do per-sample shading in single sample rasterization mode, which
       * is pointless, so just disable it in that case.
       */
      shader.enable_sample_rate_shading =
         pipeline->sample_rate_shading ||
         (pipeline->msaa && prog_data_fs->force_per_sample_msaa);

      shader.any_shader_reads_hardware_written_primitive_id = false;

      shader.do_scoreboard_wait_on_first_thread_switch =
         prog_data_fs->lock_scoreboard_on_first_thrsw;
      shader.disable_implicit_point_line_varyings =
         !prog_data_fs->uses_implicit_point_line_varyings;

      shader.number_of_varyings_in_fragment_shader =
         prog_data_fs->num_inputs;

      shader.coordinate_shader_propagate_nans = true;
      shader.vertex_shader_propagate_nans = true;
      shader.fragment_shader_propagate_nans = true;

      /* Note: see previous note about adresses */
      /* shader.coordinate_shader_code_address */
      /* shader.vertex_shader_code_address */
      /* shader.fragment_shader_code_address */

      /* FIXME: Use combined input/output size flag in the common case (also
       * on v3d, see v3dx_draw).
       */
      shader.coordinate_shader_has_separate_input_and_output_vpm_blocks =
         prog_data_vs_bin->separate_segments;
      shader.vertex_shader_has_separate_input_and_output_vpm_blocks =
         prog_data_vs->separate_segments;

      shader.coordinate_shader_input_vpm_segment_size =
         prog_data_vs_bin->separate_segments ?
         prog_data_vs_bin->vpm_input_size : 1;
      shader.vertex_shader_input_vpm_segment_size =
         prog_data_vs->separate_segments ?
         prog_data_vs->vpm_input_size : 1;

      shader.coordinate_shader_output_vpm_segment_size =
         prog_data_vs_bin->vpm_output_size;
      shader.vertex_shader_output_vpm_segment_size =
         prog_data_vs->vpm_output_size;

      /* Note: see previous note about adresses */
      /* shader.coordinate_shader_uniforms_address */
      /* shader.vertex_shader_uniforms_address */
      /* shader.fragment_shader_uniforms_address */

      shader.min_coord_shader_input_segments_required_in_play =
         pipeline->vpm_cfg_bin.As;
      shader.min_vertex_shader_input_segments_required_in_play =
         pipeline->vpm_cfg.As;

      shader.min_coord_shader_output_segments_required_in_play_in_addition_to_vcm_cache_size =
         pipeline->vpm_cfg_bin.Ve;
      shader.min_vertex_shader_output_segments_required_in_play_in_addition_to_vcm_cache_size =
         pipeline->vpm_cfg.Ve;

      shader.coordinate_shader_4_way_threadable =
         prog_data_vs_bin->base.threads == 4;
      shader.vertex_shader_4_way_threadable =
         prog_data_vs->base.threads == 4;
      shader.fragment_shader_4_way_threadable =
         prog_data_fs->base.threads == 4;

      shader.coordinate_shader_start_in_final_thread_section =
         prog_data_vs_bin->base.single_seg;
      shader.vertex_shader_start_in_final_thread_section =
         prog_data_vs->base.single_seg;
      shader.fragment_shader_start_in_final_thread_section =
         prog_data_fs->base.single_seg;

      shader.vertex_id_read_by_coordinate_shader =
         prog_data_vs_bin->uses_vid;
      shader.base_instance_id_read_by_coordinate_shader =
         prog_data_vs_bin->uses_biid;
      shader.instance_id_read_by_coordinate_shader =
         prog_data_vs_bin->uses_iid;
      shader.vertex_id_read_by_vertex_shader =
         prog_data_vs->uses_vid;
      shader.base_instance_id_read_by_vertex_shader =
         prog_data_vs->uses_biid;
      shader.instance_id_read_by_vertex_shader =
         prog_data_vs->uses_iid;

      /* Note: see previous note about adresses */
      /* shader.address_of_default_attribute_values */
   }
}

static void
pack_vcm_cache_size(struct v3dv_pipeline *pipeline)
{
   assert(sizeof(pipeline->vcm_cache_size) ==
          cl_packet_length(VCM_CACHE_SIZE));

   v3dv_pack(pipeline->vcm_cache_size, VCM_CACHE_SIZE, vcm) {
      vcm.number_of_16_vertex_batches_for_binning = pipeline->vpm_cfg_bin.Vc;
      vcm.number_of_16_vertex_batches_for_rendering = pipeline->vpm_cfg.Vc;
   }
}

/* As defined on the GL_SHADER_STATE_ATTRIBUTE_RECORD */
static uint8_t
get_attr_type(const struct util_format_description *desc)
{
   uint32_t r_size = desc->channel[0].size;
   uint8_t attr_type = ATTRIBUTE_FLOAT;

   switch (desc->channel[0].type) {
   case UTIL_FORMAT_TYPE_FLOAT:
      if (r_size == 32) {
         attr_type = ATTRIBUTE_FLOAT;
      } else {
         assert(r_size == 16);
         attr_type = ATTRIBUTE_HALF_FLOAT;
      }
      break;

   case UTIL_FORMAT_TYPE_SIGNED:
   case UTIL_FORMAT_TYPE_UNSIGNED:
      switch (r_size) {
      case 32:
         attr_type = ATTRIBUTE_INT;
         break;
      case 16:
         attr_type = ATTRIBUTE_SHORT;
         break;
      case 10:
         attr_type = ATTRIBUTE_INT2_10_10_10;
         break;
      case 8:
         attr_type = ATTRIBUTE_BYTE;
         break;
      default:
         fprintf(stderr,
                 "format %s unsupported\n",
                 desc->name);
         attr_type = ATTRIBUTE_BYTE;
         abort();
      }
      break;

   default:
      fprintf(stderr,
              "format %s unsupported\n",
              desc->name);
      abort();
   }

   return attr_type;
}

static bool
create_default_attribute_values(struct v3dv_pipeline *pipeline,
                                const VkPipelineVertexInputStateCreateInfo *vi_info)
{
   uint32_t size = MAX_VERTEX_ATTRIBS * sizeof(float) * 4;

   if (pipeline->default_attribute_values == NULL) {
      pipeline->default_attribute_values = v3dv_bo_alloc(pipeline->device, size,
                                                         "default_vi_attributes",
                                                         true);

      if (!pipeline->default_attribute_values) {
         fprintf(stderr, "failed to allocate memory for the default "
                 "attribute values\n");
         return false;
      }
   }

   bool ok = v3dv_bo_map(pipeline->device,
                         pipeline->default_attribute_values, size);
   if (!ok) {
      fprintf(stderr, "failed to map default attribute values buffer\n");
      return false;
   }

   uint32_t *attrs = pipeline->default_attribute_values->map;

   for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++) {
      attrs[i * 4 + 0] = 0;
      attrs[i * 4 + 1] = 0;
      attrs[i * 4 + 2] = 0;
      if (i < pipeline->va_count && vk_format_is_int(pipeline->va[i].vk_format)) {
         attrs[i * 4 + 3] = 1;
      } else {
         attrs[i * 4 + 3] = fui(1.0);
      }
   }

   v3dv_bo_unmap(pipeline->device, pipeline->default_attribute_values);

   return true;
}

static void
pack_shader_state_attribute_record(struct v3dv_pipeline *pipeline,
                                   uint32_t index,
                                   const VkVertexInputAttributeDescription *vi_desc)
{
   const uint32_t packet_length =
      cl_packet_length(GL_SHADER_STATE_ATTRIBUTE_RECORD);

   const struct util_format_description *desc =
      vk_format_description(vi_desc->format);

   uint32_t binding = vi_desc->binding;

   v3dv_pack(&pipeline->vertex_attrs[index * packet_length],
             GL_SHADER_STATE_ATTRIBUTE_RECORD, attr) {

      /* vec_size == 0 means 4 */
      attr.vec_size = desc->nr_channels & 3;
      attr.signed_int_type = (desc->channel[0].type ==
                              UTIL_FORMAT_TYPE_SIGNED);
      attr.normalized_int_type = desc->channel[0].normalized;
      attr.read_as_int_uint = desc->channel[0].pure_integer;

      attr.instance_divisor = MIN2(pipeline->vb[binding].instance_divisor,
                                   0xffff);
      attr.stride = pipeline->vb[binding].stride;
      attr.type = get_attr_type(desc);
   }
}

static void
pipeline_set_sample_mask(struct v3dv_pipeline *pipeline,
                         const VkPipelineMultisampleStateCreateInfo *ms_info)
{
   pipeline->sample_mask = (1 << V3D_MAX_SAMPLES) - 1;

   /* Ignore pSampleMask if we are not enabling multisampling. The hardware
    * requires this to be 0xf or 0x0 if using a single sample.
    */
   if (ms_info && ms_info->pSampleMask &&
       ms_info->rasterizationSamples > VK_SAMPLE_COUNT_1_BIT) {
      pipeline->sample_mask &= ms_info->pSampleMask[0];
   }
}

static void
pipeline_set_sample_rate_shading(struct v3dv_pipeline *pipeline,
                                 const VkPipelineMultisampleStateCreateInfo *ms_info)
{
   pipeline->sample_rate_shading =
      ms_info && ms_info->rasterizationSamples > VK_SAMPLE_COUNT_1_BIT &&
      ms_info->sampleShadingEnable;
}

static VkResult
pipeline_init(struct v3dv_pipeline *pipeline,
              struct v3dv_device *device,
              struct v3dv_pipeline_cache *cache,
              const VkGraphicsPipelineCreateInfo *pCreateInfo,
              const VkAllocationCallbacks *pAllocator)
{
   VkResult result = VK_SUCCESS;

   pipeline->device = device;

   V3DV_FROM_HANDLE(v3dv_pipeline_layout, layout, pCreateInfo->layout);
   pipeline->layout = layout;

   V3DV_FROM_HANDLE(v3dv_render_pass, render_pass, pCreateInfo->renderPass);
   assert(pCreateInfo->subpass < render_pass->subpass_count);
   pipeline->pass = render_pass;
   pipeline->subpass = &render_pass->subpasses[pCreateInfo->subpass];

   /* If rasterization is not enabled, various CreateInfo structs must be
    * ignored.
    */
   const bool raster_enabled =
      !pCreateInfo->pRasterizationState->rasterizerDiscardEnable;

   const VkPipelineViewportStateCreateInfo *vp_info =
      raster_enabled ? pCreateInfo->pViewportState : NULL;

   const VkPipelineDepthStencilStateCreateInfo *ds_info =
      raster_enabled ? pCreateInfo->pDepthStencilState : NULL;

   const VkPipelineRasterizationStateCreateInfo *rs_info =
      raster_enabled ? pCreateInfo->pRasterizationState : NULL;

   const VkPipelineColorBlendStateCreateInfo *cb_info =
      raster_enabled ? pCreateInfo->pColorBlendState : NULL;

   const VkPipelineMultisampleStateCreateInfo *ms_info =
      raster_enabled ? pCreateInfo->pMultisampleState : NULL;

   pipeline_init_dynamic_state(pipeline,
                               pCreateInfo->pDynamicState,
                               vp_info, ds_info, cb_info, rs_info);

   /* V3D 4.2 doesn't support depth bounds testing so we don't advertise that
    * feature and it shouldn't be used by any pipeline.
    */
   assert(!ds_info || !ds_info->depthBoundsTestEnable);

   pack_blend(pipeline, cb_info);
   pack_cfg_bits(pipeline, ds_info, rs_info, ms_info);
   pack_stencil_cfg(pipeline, ds_info);
   pipeline_set_ez_state(pipeline, ds_info);
   enable_depth_bias(pipeline, rs_info);
   pipeline_set_sample_mask(pipeline, ms_info);
   pipeline_set_sample_rate_shading(pipeline, ms_info);

   pipeline->primitive_restart =
      pCreateInfo->pInputAssemblyState->primitiveRestartEnable;

   result = pipeline_compile_graphics(pipeline, cache, pCreateInfo, pAllocator);

   if (result != VK_SUCCESS) {
      /* Caller would already destroy the pipeline, and we didn't allocate any
       * extra info. We don't need to do anything else.
       */
      return result;
   }

   pack_shader_state_record(pipeline);
   pack_vcm_cache_size(pipeline);

   const VkPipelineVertexInputStateCreateInfo *vi_info =
      pCreateInfo->pVertexInputState;

   pipeline->vb_count = vi_info->vertexBindingDescriptionCount;
   for (uint32_t i = 0; i < vi_info->vertexBindingDescriptionCount; i++) {
      const VkVertexInputBindingDescription *desc =
         &vi_info->pVertexBindingDescriptions[i];

      pipeline->vb[desc->binding].stride = desc->stride;
      pipeline->vb[desc->binding].instance_divisor = desc->inputRate;
   }

   pipeline->va_count = 0;
   nir_shader *shader = pipeline->vs->nir;

   for (uint32_t i = 0; i < vi_info->vertexAttributeDescriptionCount; i++) {
      const VkVertexInputAttributeDescription *desc =
         &vi_info->pVertexAttributeDescriptions[i];
      uint32_t location = desc->location + VERT_ATTRIB_GENERIC0;

      nir_variable *var = nir_find_variable_with_location(shader, nir_var_shader_in, location);

      if (var != NULL) {
         unsigned driver_location = var->data.driver_location;

         assert(driver_location < MAX_VERTEX_ATTRIBS);
         pipeline->va[driver_location].offset = desc->offset;
         pipeline->va[driver_location].binding = desc->binding;
         pipeline->va[driver_location].vk_format = desc->format;

         pack_shader_state_attribute_record(pipeline, driver_location, desc);

         pipeline->va_count++;
      }
   }

   if (!create_default_attribute_values(pipeline, vi_info))
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;

   return result;
}

static VkResult
graphics_pipeline_create(VkDevice _device,
                         VkPipelineCache _cache,
                         const VkGraphicsPipelineCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkPipeline *pPipeline)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   V3DV_FROM_HANDLE(v3dv_pipeline_cache, cache, _cache);

   struct v3dv_pipeline *pipeline;
   VkResult result;

   /* Use the default pipeline cache if none is specified */
   if (cache == NULL && device->instance->default_pipeline_cache_enabled)
       cache = &device->default_pipeline_cache;

   pipeline = vk_zalloc2(&device->alloc, pAllocator, sizeof(*pipeline), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pipeline == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   result = pipeline_init(pipeline, device, cache,
                          pCreateInfo,
                          pAllocator);

   if (result != VK_SUCCESS) {
      v3dv_destroy_pipeline(pipeline, device, pAllocator);
      return result;
   }

   *pPipeline = v3dv_pipeline_to_handle(pipeline);

   return VK_SUCCESS;
}

VkResult
v3dv_CreateGraphicsPipelines(VkDevice _device,
                             VkPipelineCache pipelineCache,
                             uint32_t count,
                             const VkGraphicsPipelineCreateInfo *pCreateInfos,
                             const VkAllocationCallbacks *pAllocator,
                             VkPipeline *pPipelines)
{
   VkResult result = VK_SUCCESS;

   for (uint32_t i = 0; i < count; i++) {
      VkResult local_result;

      local_result = graphics_pipeline_create(_device,
                                              pipelineCache,
                                              &pCreateInfos[i],
                                              pAllocator,
                                              &pPipelines[i]);

      if (local_result != VK_SUCCESS) {
         result = local_result;
         pPipelines[i] = VK_NULL_HANDLE;
      }
   }

   return result;
}

static void
shared_type_info(const struct glsl_type *type, unsigned *size, unsigned *align)
{
   assert(glsl_type_is_vector_or_scalar(type));

   uint32_t comp_size = glsl_type_is_boolean(type)
      ? 4 : glsl_get_bit_size(type) / 8;
   unsigned length = glsl_get_vector_elements(type);
   *size = comp_size * length,
   *align = comp_size * (length == 3 ? 4 : length);
}

static void
lower_cs_shared(struct nir_shader *nir)
{
   NIR_PASS_V(nir, nir_lower_vars_to_explicit_types,
              nir_var_mem_shared, shared_type_info);
   NIR_PASS_V(nir, nir_lower_explicit_io,
              nir_var_mem_shared, nir_address_format_32bit_offset);
}

static VkResult
pipeline_compile_compute(struct v3dv_pipeline *pipeline,
                         struct v3dv_pipeline_cache *cache,
                         const VkComputePipelineCreateInfo *info,
                         const VkAllocationCallbacks *alloc)
{
   struct v3dv_device *device = pipeline->device;
   struct v3dv_physical_device *physical_device =
      &device->instance->physicalDevice;

   const VkPipelineShaderStageCreateInfo *sinfo = &info->stage;
   gl_shader_stage stage = vk_to_mesa_shader_stage(sinfo->stage);

   struct v3dv_pipeline_stage *p_stage =
      vk_zalloc2(&device->alloc, alloc, sizeof(*p_stage), 8,
                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!p_stage)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   p_stage->program_id = p_atomic_inc_return(&physical_device->next_program_id);
   p_stage->compiled_variant_count = 0;
   p_stage->pipeline = pipeline;
   p_stage->stage = stage;
   p_stage->entrypoint = sinfo->pName;
   p_stage->module = v3dv_shader_module_from_handle(sinfo->module);
   p_stage->spec_info = sinfo->pSpecializationInfo;

   pipeline_hash_shader(p_stage->module,
                        p_stage->entrypoint,
                        stage,
                        p_stage->spec_info,
                        p_stage->shader_sha1);

   p_stage->nir = pipeline_stage_get_nir(p_stage, pipeline, cache);

   pipeline->active_stages |= sinfo->stage;
   st_nir_opts(p_stage->nir);
   pipeline_lower_nir(pipeline, p_stage, pipeline->layout);
   lower_cs_shared(p_stage->nir);

   pipeline->cs = p_stage;

   struct v3d_key *key = &p_stage->key.base;
   memset(key, 0, sizeof(*key));
   pipeline_populate_v3d_key(key, p_stage, 0,
                             pipeline->device->features.robustBufferAccess);

   VkResult result;
   p_stage->current_variant =
      pregenerate_shader_variants(p_stage, cache, key, sizeof(*key), alloc, &result);
   return result;
}

static VkResult
compute_pipeline_init(struct v3dv_pipeline *pipeline,
                      struct v3dv_device *device,
                      struct v3dv_pipeline_cache *cache,
                      const VkComputePipelineCreateInfo *info,
                      const VkAllocationCallbacks *alloc)
{
   V3DV_FROM_HANDLE(v3dv_pipeline_layout, layout, info->layout);

   pipeline->device = device;
   pipeline->layout = layout;

   VkResult result = pipeline_compile_compute(pipeline, cache, info, alloc);

   return result;
}

static VkResult
compute_pipeline_create(VkDevice _device,
                         VkPipelineCache _cache,
                         const VkComputePipelineCreateInfo *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkPipeline *pPipeline)
{
   V3DV_FROM_HANDLE(v3dv_device, device, _device);
   V3DV_FROM_HANDLE(v3dv_pipeline_cache, cache, _cache);

   struct v3dv_pipeline *pipeline;
   VkResult result;

   /* Use the default pipeline cache if none is specified */
   if (cache == NULL && device->instance->default_pipeline_cache_enabled)
       cache = &device->default_pipeline_cache;

   pipeline = vk_zalloc2(&device->alloc, pAllocator, sizeof(*pipeline), 8,
                         VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (pipeline == NULL)
      return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   result = compute_pipeline_init(pipeline, device, cache,
                                  pCreateInfo, pAllocator);
   if (result != VK_SUCCESS) {
      v3dv_destroy_pipeline(pipeline, device, pAllocator);
      return result;
   }

   *pPipeline = v3dv_pipeline_to_handle(pipeline);

   return VK_SUCCESS;
}

VkResult
v3dv_CreateComputePipelines(VkDevice _device,
                            VkPipelineCache pipelineCache,
                            uint32_t createInfoCount,
                            const VkComputePipelineCreateInfo *pCreateInfos,
                            const VkAllocationCallbacks *pAllocator,
                            VkPipeline *pPipelines)
{
   VkResult result = VK_SUCCESS;

   for (uint32_t i = 0; i < createInfoCount; i++) {
      VkResult local_result;
      local_result = compute_pipeline_create(_device,
                                              pipelineCache,
                                              &pCreateInfos[i],
                                              pAllocator,
                                              &pPipelines[i]);

      if (local_result != VK_SUCCESS) {
         result = local_result;
         pPipelines[i] = VK_NULL_HANDLE;
      }
   }

   return result;
}
