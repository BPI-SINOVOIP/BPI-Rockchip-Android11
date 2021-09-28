/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
 * Copyright © 2015 Intel Corporation
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

#include "radv_private.h"
#include "radv_shader.h"
#include "radv_shader_helper.h"
#include "radv_shader_args.h"
#include "radv_debug.h"
#include "nir/nir.h"

#include "sid.h"
#include "ac_binary.h"
#include "ac_llvm_util.h"
#include "ac_llvm_build.h"
#include "ac_shader_abi.h"
#include "ac_shader_util.h"
#include "ac_exp_param.h"

#define RADEON_LLVM_MAX_INPUTS (VARYING_SLOT_VAR31 + 1)

struct radv_shader_context {
	struct ac_llvm_context ac;
	const struct nir_shader *shader;
	struct ac_shader_abi abi;
	const struct radv_shader_args *args;

	gl_shader_stage stage;

	unsigned max_workgroup_size;
	LLVMContextRef context;
	LLVMValueRef main_function;

	LLVMValueRef descriptor_sets[MAX_SETS];

	LLVMValueRef ring_offsets;

	LLVMValueRef rel_auto_id;

	LLVMValueRef gs_wave_id;
	LLVMValueRef gs_vtx_offset[6];

	LLVMValueRef esgs_ring;
	LLVMValueRef gsvs_ring[4];
	LLVMValueRef hs_ring_tess_offchip;
	LLVMValueRef hs_ring_tess_factor;

	LLVMValueRef inputs[RADEON_LLVM_MAX_INPUTS * 4];

	uint64_t output_mask;

	LLVMValueRef gs_next_vertex[4];
	LLVMValueRef gs_curprim_verts[4];
	LLVMValueRef gs_generated_prims[4];
	LLVMValueRef gs_ngg_emit;
	LLVMValueRef gs_ngg_scratch;

	uint32_t tcs_num_inputs;
	uint32_t tcs_num_patches;
	uint32_t tcs_tess_lvl_inner;
	uint32_t tcs_tess_lvl_outer;

	LLVMValueRef vertexptr; /* GFX10 only */
};

struct radv_shader_output_values {
	LLVMValueRef values[4];
	unsigned slot_name;
	unsigned slot_index;
	unsigned usage_mask;
};

static inline struct radv_shader_context *
radv_shader_context_from_abi(struct ac_shader_abi *abi)
{
	struct radv_shader_context *ctx = NULL;
	return container_of(abi, ctx, abi);
}

static LLVMValueRef get_rel_patch_id(struct radv_shader_context *ctx)
{
	switch (ctx->stage) {
	case MESA_SHADER_TESS_CTRL:
		return ac_unpack_param(&ctx->ac,
				       ac_get_arg(&ctx->ac, ctx->args->ac.tcs_rel_ids),
				       0, 8);
	case MESA_SHADER_TESS_EVAL:
		return ac_get_arg(&ctx->ac, ctx->args->tes_rel_patch_id);
		break;
	default:
		unreachable("Illegal stage");
	}
}

/* Tessellation shaders pass outputs to the next shader using LDS.
 *
 * LS outputs = TCS inputs
 * TCS outputs = TES inputs
 *
 * The LDS layout is:
 * - TCS inputs for patch 0
 * - TCS inputs for patch 1
 * - TCS inputs for patch 2		= get_tcs_in_current_patch_offset (if RelPatchID==2)
 * - ...
 * - TCS outputs for patch 0            = get_tcs_out_patch0_offset
 * - Per-patch TCS outputs for patch 0  = get_tcs_out_patch0_patch_data_offset
 * - TCS outputs for patch 1
 * - Per-patch TCS outputs for patch 1
 * - TCS outputs for patch 2            = get_tcs_out_current_patch_offset (if RelPatchID==2)
 * - Per-patch TCS outputs for patch 2  = get_tcs_out_current_patch_data_offset (if RelPatchID==2)
 * - ...
 *
 * All three shaders VS(LS), TCS, TES share the same LDS space.
 */
static LLVMValueRef
get_tcs_in_patch_stride(struct radv_shader_context *ctx)
{
	assert(ctx->stage == MESA_SHADER_TESS_CTRL);
	uint32_t input_vertex_size = ctx->tcs_num_inputs * 16;
	uint32_t input_patch_size = ctx->args->options->key.tcs.input_vertices * input_vertex_size;

	input_patch_size /= 4;
	return LLVMConstInt(ctx->ac.i32, input_patch_size, false);
}

static LLVMValueRef
get_tcs_out_patch_stride(struct radv_shader_context *ctx)
{
	uint32_t num_tcs_outputs = ctx->args->shader_info->tcs.num_linked_outputs;
	uint32_t num_tcs_patch_outputs = ctx->args->shader_info->tcs.num_linked_patch_outputs;
	uint32_t output_vertex_size = num_tcs_outputs * 16;
	uint32_t pervertex_output_patch_size = ctx->shader->info.tess.tcs_vertices_out * output_vertex_size;
	uint32_t output_patch_size = pervertex_output_patch_size + num_tcs_patch_outputs * 16;
	output_patch_size /= 4;
	return LLVMConstInt(ctx->ac.i32, output_patch_size, false);
}

static LLVMValueRef
get_tcs_out_vertex_stride(struct radv_shader_context *ctx)
{
	uint32_t num_tcs_outputs = ctx->args->shader_info->tcs.num_linked_outputs;
	uint32_t output_vertex_size = num_tcs_outputs * 16;
	output_vertex_size /= 4;
	return LLVMConstInt(ctx->ac.i32, output_vertex_size, false);
}

static LLVMValueRef
get_tcs_out_patch0_offset(struct radv_shader_context *ctx)
{
	assert (ctx->stage == MESA_SHADER_TESS_CTRL);
	uint32_t input_vertex_size = ctx->tcs_num_inputs * 16;
	uint32_t input_patch_size = ctx->args->options->key.tcs.input_vertices * input_vertex_size;
	uint32_t output_patch0_offset = input_patch_size;
	unsigned num_patches = ctx->tcs_num_patches;

	output_patch0_offset *= num_patches;
	output_patch0_offset /= 4;
	return LLVMConstInt(ctx->ac.i32, output_patch0_offset, false);
}

static LLVMValueRef
get_tcs_out_patch0_patch_data_offset(struct radv_shader_context *ctx)
{
	assert (ctx->stage == MESA_SHADER_TESS_CTRL);
	uint32_t input_vertex_size = ctx->tcs_num_inputs * 16;
	uint32_t input_patch_size = ctx->args->options->key.tcs.input_vertices * input_vertex_size;
	uint32_t output_patch0_offset = input_patch_size;

	uint32_t num_tcs_outputs = ctx->args->shader_info->tcs.num_linked_outputs;
	uint32_t output_vertex_size = num_tcs_outputs * 16;
	uint32_t pervertex_output_patch_size = ctx->shader->info.tess.tcs_vertices_out * output_vertex_size;
	unsigned num_patches = ctx->tcs_num_patches;

	output_patch0_offset *= num_patches;
	output_patch0_offset += pervertex_output_patch_size;
	output_patch0_offset /= 4;
	return LLVMConstInt(ctx->ac.i32, output_patch0_offset, false);
}

static LLVMValueRef
get_tcs_in_current_patch_offset(struct radv_shader_context *ctx)
{
	LLVMValueRef patch_stride = get_tcs_in_patch_stride(ctx);
	LLVMValueRef rel_patch_id = get_rel_patch_id(ctx);

	return LLVMBuildMul(ctx->ac.builder, patch_stride, rel_patch_id, "");
}

static LLVMValueRef
get_tcs_out_current_patch_offset(struct radv_shader_context *ctx)
{
	LLVMValueRef patch0_offset = get_tcs_out_patch0_offset(ctx);
	LLVMValueRef patch_stride = get_tcs_out_patch_stride(ctx);
	LLVMValueRef rel_patch_id = get_rel_patch_id(ctx);

	return ac_build_imad(&ctx->ac, patch_stride, rel_patch_id,
			     patch0_offset);
}

static LLVMValueRef
get_tcs_out_current_patch_data_offset(struct radv_shader_context *ctx)
{
	LLVMValueRef patch0_patch_data_offset =
		get_tcs_out_patch0_patch_data_offset(ctx);
	LLVMValueRef patch_stride = get_tcs_out_patch_stride(ctx);
	LLVMValueRef rel_patch_id = get_rel_patch_id(ctx);

	return ac_build_imad(&ctx->ac, patch_stride, rel_patch_id,
			     patch0_patch_data_offset);
}

static LLVMValueRef
create_llvm_function(struct ac_llvm_context *ctx, LLVMModuleRef module,
                     LLVMBuilderRef builder,
		     const struct ac_shader_args *args,
		     enum ac_llvm_calling_convention convention,
		     unsigned max_workgroup_size,
		     const struct radv_nir_compiler_options *options)
{
	LLVMValueRef main_function =
		ac_build_main(args, ctx, convention, "main", ctx->voidt, module);

	if (options->address32_hi) {
		ac_llvm_add_target_dep_function_attr(main_function,
						     "amdgpu-32bit-address-high-bits",
						     options->address32_hi);
	}

	ac_llvm_set_workgroup_size(main_function, max_workgroup_size);

	return main_function;
}

static void
load_descriptor_sets(struct radv_shader_context *ctx)
{
	uint32_t mask = ctx->args->shader_info->desc_set_used_mask;
	if (ctx->args->shader_info->need_indirect_descriptor_sets) {
		LLVMValueRef desc_sets =
			ac_get_arg(&ctx->ac, ctx->args->descriptor_sets[0]);
		while (mask) {
			int i = u_bit_scan(&mask);

			ctx->descriptor_sets[i] =
				ac_build_load_to_sgpr(&ctx->ac, desc_sets,
						      LLVMConstInt(ctx->ac.i32, i, false));

		}
	} else {
		while (mask) {
			int i = u_bit_scan(&mask);

			ctx->descriptor_sets[i] =
				ac_get_arg(&ctx->ac, ctx->args->descriptor_sets[i]);
		}
	}
}

static enum ac_llvm_calling_convention
get_llvm_calling_convention(LLVMValueRef func, gl_shader_stage stage)
{
	switch (stage) {
	case MESA_SHADER_VERTEX:
	case MESA_SHADER_TESS_EVAL:
		return AC_LLVM_AMDGPU_VS;
		break;
	case MESA_SHADER_GEOMETRY:
		return AC_LLVM_AMDGPU_GS;
		break;
	case MESA_SHADER_TESS_CTRL:
		return AC_LLVM_AMDGPU_HS;
		break;
	case MESA_SHADER_FRAGMENT:
		return AC_LLVM_AMDGPU_PS;
		break;
	case MESA_SHADER_COMPUTE:
		return AC_LLVM_AMDGPU_CS;
		break;
	default:
		unreachable("Unhandle shader type");
	}
}

/* Returns whether the stage is a stage that can be directly before the GS */
static bool is_pre_gs_stage(gl_shader_stage stage)
{
	return stage == MESA_SHADER_VERTEX || stage == MESA_SHADER_TESS_EVAL;
}

static void create_function(struct radv_shader_context *ctx,
                            gl_shader_stage stage,
                            bool has_previous_stage)
{
	if (ctx->ac.chip_class >= GFX10) {
		if (is_pre_gs_stage(stage) && ctx->args->options->key.vs_common_out.as_ngg) {
			/* On GFX10, VS is merged into GS for NGG. */
			stage = MESA_SHADER_GEOMETRY;
			has_previous_stage = true;
		}
	}

	ctx->main_function = create_llvm_function(
	    &ctx->ac, ctx->ac.module, ctx->ac.builder, &ctx->args->ac,
	    get_llvm_calling_convention(ctx->main_function, stage),
	    ctx->max_workgroup_size,
	    ctx->args->options);

	ctx->ring_offsets = ac_build_intrinsic(&ctx->ac, "llvm.amdgcn.implicit.buffer.ptr",
					       LLVMPointerType(ctx->ac.i8, AC_ADDR_SPACE_CONST),
					       NULL, 0, AC_FUNC_ATTR_READNONE);
	ctx->ring_offsets = LLVMBuildBitCast(ctx->ac.builder, ctx->ring_offsets,
					     ac_array_in_const_addr_space(ctx->ac.v4i32), "");

	load_descriptor_sets(ctx);

	if (stage == MESA_SHADER_TESS_CTRL ||
	    (stage == MESA_SHADER_VERTEX && ctx->args->options->key.vs_common_out.as_ls) ||
	    /* GFX9 has the ESGS ring buffer in LDS. */
	    (stage == MESA_SHADER_GEOMETRY && has_previous_stage)) {
		ac_declare_lds_as_pointer(&ctx->ac);
	}

}


static LLVMValueRef
radv_load_resource(struct ac_shader_abi *abi, LLVMValueRef index,
		   unsigned desc_set, unsigned binding)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef desc_ptr = ctx->descriptor_sets[desc_set];
	struct radv_pipeline_layout *pipeline_layout = ctx->args->options->layout;
	struct radv_descriptor_set_layout *layout = pipeline_layout->set[desc_set].layout;
	unsigned base_offset = layout->binding[binding].offset;
	LLVMValueRef offset, stride;

	if (layout->binding[binding].type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
	    layout->binding[binding].type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
		unsigned idx = pipeline_layout->set[desc_set].dynamic_offset_start +
			layout->binding[binding].dynamic_offset_offset;
		desc_ptr = ac_get_arg(&ctx->ac, ctx->args->ac.push_constants);
		base_offset = pipeline_layout->push_constant_size + 16 * idx;
		stride = LLVMConstInt(ctx->ac.i32, 16, false);
	} else
		stride = LLVMConstInt(ctx->ac.i32, layout->binding[binding].size, false);

	offset = LLVMConstInt(ctx->ac.i32, base_offset, false);

	if (layout->binding[binding].type != VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) {
		offset = ac_build_imad(&ctx->ac, index, stride, offset);
	}

	desc_ptr = LLVMBuildGEP(ctx->ac.builder, desc_ptr, &offset, 1, "");
	desc_ptr = ac_cast_ptr(&ctx->ac, desc_ptr, ctx->ac.v4i32);
	LLVMSetMetadata(desc_ptr, ctx->ac.uniform_md_kind, ctx->ac.empty_md);

	return desc_ptr;
}


/* The offchip buffer layout for TCS->TES is
 *
 * - attribute 0 of patch 0 vertex 0
 * - attribute 0 of patch 0 vertex 1
 * - attribute 0 of patch 0 vertex 2
 *   ...
 * - attribute 0 of patch 1 vertex 0
 * - attribute 0 of patch 1 vertex 1
 *   ...
 * - attribute 1 of patch 0 vertex 0
 * - attribute 1 of patch 0 vertex 1
 *   ...
 * - per patch attribute 0 of patch 0
 * - per patch attribute 0 of patch 1
 *   ...
 *
 * Note that every attribute has 4 components.
 */
static LLVMValueRef get_non_vertex_index_offset(struct radv_shader_context *ctx)
{
	uint32_t num_patches = ctx->tcs_num_patches;
	uint32_t num_tcs_outputs;
	if (ctx->stage == MESA_SHADER_TESS_CTRL)
		num_tcs_outputs = ctx->args->shader_info->tcs.num_linked_outputs;
	else
		num_tcs_outputs = ctx->args->shader_info->tes.num_linked_inputs;

	uint32_t output_vertex_size = num_tcs_outputs * 16;
	uint32_t pervertex_output_patch_size = ctx->shader->info.tess.tcs_vertices_out * output_vertex_size;

	return LLVMConstInt(ctx->ac.i32, pervertex_output_patch_size * num_patches, false);
}

static LLVMValueRef calc_param_stride(struct radv_shader_context *ctx,
				      LLVMValueRef vertex_index)
{
	LLVMValueRef param_stride;
	if (vertex_index)
		param_stride = LLVMConstInt(ctx->ac.i32, ctx->shader->info.tess.tcs_vertices_out * ctx->tcs_num_patches, false);
	else
		param_stride = LLVMConstInt(ctx->ac.i32, ctx->tcs_num_patches, false);
	return param_stride;
}

static LLVMValueRef get_tcs_tes_buffer_address(struct radv_shader_context *ctx,
                                               LLVMValueRef vertex_index,
                                               LLVMValueRef param_index)
{
	LLVMValueRef base_addr;
	LLVMValueRef param_stride, constant16;
	LLVMValueRef rel_patch_id = get_rel_patch_id(ctx);
	LLVMValueRef vertices_per_patch = LLVMConstInt(ctx->ac.i32, ctx->shader->info.tess.tcs_vertices_out, false);
	constant16 = LLVMConstInt(ctx->ac.i32, 16, false);
	param_stride = calc_param_stride(ctx, vertex_index);
	if (vertex_index) {
		base_addr = ac_build_imad(&ctx->ac, rel_patch_id,
					  vertices_per_patch, vertex_index);
	} else {
		base_addr = rel_patch_id;
	}

	base_addr = LLVMBuildAdd(ctx->ac.builder, base_addr,
	                         LLVMBuildMul(ctx->ac.builder, param_index,
	                                      param_stride, ""), "");

	base_addr = LLVMBuildMul(ctx->ac.builder, base_addr, constant16, "");

	if (!vertex_index) {
		LLVMValueRef patch_data_offset = get_non_vertex_index_offset(ctx);

		base_addr = LLVMBuildAdd(ctx->ac.builder, base_addr,
		                         patch_data_offset, "");
	}
	return base_addr;
}

static LLVMValueRef get_tcs_tes_buffer_address_params(struct radv_shader_context *ctx,
						      unsigned param,
						      LLVMValueRef vertex_index,
						      LLVMValueRef indir_index)
{
	LLVMValueRef param_index;

	if (indir_index)
		param_index = LLVMBuildAdd(ctx->ac.builder, LLVMConstInt(ctx->ac.i32, param, false),
					   indir_index, "");
	else {
		param_index = LLVMConstInt(ctx->ac.i32, param, false);
	}
	return get_tcs_tes_buffer_address(ctx, vertex_index, param_index);
}

static LLVMValueRef
get_dw_address(struct radv_shader_context *ctx,
	       LLVMValueRef dw_addr,
	       unsigned param,
	       LLVMValueRef vertex_index,
	       LLVMValueRef stride,
	       LLVMValueRef indir_index)

{

	if (vertex_index) {
		dw_addr = LLVMBuildAdd(ctx->ac.builder, dw_addr,
				       LLVMBuildMul(ctx->ac.builder,
						    vertex_index,
						    stride, ""), "");
	}

	if (indir_index)
		dw_addr = LLVMBuildAdd(ctx->ac.builder, dw_addr,
				       LLVMBuildMul(ctx->ac.builder, indir_index,
						    LLVMConstInt(ctx->ac.i32, 4, false), ""), "");

	dw_addr = LLVMBuildAdd(ctx->ac.builder, dw_addr,
			       LLVMConstInt(ctx->ac.i32, param * 4, false), "");

	return dw_addr;
}

static LLVMValueRef
load_tcs_varyings(struct ac_shader_abi *abi,
		  LLVMTypeRef type,
		  LLVMValueRef vertex_index,
		  LLVMValueRef indir_index,
		  unsigned driver_location,
		  unsigned component,
		  unsigned num_components,
		  bool load_input)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef dw_addr, stride;
	LLVMValueRef value[4], result;
	unsigned param = driver_location;

	bool is_patch = vertex_index == NULL;

	if (load_input) {
		uint32_t input_vertex_size = (ctx->tcs_num_inputs * 16) / 4;
		stride = LLVMConstInt(ctx->ac.i32, input_vertex_size, false);
		dw_addr = get_tcs_in_current_patch_offset(ctx);
	} else {
		if (!is_patch) {
			stride = get_tcs_out_vertex_stride(ctx);
			dw_addr = get_tcs_out_current_patch_offset(ctx);
		} else {
			dw_addr = get_tcs_out_current_patch_data_offset(ctx);
			stride = NULL;
		}
	}

	dw_addr = get_dw_address(ctx, dw_addr, param, vertex_index, stride, indir_index);

	for (unsigned i = 0; i < num_components + component; i++) {
		value[i] = ac_lds_load(&ctx->ac, dw_addr);
		dw_addr = LLVMBuildAdd(ctx->ac.builder, dw_addr,
				       ctx->ac.i32_1, "");
	}
	result = ac_build_varying_gather_values(&ctx->ac, value, num_components, component);
	return result;
}

static void
store_tcs_output(struct ac_shader_abi *abi,
		 LLVMValueRef vertex_index,
		 LLVMValueRef param_index,
		 LLVMValueRef src,
		 unsigned writemask,
		 unsigned component,
		 unsigned location,
		 unsigned driver_location)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	const bool is_patch = vertex_index == NULL;
	LLVMValueRef dw_addr;
	LLVMValueRef stride = NULL;
	LLVMValueRef buf_addr = NULL;
	LLVMValueRef oc_lds = ac_get_arg(&ctx->ac, ctx->args->oc_lds);
	unsigned param = driver_location;
	bool store_lds = true;

	if (is_patch) {
		if (!(ctx->shader->info.patch_outputs_read & (1U << (location - VARYING_SLOT_PATCH0))))
			store_lds = false;
	} else {
		if (!(ctx->shader->info.outputs_read & (1ULL << location)))
			store_lds = false;
	}

	if (!is_patch) {
		stride = get_tcs_out_vertex_stride(ctx);
		dw_addr = get_tcs_out_current_patch_offset(ctx);
	} else {
		dw_addr = get_tcs_out_current_patch_data_offset(ctx);
	}

	dw_addr = get_dw_address(ctx, dw_addr, param, vertex_index, stride, param_index);
	buf_addr = get_tcs_tes_buffer_address_params(ctx, param, vertex_index, param_index);

	bool is_tess_factor = false;
	if (location == VARYING_SLOT_TESS_LEVEL_INNER ||
	    location == VARYING_SLOT_TESS_LEVEL_OUTER)
		is_tess_factor = true;

	for (unsigned chan = 0; chan < 8; chan++) {
		if (!(writemask & (1 << chan)))
			continue;
		LLVMValueRef value = ac_llvm_extract_elem(&ctx->ac, src, chan - component);
		value = ac_to_integer(&ctx->ac, value);
		value = LLVMBuildZExtOrBitCast(ctx->ac.builder, value, ctx->ac.i32, "");

		if (store_lds || is_tess_factor) {
			LLVMValueRef dw_addr_chan =
				LLVMBuildAdd(ctx->ac.builder, dw_addr,
				                           LLVMConstInt(ctx->ac.i32, chan, false), "");
			ac_lds_store(&ctx->ac, dw_addr_chan, value);
		}

		if (!is_tess_factor && writemask != 0xF)
			ac_build_buffer_store_dword(&ctx->ac, ctx->hs_ring_tess_offchip, value, 1,
						    buf_addr, oc_lds,
						    4 * chan, ac_glc);
	}

	if (writemask == 0xF) {
		ac_build_buffer_store_dword(&ctx->ac, ctx->hs_ring_tess_offchip, src, 4,
					    buf_addr, oc_lds, 0, ac_glc);
	}
}

