/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/**
*******************************************************************************
* @file
*  ihevce_function_selector.c
*
* @brief
*  Contains functions to initialize function pointers used in hevc
*
* @author
*  ittiam
*
* @par List of Functions:
*  ihevce_default_arch()
*  ihevce_init_function_ptr_generic()
*  ihevce_init_function_ptr_av8()
*  ihevce_init_function_ptr_a9q()
*  ihevce_init_function_ptr()
*
* @remarks
*  None
*
*******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_sao.h"
#include "ihevc_resi_trans.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_default_arch \endif
*
* \brief
*    Get Default architecture
*
*****************************************************************************
*/
IV_ARCH_T ihevce_default_arch(void)
{
#if(defined(ENABLE_NEON) && defined(ARMV8))
    return ARCH_ARM_V8_NEON;
#elif(defined(ENABLE_NEON) && defined(ARM))
    return ARCH_ARM_A9Q;
#else
    return ARCH_ARM_NONEON;
#endif
}

// clang-format off
/*!
******************************************************************************
* \if Function name : ihevce_init_function_ptr_generic \endif
*
* \brief
*    Function pointer initialization of encoder context struct
*
*****************************************************************************
*/
static void ihevce_init_function_ptr_generic(enc_ctxt_t *ps_enc_ctxt)
{
    ps_enc_ctxt->s_func_selector.ihevc_deblk_chroma_horz_fptr = &ihevc_deblk_chroma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_chroma_vert_fptr = &ihevc_deblk_chroma_vert;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_422chroma_horz_fptr = &ihevc_deblk_422chroma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_422chroma_vert_fptr = &ihevc_deblk_422chroma_vert;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_luma_vert_fptr = &ihevc_deblk_luma_vert;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_luma_horz_fptr = &ihevc_deblk_luma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_copy_fptr = &ihevc_inter_pred_chroma_copy;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_copy_w16out_fptr = &ihevc_inter_pred_chroma_copy_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_horz_fptr = &ihevc_inter_pred_chroma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_horz_w16out_fptr = &ihevc_inter_pred_chroma_horz_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_fptr = &ihevc_inter_pred_chroma_vert;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16inp_fptr = &ihevc_inter_pred_chroma_vert_w16inp;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16inp_w16out_fptr = &ihevc_inter_pred_chroma_vert_w16inp_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16out_fptr = &ihevc_inter_pred_chroma_vert_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_horz_fptr = &ihevc_inter_pred_luma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_fptr = &ihevc_inter_pred_luma_vert;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16out_fptr = &ihevc_inter_pred_luma_vert_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16inp_fptr = &ihevc_inter_pred_luma_vert_w16inp;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_copy_fptr = &ihevc_inter_pred_luma_copy;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_copy_w16out_fptr = &ihevc_inter_pred_luma_copy_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_horz_w16out_fptr = &ihevc_inter_pred_luma_horz_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16inp_w16out_fptr = &ihevc_inter_pred_luma_vert_w16inp_w16out;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_ref_substitution_fptr = &ihevc_intra_pred_chroma_ref_substitution;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_ref_substitution_fptr = &ihevc_intra_pred_luma_ref_substitution;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_ref_filtering_fptr = &ihevc_intra_pred_ref_filtering;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_dc_fptr = &ihevc_intra_pred_chroma_dc;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_horz_fptr = &ihevc_intra_pred_chroma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode2_fptr = &ihevc_intra_pred_chroma_mode2;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_18_34_fptr = &ihevc_intra_pred_chroma_mode_18_34;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_27_to_33_fptr = &ihevc_intra_pred_chroma_mode_27_to_33;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_3_to_9_fptr = &ihevc_intra_pred_chroma_mode_3_to_9;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_planar_fptr = &ihevc_intra_pred_chroma_planar;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_ver_fptr = &ihevc_intra_pred_chroma_ver;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_11_to_17_fptr = &ihevc_intra_pred_chroma_mode_11_to_17;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_19_to_25_fptr = &ihevc_intra_pred_chroma_mode_19_to_25;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_11_to_17_fptr = &ihevc_intra_pred_luma_mode_11_to_17;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_19_to_25_fptr = &ihevc_intra_pred_luma_mode_19_to_25;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_dc_fptr = &ihevc_intra_pred_luma_dc;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_horz_fptr = &ihevc_intra_pred_luma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode2_fptr = &ihevc_intra_pred_luma_mode2;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_18_34_fptr = &ihevc_intra_pred_luma_mode_18_34;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_27_to_33_fptr = &ihevc_intra_pred_luma_mode_27_to_33;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_3_to_9_fptr = &ihevc_intra_pred_luma_mode_3_to_9;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_planar_fptr = &ihevc_intra_pred_luma_planar;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_ver_fptr = &ihevc_intra_pred_luma_ver;

    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_4x4_ttype1_fptr = &ihevc_itrans_recon_4x4_ttype1;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_4x4_fptr = &ihevc_itrans_recon_4x4;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_8x8_fptr = &ihevc_itrans_recon_8x8;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_16x16_fptr = &ihevc_itrans_recon_16x16;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_32x32_fptr = &ihevc_itrans_recon_32x32;
    ps_enc_ctxt->s_func_selector.ihevc_chroma_itrans_recon_4x4_fptr = &ihevc_chroma_itrans_recon_4x4;
    ps_enc_ctxt->s_func_selector.ihevc_chroma_itrans_recon_8x8_fptr = &ihevc_chroma_itrans_recon_8x8;
    ps_enc_ctxt->s_func_selector.ihevc_chroma_itrans_recon_16x16_fptr = &ihevc_chroma_itrans_recon_16x16;

    ps_enc_ctxt->s_func_selector.ihevc_memcpy_mul_8_fptr = &ihevc_memcpy_mul_8;
    ps_enc_ctxt->s_func_selector.ihevc_memcpy_fptr = &ihevc_memcpy;
    ps_enc_ctxt->s_func_selector.ihevc_memset_mul_8_fptr = &ihevc_memset_mul_8;
    ps_enc_ctxt->s_func_selector.ihevc_memset_fptr = &ihevc_memset;
    ps_enc_ctxt->s_func_selector.ihevc_memset_16bit_mul_8_fptr = &ihevc_memset_16bit_mul_8;
    ps_enc_ctxt->s_func_selector.ihevc_memset_16bit_fptr = &ihevc_memset_16bit;

    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_bi_fptr = &ihevc_weighted_pred_bi;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_bi_default_fptr = &ihevc_weighted_pred_bi_default;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_uni_fptr = &ihevc_weighted_pred_uni;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_bi_fptr = &ihevc_weighted_pred_chroma_bi;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_bi_default_fptr = &ihevc_weighted_pred_chroma_bi_default;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_uni_fptr = &ihevc_weighted_pred_chroma_uni;

    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_4x4_ttype1_fptr = &ihevc_resi_trans_4x4_ttype1;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_4x4_fptr = &ihevc_resi_trans_4x4;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_8x8_fptr = &ihevc_resi_trans_8x8;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_16x16_fptr = &ihevc_resi_trans_16x16;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_32x32_fptr = &ihevc_resi_trans_32x32;

    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_ssd_fptr = &ihevc_quant_iquant_ssd;
    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_ssd_rdoq_fptr = &ihevc_quant_iquant_ssd_rdoq;
    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_ssd_flat_scale_mat_fptr = &ihevc_quant_iquant_ssd_flat_scale_mat;
    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_ssd_flat_scale_mat_rdoq_fptr = &ihevc_quant_iquant_ssd_flat_scale_mat_rdoq;
    ps_enc_ctxt->s_func_selector.ihevc_q_iq_ssd_var_rnd_fact_fptr = &ihevc_q_iq_ssd_var_rnd_fact;
    ps_enc_ctxt->s_func_selector.ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_fptr = &ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact;

    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_fptr = &ihevc_quant_iquant;
    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_rdoq_fptr = &ihevc_quant_iquant_rdoq;
    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_flat_scale_mat_fptr = &ihevc_quant_iquant_flat_scale_mat;
    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_flat_scale_mat_rdoq_fptr = &ihevc_quant_iquant_flat_scale_mat_rdoq;
    ps_enc_ctxt->s_func_selector.ihevc_q_iq_var_rnd_fact_fptr = &ihevc_q_iq_var_rnd_fact;
    ps_enc_ctxt->s_func_selector.ihevc_q_iq_flat_scale_mat_var_rnd_fact_fptr = &ihevc_q_iq_flat_scale_mat_var_rnd_fact;
    ps_enc_ctxt->s_func_selector.ihevc_pad_bottom_fptr = &ihevc_pad_bottom;
    ps_enc_ctxt->s_func_selector.ihevc_pad_horz_chroma_fptr = &ihevc_pad_horz_chroma;
    ps_enc_ctxt->s_func_selector.ihevc_pad_horz_luma_fptr = &ihevc_pad_horz_luma;
    ps_enc_ctxt->s_func_selector.ihevc_pad_left_chroma_fptr = &ihevc_pad_left_chroma;
    ps_enc_ctxt->s_func_selector.ihevc_pad_left_luma_fptr = &ihevc_pad_left_luma;
    ps_enc_ctxt->s_func_selector.ihevc_pad_right_chroma_fptr = &ihevc_pad_right_chroma;
    ps_enc_ctxt->s_func_selector.ihevc_pad_right_luma_fptr = &ihevc_pad_right_luma;
    ps_enc_ctxt->s_func_selector.ihevc_pad_top_fptr = &ihevc_pad_top;
    ps_enc_ctxt->s_func_selector.ihevc_pad_vert_fptr = &ihevc_pad_vert;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class0_fptr = &ihevc_sao_edge_offset_class0;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class1_fptr = &ihevc_sao_edge_offset_class1;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class2_fptr = &ihevc_sao_edge_offset_class2;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class3_fptr = &ihevc_sao_edge_offset_class3;

    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class0_chroma_fptr = &ihevc_sao_edge_offset_class0_chroma;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class1_chroma_fptr = &ihevc_sao_edge_offset_class1_chroma;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class2_chroma_fptr = &ihevc_sao_edge_offset_class2_chroma;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class3_chroma_fptr = &ihevc_sao_edge_offset_class3_chroma;
}

