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

#include "util/disk_cache.h"
#include "util/mesa-sha1.h"
#include "util/u_atomic.h"
#include "radv_debug.h"
#include "radv_private.h"
#include "radv_cs.h"
#include "radv_shader.h"
#include "nir/nir.h"
#include "nir/nir_builder.h"
#include "nir/nir_xfb_info.h"
#include "spirv/nir_spirv.h"
#include "vk_util.h"

#include "sid.h"
#include "ac_binary.h"
#include "ac_llvm_util.h"
#include "ac_nir_to_llvm.h"
#include "vk_format.h"
#include "util/debug.h"
#include "ac_exp_param.h"
#include "ac_shader_util.h"

struct radv_blend_state {
	uint32_t blend_enable_4bit;
	uint32_t need_src_alpha;

	uint32_t cb_color_control;
	uint32_t cb_target_mask;
	uint32_t cb_target_enabled_4bit;
	uint32_t sx_mrt_blend_opt[8];
	uint32_t cb_blend_control[8];

	uint32_t spi_shader_col_format;
	uint32_t col_format_is_int8;
	uint32_t col_format_is_int10;
	uint32_t cb_shader_mask;
	uint32_t db_alpha_to_mask;

	uint32_t commutative_4bit;

	bool single_cb_enable;
	bool mrt0_is_dual_src;
};

struct radv_dsa_order_invariance {
	/* Whether the final result in Z/S buffers is guaranteed to be
	 * invariant under changes to the order in which fragments arrive.
	 */
	bool zs;

	/* Whether the set of fragments that pass the combined Z/S test is
	 * guaranteed to be invariant under changes to the order in which
	 * fragments arrive.
	 */
	bool pass_set;
};

static const VkPipelineMultisampleStateCreateInfo *
radv_pipeline_get_multisample_state(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	if (!pCreateInfo->pRasterizationState->rasterizerDiscardEnable)
		return pCreateInfo->pMultisampleState;
	return NULL;
}

static const VkPipelineTessellationStateCreateInfo *
radv_pipeline_get_tessellation_state(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
		if (pCreateInfo->pStages[i].stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ||
		    pCreateInfo->pStages[i].stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
			return pCreateInfo->pTessellationState;
		}
	}
	return NULL;
}

static const VkPipelineDepthStencilStateCreateInfo *
radv_pipeline_get_depth_stencil_state(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;

	if (!pCreateInfo->pRasterizationState->rasterizerDiscardEnable &&
	    subpass->depth_stencil_attachment)
		return pCreateInfo->pDepthStencilState;
	return NULL;
}

static const VkPipelineColorBlendStateCreateInfo *
radv_pipeline_get_color_blend_state(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;

	if (!pCreateInfo->pRasterizationState->rasterizerDiscardEnable &&
	    subpass->has_color_att)
		return pCreateInfo->pColorBlendState;
	return NULL;
}

bool radv_pipeline_has_ngg(const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *variant = NULL;
	if (pipeline->shaders[MESA_SHADER_GEOMETRY])
		variant = pipeline->shaders[MESA_SHADER_GEOMETRY];
	else if (pipeline->shaders[MESA_SHADER_TESS_EVAL])
		variant = pipeline->shaders[MESA_SHADER_TESS_EVAL];
	else if (pipeline->shaders[MESA_SHADER_VERTEX])
		variant = pipeline->shaders[MESA_SHADER_VERTEX];
	else
		return false;
	return variant->info.is_ngg;
}

bool radv_pipeline_has_ngg_passthrough(const struct radv_pipeline *pipeline)
{
	assert(radv_pipeline_has_ngg(pipeline));

	struct radv_shader_variant *variant = NULL;
	if (pipeline->shaders[MESA_SHADER_GEOMETRY])
		variant = pipeline->shaders[MESA_SHADER_GEOMETRY];
	else if (pipeline->shaders[MESA_SHADER_TESS_EVAL])
		variant = pipeline->shaders[MESA_SHADER_TESS_EVAL];
	else if (pipeline->shaders[MESA_SHADER_VERTEX])
		variant = pipeline->shaders[MESA_SHADER_VERTEX];
	else
		return false;
	return variant->info.is_ngg_passthrough;
}

bool radv_pipeline_has_gs_copy_shader(const struct radv_pipeline *pipeline)
{
	if (!radv_pipeline_has_gs(pipeline))
		return false;

	/* The GS copy shader is required if the pipeline has GS on GFX6-GFX9.
	 * On GFX10, it might be required in rare cases if it's not possible to
	 * enable NGG.
	 */
	if (radv_pipeline_has_ngg(pipeline))
		return false;

	assert(pipeline->gs_copy_shader);
	return true;
}

static void
radv_pipeline_destroy(struct radv_device *device,
                      struct radv_pipeline *pipeline,
                      const VkAllocationCallbacks* allocator)
{
	for (unsigned i = 0; i < MESA_SHADER_STAGES; ++i)
		if (pipeline->shaders[i])
			radv_shader_variant_destroy(device, pipeline->shaders[i]);

	if (pipeline->gs_copy_shader)
		radv_shader_variant_destroy(device, pipeline->gs_copy_shader);

	if(pipeline->cs.buf)
		free(pipeline->cs.buf);

	vk_object_base_finish(&pipeline->base);
	vk_free2(&device->vk.alloc, allocator, pipeline);
}

void radv_DestroyPipeline(
	VkDevice                                    _device,
	VkPipeline                                  _pipeline,
	const VkAllocationCallbacks*                pAllocator)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_pipeline, pipeline, _pipeline);

	if (!_pipeline)
		return;

	radv_pipeline_destroy(device, pipeline, pAllocator);
}

static uint32_t get_hash_flags(const struct radv_device *device)
{
	uint32_t hash_flags = 0;

	if (device->instance->debug_flags & RADV_DEBUG_NO_NGG)
		hash_flags |= RADV_HASH_SHADER_NO_NGG;
	if (device->physical_device->cs_wave_size == 32)
		hash_flags |= RADV_HASH_SHADER_CS_WAVE32;
	if (device->physical_device->ps_wave_size == 32)
		hash_flags |= RADV_HASH_SHADER_PS_WAVE32;
	if (device->physical_device->ge_wave_size == 32)
		hash_flags |= RADV_HASH_SHADER_GE_WAVE32;
	if (device->physical_device->use_llvm)
		hash_flags |= RADV_HASH_SHADER_LLVM;
	if (device->instance->debug_flags & RADV_DEBUG_DISCARD_TO_DEMOTE)
		hash_flags |= RADV_HASH_SHADER_DISCARD_TO_DEMOTE;
	if (device->instance->enable_mrt_output_nan_fixup)
		hash_flags |= RADV_HASH_SHADER_MRT_NAN_FIXUP;
	if (device->instance->debug_flags & RADV_DEBUG_INVARIANT_GEOM)
		hash_flags |= RADV_HASH_SHADER_INVARIANT_GEOM;
	return hash_flags;
}

static void
radv_pipeline_init_scratch(const struct radv_device *device,
                           struct radv_pipeline *pipeline)
{
	unsigned scratch_bytes_per_wave = 0;
	unsigned max_waves = 0;
	unsigned min_waves = 1;

	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (pipeline->shaders[i] &&
		    pipeline->shaders[i]->config.scratch_bytes_per_wave) {
			unsigned max_stage_waves = device->scratch_waves;

			scratch_bytes_per_wave = MAX2(scratch_bytes_per_wave,
			                              pipeline->shaders[i]->config.scratch_bytes_per_wave);

			max_stage_waves = MIN2(max_stage_waves,
			          4 * device->physical_device->rad_info.num_good_compute_units *
			          (256 / pipeline->shaders[i]->config.num_vgprs));
			max_waves = MAX2(max_waves, max_stage_waves);
		}
	}

	if (pipeline->shaders[MESA_SHADER_COMPUTE]) {
		unsigned group_size = pipeline->shaders[MESA_SHADER_COMPUTE]->info.cs.block_size[0] *
		                      pipeline->shaders[MESA_SHADER_COMPUTE]->info.cs.block_size[1] *
		                      pipeline->shaders[MESA_SHADER_COMPUTE]->info.cs.block_size[2];
		min_waves = MAX2(min_waves, round_up_u32(group_size, 64));
	}

	pipeline->scratch_bytes_per_wave = scratch_bytes_per_wave;
	pipeline->max_waves = max_waves;
}

static uint32_t si_translate_blend_logic_op(VkLogicOp op)
{
	switch (op) {
	case VK_LOGIC_OP_CLEAR:
		return V_028808_ROP3_CLEAR;
	case VK_LOGIC_OP_AND:
		return V_028808_ROP3_AND;
	case VK_LOGIC_OP_AND_REVERSE:
		return V_028808_ROP3_AND_REVERSE;
	case VK_LOGIC_OP_COPY:
		return V_028808_ROP3_COPY;
	case VK_LOGIC_OP_AND_INVERTED:
		return V_028808_ROP3_AND_INVERTED;
	case VK_LOGIC_OP_NO_OP:
		return V_028808_ROP3_NO_OP;
	case VK_LOGIC_OP_XOR:
		return V_028808_ROP3_XOR;
	case VK_LOGIC_OP_OR:
		return V_028808_ROP3_OR;
	case VK_LOGIC_OP_NOR:
		return V_028808_ROP3_NOR;
	case VK_LOGIC_OP_EQUIVALENT:
		return V_028808_ROP3_EQUIVALENT;
	case VK_LOGIC_OP_INVERT:
		return V_028808_ROP3_INVERT;
	case VK_LOGIC_OP_OR_REVERSE:
		return V_028808_ROP3_OR_REVERSE;
	case VK_LOGIC_OP_COPY_INVERTED:
		return V_028808_ROP3_COPY_INVERTED;
	case VK_LOGIC_OP_OR_INVERTED:
		return V_028808_ROP3_OR_INVERTED;
	case VK_LOGIC_OP_NAND:
		return V_028808_ROP3_NAND;
	case VK_LOGIC_OP_SET:
		return V_028808_ROP3_SET;
	default:
		unreachable("Unhandled logic op");
	}
}


static uint32_t si_translate_blend_function(VkBlendOp op)
{
	switch (op) {
	case VK_BLEND_OP_ADD:
		return V_028780_COMB_DST_PLUS_SRC;
	case VK_BLEND_OP_SUBTRACT:
		return V_028780_COMB_SRC_MINUS_DST;
	case VK_BLEND_OP_REVERSE_SUBTRACT:
		return V_028780_COMB_DST_MINUS_SRC;
	case VK_BLEND_OP_MIN:
		return V_028780_COMB_MIN_DST_SRC;
	case VK_BLEND_OP_MAX:
		return V_028780_COMB_MAX_DST_SRC;
	default:
		return 0;
	}
}

static uint32_t si_translate_blend_factor(VkBlendFactor factor)
{
	switch (factor) {
	case VK_BLEND_FACTOR_ZERO:
		return V_028780_BLEND_ZERO;
	case VK_BLEND_FACTOR_ONE:
		return V_028780_BLEND_ONE;
	case VK_BLEND_FACTOR_SRC_COLOR:
		return V_028780_BLEND_SRC_COLOR;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
		return V_028780_BLEND_ONE_MINUS_SRC_COLOR;
	case VK_BLEND_FACTOR_DST_COLOR:
		return V_028780_BLEND_DST_COLOR;
	case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
		return V_028780_BLEND_ONE_MINUS_DST_COLOR;
	case VK_BLEND_FACTOR_SRC_ALPHA:
		return V_028780_BLEND_SRC_ALPHA;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		return V_028780_BLEND_ONE_MINUS_SRC_ALPHA;
	case VK_BLEND_FACTOR_DST_ALPHA:
		return V_028780_BLEND_DST_ALPHA;
	case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		return V_028780_BLEND_ONE_MINUS_DST_ALPHA;
	case VK_BLEND_FACTOR_CONSTANT_COLOR:
		return V_028780_BLEND_CONSTANT_COLOR;
	case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
		return V_028780_BLEND_ONE_MINUS_CONSTANT_COLOR;
	case VK_BLEND_FACTOR_CONSTANT_ALPHA:
		return V_028780_BLEND_CONSTANT_ALPHA;
	case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
		return V_028780_BLEND_ONE_MINUS_CONSTANT_ALPHA;
	case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
		return V_028780_BLEND_SRC_ALPHA_SATURATE;
	case VK_BLEND_FACTOR_SRC1_COLOR:
		return V_028780_BLEND_SRC1_COLOR;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
		return V_028780_BLEND_INV_SRC1_COLOR;
	case VK_BLEND_FACTOR_SRC1_ALPHA:
		return V_028780_BLEND_SRC1_ALPHA;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
		return V_028780_BLEND_INV_SRC1_ALPHA;
	default:
		return 0;
	}
}

static uint32_t si_translate_blend_opt_function(VkBlendOp op)
{
	switch (op) {
	case VK_BLEND_OP_ADD:
		return V_028760_OPT_COMB_ADD;
	case VK_BLEND_OP_SUBTRACT:
		return V_028760_OPT_COMB_SUBTRACT;
	case VK_BLEND_OP_REVERSE_SUBTRACT:
		return V_028760_OPT_COMB_REVSUBTRACT;
	case VK_BLEND_OP_MIN:
		return V_028760_OPT_COMB_MIN;
	case VK_BLEND_OP_MAX:
		return V_028760_OPT_COMB_MAX;
	default:
		return V_028760_OPT_COMB_BLEND_DISABLED;
	}
}

static uint32_t si_translate_blend_opt_factor(VkBlendFactor factor, bool is_alpha)
{
	switch (factor) {
	case VK_BLEND_FACTOR_ZERO:
		return V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_ALL;
	case VK_BLEND_FACTOR_ONE:
		return V_028760_BLEND_OPT_PRESERVE_ALL_IGNORE_NONE;
	case VK_BLEND_FACTOR_SRC_COLOR:
		return is_alpha ? V_028760_BLEND_OPT_PRESERVE_A1_IGNORE_A0
				: V_028760_BLEND_OPT_PRESERVE_C1_IGNORE_C0;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
		return is_alpha ? V_028760_BLEND_OPT_PRESERVE_A0_IGNORE_A1
				: V_028760_BLEND_OPT_PRESERVE_C0_IGNORE_C1;
	case VK_BLEND_FACTOR_SRC_ALPHA:
		return V_028760_BLEND_OPT_PRESERVE_A1_IGNORE_A0;
	case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		return V_028760_BLEND_OPT_PRESERVE_A0_IGNORE_A1;
	case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
		return is_alpha ? V_028760_BLEND_OPT_PRESERVE_ALL_IGNORE_NONE
				: V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_A0;
	default:
		return V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
	}
}

/**
 * Get rid of DST in the blend factors by commuting the operands:
 *    func(src * DST, dst * 0) ---> func(src * 0, dst * SRC)
 */
static void si_blend_remove_dst(unsigned *func, unsigned *src_factor,
				unsigned *dst_factor, unsigned expected_dst,
				unsigned replacement_src)
{
	if (*src_factor == expected_dst &&
	    *dst_factor == VK_BLEND_FACTOR_ZERO) {
		*src_factor = VK_BLEND_FACTOR_ZERO;
		*dst_factor = replacement_src;

		/* Commuting the operands requires reversing subtractions. */
		if (*func == VK_BLEND_OP_SUBTRACT)
			*func = VK_BLEND_OP_REVERSE_SUBTRACT;
		else if (*func == VK_BLEND_OP_REVERSE_SUBTRACT)
			*func = VK_BLEND_OP_SUBTRACT;
	}
}

static bool si_blend_factor_uses_dst(unsigned factor)
{
	return factor == VK_BLEND_FACTOR_DST_COLOR ||
		factor == VK_BLEND_FACTOR_DST_ALPHA ||
		factor == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE ||
		factor == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA ||
		factor == VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
}

static bool is_dual_src(VkBlendFactor factor)
{
	switch (factor) {
	case VK_BLEND_FACTOR_SRC1_COLOR:
	case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
	case VK_BLEND_FACTOR_SRC1_ALPHA:
	case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
		return true;
	default:
		return false;
	}
}

static unsigned radv_choose_spi_color_format(VkFormat vk_format,
					     bool blend_enable,
					     bool blend_need_alpha)
{
	const struct vk_format_description *desc = vk_format_description(vk_format);
	struct ac_spi_color_formats formats = {0};
	unsigned format, ntype, swap;

	format = radv_translate_colorformat(vk_format);
	ntype = radv_translate_color_numformat(vk_format, desc,
					       vk_format_get_first_non_void_channel(vk_format));
	swap = radv_translate_colorswap(vk_format, false);

	ac_choose_spi_color_formats(format, swap, ntype, false, &formats);

	if (blend_enable && blend_need_alpha)
		return formats.blend_alpha;
	else if(blend_need_alpha)
		return formats.alpha;
	else if(blend_enable)
		return formats.blend;
	else
		return formats.normal;
}

static bool
format_is_int8(VkFormat format)
{
	const struct vk_format_description *desc = vk_format_description(format);
	int channel =  vk_format_get_first_non_void_channel(format);

	return channel >= 0 && desc->channel[channel].pure_integer &&
	       desc->channel[channel].size == 8;
}

static bool
format_is_int10(VkFormat format)
{
	const struct vk_format_description *desc = vk_format_description(format);

	if (desc->nr_channels != 4)
		return false;
	for (unsigned i = 0; i < 4; i++) {
		if (desc->channel[i].pure_integer && desc->channel[i].size == 10)
			return true;
	}
	return false;
}

static void
radv_pipeline_compute_spi_color_formats(const struct radv_pipeline *pipeline,
					const VkGraphicsPipelineCreateInfo *pCreateInfo,
					struct radv_blend_state *blend)
{
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	unsigned col_format = 0, is_int8 = 0, is_int10 = 0;
	unsigned num_targets;

	for (unsigned i = 0; i < (blend->single_cb_enable ? 1 : subpass->color_count); ++i) {
		unsigned cf;

		if (subpass->color_attachments[i].attachment == VK_ATTACHMENT_UNUSED ||
		    !(blend->cb_target_mask & (0xfu << (i * 4)))) {
			cf = V_028714_SPI_SHADER_ZERO;
		} else {
			struct radv_render_pass_attachment *attachment = pass->attachments + subpass->color_attachments[i].attachment;
			bool blend_enable =
				blend->blend_enable_4bit & (0xfu << (i * 4));

			cf = radv_choose_spi_color_format(attachment->format,
			                                  blend_enable,
							  blend->need_src_alpha & (1 << i));

			if (format_is_int8(attachment->format))
				is_int8 |= 1 << i;
			if (format_is_int10(attachment->format))
				is_int10 |= 1 << i;
		}

		col_format |= cf << (4 * i);
	}

	if (!(col_format & 0xf) && blend->need_src_alpha & (1 << 0)) {
		/* When a subpass doesn't have any color attachments, write the
		 * alpha channel of MRT0 when alpha coverage is enabled because
		 * the depth attachment needs it.
		 */
		col_format |= V_028714_SPI_SHADER_32_AR;
	}

	/* If the i-th target format is set, all previous target formats must
	 * be non-zero to avoid hangs.
	 */
	num_targets = (util_last_bit(col_format) + 3) / 4;
	for (unsigned i = 0; i < num_targets; i++) {
		if (!(col_format & (0xfu << (i * 4)))) {
			col_format |= V_028714_SPI_SHADER_32_R << (i * 4);
		}
	}

	/* The output for dual source blending should have the same format as
	 * the first output.
	 */
	if (blend->mrt0_is_dual_src) {
		assert(!(col_format >> 4));
		col_format |= (col_format & 0xf) << 4;
	}

	blend->spi_shader_col_format = col_format;
	blend->col_format_is_int8 = is_int8;
	blend->col_format_is_int10 = is_int10;
}

/*
 * Ordered so that for each i,
 * radv_format_meta_fs_key(radv_fs_key_format_exemplars[i]) == i.
 */
const VkFormat radv_fs_key_format_exemplars[NUM_META_FS_KEYS] = {
	VK_FORMAT_R32_SFLOAT,
	VK_FORMAT_R32G32_SFLOAT,
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R16G16B16A16_UNORM,
	VK_FORMAT_R16G16B16A16_SNORM,
	VK_FORMAT_R16G16B16A16_UINT,
	VK_FORMAT_R16G16B16A16_SINT,
	VK_FORMAT_R32G32B32A32_SFLOAT,
	VK_FORMAT_R8G8B8A8_UINT,
	VK_FORMAT_R8G8B8A8_SINT,
	VK_FORMAT_A2R10G10B10_UINT_PACK32,
	VK_FORMAT_A2R10G10B10_SINT_PACK32,
};

unsigned radv_format_meta_fs_key(VkFormat format)
{
	unsigned col_format = radv_choose_spi_color_format(format, false, false);

	assert(col_format != V_028714_SPI_SHADER_32_AR);
	if (col_format >= V_028714_SPI_SHADER_32_AR)
		--col_format; /* Skip V_028714_SPI_SHADER_32_AR  since there is no such VkFormat */

	--col_format; /* Skip V_028714_SPI_SHADER_ZERO */
	bool is_int8 = format_is_int8(format);
	bool is_int10 = format_is_int10(format);

	return col_format + (is_int8 ? 3 : is_int10 ? 5 : 0);
}

static void
radv_blend_check_commutativity(struct radv_blend_state *blend,
			       VkBlendOp op, VkBlendFactor src,
			       VkBlendFactor dst, unsigned chanmask)
{
	/* Src factor is allowed when it does not depend on Dst. */
	static const uint32_t src_allowed =
		(1u << VK_BLEND_FACTOR_ONE) |
		(1u << VK_BLEND_FACTOR_SRC_COLOR) |
		(1u << VK_BLEND_FACTOR_SRC_ALPHA) |
		(1u << VK_BLEND_FACTOR_SRC_ALPHA_SATURATE) |
		(1u << VK_BLEND_FACTOR_CONSTANT_COLOR) |
		(1u << VK_BLEND_FACTOR_CONSTANT_ALPHA) |
		(1u << VK_BLEND_FACTOR_SRC1_COLOR) |
		(1u << VK_BLEND_FACTOR_SRC1_ALPHA) |
		(1u << VK_BLEND_FACTOR_ZERO) |
		(1u << VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR) |
		(1u << VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) |
		(1u << VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR) |
		(1u << VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA) |
		(1u << VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR) |
		(1u << VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);

	if (dst == VK_BLEND_FACTOR_ONE &&
	    (src_allowed & (1u << src))) {
		/* Addition is commutative, but floating point addition isn't
		 * associative: subtle changes can be introduced via different
		 * rounding. Be conservative, only enable for min and max.
		 */
		if (op == VK_BLEND_OP_MAX || op == VK_BLEND_OP_MIN)
			blend->commutative_4bit |= chanmask;
	}
}