static LLVMValueRef
load_tes_input(struct ac_shader_abi *abi,
	       LLVMTypeRef type,
	       LLVMValueRef vertex_index,
	       LLVMValueRef param_index,
	       unsigned driver_location,
	       unsigned component,
	       unsigned num_components,
	       bool load_input)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef buf_addr;
	LLVMValueRef result;
	LLVMValueRef oc_lds = ac_get_arg(&ctx->ac, ctx->args->oc_lds);
	unsigned param = driver_location;

	buf_addr = get_tcs_tes_buffer_address_params(ctx, param, vertex_index, param_index);

	LLVMValueRef comp_offset = LLVMConstInt(ctx->ac.i32, component * 4, false);
	buf_addr = LLVMBuildAdd(ctx->ac.builder, buf_addr, comp_offset, "");

	result = ac_build_buffer_load(&ctx->ac, ctx->hs_ring_tess_offchip, num_components, NULL,
				      buf_addr, oc_lds, 0, ac_glc, true, false);
	result = ac_trim_vector(&ctx->ac, result, num_components);
	return result;
}

static LLVMValueRef
load_gs_input(struct ac_shader_abi *abi,
	      unsigned driver_location,
	      unsigned component,
	      unsigned num_components,
	      unsigned vertex_index,
	      LLVMTypeRef type)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef vtx_offset;
	unsigned param = driver_location;
	unsigned vtx_offset_param;
	LLVMValueRef value[4], result;

	vtx_offset_param = vertex_index;
	assert(vtx_offset_param < 6);
	vtx_offset = LLVMBuildMul(ctx->ac.builder, ctx->gs_vtx_offset[vtx_offset_param],
				  LLVMConstInt(ctx->ac.i32, 4, false), "");

	for (unsigned i = component; i < num_components + component; i++) {
		if (ctx->ac.chip_class >= GFX9) {
			LLVMValueRef dw_addr = ctx->gs_vtx_offset[vtx_offset_param];
			dw_addr = LLVMBuildAdd(ctx->ac.builder, dw_addr,
			                       LLVMConstInt(ctx->ac.i32, param * 4 + i, 0), "");
			value[i] = ac_lds_load(&ctx->ac, dw_addr);
		} else {
			LLVMValueRef soffset =
				LLVMConstInt(ctx->ac.i32,
					     (param * 4 + i) * 256,
					     false);

			value[i] = ac_build_buffer_load(&ctx->ac,
							ctx->esgs_ring, 1,
							ctx->ac.i32_0,
							vtx_offset, soffset,
							0, ac_glc, true, false);
		}

		if (ac_get_type_size(type) == 2) {
			value[i] = LLVMBuildBitCast(ctx->ac.builder, value[i], ctx->ac.i32, "");
			value[i] = LLVMBuildTrunc(ctx->ac.builder, value[i], ctx->ac.i16, "");
		}
		value[i] = LLVMBuildBitCast(ctx->ac.builder, value[i], type, "");
	}
	result = ac_build_varying_gather_values(&ctx->ac, value, num_components, component);
	result = ac_to_integer(&ctx->ac, result);
	return result;
}

static uint32_t
radv_get_sample_pos_offset(uint32_t num_samples)
{
	uint32_t sample_pos_offset = 0;

	switch (num_samples) {
	case 2:
		sample_pos_offset = 1;
		break;
	case 4:
		sample_pos_offset = 3;
		break;
	case 8:
		sample_pos_offset = 7;
		break;
	default:
		break;
	}
	return sample_pos_offset;
}

static LLVMValueRef load_sample_position(struct ac_shader_abi *abi,
					 LLVMValueRef sample_id)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);

	LLVMValueRef result;
	LLVMValueRef index = LLVMConstInt(ctx->ac.i32, RING_PS_SAMPLE_POSITIONS, false);
	LLVMValueRef ptr = LLVMBuildGEP(ctx->ac.builder, ctx->ring_offsets, &index, 1, "");

	ptr = LLVMBuildBitCast(ctx->ac.builder, ptr,
			       ac_array_in_const_addr_space(ctx->ac.v2f32), "");

	uint32_t sample_pos_offset =
		radv_get_sample_pos_offset(ctx->args->options->key.fs.num_samples);

	sample_id =
		LLVMBuildAdd(ctx->ac.builder, sample_id,
			     LLVMConstInt(ctx->ac.i32, sample_pos_offset, false), "");
	result = ac_build_load_invariant(&ctx->ac, ptr, sample_id);

	return result;
}


static LLVMValueRef load_sample_mask_in(struct ac_shader_abi *abi)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	uint8_t log2_ps_iter_samples;

	if (ctx->args->shader_info->ps.force_persample) {
		log2_ps_iter_samples =
			util_logbase2(ctx->args->options->key.fs.num_samples);
	} else {
		log2_ps_iter_samples = ctx->args->options->key.fs.log2_ps_iter_samples;
	}

	LLVMValueRef result, sample_id;
	if (log2_ps_iter_samples) {
		/* gl_SampleMaskIn[0] = (SampleCoverage & (1 << gl_SampleID)). */
		sample_id = ac_unpack_param(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->ac.ancillary), 8, 4);
		sample_id = LLVMBuildShl(ctx->ac.builder, LLVMConstInt(ctx->ac.i32, 1, false), sample_id, "");
		result = LLVMBuildAnd(ctx->ac.builder, sample_id,
				      ac_get_arg(&ctx->ac, ctx->args->ac.sample_coverage), "");
	} else {
		result = ac_get_arg(&ctx->ac, ctx->args->ac.sample_coverage);
	}

	return result;
}


static void gfx10_ngg_gs_emit_vertex(struct radv_shader_context *ctx,
				     unsigned stream,
				     LLVMValueRef vertexidx,
				     LLVMValueRef *addrs);

static void
visit_emit_vertex_with_counter(struct ac_shader_abi *abi, unsigned stream,
			       LLVMValueRef vertexidx, LLVMValueRef *addrs)
{
	unsigned offset = 0;
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);

	if (ctx->args->options->key.vs_common_out.as_ngg) {
		gfx10_ngg_gs_emit_vertex(ctx, stream, vertexidx, addrs);
		return;
	}

	for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
		unsigned output_usage_mask =
			ctx->args->shader_info->gs.output_usage_mask[i];
		uint8_t output_stream =
			ctx->args->shader_info->gs.output_streams[i];
		LLVMValueRef *out_ptr = &addrs[i * 4];
		int length = util_last_bit(output_usage_mask);

		if (!(ctx->output_mask & (1ull << i)) ||
		    output_stream != stream)
			continue;

		for (unsigned j = 0; j < length; j++) {
			if (!(output_usage_mask & (1 << j)))
				continue;

			LLVMValueRef out_val = LLVMBuildLoad(ctx->ac.builder,
							     out_ptr[j], "");
			LLVMValueRef voffset =
				LLVMConstInt(ctx->ac.i32, offset *
					     ctx->shader->info.gs.vertices_out, false);

			offset++;

			voffset = LLVMBuildAdd(ctx->ac.builder, voffset, vertexidx, "");
			voffset = LLVMBuildMul(ctx->ac.builder, voffset, LLVMConstInt(ctx->ac.i32, 4, false), "");

			out_val = ac_to_integer(&ctx->ac, out_val);
			out_val = LLVMBuildZExtOrBitCast(ctx->ac.builder, out_val, ctx->ac.i32, "");

			ac_build_buffer_store_dword(&ctx->ac,
						    ctx->gsvs_ring[stream],
						    out_val, 1,
						    voffset,
						    ac_get_arg(&ctx->ac,
							       ctx->args->gs2vs_offset),
						    0, ac_glc | ac_slc | ac_swizzled);
		}
	}

	ac_build_sendmsg(&ctx->ac,
			 AC_SENDMSG_GS_OP_EMIT | AC_SENDMSG_GS | (stream << 8),
			 ctx->gs_wave_id);
}

static void
visit_end_primitive(struct ac_shader_abi *abi, unsigned stream)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);

	if (ctx->args->options->key.vs_common_out.as_ngg) {
		LLVMBuildStore(ctx->ac.builder, ctx->ac.i32_0, ctx->gs_curprim_verts[stream]);
		return;
	}

	ac_build_sendmsg(&ctx->ac, AC_SENDMSG_GS_OP_CUT | AC_SENDMSG_GS | (stream << 8), ctx->gs_wave_id);
}

static LLVMValueRef
load_tess_coord(struct ac_shader_abi *abi)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);

	LLVMValueRef coord[4] = {
		ac_get_arg(&ctx->ac, ctx->args->tes_u),
		ac_get_arg(&ctx->ac, ctx->args->tes_v),
		ctx->ac.f32_0,
		ctx->ac.f32_0,
	};

	if (ctx->shader->info.tess.primitive_mode == GL_TRIANGLES)
		coord[2] = LLVMBuildFSub(ctx->ac.builder, ctx->ac.f32_1,
					LLVMBuildFAdd(ctx->ac.builder, coord[0], coord[1], ""), "");

	return ac_build_gather_values(&ctx->ac, coord, 3);
}

static LLVMValueRef
load_patch_vertices_in(struct ac_shader_abi *abi)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	return LLVMConstInt(ctx->ac.i32, ctx->args->options->key.tcs.input_vertices, false);
}


static LLVMValueRef radv_load_base_vertex(struct ac_shader_abi *abi)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	return ac_get_arg(&ctx->ac, ctx->args->ac.base_vertex);
}

static LLVMValueRef radv_load_ssbo(struct ac_shader_abi *abi,
				   LLVMValueRef buffer_ptr, bool write)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef result;

	LLVMSetMetadata(buffer_ptr, ctx->ac.uniform_md_kind, ctx->ac.empty_md);

	result = LLVMBuildLoad(ctx->ac.builder, buffer_ptr, "");
	LLVMSetMetadata(result, ctx->ac.invariant_load_md_kind, ctx->ac.empty_md);

	return result;
}

static LLVMValueRef radv_load_ubo(struct ac_shader_abi *abi,
				  unsigned desc_set, unsigned binding,
				  bool valid_binding, LLVMValueRef buffer_ptr)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef result;

	if (valid_binding) {
		struct radv_pipeline_layout *pipeline_layout = ctx->args->options->layout;
		struct radv_descriptor_set_layout *layout = pipeline_layout->set[desc_set].layout;

		if (layout->binding[binding].type == VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT) {
			uint32_t desc_type = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) |
					     S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
					     S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) |
					     S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W);

			if (ctx->ac.chip_class >= GFX10) {
				desc_type |= S_008F0C_FORMAT(V_008F0C_IMG_FORMAT_32_FLOAT) |
					     S_008F0C_OOB_SELECT(V_008F0C_OOB_SELECT_RAW) |
					     S_008F0C_RESOURCE_LEVEL(1);
			} else {
				desc_type |= S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) |
					     S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);
			}

			LLVMValueRef desc_components[4] = {
				LLVMBuildPtrToInt(ctx->ac.builder, buffer_ptr, ctx->ac.intptr, ""),
				LLVMConstInt(ctx->ac.i32, S_008F04_BASE_ADDRESS_HI(ctx->args->options->address32_hi), false),
				LLVMConstInt(ctx->ac.i32, 0xffffffff, false),
				LLVMConstInt(ctx->ac.i32, desc_type, false),
			};

			return ac_build_gather_values(&ctx->ac, desc_components, 4);
		}
	}

	LLVMSetMetadata(buffer_ptr, ctx->ac.uniform_md_kind, ctx->ac.empty_md);

	result = LLVMBuildLoad(ctx->ac.builder, buffer_ptr, "");
	LLVMSetMetadata(result, ctx->ac.invariant_load_md_kind, ctx->ac.empty_md);

	return result;
}

static LLVMValueRef radv_get_sampler_desc(struct ac_shader_abi *abi,
					  unsigned descriptor_set,
					  unsigned base_index,
					  unsigned constant_index,
					  LLVMValueRef index,
					  enum ac_descriptor_type desc_type,
					  bool image, bool write,
					  bool bindless)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);
	LLVMValueRef list = ctx->descriptor_sets[descriptor_set];
	struct radv_descriptor_set_layout *layout = ctx->args->options->layout->set[descriptor_set].layout;
	struct radv_descriptor_set_binding_layout *binding = layout->binding + base_index;
	unsigned offset = binding->offset;
	unsigned stride = binding->size;
	unsigned type_size;
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMTypeRef type;

	assert(base_index < layout->binding_count);

	switch (desc_type) {
	case AC_DESC_IMAGE:
		type = ctx->ac.v8i32;
		type_size = 32;
		break;
	case AC_DESC_FMASK:
		type = ctx->ac.v8i32;
		offset += 32;
		type_size = 32;
		break;
	case AC_DESC_SAMPLER:
		type = ctx->ac.v4i32;
		if (binding->type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
			offset += radv_combined_image_descriptor_sampler_offset(binding);
		}

		type_size = 16;
		break;
	case AC_DESC_BUFFER:
		type = ctx->ac.v4i32;
		type_size = 16;
		break;
	case AC_DESC_PLANE_0:
	case AC_DESC_PLANE_1:
	case AC_DESC_PLANE_2:
		type = ctx->ac.v8i32;
		type_size = 32;
		offset += 32 * (desc_type - AC_DESC_PLANE_0);
		break;
	default:
		unreachable("invalid desc_type\n");
	}

	offset += constant_index * stride;

	if (desc_type == AC_DESC_SAMPLER && binding->immutable_samplers_offset &&
	    (!index || binding->immutable_samplers_equal)) {
		if (binding->immutable_samplers_equal)
			constant_index = 0;

		const uint32_t *samplers = radv_immutable_samplers(layout, binding);

		LLVMValueRef constants[] = {
			LLVMConstInt(ctx->ac.i32, samplers[constant_index * 4 + 0], 0),
			LLVMConstInt(ctx->ac.i32, samplers[constant_index * 4 + 1], 0),
			LLVMConstInt(ctx->ac.i32, samplers[constant_index * 4 + 2], 0),
			LLVMConstInt(ctx->ac.i32, samplers[constant_index * 4 + 3], 0),
		};
		return ac_build_gather_values(&ctx->ac, constants, 4);
	}

	assert(stride % type_size == 0);

	LLVMValueRef adjusted_index = index;
	if (!adjusted_index)
		adjusted_index = ctx->ac.i32_0;

	adjusted_index = LLVMBuildMul(builder, adjusted_index, LLVMConstInt(ctx->ac.i32, stride / type_size, 0), "");

	LLVMValueRef val_offset = LLVMConstInt(ctx->ac.i32, offset, 0);
	list = LLVMBuildGEP(builder, list, &val_offset, 1, "");
	list = LLVMBuildPointerCast(builder, list,
				    ac_array_in_const32_addr_space(type), "");

	LLVMValueRef descriptor = ac_build_load_to_sgpr(&ctx->ac, list, adjusted_index);

	/* 3 plane formats always have same size and format for plane 1 & 2, so
	 * use the tail from plane 1 so that we can store only the first 16 bytes
	 * of the last plane. */
	if (desc_type == AC_DESC_PLANE_2) {
		LLVMValueRef descriptor2 = radv_get_sampler_desc(abi, descriptor_set, base_index, constant_index, index, AC_DESC_PLANE_1,image, write, bindless);

		LLVMValueRef components[8];
		for (unsigned i = 0; i < 4; ++i)
			components[i] = ac_llvm_extract_elem(&ctx->ac, descriptor, i);

		for (unsigned i = 4; i < 8; ++i)
			components[i] = ac_llvm_extract_elem(&ctx->ac, descriptor2, i);
		descriptor = ac_build_gather_values(&ctx->ac, components, 8);
	}

	return descriptor;
}

/* For 2_10_10_10 formats the alpha is handled as unsigned by pre-vega HW.
 * so we may need to fix it up. */
static LLVMValueRef
adjust_vertex_fetch_alpha(struct radv_shader_context *ctx,
                          unsigned adjustment,
                          LLVMValueRef alpha)
{
	if (adjustment == AC_FETCH_FORMAT_NONE)
		return alpha;

	LLVMValueRef c30 = LLVMConstInt(ctx->ac.i32, 30, 0);

	alpha = LLVMBuildBitCast(ctx->ac.builder, alpha, ctx->ac.f32, "");

	if (adjustment == AC_FETCH_FORMAT_SSCALED)
		alpha = LLVMBuildFPToUI(ctx->ac.builder, alpha, ctx->ac.i32, "");
	else
		alpha = ac_to_integer(&ctx->ac, alpha);

	/* For the integer-like cases, do a natural sign extension.
	 *
	 * For the SNORM case, the values are 0.0, 0.333, 0.666, 1.0
	 * and happen to contain 0, 1, 2, 3 as the two LSBs of the
	 * exponent.
	 */
	alpha = LLVMBuildShl(ctx->ac.builder, alpha,
	                     adjustment == AC_FETCH_FORMAT_SNORM ?
	                     LLVMConstInt(ctx->ac.i32, 7, 0) : c30, "");
	alpha = LLVMBuildAShr(ctx->ac.builder, alpha, c30, "");

	/* Convert back to the right type. */
	if (adjustment == AC_FETCH_FORMAT_SNORM) {
		LLVMValueRef clamp;
		LLVMValueRef neg_one = LLVMConstReal(ctx->ac.f32, -1.0);
		alpha = LLVMBuildSIToFP(ctx->ac.builder, alpha, ctx->ac.f32, "");
		clamp = LLVMBuildFCmp(ctx->ac.builder, LLVMRealULT, alpha, neg_one, "");
		alpha = LLVMBuildSelect(ctx->ac.builder, clamp, neg_one, alpha, "");
	} else if (adjustment == AC_FETCH_FORMAT_SSCALED) {
		alpha = LLVMBuildSIToFP(ctx->ac.builder, alpha, ctx->ac.f32, "");
	}

	return LLVMBuildBitCast(ctx->ac.builder, alpha, ctx->ac.i32, "");
}

static LLVMValueRef
radv_fixup_vertex_input_fetches(struct radv_shader_context *ctx,
				LLVMValueRef value,
				unsigned num_channels,
				bool is_float)
{
	LLVMValueRef zero = is_float ? ctx->ac.f32_0 : ctx->ac.i32_0;
	LLVMValueRef one = is_float ? ctx->ac.f32_1 : ctx->ac.i32_1;
	LLVMValueRef chan[4];

	if (LLVMGetTypeKind(LLVMTypeOf(value)) == LLVMVectorTypeKind) {
		unsigned vec_size = LLVMGetVectorSize(LLVMTypeOf(value));

		if (num_channels == 4 && num_channels == vec_size)
			return value;

		num_channels = MIN2(num_channels, vec_size);

		for (unsigned i = 0; i < num_channels; i++)
			chan[i] = ac_llvm_extract_elem(&ctx->ac, value, i);
	} else {
		assert(num_channels == 1);
		chan[0] = value;
	}

	for (unsigned i = num_channels; i < 4; i++) {
		chan[i] = i == 3 ? one : zero;
		chan[i] = ac_to_integer(&ctx->ac, chan[i]);
	}

	return ac_build_gather_values(&ctx->ac, chan, 4);
}

static void
handle_vs_input_decl(struct radv_shader_context *ctx,
		     struct nir_variable *variable)
{
	LLVMValueRef t_list_ptr = ac_get_arg(&ctx->ac, ctx->args->vertex_buffers);
	LLVMValueRef t_offset;
	LLVMValueRef t_list;
	LLVMValueRef input;
	LLVMValueRef buffer_index;
	unsigned attrib_count = glsl_count_attribute_slots(variable->type, true);