#ifdef ENABLE_NEON
#ifdef ARMV8
/*!
******************************************************************************
* \if Function name : ihevce_init_function_ptr_av8 \endif
*
* \brief
*    Function pointer initialization of encoder context struct
*
*****************************************************************************
*/
static void ihevce_init_function_ptr_av8(enc_ctxt_t *ps_enc_ctxt)
{
    ps_enc_ctxt->s_func_selector.ihevc_deblk_chroma_horz_fptr = &ihevc_deblk_chroma_horz_av8;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_chroma_vert_fptr = &ihevc_deblk_chroma_vert_av8;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_luma_vert_fptr = &ihevc_deblk_luma_vert_av8;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_luma_horz_fptr = &ihevc_deblk_luma_horz_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_copy_fptr = &ihevc_inter_pred_chroma_copy_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_copy_w16out_fptr = &ihevc_inter_pred_chroma_copy_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_horz_fptr = &ihevc_inter_pred_chroma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_horz_w16out_fptr = &ihevc_inter_pred_chroma_horz_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_fptr = &ihevc_inter_pred_chroma_vert;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16inp_fptr = &ihevc_inter_pred_chroma_vert_w16inp_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16inp_w16out_fptr = &ihevc_inter_pred_chroma_vert_w16inp_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16out_fptr = &ihevc_inter_pred_chroma_vert_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_horz_fptr = &ihevc_inter_pred_luma_horz_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_fptr = &ihevc_inter_pred_luma_vert_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16out_fptr = &ihevc_inter_pred_luma_vert_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16inp_fptr = &ihevc_inter_pred_luma_vert_w16inp_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_copy_fptr = &ihevc_inter_pred_luma_copy_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_copy_w16out_fptr = &ihevc_inter_pred_luma_copy_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_horz_w16out_fptr = &ihevc_inter_pred_luma_horz_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16inp_w16out_fptr = &ihevc_inter_pred_luma_vert_w16inp_w16out_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_ref_substitution_fptr = &ihevc_intra_pred_chroma_ref_substitution;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_ref_substitution_fptr = &ihevc_intra_pred_luma_ref_substitution;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_ref_filtering_fptr = &ihevc_intra_pred_ref_filtering_neonintr;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_dc_fptr = &ihevc_intra_pred_chroma_dc_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_horz_fptr = &ihevc_intra_pred_chroma_horz_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode2_fptr = &ihevc_intra_pred_chroma_mode2_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_18_34_fptr = &ihevc_intra_pred_chroma_mode_18_34_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_27_to_33_fptr = &ihevc_intra_pred_chroma_mode_27_to_33_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_3_to_9_fptr = &ihevc_intra_pred_chroma_mode_3_to_9_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_planar_fptr = &ihevc_intra_pred_chroma_planar_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_ver_fptr = &ihevc_intra_pred_chroma_ver_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_11_to_17_fptr = &ihevc_intra_pred_chroma_mode_11_to_17_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_19_to_25_fptr = &ihevc_intra_pred_chroma_mode_19_to_25_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_11_to_17_fptr = &ihevc_intra_pred_luma_mode_11_to_17_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_19_to_25_fptr = &ihevc_intra_pred_luma_mode_19_to_25_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_dc_fptr = &ihevc_intra_pred_luma_dc_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_horz_fptr = &ihevc_intra_pred_luma_horz_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode2_fptr = &ihevc_intra_pred_luma_mode2_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_18_34_fptr = &ihevc_intra_pred_luma_mode_18_34_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_27_to_33_fptr = &ihevc_intra_pred_luma_mode_27_to_33_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_3_to_9_fptr = &ihevc_intra_pred_luma_mode_3_to_9_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_planar_fptr = &ihevc_intra_pred_luma_planar_av8;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_ver_fptr = &ihevc_intra_pred_luma_ver_av8;

    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_4x4_ttype1_fptr = &ihevc_itrans_recon_4x4_ttype1_av8;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_4x4_fptr = &ihevc_itrans_recon_4x4_av8;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_8x8_fptr = &ihevc_itrans_recon_8x8_av8;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_16x16_fptr = &ihevc_itrans_recon_16x16_av8;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_32x32_fptr = &ihevc_itrans_recon_32x32;

    ps_enc_ctxt->s_func_selector.ihevc_memcpy_mul_8_fptr = &ihevc_memcpy_mul_8_av8;
    ps_enc_ctxt->s_func_selector.ihevc_memcpy_fptr = &ihevc_memcpy_av8;
    ps_enc_ctxt->s_func_selector.ihevc_memset_mul_8_fptr = &ihevc_memset_mul_8_av8;
    ps_enc_ctxt->s_func_selector.ihevc_memset_fptr = &ihevc_memset_av8;
    ps_enc_ctxt->s_func_selector.ihevc_memset_16bit_mul_8_fptr = &ihevc_memset_16bit_mul_8_av8;
    ps_enc_ctxt->s_func_selector.ihevc_memset_16bit_fptr = &ihevc_memset_16bit_av8;

    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_bi_fptr = &ihevc_weighted_pred_bi_av8;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_bi_default_fptr = &ihevc_weighted_pred_bi_default_av8;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_uni_fptr = &ihevc_weighted_pred_uni_av8;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_bi_fptr = &ihevc_weighted_pred_chroma_bi_neonintr;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_bi_default_fptr = &ihevc_weighted_pred_chroma_bi_default_neonintr;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_uni_fptr = &ihevc_weighted_pred_chroma_uni_neonintr;

    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_4x4_ttype1_fptr = &ihevc_resi_trans_4x4_ttype1_neon;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_4x4_fptr = &ihevc_resi_trans_4x4_neon;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_8x8_fptr = &ihevc_resi_trans_8x8_neon;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_16x16_fptr = &ihevc_resi_trans_16x16_neon;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_32x32_fptr = &ihevc_resi_trans_32x32_neon;

    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_ssd_flat_scale_mat_fptr = &ihevc_quant_iquant_ssd_flat_scale_mat_neon;
    ps_enc_ctxt->s_func_selector.ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_fptr = &ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_neon;

    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class0_fptr = &ihevc_sao_edge_offset_class0_av8;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class1_fptr = &ihevc_sao_edge_offset_class1_av8;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class2_fptr = &ihevc_sao_edge_offset_class2_av8;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class3_fptr = &ihevc_sao_edge_offset_class3_av8;

    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class0_chroma_fptr = &ihevc_sao_edge_offset_class0_chroma_av8;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class1_chroma_fptr = &ihevc_sao_edge_offset_class1_chroma_av8;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class2_chroma_fptr = &ihevc_sao_edge_offset_class2_chroma_av8;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class3_chroma_fptr = &ihevc_sao_edge_offset_class3_chroma_av8;
}