static struct radv_blend_state
radv_pipeline_init_blend_state(const struct radv_pipeline *pipeline,
			       const VkGraphicsPipelineCreateInfo *pCreateInfo,
			       const struct radv_graphics_pipeline_create_info *extra)
{
	const VkPipelineColorBlendStateCreateInfo *vkblend = radv_pipeline_get_color_blend_state(pCreateInfo);
	const VkPipelineMultisampleStateCreateInfo *vkms = radv_pipeline_get_multisample_state(pCreateInfo);
	struct radv_blend_state blend = {0};
	unsigned mode = V_028808_CB_NORMAL;
	int i;

	if (extra && extra->custom_blend_mode) {
		blend.single_cb_enable = true;
		mode = extra->custom_blend_mode;
	}

	blend.cb_color_control = 0;
	if (vkblend) {
		if (vkblend->logicOpEnable)
			blend.cb_color_control |= S_028808_ROP3(si_translate_blend_logic_op(vkblend->logicOp));
		else
			blend.cb_color_control |= S_028808_ROP3(V_028808_ROP3_COPY);
	}

	blend.db_alpha_to_mask = S_028B70_ALPHA_TO_MASK_OFFSET0(3) |
		S_028B70_ALPHA_TO_MASK_OFFSET1(1) |
		S_028B70_ALPHA_TO_MASK_OFFSET2(0) |
		S_028B70_ALPHA_TO_MASK_OFFSET3(2) |
		S_028B70_OFFSET_ROUND(1);

	if (vkms && vkms->alphaToCoverageEnable) {
		blend.db_alpha_to_mask |= S_028B70_ALPHA_TO_MASK_ENABLE(1);
		blend.need_src_alpha |= 0x1;
	}

	blend.cb_target_mask = 0;
	if (vkblend) {
		for (i = 0; i < vkblend->attachmentCount; i++) {
			const VkPipelineColorBlendAttachmentState *att = &vkblend->pAttachments[i];
			unsigned blend_cntl = 0;
			unsigned srcRGB_opt, dstRGB_opt, srcA_opt, dstA_opt;
			VkBlendOp eqRGB = att->colorBlendOp;
			VkBlendFactor srcRGB = att->srcColorBlendFactor;
			VkBlendFactor dstRGB = att->dstColorBlendFactor;
			VkBlendOp eqA = att->alphaBlendOp;
			VkBlendFactor srcA = att->srcAlphaBlendFactor;
			VkBlendFactor dstA = att->dstAlphaBlendFactor;

			blend.sx_mrt_blend_opt[i] = S_028760_COLOR_COMB_FCN(V_028760_OPT_COMB_BLEND_DISABLED) | S_028760_ALPHA_COMB_FCN(V_028760_OPT_COMB_BLEND_DISABLED);

			if (!att->colorWriteMask)
				continue;

			/* Ignore other blend targets if dual-source blending
			 * is enabled to prevent wrong behaviour.
			 */
			if (blend.mrt0_is_dual_src)
				continue;

			blend.cb_target_mask |= (unsigned)att->colorWriteMask << (4 * i);
			blend.cb_target_enabled_4bit |= 0xfu << (4 * i);
			if (!att->blendEnable) {
				blend.cb_blend_control[i] = blend_cntl;
				continue;
			}

			if (is_dual_src(srcRGB) || is_dual_src(dstRGB) || is_dual_src(srcA) || is_dual_src(dstA))
				if (i == 0)
					blend.mrt0_is_dual_src = true;

			if (eqRGB == VK_BLEND_OP_MIN || eqRGB == VK_BLEND_OP_MAX) {
				srcRGB = VK_BLEND_FACTOR_ONE;
				dstRGB = VK_BLEND_FACTOR_ONE;
			}
			if (eqA == VK_BLEND_OP_MIN || eqA == VK_BLEND_OP_MAX) {
				srcA = VK_BLEND_FACTOR_ONE;
				dstA = VK_BLEND_FACTOR_ONE;
			}

			radv_blend_check_commutativity(&blend, eqRGB, srcRGB, dstRGB,
						       0x7u << (4 * i));
			radv_blend_check_commutativity(&blend, eqA, srcA, dstA,
						       0x8u << (4 * i));

			/* Blending optimizations for RB+.
			 * These transformations don't change the behavior.
			 *
			 * First, get rid of DST in the blend factors:
			 *    func(src * DST, dst * 0) ---> func(src * 0, dst * SRC)
			 */
			si_blend_remove_dst(&eqRGB, &srcRGB, &dstRGB,
					    VK_BLEND_FACTOR_DST_COLOR,
					    VK_BLEND_FACTOR_SRC_COLOR);

			si_blend_remove_dst(&eqA, &srcA, &dstA,
					    VK_BLEND_FACTOR_DST_COLOR,
					    VK_BLEND_FACTOR_SRC_COLOR);

			si_blend_remove_dst(&eqA, &srcA, &dstA,
					    VK_BLEND_FACTOR_DST_ALPHA,
					    VK_BLEND_FACTOR_SRC_ALPHA);

			/* Look up the ideal settings from tables. */
			srcRGB_opt = si_translate_blend_opt_factor(srcRGB, false);
			dstRGB_opt = si_translate_blend_opt_factor(dstRGB, false);
			srcA_opt = si_translate_blend_opt_factor(srcA, true);
			dstA_opt = si_translate_blend_opt_factor(dstA, true);

			/* Handle interdependencies. */
			if (si_blend_factor_uses_dst(srcRGB))
				dstRGB_opt = V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;
			if (si_blend_factor_uses_dst(srcA))
				dstA_opt = V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_NONE;

			if (srcRGB == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE &&
			    (dstRGB == VK_BLEND_FACTOR_ZERO ||
			     dstRGB == VK_BLEND_FACTOR_SRC_ALPHA ||
			     dstRGB == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE))
				dstRGB_opt = V_028760_BLEND_OPT_PRESERVE_NONE_IGNORE_A0;

			/* Set the final value. */
			blend.sx_mrt_blend_opt[i] =
				S_028760_COLOR_SRC_OPT(srcRGB_opt) |
				S_028760_COLOR_DST_OPT(dstRGB_opt) |
				S_028760_COLOR_COMB_FCN(si_translate_blend_opt_function(eqRGB)) |
				S_028760_ALPHA_SRC_OPT(srcA_opt) |
				S_028760_ALPHA_DST_OPT(dstA_opt) |
				S_028760_ALPHA_COMB_FCN(si_translate_blend_opt_function(eqA));
			blend_cntl |= S_028780_ENABLE(1);

			blend_cntl |= S_028780_COLOR_COMB_FCN(si_translate_blend_function(eqRGB));
			blend_cntl |= S_028780_COLOR_SRCBLEND(si_translate_blend_factor(srcRGB));
			blend_cntl |= S_028780_COLOR_DESTBLEND(si_translate_blend_factor(dstRGB));
			if (srcA != srcRGB || dstA != dstRGB || eqA != eqRGB) {
				blend_cntl |= S_028780_SEPARATE_ALPHA_BLEND(1);
				blend_cntl |= S_028780_ALPHA_COMB_FCN(si_translate_blend_function(eqA));
				blend_cntl |= S_028780_ALPHA_SRCBLEND(si_translate_blend_factor(srcA));
				blend_cntl |= S_028780_ALPHA_DESTBLEND(si_translate_blend_factor(dstA));
			}
			blend.cb_blend_control[i] = blend_cntl;

			blend.blend_enable_4bit |= 0xfu << (i * 4);

			if (srcRGB == VK_BLEND_FACTOR_SRC_ALPHA ||
			    dstRGB == VK_BLEND_FACTOR_SRC_ALPHA ||
			    srcRGB == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE ||
			    dstRGB == VK_BLEND_FACTOR_SRC_ALPHA_SATURATE ||
			    srcRGB == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA ||
			    dstRGB == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
				blend.need_src_alpha |= 1 << i;
		}
		for (i = vkblend->attachmentCount; i < 8; i++) {
			blend.cb_blend_control[i] = 0;
			blend.sx_mrt_blend_opt[i] = S_028760_COLOR_COMB_FCN(V_028760_OPT_COMB_BLEND_DISABLED) | S_028760_ALPHA_COMB_FCN(V_028760_OPT_COMB_BLEND_DISABLED);
		}
	}

	if (pipeline->device->physical_device->rad_info.has_rbplus) {
		/* Disable RB+ blend optimizations for dual source blending. */
		if (blend.mrt0_is_dual_src) {
			for (i = 0; i < 8; i++) {
				blend.sx_mrt_blend_opt[i] =
					S_028760_COLOR_COMB_FCN(V_028760_OPT_COMB_NONE) |
					S_028760_ALPHA_COMB_FCN(V_028760_OPT_COMB_NONE);
			}
		}

		/* RB+ doesn't work with dual source blending, logic op and
		 * RESOLVE.
		 */
		if (blend.mrt0_is_dual_src ||
		    (vkblend && vkblend->logicOpEnable) ||
		    mode == V_028808_CB_RESOLVE)
			blend.cb_color_control |= S_028808_DISABLE_DUAL_QUAD(1);
	}

	if (blend.cb_target_mask)
		blend.cb_color_control |= S_028808_MODE(mode);
	else
		blend.cb_color_control |= S_028808_MODE(V_028808_CB_DISABLE);

	radv_pipeline_compute_spi_color_formats(pipeline, pCreateInfo, &blend);
	return blend;
}

static uint32_t si_translate_fill(VkPolygonMode func)
{
	switch(func) {
	case VK_POLYGON_MODE_FILL:
		return V_028814_X_DRAW_TRIANGLES;
	case VK_POLYGON_MODE_LINE:
		return V_028814_X_DRAW_LINES;
	case VK_POLYGON_MODE_POINT:
		return V_028814_X_DRAW_POINTS;
	default:
		assert(0);
		return V_028814_X_DRAW_POINTS;
	}
}

static uint8_t radv_pipeline_get_ps_iter_samples(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const VkPipelineMultisampleStateCreateInfo *vkms = pCreateInfo->pMultisampleState;
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = &pass->subpasses[pCreateInfo->subpass];
	uint32_t ps_iter_samples = 1;
	uint32_t num_samples;

	/* From the Vulkan 1.1.129 spec, 26.7. Sample Shading:
	 *
	 * "If the VK_AMD_mixed_attachment_samples extension is enabled and the
	 *  subpass uses color attachments, totalSamples is the number of
	 *  samples of the color attachments. Otherwise, totalSamples is the
	 *  value of VkPipelineMultisampleStateCreateInfo::rasterizationSamples
	 *  specified at pipeline creation time."
	 */
	if (subpass->has_color_att) {
		num_samples = subpass->color_sample_count;
	} else {
		num_samples = vkms->rasterizationSamples;
	}

	if (vkms->sampleShadingEnable) {
		ps_iter_samples = ceilf(vkms->minSampleShading * num_samples);
		ps_iter_samples = util_next_power_of_two(ps_iter_samples);
	}
	return ps_iter_samples;
}

static bool
radv_is_depth_write_enabled(const VkPipelineDepthStencilStateCreateInfo *pCreateInfo)
{
	return pCreateInfo->depthTestEnable &&
	       pCreateInfo->depthWriteEnable &&
	       pCreateInfo->depthCompareOp != VK_COMPARE_OP_NEVER;
}

static bool
radv_writes_stencil(const VkStencilOpState *state)
{
	return state->writeMask &&
	       (state->failOp != VK_STENCIL_OP_KEEP ||
		state->passOp != VK_STENCIL_OP_KEEP ||
		state->depthFailOp != VK_STENCIL_OP_KEEP);
}

static bool
radv_is_stencil_write_enabled(const VkPipelineDepthStencilStateCreateInfo *pCreateInfo)
{
	return pCreateInfo->stencilTestEnable &&
	       (radv_writes_stencil(&pCreateInfo->front) ||
		radv_writes_stencil(&pCreateInfo->back));
}

static bool
radv_is_ds_write_enabled(const VkPipelineDepthStencilStateCreateInfo *pCreateInfo)
{
	return radv_is_depth_write_enabled(pCreateInfo) ||
	       radv_is_stencil_write_enabled(pCreateInfo);
}

static bool
radv_order_invariant_stencil_op(VkStencilOp op)
{
	/* REPLACE is normally order invariant, except when the stencil
	 * reference value is written by the fragment shader. Tracking this
	 * interaction does not seem worth the effort, so be conservative.
	 */
	return op != VK_STENCIL_OP_INCREMENT_AND_CLAMP &&
	       op != VK_STENCIL_OP_DECREMENT_AND_CLAMP &&
	       op != VK_STENCIL_OP_REPLACE;
}

static bool
radv_order_invariant_stencil_state(const VkStencilOpState *state)
{
	/* Compute whether, assuming Z writes are disabled, this stencil state
	 * is order invariant in the sense that the set of passing fragments as
	 * well as the final stencil buffer result does not depend on the order
	 * of fragments.
	 */
	return !state->writeMask ||
	       /* The following assumes that Z writes are disabled. */
	       (state->compareOp == VK_COMPARE_OP_ALWAYS &&
		radv_order_invariant_stencil_op(state->passOp) &&
		radv_order_invariant_stencil_op(state->depthFailOp)) ||
	       (state->compareOp == VK_COMPARE_OP_NEVER &&
		radv_order_invariant_stencil_op(state->failOp));
}

static bool
radv_is_state_dynamic(const VkGraphicsPipelineCreateInfo *pCreateInfo,
		      VkDynamicState state)
{
	if (pCreateInfo->pDynamicState) {
		uint32_t count = pCreateInfo->pDynamicState->dynamicStateCount;
		for (uint32_t i = 0; i < count; i++) {
			if (pCreateInfo->pDynamicState->pDynamicStates[i] == state)
				return true;
		}
	}

	return false;
}

static bool
radv_pipeline_has_dynamic_ds_states(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	VkDynamicState ds_states[] = {
		VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
		VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
		VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
		VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
		VK_DYNAMIC_STATE_STENCIL_OP_EXT,
	};

	for (uint32_t i = 0; i < ARRAY_SIZE(ds_states); i++) {
		if (radv_is_state_dynamic(pCreateInfo, ds_states[i]))
			return true;
	}

	return false;
}

static bool
radv_pipeline_out_of_order_rast(struct radv_pipeline *pipeline,
				const struct radv_blend_state *blend,
				const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	const VkPipelineDepthStencilStateCreateInfo *vkds = radv_pipeline_get_depth_stencil_state(pCreateInfo);
	const VkPipelineColorBlendStateCreateInfo *vkblend = radv_pipeline_get_color_blend_state(pCreateInfo);
	unsigned colormask = blend->cb_target_enabled_4bit;

	if (!pipeline->device->physical_device->out_of_order_rast_allowed)
		return false;

	/* Be conservative if a logic operation is enabled with color buffers. */
	if (colormask && vkblend && vkblend->logicOpEnable)
		return false;

	/* Be conservative if an extended dynamic depth/stencil state is
	 * enabled because the driver can't update out-of-order rasterization
	 * dynamically.
	 */
	if (radv_pipeline_has_dynamic_ds_states(pCreateInfo))
		return false;

	/* Default depth/stencil invariance when no attachment is bound. */
	struct radv_dsa_order_invariance dsa_order_invariant = {
		.zs = true, .pass_set = true
	};

	if (vkds) {
		struct radv_render_pass_attachment *attachment =
			pass->attachments + subpass->depth_stencil_attachment->attachment;
		bool has_stencil = vk_format_is_stencil(attachment->format);
		struct radv_dsa_order_invariance order_invariance[2];
		struct radv_shader_variant *ps =
			pipeline->shaders[MESA_SHADER_FRAGMENT];

		/* Compute depth/stencil order invariance in order to know if
		 * it's safe to enable out-of-order.
		 */
		bool zfunc_is_ordered =
			vkds->depthCompareOp == VK_COMPARE_OP_NEVER ||
			vkds->depthCompareOp == VK_COMPARE_OP_LESS ||
			vkds->depthCompareOp == VK_COMPARE_OP_LESS_OR_EQUAL ||
			vkds->depthCompareOp == VK_COMPARE_OP_GREATER ||
			vkds->depthCompareOp == VK_COMPARE_OP_GREATER_OR_EQUAL;

		bool nozwrite_and_order_invariant_stencil =
			!radv_is_ds_write_enabled(vkds) ||
			(!radv_is_depth_write_enabled(vkds) &&
			 radv_order_invariant_stencil_state(&vkds->front) &&
			 radv_order_invariant_stencil_state(&vkds->back));

		order_invariance[1].zs =
			nozwrite_and_order_invariant_stencil ||
			(!radv_is_stencil_write_enabled(vkds) &&
			 zfunc_is_ordered);
		order_invariance[0].zs =
			!radv_is_depth_write_enabled(vkds) || zfunc_is_ordered;

		order_invariance[1].pass_set =
			nozwrite_and_order_invariant_stencil ||
			(!radv_is_stencil_write_enabled(vkds) &&
			 (vkds->depthCompareOp == VK_COMPARE_OP_ALWAYS ||
			  vkds->depthCompareOp == VK_COMPARE_OP_NEVER));
		order_invariance[0].pass_set =
			!radv_is_depth_write_enabled(vkds) ||
			(vkds->depthCompareOp == VK_COMPARE_OP_ALWAYS ||
			 vkds->depthCompareOp == VK_COMPARE_OP_NEVER);

		dsa_order_invariant = order_invariance[has_stencil];
		if (!dsa_order_invariant.zs)
			return false;

		/* The set of PS invocations is always order invariant,
		 * except when early Z/S tests are requested.
		 */
		if (ps &&
		    ps->info.ps.writes_memory &&
		    ps->info.ps.early_fragment_test &&
		    !dsa_order_invariant.pass_set)
			return false;

		/* Determine if out-of-order rasterization should be disabled
		 * when occlusion queries are used.
		 */
		pipeline->graphics.disable_out_of_order_rast_for_occlusion =
			!dsa_order_invariant.pass_set;
	}

	/* No color buffers are enabled for writing. */
	if (!colormask)
		return true;

	unsigned blendmask = colormask & blend->blend_enable_4bit;

	if (blendmask) {
		/* Only commutative blending. */
		if (blendmask & ~blend->commutative_4bit)
			return false;

		if (!dsa_order_invariant.pass_set)
			return false;
	}

	if (colormask & ~blendmask)
		return false;

	return true;
}

static const VkConservativeRasterizationModeEXT
radv_get_conservative_raster_mode(const VkPipelineRasterizationStateCreateInfo *pCreateInfo)
{
	const VkPipelineRasterizationConservativeStateCreateInfoEXT *conservative_raster =
		vk_find_struct_const(pCreateInfo->pNext, PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT);

	if (!conservative_raster)
		return VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT;
	return conservative_raster->conservativeRasterizationMode;
}

static void
radv_pipeline_init_multisample_state(struct radv_pipeline *pipeline,
				     const struct radv_blend_state *blend,
				     const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const VkPipelineMultisampleStateCreateInfo *vkms = radv_pipeline_get_multisample_state(pCreateInfo);
	struct radv_multisample_state *ms = &pipeline->graphics.ms;
	unsigned num_tile_pipes = pipeline->device->physical_device->rad_info.num_tile_pipes;
	const VkConservativeRasterizationModeEXT mode =
		radv_get_conservative_raster_mode(pCreateInfo->pRasterizationState);
	bool out_of_order_rast = false;
	int ps_iter_samples = 1;
	uint32_t mask = 0xffff;

	if (vkms) {
		ms->num_samples = vkms->rasterizationSamples;

		/* From the Vulkan 1.1.129 spec, 26.7. Sample Shading:
		 *
		 * "Sample shading is enabled for a graphics pipeline:
		 *
		 * - If the interface of the fragment shader entry point of the
		 *   graphics pipeline includes an input variable decorated
		 *   with SampleId or SamplePosition. In this case
		 *   minSampleShadingFactor takes the value 1.0.
		 * - Else if the sampleShadingEnable member of the
		 *   VkPipelineMultisampleStateCreateInfo structure specified
		 *   when creating the graphics pipeline is set to VK_TRUE. In
		 *   this case minSampleShadingFactor takes the value of
		 *   VkPipelineMultisampleStateCreateInfo::minSampleShading.
		 *
		 * Otherwise, sample shading is considered disabled."
		 */
		if (pipeline->shaders[MESA_SHADER_FRAGMENT]->info.ps.force_persample) {
			ps_iter_samples = ms->num_samples;
		} else {
			ps_iter_samples = radv_pipeline_get_ps_iter_samples(pCreateInfo);
		}
	} else {
		ms->num_samples = 1;
	}

	const struct VkPipelineRasterizationStateRasterizationOrderAMD *raster_order =
		vk_find_struct_const(pCreateInfo->pRasterizationState->pNext, PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD);
	if (raster_order && raster_order->rasterizationOrder == VK_RASTERIZATION_ORDER_RELAXED_AMD) {
		/* Out-of-order rasterization is explicitly enabled by the
		 * application.
		 */
		out_of_order_rast = true;
	} else {
		/* Determine if the driver can enable out-of-order
		 * rasterization internally.
		 */
		out_of_order_rast =
			radv_pipeline_out_of_order_rast(pipeline, blend, pCreateInfo);
	}

	ms->pa_sc_aa_config = 0;
	ms->db_eqaa = S_028804_HIGH_QUALITY_INTERSECTIONS(1) |
		      S_028804_INCOHERENT_EQAA_READS(1) |
		      S_028804_INTERPOLATE_COMP_Z(1) |
		      S_028804_STATIC_ANCHOR_ASSOCIATIONS(1);

	/* Adjust MSAA state if conservative rasterization is enabled. */
	if (mode != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT) {
		ms->pa_sc_aa_config |= S_028BE0_AA_MASK_CENTROID_DTMN(1);

		ms->db_eqaa |= S_028804_ENABLE_POSTZ_OVERRASTERIZATION(1) |
			       S_028804_OVERRASTERIZATION_AMOUNT(4);
	}

	ms->pa_sc_mode_cntl_1 =
		S_028A4C_WALK_FENCE_ENABLE(1) | //TODO linear dst fixes
		S_028A4C_WALK_FENCE_SIZE(num_tile_pipes == 2 ? 2 : 3) |
		S_028A4C_OUT_OF_ORDER_PRIMITIVE_ENABLE(out_of_order_rast) |
		S_028A4C_OUT_OF_ORDER_WATER_MARK(0x7) |
		/* always 1: */
		S_028A4C_WALK_ALIGN8_PRIM_FITS_ST(1) |
		S_028A4C_SUPERTILE_WALK_ORDER_ENABLE(1) |
		S_028A4C_TILE_WALK_ORDER_ENABLE(1) |
		S_028A4C_MULTI_SHADER_ENGINE_PRIM_DISCARD_ENABLE(1) |
		S_028A4C_FORCE_EOV_CNTDWN_ENABLE(1) |
		S_028A4C_FORCE_EOV_REZ_ENABLE(1);
	ms->pa_sc_mode_cntl_0 = S_028A48_ALTERNATE_RBS_PER_TILE(pipeline->device->physical_device->rad_info.chip_class >= GFX9) |
	                        S_028A48_VPORT_SCISSOR_ENABLE(1);

	const VkPipelineRasterizationLineStateCreateInfoEXT *rast_line =
		vk_find_struct_const(pCreateInfo->pRasterizationState->pNext,
				     PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT);
	if (rast_line) {
		ms->pa_sc_mode_cntl_0 |= S_028A48_LINE_STIPPLE_ENABLE(rast_line->stippledLineEnable);
		if (rast_line->lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT) {
			/* From the Vulkan spec 1.1.129:
			 *
			 * "When VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT lines
			 *  are being rasterized, sample locations may all be
			 *  treated as being at the pixel center (this may
			 *  affect attribute and depth interpolation)."
			 */
			ms->num_samples = 1;
		}
	}

	if (ms->num_samples > 1) {
		RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
		struct radv_subpass *subpass = &pass->subpasses[pCreateInfo->subpass];
		uint32_t z_samples = subpass->depth_stencil_attachment ? subpass->depth_sample_count : ms->num_samples;
		unsigned log_samples = util_logbase2(ms->num_samples);
		unsigned log_z_samples = util_logbase2(z_samples);
		unsigned log_ps_iter_samples = util_logbase2(ps_iter_samples);
		ms->pa_sc_mode_cntl_0 |= S_028A48_MSAA_ENABLE(1);
		ms->db_eqaa |= S_028804_MAX_ANCHOR_SAMPLES(log_z_samples) |
			S_028804_PS_ITER_SAMPLES(log_ps_iter_samples) |
			S_028804_MASK_EXPORT_NUM_SAMPLES(log_samples) |
			S_028804_ALPHA_TO_MASK_NUM_SAMPLES(log_samples);
		ms->pa_sc_aa_config |= S_028BE0_MSAA_NUM_SAMPLES(log_samples) |
			S_028BE0_MAX_SAMPLE_DIST(radv_get_default_max_sample_dist(log_samples)) |
			S_028BE0_MSAA_EXPOSED_SAMPLES(log_samples) | /* CM_R_028BE0_PA_SC_AA_CONFIG */
			S_028BE0_COVERED_CENTROID_IS_CENTER(pipeline->device->physical_device->rad_info.chip_class >= GFX10_3);
		ms->pa_sc_mode_cntl_1 |= S_028A4C_PS_ITER_SAMPLE(ps_iter_samples > 1);
		if (ps_iter_samples > 1)
			pipeline->graphics.spi_baryc_cntl |= S_0286E0_POS_FLOAT_LOCATION(2);
	}

	if (vkms && vkms->pSampleMask) {
		mask = vkms->pSampleMask[0] & 0xffff;
	}

	ms->pa_sc_aa_mask[0] = mask | (mask << 16);
	ms->pa_sc_aa_mask[1] = mask | (mask << 16);
}

static bool
radv_prim_can_use_guardband(enum VkPrimitiveTopology topology)
{
	switch (topology) {
	case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		return false;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		return true;
	default:
		unreachable("unhandled primitive type");
	}
}

static uint32_t
si_conv_gl_prim_to_gs_out(unsigned gl_prim)
{
	switch (gl_prim) {
	case 0: /* GL_POINTS */
		return V_028A6C_POINTLIST;
	case 1: /* GL_LINES */
	case 3: /* GL_LINE_STRIP */
	case 0xA: /* GL_LINE_STRIP_ADJACENCY_ARB */
	case 0x8E7A: /* GL_ISOLINES */
		return V_028A6C_LINESTRIP;

	case 4: /* GL_TRIANGLES */
	case 0xc: /* GL_TRIANGLES_ADJACENCY_ARB */
	case 5: /* GL_TRIANGLE_STRIP */
	case 7: /* GL_QUADS */
		return V_028A6C_TRISTRIP;
	default:
		assert(0);
		return 0;
	}
}

static uint32_t
si_conv_prim_to_gs_out(enum VkPrimitiveTopology topology)
{
	switch (topology) {
	case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
	case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
		return V_028A6C_POINTLIST;
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
		return V_028A6C_LINESTRIP;
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		return V_028A6C_TRISTRIP;
	default:
		assert(0);
		return 0;
	}
}

static unsigned radv_dynamic_state_mask(VkDynamicState state)
{
	switch(state) {
	case VK_DYNAMIC_STATE_VIEWPORT:
	case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT:
		return RADV_DYNAMIC_VIEWPORT;
	case VK_DYNAMIC_STATE_SCISSOR:
	case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT:
		return RADV_DYNAMIC_SCISSOR;
	case VK_DYNAMIC_STATE_LINE_WIDTH:
		return RADV_DYNAMIC_LINE_WIDTH;
	case VK_DYNAMIC_STATE_DEPTH_BIAS:
		return RADV_DYNAMIC_DEPTH_BIAS;
	case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
		return RADV_DYNAMIC_BLEND_CONSTANTS;
	case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
		return RADV_DYNAMIC_DEPTH_BOUNDS;
	case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
		return RADV_DYNAMIC_STENCIL_COMPARE_MASK;
	case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
		return RADV_DYNAMIC_STENCIL_WRITE_MASK;
	case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
		return RADV_DYNAMIC_STENCIL_REFERENCE;
	case VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT:
		return RADV_DYNAMIC_DISCARD_RECTANGLE;
	case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
		return RADV_DYNAMIC_SAMPLE_LOCATIONS;
	case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
		return RADV_DYNAMIC_LINE_STIPPLE;
	case VK_DYNAMIC_STATE_CULL_MODE_EXT:
		return RADV_DYNAMIC_CULL_MODE;
	case VK_DYNAMIC_STATE_FRONT_FACE_EXT:
		return RADV_DYNAMIC_FRONT_FACE;
	case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT:
		return RADV_DYNAMIC_PRIMITIVE_TOPOLOGY;
	case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT:
		return RADV_DYNAMIC_DEPTH_TEST_ENABLE;
	case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT:
		return RADV_DYNAMIC_DEPTH_WRITE_ENABLE;
	case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT:
		return RADV_DYNAMIC_DEPTH_COMPARE_OP;
	case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT:
		return RADV_DYNAMIC_DEPTH_BOUNDS_TEST_ENABLE;
	case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT:
		return RADV_DYNAMIC_STENCIL_TEST_ENABLE;
	case VK_DYNAMIC_STATE_STENCIL_OP_EXT:
		return RADV_DYNAMIC_STENCIL_OP;
	case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT:
		return RADV_DYNAMIC_VERTEX_INPUT_BINDING_STRIDE;
	default:
		unreachable("Unhandled dynamic state");
	}
}

static uint32_t radv_pipeline_needed_dynamic_state(const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	uint32_t states = RADV_DYNAMIC_ALL;

	/* If rasterization is disabled we do not care about any of the
	 * dynamic states, since they are all rasterization related only,
	 * except primitive topology and vertex binding stride.
	 */
	if (pCreateInfo->pRasterizationState->rasterizerDiscardEnable)
		return RADV_DYNAMIC_PRIMITIVE_TOPOLOGY |
		       RADV_DYNAMIC_VERTEX_INPUT_BINDING_STRIDE;

	if (!pCreateInfo->pRasterizationState->depthBiasEnable)
		states &= ~RADV_DYNAMIC_DEPTH_BIAS;

	if (!pCreateInfo->pDepthStencilState ||
	    (!pCreateInfo->pDepthStencilState->depthBoundsTestEnable &&
	     !radv_is_state_dynamic(pCreateInfo, VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT)))
		states &= ~RADV_DYNAMIC_DEPTH_BOUNDS;

	if (!pCreateInfo->pDepthStencilState ||
	    (!pCreateInfo->pDepthStencilState->stencilTestEnable &&
	     !radv_is_state_dynamic(pCreateInfo, VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT)))
		states &= ~(RADV_DYNAMIC_STENCIL_COMPARE_MASK |
		            RADV_DYNAMIC_STENCIL_WRITE_MASK |
		            RADV_DYNAMIC_STENCIL_REFERENCE);

	if (!vk_find_struct_const(pCreateInfo->pNext, PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT))
		states &= ~RADV_DYNAMIC_DISCARD_RECTANGLE;

	if (!pCreateInfo->pMultisampleState ||
	    !vk_find_struct_const(pCreateInfo->pMultisampleState->pNext,
				  PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT))
		states &= ~RADV_DYNAMIC_SAMPLE_LOCATIONS;

	if (!pCreateInfo->pRasterizationState ||
	    !vk_find_struct_const(pCreateInfo->pRasterizationState->pNext,
				  PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT))
		states &= ~RADV_DYNAMIC_LINE_STIPPLE;

	/* TODO: blend constants & line width. */

	return states;
}

static struct radv_ia_multi_vgt_param_helpers
radv_compute_ia_multi_vgt_param_helpers(struct radv_pipeline *pipeline)
{
	struct radv_ia_multi_vgt_param_helpers ia_multi_vgt_param = {0};
	const struct radv_device *device = pipeline->device;

	if (radv_pipeline_has_tess(pipeline))
		ia_multi_vgt_param.primgroup_size = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.num_patches;
	else if (radv_pipeline_has_gs(pipeline))
		ia_multi_vgt_param.primgroup_size = 64;
	else
		ia_multi_vgt_param.primgroup_size = 128; /* recommended without a GS */

	/* GS requirement. */
	ia_multi_vgt_param.partial_es_wave = false;
	if (radv_pipeline_has_gs(pipeline) && device->physical_device->rad_info.chip_class <= GFX8)
		if (SI_GS_PER_ES / ia_multi_vgt_param.primgroup_size >= pipeline->device->gs_table_depth - 3)
			ia_multi_vgt_param.partial_es_wave = true;

	ia_multi_vgt_param.ia_switch_on_eoi = false;
	if (pipeline->shaders[MESA_SHADER_FRAGMENT]->info.ps.prim_id_input)
		ia_multi_vgt_param.ia_switch_on_eoi = true;
	if (radv_pipeline_has_gs(pipeline) &&
	    pipeline->shaders[MESA_SHADER_GEOMETRY]->info.uses_prim_id)
		ia_multi_vgt_param.ia_switch_on_eoi = true;
	if (radv_pipeline_has_tess(pipeline)) {
		/* SWITCH_ON_EOI must be set if PrimID is used. */
		if (pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.uses_prim_id ||
		    radv_get_shader(pipeline, MESA_SHADER_TESS_EVAL)->info.uses_prim_id)
			ia_multi_vgt_param.ia_switch_on_eoi = true;
	}

	ia_multi_vgt_param.partial_vs_wave = false;
	if (radv_pipeline_has_tess(pipeline)) {
		/* Bug with tessellation and GS on Bonaire and older 2 SE chips. */
		if ((device->physical_device->rad_info.family == CHIP_TAHITI ||
		     device->physical_device->rad_info.family == CHIP_PITCAIRN ||
		     device->physical_device->rad_info.family == CHIP_BONAIRE) &&
		    radv_pipeline_has_gs(pipeline))
			ia_multi_vgt_param.partial_vs_wave = true;
		/* Needed for 028B6C_DISTRIBUTION_MODE != 0 */
		if (device->physical_device->rad_info.has_distributed_tess) {
			if (radv_pipeline_has_gs(pipeline)) {
				if (device->physical_device->rad_info.chip_class <= GFX8)
					ia_multi_vgt_param.partial_es_wave = true;
			} else {
				ia_multi_vgt_param.partial_vs_wave = true;
			}
		}
	}

	if (radv_pipeline_has_gs(pipeline)) {
		/* On these chips there is the possibility of a hang if the
		 * pipeline uses a GS and partial_vs_wave is not set.
		 *
		 * This mostly does not hit 4-SE chips, as those typically set
		 * ia_switch_on_eoi and then partial_vs_wave is set for pipelines
		 * with GS due to another workaround.
		 *
		 * Reproducer: https://bugs.freedesktop.org/show_bug.cgi?id=109242
		 */
		if (device->physical_device->rad_info.family == CHIP_TONGA ||
		    device->physical_device->rad_info.family == CHIP_FIJI ||
		    device->physical_device->rad_info.family == CHIP_POLARIS10 ||
		    device->physical_device->rad_info.family == CHIP_POLARIS11 ||
		    device->physical_device->rad_info.family == CHIP_POLARIS12 ||
	            device->physical_device->rad_info.family == CHIP_VEGAM) {
			ia_multi_vgt_param.partial_vs_wave = true;
		}
	}

	ia_multi_vgt_param.base =
		S_028AA8_PRIMGROUP_SIZE(ia_multi_vgt_param.primgroup_size - 1) |
		/* The following field was moved to VGT_SHADER_STAGES_EN in GFX9. */
		S_028AA8_MAX_PRIMGRP_IN_WAVE(device->physical_device->rad_info.chip_class == GFX8 ? 2 : 0) |
		S_030960_EN_INST_OPT_BASIC(device->physical_device->rad_info.chip_class >= GFX9) |
		S_030960_EN_INST_OPT_ADV(device->physical_device->rad_info.chip_class >= GFX9);

	return ia_multi_vgt_param;
}

static void
radv_pipeline_init_input_assembly_state(struct radv_pipeline *pipeline,
					const VkGraphicsPipelineCreateInfo *pCreateInfo,
					const struct radv_graphics_pipeline_create_info *extra)
{
	const VkPipelineInputAssemblyStateCreateInfo *ia_state = pCreateInfo->pInputAssemblyState;
	struct radv_shader_variant *tes = pipeline->shaders[MESA_SHADER_TESS_EVAL];
	struct radv_shader_variant *gs = pipeline->shaders[MESA_SHADER_GEOMETRY];

	pipeline->graphics.prim_restart_enable = !!ia_state->primitiveRestartEnable;
	pipeline->graphics.can_use_guardband = radv_prim_can_use_guardband(ia_state->topology);

	if (radv_pipeline_has_gs(pipeline)) {
		if (si_conv_gl_prim_to_gs_out(gs->info.gs.output_prim) == V_028A6C_TRISTRIP)
			pipeline->graphics.can_use_guardband = true;
	} else if (radv_pipeline_has_tess(pipeline)) {
		if (!tes->info.tes.point_mode &&
		    si_conv_gl_prim_to_gs_out(tes->info.tes.primitive_mode) == V_028A6C_TRISTRIP)
			pipeline->graphics.can_use_guardband = true;
	}

	if (extra && extra->use_rectlist) {
		pipeline->graphics.can_use_guardband = true;
	}

	pipeline->graphics.ia_multi_vgt_param =
		radv_compute_ia_multi_vgt_param_helpers(pipeline);
}

static void
radv_pipeline_init_dynamic_state(struct radv_pipeline *pipeline,
				 const VkGraphicsPipelineCreateInfo *pCreateInfo,
				 const struct radv_graphics_pipeline_create_info *extra)
{
	uint32_t needed_states = radv_pipeline_needed_dynamic_state(pCreateInfo);
	uint32_t states = needed_states;
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = &pass->subpasses[pCreateInfo->subpass];

	pipeline->dynamic_state = default_dynamic_state;
	pipeline->graphics.needed_dynamic_state = needed_states;

	if (pCreateInfo->pDynamicState) {
		/* Remove all of the states that are marked as dynamic */
		uint32_t count = pCreateInfo->pDynamicState->dynamicStateCount;
		for (uint32_t s = 0; s < count; s++)
			states &= ~radv_dynamic_state_mask(pCreateInfo->pDynamicState->pDynamicStates[s]);
	}

	struct radv_dynamic_state *dynamic = &pipeline->dynamic_state;

	if (needed_states & RADV_DYNAMIC_VIEWPORT) {
		assert(pCreateInfo->pViewportState);

		dynamic->viewport.count = pCreateInfo->pViewportState->viewportCount;
		if (states & RADV_DYNAMIC_VIEWPORT) {
			typed_memcpy(dynamic->viewport.viewports,
			             pCreateInfo->pViewportState->pViewports,
			             pCreateInfo->pViewportState->viewportCount);
		}
	}

	if (needed_states & RADV_DYNAMIC_SCISSOR) {
		dynamic->scissor.count = pCreateInfo->pViewportState->scissorCount;
		if (states & RADV_DYNAMIC_SCISSOR) {
			typed_memcpy(dynamic->scissor.scissors,
			             pCreateInfo->pViewportState->pScissors,
			             pCreateInfo->pViewportState->scissorCount);
		}
	}

	if (states & RADV_DYNAMIC_LINE_WIDTH) {
		assert(pCreateInfo->pRasterizationState);
		dynamic->line_width = pCreateInfo->pRasterizationState->lineWidth;
	}

	if (states & RADV_DYNAMIC_DEPTH_BIAS) {
		assert(pCreateInfo->pRasterizationState);
		dynamic->depth_bias.bias =
			pCreateInfo->pRasterizationState->depthBiasConstantFactor;
		dynamic->depth_bias.clamp =
			pCreateInfo->pRasterizationState->depthBiasClamp;
		dynamic->depth_bias.slope =
			pCreateInfo->pRasterizationState->depthBiasSlopeFactor;
	}

	/* Section 9.2 of the Vulkan 1.0.15 spec says:
	 *
	 *    pColorBlendState is [...] NULL if the pipeline has rasterization
	 *    disabled or if the subpass of the render pass the pipeline is
	 *    created against does not use any color attachments.
	 */
	if (subpass->has_color_att && states & RADV_DYNAMIC_BLEND_CONSTANTS) {
		assert(pCreateInfo->pColorBlendState);
		typed_memcpy(dynamic->blend_constants,
			     pCreateInfo->pColorBlendState->blendConstants, 4);
	}

	if (states & RADV_DYNAMIC_CULL_MODE) {
		dynamic->cull_mode =
			pCreateInfo->pRasterizationState->cullMode;
	}

	if (states & RADV_DYNAMIC_FRONT_FACE) {
		dynamic->front_face =
			pCreateInfo->pRasterizationState->frontFace;
	}

	if (states & RADV_DYNAMIC_PRIMITIVE_TOPOLOGY) {
		dynamic->primitive_topology =
			si_translate_prim(pCreateInfo->pInputAssemblyState->topology);
		if (extra && extra->use_rectlist) {
			dynamic->primitive_topology = V_008958_DI_PT_RECTLIST;
		}
	}

	/* If there is no depthstencil attachment, then don't read
	 * pDepthStencilState. The Vulkan spec states that pDepthStencilState may
	 * be NULL in this case. Even if pDepthStencilState is non-NULL, there is
	 * no need to override the depthstencil defaults in
	 * radv_pipeline::dynamic_state when there is no depthstencil attachment.
	 *
	 * Section 9.2 of the Vulkan 1.0.15 spec says:
	 *
	 *    pDepthStencilState is [...] NULL if the pipeline has rasterization
	 *    disabled or if the subpass of the render pass the pipeline is created
	 *    against does not use a depth/stencil attachment.
	 */
	if (needed_states && subpass->depth_stencil_attachment) {
		assert(pCreateInfo->pDepthStencilState);

		if (states & RADV_DYNAMIC_DEPTH_BOUNDS) {
			dynamic->depth_bounds.min =
				pCreateInfo->pDepthStencilState->minDepthBounds;
			dynamic->depth_bounds.max =
				pCreateInfo->pDepthStencilState->maxDepthBounds;
		}

		if (states & RADV_DYNAMIC_STENCIL_COMPARE_MASK) {
			dynamic->stencil_compare_mask.front =
				pCreateInfo->pDepthStencilState->front.compareMask;
			dynamic->stencil_compare_mask.back =
				pCreateInfo->pDepthStencilState->back.compareMask;
		}

		if (states & RADV_DYNAMIC_STENCIL_WRITE_MASK) {
			dynamic->stencil_write_mask.front =
				pCreateInfo->pDepthStencilState->front.writeMask;
			dynamic->stencil_write_mask.back =
				pCreateInfo->pDepthStencilState->back.writeMask;
		}

		if (states & RADV_DYNAMIC_STENCIL_REFERENCE) {
			dynamic->stencil_reference.front =
				pCreateInfo->pDepthStencilState->front.reference;
			dynamic->stencil_reference.back =
				pCreateInfo->pDepthStencilState->back.reference;
		}

		if (states & RADV_DYNAMIC_DEPTH_TEST_ENABLE) {
			dynamic->depth_test_enable =
				pCreateInfo->pDepthStencilState->depthTestEnable;
		}

		if (states & RADV_DYNAMIC_DEPTH_WRITE_ENABLE) {
			dynamic->depth_write_enable =
				pCreateInfo->pDepthStencilState->depthWriteEnable;
		}

		if (states & RADV_DYNAMIC_DEPTH_COMPARE_OP) {
			dynamic->depth_compare_op =
				pCreateInfo->pDepthStencilState->depthCompareOp;
		}

		if (states & RADV_DYNAMIC_DEPTH_BOUNDS_TEST_ENABLE) {
			dynamic->depth_bounds_test_enable =
				pCreateInfo->pDepthStencilState->depthBoundsTestEnable;
		}

		if (states & RADV_DYNAMIC_STENCIL_TEST_ENABLE) {
			dynamic->stencil_test_enable =
				pCreateInfo->pDepthStencilState->stencilTestEnable;
		}

		if (states & RADV_DYNAMIC_STENCIL_OP) {
			dynamic->stencil_op.front.compare_op =
				pCreateInfo->pDepthStencilState->front.compareOp;
			dynamic->stencil_op.front.fail_op =
				pCreateInfo->pDepthStencilState->front.failOp;
			dynamic->stencil_op.front.pass_op =
				pCreateInfo->pDepthStencilState->front.passOp;
			dynamic->stencil_op.front.depth_fail_op =
				pCreateInfo->pDepthStencilState->front.depthFailOp;

			dynamic->stencil_op.back.compare_op =
				pCreateInfo->pDepthStencilState->back.compareOp;
			dynamic->stencil_op.back.fail_op =
				pCreateInfo->pDepthStencilState->back.failOp;
			dynamic->stencil_op.back.pass_op =
				pCreateInfo->pDepthStencilState->back.passOp;
			dynamic->stencil_op.back.depth_fail_op =
				pCreateInfo->pDepthStencilState->back.depthFailOp;
		}
	}

	const  VkPipelineDiscardRectangleStateCreateInfoEXT *discard_rectangle_info =
			vk_find_struct_const(pCreateInfo->pNext, PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT);
	if (needed_states & RADV_DYNAMIC_DISCARD_RECTANGLE) {
		dynamic->discard_rectangle.count = discard_rectangle_info->discardRectangleCount;
		if (states & RADV_DYNAMIC_DISCARD_RECTANGLE) {
			typed_memcpy(dynamic->discard_rectangle.rectangles,
			             discard_rectangle_info->pDiscardRectangles,
			             discard_rectangle_info->discardRectangleCount);
		}
	}

	if (needed_states & RADV_DYNAMIC_SAMPLE_LOCATIONS) {
		const VkPipelineSampleLocationsStateCreateInfoEXT *sample_location_info =
			vk_find_struct_const(pCreateInfo->pMultisampleState->pNext,
					     PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT);
		/* If sampleLocationsEnable is VK_FALSE, the default sample
		 * locations are used and the values specified in
		 * sampleLocationsInfo are ignored.
		 */
		if (sample_location_info->sampleLocationsEnable) {
			const VkSampleLocationsInfoEXT *pSampleLocationsInfo =
				&sample_location_info->sampleLocationsInfo;

			assert(pSampleLocationsInfo->sampleLocationsCount <= MAX_SAMPLE_LOCATIONS);

			dynamic->sample_location.per_pixel = pSampleLocationsInfo->sampleLocationsPerPixel;
			dynamic->sample_location.grid_size = pSampleLocationsInfo->sampleLocationGridSize;
			dynamic->sample_location.count = pSampleLocationsInfo->sampleLocationsCount;
			typed_memcpy(&dynamic->sample_location.locations[0],
				     pSampleLocationsInfo->pSampleLocations,
				     pSampleLocationsInfo->sampleLocationsCount);
		}
	}

	const VkPipelineRasterizationLineStateCreateInfoEXT *rast_line_info =
		vk_find_struct_const(pCreateInfo->pRasterizationState->pNext,
				     PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT);
	if (needed_states & RADV_DYNAMIC_LINE_STIPPLE) {
		dynamic->line_stipple.factor = rast_line_info->lineStippleFactor;
		dynamic->line_stipple.pattern = rast_line_info->lineStipplePattern;
	}

	if (!(states & RADV_DYNAMIC_VERTEX_INPUT_BINDING_STRIDE))
		pipeline->graphics.uses_dynamic_stride = true;

	pipeline->dynamic_state.mask = states;
}

static void
radv_pipeline_init_raster_state(struct radv_pipeline *pipeline,
				const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const VkPipelineRasterizationStateCreateInfo *raster_info =
		pCreateInfo->pRasterizationState;

	pipeline->graphics.pa_su_sc_mode_cntl =
		S_028814_FACE(raster_info->frontFace) |
		S_028814_CULL_FRONT(!!(raster_info->cullMode & VK_CULL_MODE_FRONT_BIT)) |
		S_028814_CULL_BACK(!!(raster_info->cullMode & VK_CULL_MODE_BACK_BIT)) |
		S_028814_POLY_MODE(raster_info->polygonMode != VK_POLYGON_MODE_FILL) |
		S_028814_POLYMODE_FRONT_PTYPE(si_translate_fill(raster_info->polygonMode)) |
		S_028814_POLYMODE_BACK_PTYPE(si_translate_fill(raster_info->polygonMode)) |
		S_028814_POLY_OFFSET_FRONT_ENABLE(raster_info->depthBiasEnable ? 1 : 0) |
		S_028814_POLY_OFFSET_BACK_ENABLE(raster_info->depthBiasEnable ? 1 : 0) |
		S_028814_POLY_OFFSET_PARA_ENABLE(raster_info->depthBiasEnable ? 1 : 0);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
		/* It should also be set if PERPENDICULAR_ENDCAP_ENA is set. */
		pipeline->graphics.pa_su_sc_mode_cntl |=
			S_028814_KEEP_TOGETHER_ENABLE(raster_info->polygonMode != VK_POLYGON_MODE_FILL);
	}
}