	enum glsl_base_type type = glsl_get_base_type(variable->type);
	for (unsigned i = 0; i < attrib_count; ++i) {
		LLVMValueRef output[4];
		unsigned attrib_index = variable->data.location + i - VERT_ATTRIB_GENERIC0;
		unsigned attrib_format = ctx->args->options->key.vs.vertex_attribute_formats[attrib_index];
		unsigned data_format = attrib_format & 0x0f;
		unsigned num_format = (attrib_format >> 4) & 0x07;
		bool is_float = num_format != V_008F0C_BUF_NUM_FORMAT_UINT &&
		                num_format != V_008F0C_BUF_NUM_FORMAT_SINT;
		uint8_t input_usage_mask =
			ctx->args->shader_info->vs.input_usage_mask[variable->data.location + i];
		unsigned num_input_channels = util_last_bit(input_usage_mask);

		if (num_input_channels == 0)
			continue;

		if (ctx->args->options->key.vs.instance_rate_inputs & (1u << attrib_index)) {
			uint32_t divisor = ctx->args->options->key.vs.instance_rate_divisors[attrib_index];

			if (divisor) {
				buffer_index = ctx->abi.instance_id;

				if (divisor != 1) {
					buffer_index = LLVMBuildUDiv(ctx->ac.builder, buffer_index,
					                             LLVMConstInt(ctx->ac.i32, divisor, 0), "");
				}
			} else {
				buffer_index = ctx->ac.i32_0;
			}

			buffer_index = LLVMBuildAdd(ctx->ac.builder,
						    ac_get_arg(&ctx->ac,
							       ctx->args->ac.start_instance),\
						    buffer_index, "");
		} else {
			buffer_index = LLVMBuildAdd(ctx->ac.builder,
						    ctx->abi.vertex_id,
			                            ac_get_arg(&ctx->ac,
							       ctx->args->ac.base_vertex), "");
		}

		const struct ac_data_format_info *vtx_info = ac_get_data_format_info(data_format);

		/* Adjust the number of channels to load based on the vertex
		 * attribute format.
		 */
		unsigned num_channels = MIN2(num_input_channels, vtx_info->num_channels);
		unsigned attrib_binding = ctx->args->options->key.vs.vertex_attribute_bindings[attrib_index];
		unsigned attrib_offset = ctx->args->options->key.vs.vertex_attribute_offsets[attrib_index];
		unsigned attrib_stride = ctx->args->options->key.vs.vertex_attribute_strides[attrib_index];
		unsigned alpha_adjust = ctx->args->options->key.vs.alpha_adjust[attrib_index];

		if (ctx->args->options->key.vs.post_shuffle & (1 << attrib_index)) {
			/* Always load, at least, 3 channels for formats that
			 * need to be shuffled because X<->Z.
			 */
			num_channels = MAX2(num_channels, 3);
		}

		t_offset = LLVMConstInt(ctx->ac.i32, attrib_binding, false);
		t_list = ac_build_load_to_sgpr(&ctx->ac, t_list_ptr, t_offset);

		/* Always split typed vertex buffer loads on GFX6 and GFX10+
		 * to avoid any alignment issues that triggers memory
		 * violations and eventually a GPU hang. This can happen if
		 * the stride (static or dynamic) is unaligned and also if the
		 * VBO offset is aligned to a scalar (eg. stride is 8 and VBO
		 * offset is 2 for R16G16B16A16_SNORM).
		 */
		if (ctx->ac.chip_class == GFX6 ||
		    ctx->ac.chip_class >= GFX10) {
			unsigned chan_format = vtx_info->chan_format;
			LLVMValueRef values[4];

			assert(ctx->ac.chip_class == GFX6 ||
			       ctx->ac.chip_class >= GFX10);

			for (unsigned chan  = 0; chan < num_channels; chan++) {
				unsigned chan_offset = attrib_offset + chan * vtx_info->chan_byte_size;
				LLVMValueRef chan_index = buffer_index;

				if (attrib_stride != 0 && chan_offset > attrib_stride) {
					LLVMValueRef buffer_offset =
						LLVMConstInt(ctx->ac.i32,
							     chan_offset / attrib_stride, false);

					chan_index = LLVMBuildAdd(ctx->ac.builder,
								  buffer_index,
								  buffer_offset, "");

					chan_offset = chan_offset % attrib_stride;
				}

				values[chan] = ac_build_struct_tbuffer_load(&ctx->ac, t_list,
									   chan_index,
									   LLVMConstInt(ctx->ac.i32, chan_offset, false),
									   ctx->ac.i32_0, ctx->ac.i32_0, 1,
									   chan_format, num_format, 0, true);
			}

			input = ac_build_gather_values(&ctx->ac, values, num_channels);
		} else {
			if (attrib_stride != 0 && attrib_offset > attrib_stride) {
				LLVMValueRef buffer_offset =
					LLVMConstInt(ctx->ac.i32,
						     attrib_offset / attrib_stride, false);

				buffer_index = LLVMBuildAdd(ctx->ac.builder,
							    buffer_index,
							    buffer_offset, "");

				attrib_offset = attrib_offset % attrib_stride;
			}

			input = ac_build_struct_tbuffer_load(&ctx->ac, t_list,
							     buffer_index,
							     LLVMConstInt(ctx->ac.i32, attrib_offset, false),
							     ctx->ac.i32_0, ctx->ac.i32_0,
							     num_channels,
							     data_format, num_format, 0, true);
		}

		if (ctx->args->options->key.vs.post_shuffle & (1 << attrib_index)) {
			LLVMValueRef c[4];
			c[0] = ac_llvm_extract_elem(&ctx->ac, input, 2);
			c[1] = ac_llvm_extract_elem(&ctx->ac, input, 1);
			c[2] = ac_llvm_extract_elem(&ctx->ac, input, 0);
			c[3] = ac_llvm_extract_elem(&ctx->ac, input, 3);

			input = ac_build_gather_values(&ctx->ac, c, 4);
		}

		input = radv_fixup_vertex_input_fetches(ctx, input, num_channels,
							is_float);

		for (unsigned chan = 0; chan < 4; chan++) {
			LLVMValueRef llvm_chan = LLVMConstInt(ctx->ac.i32, chan, false);
			output[chan] = LLVMBuildExtractElement(ctx->ac.builder, input, llvm_chan, "");
			if (type == GLSL_TYPE_FLOAT16) {
				output[chan] = LLVMBuildBitCast(ctx->ac.builder, output[chan], ctx->ac.f32, "");
				output[chan] = LLVMBuildFPTrunc(ctx->ac.builder, output[chan], ctx->ac.f16, "");
			}
		}

		output[3] = adjust_vertex_fetch_alpha(ctx, alpha_adjust, output[3]);

		for (unsigned chan = 0; chan < 4; chan++) {
			output[chan] = ac_to_integer(&ctx->ac, output[chan]);
			if (type == GLSL_TYPE_UINT16 || type == GLSL_TYPE_INT16)
				output[chan] = LLVMBuildTrunc(ctx->ac.builder, output[chan], ctx->ac.i16, "");

			ctx->inputs[ac_llvm_reg_index_soa(variable->data.location + i, chan)] = output[chan];
		}
	}
}

static void
handle_vs_inputs(struct radv_shader_context *ctx,
                 struct nir_shader *nir) {
	nir_foreach_shader_in_variable(variable, nir)
		handle_vs_input_decl(ctx, variable);
}

static void
prepare_interp_optimize(struct radv_shader_context *ctx,
                        struct nir_shader *nir)
{
	bool uses_center = false;
	bool uses_centroid = false;
	nir_foreach_shader_in_variable(variable, nir) {
		if (glsl_get_base_type(glsl_without_array(variable->type)) != GLSL_TYPE_FLOAT ||
		    variable->data.sample)
			continue;

		if (variable->data.centroid)
			uses_centroid = true;
		else
			uses_center = true;
	}

	ctx->abi.persp_centroid = ac_get_arg(&ctx->ac, ctx->args->ac.persp_centroid);
	ctx->abi.linear_centroid = ac_get_arg(&ctx->ac, ctx->args->ac.linear_centroid);

	if (uses_center && uses_centroid) {
		LLVMValueRef sel = LLVMBuildICmp(ctx->ac.builder, LLVMIntSLT,
						 ac_get_arg(&ctx->ac, ctx->args->ac.prim_mask),
						 ctx->ac.i32_0, "");
		ctx->abi.persp_centroid =
			LLVMBuildSelect(ctx->ac.builder, sel,
					ac_get_arg(&ctx->ac, ctx->args->ac.persp_center),
					ctx->abi.persp_centroid, "");
		ctx->abi.linear_centroid =
			LLVMBuildSelect(ctx->ac.builder, sel,
					ac_get_arg(&ctx->ac, ctx->args->ac.linear_center),
					ctx->abi.linear_centroid, "");
	}
}

static void
scan_shader_output_decl(struct radv_shader_context *ctx,
			struct nir_variable *variable,
			struct nir_shader *shader,
			gl_shader_stage stage)
{
	int idx = variable->data.driver_location;
	unsigned attrib_count = glsl_count_attribute_slots(variable->type, false);
	uint64_t mask_attribs;

	/* tess ctrl has it's own load/store paths for outputs */
	if (stage == MESA_SHADER_TESS_CTRL) {
		/* Remember driver location of tess factors, so we can read
		 * them later, in write_tess_factors.
		 */
		if (variable->data.location == VARYING_SLOT_TESS_LEVEL_INNER) {
			ctx->tcs_tess_lvl_inner = idx;
		} else if (variable->data.location == VARYING_SLOT_TESS_LEVEL_OUTER) {
			ctx->tcs_tess_lvl_outer = idx;
		}
		return;
	}

	if (variable->data.compact) {
		unsigned component_count = variable->data.location_frac +
		                           glsl_get_length(variable->type);
		attrib_count = (component_count + 3) / 4;
	}

	mask_attribs = ((1ull << attrib_count) - 1) << idx;

	ctx->output_mask |= mask_attribs;
}


/* Initialize arguments for the shader export intrinsic */
static void
si_llvm_init_export_args(struct radv_shader_context *ctx,
			 LLVMValueRef *values,
			 unsigned enabled_channels,
			 unsigned target,
			 struct ac_export_args *args)
{
	/* Specify the channels that are enabled. */
	args->enabled_channels = enabled_channels;

	/* Specify whether the EXEC mask represents the valid mask */
	args->valid_mask = 0;

	/* Specify whether this is the last export */
	args->done = 0;

	/* Specify the target we are exporting */
	args->target = target;

	args->compr = false;
	args->out[0] = LLVMGetUndef(ctx->ac.f32);
	args->out[1] = LLVMGetUndef(ctx->ac.f32);
	args->out[2] = LLVMGetUndef(ctx->ac.f32);
	args->out[3] = LLVMGetUndef(ctx->ac.f32);

	if (!values)
		return;

	bool is_16bit = ac_get_type_size(LLVMTypeOf(values[0])) == 2;
	if (ctx->stage == MESA_SHADER_FRAGMENT) {
		unsigned index = target - V_008DFC_SQ_EXP_MRT;
		unsigned col_format = (ctx->args->options->key.fs.col_format >> (4 * index)) & 0xf;
		bool is_int8 = (ctx->args->options->key.fs.is_int8 >> index) & 1;
		bool is_int10 = (ctx->args->options->key.fs.is_int10 >> index) & 1;
		unsigned chan;

		LLVMValueRef (*packf)(struct ac_llvm_context *ctx, LLVMValueRef args[2]) = NULL;
		LLVMValueRef (*packi)(struct ac_llvm_context *ctx, LLVMValueRef args[2],
				      unsigned bits, bool hi) = NULL;

		switch(col_format) {
		case V_028714_SPI_SHADER_ZERO:
			args->enabled_channels = 0; /* writemask */
			args->target = V_008DFC_SQ_EXP_NULL;
			break;

		case V_028714_SPI_SHADER_32_R:
			args->enabled_channels = 1;
			args->out[0] = values[0];
			break;

		case V_028714_SPI_SHADER_32_GR:
			args->enabled_channels = 0x3;
			args->out[0] = values[0];
			args->out[1] = values[1];
			break;

		case V_028714_SPI_SHADER_32_AR:
			if (ctx->ac.chip_class >= GFX10) {
				args->enabled_channels = 0x3;
				args->out[0] = values[0];
				args->out[1] = values[3];
			} else {
				args->enabled_channels = 0x9;
				args->out[0] = values[0];
				args->out[3] = values[3];
			}
			break;

		case V_028714_SPI_SHADER_FP16_ABGR:
			args->enabled_channels = 0x5;
			packf = ac_build_cvt_pkrtz_f16;
			if (is_16bit) {
				for (unsigned chan = 0; chan < 4; chan++)
					values[chan] = LLVMBuildFPExt(ctx->ac.builder,
								      values[chan],
								      ctx->ac.f32, "");
			}
			break;

		case V_028714_SPI_SHADER_UNORM16_ABGR:
			args->enabled_channels = 0x5;
			packf = ac_build_cvt_pknorm_u16;
			break;

		case V_028714_SPI_SHADER_SNORM16_ABGR:
			args->enabled_channels = 0x5;
			packf = ac_build_cvt_pknorm_i16;
			break;

		case V_028714_SPI_SHADER_UINT16_ABGR:
			args->enabled_channels = 0x5;
			packi = ac_build_cvt_pk_u16;
			if (is_16bit) {
				for (unsigned chan = 0; chan < 4; chan++)
					values[chan] = LLVMBuildZExt(ctx->ac.builder,
								      ac_to_integer(&ctx->ac, values[chan]),
								      ctx->ac.i32, "");
			}
			break;

		case V_028714_SPI_SHADER_SINT16_ABGR:
			args->enabled_channels = 0x5;
			packi = ac_build_cvt_pk_i16;
			if (is_16bit) {
				for (unsigned chan = 0; chan < 4; chan++)
					values[chan] = LLVMBuildSExt(ctx->ac.builder,
								      ac_to_integer(&ctx->ac, values[chan]),
								      ctx->ac.i32, "");
			}
			break;

		default:
		case V_028714_SPI_SHADER_32_ABGR:
			memcpy(&args->out[0], values, sizeof(values[0]) * 4);
			break;
		}

		/* Replace NaN by zero (only 32-bit) to fix game bugs if
		 * requested.
		 */
		if (ctx->args->options->enable_mrt_output_nan_fixup &&
		    !is_16bit &&
		    (col_format == V_028714_SPI_SHADER_32_R ||
		     col_format == V_028714_SPI_SHADER_32_GR ||
		     col_format == V_028714_SPI_SHADER_32_AR ||
		     col_format == V_028714_SPI_SHADER_32_ABGR ||
		     col_format == V_028714_SPI_SHADER_FP16_ABGR)) {
			for (unsigned i = 0; i < 4; i++) {
				LLVMValueRef args[2] = {
					values[i],
					LLVMConstInt(ctx->ac.i32, S_NAN | Q_NAN, false)
				};
				LLVMValueRef isnan =
					ac_build_intrinsic(&ctx->ac, "llvm.amdgcn.class.f32", ctx->ac.i1,
		                                           args, 2, AC_FUNC_ATTR_READNONE);
				values[i] = LLVMBuildSelect(ctx->ac.builder, isnan,
							    ctx->ac.f32_0,
							    values[i], "");
			}
		}

		/* Pack f16 or norm_i16/u16. */
		if (packf) {
			for (chan = 0; chan < 2; chan++) {
				LLVMValueRef pack_args[2] = {
					values[2 * chan],
					values[2 * chan + 1]
				};
				LLVMValueRef packed;

				packed = packf(&ctx->ac, pack_args);
				args->out[chan] = ac_to_float(&ctx->ac, packed);
			}
			args->compr = 1; /* COMPR flag */
		}

		/* Pack i16/u16. */
		if (packi) {
			for (chan = 0; chan < 2; chan++) {
				LLVMValueRef pack_args[2] = {
					ac_to_integer(&ctx->ac, values[2 * chan]),
					ac_to_integer(&ctx->ac, values[2 * chan + 1])
				};
				LLVMValueRef packed;

				packed = packi(&ctx->ac, pack_args,
					       is_int8 ? 8 : is_int10 ? 10 : 16,
					       chan == 1);
				args->out[chan] = ac_to_float(&ctx->ac, packed);
			}
			args->compr = 1; /* COMPR flag */
		}
		return;
	}

	if (is_16bit) {
		for (unsigned chan = 0; chan < 4; chan++) {
			values[chan] = LLVMBuildBitCast(ctx->ac.builder, values[chan], ctx->ac.i16, "");
			args->out[chan] = LLVMBuildZExt(ctx->ac.builder, values[chan], ctx->ac.i32, "");
		}
	} else
		memcpy(&args->out[0], values, sizeof(values[0]) * 4);

	for (unsigned i = 0; i < 4; ++i)
		args->out[i] = ac_to_float(&ctx->ac, args->out[i]);
}

static void
radv_export_param(struct radv_shader_context *ctx, unsigned index,
		  LLVMValueRef *values, unsigned enabled_channels)
{
	struct ac_export_args args;

	si_llvm_init_export_args(ctx, values, enabled_channels,
				 V_008DFC_SQ_EXP_PARAM + index, &args);
	ac_build_export(&ctx->ac, &args);
}

static LLVMValueRef
radv_load_output(struct radv_shader_context *ctx, unsigned index, unsigned chan)
{
	LLVMValueRef output = ctx->abi.outputs[ac_llvm_reg_index_soa(index, chan)];
	return LLVMBuildLoad(ctx->ac.builder, output, "");
}

static void
radv_emit_stream_output(struct radv_shader_context *ctx,
			 LLVMValueRef const *so_buffers,
			 LLVMValueRef const *so_write_offsets,
			 const struct radv_stream_output *output,
			 struct radv_shader_output_values *shader_out)
{
	unsigned num_comps = util_bitcount(output->component_mask);
	unsigned buf = output->buffer;
	unsigned offset = output->offset;
	unsigned start;
	LLVMValueRef out[4];

	assert(num_comps && num_comps <= 4);
	if (!num_comps || num_comps > 4)
		return;

	/* Get the first component. */
	start = ffs(output->component_mask) - 1;

	/* Load the output as int. */
	for (int i = 0; i < num_comps; i++) {
		out[i] = ac_to_integer(&ctx->ac, shader_out->values[start + i]);
	}

	/* Pack the output. */
	LLVMValueRef vdata = NULL;

	switch (num_comps) {
	case 1: /* as i32 */
		vdata = out[0];
		break;
	case 2: /* as v2i32 */
	case 3: /* as v4i32 (aligned to 4) */
		out[3] = LLVMGetUndef(ctx->ac.i32);
		/* fall through */
	case 4: /* as v4i32 */
		vdata = ac_build_gather_values(&ctx->ac, out,
					       !ac_has_vec3_support(ctx->ac.chip_class, false) ?
					       util_next_power_of_two(num_comps) :
					       num_comps);
		break;
	}

	ac_build_buffer_store_dword(&ctx->ac, so_buffers[buf],
				    vdata, num_comps, so_write_offsets[buf],
				    ctx->ac.i32_0, offset,
				    ac_glc | ac_slc);
}

static void
radv_emit_streamout(struct radv_shader_context *ctx, unsigned stream)
{
	int i;

	/* Get bits [22:16], i.e. (so_param >> 16) & 127; */
	assert(ctx->args->streamout_config.used);
	LLVMValueRef so_vtx_count =
		ac_build_bfe(&ctx->ac,
			     ac_get_arg(&ctx->ac, ctx->args->streamout_config),
			     LLVMConstInt(ctx->ac.i32, 16, false),
			     LLVMConstInt(ctx->ac.i32, 7, false), false);

	LLVMValueRef tid = ac_get_thread_id(&ctx->ac);

	/* can_emit = tid < so_vtx_count; */
	LLVMValueRef can_emit = LLVMBuildICmp(ctx->ac.builder, LLVMIntULT,
					      tid, so_vtx_count, "");

	/* Emit the streamout code conditionally. This actually avoids
	 * out-of-bounds buffer access. The hw tells us via the SGPR
	 * (so_vtx_count) which threads are allowed to emit streamout data.
	 */
	ac_build_ifcc(&ctx->ac, can_emit, 6501);
	{
		/* The buffer offset is computed as follows:
		 *   ByteOffset = streamout_offset[buffer_id]*4 +
		 *                (streamout_write_index + thread_id)*stride[buffer_id] +
		 *                attrib_offset
		 */
		LLVMValueRef so_write_index =
			ac_get_arg(&ctx->ac, ctx->args->streamout_write_idx);

		/* Compute (streamout_write_index + thread_id). */
		so_write_index =
			LLVMBuildAdd(ctx->ac.builder, so_write_index, tid, "");

		/* Load the descriptor and compute the write offset for each
		 * enabled buffer.
		 */
		LLVMValueRef so_write_offset[4] = {0};
		LLVMValueRef so_buffers[4] = {0};
		LLVMValueRef buf_ptr = ac_get_arg(&ctx->ac, ctx->args->streamout_buffers);

		for (i = 0; i < 4; i++) {
			uint16_t stride = ctx->args->shader_info->so.strides[i];

			if (!stride)
				continue;

			LLVMValueRef offset =
				LLVMConstInt(ctx->ac.i32, i, false);

			so_buffers[i] = ac_build_load_to_sgpr(&ctx->ac,
							      buf_ptr, offset);

			LLVMValueRef so_offset =
				ac_get_arg(&ctx->ac, ctx->args->streamout_offset[i]);

			so_offset = LLVMBuildMul(ctx->ac.builder, so_offset,
						 LLVMConstInt(ctx->ac.i32, 4, false), "");

			so_write_offset[i] =
				ac_build_imad(&ctx->ac, so_write_index,
					      LLVMConstInt(ctx->ac.i32,
							   stride * 4, false),
					      so_offset);
		}

		/* Write streamout data. */
		for (i = 0; i < ctx->args->shader_info->so.num_outputs; i++) {
			struct radv_shader_output_values shader_out = {0};
			struct radv_stream_output *output =
				&ctx->args->shader_info->so.outputs[i];

			if (stream != output->stream)
				continue;

			for (int j = 0; j < 4; j++) {
				shader_out.values[j] =
					radv_load_output(ctx, output->location, j);
			}

			radv_emit_stream_output(ctx, so_buffers,so_write_offset,
						output, &shader_out);
		}
	}
	ac_build_endif(&ctx->ac, 6501);
}

static void
radv_build_param_exports(struct radv_shader_context *ctx,
			 struct radv_shader_output_values *outputs,
			 unsigned noutput,
			 struct radv_vs_output_info *outinfo,
			 bool export_clip_dists)
{
	unsigned param_count = 0;

	for (unsigned i = 0; i < noutput; i++) {
		unsigned slot_name = outputs[i].slot_name;
		unsigned usage_mask = outputs[i].usage_mask;

		if (slot_name != VARYING_SLOT_LAYER &&
		    slot_name != VARYING_SLOT_PRIMITIVE_ID &&
		    slot_name != VARYING_SLOT_VIEWPORT &&
		    slot_name != VARYING_SLOT_CLIP_DIST0 &&
		    slot_name != VARYING_SLOT_CLIP_DIST1 &&
		    slot_name < VARYING_SLOT_VAR0)
			continue;

		if ((slot_name == VARYING_SLOT_CLIP_DIST0 ||
		     slot_name == VARYING_SLOT_CLIP_DIST1) && !export_clip_dists)
			continue;

		radv_export_param(ctx, param_count, outputs[i].values, usage_mask);

		assert(i < ARRAY_SIZE(outinfo->vs_output_param_offset));
		outinfo->vs_output_param_offset[slot_name] = param_count++;
        }

	outinfo->param_exports = param_count;
}

/* Generate export instructions for hardware VS shader stage or NGG GS stage
 * (position and parameter data only).
 */