#else

/*!
******************************************************************************
* \if Function name : ihevce_init_function_ptr_a9q \endif
*
* \brief
*    Function pointer initialization of encoder context struct
*
*****************************************************************************
*/
static void ihevce_init_function_ptr_a9q(enc_ctxt_t *ps_enc_ctxt)
{
    ps_enc_ctxt->s_func_selector.ihevc_deblk_chroma_horz_fptr = &ihevc_deblk_chroma_horz_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_chroma_vert_fptr = &ihevc_deblk_chroma_vert_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_luma_vert_fptr = &ihevc_deblk_luma_vert_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_deblk_luma_horz_fptr = &ihevc_deblk_luma_horz_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_copy_fptr = &ihevc_inter_pred_chroma_copy_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_copy_w16out_fptr = &ihevc_inter_pred_chroma_copy_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_horz_fptr = &ihevc_inter_pred_chroma_horz;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_horz_w16out_fptr = &ihevc_inter_pred_chroma_horz_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_fptr = &ihevc_inter_pred_chroma_vert_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16inp_fptr = &ihevc_inter_pred_chroma_vert_w16inp_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16inp_w16out_fptr = &ihevc_inter_pred_chroma_vert_w16inp_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_chroma_vert_w16out_fptr = &ihevc_inter_pred_chroma_vert_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_horz_fptr = &ihevc_inter_pred_luma_horz_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_fptr = &ihevc_inter_pred_luma_vert_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16out_fptr = &ihevc_inter_pred_luma_vert_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16inp_fptr = &ihevc_inter_pred_luma_vert_w16inp_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_copy_fptr = &ihevc_inter_pred_luma_copy_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_copy_w16out_fptr = &ihevc_inter_pred_luma_copy_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_horz_w16out_fptr = &ihevc_inter_pred_luma_horz_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_inter_pred_luma_vert_w16inp_w16out_fptr = &ihevc_inter_pred_luma_vert_w16inp_w16out_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_ref_substitution_fptr = &ihevc_intra_pred_chroma_ref_substitution;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_ref_substitution_fptr = &ihevc_intra_pred_luma_ref_substitution_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_ref_filtering_fptr = &ihevc_intra_pred_ref_filtering;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_dc_fptr = &ihevc_intra_pred_chroma_dc_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_horz_fptr = &ihevc_intra_pred_chroma_horz_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode2_fptr = &ihevc_intra_pred_chroma_mode2_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_18_34_fptr = &ihevc_intra_pred_chroma_mode_18_34_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_27_to_33_fptr = &ihevc_intra_pred_chroma_mode_27_to_33_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_3_to_9_fptr = &ihevc_intra_pred_chroma_mode_3_to_9_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_planar_fptr = &ihevc_intra_pred_chroma_planar_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_ver_fptr = &ihevc_intra_pred_chroma_ver_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_11_to_17_fptr = &ihevc_intra_pred_chroma_mode_11_to_17_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_chroma_mode_19_to_25_fptr = &ihevc_intra_pred_chroma_mode_19_to_25_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_11_to_17_fptr = &ihevc_intra_pred_luma_mode_11_to_17_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_19_to_25_fptr = &ihevc_intra_pred_luma_mode_19_to_25_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_dc_fptr = &ihevc_intra_pred_luma_dc_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_horz_fptr = &ihevc_intra_pred_luma_horz_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode2_fptr = &ihevc_intra_pred_luma_mode2_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_18_34_fptr = &ihevc_intra_pred_luma_mode_18_34_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_27_to_33_fptr = &ihevc_intra_pred_luma_mode_27_to_33_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_mode_3_to_9_fptr = &ihevc_intra_pred_luma_mode_3_to_9_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_planar_fptr = &ihevc_intra_pred_luma_planar_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_intra_pred_luma_ver_fptr = &ihevc_intra_pred_luma_ver_a9q;

    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_4x4_ttype1_fptr = &ihevc_itrans_recon_4x4_ttype1_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_4x4_fptr = &ihevc_itrans_recon_4x4_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_8x8_fptr = &ihevc_itrans_recon_8x8_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_16x16_fptr = &ihevc_itrans_recon_16x16_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_itrans_recon_32x32_fptr = &ihevc_itrans_recon_32x32;

    ps_enc_ctxt->s_func_selector.ihevc_memcpy_mul_8_fptr = &ihevc_memcpy_mul_8_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_memcpy_fptr = &ihevc_memcpy_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_memset_mul_8_fptr = &ihevc_memset_mul_8_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_memset_fptr = &ihevc_memset_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_memset_16bit_mul_8_fptr = &ihevc_memset_16bit_mul_8_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_memset_16bit_fptr = &ihevc_memset_16bit_a9q;

    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_bi_fptr = &ihevc_weighted_pred_bi_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_bi_default_fptr = &ihevc_weighted_pred_bi_default_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_uni_fptr = &ihevc_weighted_pred_uni_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_bi_fptr = &ihevc_weighted_pred_chroma_bi;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_bi_default_fptr = &ihevc_weighted_pred_chroma_bi_default;
    ps_enc_ctxt->s_func_selector.ihevc_weighted_pred_chroma_uni_fptr = &ihevc_weighted_pred_chroma_uni;

    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_4x4_ttype1_fptr = &ihevc_resi_trans_4x4_ttype1_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_4x4_fptr = &ihevc_resi_trans_4x4_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_8x8_fptr = &ihevc_resi_trans_8x8_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_16x16_fptr = &ihevc_resi_trans_16x16_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_resi_trans_32x32_fptr = &ihevc_resi_trans_32x32_a9q;

    ps_enc_ctxt->s_func_selector.ihevc_quant_iquant_ssd_flat_scale_mat_fptr = &ihevc_quant_iquant_ssd_flat_scale_mat_neon;
    ps_enc_ctxt->s_func_selector.ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_fptr = &ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_neon;

    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class0_fptr = &ihevc_sao_edge_offset_class0_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class1_fptr = &ihevc_sao_edge_offset_class1_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class2_fptr = &ihevc_sao_edge_offset_class2_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class3_fptr = &ihevc_sao_edge_offset_class3_a9q;

    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class0_chroma_fptr = &ihevc_sao_edge_offset_class0_chroma_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class1_chroma_fptr = &ihevc_sao_edge_offset_class1_chroma_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class2_chroma_fptr = &ihevc_sao_edge_offset_class2_chroma_a9q;
    ps_enc_ctxt->s_func_selector.ihevc_sao_edge_offset_class3_chroma_fptr = &ihevc_sao_edge_offset_class3_chroma_a9q;
}
#endif
#endif
// clang-format on

/*!
******************************************************************************
* \if Function name : ihevce_init_function_ptr \endif
*
* \brief
*    Function pointer initialization of encoder context struct
*
*****************************************************************************
*/
void ihevce_init_function_ptr(void *pv_enc_ctxt, IV_ARCH_T e_processor_arch)
{
    (void)e_processor_arch;
    ihevce_init_function_ptr_generic(pv_enc_ctxt);
#ifdef ENABLE_NEON
    switch(e_processor_arch)
    {
#ifdef ARMV8
    case ARCH_ARM_V8_NEON:
        ihevce_init_function_ptr_av8(pv_enc_ctxt);
        break;
#else
    case ARCH_ARM_A9Q:
        ihevce_init_function_ptr_a9q(pv_enc_ctxt);
        break;
#endif
    default:
        break;
    }
#endif
}