static void
radv_pipeline_init_depth_stencil_state(struct radv_pipeline *pipeline,
				       const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const VkPipelineDepthStencilStateCreateInfo *ds_info
		= radv_pipeline_get_depth_stencil_state(pCreateInfo);
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	struct radv_render_pass_attachment *attachment = NULL;
	uint32_t db_depth_control = 0;

	if (subpass->depth_stencil_attachment)
		attachment = pass->attachments + subpass->depth_stencil_attachment->attachment;

	bool has_depth_attachment = attachment && vk_format_is_depth(attachment->format);
	bool has_stencil_attachment = attachment && vk_format_is_stencil(attachment->format);

	if (ds_info) {
		if (has_depth_attachment) {
			db_depth_control = S_028800_Z_ENABLE(ds_info->depthTestEnable ? 1 : 0) |
			                   S_028800_Z_WRITE_ENABLE(ds_info->depthWriteEnable ? 1 : 0) |
			                   S_028800_ZFUNC(ds_info->depthCompareOp) |
			                   S_028800_DEPTH_BOUNDS_ENABLE(ds_info->depthBoundsTestEnable ? 1 : 0);
		}

		if (has_stencil_attachment && ds_info->stencilTestEnable) {
			db_depth_control |= S_028800_STENCIL_ENABLE(1) | S_028800_BACKFACE_ENABLE(1);
			db_depth_control |= S_028800_STENCILFUNC(ds_info->front.compareOp);
			db_depth_control |= S_028800_STENCILFUNC_BF(ds_info->back.compareOp);
		}
	}

	pipeline->graphics.db_depth_control = db_depth_control;
}

static void
gfx9_get_gs_info(const struct radv_pipeline_key *key,
                 const struct radv_pipeline *pipeline,
		 nir_shader **nir,
		 struct radv_shader_info *infos,
		 struct gfx9_gs_info *out)
{
	struct radv_shader_info *gs_info = &infos[MESA_SHADER_GEOMETRY];
	struct radv_es_output_info *es_info;
	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9)
		es_info = nir[MESA_SHADER_TESS_CTRL] ? &gs_info->tes.es_info : &gs_info->vs.es_info;
	else
		es_info = nir[MESA_SHADER_TESS_CTRL] ?
                       &infos[MESA_SHADER_TESS_EVAL].tes.es_info :
                       &infos[MESA_SHADER_VERTEX].vs.es_info;

	unsigned gs_num_invocations = MAX2(gs_info->gs.invocations, 1);
	bool uses_adjacency;
	switch(key->topology) {
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		uses_adjacency = true;
		break;
	default:
		uses_adjacency = false;
		break;
	}

	/* All these are in dwords: */
	/* We can't allow using the whole LDS, because GS waves compete with
	 * other shader stages for LDS space. */
	const unsigned max_lds_size = 8 * 1024;
	const unsigned esgs_itemsize = es_info->esgs_itemsize / 4;
	unsigned esgs_lds_size;

	/* All these are per subgroup: */
	const unsigned max_out_prims = 32 * 1024;
	const unsigned max_es_verts = 255;
	const unsigned ideal_gs_prims = 64;
	unsigned max_gs_prims, gs_prims;
	unsigned min_es_verts, es_verts, worst_case_es_verts;

	if (uses_adjacency || gs_num_invocations > 1)
		max_gs_prims = 127 / gs_num_invocations;
	else
		max_gs_prims = 255;

	/* MAX_PRIMS_PER_SUBGROUP = gs_prims * max_vert_out * gs_invocations.
	 * Make sure we don't go over the maximum value.
	 */
	if (gs_info->gs.vertices_out > 0) {
		max_gs_prims = MIN2(max_gs_prims,
				    max_out_prims /
				    (gs_info->gs.vertices_out * gs_num_invocations));
	}
	assert(max_gs_prims > 0);

	/* If the primitive has adjacency, halve the number of vertices
	 * that will be reused in multiple primitives.
	 */
	min_es_verts = gs_info->gs.vertices_in / (uses_adjacency ? 2 : 1);

	gs_prims = MIN2(ideal_gs_prims, max_gs_prims);
	worst_case_es_verts = MIN2(min_es_verts * gs_prims, max_es_verts);

	/* Compute ESGS LDS size based on the worst case number of ES vertices
	 * needed to create the target number of GS prims per subgroup.
	 */
	esgs_lds_size = esgs_itemsize * worst_case_es_verts;

	/* If total LDS usage is too big, refactor partitions based on ratio
	 * of ESGS item sizes.
	 */
	if (esgs_lds_size > max_lds_size) {
		/* Our target GS Prims Per Subgroup was too large. Calculate
		 * the maximum number of GS Prims Per Subgroup that will fit
		 * into LDS, capped by the maximum that the hardware can support.
		 */
		gs_prims = MIN2((max_lds_size / (esgs_itemsize * min_es_verts)),
				max_gs_prims);
		assert(gs_prims > 0);
		worst_case_es_verts = MIN2(min_es_verts * gs_prims,
					   max_es_verts);

		esgs_lds_size = esgs_itemsize * worst_case_es_verts;
		assert(esgs_lds_size <= max_lds_size);
	}

	/* Now calculate remaining ESGS information. */
	if (esgs_lds_size)
		es_verts = MIN2(esgs_lds_size / esgs_itemsize, max_es_verts);
	else
		es_verts = max_es_verts;

	/* Vertices for adjacency primitives are not always reused, so restore
	 * it for ES_VERTS_PER_SUBGRP.
	 */
	min_es_verts = gs_info->gs.vertices_in;

	/* For normal primitives, the VGT only checks if they are past the ES
	 * verts per subgroup after allocating a full GS primitive and if they
	 * are, kick off a new subgroup.  But if those additional ES verts are
	 * unique (e.g. not reused) we need to make sure there is enough LDS
	 * space to account for those ES verts beyond ES_VERTS_PER_SUBGRP.
	 */
	es_verts -= min_es_verts - 1;

	uint32_t es_verts_per_subgroup = es_verts;
	uint32_t gs_prims_per_subgroup = gs_prims;
	uint32_t gs_inst_prims_in_subgroup = gs_prims * gs_num_invocations;
	uint32_t max_prims_per_subgroup = gs_inst_prims_in_subgroup * gs_info->gs.vertices_out;
	out->lds_size = align(esgs_lds_size, 128) / 128;
	out->vgt_gs_onchip_cntl = S_028A44_ES_VERTS_PER_SUBGRP(es_verts_per_subgroup) |
	                        S_028A44_GS_PRIMS_PER_SUBGRP(gs_prims_per_subgroup) |
	                        S_028A44_GS_INST_PRIMS_IN_SUBGRP(gs_inst_prims_in_subgroup);
	out->vgt_gs_max_prims_per_subgroup = S_028A94_MAX_PRIMS_PER_SUBGROUP(max_prims_per_subgroup);
	out->vgt_esgs_ring_itemsize  = esgs_itemsize;
	assert(max_prims_per_subgroup <= max_out_prims);
}

static void clamp_gsprims_to_esverts(unsigned *max_gsprims, unsigned max_esverts,
				     unsigned min_verts_per_prim, bool use_adjacency)
{
	unsigned max_reuse = max_esverts - min_verts_per_prim;
	if (use_adjacency)
		max_reuse /= 2;
	*max_gsprims = MIN2(*max_gsprims, 1 + max_reuse);
}

static unsigned
radv_get_num_input_vertices(nir_shader **nir)
{
	if (nir[MESA_SHADER_GEOMETRY]) {
		nir_shader *gs = nir[MESA_SHADER_GEOMETRY];

		return gs->info.gs.vertices_in;
	}

	if (nir[MESA_SHADER_TESS_CTRL]) {
		nir_shader *tes = nir[MESA_SHADER_TESS_EVAL];

		if (tes->info.tess.point_mode)
			return 1;
		if (tes->info.tess.primitive_mode == GL_ISOLINES)
			return 2;
		return 3;
	}

	return 3;
}

static void
gfx10_get_ngg_info(const struct radv_pipeline_key *key,
		   struct radv_pipeline *pipeline,
		   nir_shader **nir,
		   struct radv_shader_info *infos,
		   struct gfx10_ngg_info *ngg)
{
	struct radv_shader_info *gs_info = &infos[MESA_SHADER_GEOMETRY];
	struct radv_es_output_info *es_info =
		nir[MESA_SHADER_TESS_CTRL] ? &gs_info->tes.es_info : &gs_info->vs.es_info;
	unsigned gs_type = nir[MESA_SHADER_GEOMETRY] ? MESA_SHADER_GEOMETRY : MESA_SHADER_VERTEX;
	unsigned max_verts_per_prim = radv_get_num_input_vertices(nir);
	unsigned min_verts_per_prim =
		gs_type == MESA_SHADER_GEOMETRY ? max_verts_per_prim : 1;
	unsigned gs_num_invocations = nir[MESA_SHADER_GEOMETRY] ? MAX2(gs_info->gs.invocations, 1) : 1;
	bool uses_adjacency;
	switch(key->topology) {
	case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
	case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
		uses_adjacency = true;
		break;
	default:
		uses_adjacency = false;
		break;
	}

	/* All these are in dwords: */
	/* We can't allow using the whole LDS, because GS waves compete with
	 * other shader stages for LDS space.
	 *
	 * TODO: We should really take the shader's internal LDS use into
	 *       account. The linker will fail if the size is greater than
	 *       8K dwords.
	 */
	const unsigned max_lds_size = 8 * 1024 - 768;
	const unsigned target_lds_size = max_lds_size;
	unsigned esvert_lds_size = 0;
	unsigned gsprim_lds_size = 0;

	/* All these are per subgroup: */
	const unsigned min_esverts = pipeline->device->physical_device->rad_info.chip_class >= GFX10_3 ? 29 : 24;
	bool max_vert_out_per_gs_instance = false;
	unsigned max_esverts_base = 256;
	unsigned max_gsprims_base = 128; /* default prim group size clamp */

	/* Hardware has the following non-natural restrictions on the value
	 * of GE_CNTL.VERT_GRP_SIZE based on based on the primitive type of
	 * the draw:
	 *  - at most 252 for any line input primitive type
	 *  - at most 251 for any quad input primitive type
	 *  - at most 251 for triangle strips with adjacency (this happens to
	 *    be the natural limit for triangle *lists* with adjacency)
	 */
	max_esverts_base = MIN2(max_esverts_base, 251 + max_verts_per_prim - 1);

	if (gs_type == MESA_SHADER_GEOMETRY) {
		unsigned max_out_verts_per_gsprim =
			gs_info->gs.vertices_out * gs_num_invocations;

		if (max_out_verts_per_gsprim <= 256) {
			if (max_out_verts_per_gsprim) {
				max_gsprims_base = MIN2(max_gsprims_base,
							256 / max_out_verts_per_gsprim);
			}
		} else {
			/* Use special multi-cycling mode in which each GS
			 * instance gets its own subgroup. Does not work with
			 * tessellation. */
			max_vert_out_per_gs_instance = true;
			max_gsprims_base = 1;
			max_out_verts_per_gsprim = gs_info->gs.vertices_out;
		}

		esvert_lds_size = es_info->esgs_itemsize / 4;
		gsprim_lds_size = (gs_info->gs.gsvs_vertex_size / 4 + 1) * max_out_verts_per_gsprim;
	} else {
		/* VS and TES. */
		/* LDS size for passing data from GS to ES. */
		struct radv_streamout_info *so_info = nir[MESA_SHADER_TESS_CTRL]
			? &infos[MESA_SHADER_TESS_EVAL].so
			: &infos[MESA_SHADER_VERTEX].so;

		if (so_info->num_outputs)
			esvert_lds_size = 4 * so_info->num_outputs + 1;

		/* GS stores Primitive IDs (one DWORD) into LDS at the address
		 * corresponding to the ES thread of the provoking vertex. All
		 * ES threads load and export PrimitiveID for their thread.
		 */
		if (!nir[MESA_SHADER_TESS_CTRL] &&
		    infos[MESA_SHADER_VERTEX].vs.outinfo.export_prim_id)
			esvert_lds_size = MAX2(esvert_lds_size, 1);
	}

	unsigned max_gsprims = max_gsprims_base;
	unsigned max_esverts = max_esverts_base;

	if (esvert_lds_size)
		max_esverts = MIN2(max_esverts, target_lds_size / esvert_lds_size);
	if (gsprim_lds_size)
		max_gsprims = MIN2(max_gsprims, target_lds_size / gsprim_lds_size);

	max_esverts = MIN2(max_esverts, max_gsprims * max_verts_per_prim);
	clamp_gsprims_to_esverts(&max_gsprims, max_esverts, min_verts_per_prim, uses_adjacency);
	assert(max_esverts >= max_verts_per_prim && max_gsprims >= 1);

	if (esvert_lds_size || gsprim_lds_size) {
		/* Now that we have a rough proportionality between esverts
		 * and gsprims based on the primitive type, scale both of them
		 * down simultaneously based on required LDS space.
		 *
		 * We could be smarter about this if we knew how much vertex
		 * reuse to expect.
		 */
		unsigned lds_total = max_esverts * esvert_lds_size +
				     max_gsprims * gsprim_lds_size;
		if (lds_total > target_lds_size) {
			max_esverts = max_esverts * target_lds_size / lds_total;
			max_gsprims = max_gsprims * target_lds_size / lds_total;

			max_esverts = MIN2(max_esverts, max_gsprims * max_verts_per_prim);
			clamp_gsprims_to_esverts(&max_gsprims, max_esverts,
						 min_verts_per_prim, uses_adjacency);
			assert(max_esverts >= max_verts_per_prim && max_gsprims >= 1);
		}
	}

	/* Round up towards full wave sizes for better ALU utilization. */
	if (!max_vert_out_per_gs_instance) {
		unsigned orig_max_esverts;
		unsigned orig_max_gsprims;
		unsigned wavesize;

		if (gs_type == MESA_SHADER_GEOMETRY) {
			wavesize = gs_info->wave_size;
		} else {
			wavesize = nir[MESA_SHADER_TESS_CTRL]
				? infos[MESA_SHADER_TESS_EVAL].wave_size
				: infos[MESA_SHADER_VERTEX].wave_size;
		}

		do {
			orig_max_esverts = max_esverts;
			orig_max_gsprims = max_gsprims;

			max_esverts = align(max_esverts, wavesize);
			max_esverts = MIN2(max_esverts, max_esverts_base);
			if (esvert_lds_size)
				max_esverts = MIN2(max_esverts,
						   (max_lds_size - max_gsprims * gsprim_lds_size) /
						   esvert_lds_size);
			max_esverts = MIN2(max_esverts, max_gsprims * max_verts_per_prim);
			/* Hardware restriction: minimum value of max_esverts */
			max_esverts = MAX2(max_esverts, min_esverts - 1 + max_verts_per_prim);

			max_gsprims = align(max_gsprims, wavesize);
			max_gsprims = MIN2(max_gsprims, max_gsprims_base);
			if (gsprim_lds_size) {
				/* Don't count unusable vertices to the LDS
				 * size. Those are vertices above the maximum
				 * number of vertices that can occur in the
				 * workgroup, which is e.g. max_gsprims * 3
				 * for triangles.
				 */
				unsigned usable_esverts = MIN2(max_esverts, max_gsprims * max_verts_per_prim);
				max_gsprims =
					MIN2(max_gsprims, (max_lds_size - usable_esverts * esvert_lds_size) / gsprim_lds_size);
			}
			clamp_gsprims_to_esverts(&max_gsprims, max_esverts,
						 min_verts_per_prim, uses_adjacency);
			assert(max_esverts >= max_verts_per_prim && max_gsprims >= 1);
		} while (orig_max_esverts != max_esverts || orig_max_gsprims != max_gsprims);

		/* Verify the restriction. */
		assert(max_esverts >= min_esverts - 1 + max_verts_per_prim);
	} else {
		/* Hardware restriction: minimum value of max_esverts */
		max_esverts = MAX2(max_esverts, min_esverts - 1 + max_verts_per_prim);
	}

	unsigned max_out_vertices =
		max_vert_out_per_gs_instance ? gs_info->gs.vertices_out :
		gs_type == MESA_SHADER_GEOMETRY ?
		max_gsprims * gs_num_invocations * gs_info->gs.vertices_out :
		max_esverts;
	assert(max_out_vertices <= 256);

	unsigned prim_amp_factor = 1;
	if (gs_type == MESA_SHADER_GEOMETRY) {
		/* Number of output primitives per GS input primitive after
		 * GS instancing. */
		prim_amp_factor = gs_info->gs.vertices_out;
	}

	/* The GE only checks against the maximum number of ES verts after
	 * allocating a full GS primitive. So we need to ensure that whenever
	 * this check passes, there is enough space for a full primitive without
	 * vertex reuse.
	 */
	ngg->hw_max_esverts = max_esverts - max_verts_per_prim + 1;
	ngg->max_gsprims = max_gsprims;
	ngg->max_out_verts = max_out_vertices;
	ngg->prim_amp_factor = prim_amp_factor;
	ngg->max_vert_out_per_gs_instance = max_vert_out_per_gs_instance;
	ngg->ngg_emit_size = max_gsprims * gsprim_lds_size;

	/* Don't count unusable vertices. */
	ngg->esgs_ring_size =
		MIN2(max_esverts, max_gsprims * max_verts_per_prim) * esvert_lds_size * 4;

	if (gs_type == MESA_SHADER_GEOMETRY) {
		ngg->vgt_esgs_ring_itemsize = es_info->esgs_itemsize / 4;
	} else {
		ngg->vgt_esgs_ring_itemsize = 1;
	}

	pipeline->graphics.esgs_ring_size = ngg->esgs_ring_size;

	assert(ngg->hw_max_esverts >= min_esverts); /* HW limitation */
}

static void
radv_pipeline_init_gs_ring_state(struct radv_pipeline *pipeline,
				 const struct gfx9_gs_info *gs)
{
	struct radv_device *device = pipeline->device;
	unsigned num_se = device->physical_device->rad_info.max_se;
	unsigned wave_size = 64;
	unsigned max_gs_waves = 32 * num_se; /* max 32 per SE on GCN */
	/* On GFX6-GFX7, the value comes from VGT_GS_VERTEX_REUSE = 16.
	 * On GFX8+, the value comes from VGT_VERTEX_REUSE_BLOCK_CNTL = 30 (+2).
	 */
	unsigned gs_vertex_reuse =
		(device->physical_device->rad_info.chip_class >= GFX8 ? 32 : 16) * num_se;
	unsigned alignment = 256 * num_se;
	/* The maximum size is 63.999 MB per SE. */
	unsigned max_size = ((unsigned)(63.999 * 1024 * 1024) & ~255) * num_se;
	struct radv_shader_info *gs_info = &pipeline->shaders[MESA_SHADER_GEOMETRY]->info;

	/* Calculate the minimum size. */
	unsigned min_esgs_ring_size = align(gs->vgt_esgs_ring_itemsize * 4 * gs_vertex_reuse *
					    wave_size, alignment);
	/* These are recommended sizes, not minimum sizes. */
	unsigned esgs_ring_size = max_gs_waves * 2 * wave_size *
		gs->vgt_esgs_ring_itemsize * 4 * gs_info->gs.vertices_in;
	unsigned gsvs_ring_size = max_gs_waves * 2 * wave_size *
		gs_info->gs.max_gsvs_emit_size;

	min_esgs_ring_size = align(min_esgs_ring_size, alignment);
	esgs_ring_size = align(esgs_ring_size, alignment);
	gsvs_ring_size = align(gsvs_ring_size, alignment);

	if (pipeline->device->physical_device->rad_info.chip_class <= GFX8)
		pipeline->graphics.esgs_ring_size = CLAMP(esgs_ring_size, min_esgs_ring_size, max_size);

	pipeline->graphics.gsvs_ring_size = MIN2(gsvs_ring_size, max_size);
}

struct radv_shader_variant *
radv_get_shader(const struct radv_pipeline *pipeline,
		gl_shader_stage stage)
{
	if (stage == MESA_SHADER_VERTEX) {
		if (pipeline->shaders[MESA_SHADER_VERTEX])
			return pipeline->shaders[MESA_SHADER_VERTEX];
		if (pipeline->shaders[MESA_SHADER_TESS_CTRL])
			return pipeline->shaders[MESA_SHADER_TESS_CTRL];
		if (pipeline->shaders[MESA_SHADER_GEOMETRY])
			return pipeline->shaders[MESA_SHADER_GEOMETRY];
	} else if (stage == MESA_SHADER_TESS_EVAL) {
		if (!radv_pipeline_has_tess(pipeline))
			return NULL;
		if (pipeline->shaders[MESA_SHADER_TESS_EVAL])
			return pipeline->shaders[MESA_SHADER_TESS_EVAL];
		if (pipeline->shaders[MESA_SHADER_GEOMETRY])
			return pipeline->shaders[MESA_SHADER_GEOMETRY];
	}
	return pipeline->shaders[stage];
}

static const struct radv_vs_output_info *get_vs_output_info(const struct radv_pipeline *pipeline)
{
	if (radv_pipeline_has_gs(pipeline))
		if (radv_pipeline_has_ngg(pipeline))
			return &pipeline->shaders[MESA_SHADER_GEOMETRY]->info.vs.outinfo;
		else
			return &pipeline->gs_copy_shader->info.vs.outinfo;
	else if (radv_pipeline_has_tess(pipeline))
		return &pipeline->shaders[MESA_SHADER_TESS_EVAL]->info.tes.outinfo;
	else
		return &pipeline->shaders[MESA_SHADER_VERTEX]->info.vs.outinfo;
}