static void
radv_llvm_export_vs(struct radv_shader_context *ctx,
                    struct radv_shader_output_values *outputs,
                    unsigned noutput,
                    struct radv_vs_output_info *outinfo,
		    bool export_clip_dists)
{
	LLVMValueRef psize_value = NULL, layer_value = NULL, viewport_value = NULL;
	struct ac_export_args pos_args[4] = {0};
	unsigned pos_idx, index;
	int i;

	/* Build position exports */
	for (i = 0; i < noutput; i++) {
		switch (outputs[i].slot_name) {
		case VARYING_SLOT_POS:
			si_llvm_init_export_args(ctx, outputs[i].values, 0xf,
						 V_008DFC_SQ_EXP_POS, &pos_args[0]);
			break;
		case VARYING_SLOT_PSIZ:
			psize_value = outputs[i].values[0];
			break;
		case VARYING_SLOT_LAYER:
			layer_value = outputs[i].values[0];
			break;
		case VARYING_SLOT_VIEWPORT:
			viewport_value = outputs[i].values[0];
			break;
		case VARYING_SLOT_CLIP_DIST0:
		case VARYING_SLOT_CLIP_DIST1:
			index = 2 + outputs[i].slot_index;
			si_llvm_init_export_args(ctx, outputs[i].values, 0xf,
						 V_008DFC_SQ_EXP_POS + index,
						 &pos_args[index]);
			break;
		default:
			break;
		}
	}

	/* We need to add the position output manually if it's missing. */
	if (!pos_args[0].out[0]) {
		pos_args[0].enabled_channels = 0xf; /* writemask */
		pos_args[0].valid_mask = 0; /* EXEC mask */
		pos_args[0].done = 0; /* last export? */
		pos_args[0].target = V_008DFC_SQ_EXP_POS;
		pos_args[0].compr = 0; /* COMPR flag */
		pos_args[0].out[0] = ctx->ac.f32_0; /* X */
		pos_args[0].out[1] = ctx->ac.f32_0; /* Y */
		pos_args[0].out[2] = ctx->ac.f32_0; /* Z */
		pos_args[0].out[3] = ctx->ac.f32_1;  /* W */
	}

	if (outinfo->writes_pointsize ||
	    outinfo->writes_layer ||
	    outinfo->writes_viewport_index) {
		pos_args[1].enabled_channels = ((outinfo->writes_pointsize == true ? 1 : 0) |
						(outinfo->writes_layer == true ? 4 : 0));
		pos_args[1].valid_mask = 0;
		pos_args[1].done = 0;
		pos_args[1].target = V_008DFC_SQ_EXP_POS + 1;
		pos_args[1].compr = 0;
		pos_args[1].out[0] = ctx->ac.f32_0; /* X */
		pos_args[1].out[1] = ctx->ac.f32_0; /* Y */
		pos_args[1].out[2] = ctx->ac.f32_0; /* Z */
		pos_args[1].out[3] = ctx->ac.f32_0;  /* W */

		if (outinfo->writes_pointsize == true)
			pos_args[1].out[0] = psize_value;
		if (outinfo->writes_layer == true)
			pos_args[1].out[2] = layer_value;
		if (outinfo->writes_viewport_index == true) {
			if (ctx->args->options->chip_class >= GFX9) {
				/* GFX9 has the layer in out.z[10:0] and the viewport
				 * index in out.z[19:16].
				 */
				LLVMValueRef v = viewport_value;
				v = ac_to_integer(&ctx->ac, v);
				v = LLVMBuildShl(ctx->ac.builder, v,
						 LLVMConstInt(ctx->ac.i32, 16, false),
						 "");
				v = LLVMBuildOr(ctx->ac.builder, v,
						ac_to_integer(&ctx->ac, pos_args[1].out[2]), "");

				pos_args[1].out[2] = ac_to_float(&ctx->ac, v);
				pos_args[1].enabled_channels |= 1 << 2;
			} else {
				pos_args[1].out[3] = viewport_value;
				pos_args[1].enabled_channels |= 1 << 3;
			}
		}
	}

	for (i = 0; i < 4; i++) {
		if (pos_args[i].out[0])
			outinfo->pos_exports++;
	}

	/* GFX10 skip POS0 exports if EXEC=0 and DONE=0, causing a hang.
	 * Setting valid_mask=1 prevents it and has no other effect.
	 */
	if (ctx->ac.chip_class == GFX10)
		pos_args[0].valid_mask = 1;

	pos_idx = 0;
	for (i = 0; i < 4; i++) {
		if (!pos_args[i].out[0])
			continue;

		/* Specify the target we are exporting */
		pos_args[i].target = V_008DFC_SQ_EXP_POS + pos_idx++;

		if (pos_idx == outinfo->pos_exports)
			/* Specify that this is the last export */
			pos_args[i].done = 1;

		ac_build_export(&ctx->ac, &pos_args[i]);
	}

	/* Build parameter exports */
	radv_build_param_exports(ctx, outputs, noutput, outinfo, export_clip_dists);
}

static void
handle_vs_outputs_post(struct radv_shader_context *ctx,
		       bool export_prim_id,
		       bool export_clip_dists,
		       struct radv_vs_output_info *outinfo)
{
	struct radv_shader_output_values *outputs;
	unsigned noutput = 0;

	if (ctx->args->options->key.has_multiview_view_index) {
		LLVMValueRef* tmp_out = &ctx->abi.outputs[ac_llvm_reg_index_soa(VARYING_SLOT_LAYER, 0)];
		if(!*tmp_out) {
			for(unsigned i = 0; i < 4; ++i)
				ctx->abi.outputs[ac_llvm_reg_index_soa(VARYING_SLOT_LAYER, i)] =
				            ac_build_alloca_undef(&ctx->ac, ctx->ac.f32, "");
		}

		LLVMValueRef view_index = ac_get_arg(&ctx->ac, ctx->args->ac.view_index);
		LLVMBuildStore(ctx->ac.builder, ac_to_float(&ctx->ac, view_index), *tmp_out);
		ctx->output_mask |= 1ull << VARYING_SLOT_LAYER;
	}

	memset(outinfo->vs_output_param_offset, AC_EXP_PARAM_UNDEFINED,
	       sizeof(outinfo->vs_output_param_offset));
	outinfo->pos_exports = 0;

	if (!ctx->args->options->use_ngg_streamout &&
	    ctx->args->shader_info->so.num_outputs &&
	    !ctx->args->is_gs_copy_shader) {
		/* The GS copy shader emission already emits streamout. */
		radv_emit_streamout(ctx, 0);
	}

	/* Allocate a temporary array for the output values. */
	unsigned num_outputs = util_bitcount64(ctx->output_mask) + export_prim_id;
	outputs = malloc(num_outputs * sizeof(outputs[0]));

	for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
		if (!(ctx->output_mask & (1ull << i)))
			continue;

		outputs[noutput].slot_name = i;
		outputs[noutput].slot_index = i == VARYING_SLOT_CLIP_DIST1;

		if (ctx->stage == MESA_SHADER_VERTEX &&
		    !ctx->args->is_gs_copy_shader) {
			outputs[noutput].usage_mask =
				ctx->args->shader_info->vs.output_usage_mask[i];
		} else if (ctx->stage == MESA_SHADER_TESS_EVAL) {
			outputs[noutput].usage_mask =
				ctx->args->shader_info->tes.output_usage_mask[i];
		} else {
			assert(ctx->args->is_gs_copy_shader);
			outputs[noutput].usage_mask =
				ctx->args->shader_info->gs.output_usage_mask[i];
		}

		for (unsigned j = 0; j < 4; j++) {
			outputs[noutput].values[j] =
				ac_to_float(&ctx->ac, radv_load_output(ctx, i, j));
		}

		noutput++;
	}

	/* Export PrimitiveID. */
	if (export_prim_id) {
		outputs[noutput].slot_name = VARYING_SLOT_PRIMITIVE_ID;
		outputs[noutput].slot_index = 0;
		outputs[noutput].usage_mask = 0x1;
		if (ctx->stage == MESA_SHADER_TESS_EVAL)
			outputs[noutput].values[0] =
				ac_get_arg(&ctx->ac, ctx->args->ac.tes_patch_id);
		else
			outputs[noutput].values[0] =
				ac_get_arg(&ctx->ac, ctx->args->vs_prim_id);
		for (unsigned j = 1; j < 4; j++)
			outputs[noutput].values[j] = ctx->ac.f32_0;
		noutput++;
	}

	radv_llvm_export_vs(ctx, outputs, noutput, outinfo, export_clip_dists);

	free(outputs);
}

static void
handle_es_outputs_post(struct radv_shader_context *ctx,
		       struct radv_es_output_info *outinfo)
{
	int j;
	LLVMValueRef lds_base = NULL;

	if (ctx->ac.chip_class  >= GFX9) {
		unsigned itemsize_dw = outinfo->esgs_itemsize / 4;
		LLVMValueRef vertex_idx = ac_get_thread_id(&ctx->ac);
		LLVMValueRef wave_idx =
			ac_unpack_param(&ctx->ac,
					ac_get_arg(&ctx->ac, ctx->args->merged_wave_info), 24, 4);
		vertex_idx = LLVMBuildOr(ctx->ac.builder, vertex_idx,
					 LLVMBuildMul(ctx->ac.builder, wave_idx,
						      LLVMConstInt(ctx->ac.i32,
								   ctx->ac.wave_size, false), ""), "");
		lds_base = LLVMBuildMul(ctx->ac.builder, vertex_idx,
					LLVMConstInt(ctx->ac.i32, itemsize_dw, 0), "");
	}

	for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
		LLVMValueRef dw_addr = NULL;
		LLVMValueRef *out_ptr = &ctx->abi.outputs[i * 4];
		unsigned output_usage_mask;

		if (!(ctx->output_mask & (1ull << i)))
			continue;

		if (ctx->stage == MESA_SHADER_VERTEX) {
			output_usage_mask =
				ctx->args->shader_info->vs.output_usage_mask[i];
		} else {
			assert(ctx->stage == MESA_SHADER_TESS_EVAL);
			output_usage_mask =
				ctx->args->shader_info->tes.output_usage_mask[i];
		}

		if (lds_base) {
			dw_addr = LLVMBuildAdd(ctx->ac.builder, lds_base,
			                       LLVMConstInt(ctx->ac.i32, i * 4, false),
			                       "");
		}

		for (j = 0; j < 4; j++) {
			if (!(output_usage_mask & (1 << j)))
				continue;

			LLVMValueRef out_val = LLVMBuildLoad(ctx->ac.builder, out_ptr[j], "");
			out_val = ac_to_integer(&ctx->ac, out_val);
			out_val = LLVMBuildZExtOrBitCast(ctx->ac.builder, out_val, ctx->ac.i32, "");

			if (ctx->ac.chip_class  >= GFX9) {
				LLVMValueRef dw_addr_offset =
					LLVMBuildAdd(ctx->ac.builder, dw_addr,
						     LLVMConstInt(ctx->ac.i32,
								  j, false), "");

				ac_lds_store(&ctx->ac, dw_addr_offset, out_val);
			} else {
				ac_build_buffer_store_dword(&ctx->ac,
				                            ctx->esgs_ring,
				                            out_val, 1,
				                            NULL,
							    ac_get_arg(&ctx->ac, ctx->args->es2gs_offset),
				                            (4 * i + j) * 4,
				                            ac_glc | ac_slc | ac_swizzled);
			}
		}
	}
}

static void
handle_ls_outputs_post(struct radv_shader_context *ctx)
{
	LLVMValueRef vertex_id = ctx->rel_auto_id;
	uint32_t num_tcs_inputs = ctx->args->shader_info->vs.num_linked_outputs;
	LLVMValueRef vertex_dw_stride = LLVMConstInt(ctx->ac.i32, num_tcs_inputs * 4, false);
	LLVMValueRef base_dw_addr = LLVMBuildMul(ctx->ac.builder, vertex_id,
						 vertex_dw_stride, "");

	for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
		LLVMValueRef *out_ptr = &ctx->abi.outputs[i * 4];

		if (!(ctx->output_mask & (1ull << i)))
			continue;

		LLVMValueRef dw_addr = LLVMBuildAdd(ctx->ac.builder, base_dw_addr,
						    LLVMConstInt(ctx->ac.i32, i * 4, false),
						    "");
		for (unsigned j = 0; j < 4; j++) {
			LLVMValueRef value = LLVMBuildLoad(ctx->ac.builder, out_ptr[j], "");
			value = ac_to_integer(&ctx->ac, value);
			value = LLVMBuildZExtOrBitCast(ctx->ac.builder, value, ctx->ac.i32, "");
			ac_lds_store(&ctx->ac, dw_addr, value);
			dw_addr = LLVMBuildAdd(ctx->ac.builder, dw_addr, ctx->ac.i32_1, "");
		}
	}
}

static LLVMValueRef get_wave_id_in_tg(struct radv_shader_context *ctx)
{
	return ac_unpack_param(&ctx->ac,
			       ac_get_arg(&ctx->ac, ctx->args->merged_wave_info), 24, 4);
}

static LLVMValueRef get_tgsize(struct radv_shader_context *ctx)
{
	return ac_unpack_param(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->merged_wave_info), 28, 4);
}

static LLVMValueRef get_thread_id_in_tg(struct radv_shader_context *ctx)
{
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef tmp;
	tmp = LLVMBuildMul(builder, get_wave_id_in_tg(ctx),
			   LLVMConstInt(ctx->ac.i32, ctx->ac.wave_size, false), "");
	return LLVMBuildAdd(builder, tmp, ac_get_thread_id(&ctx->ac), "");
}

static LLVMValueRef ngg_get_vtx_cnt(struct radv_shader_context *ctx)
{
	return ac_build_bfe(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->gs_tg_info),
			    LLVMConstInt(ctx->ac.i32, 12, false),
			    LLVMConstInt(ctx->ac.i32, 9, false),
			    false);
}

static LLVMValueRef ngg_get_prim_cnt(struct radv_shader_context *ctx)
{
	return ac_build_bfe(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->gs_tg_info),
			    LLVMConstInt(ctx->ac.i32, 22, false),
			    LLVMConstInt(ctx->ac.i32, 9, false),
			    false);
}

static LLVMValueRef ngg_get_ordered_id(struct radv_shader_context *ctx)
{
	return ac_build_bfe(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->gs_tg_info),
			    ctx->ac.i32_0,
			    LLVMConstInt(ctx->ac.i32, 12, false),
			    false);
}

static LLVMValueRef
ngg_gs_get_vertex_storage(struct radv_shader_context *ctx)
{
	unsigned num_outputs = util_bitcount64(ctx->output_mask);

	if (ctx->args->options->key.has_multiview_view_index)
		num_outputs++;

	LLVMTypeRef elements[2] = {
		LLVMArrayType(ctx->ac.i32, 4 * num_outputs),
		LLVMArrayType(ctx->ac.i8, 4),
	};
	LLVMTypeRef type = LLVMStructTypeInContext(ctx->ac.context, elements, 2, false);
	type = LLVMPointerType(LLVMArrayType(type, 0), AC_ADDR_SPACE_LDS);
	return LLVMBuildBitCast(ctx->ac.builder, ctx->gs_ngg_emit, type, "");
}

/**
 * Return a pointer to the LDS storage reserved for the N'th vertex, where N
 * is in emit order; that is:
 * - during the epilogue, N is the threadidx (relative to the entire threadgroup)
 * - during vertex emit, i.e. while the API GS shader invocation is running,
 *   N = threadidx * gs_max_out_vertices + emitidx
 *
 * Goals of the LDS memory layout:
 * 1. Eliminate bank conflicts on write for geometry shaders that have all emits
 *    in uniform control flow
 * 2. Eliminate bank conflicts on read for export if, additionally, there is no
 *    culling
 * 3. Agnostic to the number of waves (since we don't know it before compiling)
 * 4. Allow coalescing of LDS instructions (ds_write_b128 etc.)
 * 5. Avoid wasting memory.
 *
 * We use an AoS layout due to point 4 (this also helps point 3). In an AoS
 * layout, elimination of bank conflicts requires that each vertex occupy an
 * odd number of dwords. We use the additional dword to store the output stream
 * index as well as a flag to indicate whether this vertex ends a primitive
 * for rasterization.
 *
 * Swizzling is required to satisfy points 1 and 2 simultaneously.
 *
 * Vertices are stored in export order (gsthread * gs_max_out_vertices + emitidx).
 * Indices are swizzled in groups of 32, which ensures point 1 without
 * disturbing point 2.
 *
 * \return an LDS pointer to type {[N x i32], [4 x i8]}
 */
static LLVMValueRef
ngg_gs_vertex_ptr(struct radv_shader_context *ctx, LLVMValueRef vertexidx)
{
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef storage = ngg_gs_get_vertex_storage(ctx);

	/* gs_max_out_vertices = 2^(write_stride_2exp) * some odd number */
	unsigned write_stride_2exp = ffs(ctx->shader->info.gs.vertices_out) - 1;
	if (write_stride_2exp) {
		LLVMValueRef row =
			LLVMBuildLShr(builder, vertexidx,
				      LLVMConstInt(ctx->ac.i32, 5, false), "");
		LLVMValueRef swizzle =
			LLVMBuildAnd(builder, row,
				     LLVMConstInt(ctx->ac.i32, (1u << write_stride_2exp) - 1,
						  false), "");
		vertexidx = LLVMBuildXor(builder, vertexidx, swizzle, "");
	}

	return ac_build_gep0(&ctx->ac, storage, vertexidx);
}

static LLVMValueRef
ngg_gs_emit_vertex_ptr(struct radv_shader_context *ctx, LLVMValueRef gsthread,
		       LLVMValueRef emitidx)
{
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef tmp;

	tmp = LLVMConstInt(ctx->ac.i32, ctx->shader->info.gs.vertices_out, false);
	tmp = LLVMBuildMul(builder, tmp, gsthread, "");
	const LLVMValueRef vertexidx = LLVMBuildAdd(builder, tmp, emitidx, "");
	return ngg_gs_vertex_ptr(ctx, vertexidx);
}

static LLVMValueRef
ngg_gs_get_emit_output_ptr(struct radv_shader_context *ctx, LLVMValueRef vertexptr,
			   unsigned out_idx)
{
	LLVMValueRef gep_idx[3] = {
		ctx->ac.i32_0, /* implied C-style array */
		ctx->ac.i32_0, /* first struct entry */
		LLVMConstInt(ctx->ac.i32, out_idx, false),
	};
	return LLVMBuildGEP(ctx->ac.builder, vertexptr, gep_idx, 3, "");
}

static LLVMValueRef
ngg_gs_get_emit_primflag_ptr(struct radv_shader_context *ctx, LLVMValueRef vertexptr,
			     unsigned stream)
{
	LLVMValueRef gep_idx[3] = {
		ctx->ac.i32_0, /* implied C-style array */
		ctx->ac.i32_1, /* second struct entry */
		LLVMConstInt(ctx->ac.i32, stream, false),
	};
	return LLVMBuildGEP(ctx->ac.builder, vertexptr, gep_idx, 3, "");
}

static struct radv_stream_output *
radv_get_stream_output_by_loc(struct radv_streamout_info *so, unsigned location)
{
	for (unsigned i = 0; i < so->num_outputs; ++i) {
		if (so->outputs[i].location == location)
			return &so->outputs[i];
	}

	return NULL;
}

static void build_streamout_vertex(struct radv_shader_context *ctx,
				   LLVMValueRef *so_buffer, LLVMValueRef *wg_offset_dw,
				   unsigned stream, LLVMValueRef offset_vtx,
				   LLVMValueRef vertexptr)
{
	struct radv_streamout_info *so = &ctx->args->shader_info->so;
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef offset[4] = {0};
	LLVMValueRef tmp;

	for (unsigned buffer = 0; buffer < 4; ++buffer) {
		if (!wg_offset_dw[buffer])
			continue;

		tmp = LLVMBuildMul(builder, offset_vtx,
				   LLVMConstInt(ctx->ac.i32, so->strides[buffer], false), "");
		tmp = LLVMBuildAdd(builder, wg_offset_dw[buffer], tmp, "");
		offset[buffer] = LLVMBuildShl(builder, tmp, LLVMConstInt(ctx->ac.i32, 2, false), "");
	}

	if (ctx->stage == MESA_SHADER_GEOMETRY) {
		struct radv_shader_output_values outputs[AC_LLVM_MAX_OUTPUTS];
		unsigned noutput = 0;
		unsigned out_idx = 0;

		for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
			unsigned output_usage_mask =
				ctx->args->shader_info->gs.output_usage_mask[i];
			uint8_t output_stream = ctx->args->shader_info->gs.output_streams[i];

			if (!(ctx->output_mask & (1ull << i)) ||
			    output_stream != stream)
				continue;

			outputs[noutput].slot_name = i;
			outputs[noutput].slot_index = i == VARYING_SLOT_CLIP_DIST1;
			outputs[noutput].usage_mask = output_usage_mask;

			int length = util_last_bit(output_usage_mask);

			for (unsigned j = 0; j < length; j++, out_idx++) {
				if (!(output_usage_mask & (1 << j)))
					continue;

				tmp = ac_build_gep0(&ctx->ac, vertexptr,
						    LLVMConstInt(ctx->ac.i32, out_idx, false));
				outputs[noutput].values[j] = LLVMBuildLoad(builder, tmp, "");
			}

			for (unsigned j = length; j < 4; j++)
				outputs[noutput].values[j] = LLVMGetUndef(ctx->ac.f32);

			noutput++;
		}

		for (unsigned i = 0; i < noutput; i++) {
			struct radv_stream_output *output =
				radv_get_stream_output_by_loc(so, outputs[i].slot_name);

			if (!output ||
			    output->stream != stream)
				continue;

			struct radv_shader_output_values out = {0};

			for (unsigned j = 0; j < 4; j++) {
				out.values[j] = outputs[i].values[j];
			}

			radv_emit_stream_output(ctx, so_buffer, offset, output, &out);
		}
	} else {
		for (unsigned i = 0; i < so->num_outputs; ++i) {
			struct radv_stream_output *output =
				&ctx->args->shader_info->so.outputs[i];

			if (stream != output->stream)
				continue;

			struct radv_shader_output_values out = {0};

			for (unsigned comp = 0; comp < 4; comp++) {
				if (!(output->component_mask & (1 << comp)))
					continue;

				tmp = ac_build_gep0(&ctx->ac, vertexptr,
						    LLVMConstInt(ctx->ac.i32, 4 * i + comp, false));
				out.values[comp] = LLVMBuildLoad(builder, tmp, "");
			}

			radv_emit_stream_output(ctx, so_buffer, offset, output, &out);
		}
	}
}

struct ngg_streamout {
	LLVMValueRef num_vertices;

	/* per-thread data */
	LLVMValueRef prim_enable[4]; /* i1 per stream */
	LLVMValueRef vertices[3]; /* [N x i32] addrspace(LDS)* */

	/* Output */
	LLVMValueRef emit[4]; /* per-stream emitted primitives (only valid for used streams) */
};

/**
 * Build streamout logic.
 *
 * Implies a barrier.
 *
 * Writes number of emitted primitives to gs_ngg_scratch[4:7].
 *
 * Clobbers gs_ngg_scratch[8:].
 */
static void build_streamout(struct radv_shader_context *ctx,
			    struct ngg_streamout *nggso)
{
	struct radv_streamout_info *so = &ctx->args->shader_info->so;
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef buf_ptr = ac_get_arg(&ctx->ac, ctx->args->streamout_buffers);
	LLVMValueRef tid = get_thread_id_in_tg(ctx);
	LLVMValueRef cond, tmp, tmp2;
	LLVMValueRef i32_2 = LLVMConstInt(ctx->ac.i32, 2, false);
	LLVMValueRef i32_4 = LLVMConstInt(ctx->ac.i32, 4, false);
	LLVMValueRef i32_8 = LLVMConstInt(ctx->ac.i32, 8, false);
	LLVMValueRef so_buffer[4] = {0};
	unsigned max_num_vertices = 1 + (nggso->vertices[1] ? 1 : 0) +
					(nggso->vertices[2] ? 1 : 0);
	LLVMValueRef prim_stride_dw[4] = {0};
	LLVMValueRef prim_stride_dw_vgpr = LLVMGetUndef(ctx->ac.i32);
	int stream_for_buffer[4] = { -1, -1, -1, -1 };
	unsigned bufmask_for_stream[4] = {0};
	bool isgs = ctx->stage == MESA_SHADER_GEOMETRY;
	unsigned scratch_emit_base = isgs ? 4 : 0;
	LLVMValueRef scratch_emit_basev = isgs ? i32_4 : ctx->ac.i32_0;
	unsigned scratch_offset_base = isgs ? 8 : 4;
	LLVMValueRef scratch_offset_basev = isgs ? i32_8 : i32_4;

	ac_llvm_add_target_dep_function_attr(ctx->main_function,
					     "amdgpu-gds-size", 256);

	/* Determine the mapping of streamout buffers to vertex streams. */
	for (unsigned i = 0; i < so->num_outputs; ++i) {
		unsigned buf = so->outputs[i].buffer;
		unsigned stream = so->outputs[i].stream;
		assert(stream_for_buffer[buf] < 0 || stream_for_buffer[buf] == stream);
		stream_for_buffer[buf] = stream;
		bufmask_for_stream[stream] |= 1 << buf;
	}

	for (unsigned buffer = 0; buffer < 4; ++buffer) {
		if (stream_for_buffer[buffer] == -1)
			continue;

		assert(so->strides[buffer]);

		LLVMValueRef stride_for_buffer =
			LLVMConstInt(ctx->ac.i32, so->strides[buffer], false);
		prim_stride_dw[buffer] =
			LLVMBuildMul(builder, stride_for_buffer,
				     nggso->num_vertices, "");
		prim_stride_dw_vgpr = ac_build_writelane(
			&ctx->ac, prim_stride_dw_vgpr, prim_stride_dw[buffer],
			LLVMConstInt(ctx->ac.i32, buffer, false));

		LLVMValueRef offset = LLVMConstInt(ctx->ac.i32, buffer, false);
		so_buffer[buffer] = ac_build_load_to_sgpr(&ctx->ac, buf_ptr,
							  offset);
	}

	cond = LLVMBuildICmp(builder, LLVMIntEQ, get_wave_id_in_tg(ctx), ctx->ac.i32_0, "");
	ac_build_ifcc(&ctx->ac, cond, 5200);
	{
		LLVMTypeRef gdsptr = LLVMPointerType(ctx->ac.i32, AC_ADDR_SPACE_GDS);
		LLVMValueRef gdsbase = LLVMBuildIntToPtr(builder, ctx->ac.i32_0, gdsptr, "");

		/* Advance the streamout offsets in GDS. */
		LLVMValueRef offsets_vgpr = ac_build_alloca_undef(&ctx->ac, ctx->ac.i32, "");
		LLVMValueRef generated_by_stream_vgpr = ac_build_alloca_undef(&ctx->ac, ctx->ac.i32, "");

		cond = LLVMBuildICmp(builder, LLVMIntULT, ac_get_thread_id(&ctx->ac), i32_4, "");
		ac_build_ifcc(&ctx->ac, cond, 5210);
		{
			/* Fetch the number of generated primitives and store
			 * it in GDS for later use.
			 */
			if (isgs) {
				tmp = ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch, tid);
				tmp = LLVMBuildLoad(builder, tmp, "");
			} else {
				tmp = ac_build_writelane(&ctx->ac, ctx->ac.i32_0,
						ngg_get_prim_cnt(ctx), ctx->ac.i32_0);
			}
			LLVMBuildStore(builder, tmp, generated_by_stream_vgpr);

			unsigned swizzle[4];
			int unused_stream = -1;
			for (unsigned stream = 0; stream < 4; ++stream) {
				if (!ctx->args->shader_info->gs.num_stream_output_components[stream]) {
					unused_stream = stream;
					break;
				}
			}
			for (unsigned buffer = 0; buffer < 4; ++buffer) {
				if (stream_for_buffer[buffer] >= 0) {
					swizzle[buffer] = stream_for_buffer[buffer];
				} else {
					assert(unused_stream >= 0);
					swizzle[buffer] = unused_stream;
				}
			}

			tmp = ac_build_quad_swizzle(&ctx->ac, tmp,
				swizzle[0], swizzle[1], swizzle[2], swizzle[3]);
			tmp = LLVMBuildMul(builder, tmp, prim_stride_dw_vgpr, "");

			LLVMValueRef args[] = {
				LLVMBuildIntToPtr(builder, ngg_get_ordered_id(ctx), gdsptr, ""),
				tmp,
				ctx->ac.i32_0, // ordering
				ctx->ac.i32_0, // scope
				ctx->ac.i1false, // isVolatile
				LLVMConstInt(ctx->ac.i32, 4 << 24, false), // OA index
				ctx->ac.i1true, // wave release
				ctx->ac.i1true, // wave done
			};

			tmp = ac_build_intrinsic(&ctx->ac, "llvm.amdgcn.ds.ordered.add",
						 ctx->ac.i32, args, ARRAY_SIZE(args), 0);

			/* Keep offsets in a VGPR for quick retrieval via readlane by
			 * the first wave for bounds checking, and also store in LDS
			 * for retrieval by all waves later. */
			LLVMBuildStore(builder, tmp, offsets_vgpr);

			tmp2 = LLVMBuildAdd(builder, ac_get_thread_id(&ctx->ac),
					    scratch_offset_basev, "");
			tmp2 = ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch, tmp2);
			LLVMBuildStore(builder, tmp, tmp2);
		}
		ac_build_endif(&ctx->ac, 5210);

		/* Determine the max emit per buffer. This is done via the SALU, in part
		 * because LLVM can't generate divide-by-multiply if we try to do this
		 * via VALU with one lane per buffer.
		 */
		LLVMValueRef max_emit[4] = {0};
		for (unsigned buffer = 0; buffer < 4; ++buffer) {
			if (stream_for_buffer[buffer] == -1)
				continue;

			/* Compute the streamout buffer size in DWORD. */
			LLVMValueRef bufsize_dw =
				LLVMBuildLShr(builder,
					LLVMBuildExtractElement(builder, so_buffer[buffer], i32_2, ""),
					i32_2, "");

			/* Load the streamout buffer offset from GDS. */
			tmp = LLVMBuildLoad(builder, offsets_vgpr, "");
			LLVMValueRef offset_dw =
				ac_build_readlane(&ctx->ac, tmp,
						LLVMConstInt(ctx->ac.i32, buffer, false));

			/* Compute the remaining size to emit. */
			LLVMValueRef remaining_dw =
				LLVMBuildSub(builder, bufsize_dw, offset_dw, "");
			tmp = LLVMBuildUDiv(builder, remaining_dw,
					    prim_stride_dw[buffer], "");

			cond = LLVMBuildICmp(builder, LLVMIntULT,
					     bufsize_dw, offset_dw, "");
			max_emit[buffer] = LLVMBuildSelect(builder, cond,
							   ctx->ac.i32_0, tmp, "");
		}

		/* Determine the number of emitted primitives per stream and fixup the
		 * GDS counter if necessary.
		 *
		 * This is complicated by the fact that a single stream can emit to
		 * multiple buffers (but luckily not vice versa).
		 */
		LLVMValueRef emit_vgpr = ctx->ac.i32_0;

		for (unsigned stream = 0; stream < 4; ++stream) {
			if (!ctx->args->shader_info->gs.num_stream_output_components[stream])
				continue;

			/* Load the number of generated primitives from GDS and
			 * determine that number for the given stream.
			 */
			tmp = LLVMBuildLoad(builder, generated_by_stream_vgpr, "");
			LLVMValueRef generated =
				ac_build_readlane(&ctx->ac, tmp,
						  LLVMConstInt(ctx->ac.i32, stream, false));


			/* Compute the number of emitted primitives. */
			LLVMValueRef emit = generated;
			for (unsigned buffer = 0; buffer < 4; ++buffer) {
				if (stream_for_buffer[buffer] == stream)
					emit = ac_build_umin(&ctx->ac, emit, max_emit[buffer]);
			}

			/* Store the number of emitted primitives for that
			 * stream.
			 */
			emit_vgpr = ac_build_writelane(&ctx->ac, emit_vgpr, emit,
						       LLVMConstInt(ctx->ac.i32, stream, false));

			/* Fixup the offset using a plain GDS atomic if we overflowed. */
			cond = LLVMBuildICmp(builder, LLVMIntULT, emit, generated, "");
			ac_build_ifcc(&ctx->ac, cond, 5221); /* scalar branch */
			tmp = LLVMBuildLShr(builder,
					    LLVMConstInt(ctx->ac.i32, bufmask_for_stream[stream], false),
					    ac_get_thread_id(&ctx->ac), "");
			tmp = LLVMBuildTrunc(builder, tmp, ctx->ac.i1, "");
			ac_build_ifcc(&ctx->ac, tmp, 5222);
			{
				tmp = LLVMBuildSub(builder, generated, emit, "");
				tmp = LLVMBuildMul(builder, tmp, prim_stride_dw_vgpr, "");
				tmp2 = LLVMBuildGEP(builder, gdsbase, &tid, 1, "");
				LLVMBuildAtomicRMW(builder, LLVMAtomicRMWBinOpSub, tmp2, tmp,
						   LLVMAtomicOrderingMonotonic, false);
			}
			ac_build_endif(&ctx->ac, 5222);
			ac_build_endif(&ctx->ac, 5221);
		}

		/* Store the number of emitted primitives to LDS for later use. */
		cond = LLVMBuildICmp(builder, LLVMIntULT, ac_get_thread_id(&ctx->ac), i32_4, "");
		ac_build_ifcc(&ctx->ac, cond, 5225);
		{
			tmp = LLVMBuildAdd(builder, ac_get_thread_id(&ctx->ac),
					   scratch_emit_basev, "");
			tmp = ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch, tmp);
			LLVMBuildStore(builder, emit_vgpr, tmp);
		}
		ac_build_endif(&ctx->ac, 5225);
	}
	ac_build_endif(&ctx->ac, 5200);

	/* Determine the workgroup-relative per-thread / primitive offset into
	 * the streamout buffers */
	struct ac_wg_scan primemit_scan[4] = {0};

	if (isgs) {
		for (unsigned stream = 0; stream < 4; ++stream) {
			if (!ctx->args->shader_info->gs.num_stream_output_components[stream])
				continue;

			primemit_scan[stream].enable_exclusive = true;
			primemit_scan[stream].op = nir_op_iadd;
			primemit_scan[stream].src = nggso->prim_enable[stream];
			primemit_scan[stream].scratch =
				ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch,
					LLVMConstInt(ctx->ac.i32, 12 + 8 * stream, false));
			primemit_scan[stream].waveidx = get_wave_id_in_tg(ctx);
			primemit_scan[stream].numwaves = get_tgsize(ctx);
			primemit_scan[stream].maxwaves = 8;
			ac_build_wg_scan_top(&ctx->ac, &primemit_scan[stream]);
		}
	}

	ac_build_s_barrier(&ctx->ac);

	/* Fetch the per-buffer offsets and per-stream emit counts in all waves. */
	LLVMValueRef wgoffset_dw[4] = {0};

	{
		LLVMValueRef scratch_vgpr;

		tmp = ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch, ac_get_thread_id(&ctx->ac));
		scratch_vgpr = LLVMBuildLoad(builder, tmp, "");

		for (unsigned buffer = 0; buffer < 4; ++buffer) {
			if (stream_for_buffer[buffer] >= 0) {
				wgoffset_dw[buffer] = ac_build_readlane(
					&ctx->ac, scratch_vgpr,
					LLVMConstInt(ctx->ac.i32, scratch_offset_base + buffer, false));
			}
		}

		for (unsigned stream = 0; stream < 4; ++stream) {
			if (ctx->args->shader_info->gs.num_stream_output_components[stream]) {
				nggso->emit[stream] = ac_build_readlane(
					&ctx->ac, scratch_vgpr,
					LLVMConstInt(ctx->ac.i32, scratch_emit_base + stream, false));
			}
		}
	}

	/* Write out primitive data */
	for (unsigned stream = 0; stream < 4; ++stream) {
		if (!ctx->args->shader_info->gs.num_stream_output_components[stream])
			continue;

		if (isgs) {
			ac_build_wg_scan_bottom(&ctx->ac, &primemit_scan[stream]);
		} else {
			primemit_scan[stream].result_exclusive = tid;
		}

		cond = LLVMBuildICmp(builder, LLVMIntULT,
				    primemit_scan[stream].result_exclusive,
				    nggso->emit[stream], "");
		cond = LLVMBuildAnd(builder, cond, nggso->prim_enable[stream], "");
		ac_build_ifcc(&ctx->ac, cond, 5240);
		{
			LLVMValueRef offset_vtx =
				LLVMBuildMul(builder, primemit_scan[stream].result_exclusive,
					     nggso->num_vertices, "");

			for (unsigned i = 0; i < max_num_vertices; ++i) {
				cond = LLVMBuildICmp(builder, LLVMIntULT,
						    LLVMConstInt(ctx->ac.i32, i, false),
						    nggso->num_vertices, "");
				ac_build_ifcc(&ctx->ac, cond, 5241);
				build_streamout_vertex(ctx, so_buffer, wgoffset_dw,
						       stream, offset_vtx, nggso->vertices[i]);
				ac_build_endif(&ctx->ac, 5241);
				offset_vtx = LLVMBuildAdd(builder, offset_vtx, ctx->ac.i32_1, "");
			}
		}
		ac_build_endif(&ctx->ac, 5240);
	}
}

static unsigned ngg_nogs_vertex_size(struct radv_shader_context *ctx)
{
	unsigned lds_vertex_size = 0;

	if (ctx->args->shader_info->so.num_outputs)
		lds_vertex_size = 4 * ctx->args->shader_info->so.num_outputs + 1;

	return lds_vertex_size;
}

/**
 * Returns an `[N x i32] addrspace(LDS)*` pointing at contiguous LDS storage
 * for the vertex outputs.
 */
static LLVMValueRef ngg_nogs_vertex_ptr(struct radv_shader_context *ctx,
					LLVMValueRef vtxid)
{
	/* The extra dword is used to avoid LDS bank conflicts. */
	unsigned vertex_size = ngg_nogs_vertex_size(ctx);
	LLVMTypeRef ai32 = LLVMArrayType(ctx->ac.i32, vertex_size);
	LLVMTypeRef pai32 = LLVMPointerType(ai32, AC_ADDR_SPACE_LDS);
	LLVMValueRef tmp = LLVMBuildBitCast(ctx->ac.builder, ctx->esgs_ring, pai32, "");
	return LLVMBuildGEP(ctx->ac.builder, tmp, &vtxid, 1, "");
}

static void
handle_ngg_outputs_post_1(struct radv_shader_context *ctx)
{
	struct radv_streamout_info *so = &ctx->args->shader_info->so;
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef vertex_ptr = NULL;
	LLVMValueRef tmp, tmp2;

	assert((ctx->stage == MESA_SHADER_VERTEX ||
	        ctx->stage == MESA_SHADER_TESS_EVAL) && !ctx->args->is_gs_copy_shader);

	if (!ctx->args->shader_info->so.num_outputs)
		return;

	vertex_ptr = ngg_nogs_vertex_ptr(ctx, get_thread_id_in_tg(ctx));

	for (unsigned i = 0; i < so->num_outputs; ++i) {
		struct radv_stream_output *output =
			&ctx->args->shader_info->so.outputs[i];

		unsigned loc = output->location;

		for (unsigned comp = 0; comp < 4; comp++) {
			if (!(output->component_mask & (1 << comp)))
				continue;

			tmp = ac_build_gep0(&ctx->ac, vertex_ptr,
					    LLVMConstInt(ctx->ac.i32, 4 * i + comp, false));
			tmp2 = LLVMBuildLoad(builder,
					     ctx->abi.outputs[4 * loc + comp], "");
			tmp2 = ac_to_integer(&ctx->ac, tmp2);
			LLVMBuildStore(builder, tmp2, tmp);
		}
	}
}

static void
handle_ngg_outputs_post_2(struct radv_shader_context *ctx)
{
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef tmp;

	assert((ctx->stage == MESA_SHADER_VERTEX ||
	        ctx->stage == MESA_SHADER_TESS_EVAL) && !ctx->args->is_gs_copy_shader);

	LLVMValueRef prims_in_wave = ac_unpack_param(&ctx->ac,
						     ac_get_arg(&ctx->ac, ctx->args->merged_wave_info), 8, 8);
	LLVMValueRef vtx_in_wave = ac_unpack_param(&ctx->ac,
						   ac_get_arg(&ctx->ac, ctx->args->merged_wave_info), 0, 8);
	LLVMValueRef is_gs_thread = LLVMBuildICmp(builder, LLVMIntULT,
						  ac_get_thread_id(&ctx->ac), prims_in_wave, "");
	LLVMValueRef is_es_thread = LLVMBuildICmp(builder, LLVMIntULT,
						  ac_get_thread_id(&ctx->ac), vtx_in_wave, "");
	LLVMValueRef vtxindex[] = {
		ac_unpack_param(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->gs_vtx_offset[0]), 0, 16),
		ac_unpack_param(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->gs_vtx_offset[0]), 16, 16),
		ac_unpack_param(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->gs_vtx_offset[2]), 0, 16),
	};

	/* Determine the number of vertices per primitive. */
	unsigned num_vertices;
	LLVMValueRef num_vertices_val;

	if (ctx->stage == MESA_SHADER_VERTEX) {
		LLVMValueRef outprim_val =
			LLVMConstInt(ctx->ac.i32,
				     ctx->args->options->key.vs.outprim, false);
		num_vertices_val = LLVMBuildAdd(builder, outprim_val,
						ctx->ac.i32_1, "");
		num_vertices = 3; /* TODO: optimize for points & lines */
	} else {
		assert(ctx->stage == MESA_SHADER_TESS_EVAL);

		if (ctx->shader->info.tess.point_mode)
			num_vertices = 1;
		else if (ctx->shader->info.tess.primitive_mode == GL_ISOLINES)
			num_vertices = 2;
		else
			num_vertices = 3;

		num_vertices_val = LLVMConstInt(ctx->ac.i32, num_vertices, false);
	}

	/* Streamout */
	if (ctx->args->shader_info->so.num_outputs) {
		struct ngg_streamout nggso = {0};

		nggso.num_vertices = num_vertices_val;
		nggso.prim_enable[0] = is_gs_thread;

		for (unsigned i = 0; i < num_vertices; ++i)
			nggso.vertices[i] = ngg_nogs_vertex_ptr(ctx, vtxindex[i]);

		build_streamout(ctx, &nggso);
	}

	/* Copy Primitive IDs from GS threads to the LDS address corresponding
	 * to the ES thread of the provoking vertex.
	 */
	if (ctx->stage == MESA_SHADER_VERTEX &&
	    ctx->args->options->key.vs_common_out.export_prim_id) {
		if (ctx->args->shader_info->so.num_outputs)
			ac_build_s_barrier(&ctx->ac);

		ac_build_ifcc(&ctx->ac, is_gs_thread, 5400);
		/* Extract the PROVOKING_VTX_INDEX field. */
		LLVMValueRef provoking_vtx_in_prim =
			LLVMConstInt(ctx->ac.i32, 0, false);

		/* provoking_vtx_index = vtxindex[provoking_vtx_in_prim]; */
		LLVMValueRef indices = ac_build_gather_values(&ctx->ac, vtxindex, 3);
		LLVMValueRef provoking_vtx_index =
			LLVMBuildExtractElement(builder, indices, provoking_vtx_in_prim, "");

		LLVMBuildStore(builder, ac_get_arg(&ctx->ac, ctx->args->ac.gs_prim_id),
			       ac_build_gep0(&ctx->ac, ctx->esgs_ring, provoking_vtx_index));
		ac_build_endif(&ctx->ac, 5400);
	}

	/* TODO: primitive culling */

	ac_build_sendmsg_gs_alloc_req(&ctx->ac, get_wave_id_in_tg(ctx),
				      ngg_get_vtx_cnt(ctx), ngg_get_prim_cnt(ctx));

	/* TODO: streamout queries */
	/* Export primitive data to the index buffer.
	 *
	 * For the first version, we will always build up all three indices
	 * independent of the primitive type. The additional garbage data
	 * shouldn't hurt.
	 *
	 * TODO: culling depends on the primitive type, so can have some
	 * interaction here.
	 */
	ac_build_ifcc(&ctx->ac, is_gs_thread, 6001);
	{
		struct ac_ngg_prim prim = {0};

		if (ctx->args->options->key.vs_common_out.as_ngg_passthrough) {
			prim.passthrough = ac_get_arg(&ctx->ac, ctx->args->gs_vtx_offset[0]);
		} else {
			prim.num_vertices = num_vertices;
			prim.isnull = ctx->ac.i1false;
			memcpy(prim.index, vtxindex, sizeof(vtxindex[0]) * 3);

			for (unsigned i = 0; i < num_vertices; ++i) {
				tmp = LLVMBuildLShr(builder,
						    ac_get_arg(&ctx->ac, ctx->args->ac.gs_invocation_id),
						    LLVMConstInt(ctx->ac.i32, 8 + i, false), "");
				prim.edgeflag[i] = LLVMBuildTrunc(builder, tmp, ctx->ac.i1, "");
			}
		}

		ac_build_export_prim(&ctx->ac, &prim);
	}
	ac_build_endif(&ctx->ac, 6001);

	/* Export per-vertex data (positions and parameters). */
	ac_build_ifcc(&ctx->ac, is_es_thread, 6002);
	{
		struct radv_vs_output_info *outinfo =
			ctx->stage == MESA_SHADER_TESS_EVAL ?
			&ctx->args->shader_info->tes.outinfo : &ctx->args->shader_info->vs.outinfo;

		/* Exporting the primitive ID is handled below. */
		/* TODO: use the new VS export path */
		handle_vs_outputs_post(ctx, false,
				       ctx->args->options->key.vs_common_out.export_clip_dists,
				       outinfo);

		if (ctx->args->options->key.vs_common_out.export_prim_id) {
			unsigned param_count = outinfo->param_exports;
			LLVMValueRef values[4];

			if (ctx->stage == MESA_SHADER_VERTEX) {
				/* Wait for GS stores to finish. */
				ac_build_s_barrier(&ctx->ac);

				tmp = ac_build_gep0(&ctx->ac, ctx->esgs_ring,
						    get_thread_id_in_tg(ctx));
				values[0] = LLVMBuildLoad(builder, tmp, "");
			} else {
				assert(ctx->stage == MESA_SHADER_TESS_EVAL);
				values[0] = ac_get_arg(&ctx->ac, ctx->args->ac.tes_patch_id);
			}

			values[0] = ac_to_float(&ctx->ac, values[0]);
			for (unsigned j = 1; j < 4; j++)
				values[j] = ctx->ac.f32_0;

			radv_export_param(ctx, param_count, values, 0x1);

			outinfo->vs_output_param_offset[VARYING_SLOT_PRIMITIVE_ID] = param_count++;
			outinfo->param_exports = param_count;
		}
	}
	ac_build_endif(&ctx->ac, 6002);
}