static void
radv_link_shaders(struct radv_pipeline *pipeline, nir_shader **shaders,
		  bool optimize_conservatively)
{
	nir_shader* ordered_shaders[MESA_SHADER_STAGES];
	int shader_count = 0;

	if(shaders[MESA_SHADER_FRAGMENT]) {
		ordered_shaders[shader_count++] = shaders[MESA_SHADER_FRAGMENT];
	}
	if(shaders[MESA_SHADER_GEOMETRY]) {
		ordered_shaders[shader_count++] = shaders[MESA_SHADER_GEOMETRY];
	}
	if(shaders[MESA_SHADER_TESS_EVAL]) {
		ordered_shaders[shader_count++] = shaders[MESA_SHADER_TESS_EVAL];
	}
	if(shaders[MESA_SHADER_TESS_CTRL]) {
		ordered_shaders[shader_count++] = shaders[MESA_SHADER_TESS_CTRL];
	}
	if(shaders[MESA_SHADER_VERTEX]) {
		ordered_shaders[shader_count++] = shaders[MESA_SHADER_VERTEX];
	}
	if(shaders[MESA_SHADER_COMPUTE]) {
		ordered_shaders[shader_count++] = shaders[MESA_SHADER_COMPUTE];
	}

	if (!optimize_conservatively && shader_count > 1) {
		unsigned first = ordered_shaders[shader_count - 1]->info.stage;
		unsigned last = ordered_shaders[0]->info.stage;

		if (ordered_shaders[0]->info.stage == MESA_SHADER_FRAGMENT &&
		    ordered_shaders[1]->info.has_transform_feedback_varyings)
			nir_link_xfb_varyings(ordered_shaders[1], ordered_shaders[0]);

		for (int i = 1; i < shader_count; ++i) {
			nir_lower_io_arrays_to_elements(ordered_shaders[i],
							ordered_shaders[i - 1]);
		}

		for (int i = 0; i < shader_count; ++i)  {
			nir_variable_mode mask = 0;

			if (ordered_shaders[i]->info.stage != first)
				mask = mask | nir_var_shader_in;

			if (ordered_shaders[i]->info.stage != last)
				mask = mask | nir_var_shader_out;

			if (nir_lower_io_to_scalar_early(ordered_shaders[i], mask)) {
				/* Optimize the new vector code and then remove dead vars */
				nir_copy_prop(ordered_shaders[i]);
				nir_opt_shrink_vectors(ordered_shaders[i]);

		                if (ordered_shaders[i]->info.stage != last) {
					/* Optimize swizzled movs of load_const for
					 * nir_link_opt_varyings's constant propagation
					 */
				        nir_opt_constant_folding(ordered_shaders[i]);
				        /* For nir_link_opt_varyings's duplicate input opt */
				        nir_opt_cse(ordered_shaders[i]);
				}

				/* Run copy-propagation to help remove dead
				 * output variables (some shaders have useless
				 * copies to/from an output), so compaction
				 * later will be more effective.
				 *
				 * This will have been done earlier but it might
				 * not have worked because the outputs were vector.
				 */
				if (ordered_shaders[i]->info.stage == MESA_SHADER_TESS_CTRL)
					nir_opt_copy_prop_vars(ordered_shaders[i]);

				nir_opt_dce(ordered_shaders[i]);
				nir_remove_dead_variables(ordered_shaders[i],
							  nir_var_function_temp | nir_var_shader_in | nir_var_shader_out, NULL);
			}
		}
	}

	for (int i = 1; !optimize_conservatively && (i < shader_count); ++i)  {
		if (nir_link_opt_varyings(ordered_shaders[i], ordered_shaders[i - 1])) {
			nir_opt_constant_folding(ordered_shaders[i - 1]);
			nir_opt_algebraic(ordered_shaders[i - 1]);
			nir_opt_dce(ordered_shaders[i - 1]);
		}

		nir_remove_dead_variables(ordered_shaders[i],
					  nir_var_shader_out, NULL);
		nir_remove_dead_variables(ordered_shaders[i - 1],
					  nir_var_shader_in, NULL);

		bool progress = nir_remove_unused_varyings(ordered_shaders[i],
							   ordered_shaders[i - 1]);

		nir_compact_varyings(ordered_shaders[i],
				     ordered_shaders[i - 1], true);

		if (progress) {
			if (nir_lower_global_vars_to_local(ordered_shaders[i])) {
				ac_lower_indirect_derefs(ordered_shaders[i],
				                         pipeline->device->physical_device->rad_info.chip_class);
				/* remove dead writes, which can remove input loads */
				nir_lower_vars_to_ssa(ordered_shaders[i]);
				nir_opt_dce(ordered_shaders[i]);
			}

			if (nir_lower_global_vars_to_local(ordered_shaders[i - 1])) {
				ac_lower_indirect_derefs(ordered_shaders[i - 1],
				                         pipeline->device->physical_device->rad_info.chip_class);
			}
		}
	}
}

static void
radv_set_driver_locations(struct radv_pipeline *pipeline, nir_shader **shaders,
                              struct radv_shader_info infos[MESA_SHADER_STAGES])
{
	if (shaders[MESA_SHADER_FRAGMENT]) {
		nir_foreach_shader_out_variable(var, shaders[MESA_SHADER_FRAGMENT])
		{
			var->data.driver_location = var->data.location + var->data.index;
		}
	}

	if (!shaders[MESA_SHADER_VERTEX])
		return;

	bool has_tess = shaders[MESA_SHADER_TESS_CTRL];
	bool has_gs = shaders[MESA_SHADER_GEOMETRY];
	unsigned vs_info_idx = MESA_SHADER_VERTEX;
	unsigned tes_info_idx = MESA_SHADER_TESS_EVAL;
	unsigned last_vtg_stage = MESA_SHADER_VERTEX;

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9) {
		/* These are merged into the next stage */
		vs_info_idx = has_tess ? MESA_SHADER_TESS_CTRL : MESA_SHADER_GEOMETRY;
		tes_info_idx = has_gs ? MESA_SHADER_GEOMETRY : MESA_SHADER_TESS_EVAL;
	}

	nir_foreach_shader_in_variable(var, shaders[MESA_SHADER_VERTEX]) {
		var->data.driver_location = var->data.location;
	}

	if (has_tess) {
		nir_linked_io_var_info vs2tcs =
			nir_assign_linked_io_var_locations(shaders[MESA_SHADER_VERTEX], shaders[MESA_SHADER_TESS_CTRL]);
		nir_linked_io_var_info tcs2tes =
			nir_assign_linked_io_var_locations(shaders[MESA_SHADER_TESS_CTRL], shaders[MESA_SHADER_TESS_EVAL]);

		infos[vs_info_idx].vs.num_linked_outputs = vs2tcs.num_linked_io_vars;
		infos[MESA_SHADER_TESS_CTRL].tcs.num_linked_inputs = vs2tcs.num_linked_io_vars;
		infos[MESA_SHADER_TESS_CTRL].tcs.num_linked_outputs = tcs2tes.num_linked_io_vars;
		infos[MESA_SHADER_TESS_CTRL].tcs.num_linked_patch_outputs = tcs2tes.num_linked_patch_io_vars;
		infos[tes_info_idx].tes.num_linked_inputs = tcs2tes.num_linked_io_vars;
		infos[tes_info_idx].tes.num_linked_patch_inputs = tcs2tes.num_linked_patch_io_vars;

		if (has_gs) {
			nir_linked_io_var_info tes2gs =
				nir_assign_linked_io_var_locations(shaders[MESA_SHADER_TESS_EVAL], shaders[MESA_SHADER_GEOMETRY]);

			infos[tes_info_idx].tes.num_linked_outputs = tes2gs.num_linked_io_vars;
			infos[MESA_SHADER_GEOMETRY].gs.num_linked_inputs = tes2gs.num_linked_io_vars;
			last_vtg_stage = MESA_SHADER_GEOMETRY;
		} else {
			last_vtg_stage = MESA_SHADER_TESS_EVAL;
		}
	} else if (has_gs) {
		nir_linked_io_var_info vs2gs =
			nir_assign_linked_io_var_locations(shaders[MESA_SHADER_VERTEX], shaders[MESA_SHADER_GEOMETRY]);

		infos[vs_info_idx].vs.num_linked_outputs = vs2gs.num_linked_io_vars;
		infos[MESA_SHADER_GEOMETRY].gs.num_linked_inputs = vs2gs.num_linked_io_vars;
		last_vtg_stage = MESA_SHADER_GEOMETRY;
	}

	nir_foreach_shader_out_variable(var, shaders[last_vtg_stage]) {
		var->data.driver_location = var->data.location;
	}
}

static uint32_t
radv_get_attrib_stride(const VkPipelineVertexInputStateCreateInfo *input_state,
		       uint32_t attrib_binding)
{
	for (uint32_t i = 0; i < input_state->vertexBindingDescriptionCount; i++) {
		const VkVertexInputBindingDescription *input_binding =
			&input_state->pVertexBindingDescriptions[i];

		if (input_binding->binding == attrib_binding)
			return input_binding->stride;
	}

	return 0;
}

static struct radv_pipeline_key
radv_generate_graphics_pipeline_key(const struct radv_pipeline *pipeline,
                                    const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                    const struct radv_blend_state *blend)
{
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	const VkPipelineVertexInputStateCreateInfo *input_state =
	                                         pCreateInfo->pVertexInputState;
	const VkPipelineVertexInputDivisorStateCreateInfoEXT *divisor_state =
		vk_find_struct_const(input_state->pNext, PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT);
	bool uses_dynamic_stride = false;

	struct radv_pipeline_key key;
	memset(&key, 0, sizeof(key));

	if (pCreateInfo->flags & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT)
		key.optimisations_disabled = 1;

	key.has_multiview_view_index = !!subpass->view_mask;

	uint32_t binding_input_rate = 0;
	uint32_t instance_rate_divisors[MAX_VERTEX_ATTRIBS];
	for (unsigned i = 0; i < input_state->vertexBindingDescriptionCount; ++i) {
		if (input_state->pVertexBindingDescriptions[i].inputRate) {
			unsigned binding = input_state->pVertexBindingDescriptions[i].binding;
			binding_input_rate |= 1u << binding;
			instance_rate_divisors[binding] = 1;
		}
	}
	if (divisor_state) {
		for (unsigned i = 0; i < divisor_state->vertexBindingDivisorCount; ++i) {
			instance_rate_divisors[divisor_state->pVertexBindingDivisors[i].binding] =
				divisor_state->pVertexBindingDivisors[i].divisor;
		}
	}

	if (pCreateInfo->pDynamicState) {
		uint32_t count = pCreateInfo->pDynamicState->dynamicStateCount;
		for (uint32_t i = 0; i < count; i++) {
			if (pCreateInfo->pDynamicState->pDynamicStates[i] == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT) {
				uses_dynamic_stride = true;
				break;
			}
		}
	}

	for (unsigned i = 0; i < input_state->vertexAttributeDescriptionCount; ++i) {
		const VkVertexInputAttributeDescription *desc =
			&input_state->pVertexAttributeDescriptions[i];
		const struct vk_format_description *format_desc;
		unsigned location = desc->location;
		unsigned binding = desc->binding;
		unsigned num_format, data_format;
		int first_non_void;

		if (binding_input_rate & (1u << binding)) {
			key.instance_rate_inputs |= 1u << location;
			key.instance_rate_divisors[location] = instance_rate_divisors[binding];
		}

		format_desc = vk_format_description(desc->format);
		first_non_void = vk_format_get_first_non_void_channel(desc->format);

		num_format = radv_translate_buffer_numformat(format_desc, first_non_void);
		data_format = radv_translate_buffer_dataformat(format_desc, first_non_void);

		key.vertex_attribute_formats[location] = data_format | (num_format << 4);
		key.vertex_attribute_bindings[location] = desc->binding;
		key.vertex_attribute_offsets[location] = desc->offset;

		if (!uses_dynamic_stride) {
			/* From the Vulkan spec 1.2.157:
			 *
			 * "If the bound pipeline state object was created
			 *  with the
			 *  VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT
			 *  dynamic state enabled then pStrides[i] specifies
			 *  the distance in bytes between two consecutive
			 *  elements within the corresponding buffer. In this
			 *  case the VkVertexInputBindingDescription::stride
			 *  state from the pipeline state object is ignored."
			 *
			 * Make sure the vertex attribute stride is zero to
			 * avoid computing a wrong offset if it's initialized
			 * to something else than zero.
			 */
			key.vertex_attribute_strides[location] =
				radv_get_attrib_stride(input_state, desc->binding);
		}

		enum ac_fetch_format adjust = AC_FETCH_FORMAT_NONE;
		if (pipeline->device->physical_device->rad_info.chip_class <= GFX8 &&
		    pipeline->device->physical_device->rad_info.family != CHIP_STONEY) {
			VkFormat format = input_state->pVertexAttributeDescriptions[i].format;
			switch(format) {
			case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
			case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
				adjust = AC_FETCH_FORMAT_SNORM;
				break;
			case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
			case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
				adjust = AC_FETCH_FORMAT_SSCALED;
				break;
			case VK_FORMAT_A2R10G10B10_SINT_PACK32:
			case VK_FORMAT_A2B10G10R10_SINT_PACK32:
				adjust = AC_FETCH_FORMAT_SINT;
				break;
			default:
				break;
			}
		}
		key.vertex_alpha_adjust[location] = adjust;

		switch (desc->format) {
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
		case VK_FORMAT_A2R10G10B10_UINT_PACK32:
		case VK_FORMAT_A2R10G10B10_SINT_PACK32:
			key.vertex_post_shuffle |= 1 << location;
			break;
		default:
			break;
		}
	}

	const VkPipelineTessellationStateCreateInfo *tess =
		radv_pipeline_get_tessellation_state(pCreateInfo);
	if (tess)
		key.tess_input_vertices = tess->patchControlPoints;

	const VkPipelineMultisampleStateCreateInfo *vkms =
		radv_pipeline_get_multisample_state(pCreateInfo);
	if (vkms && vkms->rasterizationSamples > 1) {
		uint32_t num_samples = vkms->rasterizationSamples;
		uint32_t ps_iter_samples = radv_pipeline_get_ps_iter_samples(pCreateInfo);
		key.num_samples = num_samples;
		key.log2_ps_iter_samples = util_logbase2(ps_iter_samples);
	}

	key.col_format = blend->spi_shader_col_format;
	key.is_dual_src = blend->mrt0_is_dual_src;
	if (pipeline->device->physical_device->rad_info.chip_class < GFX8) {
		key.is_int8 = blend->col_format_is_int8;
		key.is_int10 = blend->col_format_is_int10;
	}

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10)
		key.topology = pCreateInfo->pInputAssemblyState->topology;

	return key;
}

static bool
radv_nir_stage_uses_xfb(const nir_shader *nir)
{
	nir_xfb_info *xfb = nir_gather_xfb_info(nir, NULL);
	bool uses_xfb = !!xfb;

	ralloc_free(xfb);
	return uses_xfb;
}

static void
radv_fill_shader_keys(struct radv_device *device,
		      struct radv_shader_variant_key *keys,
                      const struct radv_pipeline_key *key,
                      nir_shader **nir)
{
	keys[MESA_SHADER_VERTEX].vs.instance_rate_inputs = key->instance_rate_inputs;
	keys[MESA_SHADER_VERTEX].vs.post_shuffle = key->vertex_post_shuffle;
	for (unsigned i = 0; i < MAX_VERTEX_ATTRIBS; ++i) {
		keys[MESA_SHADER_VERTEX].vs.instance_rate_divisors[i] = key->instance_rate_divisors[i];
		keys[MESA_SHADER_VERTEX].vs.vertex_attribute_formats[i] = key->vertex_attribute_formats[i];
		keys[MESA_SHADER_VERTEX].vs.vertex_attribute_bindings[i] = key->vertex_attribute_bindings[i];
		keys[MESA_SHADER_VERTEX].vs.vertex_attribute_offsets[i] = key->vertex_attribute_offsets[i];
		keys[MESA_SHADER_VERTEX].vs.vertex_attribute_strides[i] = key->vertex_attribute_strides[i];
		keys[MESA_SHADER_VERTEX].vs.alpha_adjust[i] = key->vertex_alpha_adjust[i];
	}
	keys[MESA_SHADER_VERTEX].vs.outprim = si_conv_prim_to_gs_out(key->topology);

	if (nir[MESA_SHADER_TESS_CTRL]) {
		keys[MESA_SHADER_VERTEX].vs_common_out.as_ls = true;
		keys[MESA_SHADER_TESS_CTRL].tcs.input_vertices = key->tess_input_vertices;
		keys[MESA_SHADER_TESS_CTRL].tcs.primitive_mode = nir[MESA_SHADER_TESS_EVAL]->info.tess.primitive_mode;

		keys[MESA_SHADER_TESS_CTRL].tcs.tes_reads_tess_factors = !!(nir[MESA_SHADER_TESS_EVAL]->info.inputs_read & (VARYING_BIT_TESS_LEVEL_INNER | VARYING_BIT_TESS_LEVEL_OUTER));
	}

	if (nir[MESA_SHADER_GEOMETRY]) {
		if (nir[MESA_SHADER_TESS_CTRL])
			keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_es = true;
		else
			keys[MESA_SHADER_VERTEX].vs_common_out.as_es = true;
	}

	if (device->physical_device->use_ngg) {
		if (nir[MESA_SHADER_TESS_CTRL]) {
			keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_ngg = true;
		} else {
			keys[MESA_SHADER_VERTEX].vs_common_out.as_ngg = true;
		}

		if (nir[MESA_SHADER_TESS_CTRL] &&
		    nir[MESA_SHADER_GEOMETRY] &&
		    nir[MESA_SHADER_GEOMETRY]->info.gs.invocations *
		    nir[MESA_SHADER_GEOMETRY]->info.gs.vertices_out > 256) {
			/* Fallback to the legacy path if tessellation is
			 * enabled with extreme geometry because
			 * EN_MAX_VERT_OUT_PER_GS_INSTANCE doesn't work and it
			 * might hang.
			 */
			keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_ngg = false;
		}

		gl_shader_stage last_xfb_stage = MESA_SHADER_VERTEX;

		for (int i = MESA_SHADER_VERTEX; i <= MESA_SHADER_GEOMETRY; i++) {
			if (nir[i])
				last_xfb_stage = i;
		}

		bool uses_xfb = nir[last_xfb_stage] &&
				radv_nir_stage_uses_xfb(nir[last_xfb_stage]);

		if (!device->physical_device->use_ngg_streamout && uses_xfb) {
			if (nir[MESA_SHADER_TESS_CTRL])
				keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_ngg = false;
			else
				keys[MESA_SHADER_VERTEX].vs_common_out.as_ngg = false;
		}

		/* Determine if the pipeline is eligible for the NGG passthrough
		 * mode. It can't be enabled for geometry shaders, for NGG
		 * streamout or for vertex shaders that export the primitive ID
		 * (this is checked later because we don't have the info here.)
		 */
		if (!nir[MESA_SHADER_GEOMETRY] && !uses_xfb) {
			if (nir[MESA_SHADER_TESS_CTRL] &&
			    keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_ngg) {
				keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_ngg_passthrough = true;
			} else if (nir[MESA_SHADER_VERTEX] &&
				   keys[MESA_SHADER_VERTEX].vs_common_out.as_ngg) {
				keys[MESA_SHADER_VERTEX].vs_common_out.as_ngg_passthrough = true;
			}
		}
	}

	for(int i = 0; i < MESA_SHADER_STAGES; ++i)
		keys[i].has_multiview_view_index = key->has_multiview_view_index;

	keys[MESA_SHADER_FRAGMENT].fs.col_format = key->col_format;
	keys[MESA_SHADER_FRAGMENT].fs.is_int8 = key->is_int8;
	keys[MESA_SHADER_FRAGMENT].fs.is_int10 = key->is_int10;
	keys[MESA_SHADER_FRAGMENT].fs.log2_ps_iter_samples = key->log2_ps_iter_samples;
	keys[MESA_SHADER_FRAGMENT].fs.num_samples = key->num_samples;
	keys[MESA_SHADER_FRAGMENT].fs.is_dual_src = key->is_dual_src;

	if (nir[MESA_SHADER_COMPUTE]) {
		keys[MESA_SHADER_COMPUTE].cs.subgroup_size = key->compute_subgroup_size;
	}
}

static uint8_t
radv_get_wave_size(struct radv_device *device,
		   const VkPipelineShaderStageCreateInfo *pStage,
		   gl_shader_stage stage,
		   const struct radv_shader_variant_key *key)
{
	if (stage == MESA_SHADER_GEOMETRY && !key->vs_common_out.as_ngg)
		return 64;
	else if (stage == MESA_SHADER_COMPUTE) {
		if (key->cs.subgroup_size) {
			/* Return the required subgroup size if specified. */
			return key->cs.subgroup_size;
		}
		return device->physical_device->cs_wave_size;
	}
	else if (stage == MESA_SHADER_FRAGMENT)
		return device->physical_device->ps_wave_size;
	else
		return device->physical_device->ge_wave_size;
}

static uint8_t
radv_get_ballot_bit_size(struct radv_device *device,
			 const VkPipelineShaderStageCreateInfo *pStage,
			 gl_shader_stage stage,
			 const struct radv_shader_variant_key *key)
{
	if (stage == MESA_SHADER_COMPUTE && key->cs.subgroup_size)
		return key->cs.subgroup_size;
	return 64;
}

static void
radv_fill_shader_info(struct radv_pipeline *pipeline,
		      const VkPipelineShaderStageCreateInfo **pStages,
		      struct radv_shader_variant_key *keys,
                      struct radv_shader_info *infos,
                      nir_shader **nir)
{
	unsigned active_stages = 0;
	unsigned filled_stages = 0;

	for (int i = 0; i < MESA_SHADER_STAGES; i++) {
		if (nir[i])
			active_stages |= (1 << i);
	}

	if (nir[MESA_SHADER_FRAGMENT]) {
		radv_nir_shader_info_init(&infos[MESA_SHADER_FRAGMENT]);
		radv_nir_shader_info_pass(nir[MESA_SHADER_FRAGMENT],
					  pipeline->layout,
					  &keys[MESA_SHADER_FRAGMENT],
					  &infos[MESA_SHADER_FRAGMENT]);

		/* TODO: These are no longer used as keys we should refactor this */
		keys[MESA_SHADER_VERTEX].vs_common_out.export_prim_id =
		        infos[MESA_SHADER_FRAGMENT].ps.prim_id_input;
		keys[MESA_SHADER_VERTEX].vs_common_out.export_layer_id =
		        infos[MESA_SHADER_FRAGMENT].ps.layer_input;
		keys[MESA_SHADER_VERTEX].vs_common_out.export_clip_dists =
		        !!infos[MESA_SHADER_FRAGMENT].ps.num_input_clips_culls;
		keys[MESA_SHADER_VERTEX].vs_common_out.export_viewport_index =
		        infos[MESA_SHADER_FRAGMENT].ps.viewport_index_input;
		keys[MESA_SHADER_TESS_EVAL].vs_common_out.export_prim_id =
		        infos[MESA_SHADER_FRAGMENT].ps.prim_id_input;
		keys[MESA_SHADER_TESS_EVAL].vs_common_out.export_layer_id =
		        infos[MESA_SHADER_FRAGMENT].ps.layer_input;
		keys[MESA_SHADER_TESS_EVAL].vs_common_out.export_clip_dists =
		        !!infos[MESA_SHADER_FRAGMENT].ps.num_input_clips_culls;
		keys[MESA_SHADER_TESS_EVAL].vs_common_out.export_viewport_index =
		        infos[MESA_SHADER_FRAGMENT].ps.viewport_index_input;

		/* NGG passthrough mode can't be enabled for vertex shaders
		 * that export the primitive ID.
		 *
		 * TODO: I should really refactor the keys logic.
		 */
		if (nir[MESA_SHADER_VERTEX] &&
		    keys[MESA_SHADER_VERTEX].vs_common_out.export_prim_id) {
			keys[MESA_SHADER_VERTEX].vs_common_out.as_ngg_passthrough = false;
		}

		filled_stages |= (1 << MESA_SHADER_FRAGMENT);
	}

	if (nir[MESA_SHADER_TESS_CTRL]) {
		infos[MESA_SHADER_TESS_CTRL].tcs.tes_inputs_read =
			nir[MESA_SHADER_TESS_EVAL]->info.inputs_read;
		infos[MESA_SHADER_TESS_CTRL].tcs.tes_patch_inputs_read =
			nir[MESA_SHADER_TESS_EVAL]->info.patch_inputs_read;
	}

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9 &&
	    nir[MESA_SHADER_TESS_CTRL]) {
		struct nir_shader *combined_nir[] = {nir[MESA_SHADER_VERTEX], nir[MESA_SHADER_TESS_CTRL]};
		struct radv_shader_variant_key key = keys[MESA_SHADER_TESS_CTRL];
		key.tcs.vs_key = keys[MESA_SHADER_VERTEX].vs;

		radv_nir_shader_info_init(&infos[MESA_SHADER_TESS_CTRL]);

		for (int i = 0; i < 2; i++) {
			radv_nir_shader_info_pass(combined_nir[i],
						  pipeline->layout, &key,
						  &infos[MESA_SHADER_TESS_CTRL]);
		}

		keys[MESA_SHADER_TESS_EVAL].tes.num_patches =
			infos[MESA_SHADER_TESS_CTRL].tcs.num_patches;

		filled_stages |= (1 << MESA_SHADER_VERTEX);
		filled_stages |= (1 << MESA_SHADER_TESS_CTRL);
	}

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9 &&
	    nir[MESA_SHADER_GEOMETRY]) {
		gl_shader_stage pre_stage = nir[MESA_SHADER_TESS_EVAL] ? MESA_SHADER_TESS_EVAL : MESA_SHADER_VERTEX;
		struct nir_shader *combined_nir[] = {nir[pre_stage], nir[MESA_SHADER_GEOMETRY]};

		radv_nir_shader_info_init(&infos[MESA_SHADER_GEOMETRY]);

		for (int i = 0; i < 2; i++) {
			radv_nir_shader_info_pass(combined_nir[i],
						  pipeline->layout,
						  &keys[pre_stage],
						  &infos[MESA_SHADER_GEOMETRY]);
		}

		filled_stages |= (1 << pre_stage);
		filled_stages |= (1 << MESA_SHADER_GEOMETRY);
	}

	active_stages ^= filled_stages;
	while (active_stages) {
		int i = u_bit_scan(&active_stages);

		if (i == MESA_SHADER_TESS_EVAL) {
			keys[MESA_SHADER_TESS_EVAL].tes.num_patches =
				infos[MESA_SHADER_TESS_CTRL].tcs.num_patches;
		}

		radv_nir_shader_info_init(&infos[i]);
		radv_nir_shader_info_pass(nir[i], pipeline->layout,
					  &keys[i], &infos[i]);
	}

	for (int i = 0; i < MESA_SHADER_STAGES; i++) {
		if (nir[i]) {
			infos[i].wave_size =
				radv_get_wave_size(pipeline->device, pStages[i],
						   i, &keys[i]);
			infos[i].ballot_bit_size =
				radv_get_ballot_bit_size(pipeline->device,
							 pStages[i], i,
							 &keys[i]);
		}
	}
}

static void
merge_tess_info(struct shader_info *tes_info,
                const struct shader_info *tcs_info)
{
	/* The Vulkan 1.0.38 spec, section 21.1 Tessellator says:
	 *
	 *    "PointMode. Controls generation of points rather than triangles
	 *     or lines. This functionality defaults to disabled, and is
	 *     enabled if either shader stage includes the execution mode.
	 *
	 * and about Triangles, Quads, IsoLines, VertexOrderCw, VertexOrderCcw,
	 * PointMode, SpacingEqual, SpacingFractionalEven, SpacingFractionalOdd,
	 * and OutputVertices, it says:
	 *
	 *    "One mode must be set in at least one of the tessellation
	 *     shader stages."
	 *
	 * So, the fields can be set in either the TCS or TES, but they must
	 * agree if set in both.  Our backend looks at TES, so bitwise-or in
	 * the values from the TCS.
	 */
	assert(tcs_info->tess.tcs_vertices_out == 0 ||
	       tes_info->tess.tcs_vertices_out == 0 ||
	       tcs_info->tess.tcs_vertices_out == tes_info->tess.tcs_vertices_out);
	tes_info->tess.tcs_vertices_out |= tcs_info->tess.tcs_vertices_out;

	assert(tcs_info->tess.spacing == TESS_SPACING_UNSPECIFIED ||
	       tes_info->tess.spacing == TESS_SPACING_UNSPECIFIED ||
	       tcs_info->tess.spacing == tes_info->tess.spacing);
	tes_info->tess.spacing |= tcs_info->tess.spacing;

	assert(tcs_info->tess.primitive_mode == 0 ||
	       tes_info->tess.primitive_mode == 0 ||
	       tcs_info->tess.primitive_mode == tes_info->tess.primitive_mode);
	tes_info->tess.primitive_mode |= tcs_info->tess.primitive_mode;
	tes_info->tess.ccw |= tcs_info->tess.ccw;
	tes_info->tess.point_mode |= tcs_info->tess.point_mode;
}

static
void radv_init_feedback(const VkPipelineCreationFeedbackCreateInfoEXT *ext)
{
	if (!ext)
		return;

	if (ext->pPipelineCreationFeedback) {
		ext->pPipelineCreationFeedback->flags = 0;
		ext->pPipelineCreationFeedback->duration = 0;
	}

	for (unsigned i = 0; i < ext->pipelineStageCreationFeedbackCount; ++i) {
		ext->pPipelineStageCreationFeedbacks[i].flags = 0;
		ext->pPipelineStageCreationFeedbacks[i].duration = 0;
	}
}

static
void radv_start_feedback(VkPipelineCreationFeedbackEXT *feedback)
{
	if (!feedback)
		return;

	feedback->duration -= radv_get_current_time();
	feedback ->flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT;
}

static
void radv_stop_feedback(VkPipelineCreationFeedbackEXT *feedback, bool cache_hit)
{
	if (!feedback)
		return;

	feedback->duration += radv_get_current_time();
	feedback ->flags = VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT_EXT |
	                   (cache_hit ? VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT_EXT : 0);
}

static bool
mem_vectorize_callback(unsigned align_mul, unsigned align_offset,
                       unsigned bit_size,
                       unsigned num_components,
                       nir_intrinsic_instr *low, nir_intrinsic_instr *high)
{
	if (num_components > 4)
		return false;

	/* >128 bit loads are split except with SMEM */
	if (bit_size * num_components > 128)
		return false;

	uint32_t align;
	if (align_offset)
		align = 1 << (ffs(align_offset) - 1);
	else
		align = align_mul;

	switch (low->intrinsic) {
	case nir_intrinsic_load_global:
	case nir_intrinsic_store_global:
	case nir_intrinsic_store_ssbo:
	case nir_intrinsic_load_ssbo:
	case nir_intrinsic_load_ubo:
	case nir_intrinsic_load_push_constant:
		return align % (bit_size == 8 ? 2 : 4) == 0;
	case nir_intrinsic_load_deref:
	case nir_intrinsic_store_deref:
		assert(nir_deref_mode_is(nir_src_as_deref(low->src[0]),
		                         nir_var_mem_shared));
		/* fallthrough */
	case nir_intrinsic_load_shared:
	case nir_intrinsic_store_shared:
		if (bit_size * num_components > 64) /* 96 and 128 bit loads require 128 bit alignment and are split otherwise */
			return align % 16 == 0;
		else
			return align % (bit_size == 8 ? 2 : 4) == 0;
	default:
		return false;
	}
	return false;
}