static void gfx10_ngg_gs_emit_prologue(struct radv_shader_context *ctx)
{
	/* Zero out the part of LDS scratch that is used to accumulate the
	 * per-stream generated primitive count.
	 */
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef scratchptr = ctx->gs_ngg_scratch;
	LLVMValueRef tid = get_thread_id_in_tg(ctx);
	LLVMBasicBlockRef merge_block;
	LLVMValueRef cond;

	LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->ac.builder));
	LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(ctx->ac.context, fn, "");
	merge_block = LLVMAppendBasicBlockInContext(ctx->ac.context, fn, "");

	cond = LLVMBuildICmp(builder, LLVMIntULT, tid, LLVMConstInt(ctx->ac.i32, 4, false), "");
	LLVMBuildCondBr(ctx->ac.builder, cond, then_block, merge_block);
	LLVMPositionBuilderAtEnd(ctx->ac.builder, then_block);

	LLVMValueRef ptr = ac_build_gep0(&ctx->ac, scratchptr, tid);
	LLVMBuildStore(builder, ctx->ac.i32_0, ptr);

	LLVMBuildBr(ctx->ac.builder, merge_block);
	LLVMPositionBuilderAtEnd(ctx->ac.builder, merge_block);

	ac_build_s_barrier(&ctx->ac);
}

static void gfx10_ngg_gs_emit_epilogue_1(struct radv_shader_context *ctx)
{
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef i8_0 = LLVMConstInt(ctx->ac.i8, 0, false);
	LLVMValueRef tmp;

	/* Zero out remaining (non-emitted) primitive flags.
	 *
	 * Note: Alternatively, we could pass the relevant gs_next_vertex to
	 *       the emit threads via LDS. This is likely worse in the expected
	 *       typical case where each GS thread emits the full set of
	 *       vertices.
	 */
	for (unsigned stream = 0; stream < 4; ++stream) {
		unsigned num_components;

		num_components =
			ctx->args->shader_info->gs.num_stream_output_components[stream];
		if (!num_components)
			continue;

		const LLVMValueRef gsthread = get_thread_id_in_tg(ctx);

		ac_build_bgnloop(&ctx->ac, 5100);

		const LLVMValueRef vertexidx =
			LLVMBuildLoad(builder, ctx->gs_next_vertex[stream], "");
		tmp = LLVMBuildICmp(builder, LLVMIntUGE, vertexidx,
			LLVMConstInt(ctx->ac.i32, ctx->shader->info.gs.vertices_out, false), "");
		ac_build_ifcc(&ctx->ac, tmp, 5101);
		ac_build_break(&ctx->ac);
		ac_build_endif(&ctx->ac, 5101);

		tmp = LLVMBuildAdd(builder, vertexidx, ctx->ac.i32_1, "");
		LLVMBuildStore(builder, tmp, ctx->gs_next_vertex[stream]);

		tmp = ngg_gs_emit_vertex_ptr(ctx, gsthread, vertexidx);
		LLVMBuildStore(builder, i8_0,
			       ngg_gs_get_emit_primflag_ptr(ctx, tmp, stream));

		ac_build_endloop(&ctx->ac, 5100);
	}

	/* Accumulate generated primitives counts across the entire threadgroup. */
	for (unsigned stream = 0; stream < 4; ++stream) {
		unsigned num_components;

		num_components =
			ctx->args->shader_info->gs.num_stream_output_components[stream];
		if (!num_components)
			continue;

		LLVMValueRef numprims =
			LLVMBuildLoad(builder, ctx->gs_generated_prims[stream], "");
		numprims = ac_build_reduce(&ctx->ac, numprims, nir_op_iadd, ctx->ac.wave_size);

		tmp = LLVMBuildICmp(builder, LLVMIntEQ, ac_get_thread_id(&ctx->ac), ctx->ac.i32_0, "");
		ac_build_ifcc(&ctx->ac, tmp, 5105);
		{
			LLVMBuildAtomicRMW(builder, LLVMAtomicRMWBinOpAdd,
					   ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch,
							 LLVMConstInt(ctx->ac.i32, stream, false)),
					   numprims, LLVMAtomicOrderingMonotonic, false);
		}
		ac_build_endif(&ctx->ac, 5105);
	}
}

static void gfx10_ngg_gs_emit_epilogue_2(struct radv_shader_context *ctx)
{
	const unsigned verts_per_prim = si_conv_gl_prim_to_vertices(ctx->shader->info.gs.output_primitive);
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef tmp, tmp2;

	ac_build_s_barrier(&ctx->ac);

	const LLVMValueRef tid = get_thread_id_in_tg(ctx);
	LLVMValueRef num_emit_threads = ngg_get_prim_cnt(ctx);

	/* Streamout */
	if (ctx->args->shader_info->so.num_outputs) {
		struct ngg_streamout nggso = {0};

		nggso.num_vertices = LLVMConstInt(ctx->ac.i32, verts_per_prim, false);

		LLVMValueRef vertexptr = ngg_gs_vertex_ptr(ctx, tid);
		for (unsigned stream = 0; stream < 4; ++stream) {
			if (!ctx->args->shader_info->gs.num_stream_output_components[stream])
				continue;

			tmp = LLVMBuildLoad(builder,
					    ngg_gs_get_emit_primflag_ptr(ctx, vertexptr, stream), "");
			tmp = LLVMBuildTrunc(builder, tmp, ctx->ac.i1, "");
			tmp2 = LLVMBuildICmp(builder, LLVMIntULT, tid, num_emit_threads, "");
			nggso.prim_enable[stream] = LLVMBuildAnd(builder, tmp, tmp2, "");
		}

		for (unsigned i = 0; i < verts_per_prim; ++i) {
			tmp = LLVMBuildSub(builder, tid,
					   LLVMConstInt(ctx->ac.i32, verts_per_prim - i - 1, false), "");
			tmp = ngg_gs_vertex_ptr(ctx, tmp);
			nggso.vertices[i] = ac_build_gep0(&ctx->ac, tmp, ctx->ac.i32_0);
		}

		build_streamout(ctx, &nggso);
	}

	/* Write shader query data. */
	tmp = ac_get_arg(&ctx->ac, ctx->args->ngg_gs_state);
	tmp = LLVMBuildTrunc(builder, tmp, ctx->ac.i1, "");
	ac_build_ifcc(&ctx->ac, tmp, 5109);
	tmp = LLVMBuildICmp(builder, LLVMIntULT, tid,
			    LLVMConstInt(ctx->ac.i32, 4, false), "");
	ac_build_ifcc(&ctx->ac, tmp, 5110);
	{
		tmp = LLVMBuildLoad(builder, ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch, tid), "");

		ac_llvm_add_target_dep_function_attr(ctx->main_function,
						     "amdgpu-gds-size", 256);

		LLVMTypeRef gdsptr = LLVMPointerType(ctx->ac.i32, AC_ADDR_SPACE_GDS);
		LLVMValueRef gdsbase = LLVMBuildIntToPtr(builder, ctx->ac.i32_0, gdsptr, "");

		const char *sync_scope = LLVM_VERSION_MAJOR >= 9 ? "workgroup-one-as" : "workgroup";

		/* Use a plain GDS atomic to accumulate the number of generated
		 * primitives.
		 */
		ac_build_atomic_rmw(&ctx->ac, LLVMAtomicRMWBinOpAdd, gdsbase,
				    tmp, sync_scope);
	}
	ac_build_endif(&ctx->ac, 5110);
	ac_build_endif(&ctx->ac, 5109);

	/* TODO: culling */

	/* Determine vertex liveness. */
	LLVMValueRef vertliveptr = ac_build_alloca(&ctx->ac, ctx->ac.i1, "vertexlive");

	tmp = LLVMBuildICmp(builder, LLVMIntULT, tid, num_emit_threads, "");
	ac_build_ifcc(&ctx->ac, tmp, 5120);
	{
		for (unsigned i = 0; i < verts_per_prim; ++i) {
			const LLVMValueRef primidx =
				LLVMBuildAdd(builder, tid,
					     LLVMConstInt(ctx->ac.i32, i, false), "");

			if (i > 0) {
				tmp = LLVMBuildICmp(builder, LLVMIntULT, primidx, num_emit_threads, "");
				ac_build_ifcc(&ctx->ac, tmp, 5121 + i);
			}

			/* Load primitive liveness */
			tmp = ngg_gs_vertex_ptr(ctx, primidx);
			tmp = LLVMBuildLoad(builder,
					    ngg_gs_get_emit_primflag_ptr(ctx, tmp, 0), "");
			const LLVMValueRef primlive =
				LLVMBuildTrunc(builder, tmp, ctx->ac.i1, "");

			tmp = LLVMBuildLoad(builder, vertliveptr, "");
			tmp = LLVMBuildOr(builder, tmp, primlive, ""),
			LLVMBuildStore(builder, tmp, vertliveptr);

			if (i > 0)
				ac_build_endif(&ctx->ac, 5121 + i);
		}
	}
	ac_build_endif(&ctx->ac, 5120);

	/* Inclusive scan addition across the current wave. */
	LLVMValueRef vertlive = LLVMBuildLoad(builder, vertliveptr, "");
	struct ac_wg_scan vertlive_scan = {0};
	vertlive_scan.op = nir_op_iadd;
	vertlive_scan.enable_reduce = true;
	vertlive_scan.enable_exclusive = true;
	vertlive_scan.src = vertlive;
	vertlive_scan.scratch = ac_build_gep0(&ctx->ac, ctx->gs_ngg_scratch, ctx->ac.i32_0);
	vertlive_scan.waveidx = get_wave_id_in_tg(ctx);
	vertlive_scan.numwaves = get_tgsize(ctx);
	vertlive_scan.maxwaves = 8;

	ac_build_wg_scan(&ctx->ac, &vertlive_scan);

	/* Skip all exports (including index exports) when possible. At least on
	 * early gfx10 revisions this is also to avoid hangs.
	 */
	LLVMValueRef have_exports =
		LLVMBuildICmp(builder, LLVMIntNE, vertlive_scan.result_reduce, ctx->ac.i32_0, "");
	num_emit_threads =
		LLVMBuildSelect(builder, have_exports, num_emit_threads, ctx->ac.i32_0, "");

	/* Allocate export space. Send this message as early as possible, to
	 * hide the latency of the SQ <-> SPI roundtrip.
	 *
	 * Note: We could consider compacting primitives for export as well.
	 *       PA processes 1 non-null prim / clock, but it fetches 4 DW of
	 *       prim data per clock and skips null primitives at no additional
	 *       cost. So compacting primitives can only be beneficial when
	 *       there are 4 or more contiguous null primitives in the export
	 *       (in the common case of single-dword prim exports).
	 */
	ac_build_sendmsg_gs_alloc_req(&ctx->ac, get_wave_id_in_tg(ctx),
				      vertlive_scan.result_reduce, num_emit_threads);

	/* Setup the reverse vertex compaction permutation. We re-use stream 1
	 * of the primitive liveness flags, relying on the fact that each
	 * threadgroup can have at most 256 threads. */
	ac_build_ifcc(&ctx->ac, vertlive, 5130);
	{
		tmp = ngg_gs_vertex_ptr(ctx, vertlive_scan.result_exclusive);
		tmp2 = LLVMBuildTrunc(builder, tid, ctx->ac.i8, "");
		LLVMBuildStore(builder, tmp2,
			       ngg_gs_get_emit_primflag_ptr(ctx, tmp, 1));
	}
	ac_build_endif(&ctx->ac, 5130);

	ac_build_s_barrier(&ctx->ac);

	/* Export primitive data */
	tmp = LLVMBuildICmp(builder, LLVMIntULT, tid, num_emit_threads, "");
	ac_build_ifcc(&ctx->ac, tmp, 5140);
	{
		LLVMValueRef flags;
		struct ac_ngg_prim prim = {0};
		prim.num_vertices = verts_per_prim;

		tmp = ngg_gs_vertex_ptr(ctx, tid);
		flags = LLVMBuildLoad(builder,
				      ngg_gs_get_emit_primflag_ptr(ctx, tmp, 0), "");
		prim.isnull = LLVMBuildNot(builder, LLVMBuildTrunc(builder, flags, ctx->ac.i1, ""), "");

		for (unsigned i = 0; i < verts_per_prim; ++i) {
			prim.index[i] = LLVMBuildSub(builder, vertlive_scan.result_exclusive,
				LLVMConstInt(ctx->ac.i32, verts_per_prim - i - 1, false), "");
			prim.edgeflag[i] = ctx->ac.i1false;
		}

		/* Geometry shaders output triangle strips, but NGG expects
		 * triangles. We need to change the vertex order for odd
		 * triangles to get correct front/back facing by swapping 2
		 * vertex indices, but we also have to keep the provoking
		 * vertex in the same place.
		 */
		if (verts_per_prim == 3) {
			LLVMValueRef is_odd = LLVMBuildLShr(builder, flags, ctx->ac.i8_1, "");
			is_odd = LLVMBuildTrunc(builder, is_odd, ctx->ac.i1, "");

			struct ac_ngg_prim in = prim;
			prim.index[0] = in.index[0];
			prim.index[1] = LLVMBuildSelect(builder, is_odd,
							in.index[2], in.index[1], "");
			prim.index[2] = LLVMBuildSelect(builder, is_odd,
							in.index[1], in.index[2], "");
		}

		ac_build_export_prim(&ctx->ac, &prim);
	}
	ac_build_endif(&ctx->ac, 5140);

	/* Export position and parameter data */
	tmp = LLVMBuildICmp(builder, LLVMIntULT, tid, vertlive_scan.result_reduce, "");
	ac_build_ifcc(&ctx->ac, tmp, 5145);
	{
		struct radv_vs_output_info *outinfo = &ctx->args->shader_info->vs.outinfo;
		bool export_view_index = ctx->args->options->key.has_multiview_view_index;
		struct radv_shader_output_values *outputs;
		unsigned noutput = 0;

		/* Allocate a temporary array for the output values. */
		unsigned num_outputs = util_bitcount64(ctx->output_mask) + export_view_index;
		outputs = calloc(num_outputs, sizeof(outputs[0]));

		memset(outinfo->vs_output_param_offset, AC_EXP_PARAM_UNDEFINED,
		       sizeof(outinfo->vs_output_param_offset));
		outinfo->pos_exports = 0;

		tmp = ngg_gs_vertex_ptr(ctx, tid);
		tmp = LLVMBuildLoad(builder,
				    ngg_gs_get_emit_primflag_ptr(ctx, tmp, 1), "");
		tmp = LLVMBuildZExt(builder, tmp, ctx->ac.i32, "");
		const LLVMValueRef vertexptr = ngg_gs_vertex_ptr(ctx, tmp);

		unsigned out_idx = 0;
		for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
			unsigned output_usage_mask =
				ctx->args->shader_info->gs.output_usage_mask[i];
			int length = util_last_bit(output_usage_mask);

			if (!(ctx->output_mask & (1ull << i)))
				continue;

			outputs[noutput].slot_name = i;
			outputs[noutput].slot_index = i == VARYING_SLOT_CLIP_DIST1;
			outputs[noutput].usage_mask = output_usage_mask;

			for (unsigned j = 0; j < length; j++, out_idx++) {
				if (!(output_usage_mask & (1 << j)))
					continue;

				tmp = ngg_gs_get_emit_output_ptr(ctx, vertexptr, out_idx);
				tmp = LLVMBuildLoad(builder, tmp, "");

				LLVMTypeRef type = LLVMGetAllocatedType(ctx->abi.outputs[ac_llvm_reg_index_soa(i, j)]);
				if (ac_get_type_size(type) == 2) {
					tmp = ac_to_integer(&ctx->ac, tmp);
					tmp = LLVMBuildTrunc(ctx->ac.builder, tmp, ctx->ac.i16, "");
				}

				outputs[noutput].values[j] = ac_to_float(&ctx->ac, tmp);
			}

			for (unsigned j = length; j < 4; j++)
				outputs[noutput].values[j] = LLVMGetUndef(ctx->ac.f32);

			noutput++;
		}

		/* Export ViewIndex. */
		if (export_view_index) {
			outputs[noutput].slot_name = VARYING_SLOT_LAYER;
			outputs[noutput].slot_index = 0;
			outputs[noutput].usage_mask = 0x1;
			outputs[noutput].values[0] =
				ac_to_float(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->ac.view_index));
			for (unsigned j = 1; j < 4; j++)
				outputs[noutput].values[j] = ctx->ac.f32_0;
			noutput++;
		}

		radv_llvm_export_vs(ctx, outputs, noutput, outinfo,
				    ctx->args->options->key.vs_common_out.export_clip_dists);
		FREE(outputs);
	}
	ac_build_endif(&ctx->ac, 5145);
}

static void gfx10_ngg_gs_emit_vertex(struct radv_shader_context *ctx,
				     unsigned stream,
				     LLVMValueRef vertexidx,
				     LLVMValueRef *addrs)
{
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef tmp;

	const LLVMValueRef vertexptr =
		ngg_gs_emit_vertex_ptr(ctx, get_thread_id_in_tg(ctx), vertexidx);
	unsigned out_idx = 0;
	for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
		unsigned output_usage_mask =
			ctx->args->shader_info->gs.output_usage_mask[i];
		uint8_t output_stream =
			ctx->args->shader_info->gs.output_streams[i];
		LLVMValueRef *out_ptr = &addrs[i * 4];
		int length = util_last_bit(output_usage_mask);

		if (!(ctx->output_mask & (1ull << i)) ||
		    output_stream != stream)
			continue;

		for (unsigned j = 0; j < length; j++, out_idx++) {
			if (!(output_usage_mask & (1 << j)))
				continue;

			LLVMValueRef out_val = LLVMBuildLoad(ctx->ac.builder,
							     out_ptr[j], "");
			out_val = ac_to_integer(&ctx->ac, out_val);
			out_val = LLVMBuildZExtOrBitCast(ctx->ac.builder, out_val, ctx->ac.i32, "");

			LLVMBuildStore(builder, out_val,
				       ngg_gs_get_emit_output_ptr(ctx, vertexptr, out_idx));
		}
	}
	assert(out_idx * 4 <= ctx->args->shader_info->gs.gsvs_vertex_size);

	/* Store the current number of emitted vertices to zero out remaining
	 * primitive flags in case the geometry shader doesn't emit the maximum
	 * number of vertices.
	 */
	tmp = LLVMBuildAdd(builder, vertexidx, ctx->ac.i32_1, "");
	LLVMBuildStore(builder, tmp, ctx->gs_next_vertex[stream]);

	/* Determine and store whether this vertex completed a primitive. */
	const LLVMValueRef curverts = LLVMBuildLoad(builder, ctx->gs_curprim_verts[stream], "");

	tmp = LLVMConstInt(ctx->ac.i32, si_conv_gl_prim_to_vertices(ctx->shader->info.gs.output_primitive) - 1, false);
	const LLVMValueRef iscompleteprim =
		LLVMBuildICmp(builder, LLVMIntUGE, curverts, tmp, "");

	/* Since the geometry shader emits triangle strips, we need to
	 * track which primitive is odd and swap vertex indices to get
	 * the correct vertex order.
	 */
	LLVMValueRef is_odd = ctx->ac.i1false;
	if (stream == 0 &&
	    si_conv_gl_prim_to_vertices(ctx->shader->info.gs.output_primitive) == 3) {
		tmp = LLVMBuildAnd(builder, curverts, ctx->ac.i32_1, "");
		is_odd = LLVMBuildICmp(builder, LLVMIntEQ, tmp, ctx->ac.i32_1, "");
	}

	tmp = LLVMBuildAdd(builder, curverts, ctx->ac.i32_1, "");
	LLVMBuildStore(builder, tmp, ctx->gs_curprim_verts[stream]);

	/* The per-vertex primitive flag encoding:
	 *   bit 0: whether this vertex finishes a primitive
	 *   bit 1: whether the primitive is odd (if we are emitting triangle strips)
	 */
	tmp = LLVMBuildZExt(builder, iscompleteprim, ctx->ac.i8, "");
	tmp = LLVMBuildOr(builder, tmp,
			  LLVMBuildShl(builder,
				       LLVMBuildZExt(builder, is_odd, ctx->ac.i8, ""),
				       ctx->ac.i8_1, ""), "");
	LLVMBuildStore(builder, tmp,
		       ngg_gs_get_emit_primflag_ptr(ctx, vertexptr, stream));

	tmp = LLVMBuildLoad(builder, ctx->gs_generated_prims[stream], "");
	tmp = LLVMBuildAdd(builder, tmp, LLVMBuildZExt(builder, iscompleteprim, ctx->ac.i32, ""), "");
	LLVMBuildStore(builder, tmp, ctx->gs_generated_prims[stream]);
}

static void
write_tess_factors(struct radv_shader_context *ctx)
{
	unsigned stride, outer_comps, inner_comps;
	LLVMValueRef tcs_rel_ids = ac_get_arg(&ctx->ac, ctx->args->ac.tcs_rel_ids);
	LLVMValueRef invocation_id = ac_unpack_param(&ctx->ac, tcs_rel_ids, 8, 5);
	LLVMValueRef rel_patch_id = ac_unpack_param(&ctx->ac, tcs_rel_ids, 0, 8);
	LLVMValueRef lds_base, lds_inner = NULL, lds_outer, byteoffset, buffer;
	LLVMValueRef out[6], vec0, vec1, tf_base, inner[4], outer[4];
	int i;
	ac_emit_barrier(&ctx->ac, ctx->stage);

	switch (ctx->args->options->key.tcs.primitive_mode) {
	case GL_ISOLINES:
		stride = 2;
		outer_comps = 2;
		inner_comps = 0;
		break;
	case GL_TRIANGLES:
		stride = 4;
		outer_comps = 3;
		inner_comps = 1;
		break;
	case GL_QUADS:
		stride = 6;
		outer_comps = 4;
		inner_comps = 2;
		break;
	default:
		return;
	}

	ac_build_ifcc(&ctx->ac,
			LLVMBuildICmp(ctx->ac.builder, LLVMIntEQ,
				      invocation_id, ctx->ac.i32_0, ""), 6503);

	lds_base = get_tcs_out_current_patch_data_offset(ctx);

	if (inner_comps) {
		lds_inner = LLVMBuildAdd(ctx->ac.builder, lds_base,
					 LLVMConstInt(ctx->ac.i32, ctx->tcs_tess_lvl_inner * 4, false), "");
	}

	lds_outer = LLVMBuildAdd(ctx->ac.builder, lds_base,
				 LLVMConstInt(ctx->ac.i32, ctx->tcs_tess_lvl_outer * 4, false), "");

	for (i = 0; i < 4; i++) {
		inner[i] = LLVMGetUndef(ctx->ac.i32);
		outer[i] = LLVMGetUndef(ctx->ac.i32);
	}

	// LINES reversal
	if (ctx->args->options->key.tcs.primitive_mode == GL_ISOLINES) {
		outer[0] = out[1] = ac_lds_load(&ctx->ac, lds_outer);
		lds_outer = LLVMBuildAdd(ctx->ac.builder, lds_outer,
					 ctx->ac.i32_1, "");
		outer[1] = out[0] = ac_lds_load(&ctx->ac, lds_outer);
	} else {
		for (i = 0; i < outer_comps; i++) {
			outer[i] = out[i] =
				ac_lds_load(&ctx->ac, lds_outer);
			lds_outer = LLVMBuildAdd(ctx->ac.builder, lds_outer,
						 ctx->ac.i32_1, "");
		}
		for (i = 0; i < inner_comps; i++) {
			inner[i] = out[outer_comps+i] =
				ac_lds_load(&ctx->ac, lds_inner);
			lds_inner = LLVMBuildAdd(ctx->ac.builder, lds_inner,
						 ctx->ac.i32_1, "");
		}
	}

	/* Convert the outputs to vectors for stores. */
	vec0 = ac_build_gather_values(&ctx->ac, out, MIN2(stride, 4));
	vec1 = NULL;

	if (stride > 4)
		vec1 = ac_build_gather_values(&ctx->ac, out + 4, stride - 4);


	buffer = ctx->hs_ring_tess_factor;
	tf_base = ac_get_arg(&ctx->ac, ctx->args->tess_factor_offset);
	byteoffset = LLVMBuildMul(ctx->ac.builder, rel_patch_id,
				  LLVMConstInt(ctx->ac.i32, 4 * stride, false), "");
	unsigned tf_offset = 0;

	if (ctx->ac.chip_class <= GFX8) {
		ac_build_ifcc(&ctx->ac,
		                LLVMBuildICmp(ctx->ac.builder, LLVMIntEQ,
		                              rel_patch_id, ctx->ac.i32_0, ""), 6504);

		/* Store the dynamic HS control word. */
		ac_build_buffer_store_dword(&ctx->ac, buffer,
					    LLVMConstInt(ctx->ac.i32, 0x80000000, false),
					    1, ctx->ac.i32_0, tf_base,
					    0, ac_glc);
		tf_offset += 4;

		ac_build_endif(&ctx->ac, 6504);
	}

	/* Store the tessellation factors. */
	ac_build_buffer_store_dword(&ctx->ac, buffer, vec0,
				    MIN2(stride, 4), byteoffset, tf_base,
				    tf_offset, ac_glc);
	if (vec1)
		ac_build_buffer_store_dword(&ctx->ac, buffer, vec1,
					    stride - 4, byteoffset, tf_base,
					    16 + tf_offset, ac_glc);

	//store to offchip for TES to read - only if TES reads them
	if (ctx->args->options->key.tcs.tes_reads_tess_factors) {
		LLVMValueRef inner_vec, outer_vec, tf_outer_offset;
		LLVMValueRef tf_inner_offset;

		tf_outer_offset = get_tcs_tes_buffer_address(ctx, NULL,
							     LLVMConstInt(ctx->ac.i32, ctx->tcs_tess_lvl_outer, 0));

		outer_vec = ac_build_gather_values(&ctx->ac, outer,
						   util_next_power_of_two(outer_comps));

		ac_build_buffer_store_dword(&ctx->ac, ctx->hs_ring_tess_offchip, outer_vec,
					    outer_comps, tf_outer_offset,
					    ac_get_arg(&ctx->ac, ctx->args->oc_lds),
					    0, ac_glc);
		if (inner_comps) {
			tf_inner_offset = get_tcs_tes_buffer_address(ctx, NULL,
								     LLVMConstInt(ctx->ac.i32, ctx->tcs_tess_lvl_inner, 0));

			inner_vec = inner_comps == 1 ? inner[0] :
				ac_build_gather_values(&ctx->ac, inner, inner_comps);
			ac_build_buffer_store_dword(&ctx->ac, ctx->hs_ring_tess_offchip, inner_vec,
						    inner_comps, tf_inner_offset,
						    ac_get_arg(&ctx->ac, ctx->args->oc_lds),
						    0, ac_glc);
		}
	}

	ac_build_endif(&ctx->ac, 6503);
}

static void
handle_tcs_outputs_post(struct radv_shader_context *ctx)
{
	write_tess_factors(ctx);
}

static bool
si_export_mrt_color(struct radv_shader_context *ctx,
		    LLVMValueRef *color, unsigned index,
		    struct ac_export_args *args)
{
	/* Export */
	si_llvm_init_export_args(ctx, color, 0xf,
				 V_008DFC_SQ_EXP_MRT + index, args);
	if (!args->enabled_channels)
		return false; /* unnecessary NULL export */

	return true;
}

static void
radv_export_mrt_z(struct radv_shader_context *ctx,
		  LLVMValueRef depth, LLVMValueRef stencil,
		  LLVMValueRef samplemask)
{
	struct ac_export_args args;

	ac_export_mrt_z(&ctx->ac, depth, stencil, samplemask, &args);

	ac_build_export(&ctx->ac, &args);
}

static void
handle_fs_outputs_post(struct radv_shader_context *ctx)
{
	unsigned index = 0;
	LLVMValueRef depth = NULL, stencil = NULL, samplemask = NULL;
	struct ac_export_args color_args[8];

	for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
		LLVMValueRef values[4];

		if (!(ctx->output_mask & (1ull << i)))
			continue;

		if (i < FRAG_RESULT_DATA0)
			continue;

		for (unsigned j = 0; j < 4; j++)
			values[j] = ac_to_float(&ctx->ac,
						radv_load_output(ctx, i, j));

		bool ret = si_export_mrt_color(ctx, values,
					       i - FRAG_RESULT_DATA0,
					       &color_args[index]);
		if (ret)
			index++;
	}

	/* Process depth, stencil, samplemask. */
	if (ctx->args->shader_info->ps.writes_z) {
		depth = ac_to_float(&ctx->ac,
				    radv_load_output(ctx, FRAG_RESULT_DEPTH, 0));
	}
	if (ctx->args->shader_info->ps.writes_stencil) {
		stencil = ac_to_float(&ctx->ac,
				      radv_load_output(ctx, FRAG_RESULT_STENCIL, 0));
	}
	if (ctx->args->shader_info->ps.writes_sample_mask) {
		samplemask = ac_to_float(&ctx->ac,
					 radv_load_output(ctx, FRAG_RESULT_SAMPLE_MASK, 0));
	}

	/* Set the DONE bit on last non-null color export only if Z isn't
	 * exported.
	 */
	if (index > 0 &&
	    !ctx->args->shader_info->ps.writes_z &&
	    !ctx->args->shader_info->ps.writes_stencil &&
	    !ctx->args->shader_info->ps.writes_sample_mask) {
		unsigned last = index - 1;

               color_args[last].valid_mask = 1; /* whether the EXEC mask is valid */
               color_args[last].done = 1; /* DONE bit */
	}

	/* Export PS outputs. */
	for (unsigned i = 0; i < index; i++)
		ac_build_export(&ctx->ac, &color_args[i]);

	if (depth || stencil || samplemask)
		radv_export_mrt_z(ctx, depth, stencil, samplemask);
	else if (!index)
		ac_build_export_null(&ctx->ac);
}

static void
emit_gs_epilogue(struct radv_shader_context *ctx)
{
	if (ctx->args->options->key.vs_common_out.as_ngg) {
		gfx10_ngg_gs_emit_epilogue_1(ctx);
		return;
	}

	if (ctx->ac.chip_class >= GFX10)
		LLVMBuildFence(ctx->ac.builder, LLVMAtomicOrderingRelease, false, "");

	ac_build_sendmsg(&ctx->ac, AC_SENDMSG_GS_OP_NOP | AC_SENDMSG_GS_DONE, ctx->gs_wave_id);
}

static void
handle_shader_outputs_post(struct ac_shader_abi *abi, unsigned max_outputs,
			   LLVMValueRef *addrs)
{
	struct radv_shader_context *ctx = radv_shader_context_from_abi(abi);

	switch (ctx->stage) {
	case MESA_SHADER_VERTEX:
		if (ctx->args->options->key.vs_common_out.as_ls)
			handle_ls_outputs_post(ctx);
		else if (ctx->args->options->key.vs_common_out.as_es)
			handle_es_outputs_post(ctx, &ctx->args->shader_info->vs.es_info);
		else if (ctx->args->options->key.vs_common_out.as_ngg)
			handle_ngg_outputs_post_1(ctx);
		else
			handle_vs_outputs_post(ctx, ctx->args->options->key.vs_common_out.export_prim_id,
					       ctx->args->options->key.vs_common_out.export_clip_dists,
					       &ctx->args->shader_info->vs.outinfo);
		break;
	case MESA_SHADER_FRAGMENT:
		handle_fs_outputs_post(ctx);
		break;
	case MESA_SHADER_GEOMETRY:
		emit_gs_epilogue(ctx);
		break;
	case MESA_SHADER_TESS_CTRL:
		handle_tcs_outputs_post(ctx);
		break;
	case MESA_SHADER_TESS_EVAL:
		if (ctx->args->options->key.vs_common_out.as_es)
			handle_es_outputs_post(ctx, &ctx->args->shader_info->tes.es_info);
		else if (ctx->args->options->key.vs_common_out.as_ngg)
			handle_ngg_outputs_post_1(ctx);
		else
			handle_vs_outputs_post(ctx, ctx->args->options->key.vs_common_out.export_prim_id,
					       ctx->args->options->key.vs_common_out.export_clip_dists,
					       &ctx->args->shader_info->tes.outinfo);
		break;
	default:
		break;
	}
}

static void ac_llvm_finalize_module(struct radv_shader_context *ctx,
				    LLVMPassManagerRef passmgr,
				    const struct radv_nir_compiler_options *options)
{
	LLVMRunPassManager(passmgr, ctx->ac.module);
	LLVMDisposeBuilder(ctx->ac.builder);

	ac_llvm_context_dispose(&ctx->ac);
}

static void
ac_nir_eliminate_const_vs_outputs(struct radv_shader_context *ctx)
{
	struct radv_vs_output_info *outinfo;

	switch (ctx->stage) {
	case MESA_SHADER_FRAGMENT:
	case MESA_SHADER_COMPUTE:
	case MESA_SHADER_TESS_CTRL:
	case MESA_SHADER_GEOMETRY:
		return;
	case MESA_SHADER_VERTEX:
		if (ctx->args->options->key.vs_common_out.as_ls ||
		    ctx->args->options->key.vs_common_out.as_es)
			return;
		outinfo = &ctx->args->shader_info->vs.outinfo;
		break;
	case MESA_SHADER_TESS_EVAL:
		if (ctx->args->options->key.vs_common_out.as_es)
			return;
		outinfo = &ctx->args->shader_info->tes.outinfo;
		break;
	default:
		unreachable("Unhandled shader type");
	}

	ac_optimize_vs_outputs(&ctx->ac,
			       ctx->main_function,
			       outinfo->vs_output_param_offset,
			       VARYING_SLOT_MAX, 0,
			       &outinfo->param_exports);
}

static void
ac_setup_rings(struct radv_shader_context *ctx)
{
	if (ctx->args->options->chip_class <= GFX8 &&
	    (ctx->stage == MESA_SHADER_GEOMETRY ||
	     ctx->args->options->key.vs_common_out.as_es)) {
		unsigned ring = ctx->stage == MESA_SHADER_GEOMETRY ? RING_ESGS_GS
								   : RING_ESGS_VS;
		LLVMValueRef offset = LLVMConstInt(ctx->ac.i32, ring, false);

		ctx->esgs_ring = ac_build_load_to_sgpr(&ctx->ac,
						       ctx->ring_offsets,
						       offset);
	}

	if (ctx->args->is_gs_copy_shader) {
		ctx->gsvs_ring[0] =
			ac_build_load_to_sgpr(&ctx->ac, ctx->ring_offsets,
					      LLVMConstInt(ctx->ac.i32,
							   RING_GSVS_VS, false));
	}

	if (ctx->stage == MESA_SHADER_GEOMETRY) {
		/* The conceptual layout of the GSVS ring is
		 *   v0c0 .. vLv0 v0c1 .. vLc1 ..
		 * but the real memory layout is swizzled across
		 * threads:
		 *   t0v0c0 .. t15v0c0 t0v1c0 .. t15v1c0 ... t15vLcL
		 *   t16v0c0 ..
		 * Override the buffer descriptor accordingly.
		 */
		LLVMTypeRef v2i64 = LLVMVectorType(ctx->ac.i64, 2);
		uint64_t stream_offset = 0;
		unsigned num_records = ctx->ac.wave_size;
		LLVMValueRef base_ring;

		base_ring =
			ac_build_load_to_sgpr(&ctx->ac, ctx->ring_offsets,
					      LLVMConstInt(ctx->ac.i32,
							   RING_GSVS_GS, false));

		for (unsigned stream = 0; stream < 4; stream++) {
			unsigned num_components, stride;
			LLVMValueRef ring, tmp;

			num_components =
				ctx->args->shader_info->gs.num_stream_output_components[stream];

			if (!num_components)
				continue;

			stride = 4 * num_components * ctx->shader->info.gs.vertices_out;

			/* Limit on the stride field for <= GFX7. */
			assert(stride < (1 << 14));

			ring = LLVMBuildBitCast(ctx->ac.builder,
						base_ring, v2i64, "");
			tmp = LLVMBuildExtractElement(ctx->ac.builder,
						      ring, ctx->ac.i32_0, "");
			tmp = LLVMBuildAdd(ctx->ac.builder, tmp,
					   LLVMConstInt(ctx->ac.i64,
							stream_offset, 0), "");
			ring = LLVMBuildInsertElement(ctx->ac.builder,
						      ring, tmp, ctx->ac.i32_0, "");

			stream_offset += stride * ctx->ac.wave_size;

			ring = LLVMBuildBitCast(ctx->ac.builder, ring,
						ctx->ac.v4i32, "");

			tmp = LLVMBuildExtractElement(ctx->ac.builder, ring,
						      ctx->ac.i32_1, "");
			tmp = LLVMBuildOr(ctx->ac.builder, tmp,
					  LLVMConstInt(ctx->ac.i32,
						       S_008F04_STRIDE(stride), false), "");
			ring = LLVMBuildInsertElement(ctx->ac.builder, ring, tmp,
						      ctx->ac.i32_1, "");

			ring = LLVMBuildInsertElement(ctx->ac.builder, ring,
						      LLVMConstInt(ctx->ac.i32,
								   num_records, false),
						      LLVMConstInt(ctx->ac.i32, 2, false), "");

			ctx->gsvs_ring[stream] = ring;
		}
	}

	if (ctx->stage == MESA_SHADER_TESS_CTRL ||
	    ctx->stage == MESA_SHADER_TESS_EVAL) {
		ctx->hs_ring_tess_offchip = ac_build_load_to_sgpr(&ctx->ac, ctx->ring_offsets, LLVMConstInt(ctx->ac.i32, RING_HS_TESS_OFFCHIP, false));
		ctx->hs_ring_tess_factor = ac_build_load_to_sgpr(&ctx->ac, ctx->ring_offsets, LLVMConstInt(ctx->ac.i32, RING_HS_TESS_FACTOR, false));
	}
}

unsigned
radv_nir_get_max_workgroup_size(enum chip_class chip_class,
				gl_shader_stage stage,
				const struct nir_shader *nir)
{
	const unsigned backup_sizes[] = {chip_class >= GFX9 ? 128 : 64, 1, 1};
	unsigned sizes[3];
	for (unsigned i = 0; i < 3; i++)
		sizes[i] = nir ? nir->info.cs.local_size[i] : backup_sizes[i];
	return radv_get_max_workgroup_size(chip_class, stage, sizes);
}

/* Fixup the HW not emitting the TCS regs if there are no HS threads. */
static void ac_nir_fixup_ls_hs_input_vgprs(struct radv_shader_context *ctx)
{
	LLVMValueRef count =
		ac_unpack_param(&ctx->ac, ac_get_arg(&ctx->ac, ctx->args->merged_wave_info), 8, 8);
	LLVMValueRef hs_empty = LLVMBuildICmp(ctx->ac.builder, LLVMIntEQ, count,
	                                      ctx->ac.i32_0, "");
	ctx->abi.instance_id = LLVMBuildSelect(ctx->ac.builder, hs_empty,
					       ac_get_arg(&ctx->ac, ctx->args->rel_auto_id),
					       ctx->abi.instance_id, "");
	ctx->rel_auto_id = LLVMBuildSelect(ctx->ac.builder, hs_empty,
					   ac_get_arg(&ctx->ac, ctx->args->ac.tcs_rel_ids),
					   ctx->rel_auto_id,
					   "");
	ctx->abi.vertex_id = LLVMBuildSelect(ctx->ac.builder, hs_empty,
						 ac_get_arg(&ctx->ac, ctx->args->ac.tcs_patch_id),
						 ctx->abi.vertex_id, "");
}

static void prepare_gs_input_vgprs(struct radv_shader_context *ctx, bool merged)
{
	if (merged) {
		for(int i = 5; i >= 0; --i) {
			ctx->gs_vtx_offset[i] =
				ac_unpack_param(&ctx->ac,
						ac_get_arg(&ctx->ac, ctx->args->gs_vtx_offset[i & ~1]),
							   (i & 1) * 16, 16);
		}

		ctx->gs_wave_id = ac_unpack_param(&ctx->ac,
						  ac_get_arg(&ctx->ac, ctx->args->merged_wave_info),
						  16, 8);
	} else {
		for (int i = 0; i < 6; i++)
			ctx->gs_vtx_offset[i] = ac_get_arg(&ctx->ac, ctx->args->gs_vtx_offset[i]);
		ctx->gs_wave_id = ac_get_arg(&ctx->ac, ctx->args->gs_wave_id);
	}
}

/* Ensure that the esgs ring is declared.
 *
 * We declare it with 64KB alignment as a hint that the
 * pointer value will always be 0.
 */
static void declare_esgs_ring(struct radv_shader_context *ctx)
{
	if (ctx->esgs_ring)
		return;

	assert(!LLVMGetNamedGlobal(ctx->ac.module, "esgs_ring"));

	ctx->esgs_ring = LLVMAddGlobalInAddressSpace(
		ctx->ac.module, LLVMArrayType(ctx->ac.i32, 0),
		"esgs_ring",
		AC_ADDR_SPACE_LDS);
	LLVMSetLinkage(ctx->esgs_ring, LLVMExternalLinkage);
	LLVMSetAlignment(ctx->esgs_ring, 64 * 1024);
}