static unsigned
lower_bit_size_callback(const nir_instr *instr, void *_)
{
	struct radv_device *device = _;
	enum chip_class chip = device->physical_device->rad_info.chip_class;

	if (instr->type != nir_instr_type_alu)
		return 0;
	nir_alu_instr *alu = nir_instr_as_alu(instr);

	if (alu->dest.dest.ssa.bit_size & (8 | 16)) {
		unsigned bit_size = alu->dest.dest.ssa.bit_size;
		switch (alu->op) {
		case nir_op_iabs:
		case nir_op_bitfield_select:
		case nir_op_udiv:
		case nir_op_idiv:
		case nir_op_umod:
		case nir_op_imod:
		case nir_op_imul_high:
		case nir_op_umul_high:
		case nir_op_ineg:
		case nir_op_irem:
		case nir_op_isign:
			return 32;
		case nir_op_imax:
		case nir_op_umax:
		case nir_op_imin:
		case nir_op_umin:
		case nir_op_ishr:
		case nir_op_ushr:
		case nir_op_ishl:
		case nir_op_uadd_sat:
			return (bit_size == 8 ||
			        !(chip >= GFX8 && nir_dest_is_divergent(alu->dest.dest))) ? 32 : 0;
		default:
			return 0;
		}
	}

	if (nir_src_bit_size(alu->src[0].src) & (8 | 16)) {
		unsigned bit_size = nir_src_bit_size(alu->src[0].src);
		switch (alu->op) {
		case nir_op_bit_count:
		case nir_op_find_lsb:
		case nir_op_ufind_msb:
		case nir_op_i2b1:
			return 32;
		case nir_op_ilt:
		case nir_op_ige:
		case nir_op_ieq:
		case nir_op_ine:
		case nir_op_ult:
		case nir_op_uge:
			return (bit_size == 8 ||
			        !(chip >= GFX8 && nir_dest_is_divergent(alu->dest.dest))) ? 32 : 0;
		default:
			return 0;
		}
	}

	return 0;
}

VkResult radv_create_shaders(struct radv_pipeline *pipeline,
                             struct radv_device *device,
                             struct radv_pipeline_cache *cache,
                             const struct radv_pipeline_key *key,
                             const VkPipelineShaderStageCreateInfo **pStages,
                             const VkPipelineCreateFlags flags,
                             VkPipelineCreationFeedbackEXT *pipeline_feedback,
                             VkPipelineCreationFeedbackEXT **stage_feedbacks)
{
	struct radv_shader_module fs_m = {0};
	struct radv_shader_module *modules[MESA_SHADER_STAGES] = { 0, };
	nir_shader *nir[MESA_SHADER_STAGES] = {0};
	struct radv_shader_binary *binaries[MESA_SHADER_STAGES] = {NULL};
	struct radv_shader_variant_key keys[MESA_SHADER_STAGES] = {{{{{0}}}}};
	struct radv_shader_info infos[MESA_SHADER_STAGES] = {0};
	unsigned char hash[20], gs_copy_hash[20];
	bool keep_executable_info = (flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) || device->keep_shader_info;
	bool keep_statistic_info = (flags & VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR) ||
	                           (device->instance->debug_flags & RADV_DEBUG_DUMP_SHADER_STATS) ||
	                           device->keep_shader_info;
	bool disable_optimizations = flags & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;

	radv_start_feedback(pipeline_feedback);

	for (unsigned i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (pStages[i]) {
			modules[i] = radv_shader_module_from_handle(pStages[i]->module);
			if (modules[i]->nir)
				_mesa_sha1_compute(modules[i]->nir->info.name,
				                   strlen(modules[i]->nir->info.name),
				                   modules[i]->sha1);

			pipeline->active_stages |= mesa_to_vk_shader_stage(i);
		}
	}

	radv_hash_shaders(hash, pStages, pipeline->layout, key, get_hash_flags(device));
	memcpy(gs_copy_hash, hash, 20);
	gs_copy_hash[0] ^= 1;

	bool found_in_application_cache = true;
	if (modules[MESA_SHADER_GEOMETRY] && !keep_executable_info && !keep_statistic_info) {
		struct radv_shader_variant *variants[MESA_SHADER_STAGES] = {0};
		radv_create_shader_variants_from_pipeline_cache(device, cache, gs_copy_hash, variants,
		                                                &found_in_application_cache);
		pipeline->gs_copy_shader = variants[MESA_SHADER_GEOMETRY];
	}

	if (!keep_executable_info && !keep_statistic_info &&
	    radv_create_shader_variants_from_pipeline_cache(device, cache, hash, pipeline->shaders,
	                                                    &found_in_application_cache) &&
	    (!modules[MESA_SHADER_GEOMETRY] || pipeline->gs_copy_shader)) {
		radv_stop_feedback(pipeline_feedback, found_in_application_cache);
		return VK_SUCCESS;
	}

	if (flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT) {
		radv_stop_feedback(pipeline_feedback, found_in_application_cache);
		return VK_PIPELINE_COMPILE_REQUIRED_EXT;
	}

	if (!modules[MESA_SHADER_FRAGMENT] && !modules[MESA_SHADER_COMPUTE]) {
		nir_builder fs_b;
		nir_builder_init_simple_shader(&fs_b, NULL, MESA_SHADER_FRAGMENT, NULL);
		fs_b.shader->info.name = ralloc_strdup(fs_b.shader, "noop_fs");
		fs_m.nir = fs_b.shader;
		modules[MESA_SHADER_FRAGMENT] = &fs_m;
	}

	for (unsigned i = 0; i < MESA_SHADER_STAGES; ++i) {
		const VkPipelineShaderStageCreateInfo *stage = pStages[i];
		unsigned subgroup_size = 64, ballot_bit_size = 64;

		if (!modules[i])
			continue;

		radv_start_feedback(stage_feedbacks[i]);

		if (key->compute_subgroup_size) {
			/* Only compute shaders currently support requiring a
			 * specific subgroup size.
                         */
			assert(i == MESA_SHADER_COMPUTE);
			subgroup_size = key->compute_subgroup_size;
			ballot_bit_size = key->compute_subgroup_size;
		}

		nir[i] = radv_shader_compile_to_nir(device, modules[i],
						    stage ? stage->pName : "main", i,
						    stage ? stage->pSpecializationInfo : NULL,
						    flags, pipeline->layout,
						    subgroup_size, ballot_bit_size);

		/* We don't want to alter meta shaders IR directly so clone it
		 * first.
		 */
		if (nir[i]->info.name) {
			nir[i] = nir_shader_clone(NULL, nir[i]);
		}

		radv_stop_feedback(stage_feedbacks[i], false);
	}

	if (nir[MESA_SHADER_TESS_CTRL]) {
		nir_lower_patch_vertices(nir[MESA_SHADER_TESS_EVAL], nir[MESA_SHADER_TESS_CTRL]->info.tess.tcs_vertices_out, NULL);
		merge_tess_info(&nir[MESA_SHADER_TESS_EVAL]->info, &nir[MESA_SHADER_TESS_CTRL]->info);
	}

	bool optimize_conservatively = flags & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;

	radv_link_shaders(pipeline, nir, optimize_conservatively);

	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (nir[i]) {
			radv_start_feedback(stage_feedbacks[i]);
			radv_optimize_nir(nir[i], optimize_conservatively, false);
			radv_stop_feedback(stage_feedbacks[i], false);
		}
	}

	radv_set_driver_locations(pipeline, nir, infos);

	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (nir[i]) {
			radv_start_feedback(stage_feedbacks[i]);

			if (!radv_use_llvm_for_stage(device, i)) {
				NIR_PASS_V(nir[i], nir_lower_non_uniform_access,
				           nir_lower_non_uniform_ubo_access |
				           nir_lower_non_uniform_ssbo_access |
				           nir_lower_non_uniform_texture_access |
				           nir_lower_non_uniform_image_access);
			}
			NIR_PASS_V(nir[i], nir_lower_memory_model);

			bool lower_to_scalar = false;
			bool lower_pack = false;
			nir_variable_mode robust_modes = (nir_variable_mode)0;

			if (device->robust_buffer_access) {
				robust_modes = nir_var_mem_ubo |
					       nir_var_mem_ssbo |
					       nir_var_mem_global |
					       nir_var_mem_push_const;
			}

			if (nir_opt_load_store_vectorize(nir[i],
							 nir_var_mem_ssbo | nir_var_mem_ubo |
							 nir_var_mem_push_const | nir_var_mem_shared |
							 nir_var_mem_global,
							 mem_vectorize_callback, robust_modes)) {
				lower_to_scalar = true;
				lower_pack = true;
			}

			/* do this again since information such as outputs_read can be out-of-date */
			nir_shader_gather_info(nir[i], nir_shader_get_entrypoint(nir[i]));

			radv_lower_io(device, nir[i]);

			lower_to_scalar |= nir_opt_shrink_vectors(nir[i]);

			if (lower_to_scalar)
				nir_lower_alu_to_scalar(nir[i], NULL, NULL);
			if (lower_pack)
				nir_lower_pack(nir[i]);

			/* lower ALU operations */
			/* TODO: Some 64-bit tests crash inside LLVM. */
			if (!radv_use_llvm_for_stage(device, i))
				nir_lower_int64(nir[i]);

			/* TODO: Implement nir_op_uadd_sat with LLVM. */
			if (!radv_use_llvm_for_stage(device, i))
				nir_opt_idiv_const(nir[i], 32);
			nir_lower_idiv(nir[i], nir_lower_idiv_precise);

			/* optimize the lowered ALU operations */
			bool more_algebraic = true;
			while (more_algebraic) {
				more_algebraic = false;
				NIR_PASS_V(nir[i], nir_copy_prop);
				NIR_PASS_V(nir[i], nir_opt_dce);
				NIR_PASS_V(nir[i], nir_opt_constant_folding);
				NIR_PASS(more_algebraic, nir[i], nir_opt_algebraic);
			}

			/* Do late algebraic optimization to turn add(a,
			 * neg(b)) back into subs, then the mandatory cleanup
			 * after algebraic.  Note that it may produce fnegs,
			 * and if so then we need to keep running to squash
			 * fneg(fneg(a)).
			 */
			bool more_late_algebraic = true;
			while (more_late_algebraic) {
				more_late_algebraic = false;
				NIR_PASS(more_late_algebraic, nir[i], nir_opt_algebraic_late);
				NIR_PASS_V(nir[i], nir_opt_constant_folding);
				NIR_PASS_V(nir[i], nir_copy_prop);
				NIR_PASS_V(nir[i], nir_opt_dce);
				NIR_PASS_V(nir[i], nir_opt_cse);
			}

			if (nir[i]->info.bit_sizes_int & (8 | 16)) {
				if (device->physical_device->rad_info.chip_class >= GFX8) {
					nir_convert_to_lcssa(nir[i], true, true);
					nir_divergence_analysis(nir[i]);
				}

				if (nir_lower_bit_size(nir[i], lower_bit_size_callback, device)) {
					nir_lower_idiv(nir[i], nir_lower_idiv_precise);
					nir_opt_constant_folding(nir[i]);
					nir_opt_dce(nir[i]);
				}

				if (device->physical_device->rad_info.chip_class >= GFX8)
					nir_opt_remove_phis(nir[i]); /* cleanup LCSSA phis */
			}

			/* cleanup passes */
			nir_lower_load_const_to_scalar(nir[i]);
			nir_move_options move_opts = (nir_move_options)(
				nir_move_const_undef | nir_move_load_ubo | nir_move_load_input |
				nir_move_comparisons | nir_move_copies);
			nir_opt_sink(nir[i], move_opts);
			nir_opt_move(nir[i], move_opts);

			radv_stop_feedback(stage_feedbacks[i], false);
		}
	}

	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (radv_can_dump_shader(device, modules[i], false))
			nir_print_shader(nir[i], stderr);
	}

	radv_fill_shader_keys(device, keys, key, nir);

	radv_fill_shader_info(pipeline, pStages, keys, infos, nir);

	if ((nir[MESA_SHADER_VERTEX] &&
	     keys[MESA_SHADER_VERTEX].vs_common_out.as_ngg) ||
	    (nir[MESA_SHADER_TESS_EVAL] &&
	     keys[MESA_SHADER_TESS_EVAL].vs_common_out.as_ngg)) {
		struct gfx10_ngg_info *ngg_info;

		if (nir[MESA_SHADER_GEOMETRY])
			ngg_info = &infos[MESA_SHADER_GEOMETRY].ngg_info;
		else if (nir[MESA_SHADER_TESS_CTRL])
			ngg_info = &infos[MESA_SHADER_TESS_EVAL].ngg_info;
		else
			ngg_info = &infos[MESA_SHADER_VERTEX].ngg_info;

		gfx10_get_ngg_info(key, pipeline, nir, infos, ngg_info);
	} else if (nir[MESA_SHADER_GEOMETRY]) {
		struct gfx9_gs_info *gs_info =
			&infos[MESA_SHADER_GEOMETRY].gs_ring_info;

		gfx9_get_gs_info(key, pipeline, nir, infos, gs_info);
	}

	if(modules[MESA_SHADER_GEOMETRY]) {
		struct radv_shader_binary *gs_copy_binary = NULL;
		if (!pipeline->gs_copy_shader &&
		    !radv_pipeline_has_ngg(pipeline)) {
			struct radv_shader_info info = {0};
			struct radv_shader_variant_key key = {0};

			key.has_multiview_view_index =
				keys[MESA_SHADER_GEOMETRY].has_multiview_view_index;

			radv_nir_shader_info_pass(nir[MESA_SHADER_GEOMETRY],
						  pipeline->layout, &key,
						  &info);
			info.wave_size = 64; /* Wave32 not supported. */
			info.ballot_bit_size = 64;

			pipeline->gs_copy_shader = radv_create_gs_copy_shader(
					device, nir[MESA_SHADER_GEOMETRY], &info,
					&gs_copy_binary, keep_executable_info, keep_statistic_info,
					keys[MESA_SHADER_GEOMETRY].has_multiview_view_index,
					disable_optimizations);
		}

		if (!keep_executable_info && !keep_statistic_info && pipeline->gs_copy_shader) {
			struct radv_shader_binary *binaries[MESA_SHADER_STAGES] = {NULL};
			struct radv_shader_variant *variants[MESA_SHADER_STAGES] = {0};

			binaries[MESA_SHADER_GEOMETRY] = gs_copy_binary;
			variants[MESA_SHADER_GEOMETRY] = pipeline->gs_copy_shader;

			radv_pipeline_cache_insert_shaders(device, cache,
							   gs_copy_hash,
							   variants,
							   binaries);
		}
		free(gs_copy_binary);
	}

	if (nir[MESA_SHADER_FRAGMENT]) {
		if (!pipeline->shaders[MESA_SHADER_FRAGMENT]) {
			radv_start_feedback(stage_feedbacks[MESA_SHADER_FRAGMENT]);

			pipeline->shaders[MESA_SHADER_FRAGMENT] =
			       radv_shader_variant_compile(device, modules[MESA_SHADER_FRAGMENT], &nir[MESA_SHADER_FRAGMENT], 1,
			                                  pipeline->layout, keys + MESA_SHADER_FRAGMENT,
							  infos + MESA_SHADER_FRAGMENT,
			                                  keep_executable_info, keep_statistic_info,
							  disable_optimizations,
							  &binaries[MESA_SHADER_FRAGMENT]);

			radv_stop_feedback(stage_feedbacks[MESA_SHADER_FRAGMENT], false);
		}
	}

	if (device->physical_device->rad_info.chip_class >= GFX9 && modules[MESA_SHADER_TESS_CTRL]) {
		if (!pipeline->shaders[MESA_SHADER_TESS_CTRL]) {
			struct nir_shader *combined_nir[] = {nir[MESA_SHADER_VERTEX], nir[MESA_SHADER_TESS_CTRL]};
			struct radv_shader_variant_key key = keys[MESA_SHADER_TESS_CTRL];
			key.tcs.vs_key = keys[MESA_SHADER_VERTEX].vs;

			radv_start_feedback(stage_feedbacks[MESA_SHADER_TESS_CTRL]);

			pipeline->shaders[MESA_SHADER_TESS_CTRL] = radv_shader_variant_compile(device, modules[MESA_SHADER_TESS_CTRL], combined_nir, 2,
			                                                                      pipeline->layout,
			                                                                      &key, &infos[MESA_SHADER_TESS_CTRL], keep_executable_info,
			                                                                      keep_statistic_info,
											      disable_optimizations,
											      &binaries[MESA_SHADER_TESS_CTRL]);

			radv_stop_feedback(stage_feedbacks[MESA_SHADER_TESS_CTRL], false);
		}
		modules[MESA_SHADER_VERTEX] = NULL;
		keys[MESA_SHADER_TESS_EVAL].tes.num_patches = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.num_patches;
	}

	if (device->physical_device->rad_info.chip_class >= GFX9 && modules[MESA_SHADER_GEOMETRY]) {
		gl_shader_stage pre_stage = modules[MESA_SHADER_TESS_EVAL] ? MESA_SHADER_TESS_EVAL : MESA_SHADER_VERTEX;
		if (!pipeline->shaders[MESA_SHADER_GEOMETRY]) {
			struct nir_shader *combined_nir[] = {nir[pre_stage], nir[MESA_SHADER_GEOMETRY]};

			radv_start_feedback(stage_feedbacks[MESA_SHADER_GEOMETRY]);

			pipeline->shaders[MESA_SHADER_GEOMETRY] = radv_shader_variant_compile(device, modules[MESA_SHADER_GEOMETRY], combined_nir, 2,
			                                                                     pipeline->layout,
			                                                                     &keys[pre_stage], &infos[MESA_SHADER_GEOMETRY], keep_executable_info,
			                                                                     keep_statistic_info,
											     disable_optimizations,
											     &binaries[MESA_SHADER_GEOMETRY]);

			radv_stop_feedback(stage_feedbacks[MESA_SHADER_GEOMETRY], false);
		}
		modules[pre_stage] = NULL;
	}

	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if(modules[i] && !pipeline->shaders[i]) {
			if (i == MESA_SHADER_TESS_EVAL) {
				keys[MESA_SHADER_TESS_EVAL].tes.num_patches = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.num_patches;
			}

			radv_start_feedback(stage_feedbacks[i]);

			pipeline->shaders[i] = radv_shader_variant_compile(device, modules[i], &nir[i], 1,
									  pipeline->layout,
									  keys + i, infos + i, keep_executable_info,
									  keep_statistic_info,
									  disable_optimizations,
									  &binaries[i]);

			radv_stop_feedback(stage_feedbacks[i], false);
		}
	}

	if (!keep_executable_info && !keep_statistic_info) {
		radv_pipeline_cache_insert_shaders(device, cache, hash, pipeline->shaders,
						   binaries);
	}

	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		free(binaries[i]);
		if (nir[i]) {
			ralloc_free(nir[i]);

			if (radv_can_dump_shader_stats(device, modules[i])) {
				radv_dump_shader_stats(device, pipeline, i, stderr);
			}
		}
	}

	if (fs_m.nir)
		ralloc_free(fs_m.nir);

	radv_stop_feedback(pipeline_feedback, false);
	return VK_SUCCESS;
}

static uint32_t
radv_pipeline_stage_to_user_data_0(struct radv_pipeline *pipeline,
				   gl_shader_stage stage, enum chip_class chip_class)
{
	bool has_gs = radv_pipeline_has_gs(pipeline);
	bool has_tess = radv_pipeline_has_tess(pipeline);
	bool has_ngg = radv_pipeline_has_ngg(pipeline);

	switch (stage) {
	case MESA_SHADER_FRAGMENT:
		return R_00B030_SPI_SHADER_USER_DATA_PS_0;
	case MESA_SHADER_VERTEX:
		if (has_tess) {
			if (chip_class >= GFX10) {
				return R_00B430_SPI_SHADER_USER_DATA_HS_0;
			} else if (chip_class == GFX9) {
				return R_00B430_SPI_SHADER_USER_DATA_LS_0;
			} else {
				return R_00B530_SPI_SHADER_USER_DATA_LS_0;
			}

		}

		if (has_gs) {
			if (chip_class >= GFX10) {
				return R_00B230_SPI_SHADER_USER_DATA_GS_0;
			} else {
				return R_00B330_SPI_SHADER_USER_DATA_ES_0;
			}
		}

		if (has_ngg)
			return R_00B230_SPI_SHADER_USER_DATA_GS_0;

		return R_00B130_SPI_SHADER_USER_DATA_VS_0;
	case MESA_SHADER_GEOMETRY:
		return chip_class == GFX9 ? R_00B330_SPI_SHADER_USER_DATA_ES_0 :
		                            R_00B230_SPI_SHADER_USER_DATA_GS_0;
	case MESA_SHADER_COMPUTE:
		return R_00B900_COMPUTE_USER_DATA_0;
	case MESA_SHADER_TESS_CTRL:
		return chip_class == GFX9 ? R_00B430_SPI_SHADER_USER_DATA_LS_0 :
		                            R_00B430_SPI_SHADER_USER_DATA_HS_0;
	case MESA_SHADER_TESS_EVAL:
		if (has_gs) {
			return chip_class >= GFX10 ? R_00B230_SPI_SHADER_USER_DATA_GS_0 :
						     R_00B330_SPI_SHADER_USER_DATA_ES_0;
		} else if (has_ngg) {
			return R_00B230_SPI_SHADER_USER_DATA_GS_0;
		} else {
			return R_00B130_SPI_SHADER_USER_DATA_VS_0;
		}
	default:
		unreachable("unknown shader");
	}
}

struct radv_bin_size_entry {
	unsigned bpp;
	VkExtent2D extent;
};