static
LLVMModuleRef ac_translate_nir_to_llvm(struct ac_llvm_compiler *ac_llvm,
                                       struct nir_shader *const *shaders,
                                       int shader_count,
                                       const struct radv_shader_args *args)
{
	struct radv_shader_context ctx = {0};
	ctx.args = args;

	enum ac_float_mode float_mode = AC_FLOAT_MODE_DEFAULT;

	if (args->shader_info->float_controls_mode & FLOAT_CONTROLS_DENORM_FLUSH_TO_ZERO_FP32) {
		float_mode = AC_FLOAT_MODE_DENORM_FLUSH_TO_ZERO;
	}

	ac_llvm_context_init(&ctx.ac, ac_llvm, args->options->chip_class,
			     args->options->family, float_mode,
			     args->shader_info->wave_size,
			     args->shader_info->ballot_bit_size);
	ctx.context = ctx.ac.context;

	ctx.max_workgroup_size = 0;
	for (int i = 0; i < shader_count; ++i) {
		ctx.max_workgroup_size = MAX2(ctx.max_workgroup_size,
		                              radv_nir_get_max_workgroup_size(args->options->chip_class,
									      shaders[i]->info.stage,
									      shaders[i]));
	}

	if (ctx.ac.chip_class >= GFX10) {
		if (is_pre_gs_stage(shaders[0]->info.stage) &&
		    args->options->key.vs_common_out.as_ngg) {
			ctx.max_workgroup_size = 128;
		}
	}

	create_function(&ctx, shaders[shader_count - 1]->info.stage, shader_count >= 2);

	ctx.abi.inputs = &ctx.inputs[0];
	ctx.abi.emit_outputs = handle_shader_outputs_post;
	ctx.abi.emit_vertex_with_counter = visit_emit_vertex_with_counter;
	ctx.abi.load_ubo = radv_load_ubo;
	ctx.abi.load_ssbo = radv_load_ssbo;
	ctx.abi.load_sampler_desc = radv_get_sampler_desc;
	ctx.abi.load_resource = radv_load_resource;
	ctx.abi.clamp_shadow_reference = false;
	ctx.abi.robust_buffer_access = args->options->robust_buffer_access;

	bool is_ngg = is_pre_gs_stage(shaders[0]->info.stage) &&  args->options->key.vs_common_out.as_ngg;
	if (shader_count >= 2 || is_ngg)
		ac_init_exec_full_mask(&ctx.ac);

	if (args->ac.vertex_id.used)
		ctx.abi.vertex_id = ac_get_arg(&ctx.ac, args->ac.vertex_id);
	if (args->rel_auto_id.used)
		ctx.rel_auto_id = ac_get_arg(&ctx.ac, args->rel_auto_id);
	if (args->ac.instance_id.used)
		ctx.abi.instance_id = ac_get_arg(&ctx.ac, args->ac.instance_id);

	if (args->options->has_ls_vgpr_init_bug &&
	    shaders[shader_count - 1]->info.stage == MESA_SHADER_TESS_CTRL)
		ac_nir_fixup_ls_hs_input_vgprs(&ctx);

	if (is_ngg) {
		/* Declare scratch space base for streamout and vertex
		 * compaction. Whether space is actually allocated is
		 * determined during linking / PM4 creation.
		 *
		 * Add an extra dword per vertex to ensure an odd stride, which
		 * avoids bank conflicts for SoA accesses.
		 */
		if (!args->options->key.vs_common_out.as_ngg_passthrough)
			declare_esgs_ring(&ctx);

		/* This is really only needed when streamout and / or vertex
		 * compaction is enabled.
		 */
		if (args->shader_info->so.num_outputs) {
			LLVMTypeRef asi32 = LLVMArrayType(ctx.ac.i32, 8);
			ctx.gs_ngg_scratch = LLVMAddGlobalInAddressSpace(ctx.ac.module,
				asi32, "ngg_scratch", AC_ADDR_SPACE_LDS);
			LLVMSetInitializer(ctx.gs_ngg_scratch, LLVMGetUndef(asi32));
			LLVMSetAlignment(ctx.gs_ngg_scratch, 4);
		}
	}

	for(int i = 0; i < shader_count; ++i) {
		ctx.stage = shaders[i]->info.stage;
		ctx.shader = shaders[i];
		ctx.output_mask = 0;

		if (shaders[i]->info.stage == MESA_SHADER_GEOMETRY) {
			for (int i = 0; i < 4; i++) {
				ctx.gs_next_vertex[i] =
					ac_build_alloca(&ctx.ac, ctx.ac.i32, "");
			}
			if (args->options->key.vs_common_out.as_ngg) {
				for (unsigned i = 0; i < 4; ++i) {
					ctx.gs_curprim_verts[i] =
						ac_build_alloca(&ctx.ac, ctx.ac.i32, "");
					ctx.gs_generated_prims[i] =
						ac_build_alloca(&ctx.ac, ctx.ac.i32, "");
				}

				unsigned scratch_size = 8;
				if (args->shader_info->so.num_outputs)
					scratch_size = 44;

				LLVMTypeRef ai32 = LLVMArrayType(ctx.ac.i32, scratch_size);
				ctx.gs_ngg_scratch =
					LLVMAddGlobalInAddressSpace(ctx.ac.module,
								    ai32, "ngg_scratch", AC_ADDR_SPACE_LDS);
				LLVMSetInitializer(ctx.gs_ngg_scratch, LLVMGetUndef(ai32));
				LLVMSetAlignment(ctx.gs_ngg_scratch, 4);

				ctx.gs_ngg_emit = LLVMAddGlobalInAddressSpace(ctx.ac.module,
					LLVMArrayType(ctx.ac.i32, 0), "ngg_emit", AC_ADDR_SPACE_LDS);
				LLVMSetLinkage(ctx.gs_ngg_emit, LLVMExternalLinkage);
				LLVMSetAlignment(ctx.gs_ngg_emit, 4);
			}

			ctx.abi.load_inputs = load_gs_input;
			ctx.abi.emit_primitive = visit_end_primitive;
		} else if (shaders[i]->info.stage == MESA_SHADER_TESS_CTRL) {
			ctx.abi.load_tess_varyings = load_tcs_varyings;
			ctx.abi.load_patch_vertices_in = load_patch_vertices_in;
			ctx.abi.store_tcs_outputs = store_tcs_output;
			ctx.tcs_num_inputs = ctx.args->shader_info->tcs.num_linked_inputs;
			unsigned tcs_num_outputs = ctx.args->shader_info->tcs.num_linked_outputs;
			unsigned tcs_num_patch_outputs = ctx.args->shader_info->tcs.num_linked_patch_outputs;
			ctx.tcs_num_patches =
				get_tcs_num_patches(
					ctx.args->options->key.tcs.input_vertices,
					ctx.shader->info.tess.tcs_vertices_out,
					ctx.tcs_num_inputs,
					tcs_num_outputs,
					tcs_num_patch_outputs,
					ctx.args->options->tess_offchip_block_dw_size,
					ctx.args->options->chip_class,
					ctx.args->options->family);
		} else if (shaders[i]->info.stage == MESA_SHADER_TESS_EVAL) {
			ctx.abi.load_tess_varyings = load_tes_input;
			ctx.abi.load_tess_coord = load_tess_coord;
			ctx.abi.load_patch_vertices_in = load_patch_vertices_in;
			ctx.tcs_num_patches = args->options->key.tes.num_patches;
		} else if (shaders[i]->info.stage == MESA_SHADER_VERTEX) {
			ctx.abi.load_base_vertex = radv_load_base_vertex;
		} else if (shaders[i]->info.stage == MESA_SHADER_FRAGMENT) {
			ctx.abi.load_sample_position = load_sample_position;
			ctx.abi.load_sample_mask_in = load_sample_mask_in;
		}

		if (shaders[i]->info.stage == MESA_SHADER_VERTEX &&
		    args->options->key.vs_common_out.as_ngg &&
		    args->options->key.vs_common_out.export_prim_id) {
			declare_esgs_ring(&ctx);
		}

		bool nested_barrier = false;

		if (i) {
			if (shaders[i]->info.stage == MESA_SHADER_GEOMETRY &&
			    args->options->key.vs_common_out.as_ngg) {
				gfx10_ngg_gs_emit_prologue(&ctx);
				nested_barrier = false;
			} else {
				nested_barrier = true;
			}
		}

		if (nested_barrier) {
			/* Execute a barrier before the second shader in
			 * a merged shader.
			 *
			 * Execute the barrier inside the conditional block,
			 * so that empty waves can jump directly to s_endpgm,
			 * which will also signal the barrier.
			 *
			 * This is possible in gfx9, because an empty wave
			 * for the second shader does not participate in
			 * the epilogue. With NGG, empty waves may still
			 * be required to export data (e.g. GS output vertices),
			 * so we cannot let them exit early.
			 *
			 * If the shader is TCS and the TCS epilog is present
			 * and contains a barrier, it will wait there and then
			 * reach s_endpgm.
			*/
			ac_emit_barrier(&ctx.ac, ctx.stage);
		}

		nir_foreach_shader_out_variable(variable, shaders[i])
			scan_shader_output_decl(&ctx, variable, shaders[i], shaders[i]->info.stage);

		ac_setup_rings(&ctx);

		LLVMBasicBlockRef merge_block = NULL;
		if (shader_count >= 2 || is_ngg) {
			LLVMValueRef fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx.ac.builder));
			LLVMBasicBlockRef then_block = LLVMAppendBasicBlockInContext(ctx.ac.context, fn, "");
			merge_block = LLVMAppendBasicBlockInContext(ctx.ac.context, fn, "");

			LLVMValueRef count =
				ac_unpack_param(&ctx.ac,
						ac_get_arg(&ctx.ac, args->merged_wave_info),
						8 * i, 8);
			LLVMValueRef thread_id = ac_get_thread_id(&ctx.ac);
			LLVMValueRef cond = LLVMBuildICmp(ctx.ac.builder, LLVMIntULT,
			                                  thread_id, count, "");
			LLVMBuildCondBr(ctx.ac.builder, cond, then_block, merge_block);

			LLVMPositionBuilderAtEnd(ctx.ac.builder, then_block);
		}

		if (shaders[i]->info.stage == MESA_SHADER_FRAGMENT)
			prepare_interp_optimize(&ctx, shaders[i]);
		else if(shaders[i]->info.stage == MESA_SHADER_VERTEX)
			handle_vs_inputs(&ctx, shaders[i]);
		else if(shaders[i]->info.stage == MESA_SHADER_GEOMETRY)
			prepare_gs_input_vgprs(&ctx, shader_count >= 2);

		ac_nir_translate(&ctx.ac, &ctx.abi, &args->ac, shaders[i]);

		if (shader_count >= 2 || is_ngg) {
			LLVMBuildBr(ctx.ac.builder, merge_block);
			LLVMPositionBuilderAtEnd(ctx.ac.builder, merge_block);
		}

		/* This needs to be outside the if wrapping the shader body, as sometimes
		 * the HW generates waves with 0 es/vs threads. */
		if (is_pre_gs_stage(shaders[i]->info.stage) &&
		    args->options->key.vs_common_out.as_ngg &&
		    i == shader_count - 1) {
			handle_ngg_outputs_post_2(&ctx);
		} else if (shaders[i]->info.stage == MESA_SHADER_GEOMETRY &&
			   args->options->key.vs_common_out.as_ngg) {
			gfx10_ngg_gs_emit_epilogue_2(&ctx);
		}

		if (shaders[i]->info.stage == MESA_SHADER_TESS_CTRL) {
			unsigned tcs_num_outputs = ctx.args->shader_info->tcs.num_linked_outputs;
			unsigned tcs_num_patch_outputs = ctx.args->shader_info->tcs.num_linked_patch_outputs;
			args->shader_info->tcs.num_patches = ctx.tcs_num_patches;
			args->shader_info->tcs.num_lds_blocks =
				calculate_tess_lds_size(
					ctx.args->options->chip_class,
					ctx.args->options->key.tcs.input_vertices,
					ctx.shader->info.tess.tcs_vertices_out,
					ctx.tcs_num_inputs,
					ctx.tcs_num_patches,
					tcs_num_outputs,
					tcs_num_patch_outputs);
		}
	}

	LLVMBuildRetVoid(ctx.ac.builder);

	if (args->options->dump_preoptir) {
		fprintf(stderr, "%s LLVM IR:\n\n",
			radv_get_shader_name(args->shader_info,
					     shaders[shader_count - 1]->info.stage));
		ac_dump_module(ctx.ac.module);
		fprintf(stderr, "\n");
	}

	ac_llvm_finalize_module(&ctx, ac_llvm->passmgr, args->options);

	if (shader_count == 1)
		ac_nir_eliminate_const_vs_outputs(&ctx);

	if (args->options->dump_shader) {
		args->shader_info->private_mem_vgprs =
			ac_count_scratch_private_memory(ctx.main_function);
	}

	return ctx.ac.module;
}

static void ac_diagnostic_handler(LLVMDiagnosticInfoRef di, void *context)
{
	unsigned *retval = (unsigned *)context;
	LLVMDiagnosticSeverity severity = LLVMGetDiagInfoSeverity(di);
	char *description = LLVMGetDiagInfoDescription(di);

	if (severity == LLVMDSError) {
		*retval = 1;
		fprintf(stderr, "LLVM triggered Diagnostic Handler: %s\n",
		        description);
	}

	LLVMDisposeMessage(description);
}

static unsigned radv_llvm_compile(LLVMModuleRef M,
                                  char **pelf_buffer, size_t *pelf_size,
                                  struct ac_llvm_compiler *ac_llvm)
{
	unsigned retval = 0;
	LLVMContextRef llvm_ctx;

	/* Setup Diagnostic Handler*/
	llvm_ctx = LLVMGetModuleContext(M);

	LLVMContextSetDiagnosticHandler(llvm_ctx, ac_diagnostic_handler,
	                                &retval);

	/* Compile IR*/
	if (!radv_compile_to_elf(ac_llvm, M, pelf_buffer, pelf_size))
		retval = 1;
	return retval;
}

static void ac_compile_llvm_module(struct ac_llvm_compiler *ac_llvm,
				   LLVMModuleRef llvm_module,
				   struct radv_shader_binary **rbinary,
				   gl_shader_stage stage,
				   const char *name,
				   const struct radv_nir_compiler_options *options)
{
	char *elf_buffer = NULL;
	size_t elf_size = 0;
	char *llvm_ir_string = NULL;

	if (options->dump_shader) {
		fprintf(stderr, "%s LLVM IR:\n\n", name);
		ac_dump_module(llvm_module);
		fprintf(stderr, "\n");
	}

	if (options->record_ir) {
		char *llvm_ir = LLVMPrintModuleToString(llvm_module);
		llvm_ir_string = strdup(llvm_ir);
		LLVMDisposeMessage(llvm_ir);
	}

	int v = radv_llvm_compile(llvm_module, &elf_buffer, &elf_size, ac_llvm);
	if (v) {
		fprintf(stderr, "compile failed\n");
	}

	LLVMContextRef ctx = LLVMGetModuleContext(llvm_module);
	LLVMDisposeModule(llvm_module);
	LLVMContextDispose(ctx);

	size_t llvm_ir_size = llvm_ir_string ? strlen(llvm_ir_string) : 0;
	size_t alloc_size = sizeof(struct radv_shader_binary_rtld) + elf_size + llvm_ir_size + 1;
	struct radv_shader_binary_rtld *rbin = calloc(1, alloc_size);
	memcpy(rbin->data,  elf_buffer, elf_size);
	if (llvm_ir_string)
		memcpy(rbin->data + elf_size, llvm_ir_string, llvm_ir_size + 1);

	rbin->base.type = RADV_BINARY_TYPE_RTLD;
	rbin->base.stage = stage;
	rbin->base.total_size = alloc_size;
	rbin->elf_size = elf_size;
	rbin->llvm_ir_size = llvm_ir_size;
	*rbinary = &rbin->base;

	free(llvm_ir_string);
	free(elf_buffer);
}

static void
radv_compile_nir_shader(struct ac_llvm_compiler *ac_llvm,
			struct radv_shader_binary **rbinary,
			const struct radv_shader_args *args,
			struct nir_shader *const *nir,
			int nir_count)
{

	LLVMModuleRef llvm_module;

	llvm_module = ac_translate_nir_to_llvm(ac_llvm, nir, nir_count, args);

	ac_compile_llvm_module(ac_llvm, llvm_module, rbinary,
			       nir[nir_count - 1]->info.stage,
			       radv_get_shader_name(args->shader_info,
						    nir[nir_count - 1]->info.stage),
			       args->options);

	/* Determine the ES type (VS or TES) for the GS on GFX9. */
	if (args->options->chip_class >= GFX9) {
		if (nir_count == 2 &&
		    nir[1]->info.stage == MESA_SHADER_GEOMETRY) {
			args->shader_info->gs.es_type = nir[0]->info.stage;
		}
	}
}

static void
ac_gs_copy_shader_emit(struct radv_shader_context *ctx)
{
	LLVMValueRef vtx_offset =
		LLVMBuildMul(ctx->ac.builder, ac_get_arg(&ctx->ac, ctx->args->ac.vertex_id),
			     LLVMConstInt(ctx->ac.i32, 4, false), "");
	LLVMValueRef stream_id;

	/* Fetch the vertex stream ID. */
	if (!ctx->args->options->use_ngg_streamout &&
	    ctx->args->shader_info->so.num_outputs) {
		stream_id =
			ac_unpack_param(&ctx->ac,
					ac_get_arg(&ctx->ac,
						   ctx->args->streamout_config),
					24, 2);
	} else {
		stream_id = ctx->ac.i32_0;
	}

	LLVMBasicBlockRef end_bb;
	LLVMValueRef switch_inst;

	end_bb = LLVMAppendBasicBlockInContext(ctx->ac.context,
					       ctx->main_function, "end");
	switch_inst = LLVMBuildSwitch(ctx->ac.builder, stream_id, end_bb, 4);

	for (unsigned stream = 0; stream < 4; stream++) {
		unsigned num_components =
			ctx->args->shader_info->gs.num_stream_output_components[stream];
		LLVMBasicBlockRef bb;
		unsigned offset;

		if (stream > 0 && !num_components)
			continue;

		if (stream > 0 && !ctx->args->shader_info->so.num_outputs)
			continue;

		bb = LLVMInsertBasicBlockInContext(ctx->ac.context, end_bb, "out");
		LLVMAddCase(switch_inst, LLVMConstInt(ctx->ac.i32, stream, 0), bb);
		LLVMPositionBuilderAtEnd(ctx->ac.builder, bb);

		offset = 0;
		for (unsigned i = 0; i < AC_LLVM_MAX_OUTPUTS; ++i) {
			unsigned output_usage_mask =
				ctx->args->shader_info->gs.output_usage_mask[i];
			unsigned output_stream =
				ctx->args->shader_info->gs.output_streams[i];
			int length = util_last_bit(output_usage_mask);

			if (!(ctx->output_mask & (1ull << i)) ||
			    output_stream != stream)
				continue;

			for (unsigned j = 0; j < length; j++) {
				LLVMValueRef value, soffset;

				if (!(output_usage_mask & (1 << j)))
					continue;

				soffset = LLVMConstInt(ctx->ac.i32,
						       offset *
						       ctx->shader->info.gs.vertices_out * 16 * 4, false);

				offset++;

				value = ac_build_buffer_load(&ctx->ac,
							     ctx->gsvs_ring[0],
							     1, ctx->ac.i32_0,
							     vtx_offset, soffset,
							     0, ac_glc | ac_slc, true, false);

				LLVMTypeRef type = LLVMGetAllocatedType(ctx->abi.outputs[ac_llvm_reg_index_soa(i, j)]);
				if (ac_get_type_size(type) == 2) {
					value = LLVMBuildBitCast(ctx->ac.builder, value, ctx->ac.i32, "");
					value = LLVMBuildTrunc(ctx->ac.builder, value, ctx->ac.i16, "");
				}

				LLVMBuildStore(ctx->ac.builder,
					       ac_to_float(&ctx->ac, value), ctx->abi.outputs[ac_llvm_reg_index_soa(i, j)]);
			}
		}

		if (!ctx->args->options->use_ngg_streamout &&
		    ctx->args->shader_info->so.num_outputs)
			radv_emit_streamout(ctx, stream);

		if (stream == 0) {
			handle_vs_outputs_post(ctx, false, true,
					       &ctx->args->shader_info->vs.outinfo);
		}

		LLVMBuildBr(ctx->ac.builder, end_bb);
	}

	LLVMPositionBuilderAtEnd(ctx->ac.builder, end_bb);
}

static void
radv_compile_gs_copy_shader(struct ac_llvm_compiler *ac_llvm,
			    struct nir_shader *geom_shader,
			    struct radv_shader_binary **rbinary,
			    const struct radv_shader_args *args)
{
	struct radv_shader_context ctx = {0};
	ctx.args = args;

	assert(args->is_gs_copy_shader);

	ac_llvm_context_init(&ctx.ac, ac_llvm, args->options->chip_class,
			     args->options->family, AC_FLOAT_MODE_DEFAULT, 64, 64);
	ctx.context = ctx.ac.context;

	ctx.stage = MESA_SHADER_VERTEX;
	ctx.shader = geom_shader;

	create_function(&ctx, MESA_SHADER_VERTEX, false);

	ac_setup_rings(&ctx);

	nir_foreach_shader_out_variable(variable, geom_shader) {
		scan_shader_output_decl(&ctx, variable, geom_shader, MESA_SHADER_VERTEX);
		ac_handle_shader_output_decl(&ctx.ac, &ctx.abi, geom_shader,
					     variable, MESA_SHADER_VERTEX);
	}

	ac_gs_copy_shader_emit(&ctx);

	LLVMBuildRetVoid(ctx.ac.builder);

	ac_llvm_finalize_module(&ctx, ac_llvm->passmgr, args->options);

	ac_compile_llvm_module(ac_llvm, ctx.ac.module, rbinary,
			       MESA_SHADER_VERTEX, "GS Copy Shader", args->options);
	(*rbinary)->is_gs_copy_shader = true;

}

void
llvm_compile_shader(struct radv_device *device,
		    unsigned shader_count,
		    struct nir_shader *const *shaders,
		    struct radv_shader_binary **binary,
		    struct radv_shader_args *args)
{
	enum ac_target_machine_options tm_options = 0;
	struct ac_llvm_compiler ac_llvm;
	bool thread_compiler;

	tm_options |= AC_TM_SUPPORTS_SPILL;
	if (args->options->check_ir)
		tm_options |= AC_TM_CHECK_IR;

	thread_compiler = !(device->instance->debug_flags & RADV_DEBUG_NOTHREADLLVM);

	radv_init_llvm_compiler(&ac_llvm, thread_compiler,
				args->options->family, tm_options,
				args->shader_info->wave_size);

	if (args->is_gs_copy_shader) {
		radv_compile_gs_copy_shader(&ac_llvm, *shaders, binary, args);
	} else {
		radv_compile_nir_shader(&ac_llvm, binary, args,
					shaders, shader_count);
	}

	radv_destroy_llvm_compiler(&ac_llvm, thread_compiler);
}