static VkExtent2D
radv_gfx9_compute_bin_size(const struct radv_pipeline *pipeline, const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	static const struct radv_bin_size_entry color_size_table[][3][9] = {
		{
			/* One RB / SE */
			{
				/* One shader engine */
				{        0, {128,  128}},
				{        1, { 64,  128}},
				{        2, { 32,  128}},
				{        3, { 16,  128}},
				{       17, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				/* Two shader engines */
				{        0, {128,  128}},
				{        2, { 64,  128}},
				{        3, { 32,  128}},
				{        5, { 16,  128}},
				{       17, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				/* Four shader engines */
				{        0, {128,  128}},
				{        3, { 64,  128}},
				{        5, { 16,  128}},
				{       17, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
		},
		{
			/* Two RB / SE */
			{
				/* One shader engine */
				{        0, {128,  128}},
				{        2, { 64,  128}},
				{        3, { 32,  128}},
				{        5, { 16,  128}},
				{       33, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				/* Two shader engines */
				{        0, {128,  128}},
				{        3, { 64,  128}},
				{        5, { 32,  128}},
				{        9, { 16,  128}},
				{       33, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				/* Four shader engines */
				{        0, {256,  256}},
				{        2, {128,  256}},
				{        3, {128,  128}},
				{        5, { 64,  128}},
				{        9, { 16,  128}},
				{       33, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
		},
		{
			/* Four RB / SE */
			{
				/* One shader engine */
				{        0, {128,  256}},
				{        2, {128,  128}},
				{        3, { 64,  128}},
				{        5, { 32,  128}},
				{        9, { 16,  128}},
				{       33, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				/* Two shader engines */
				{        0, {256,  256}},
				{        2, {128,  256}},
				{        3, {128,  128}},
				{        5, { 64,  128}},
				{        9, { 32,  128}},
				{       17, { 16,  128}},
				{       33, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				/* Four shader engines */
				{        0, {256,  512}},
				{        2, {256,  256}},
				{        3, {128,  256}},
				{        5, {128,  128}},
				{        9, { 64,  128}},
				{       17, { 16,  128}},
				{       33, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
		},
	};
	static const struct radv_bin_size_entry ds_size_table[][3][9] = {
		{
			// One RB / SE
			{
				// One shader engine
				{        0, {128,  256}},
				{        2, {128,  128}},
				{        4, { 64,  128}},
				{        7, { 32,  128}},
				{       13, { 16,  128}},
				{       49, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				// Two shader engines
				{        0, {256,  256}},
				{        2, {128,  256}},
				{        4, {128,  128}},
				{        7, { 64,  128}},
				{       13, { 32,  128}},
				{       25, { 16,  128}},
				{       49, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				// Four shader engines
				{        0, {256,  512}},
				{        2, {256,  256}},
				{        4, {128,  256}},
				{        7, {128,  128}},
				{       13, { 64,  128}},
				{       25, { 16,  128}},
				{       49, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
		},
		{
			// Two RB / SE
			{
				// One shader engine
				{        0, {256,  256}},
				{        2, {128,  256}},
				{        4, {128,  128}},
				{        7, { 64,  128}},
				{       13, { 32,  128}},
				{       25, { 16,  128}},
				{       97, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				// Two shader engines
				{        0, {256,  512}},
				{        2, {256,  256}},
				{        4, {128,  256}},
				{        7, {128,  128}},
				{       13, { 64,  128}},
				{       25, { 32,  128}},
				{       49, { 16,  128}},
				{       97, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				// Four shader engines
				{        0, {512,  512}},
				{        2, {256,  512}},
				{        4, {256,  256}},
				{        7, {128,  256}},
				{       13, {128,  128}},
				{       25, { 64,  128}},
				{       49, { 16,  128}},
				{       97, {  0,    0}},
				{ UINT_MAX, {  0,    0}},
			},
		},
		{
			// Four RB / SE
			{
				// One shader engine
				{        0, {256,  512}},
				{        2, {256,  256}},
				{        4, {128,  256}},
				{        7, {128,  128}},
				{       13, { 64,  128}},
				{       25, { 32,  128}},
				{       49, { 16,  128}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				// Two shader engines
				{        0, {512,  512}},
				{        2, {256,  512}},
				{        4, {256,  256}},
				{        7, {128,  256}},
				{       13, {128,  128}},
				{       25, { 64,  128}},
				{       49, { 32,  128}},
				{       97, { 16,  128}},
				{ UINT_MAX, {  0,    0}},
			},
			{
				// Four shader engines
				{        0, {512,  512}},
				{        4, {256,  512}},
				{        7, {256,  256}},
				{       13, {128,  256}},
				{       25, {128,  128}},
				{       49, { 64,  128}},
				{       97, { 16,  128}},
				{ UINT_MAX, {  0,    0}},
			},
		},
	};

	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	VkExtent2D extent = {512, 512};

	unsigned log_num_rb_per_se =
	    util_logbase2_ceil(pipeline->device->physical_device->rad_info.num_render_backends /
	                       pipeline->device->physical_device->rad_info.max_se);
	unsigned log_num_se = util_logbase2_ceil(pipeline->device->physical_device->rad_info.max_se);

	unsigned total_samples = 1u << G_028BE0_MSAA_NUM_SAMPLES(pipeline->graphics.ms.pa_sc_aa_config);
	unsigned ps_iter_samples = 1u << G_028804_PS_ITER_SAMPLES(pipeline->graphics.ms.db_eqaa);
	unsigned effective_samples = total_samples;
	unsigned color_bytes_per_pixel = 0;

	const VkPipelineColorBlendStateCreateInfo *vkblend =
		radv_pipeline_get_color_blend_state(pCreateInfo);
	if (vkblend) {
		for (unsigned i = 0; i < subpass->color_count; i++) {
			if (!vkblend->pAttachments[i].colorWriteMask)
				continue;

			if (subpass->color_attachments[i].attachment == VK_ATTACHMENT_UNUSED)
				continue;

			VkFormat format = pass->attachments[subpass->color_attachments[i].attachment].format;
			color_bytes_per_pixel += vk_format_get_blocksize(format);
		}

		/* MSAA images typically don't use all samples all the time. */
		if (effective_samples >= 2 && ps_iter_samples <= 1)
			effective_samples = 2;
		color_bytes_per_pixel *= effective_samples;
	}

	const struct radv_bin_size_entry *color_entry = color_size_table[log_num_rb_per_se][log_num_se];
	while(color_entry[1].bpp <= color_bytes_per_pixel)
		++color_entry;

	extent = color_entry->extent;

	if (subpass->depth_stencil_attachment) {
		struct radv_render_pass_attachment *attachment = pass->attachments + subpass->depth_stencil_attachment->attachment;

		/* Coefficients taken from AMDVLK */
		unsigned depth_coeff = vk_format_is_depth(attachment->format) ? 5 : 0;
		unsigned stencil_coeff = vk_format_is_stencil(attachment->format) ? 1 : 0;
		unsigned ds_bytes_per_pixel = 4 * (depth_coeff + stencil_coeff) * total_samples;

		const struct radv_bin_size_entry *ds_entry = ds_size_table[log_num_rb_per_se][log_num_se];
		while(ds_entry[1].bpp <= ds_bytes_per_pixel)
			++ds_entry;

		if (ds_entry->extent.width * ds_entry->extent.height < extent.width * extent.height)
			extent = ds_entry->extent;
	}

	return extent;
}

static VkExtent2D
radv_gfx10_compute_bin_size(const struct radv_pipeline *pipeline, const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	VkExtent2D extent = {512, 512};

	const unsigned db_tag_size = 64;
	const unsigned db_tag_count = 312;
	const unsigned color_tag_size = 1024;
	const unsigned color_tag_count = 31;
	const unsigned fmask_tag_size = 256;
	const unsigned fmask_tag_count = 44;

	const unsigned rb_count = pipeline->device->physical_device->rad_info.num_render_backends;
	const unsigned pipe_count = MAX2(rb_count, pipeline->device->physical_device->rad_info.num_sdp_interfaces);

	const unsigned db_tag_part = (db_tag_count * rb_count / pipe_count) * db_tag_size * pipe_count;
	const unsigned color_tag_part = (color_tag_count * rb_count / pipe_count) * color_tag_size * pipe_count;
	const unsigned fmask_tag_part = (fmask_tag_count * rb_count / pipe_count) * fmask_tag_size * pipe_count;

	const unsigned total_samples = 1u << G_028BE0_MSAA_NUM_SAMPLES(pipeline->graphics.ms.pa_sc_aa_config);
	const unsigned samples_log = util_logbase2_ceil(total_samples);

	unsigned color_bytes_per_pixel = 0;
	unsigned fmask_bytes_per_pixel = 0;

	const VkPipelineColorBlendStateCreateInfo *vkblend =
		radv_pipeline_get_color_blend_state(pCreateInfo);
	if (vkblend) {
		for (unsigned i = 0; i < subpass->color_count; i++) {
			if (!vkblend->pAttachments[i].colorWriteMask)
				continue;

			if (subpass->color_attachments[i].attachment == VK_ATTACHMENT_UNUSED)
				continue;

			VkFormat format = pass->attachments[subpass->color_attachments[i].attachment].format;
			color_bytes_per_pixel += vk_format_get_blocksize(format);

			if (total_samples > 1) {
				assert(samples_log <= 3);
				const unsigned fmask_array[] = {0, 1, 1, 4};
				fmask_bytes_per_pixel += fmask_array[samples_log];
			}
		}

		color_bytes_per_pixel *= total_samples;
	}
	color_bytes_per_pixel = MAX2(color_bytes_per_pixel, 1);

	const unsigned color_pixel_count_log = util_logbase2(color_tag_part / color_bytes_per_pixel);
	extent.width = 1ull << ((color_pixel_count_log + 1) / 2);
	extent.height = 1ull << (color_pixel_count_log / 2);

	if (fmask_bytes_per_pixel) {
		const unsigned fmask_pixel_count_log = util_logbase2(fmask_tag_part / fmask_bytes_per_pixel);

		const VkExtent2D fmask_extent = (VkExtent2D){
			.width = 1ull << ((fmask_pixel_count_log + 1) / 2),
			.height = 1ull << (color_pixel_count_log / 2)
		};

		if (fmask_extent.width * fmask_extent.height < extent.width * extent.height)
		    extent = fmask_extent;
	}

	if (subpass->depth_stencil_attachment) {
		struct radv_render_pass_attachment *attachment = pass->attachments + subpass->depth_stencil_attachment->attachment;

		/* Coefficients taken from AMDVLK */
		unsigned depth_coeff = vk_format_is_depth(attachment->format) ? 5 : 0;
		unsigned stencil_coeff = vk_format_is_stencil(attachment->format) ? 1 : 0;
		unsigned db_bytes_per_pixel = (depth_coeff + stencil_coeff) * total_samples;

		const unsigned db_pixel_count_log = util_logbase2(db_tag_part / db_bytes_per_pixel);

		const VkExtent2D db_extent = (VkExtent2D){
			.width = 1ull << ((db_pixel_count_log + 1) / 2),
			.height = 1ull << (color_pixel_count_log / 2)
		};

		if (db_extent.width * db_extent.height < extent.width * extent.height)
		    extent = db_extent;
	}

	extent.width = MAX2(extent.width, 128);
	extent.height = MAX2(extent.width, 64);

	return extent;
}

static void
radv_pipeline_init_disabled_binning_state(struct radv_pipeline *pipeline,
					  const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	uint32_t pa_sc_binner_cntl_0 =
	                S_028C44_BINNING_MODE(V_028C44_DISABLE_BINNING_USE_LEGACY_SC) |
	                S_028C44_DISABLE_START_OF_PRIM(1);
	uint32_t db_dfsm_control = S_028060_PUNCHOUT_MODE(V_028060_FORCE_OFF);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
		RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
		struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
		const VkPipelineColorBlendStateCreateInfo *vkblend =
			radv_pipeline_get_color_blend_state(pCreateInfo);
		unsigned min_bytes_per_pixel = 0;

		if (vkblend) {
			for (unsigned i = 0; i < subpass->color_count; i++) {
				if (!vkblend->pAttachments[i].colorWriteMask)
					continue;

				if (subpass->color_attachments[i].attachment == VK_ATTACHMENT_UNUSED)
					continue;

				VkFormat format = pass->attachments[subpass->color_attachments[i].attachment].format;
				unsigned bytes = vk_format_get_blocksize(format);
				if (!min_bytes_per_pixel || bytes < min_bytes_per_pixel)
					min_bytes_per_pixel = bytes;
			}
		}

		pa_sc_binner_cntl_0 =
			S_028C44_BINNING_MODE(V_028C44_DISABLE_BINNING_USE_NEW_SC) |
			S_028C44_BIN_SIZE_X(0) |
			S_028C44_BIN_SIZE_Y(0) |
			S_028C44_BIN_SIZE_X_EXTEND(2) | /* 128 */
			S_028C44_BIN_SIZE_Y_EXTEND(min_bytes_per_pixel <= 4 ? 2 : 1) | /* 128 or 64 */
			S_028C44_DISABLE_START_OF_PRIM(1);
	}

	pipeline->graphics.binning.pa_sc_binner_cntl_0 = pa_sc_binner_cntl_0;
	pipeline->graphics.binning.db_dfsm_control = db_dfsm_control;
}

struct radv_binning_settings
radv_get_binning_settings(const struct radv_physical_device *pdev)
{
	struct radv_binning_settings settings;
	if (pdev->rad_info.has_dedicated_vram) {
		if (pdev->rad_info.num_render_backends > 4) {
			settings.context_states_per_bin = 1;
			settings.persistent_states_per_bin = 1;
		} else {
			settings.context_states_per_bin = 3;
			settings.persistent_states_per_bin = 8;
		}
		settings.fpovs_per_batch = 63;
	} else {
		/* The context states are affected by the scissor bug. */
		settings.context_states_per_bin = 6;
		/* 32 causes hangs for RAVEN. */
		settings.persistent_states_per_bin = 16;
		settings.fpovs_per_batch = 63;
	}

	if (pdev->rad_info.has_gfx9_scissor_bug)
		settings.context_states_per_bin = 1;

	return settings;
}

static void
radv_pipeline_init_binning_state(struct radv_pipeline *pipeline,
				 const VkGraphicsPipelineCreateInfo *pCreateInfo,
				 const struct radv_blend_state *blend)
{
	if (pipeline->device->physical_device->rad_info.chip_class < GFX9)
		return;

	VkExtent2D bin_size;
	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
		bin_size = radv_gfx10_compute_bin_size(pipeline, pCreateInfo);
	} else if (pipeline->device->physical_device->rad_info.chip_class == GFX9) {
		bin_size = radv_gfx9_compute_bin_size(pipeline, pCreateInfo);
	} else
		unreachable("Unhandled generation for binning bin size calculation");

	if (pipeline->device->pbb_allowed && bin_size.width && bin_size.height) {
		struct radv_binning_settings settings =
			radv_get_binning_settings(pipeline->device->physical_device);

		bool disable_start_of_prim = true;
		uint32_t db_dfsm_control = S_028060_PUNCHOUT_MODE(V_028060_FORCE_OFF);

		const struct radv_shader_variant *ps = pipeline->shaders[MESA_SHADER_FRAGMENT];

		if (pipeline->device->dfsm_allowed && ps &&
		    !ps->info.ps.can_discard &&
		    !ps->info.ps.writes_memory &&
		    blend->cb_target_enabled_4bit) {
			db_dfsm_control = S_028060_PUNCHOUT_MODE(V_028060_AUTO);
			disable_start_of_prim = (blend->blend_enable_4bit & blend->cb_target_enabled_4bit) != 0;
		}

		const uint32_t pa_sc_binner_cntl_0 =
	                S_028C44_BINNING_MODE(V_028C44_BINNING_ALLOWED) |
	                S_028C44_BIN_SIZE_X(bin_size.width == 16) |
	                S_028C44_BIN_SIZE_Y(bin_size.height == 16) |
	                S_028C44_BIN_SIZE_X_EXTEND(util_logbase2(MAX2(bin_size.width, 32)) - 5) |
	                S_028C44_BIN_SIZE_Y_EXTEND(util_logbase2(MAX2(bin_size.height, 32)) - 5) |
	                S_028C44_CONTEXT_STATES_PER_BIN(settings.context_states_per_bin - 1) |
	                S_028C44_PERSISTENT_STATES_PER_BIN(settings.persistent_states_per_bin - 1) |
	                S_028C44_DISABLE_START_OF_PRIM(disable_start_of_prim) |
	                S_028C44_FPOVS_PER_BATCH(settings.fpovs_per_batch) |
	                S_028C44_OPTIMAL_BIN_SELECTION(1);

		pipeline->graphics.binning.pa_sc_binner_cntl_0 = pa_sc_binner_cntl_0;
		pipeline->graphics.binning.db_dfsm_control = db_dfsm_control;
	} else
		radv_pipeline_init_disabled_binning_state(pipeline, pCreateInfo);
}


static void
radv_pipeline_generate_depth_stencil_state(struct radeon_cmdbuf *ctx_cs,
                                           const struct radv_pipeline *pipeline,
                                           const VkGraphicsPipelineCreateInfo *pCreateInfo,
                                           const struct radv_graphics_pipeline_create_info *extra)
{
	const VkPipelineDepthStencilStateCreateInfo *vkds = radv_pipeline_get_depth_stencil_state(pCreateInfo);
	RADV_FROM_HANDLE(radv_render_pass, pass, pCreateInfo->renderPass);
	struct radv_subpass *subpass = pass->subpasses + pCreateInfo->subpass;
	struct radv_shader_variant *ps = pipeline->shaders[MESA_SHADER_FRAGMENT];
	struct radv_render_pass_attachment *attachment = NULL;
	uint32_t db_render_control = 0, db_render_override2 = 0;
	uint32_t db_render_override = 0;

	if (subpass->depth_stencil_attachment)
		attachment = pass->attachments + subpass->depth_stencil_attachment->attachment;

	bool has_depth_attachment = attachment && vk_format_is_depth(attachment->format);

	if (vkds && has_depth_attachment) {
		/* from amdvlk: For 4xAA and 8xAA need to decompress on flush for better performance */
		db_render_override2 |= S_028010_DECOMPRESS_Z_ON_FLUSH(attachment->samples > 2);

		if (pipeline->device->physical_device->rad_info.chip_class >= GFX10_3)
			db_render_override2 |= S_028010_CENTROID_COMPUTATION_MODE(2);
	}

	if (attachment && extra) {
		db_render_control |= S_028000_DEPTH_CLEAR_ENABLE(extra->db_depth_clear);
		db_render_control |= S_028000_STENCIL_CLEAR_ENABLE(extra->db_stencil_clear);

		db_render_control |= S_028000_RESUMMARIZE_ENABLE(extra->resummarize_enable);
		db_render_control |= S_028000_DEPTH_COMPRESS_DISABLE(extra->depth_compress_disable);
		db_render_control |= S_028000_STENCIL_COMPRESS_DISABLE(extra->stencil_compress_disable);
		db_render_override2 |= S_028010_DISABLE_ZMASK_EXPCLEAR_OPTIMIZATION(extra->db_depth_disable_expclear);
		db_render_override2 |= S_028010_DISABLE_SMEM_EXPCLEAR_OPTIMIZATION(extra->db_stencil_disable_expclear);
	}

	db_render_override |= S_02800C_FORCE_HIS_ENABLE0(V_02800C_FORCE_DISABLE) |
			      S_02800C_FORCE_HIS_ENABLE1(V_02800C_FORCE_DISABLE);

	if (!pCreateInfo->pRasterizationState->depthClampEnable &&
	    ps->info.ps.writes_z) {
		/* From VK_EXT_depth_range_unrestricted spec:
		 *
		 * "The behavior described in Primitive Clipping still applies.
		 *  If depth clamping is disabled the depth values are still
		 *  clipped to 0 ≤ zc ≤ wc before the viewport transform. If
		 *  depth clamping is enabled the above equation is ignored and
		 *  the depth values are instead clamped to the VkViewport
		 *  minDepth and maxDepth values, which in the case of this
		 *  extension can be outside of the 0.0 to 1.0 range."
		 */
		db_render_override |= S_02800C_DISABLE_VIEWPORT_CLAMP(1);
	}

	radeon_set_context_reg(ctx_cs, R_028000_DB_RENDER_CONTROL, db_render_control);
	radeon_set_context_reg(ctx_cs, R_02800C_DB_RENDER_OVERRIDE, db_render_override);
	radeon_set_context_reg(ctx_cs, R_028010_DB_RENDER_OVERRIDE2, db_render_override2);
}

static void
radv_pipeline_generate_blend_state(struct radeon_cmdbuf *ctx_cs,
                                   const struct radv_pipeline *pipeline,
                                   const struct radv_blend_state *blend)
{
	radeon_set_context_reg_seq(ctx_cs, R_028780_CB_BLEND0_CONTROL, 8);
	radeon_emit_array(ctx_cs, blend->cb_blend_control,
			  8);
	radeon_set_context_reg(ctx_cs, R_028808_CB_COLOR_CONTROL, blend->cb_color_control);
	radeon_set_context_reg(ctx_cs, R_028B70_DB_ALPHA_TO_MASK, blend->db_alpha_to_mask);

	if (pipeline->device->physical_device->rad_info.has_rbplus) {

		radeon_set_context_reg_seq(ctx_cs, R_028760_SX_MRT0_BLEND_OPT, 8);
		radeon_emit_array(ctx_cs, blend->sx_mrt_blend_opt, 8);
	}

	radeon_set_context_reg(ctx_cs, R_028714_SPI_SHADER_COL_FORMAT, blend->spi_shader_col_format);

	radeon_set_context_reg(ctx_cs, R_028238_CB_TARGET_MASK, blend->cb_target_mask);
	radeon_set_context_reg(ctx_cs, R_02823C_CB_SHADER_MASK, blend->cb_shader_mask);
}

static void
radv_pipeline_generate_raster_state(struct radeon_cmdbuf *ctx_cs,
				    const struct radv_pipeline *pipeline,
                                    const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const VkPipelineRasterizationStateCreateInfo *vkraster = pCreateInfo->pRasterizationState;
	const VkConservativeRasterizationModeEXT mode =
		radv_get_conservative_raster_mode(vkraster);
	uint32_t pa_sc_conservative_rast = S_028C4C_NULL_SQUAD_AA_MASK_ENABLE(1);
	bool depth_clip_disable = vkraster->depthClampEnable;

	const VkPipelineRasterizationDepthClipStateCreateInfoEXT *depth_clip_state =
		vk_find_struct_const(vkraster->pNext, PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT);
	if (depth_clip_state) {
		depth_clip_disable = !depth_clip_state->depthClipEnable;
	}

	radeon_set_context_reg(ctx_cs, R_028810_PA_CL_CLIP_CNTL,
	                       S_028810_DX_CLIP_SPACE_DEF(1) | // vulkan uses DX conventions.
	                       S_028810_ZCLIP_NEAR_DISABLE(depth_clip_disable ? 1 : 0) |
	                       S_028810_ZCLIP_FAR_DISABLE(depth_clip_disable ? 1 : 0) |
	                       S_028810_DX_RASTERIZATION_KILL(vkraster->rasterizerDiscardEnable ? 1 : 0) |
	                       S_028810_DX_LINEAR_ATTR_CLIP_ENA(1));

	radeon_set_context_reg(ctx_cs, R_028BDC_PA_SC_LINE_CNTL,
			       S_028BDC_DX10_DIAMOND_TEST_ENA(1));

	/* Conservative rasterization. */
	if (mode != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT) {
		pa_sc_conservative_rast = S_028C4C_PREZ_AA_MASK_ENABLE(1) |
					  S_028C4C_POSTZ_AA_MASK_ENABLE(1) |
					  S_028C4C_CENTROID_SAMPLE_OVERRIDE(1);

		if (mode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT) {
			pa_sc_conservative_rast |=
				S_028C4C_OVER_RAST_ENABLE(1) |
				S_028C4C_OVER_RAST_SAMPLE_SELECT(0) |
				S_028C4C_UNDER_RAST_ENABLE(0) |
				S_028C4C_UNDER_RAST_SAMPLE_SELECT(1) |
				S_028C4C_PBB_UNCERTAINTY_REGION_ENABLE(1);
		} else {
			assert(mode == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT);
			pa_sc_conservative_rast |=
				S_028C4C_OVER_RAST_ENABLE(0) |
				S_028C4C_OVER_RAST_SAMPLE_SELECT(1) |
				S_028C4C_UNDER_RAST_ENABLE(1) |
				S_028C4C_UNDER_RAST_SAMPLE_SELECT(0) |
				S_028C4C_PBB_UNCERTAINTY_REGION_ENABLE(0);
		}
	}

	radeon_set_context_reg(ctx_cs, R_028C4C_PA_SC_CONSERVATIVE_RASTERIZATION_CNTL,
				   pa_sc_conservative_rast);
}


static void
radv_pipeline_generate_multisample_state(struct radeon_cmdbuf *ctx_cs,
                                         const struct radv_pipeline *pipeline)
{
	const struct radv_multisample_state *ms = &pipeline->graphics.ms;

	radeon_set_context_reg_seq(ctx_cs, R_028C38_PA_SC_AA_MASK_X0Y0_X1Y0, 2);
	radeon_emit(ctx_cs, ms->pa_sc_aa_mask[0]);
	radeon_emit(ctx_cs, ms->pa_sc_aa_mask[1]);

	radeon_set_context_reg(ctx_cs, R_028804_DB_EQAA, ms->db_eqaa);
	radeon_set_context_reg(ctx_cs, R_028A48_PA_SC_MODE_CNTL_0, ms->pa_sc_mode_cntl_0);
	radeon_set_context_reg(ctx_cs, R_028A4C_PA_SC_MODE_CNTL_1, ms->pa_sc_mode_cntl_1);
	radeon_set_context_reg(ctx_cs, R_028BE0_PA_SC_AA_CONFIG, ms->pa_sc_aa_config);

	/* The exclusion bits can be set to improve rasterization efficiency
	 * if no sample lies on the pixel boundary (-8 sample offset). It's
	 * currently always TRUE because the driver doesn't support 16 samples.
	 */
	bool exclusion = pipeline->device->physical_device->rad_info.chip_class >= GFX7;
	radeon_set_context_reg(ctx_cs, R_02882C_PA_SU_PRIM_FILTER_CNTL,
			       S_02882C_XMAX_RIGHT_EXCLUSION(exclusion) |
			       S_02882C_YMAX_BOTTOM_EXCLUSION(exclusion));

	/* GFX9: Flush DFSM when the AA mode changes. */
	if (pipeline->device->dfsm_allowed) {
		radeon_emit(ctx_cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
		radeon_emit(ctx_cs, EVENT_TYPE(V_028A90_FLUSH_DFSM) | EVENT_INDEX(0));
	}
}

static void
radv_pipeline_generate_vgt_gs_mode(struct radeon_cmdbuf *ctx_cs,
                                   const struct radv_pipeline *pipeline)
{
	const struct radv_vs_output_info *outinfo = get_vs_output_info(pipeline);
	const struct radv_shader_variant *vs =
		pipeline->shaders[MESA_SHADER_TESS_EVAL] ?
		pipeline->shaders[MESA_SHADER_TESS_EVAL] :
		pipeline->shaders[MESA_SHADER_VERTEX];
	unsigned vgt_primitiveid_en = 0;
	uint32_t vgt_gs_mode = 0;

	if (radv_pipeline_has_ngg(pipeline))
		return;

	if (radv_pipeline_has_gs(pipeline)) {
		const struct radv_shader_variant *gs =
			pipeline->shaders[MESA_SHADER_GEOMETRY];

		vgt_gs_mode = ac_vgt_gs_mode(gs->info.gs.vertices_out,
		                             pipeline->device->physical_device->rad_info.chip_class);
	} else if (outinfo->export_prim_id || vs->info.uses_prim_id) {
		vgt_gs_mode = S_028A40_MODE(V_028A40_GS_SCENARIO_A);
		vgt_primitiveid_en |= S_028A84_PRIMITIVEID_EN(1);
	}

	radeon_set_context_reg(ctx_cs, R_028A84_VGT_PRIMITIVEID_EN, vgt_primitiveid_en);
	radeon_set_context_reg(ctx_cs, R_028A40_VGT_GS_MODE, vgt_gs_mode);
}

static void
radv_pipeline_generate_hw_vs(struct radeon_cmdbuf *ctx_cs,
			     struct radeon_cmdbuf *cs,
			     const struct radv_pipeline *pipeline,
			     const struct radv_shader_variant *shader)
{
	uint64_t va = radv_buffer_get_va(shader->bo) + shader->bo_offset;

	radeon_set_sh_reg_seq(cs, R_00B120_SPI_SHADER_PGM_LO_VS, 4);
	radeon_emit(cs, va >> 8);
	radeon_emit(cs, S_00B124_MEM_BASE(va >> 40));
	radeon_emit(cs, shader->config.rsrc1);
	radeon_emit(cs, shader->config.rsrc2);

	const struct radv_vs_output_info *outinfo = get_vs_output_info(pipeline);
	unsigned clip_dist_mask, cull_dist_mask, total_mask;
	clip_dist_mask = outinfo->clip_dist_mask;
	cull_dist_mask = outinfo->cull_dist_mask;
	total_mask = clip_dist_mask | cull_dist_mask;
	bool misc_vec_ena = outinfo->writes_pointsize ||
		outinfo->writes_layer ||
		outinfo->writes_viewport_index;
	unsigned spi_vs_out_config, nparams;

	/* VS is required to export at least one param. */
	nparams = MAX2(outinfo->param_exports, 1);
	spi_vs_out_config = S_0286C4_VS_EXPORT_COUNT(nparams - 1);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
		spi_vs_out_config |= S_0286C4_NO_PC_EXPORT(outinfo->param_exports == 0);
	}

	radeon_set_context_reg(ctx_cs, R_0286C4_SPI_VS_OUT_CONFIG, spi_vs_out_config);

	radeon_set_context_reg(ctx_cs, R_02870C_SPI_SHADER_POS_FORMAT,
	                       S_02870C_POS0_EXPORT_FORMAT(V_02870C_SPI_SHADER_4COMP) |
	                       S_02870C_POS1_EXPORT_FORMAT(outinfo->pos_exports > 1 ?
	                                                   V_02870C_SPI_SHADER_4COMP :
	                                                   V_02870C_SPI_SHADER_NONE) |
	                       S_02870C_POS2_EXPORT_FORMAT(outinfo->pos_exports > 2 ?
	                                                   V_02870C_SPI_SHADER_4COMP :
	                                                   V_02870C_SPI_SHADER_NONE) |
	                       S_02870C_POS3_EXPORT_FORMAT(outinfo->pos_exports > 3 ?
	                                                   V_02870C_SPI_SHADER_4COMP :
	                                                   V_02870C_SPI_SHADER_NONE));

	radeon_set_context_reg(ctx_cs, R_02881C_PA_CL_VS_OUT_CNTL,
	                       S_02881C_USE_VTX_POINT_SIZE(outinfo->writes_pointsize) |
	                       S_02881C_USE_VTX_RENDER_TARGET_INDX(outinfo->writes_layer) |
	                       S_02881C_USE_VTX_VIEWPORT_INDX(outinfo->writes_viewport_index) |
	                       S_02881C_VS_OUT_MISC_VEC_ENA(misc_vec_ena) |
	                       S_02881C_VS_OUT_MISC_SIDE_BUS_ENA(misc_vec_ena) |
	                       S_02881C_VS_OUT_CCDIST0_VEC_ENA((total_mask & 0x0f) != 0) |
	                       S_02881C_VS_OUT_CCDIST1_VEC_ENA((total_mask & 0xf0) != 0) |
			       S_02881C_BYPASS_PRIM_RATE_COMBINER(pipeline->device->physical_device->rad_info.chip_class >= GFX10_3) |
			       S_02881C_BYPASS_VTX_RATE_COMBINER(pipeline->device->physical_device->rad_info.chip_class >= GFX10_3) |
	                       cull_dist_mask << 8 |
	                       clip_dist_mask);

	if (pipeline->device->physical_device->rad_info.chip_class <= GFX8)
		radeon_set_context_reg(ctx_cs, R_028AB4_VGT_REUSE_OFF,
		                       outinfo->writes_viewport_index);
}

static void
radv_pipeline_generate_hw_es(struct radeon_cmdbuf *cs,
			     const struct radv_pipeline *pipeline,
			     const struct radv_shader_variant *shader)
{
	uint64_t va = radv_buffer_get_va(shader->bo) + shader->bo_offset;

	radeon_set_sh_reg_seq(cs, R_00B320_SPI_SHADER_PGM_LO_ES, 4);
	radeon_emit(cs, va >> 8);
	radeon_emit(cs, S_00B324_MEM_BASE(va >> 40));
	radeon_emit(cs, shader->config.rsrc1);
	radeon_emit(cs, shader->config.rsrc2);
}

static void
radv_pipeline_generate_hw_ls(struct radeon_cmdbuf *cs,
			     const struct radv_pipeline *pipeline,
			     const struct radv_shader_variant *shader)
{
	unsigned num_lds_blocks = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.num_lds_blocks;
	uint64_t va = radv_buffer_get_va(shader->bo) + shader->bo_offset;
	uint32_t rsrc2 = shader->config.rsrc2;

	radeon_set_sh_reg_seq(cs, R_00B520_SPI_SHADER_PGM_LO_LS, 2);
	radeon_emit(cs, va >> 8);
	radeon_emit(cs, S_00B524_MEM_BASE(va >> 40));

	rsrc2 |= S_00B52C_LDS_SIZE(num_lds_blocks);
	if (pipeline->device->physical_device->rad_info.chip_class == GFX7 &&
	    pipeline->device->physical_device->rad_info.family != CHIP_HAWAII)
		radeon_set_sh_reg(cs, R_00B52C_SPI_SHADER_PGM_RSRC2_LS, rsrc2);

	radeon_set_sh_reg_seq(cs, R_00B528_SPI_SHADER_PGM_RSRC1_LS, 2);
	radeon_emit(cs, shader->config.rsrc1);
	radeon_emit(cs, rsrc2);
}

static void
radv_pipeline_generate_hw_ngg(struct radeon_cmdbuf *ctx_cs,
			      struct radeon_cmdbuf *cs,
			      const struct radv_pipeline *pipeline,
			      const struct radv_shader_variant *shader)
{
	uint64_t va = radv_buffer_get_va(shader->bo) + shader->bo_offset;
	gl_shader_stage es_type =
		radv_pipeline_has_tess(pipeline) ? MESA_SHADER_TESS_EVAL : MESA_SHADER_VERTEX;
	struct radv_shader_variant *es =
		es_type == MESA_SHADER_TESS_EVAL ? pipeline->shaders[MESA_SHADER_TESS_EVAL] : pipeline->shaders[MESA_SHADER_VERTEX];
	const struct gfx10_ngg_info *ngg_state = &shader->info.ngg_info;

	radeon_set_sh_reg_seq(cs, R_00B320_SPI_SHADER_PGM_LO_ES, 2);
	radeon_emit(cs, va >> 8);
	radeon_emit(cs, S_00B324_MEM_BASE(va >> 40));
	radeon_set_sh_reg_seq(cs, R_00B228_SPI_SHADER_PGM_RSRC1_GS, 2);
	radeon_emit(cs, shader->config.rsrc1);
	radeon_emit(cs, shader->config.rsrc2);

	const struct radv_vs_output_info *outinfo = get_vs_output_info(pipeline);
	unsigned clip_dist_mask, cull_dist_mask, total_mask;
	clip_dist_mask = outinfo->clip_dist_mask;
	cull_dist_mask = outinfo->cull_dist_mask;
	total_mask = clip_dist_mask | cull_dist_mask;
	bool misc_vec_ena = outinfo->writes_pointsize ||
		outinfo->writes_layer ||
		outinfo->writes_viewport_index;
	bool es_enable_prim_id = outinfo->export_prim_id ||
				 (es && es->info.uses_prim_id);
	bool break_wave_at_eoi = false;
	unsigned ge_cntl;
	unsigned nparams;

	if (es_type == MESA_SHADER_TESS_EVAL) {
		struct radv_shader_variant *gs =
			pipeline->shaders[MESA_SHADER_GEOMETRY];

		if (es_enable_prim_id || (gs && gs->info.uses_prim_id))
			break_wave_at_eoi = true;
	}

	nparams = MAX2(outinfo->param_exports, 1);
	radeon_set_context_reg(ctx_cs, R_0286C4_SPI_VS_OUT_CONFIG,
	                       S_0286C4_VS_EXPORT_COUNT(nparams - 1) |
			       S_0286C4_NO_PC_EXPORT(outinfo->param_exports == 0));

	radeon_set_context_reg(ctx_cs, R_028708_SPI_SHADER_IDX_FORMAT,
			       S_028708_IDX0_EXPORT_FORMAT(V_028708_SPI_SHADER_1COMP));
	radeon_set_context_reg(ctx_cs, R_02870C_SPI_SHADER_POS_FORMAT,
	                       S_02870C_POS0_EXPORT_FORMAT(V_02870C_SPI_SHADER_4COMP) |
	                       S_02870C_POS1_EXPORT_FORMAT(outinfo->pos_exports > 1 ?
	                                                   V_02870C_SPI_SHADER_4COMP :
	                                                   V_02870C_SPI_SHADER_NONE) |
	                       S_02870C_POS2_EXPORT_FORMAT(outinfo->pos_exports > 2 ?
	                                                   V_02870C_SPI_SHADER_4COMP :
	                                                   V_02870C_SPI_SHADER_NONE) |
	                       S_02870C_POS3_EXPORT_FORMAT(outinfo->pos_exports > 3 ?
	                                                   V_02870C_SPI_SHADER_4COMP :
	                                                   V_02870C_SPI_SHADER_NONE));

	radeon_set_context_reg(ctx_cs, R_02881C_PA_CL_VS_OUT_CNTL,
	                       S_02881C_USE_VTX_POINT_SIZE(outinfo->writes_pointsize) |
	                       S_02881C_USE_VTX_RENDER_TARGET_INDX(outinfo->writes_layer) |
	                       S_02881C_USE_VTX_VIEWPORT_INDX(outinfo->writes_viewport_index) |
	                       S_02881C_VS_OUT_MISC_VEC_ENA(misc_vec_ena) |
	                       S_02881C_VS_OUT_MISC_SIDE_BUS_ENA(misc_vec_ena) |
	                       S_02881C_VS_OUT_CCDIST0_VEC_ENA((total_mask & 0x0f) != 0) |
	                       S_02881C_VS_OUT_CCDIST1_VEC_ENA((total_mask & 0xf0) != 0) |
			       S_02881C_BYPASS_PRIM_RATE_COMBINER(pipeline->device->physical_device->rad_info.chip_class >= GFX10_3) |
			       S_02881C_BYPASS_VTX_RATE_COMBINER(pipeline->device->physical_device->rad_info.chip_class >= GFX10_3) |
	                       cull_dist_mask << 8 |
	                       clip_dist_mask);

	radeon_set_context_reg(ctx_cs, R_028A84_VGT_PRIMITIVEID_EN,
			       S_028A84_PRIMITIVEID_EN(es_enable_prim_id) |
			       S_028A84_NGG_DISABLE_PROVOK_REUSE(outinfo->export_prim_id));

	radeon_set_context_reg(ctx_cs, R_028AAC_VGT_ESGS_RING_ITEMSIZE,
			       ngg_state->vgt_esgs_ring_itemsize);

	/* NGG specific registers. */
	struct radv_shader_variant *gs = pipeline->shaders[MESA_SHADER_GEOMETRY];
	uint32_t gs_num_invocations = gs ? gs->info.gs.invocations : 1;

	radeon_set_context_reg(ctx_cs, R_028A44_VGT_GS_ONCHIP_CNTL,
			       S_028A44_ES_VERTS_PER_SUBGRP(ngg_state->hw_max_esverts) |
			       S_028A44_GS_PRIMS_PER_SUBGRP(ngg_state->max_gsprims) |
			       S_028A44_GS_INST_PRIMS_IN_SUBGRP(ngg_state->max_gsprims * gs_num_invocations));
	radeon_set_context_reg(ctx_cs, R_0287FC_GE_MAX_OUTPUT_PER_SUBGROUP,
			       S_0287FC_MAX_VERTS_PER_SUBGROUP(ngg_state->max_out_verts));
	radeon_set_context_reg(ctx_cs, R_028B4C_GE_NGG_SUBGRP_CNTL,
			       S_028B4C_PRIM_AMP_FACTOR(ngg_state->prim_amp_factor) |
			       S_028B4C_THDS_PER_SUBGRP(0)); /* for fast launch */
	radeon_set_context_reg(ctx_cs, R_028B90_VGT_GS_INSTANCE_CNT,
			       S_028B90_CNT(gs_num_invocations) |
			       S_028B90_ENABLE(gs_num_invocations > 1) |
			       S_028B90_EN_MAX_VERT_OUT_PER_GS_INSTANCE(ngg_state->max_vert_out_per_gs_instance));

	/* User edge flags are set by the pos exports. If user edge flags are
	 * not used, we must use hw-generated edge flags and pass them via
	 * the prim export to prevent drawing lines on internal edges of
	 * decomposed primitives (such as quads) with polygon mode = lines.
	 *
	 * TODO: We should combine hw-generated edge flags with user edge
	 *       flags in the shader.
	 */
	radeon_set_context_reg(ctx_cs, R_028838_PA_CL_NGG_CNTL,
			       S_028838_INDEX_BUF_EDGE_FLAG_ENA(!radv_pipeline_has_tess(pipeline) &&
			                                        !radv_pipeline_has_gs(pipeline)) |
			       /* Reuse for NGG. */
			       S_028838_VERTEX_REUSE_DEPTH(pipeline->device->physical_device->rad_info.chip_class >= GFX10_3 ? 30 : 0));

	ge_cntl = S_03096C_PRIM_GRP_SIZE(ngg_state->max_gsprims) |
		  S_03096C_VERT_GRP_SIZE(256) | /* 256 = disable vertex grouping */
		  S_03096C_BREAK_WAVE_AT_EOI(break_wave_at_eoi);

	/* Bug workaround for a possible hang with non-tessellation cases.
	 * Tessellation always sets GE_CNTL.VERT_GRP_SIZE = 0
	 *
	 * Requirement: GE_CNTL.VERT_GRP_SIZE = VGT_GS_ONCHIP_CNTL.ES_VERTS_PER_SUBGRP - 5
	 */
	if (pipeline->device->physical_device->rad_info.chip_class == GFX10 &&
	    !radv_pipeline_has_tess(pipeline) &&
	    ngg_state->hw_max_esverts != 256) {
		ge_cntl &= C_03096C_VERT_GRP_SIZE;

		if (ngg_state->hw_max_esverts > 5) {
			ge_cntl |= S_03096C_VERT_GRP_SIZE(ngg_state->hw_max_esverts - 5);
		}
	}

	radeon_set_uconfig_reg(ctx_cs, R_03096C_GE_CNTL, ge_cntl);
}

static void
radv_pipeline_generate_hw_hs(struct radeon_cmdbuf *cs,
			     const struct radv_pipeline *pipeline,
			     const struct radv_shader_variant *shader)
{
	uint64_t va = radv_buffer_get_va(shader->bo) + shader->bo_offset;

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9) {
		if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
			radeon_set_sh_reg_seq(cs, R_00B520_SPI_SHADER_PGM_LO_LS, 2);
			radeon_emit(cs, va >> 8);
			radeon_emit(cs, S_00B524_MEM_BASE(va >> 40));
		} else {
			radeon_set_sh_reg_seq(cs, R_00B410_SPI_SHADER_PGM_LO_LS, 2);
			radeon_emit(cs, va >> 8);
			radeon_emit(cs, S_00B414_MEM_BASE(va >> 40));
		}

		radeon_set_sh_reg_seq(cs, R_00B428_SPI_SHADER_PGM_RSRC1_HS, 2);
		radeon_emit(cs, shader->config.rsrc1);
		radeon_emit(cs, shader->config.rsrc2);
	} else {
		radeon_set_sh_reg_seq(cs, R_00B420_SPI_SHADER_PGM_LO_HS, 4);
		radeon_emit(cs, va >> 8);
		radeon_emit(cs, S_00B424_MEM_BASE(va >> 40));
		radeon_emit(cs, shader->config.rsrc1);
		radeon_emit(cs, shader->config.rsrc2);
	}
}

static void
radv_pipeline_generate_vertex_shader(struct radeon_cmdbuf *ctx_cs,
				     struct radeon_cmdbuf *cs,
				     const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *vs;

	/* Skip shaders merged into HS/GS */
	vs = pipeline->shaders[MESA_SHADER_VERTEX];
	if (!vs)
		return;

	if (vs->info.vs.as_ls)
		radv_pipeline_generate_hw_ls(cs, pipeline, vs);
	else if (vs->info.vs.as_es)
		radv_pipeline_generate_hw_es(cs, pipeline, vs);
	else if (vs->info.is_ngg)
		radv_pipeline_generate_hw_ngg(ctx_cs, cs, pipeline, vs);
	else
		radv_pipeline_generate_hw_vs(ctx_cs, cs, pipeline, vs);
}

static void
radv_pipeline_generate_tess_shaders(struct radeon_cmdbuf *ctx_cs,
				    struct radeon_cmdbuf *cs,
				    const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *tes, *tcs;

	tcs = pipeline->shaders[MESA_SHADER_TESS_CTRL];
	tes = pipeline->shaders[MESA_SHADER_TESS_EVAL];

	if (tes) {
		if (tes->info.is_ngg) {
			radv_pipeline_generate_hw_ngg(ctx_cs, cs, pipeline, tes);
		} else if (tes->info.tes.as_es)
			radv_pipeline_generate_hw_es(cs, pipeline, tes);
		else
			radv_pipeline_generate_hw_vs(ctx_cs, cs, pipeline, tes);
	}

	radv_pipeline_generate_hw_hs(cs, pipeline, tcs);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10 &&
	    !radv_pipeline_has_gs(pipeline) && !radv_pipeline_has_ngg(pipeline)) {
		radeon_set_context_reg(ctx_cs, R_028A44_VGT_GS_ONCHIP_CNTL,
		                       S_028A44_ES_VERTS_PER_SUBGRP(250) |
		                       S_028A44_GS_PRIMS_PER_SUBGRP(126) |
		                       S_028A44_GS_INST_PRIMS_IN_SUBGRP(126));
	}
}

static void
radv_pipeline_generate_tess_state(struct radeon_cmdbuf *ctx_cs,
				  const struct radv_pipeline *pipeline,
				  const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	struct radv_shader_variant *tes = radv_get_shader(pipeline, MESA_SHADER_TESS_EVAL);
	unsigned type = 0, partitioning = 0, topology = 0, distribution_mode = 0;
	unsigned num_tcs_input_cp, num_tcs_output_cp, num_patches;
	unsigned ls_hs_config;

	num_tcs_input_cp = pCreateInfo->pTessellationState->patchControlPoints;
	num_tcs_output_cp = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.tcs_vertices_out; //TCS VERTICES OUT
	num_patches = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.num_patches;

	ls_hs_config = S_028B58_NUM_PATCHES(num_patches) |
		       S_028B58_HS_NUM_INPUT_CP(num_tcs_input_cp) |
		       S_028B58_HS_NUM_OUTPUT_CP(num_tcs_output_cp);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX7) {
		radeon_set_context_reg_idx(ctx_cs, R_028B58_VGT_LS_HS_CONFIG,
					   2, ls_hs_config);
	} else {
		radeon_set_context_reg(ctx_cs, R_028B58_VGT_LS_HS_CONFIG,
				       ls_hs_config);
	}

	switch (tes->info.tes.primitive_mode) {
	case GL_TRIANGLES:
		type = V_028B6C_TESS_TRIANGLE;
		break;
	case GL_QUADS:
		type = V_028B6C_TESS_QUAD;
		break;
	case GL_ISOLINES:
		type = V_028B6C_TESS_ISOLINE;
		break;
	}

	switch (tes->info.tes.spacing) {
	case TESS_SPACING_EQUAL:
		partitioning = V_028B6C_PART_INTEGER;
		break;
	case TESS_SPACING_FRACTIONAL_ODD:
		partitioning = V_028B6C_PART_FRAC_ODD;
		break;
	case TESS_SPACING_FRACTIONAL_EVEN:
		partitioning = V_028B6C_PART_FRAC_EVEN;
		break;
	default:
		break;
	}

	bool ccw = tes->info.tes.ccw;
	const VkPipelineTessellationDomainOriginStateCreateInfo *domain_origin_state =
	              vk_find_struct_const(pCreateInfo->pTessellationState,
	                                   PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO);

	if (domain_origin_state && domain_origin_state->domainOrigin != VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT)
		ccw = !ccw;

	if (tes->info.tes.point_mode)
		topology = V_028B6C_OUTPUT_POINT;
	else if (tes->info.tes.primitive_mode == GL_ISOLINES)
		topology = V_028B6C_OUTPUT_LINE;
	else if (ccw)
		topology = V_028B6C_OUTPUT_TRIANGLE_CCW;
	else
		topology = V_028B6C_OUTPUT_TRIANGLE_CW;

	if (pipeline->device->physical_device->rad_info.has_distributed_tess) {
		if (pipeline->device->physical_device->rad_info.family == CHIP_FIJI ||
		    pipeline->device->physical_device->rad_info.family >= CHIP_POLARIS10)
			distribution_mode = V_028B6C_TRAPEZOIDS;
		else
			distribution_mode = V_028B6C_DONUTS;
	} else
		distribution_mode = V_028B6C_NO_DIST;

	radeon_set_context_reg(ctx_cs, R_028B6C_VGT_TF_PARAM,
			       S_028B6C_TYPE(type) |
			       S_028B6C_PARTITIONING(partitioning) |
			       S_028B6C_TOPOLOGY(topology) |
			       S_028B6C_DISTRIBUTION_MODE(distribution_mode));
}

static void
radv_pipeline_generate_hw_gs(struct radeon_cmdbuf *ctx_cs,
			     struct radeon_cmdbuf *cs,
			     const struct radv_pipeline *pipeline,
			     const struct radv_shader_variant *gs)
{
	const struct gfx9_gs_info *gs_state = &gs->info.gs_ring_info;
	unsigned gs_max_out_vertices;
	const uint8_t *num_components;
	uint8_t max_stream;
	unsigned offset;
	uint64_t va;

	gs_max_out_vertices = gs->info.gs.vertices_out;
	max_stream = gs->info.gs.max_stream;
	num_components = gs->info.gs.num_stream_output_components;

	offset = num_components[0] * gs_max_out_vertices;

	radeon_set_context_reg_seq(ctx_cs, R_028A60_VGT_GSVS_RING_OFFSET_1, 3);
	radeon_emit(ctx_cs, offset);
	if (max_stream >= 1)
		offset += num_components[1] * gs_max_out_vertices;
	radeon_emit(ctx_cs, offset);
	if (max_stream >= 2)
		offset += num_components[2] * gs_max_out_vertices;
	radeon_emit(ctx_cs, offset);
	if (max_stream >= 3)
		offset += num_components[3] * gs_max_out_vertices;
	radeon_set_context_reg(ctx_cs, R_028AB0_VGT_GSVS_RING_ITEMSIZE, offset);

	radeon_set_context_reg_seq(ctx_cs, R_028B5C_VGT_GS_VERT_ITEMSIZE, 4);
	radeon_emit(ctx_cs, num_components[0]);
	radeon_emit(ctx_cs, (max_stream >= 1) ? num_components[1] : 0);
	radeon_emit(ctx_cs, (max_stream >= 2) ? num_components[2] : 0);
	radeon_emit(ctx_cs, (max_stream >= 3) ? num_components[3] : 0);

	uint32_t gs_num_invocations = gs->info.gs.invocations;
	radeon_set_context_reg(ctx_cs, R_028B90_VGT_GS_INSTANCE_CNT,
			       S_028B90_CNT(MIN2(gs_num_invocations, 127)) |
			       S_028B90_ENABLE(gs_num_invocations > 0));

	radeon_set_context_reg(ctx_cs, R_028AAC_VGT_ESGS_RING_ITEMSIZE,
			       gs_state->vgt_esgs_ring_itemsize);

	va = radv_buffer_get_va(gs->bo) + gs->bo_offset;

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9) {
		if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
			radeon_set_sh_reg_seq(cs, R_00B320_SPI_SHADER_PGM_LO_ES, 2);
			radeon_emit(cs, va >> 8);
			radeon_emit(cs, S_00B324_MEM_BASE(va >> 40));
		} else {
			radeon_set_sh_reg_seq(cs, R_00B210_SPI_SHADER_PGM_LO_ES, 2);
			radeon_emit(cs, va >> 8);
			radeon_emit(cs, S_00B214_MEM_BASE(va >> 40));
		}

		radeon_set_sh_reg_seq(cs, R_00B228_SPI_SHADER_PGM_RSRC1_GS, 2);
		radeon_emit(cs, gs->config.rsrc1);
		radeon_emit(cs, gs->config.rsrc2 | S_00B22C_LDS_SIZE(gs_state->lds_size));

		radeon_set_context_reg(ctx_cs, R_028A44_VGT_GS_ONCHIP_CNTL, gs_state->vgt_gs_onchip_cntl);
		radeon_set_context_reg(ctx_cs, R_028A94_VGT_GS_MAX_PRIMS_PER_SUBGROUP, gs_state->vgt_gs_max_prims_per_subgroup);
	} else {
		radeon_set_sh_reg_seq(cs, R_00B220_SPI_SHADER_PGM_LO_GS, 4);
		radeon_emit(cs, va >> 8);
		radeon_emit(cs, S_00B224_MEM_BASE(va >> 40));
		radeon_emit(cs, gs->config.rsrc1);
		radeon_emit(cs, gs->config.rsrc2);
	}

	radv_pipeline_generate_hw_vs(ctx_cs, cs, pipeline, pipeline->gs_copy_shader);
}

static void
radv_pipeline_generate_geometry_shader(struct radeon_cmdbuf *ctx_cs,
				       struct radeon_cmdbuf *cs,
				       const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *gs;

	gs = pipeline->shaders[MESA_SHADER_GEOMETRY];
	if (!gs)
		return;

	if (gs->info.is_ngg)
		radv_pipeline_generate_hw_ngg(ctx_cs, cs, pipeline, gs);
	else
		radv_pipeline_generate_hw_gs(ctx_cs, cs, pipeline, gs);

	radeon_set_context_reg(ctx_cs, R_028B38_VGT_GS_MAX_VERT_OUT,
			      gs->info.gs.vertices_out);
}

static uint32_t offset_to_ps_input(uint32_t offset, bool flat_shade,
				   bool explicit, bool float16)
{
	uint32_t ps_input_cntl;
	if (offset <= AC_EXP_PARAM_OFFSET_31) {
		ps_input_cntl = S_028644_OFFSET(offset);
		if (flat_shade || explicit)
			ps_input_cntl |= S_028644_FLAT_SHADE(1);
		if (explicit) {
			/* Force parameter cache to be read in passthrough
			 * mode.
			 */
			ps_input_cntl |= S_028644_OFFSET(1 << 5);
		}
		if (float16) {
			ps_input_cntl |= S_028644_FP16_INTERP_MODE(1) |
			                 S_028644_ATTR0_VALID(1);
		}
	} else {
		/* The input is a DEFAULT_VAL constant. */
		assert(offset >= AC_EXP_PARAM_DEFAULT_VAL_0000 &&
		       offset <= AC_EXP_PARAM_DEFAULT_VAL_1111);
		offset -= AC_EXP_PARAM_DEFAULT_VAL_0000;
		ps_input_cntl = S_028644_OFFSET(0x20) |
			S_028644_DEFAULT_VAL(offset);
	}
	return ps_input_cntl;
}

static void
radv_pipeline_generate_ps_inputs(struct radeon_cmdbuf *ctx_cs,
				 const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *ps = pipeline->shaders[MESA_SHADER_FRAGMENT];
	const struct radv_vs_output_info *outinfo = get_vs_output_info(pipeline);
	uint32_t ps_input_cntl[32];

	unsigned ps_offset = 0;

	if (ps->info.ps.prim_id_input) {
		unsigned vs_offset = outinfo->vs_output_param_offset[VARYING_SLOT_PRIMITIVE_ID];
		if (vs_offset != AC_EXP_PARAM_UNDEFINED) {
			ps_input_cntl[ps_offset] = offset_to_ps_input(vs_offset, true, false, false);
			++ps_offset;
		}
	}

	if (ps->info.ps.layer_input ||
	    ps->info.needs_multiview_view_index) {
		unsigned vs_offset = outinfo->vs_output_param_offset[VARYING_SLOT_LAYER];
		if (vs_offset != AC_EXP_PARAM_UNDEFINED)
			ps_input_cntl[ps_offset] = offset_to_ps_input(vs_offset, true, false, false);
		else
			ps_input_cntl[ps_offset] = offset_to_ps_input(AC_EXP_PARAM_DEFAULT_VAL_0000, true, false, false);
		++ps_offset;
	}

	if (ps->info.ps.viewport_index_input) {
		unsigned vs_offset = outinfo->vs_output_param_offset[VARYING_SLOT_VIEWPORT];
		if (vs_offset != AC_EXP_PARAM_UNDEFINED)
			ps_input_cntl[ps_offset] = offset_to_ps_input(vs_offset, true, false, false);
		else
			ps_input_cntl[ps_offset] = offset_to_ps_input(AC_EXP_PARAM_DEFAULT_VAL_0000, true, false, false);
		++ps_offset;
	}

	if (ps->info.ps.has_pcoord) {
		unsigned val;
		val = S_028644_PT_SPRITE_TEX(1) | S_028644_OFFSET(0x20);
		ps_input_cntl[ps_offset] = val;
		ps_offset++;
	}

	if (ps->info.ps.num_input_clips_culls) {
		unsigned vs_offset;

		vs_offset = outinfo->vs_output_param_offset[VARYING_SLOT_CLIP_DIST0];
		if (vs_offset != AC_EXP_PARAM_UNDEFINED) {
			ps_input_cntl[ps_offset] = offset_to_ps_input(vs_offset, false, false, false);
			++ps_offset;
		}

		vs_offset = outinfo->vs_output_param_offset[VARYING_SLOT_CLIP_DIST1];
		if (vs_offset != AC_EXP_PARAM_UNDEFINED &&
		    ps->info.ps.num_input_clips_culls > 4) {
			ps_input_cntl[ps_offset] = offset_to_ps_input(vs_offset, false, false, false);
			++ps_offset;
		}
	}

	for (unsigned i = 0; i < 32 && (1u << i) <= ps->info.ps.input_mask; ++i) {
		unsigned vs_offset;
		bool flat_shade;
		bool explicit;
		bool float16;
		if (!(ps->info.ps.input_mask & (1u << i)))
			continue;

		vs_offset = outinfo->vs_output_param_offset[VARYING_SLOT_VAR0 + i];
		if (vs_offset == AC_EXP_PARAM_UNDEFINED) {
			ps_input_cntl[ps_offset] = S_028644_OFFSET(0x20);
			++ps_offset;
			continue;
		}

		flat_shade = !!(ps->info.ps.flat_shaded_mask & (1u << ps_offset));
		explicit = !!(ps->info.ps.explicit_shaded_mask & (1u << ps_offset));
		float16 = !!(ps->info.ps.float16_shaded_mask & (1u << ps_offset));

		ps_input_cntl[ps_offset] = offset_to_ps_input(vs_offset, flat_shade, explicit, float16);
		++ps_offset;
	}

	if (ps_offset) {
		radeon_set_context_reg_seq(ctx_cs, R_028644_SPI_PS_INPUT_CNTL_0, ps_offset);
		for (unsigned i = 0; i < ps_offset; i++) {
			radeon_emit(ctx_cs, ps_input_cntl[i]);
		}
	}
}

static uint32_t
radv_compute_db_shader_control(const struct radv_device *device,
			       const struct radv_pipeline *pipeline,
                               const struct radv_shader_variant *ps)
{
	unsigned conservative_z_export = V_02880C_EXPORT_ANY_Z;
	unsigned z_order;
	if (ps->info.ps.early_fragment_test || !ps->info.ps.writes_memory)
		z_order = V_02880C_EARLY_Z_THEN_LATE_Z;
	else
		z_order = V_02880C_LATE_Z;

	if (ps->info.ps.depth_layout == FRAG_DEPTH_LAYOUT_GREATER)
		conservative_z_export = V_02880C_EXPORT_GREATER_THAN_Z;
	else if (ps->info.ps.depth_layout == FRAG_DEPTH_LAYOUT_LESS)
		conservative_z_export = V_02880C_EXPORT_LESS_THAN_Z;

	bool disable_rbplus = device->physical_device->rad_info.has_rbplus &&
	                      !device->physical_device->rad_info.rbplus_allowed;

	/* It shouldn't be needed to export gl_SampleMask when MSAA is disabled
	 * but this appears to break Project Cars (DXVK). See
	 * https://bugs.freedesktop.org/show_bug.cgi?id=109401
	 */
	bool mask_export_enable = ps->info.ps.writes_sample_mask;

	return  S_02880C_Z_EXPORT_ENABLE(ps->info.ps.writes_z) |
		S_02880C_STENCIL_TEST_VAL_EXPORT_ENABLE(ps->info.ps.writes_stencil) |
		S_02880C_KILL_ENABLE(!!ps->info.ps.can_discard) |
		S_02880C_MASK_EXPORT_ENABLE(mask_export_enable) |
		S_02880C_CONSERVATIVE_Z_EXPORT(conservative_z_export) |
		S_02880C_Z_ORDER(z_order) |
		S_02880C_DEPTH_BEFORE_SHADER(ps->info.ps.early_fragment_test) |
		S_02880C_PRE_SHADER_DEPTH_COVERAGE_ENABLE(ps->info.ps.post_depth_coverage) |
		S_02880C_EXEC_ON_HIER_FAIL(ps->info.ps.writes_memory) |
		S_02880C_EXEC_ON_NOOP(ps->info.ps.writes_memory) |
		S_02880C_DUAL_QUAD_DISABLE(disable_rbplus);
}

static void
radv_pipeline_generate_fragment_shader(struct radeon_cmdbuf *ctx_cs,
				       struct radeon_cmdbuf *cs,
				       struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *ps;
	uint64_t va;
	assert (pipeline->shaders[MESA_SHADER_FRAGMENT]);

	ps = pipeline->shaders[MESA_SHADER_FRAGMENT];
	va = radv_buffer_get_va(ps->bo) + ps->bo_offset;

	radeon_set_sh_reg_seq(cs, R_00B020_SPI_SHADER_PGM_LO_PS, 4);
	radeon_emit(cs, va >> 8);
	radeon_emit(cs, S_00B024_MEM_BASE(va >> 40));
	radeon_emit(cs, ps->config.rsrc1);
	radeon_emit(cs, ps->config.rsrc2);

	radeon_set_context_reg(ctx_cs, R_02880C_DB_SHADER_CONTROL,
	                       radv_compute_db_shader_control(pipeline->device,
							      pipeline, ps));

	radeon_set_context_reg(ctx_cs, R_0286CC_SPI_PS_INPUT_ENA,
			       ps->config.spi_ps_input_ena);

	radeon_set_context_reg(ctx_cs, R_0286D0_SPI_PS_INPUT_ADDR,
			       ps->config.spi_ps_input_addr);

	radeon_set_context_reg(ctx_cs, R_0286D8_SPI_PS_IN_CONTROL,
			       S_0286D8_NUM_INTERP(ps->info.ps.num_interp) |
			       S_0286D8_PS_W32_EN(ps->info.wave_size == 32));

	radeon_set_context_reg(ctx_cs, R_0286E0_SPI_BARYC_CNTL, pipeline->graphics.spi_baryc_cntl);

	radeon_set_context_reg(ctx_cs, R_028710_SPI_SHADER_Z_FORMAT,
	                       ac_get_spi_shader_z_format(ps->info.ps.writes_z,
	                                                  ps->info.ps.writes_stencil,
	                                                  ps->info.ps.writes_sample_mask));

	if (pipeline->device->dfsm_allowed) {
		/* optimise this? */
		radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
		radeon_emit(cs, EVENT_TYPE(V_028A90_FLUSH_DFSM) | EVENT_INDEX(0));
	}
}

static void
radv_pipeline_generate_vgt_vertex_reuse(struct radeon_cmdbuf *ctx_cs,
					const struct radv_pipeline *pipeline)
{
	if (pipeline->device->physical_device->rad_info.family < CHIP_POLARIS10 ||
	    pipeline->device->physical_device->rad_info.chip_class >= GFX10)
		return;

	unsigned vtx_reuse_depth = 30;
	if (radv_pipeline_has_tess(pipeline) &&
	    radv_get_shader(pipeline, MESA_SHADER_TESS_EVAL)->info.tes.spacing == TESS_SPACING_FRACTIONAL_ODD) {
		vtx_reuse_depth = 14;
	}
	radeon_set_context_reg(ctx_cs, R_028C58_VGT_VERTEX_REUSE_BLOCK_CNTL,
	                       S_028C58_VTX_REUSE_DEPTH(vtx_reuse_depth));
}

static void
radv_pipeline_generate_vgt_shader_config(struct radeon_cmdbuf *ctx_cs,
					 const struct radv_pipeline *pipeline)
{
	uint32_t stages = 0;
	if (radv_pipeline_has_tess(pipeline)) {
		stages |= S_028B54_LS_EN(V_028B54_LS_STAGE_ON) |
			S_028B54_HS_EN(1) | S_028B54_DYNAMIC_HS(1);

		if (radv_pipeline_has_gs(pipeline))
			stages |= S_028B54_ES_EN(V_028B54_ES_STAGE_DS) |
				  S_028B54_GS_EN(1);
		else if (radv_pipeline_has_ngg(pipeline))
			stages |= S_028B54_ES_EN(V_028B54_ES_STAGE_DS);
		else
			stages |= S_028B54_VS_EN(V_028B54_VS_STAGE_DS);
	} else if (radv_pipeline_has_gs(pipeline)) {
		stages |= S_028B54_ES_EN(V_028B54_ES_STAGE_REAL) |
			S_028B54_GS_EN(1);
	} else if (radv_pipeline_has_ngg(pipeline)) {
		stages |= S_028B54_ES_EN(V_028B54_ES_STAGE_REAL);
	}

	if (radv_pipeline_has_ngg(pipeline)) {
		stages |= S_028B54_PRIMGEN_EN(1);
		if (pipeline->streamout_shader)
			stages |= S_028B54_NGG_WAVE_ID_EN(1);
		if (radv_pipeline_has_ngg_passthrough(pipeline))
			stages |= S_028B54_PRIMGEN_PASSTHRU_EN(1);
	} else if (radv_pipeline_has_gs(pipeline)) {
		stages |= S_028B54_VS_EN(V_028B54_VS_STAGE_COPY_SHADER);
	}

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX9)
		stages |= S_028B54_MAX_PRIMGRP_IN_WAVE(2);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10) {
		uint8_t hs_size = 64, gs_size = 64, vs_size = 64;

		if (radv_pipeline_has_tess(pipeline))
			hs_size = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.wave_size;

		if (pipeline->shaders[MESA_SHADER_GEOMETRY]) {
			vs_size = gs_size = pipeline->shaders[MESA_SHADER_GEOMETRY]->info.wave_size;
			if (pipeline->gs_copy_shader)
				vs_size = pipeline->gs_copy_shader->info.wave_size;
		} else if (pipeline->shaders[MESA_SHADER_TESS_EVAL])
			vs_size = pipeline->shaders[MESA_SHADER_TESS_EVAL]->info.wave_size;
		else if (pipeline->shaders[MESA_SHADER_VERTEX])
			vs_size = pipeline->shaders[MESA_SHADER_VERTEX]->info.wave_size;

		if (radv_pipeline_has_ngg(pipeline))
			gs_size = vs_size;

		/* legacy GS only supports Wave64 */
		stages |= S_028B54_HS_W32_EN(hs_size == 32 ? 1 : 0) |
			  S_028B54_GS_W32_EN(gs_size == 32 ? 1 : 0) |
			  S_028B54_VS_W32_EN(vs_size == 32 ? 1 : 0);
	}

	radeon_set_context_reg(ctx_cs, R_028B54_VGT_SHADER_STAGES_EN, stages);
}

static void
radv_pipeline_generate_cliprect_rule(struct radeon_cmdbuf *ctx_cs,
				     const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const  VkPipelineDiscardRectangleStateCreateInfoEXT *discard_rectangle_info =
			vk_find_struct_const(pCreateInfo->pNext, PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT);
	uint32_t cliprect_rule = 0;

	if (!discard_rectangle_info) {
		cliprect_rule = 0xffff;
	} else {
		for (unsigned i = 0; i < (1u << MAX_DISCARD_RECTANGLES); ++i) {
			/* Interpret i as a bitmask, and then set the bit in
			 * the mask if that combination of rectangles in which
			 * the pixel is contained should pass the cliprect
			 * test.
			 */
			unsigned relevant_subset = i & ((1u << discard_rectangle_info->discardRectangleCount) - 1);

			if (discard_rectangle_info->discardRectangleMode == VK_DISCARD_RECTANGLE_MODE_INCLUSIVE_EXT &&
			    !relevant_subset)
				continue;

			if (discard_rectangle_info->discardRectangleMode == VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT &&
			    relevant_subset)
				continue;

			cliprect_rule |= 1u << i;
		}
	}

	radeon_set_context_reg(ctx_cs, R_02820C_PA_SC_CLIPRECT_RULE, cliprect_rule);
}

static void
gfx10_pipeline_generate_ge_cntl(struct radeon_cmdbuf *ctx_cs,
				struct radv_pipeline *pipeline)
{
	bool break_wave_at_eoi = false;
	unsigned primgroup_size;
	unsigned vertgroup_size = 256; /* 256 = disable vertex grouping */

	if (radv_pipeline_has_tess(pipeline)) {
		primgroup_size = pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.tcs.num_patches;
	} else if (radv_pipeline_has_gs(pipeline)) {
		const struct gfx9_gs_info *gs_state =
			&pipeline->shaders[MESA_SHADER_GEOMETRY]->info.gs_ring_info;
		unsigned vgt_gs_onchip_cntl = gs_state->vgt_gs_onchip_cntl;
		primgroup_size = G_028A44_GS_PRIMS_PER_SUBGRP(vgt_gs_onchip_cntl);
	} else {
		primgroup_size = 128; /* recommended without a GS and tess */
	}

	if (radv_pipeline_has_tess(pipeline)) {
		if (pipeline->shaders[MESA_SHADER_TESS_CTRL]->info.uses_prim_id ||
		    radv_get_shader(pipeline, MESA_SHADER_TESS_EVAL)->info.uses_prim_id)
			break_wave_at_eoi = true;
	}

	radeon_set_uconfig_reg(ctx_cs, R_03096C_GE_CNTL,
			       S_03096C_PRIM_GRP_SIZE(primgroup_size) |
			       S_03096C_VERT_GRP_SIZE(vertgroup_size) |
			       S_03096C_PACKET_TO_ONE_PA(0) /* line stipple */ |
			       S_03096C_BREAK_WAVE_AT_EOI(break_wave_at_eoi));
}

static void
radv_pipeline_generate_vgt_gs_out(struct radeon_cmdbuf *ctx_cs,
				  const struct radv_pipeline *pipeline,
				  const VkGraphicsPipelineCreateInfo *pCreateInfo,
				  const struct radv_graphics_pipeline_create_info *extra)
{
	uint32_t gs_out;

	if (radv_pipeline_has_gs(pipeline)) {
		gs_out = si_conv_gl_prim_to_gs_out(pipeline->shaders[MESA_SHADER_GEOMETRY]->info.gs.output_prim);
	} else if (radv_pipeline_has_tess(pipeline)) {
		if (pipeline->shaders[MESA_SHADER_TESS_EVAL]->info.tes.point_mode) {
			gs_out = V_028A6C_POINTLIST;
		} else {
			gs_out = si_conv_gl_prim_to_gs_out(pipeline->shaders[MESA_SHADER_TESS_EVAL]->info.tes.primitive_mode);
		}
	} else {
		gs_out = si_conv_prim_to_gs_out(pCreateInfo->pInputAssemblyState->topology);
	}

	if (extra && extra->use_rectlist) {
		gs_out = V_028A6C_TRISTRIP;
		if (radv_pipeline_has_ngg(pipeline))
			gs_out = V_028A6C_RECTLIST;
	}

	radeon_set_context_reg(ctx_cs, R_028A6C_VGT_GS_OUT_PRIM_TYPE, gs_out);
}

static void
radv_pipeline_generate_pm4(struct radv_pipeline *pipeline,
                           const VkGraphicsPipelineCreateInfo *pCreateInfo,
                           const struct radv_graphics_pipeline_create_info *extra,
                           const struct radv_blend_state *blend)
{
	struct radeon_cmdbuf *ctx_cs = &pipeline->ctx_cs;
	struct radeon_cmdbuf *cs = &pipeline->cs;

	cs->max_dw = 64;
	ctx_cs->max_dw = 256;
	cs->buf = malloc(4 * (cs->max_dw + ctx_cs->max_dw));
	ctx_cs->buf = cs->buf + cs->max_dw;

	radv_pipeline_generate_depth_stencil_state(ctx_cs, pipeline, pCreateInfo, extra);
	radv_pipeline_generate_blend_state(ctx_cs, pipeline, blend);
	radv_pipeline_generate_raster_state(ctx_cs, pipeline, pCreateInfo);
	radv_pipeline_generate_multisample_state(ctx_cs, pipeline);
	radv_pipeline_generate_vgt_gs_mode(ctx_cs, pipeline);
	radv_pipeline_generate_vertex_shader(ctx_cs, cs, pipeline);

	if (radv_pipeline_has_tess(pipeline)) {
		radv_pipeline_generate_tess_shaders(ctx_cs, cs, pipeline);
		radv_pipeline_generate_tess_state(ctx_cs, pipeline, pCreateInfo);
	}

	radv_pipeline_generate_geometry_shader(ctx_cs, cs, pipeline);
	radv_pipeline_generate_fragment_shader(ctx_cs, cs, pipeline);
	radv_pipeline_generate_ps_inputs(ctx_cs, pipeline);
	radv_pipeline_generate_vgt_vertex_reuse(ctx_cs, pipeline);
	radv_pipeline_generate_vgt_shader_config(ctx_cs, pipeline);
	radv_pipeline_generate_cliprect_rule(ctx_cs, pCreateInfo);
	radv_pipeline_generate_vgt_gs_out(ctx_cs, pipeline, pCreateInfo, extra);

	if (pipeline->device->physical_device->rad_info.chip_class >= GFX10 && !radv_pipeline_has_ngg(pipeline))
		gfx10_pipeline_generate_ge_cntl(ctx_cs, pipeline);

	pipeline->ctx_cs_hash = _mesa_hash_data(ctx_cs->buf, ctx_cs->cdw * 4);

	assert(ctx_cs->cdw <= ctx_cs->max_dw);
	assert(cs->cdw <= cs->max_dw);
}

static void
radv_pipeline_init_vertex_input_state(struct radv_pipeline *pipeline,
				      const VkGraphicsPipelineCreateInfo *pCreateInfo)
{
	const VkPipelineVertexInputStateCreateInfo *vi_info =
		pCreateInfo->pVertexInputState;

	for (uint32_t i = 0; i < vi_info->vertexBindingDescriptionCount; i++) {
		const VkVertexInputBindingDescription *desc =
			&vi_info->pVertexBindingDescriptions[i];

		pipeline->binding_stride[desc->binding] = desc->stride;
		pipeline->num_vertex_bindings =
			MAX2(pipeline->num_vertex_bindings, desc->binding + 1);
	}
}

static struct radv_shader_variant *
radv_pipeline_get_streamout_shader(struct radv_pipeline *pipeline)
{
	int i;

	for (i = MESA_SHADER_GEOMETRY; i >= MESA_SHADER_VERTEX; i--) {
		struct radv_shader_variant *shader =
			radv_get_shader(pipeline, i);

		if (shader && shader->info.so.num_outputs > 0)
			return shader;
	}

	return NULL;
}

static void
radv_pipeline_init_shader_stages_state(struct radv_pipeline *pipeline)
{
	struct radv_device *device = pipeline->device;

	for (unsigned i = 0; i < MESA_SHADER_STAGES; i++) {
		pipeline->user_data_0[i] =
			radv_pipeline_stage_to_user_data_0(pipeline, i,
							   device->physical_device->rad_info.chip_class);

		if (pipeline->shaders[i]) {
			pipeline->need_indirect_descriptor_sets |= pipeline->shaders[i]->info.need_indirect_descriptor_sets;
		}
	}

	struct radv_userdata_info *loc = radv_lookup_user_sgpr(pipeline, MESA_SHADER_VERTEX,
							     AC_UD_VS_BASE_VERTEX_START_INSTANCE);
	if (loc->sgpr_idx != -1) {
		pipeline->graphics.vtx_base_sgpr = pipeline->user_data_0[MESA_SHADER_VERTEX];
		pipeline->graphics.vtx_base_sgpr += loc->sgpr_idx * 4;
		if (radv_get_shader(pipeline, MESA_SHADER_VERTEX)->info.vs.needs_draw_id)
			pipeline->graphics.vtx_emit_num = 3;
		else
			pipeline->graphics.vtx_emit_num = 2;
	}
}

static VkResult
radv_pipeline_init(struct radv_pipeline *pipeline,
		   struct radv_device *device,
		   struct radv_pipeline_cache *cache,
		   const VkGraphicsPipelineCreateInfo *pCreateInfo,
		   const struct radv_graphics_pipeline_create_info *extra)
{
	VkResult result;

	pipeline->device = device;
	pipeline->layout = radv_pipeline_layout_from_handle(pCreateInfo->layout);
	assert(pipeline->layout);

	struct radv_blend_state blend = radv_pipeline_init_blend_state(pipeline, pCreateInfo, extra);

	const VkPipelineCreationFeedbackCreateInfoEXT *creation_feedback =
		vk_find_struct_const(pCreateInfo->pNext, PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT);
	radv_init_feedback(creation_feedback);

	VkPipelineCreationFeedbackEXT *pipeline_feedback = creation_feedback ? creation_feedback->pPipelineCreationFeedback : NULL;

	const VkPipelineShaderStageCreateInfo *pStages[MESA_SHADER_STAGES] = { 0, };
	VkPipelineCreationFeedbackEXT *stage_feedbacks[MESA_SHADER_STAGES] = { 0 };
	for (uint32_t i = 0; i < pCreateInfo->stageCount; i++) {
		gl_shader_stage stage = ffs(pCreateInfo->pStages[i].stage) - 1;
		pStages[stage] = &pCreateInfo->pStages[i];
		if(creation_feedback)
			stage_feedbacks[stage] = &creation_feedback->pPipelineStageCreationFeedbacks[i];
	}

	struct radv_pipeline_key key = radv_generate_graphics_pipeline_key(pipeline, pCreateInfo, &blend);

	result = radv_create_shaders(pipeline, device, cache, &key, pStages,
		                     pCreateInfo->flags, pipeline_feedback,
				     stage_feedbacks);
	if (result != VK_SUCCESS)
		return result;

	pipeline->graphics.spi_baryc_cntl = S_0286E0_FRONT_FACE_ALL_BITS(1);
	radv_pipeline_init_multisample_state(pipeline, &blend, pCreateInfo);
	radv_pipeline_init_input_assembly_state(pipeline, pCreateInfo, extra);
	radv_pipeline_init_dynamic_state(pipeline, pCreateInfo, extra);
	radv_pipeline_init_raster_state(pipeline, pCreateInfo);
	radv_pipeline_init_depth_stencil_state(pipeline, pCreateInfo);

	/* Ensure that some export memory is always allocated, for two reasons:
	 *
	 * 1) Correctness: The hardware ignores the EXEC mask if no export
	 *    memory is allocated, so KILL and alpha test do not work correctly
	 *    without this.
	 * 2) Performance: Every shader needs at least a NULL export, even when
	 *    it writes no color/depth output. The NULL export instruction
	 *    stalls without this setting.
	 *
	 * Don't add this to CB_SHADER_MASK.
	 *
	 * GFX10 supports pixel shaders without exports by setting both the
	 * color and Z formats to SPI_SHADER_ZERO. The hw will skip export
	 * instructions if any are present.
	 */
	struct radv_shader_variant *ps = pipeline->shaders[MESA_SHADER_FRAGMENT];
	if ((pipeline->device->physical_device->rad_info.chip_class <= GFX9 ||
	     ps->info.ps.can_discard) &&
	    !blend.spi_shader_col_format) {
		if (!ps->info.ps.writes_z &&
		    !ps->info.ps.writes_stencil &&
		    !ps->info.ps.writes_sample_mask)
			blend.spi_shader_col_format = V_028714_SPI_SHADER_32_R;
	}

	blend.cb_shader_mask = ps->info.ps.cb_shader_mask;

	if (extra &&
	    (extra->custom_blend_mode == V_028808_CB_ELIMINATE_FAST_CLEAR ||
	     extra->custom_blend_mode == V_028808_CB_FMASK_DECOMPRESS ||
	     extra->custom_blend_mode == V_028808_CB_DCC_DECOMPRESS ||
	     extra->custom_blend_mode == V_028808_CB_RESOLVE)) {
		/* According to the CB spec states, CB_SHADER_MASK should be
		 * set to enable writes to all four channels of MRT0.
		 */
		blend.cb_shader_mask = 0xf;
	}

	pipeline->graphics.col_format = blend.spi_shader_col_format;
	pipeline->graphics.cb_target_mask = blend.cb_target_mask;

	if (radv_pipeline_has_gs(pipeline) && !radv_pipeline_has_ngg(pipeline)) {
		struct radv_shader_variant *gs =
			pipeline->shaders[MESA_SHADER_GEOMETRY];

		radv_pipeline_init_gs_ring_state(pipeline, &gs->info.gs_ring_info);
	}

	if (radv_pipeline_has_tess(pipeline)) {
		pipeline->graphics.tess_patch_control_points =
			pCreateInfo->pTessellationState->patchControlPoints;
	}

	radv_pipeline_init_vertex_input_state(pipeline, pCreateInfo);
	radv_pipeline_init_binning_state(pipeline, pCreateInfo, &blend);
	radv_pipeline_init_shader_stages_state(pipeline);
	radv_pipeline_init_scratch(device, pipeline);

	/* Find the last vertex shader stage that eventually uses streamout. */
	pipeline->streamout_shader = radv_pipeline_get_streamout_shader(pipeline);

	radv_pipeline_generate_pm4(pipeline, pCreateInfo, extra, &blend);

	return result;
}

VkResult
radv_graphics_pipeline_create(
	VkDevice _device,
	VkPipelineCache _cache,
	const VkGraphicsPipelineCreateInfo *pCreateInfo,
	const struct radv_graphics_pipeline_create_info *extra,
	const VkAllocationCallbacks *pAllocator,
	VkPipeline *pPipeline)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_pipeline_cache, cache, _cache);
	struct radv_pipeline *pipeline;
	VkResult result;

	pipeline = vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*pipeline), 8,
			      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (pipeline == NULL)
		return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

	vk_object_base_init(&device->vk, &pipeline->base,
			    VK_OBJECT_TYPE_PIPELINE);

	result = radv_pipeline_init(pipeline, device, cache,
				    pCreateInfo, extra);
	if (result != VK_SUCCESS) {
		radv_pipeline_destroy(device, pipeline, pAllocator);
		return result;
	}

	*pPipeline = radv_pipeline_to_handle(pipeline);

	return VK_SUCCESS;
}

VkResult radv_CreateGraphicsPipelines(
	VkDevice                                    _device,
	VkPipelineCache                             pipelineCache,
	uint32_t                                    count,
	const VkGraphicsPipelineCreateInfo*         pCreateInfos,
	const VkAllocationCallbacks*                pAllocator,
	VkPipeline*                                 pPipelines)
{
	VkResult result = VK_SUCCESS;
	unsigned i = 0;

	for (; i < count; i++) {
		VkResult r;
		r = radv_graphics_pipeline_create(_device,
						  pipelineCache,
						  &pCreateInfos[i],
						  NULL, pAllocator, &pPipelines[i]);
		if (r != VK_SUCCESS) {
			result = r;
			pPipelines[i] = VK_NULL_HANDLE;

			if (pCreateInfos[i].flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
				break;
		}
	}

	for (; i < count; ++i)
		pPipelines[i] = VK_NULL_HANDLE;

	return result;
}

static void
radv_pipeline_generate_hw_cs(struct radeon_cmdbuf *cs,
			     const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *shader = pipeline->shaders[MESA_SHADER_COMPUTE];
	uint64_t va = radv_buffer_get_va(shader->bo) + shader->bo_offset;
	struct radv_device *device = pipeline->device;

	radeon_set_sh_reg_seq(cs, R_00B830_COMPUTE_PGM_LO, 2);
	radeon_emit(cs, va >> 8);
	radeon_emit(cs, S_00B834_DATA(va >> 40));

	radeon_set_sh_reg_seq(cs, R_00B848_COMPUTE_PGM_RSRC1, 2);
	radeon_emit(cs, shader->config.rsrc1);
	radeon_emit(cs, shader->config.rsrc2);
	if (device->physical_device->rad_info.chip_class >= GFX10) {
		radeon_set_sh_reg(cs, R_00B8A0_COMPUTE_PGM_RSRC3, shader->config.rsrc3);
	}
}

static void
radv_pipeline_generate_compute_state(struct radeon_cmdbuf *cs,
				     const struct radv_pipeline *pipeline)
{
	struct radv_shader_variant *shader = pipeline->shaders[MESA_SHADER_COMPUTE];
	struct radv_device *device = pipeline->device;
	unsigned threads_per_threadgroup;
	unsigned threadgroups_per_cu = 1;
	unsigned waves_per_threadgroup;
	unsigned max_waves_per_sh = 0;

	/* Calculate best compute resource limits. */
	threads_per_threadgroup = shader->info.cs.block_size[0] *
				  shader->info.cs.block_size[1] *
				  shader->info.cs.block_size[2];
	waves_per_threadgroup = DIV_ROUND_UP(threads_per_threadgroup,
					     shader->info.wave_size);

	if (device->physical_device->rad_info.chip_class >= GFX10 &&
	    waves_per_threadgroup == 1)
		threadgroups_per_cu = 2;

	radeon_set_sh_reg(cs, R_00B854_COMPUTE_RESOURCE_LIMITS,
			  ac_get_compute_resource_limits(&device->physical_device->rad_info,
							 waves_per_threadgroup,
							 max_waves_per_sh,
							 threadgroups_per_cu));

	radeon_set_sh_reg_seq(cs, R_00B81C_COMPUTE_NUM_THREAD_X, 3);
	radeon_emit(cs, S_00B81C_NUM_THREAD_FULL(shader->info.cs.block_size[0]));
	radeon_emit(cs, S_00B81C_NUM_THREAD_FULL(shader->info.cs.block_size[1]));
	radeon_emit(cs, S_00B81C_NUM_THREAD_FULL(shader->info.cs.block_size[2]));
}

static void
radv_compute_generate_pm4(struct radv_pipeline *pipeline)
{
	struct radv_device *device = pipeline->device;
	struct radeon_cmdbuf *cs = &pipeline->cs;

	cs->max_dw = device->physical_device->rad_info.chip_class >= GFX10 ? 19 : 16;
	cs->buf = malloc(cs->max_dw * 4);

	radv_pipeline_generate_hw_cs(cs, pipeline);
	radv_pipeline_generate_compute_state(cs, pipeline);

	assert(pipeline->cs.cdw <= pipeline->cs.max_dw);
}

static struct radv_pipeline_key
radv_generate_compute_pipeline_key(struct radv_pipeline *pipeline,
				   const VkComputePipelineCreateInfo *pCreateInfo)
{
	const VkPipelineShaderStageCreateInfo *stage = &pCreateInfo->stage;
	struct radv_pipeline_key key;
	memset(&key, 0, sizeof(key));

	if (pCreateInfo->flags & VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT)
		key.optimisations_disabled = 1;

	const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT *subgroup_size =
		vk_find_struct_const(stage->pNext,
				     PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT);

	if (subgroup_size) {
		assert(subgroup_size->requiredSubgroupSize == 32 ||
		       subgroup_size->requiredSubgroupSize == 64);
		key.compute_subgroup_size = subgroup_size->requiredSubgroupSize;
	}

	return key;
}

static VkResult radv_compute_pipeline_create(
	VkDevice                                    _device,
	VkPipelineCache                             _cache,
	const VkComputePipelineCreateInfo*          pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkPipeline*                                 pPipeline)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_pipeline_cache, cache, _cache);
	const VkPipelineShaderStageCreateInfo *pStages[MESA_SHADER_STAGES] = { 0, };
	VkPipelineCreationFeedbackEXT *stage_feedbacks[MESA_SHADER_STAGES] = { 0 };
	struct radv_pipeline *pipeline;
	VkResult result;

	pipeline = vk_zalloc2(&device->vk.alloc, pAllocator, sizeof(*pipeline), 8,
			      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (pipeline == NULL)
		return vk_error(device->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

	vk_object_base_init(&device->vk, &pipeline->base,
			    VK_OBJECT_TYPE_PIPELINE);

	pipeline->device = device;
	pipeline->layout = radv_pipeline_layout_from_handle(pCreateInfo->layout);
	assert(pipeline->layout);

	const VkPipelineCreationFeedbackCreateInfoEXT *creation_feedback =
		vk_find_struct_const(pCreateInfo->pNext, PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT);
	radv_init_feedback(creation_feedback);

	VkPipelineCreationFeedbackEXT *pipeline_feedback = creation_feedback ? creation_feedback->pPipelineCreationFeedback : NULL;
	if (creation_feedback)
		stage_feedbacks[MESA_SHADER_COMPUTE] = &creation_feedback->pPipelineStageCreationFeedbacks[0];

	pStages[MESA_SHADER_COMPUTE] = &pCreateInfo->stage;

	struct radv_pipeline_key key =
		radv_generate_compute_pipeline_key(pipeline, pCreateInfo);

	result = radv_create_shaders(pipeline, device, cache, &key, pStages,
		                     pCreateInfo->flags, pipeline_feedback,
				     stage_feedbacks);
	if (result != VK_SUCCESS) {
		radv_pipeline_destroy(device, pipeline, pAllocator);
		return result;
	}

	pipeline->user_data_0[MESA_SHADER_COMPUTE] = radv_pipeline_stage_to_user_data_0(pipeline, MESA_SHADER_COMPUTE, device->physical_device->rad_info.chip_class);
	pipeline->need_indirect_descriptor_sets |= pipeline->shaders[MESA_SHADER_COMPUTE]->info.need_indirect_descriptor_sets;
	radv_pipeline_init_scratch(device, pipeline);

	radv_compute_generate_pm4(pipeline);

	*pPipeline = radv_pipeline_to_handle(pipeline);

	return VK_SUCCESS;
}

VkResult radv_CreateComputePipelines(
	VkDevice                                    _device,
	VkPipelineCache                             pipelineCache,
	uint32_t                                    count,
	const VkComputePipelineCreateInfo*          pCreateInfos,
	const VkAllocationCallbacks*                pAllocator,
	VkPipeline*                                 pPipelines)
{
	VkResult result = VK_SUCCESS;

	unsigned i = 0;
	for (; i < count; i++) {
		VkResult r;
		r = radv_compute_pipeline_create(_device, pipelineCache,
						 &pCreateInfos[i],
						 pAllocator, &pPipelines[i]);
		if (r != VK_SUCCESS) {
			result = r;
			pPipelines[i] = VK_NULL_HANDLE;

			if (pCreateInfos[i].flags & VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT)
				break;
		}
	}

	for (; i < count; ++i)
		pPipelines[i] = VK_NULL_HANDLE;

	return result;
}


static uint32_t radv_get_executable_count(const struct radv_pipeline *pipeline)
{
	uint32_t ret = 0;
	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (!pipeline->shaders[i])
			continue;

		if (i == MESA_SHADER_GEOMETRY &&
		    !radv_pipeline_has_ngg(pipeline)) {
			ret += 2u;
		} else {
			ret += 1u;
		}

	}
	return ret;
}

static struct radv_shader_variant *
radv_get_shader_from_executable_index(const struct radv_pipeline *pipeline, int index, gl_shader_stage *stage)
{
	for (int i = 0; i < MESA_SHADER_STAGES; ++i) {
		if (!pipeline->shaders[i])
			continue;
		if (!index) {
			*stage = i;
			return pipeline->shaders[i];
		}

		--index;

		if (i == MESA_SHADER_GEOMETRY &&
		    !radv_pipeline_has_ngg(pipeline)) {
			if (!index) {
				*stage = i;
				return pipeline->gs_copy_shader;
			}
			--index;
		}
	}

	*stage = -1;
	return NULL;
}

/* Basically strlcpy (which does not exist on linux) specialized for
 * descriptions. */
static void desc_copy(char *desc, const char *src) {
	int len = strlen(src);
	assert(len < VK_MAX_DESCRIPTION_SIZE);
	memcpy(desc, src, len);
	memset(desc + len, 0, VK_MAX_DESCRIPTION_SIZE - len);
}

VkResult radv_GetPipelineExecutablePropertiesKHR(
    VkDevice                                    _device,
    const VkPipelineInfoKHR*                    pPipelineInfo,
    uint32_t*                                   pExecutableCount,
    VkPipelineExecutablePropertiesKHR*          pProperties)
{
	RADV_FROM_HANDLE(radv_pipeline, pipeline, pPipelineInfo->pipeline);
	const uint32_t total_count = radv_get_executable_count(pipeline);

	if (!pProperties) {
		*pExecutableCount = total_count;
		return VK_SUCCESS;
	}

	const uint32_t count = MIN2(total_count, *pExecutableCount);
	for (unsigned i = 0, executable_idx = 0;
	     i < MESA_SHADER_STAGES && executable_idx < count; ++i) {
		if (!pipeline->shaders[i])
			continue;
		pProperties[executable_idx].stages = mesa_to_vk_shader_stage(i);
		const char *name = NULL;
		const char *description = NULL;
		switch(i) {
		case MESA_SHADER_VERTEX:
			name = "Vertex Shader";
			description = "Vulkan Vertex Shader";
			break;
		case MESA_SHADER_TESS_CTRL:
			if (!pipeline->shaders[MESA_SHADER_VERTEX]) {
				pProperties[executable_idx].stages |= VK_SHADER_STAGE_VERTEX_BIT;
				name = "Vertex + Tessellation Control Shaders";
				description = "Combined Vulkan Vertex and Tessellation Control Shaders";
			} else {
				name = "Tessellation Control Shader";
				description = "Vulkan Tessellation Control Shader";
			}
			break;
		case MESA_SHADER_TESS_EVAL:
			name = "Tessellation Evaluation Shader";
			description = "Vulkan Tessellation Evaluation Shader";
			break;
		case MESA_SHADER_GEOMETRY:
			if (radv_pipeline_has_tess(pipeline) && !pipeline->shaders[MESA_SHADER_TESS_EVAL]) {
				pProperties[executable_idx].stages |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
				name = "Tessellation Evaluation + Geometry Shaders";
				description = "Combined Vulkan Tessellation Evaluation and Geometry Shaders";
			} else if (!radv_pipeline_has_tess(pipeline) && !pipeline->shaders[MESA_SHADER_VERTEX]) {
				pProperties[executable_idx].stages |= VK_SHADER_STAGE_VERTEX_BIT;
				name = "Vertex + Geometry Shader";
				description = "Combined Vulkan Vertex and Geometry Shaders";
			} else {
				name = "Geometry Shader";
				description = "Vulkan Geometry Shader";
			}
			break;
		case MESA_SHADER_FRAGMENT:
			name = "Fragment Shader";
			description = "Vulkan Fragment Shader";
			break;
		case MESA_SHADER_COMPUTE:
			name = "Compute Shader";
			description = "Vulkan Compute Shader";
			break;
		}

		pProperties[executable_idx].subgroupSize = pipeline->shaders[i]->info.wave_size;
		desc_copy(pProperties[executable_idx].name, name);
		desc_copy(pProperties[executable_idx].description, description);

		++executable_idx;
		if (i == MESA_SHADER_GEOMETRY &&
		    !radv_pipeline_has_ngg(pipeline)) {
			assert(pipeline->gs_copy_shader);
			if (executable_idx >= count)
				break;

			pProperties[executable_idx].stages = VK_SHADER_STAGE_GEOMETRY_BIT;
			pProperties[executable_idx].subgroupSize = 64;
			desc_copy(pProperties[executable_idx].name, "GS Copy Shader");
			desc_copy(pProperties[executable_idx].description,
				  "Extra shader stage that loads the GS output ringbuffer into the rasterizer");

			++executable_idx;
		}
	}

	VkResult result = *pExecutableCount < total_count ? VK_INCOMPLETE : VK_SUCCESS;
	*pExecutableCount = count;
	return result;
}

VkResult radv_GetPipelineExecutableStatisticsKHR(
    VkDevice                                    _device,
    const VkPipelineExecutableInfoKHR*          pExecutableInfo,
    uint32_t*                                   pStatisticCount,
    VkPipelineExecutableStatisticKHR*           pStatistics)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_pipeline, pipeline, pExecutableInfo->pipeline);
	gl_shader_stage stage;
	struct radv_shader_variant *shader = radv_get_shader_from_executable_index(pipeline, pExecutableInfo->executableIndex, &stage);

	enum chip_class chip_class = device->physical_device->rad_info.chip_class;
	unsigned lds_increment = chip_class >= GFX7 ? 512 : 256;
	unsigned max_waves = radv_get_max_waves(device, shader, stage);

	VkPipelineExecutableStatisticKHR *s = pStatistics;
	VkPipelineExecutableStatisticKHR *end = s + (pStatistics ? *pStatisticCount : 0);
	VkResult result = VK_SUCCESS;

	if (s < end) {
		desc_copy(s->name, "SGPRs");
		desc_copy(s->description, "Number of SGPR registers allocated per subgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->config.num_sgprs;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "VGPRs");
		desc_copy(s->description, "Number of VGPR registers allocated per subgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->config.num_vgprs;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "Spilled SGPRs");
		desc_copy(s->description, "Number of SGPR registers spilled per subgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->config.spilled_sgprs;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "Spilled VGPRs");
		desc_copy(s->description, "Number of VGPR registers spilled per subgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->config.spilled_vgprs;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "PrivMem VGPRs");
		desc_copy(s->description, "Number of VGPRs stored in private memory per subgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->info.private_mem_vgprs;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "Code size");
		desc_copy(s->description, "Code size in bytes");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->exec_size;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "LDS size");
		desc_copy(s->description, "LDS size in bytes per workgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->config.lds_size * lds_increment;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "Scratch size");
		desc_copy(s->description, "Private memory in bytes per subgroup");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = shader->config.scratch_bytes_per_wave;
	}
	++s;

	if (s < end) {
		desc_copy(s->name, "Subgroups per SIMD");
		desc_copy(s->description, "The maximum number of subgroups in flight on a SIMD unit");
		s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
		s->value.u64 = max_waves;
	}
	++s;

	if (shader->statistics) {
		for (unsigned i = 0; i < shader->statistics->count; i++) {
			struct aco_compiler_statistic_info *info = &shader->statistics->infos[i];
			uint32_t value = shader->statistics->values[i];
			if (s < end) {
				desc_copy(s->name, info->name);
				desc_copy(s->description, info->desc);
				s->format = VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR;
				s->value.u64 = value;
			}
			++s;
		}
	}

	if (!pStatistics)
		*pStatisticCount = s - pStatistics;
	else if (s > end) {
		*pStatisticCount = end - pStatistics;
		result = VK_INCOMPLETE;
	} else {
		*pStatisticCount = s - pStatistics;
	}

	return result;
}

static VkResult radv_copy_representation(void *data, size_t *data_size, const char *src)
{
	size_t total_size  = strlen(src) + 1;

	if (!data) {
		*data_size = total_size;
		return VK_SUCCESS;
	}

	size_t size = MIN2(total_size, *data_size);

	memcpy(data, src, size);
	if (size)
		*((char*)data + size - 1) = 0;
	return size < total_size ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult radv_GetPipelineExecutableInternalRepresentationsKHR(
    VkDevice                                    device,
    const VkPipelineExecutableInfoKHR*          pExecutableInfo,
    uint32_t*                                   pInternalRepresentationCount,
    VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations)
{
	RADV_FROM_HANDLE(radv_pipeline, pipeline, pExecutableInfo->pipeline);
	gl_shader_stage stage;
	struct radv_shader_variant *shader = radv_get_shader_from_executable_index(pipeline, pExecutableInfo->executableIndex, &stage);

	VkPipelineExecutableInternalRepresentationKHR *p = pInternalRepresentations;
	VkPipelineExecutableInternalRepresentationKHR *end = p + (pInternalRepresentations ? *pInternalRepresentationCount : 0);
	VkResult result = VK_SUCCESS;
	/* optimized NIR */
	if (p < end) {
		p->isText = true;
		desc_copy(p->name, "NIR Shader(s)");
		desc_copy(p->description, "The optimized NIR shader(s)");
		if (radv_copy_representation(p->pData, &p->dataSize, shader->nir_string) != VK_SUCCESS)
			result = VK_INCOMPLETE;
	}
	++p;

	/* backend IR */
	if (p < end) {
		p->isText = true;
		if (radv_use_llvm_for_stage(pipeline->device, stage)) {
			desc_copy(p->name, "LLVM IR");
			desc_copy(p->description, "The LLVM IR after some optimizations");
		} else {
			desc_copy(p->name, "ACO IR");
			desc_copy(p->description, "The ACO IR after some optimizations");
		}
		if (radv_copy_representation(p->pData, &p->dataSize, shader->ir_string) != VK_SUCCESS)
			result = VK_INCOMPLETE;
	}
	++p;

	/* Disassembler */
	if (p < end) {
		p->isText = true;
		desc_copy(p->name, "Assembly");
		desc_copy(p->description, "Final Assembly");
		if (radv_copy_representation(p->pData, &p->dataSize, shader->disasm_string) != VK_SUCCESS)
			result = VK_INCOMPLETE;
	}
	++p;

	if (!pInternalRepresentations)
		*pInternalRepresentationCount = p - pInternalRepresentations;
	else if(p > end) {
		result = VK_INCOMPLETE;
		*pInternalRepresentationCount = end - pInternalRepresentations;
	} else {
		*pInternalRepresentationCount = p - pInternalRepresentations;
	}

	return result;
}
