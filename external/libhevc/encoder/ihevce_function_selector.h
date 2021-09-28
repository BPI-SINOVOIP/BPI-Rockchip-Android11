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
*  ihevce_function_selector.h
*
* @brief
*  Structure definitions used in the decoder
*
* @author
*  ittiam
*
* @par List of Functions:
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef _IHEVCE_FUNCTION_SELECTOR_H_
#define _IHEVCE_FUNCTION_SELECTOR_H_

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    ihevc_deblk_chroma_horz_ft *ihevc_deblk_chroma_horz_fptr;
    ihevc_deblk_chroma_vert_ft *ihevc_deblk_chroma_vert_fptr;
    ihevc_deblk_chroma_horz_ft *ihevc_deblk_422chroma_horz_fptr;
    ihevc_deblk_chroma_vert_ft *ihevc_deblk_422chroma_vert_fptr;
    ihevc_deblk_luma_vert_ft *ihevc_deblk_luma_vert_fptr;
    ihevc_deblk_luma_horz_ft *ihevc_deblk_luma_horz_fptr;
    ihevc_inter_pred_ft *ihevc_inter_pred_chroma_copy_fptr;
    ihevc_inter_pred_w16out_ft *ihevc_inter_pred_chroma_copy_w16out_fptr;
    ihevc_inter_pred_ft *ihevc_inter_pred_chroma_horz_fptr;
    ihevc_inter_pred_w16out_ft *ihevc_inter_pred_chroma_horz_w16out_fptr;
    ihevc_inter_pred_ft *ihevc_inter_pred_chroma_vert_fptr;
    ihevc_inter_pred_w16inp_ft *ihevc_inter_pred_chroma_vert_w16inp_fptr;
    ihevc_inter_pred_w16inp_w16out_ft *ihevc_inter_pred_chroma_vert_w16inp_w16out_fptr;
    ihevc_inter_pred_w16out_ft *ihevc_inter_pred_chroma_vert_w16out_fptr;
    ihevc_inter_pred_ft *ihevc_inter_pred_luma_horz_fptr;
    ihevc_inter_pred_ft *ihevc_inter_pred_luma_vert_fptr;
    ihevc_inter_pred_w16out_ft *ihevc_inter_pred_luma_vert_w16out_fptr;
    ihevc_inter_pred_w16inp_ft *ihevc_inter_pred_luma_vert_w16inp_fptr;
    ihevc_inter_pred_ft *ihevc_inter_pred_luma_copy_fptr;
    ihevc_inter_pred_w16out_ft *ihevc_inter_pred_luma_copy_w16out_fptr;
    ihevc_inter_pred_w16out_ft *ihevc_inter_pred_luma_horz_w16out_fptr;
    ihevc_inter_pred_w16inp_w16out_ft *ihevc_inter_pred_luma_vert_w16inp_w16out_fptr;
    ihevc_intra_pred_chroma_ref_substitution_ft *ihevc_intra_pred_chroma_ref_substitution_fptr;
    ihevc_intra_pred_luma_ref_substitution_ft *ihevc_intra_pred_luma_ref_substitution_fptr;
    ihevc_intra_pred_ref_filtering_ft *ihevc_intra_pred_ref_filtering_fptr;
    ihevc_intra_pred_chroma_dc_ft *ihevc_intra_pred_chroma_dc_fptr;
    ihevc_intra_pred_chroma_horz_ft *ihevc_intra_pred_chroma_horz_fptr;
    ihevc_intra_pred_chroma_mode2_ft *ihevc_intra_pred_chroma_mode2_fptr;
    ihevc_intra_pred_chroma_mode_18_34_ft *ihevc_intra_pred_chroma_mode_18_34_fptr;
    ihevc_intra_pred_chroma_mode_27_to_33_ft *ihevc_intra_pred_chroma_mode_27_to_33_fptr;
    ihevc_intra_pred_chroma_mode_3_to_9_ft *ihevc_intra_pred_chroma_mode_3_to_9_fptr;
    ihevc_intra_pred_chroma_planar_ft *ihevc_intra_pred_chroma_planar_fptr;
    ihevc_intra_pred_chroma_ver_ft *ihevc_intra_pred_chroma_ver_fptr;
    ihevc_intra_pred_chroma_mode_11_to_17_ft *ihevc_intra_pred_chroma_mode_11_to_17_fptr;
    ihevc_intra_pred_chroma_mode_19_to_25_ft *ihevc_intra_pred_chroma_mode_19_to_25_fptr;
    ihevc_intra_pred_luma_mode_11_to_17_ft *ihevc_intra_pred_luma_mode_11_to_17_fptr;
    ihevc_intra_pred_luma_mode_19_to_25_ft *ihevc_intra_pred_luma_mode_19_to_25_fptr;
    ihevc_intra_pred_luma_dc_ft *ihevc_intra_pred_luma_dc_fptr;
    ihevc_intra_pred_luma_horz_ft *ihevc_intra_pred_luma_horz_fptr;
    ihevc_intra_pred_luma_mode2_ft *ihevc_intra_pred_luma_mode2_fptr;
    ihevc_intra_pred_luma_mode_18_34_ft *ihevc_intra_pred_luma_mode_18_34_fptr;
    ihevc_intra_pred_luma_mode_27_to_33_ft *ihevc_intra_pred_luma_mode_27_to_33_fptr;
    ihevc_intra_pred_luma_mode_3_to_9_ft *ihevc_intra_pred_luma_mode_3_to_9_fptr;
    ihevc_intra_pred_luma_planar_ft *ihevc_intra_pred_luma_planar_fptr;
    ihevc_intra_pred_luma_ver_ft *ihevc_intra_pred_luma_ver_fptr;
    ihevc_itrans_recon_4x4_ttype1_ft *ihevc_itrans_recon_4x4_ttype1_fptr;
    ihevc_itrans_recon_4x4_ft *ihevc_itrans_recon_4x4_fptr;
    ihevc_itrans_recon_8x8_ft *ihevc_itrans_recon_8x8_fptr;
    ihevc_itrans_recon_16x16_ft *ihevc_itrans_recon_16x16_fptr;
    ihevc_itrans_recon_32x32_ft *ihevc_itrans_recon_32x32_fptr;
    ihevc_chroma_itrans_recon_4x4_ft *ihevc_chroma_itrans_recon_4x4_fptr;
    ihevc_chroma_itrans_recon_8x8_ft *ihevc_chroma_itrans_recon_8x8_fptr;
    ihevc_chroma_itrans_recon_16x16_ft *ihevc_chroma_itrans_recon_16x16_fptr;
    ihevc_memcpy_mul_8_ft *ihevc_memcpy_mul_8_fptr;
    ihevc_memcpy_ft *ihevc_memcpy_fptr;
    ihevc_memset_mul_8_ft *ihevc_memset_mul_8_fptr;
    ihevc_memset_ft *ihevc_memset_fptr;
    ihevc_memset_16bit_mul_8_ft *ihevc_memset_16bit_mul_8_fptr;
    ihevc_memset_16bit_ft *ihevc_memset_16bit_fptr;

    ihevc_weighted_pred_bi_ft *ihevc_weighted_pred_bi_fptr;
    ihevc_weighted_pred_bi_default_ft *ihevc_weighted_pred_bi_default_fptr;
    ihevc_weighted_pred_uni_ft *ihevc_weighted_pred_uni_fptr;
    ihevc_weighted_pred_chroma_bi_ft *ihevc_weighted_pred_chroma_bi_fptr;
    ihevc_weighted_pred_chroma_bi_default_ft *ihevc_weighted_pred_chroma_bi_default_fptr;
    ihevc_weighted_pred_chroma_uni_ft *ihevc_weighted_pred_chroma_uni_fptr;
    ihevc_resi_trans_4x4_ttype1_ft *ihevc_resi_trans_4x4_ttype1_fptr;
    ihevc_resi_trans_4x4_ft *ihevc_resi_trans_4x4_fptr;
    ihevc_resi_trans_8x8_ft *ihevc_resi_trans_8x8_fptr;
    ihevc_resi_trans_16x16_ft *ihevc_resi_trans_16x16_fptr;
    ihevc_resi_trans_32x32_ft *ihevc_resi_trans_32x32_fptr;
    ihevc_quant_iquant_ssd_ft *ihevc_quant_iquant_ssd_fptr;
    ihevc_quant_iquant_ssd_rdoq_ft *ihevc_quant_iquant_ssd_rdoq_fptr;
    ihevc_quant_iquant_ssd_flat_scale_mat_ft *ihevc_quant_iquant_ssd_flat_scale_mat_fptr;
    ihevc_quant_iquant_ssd_flat_scale_mat_rdoq_ft *ihevc_quant_iquant_ssd_flat_scale_mat_rdoq_fptr;
    ihevc_q_iq_ssd_var_rnd_fact_ft *ihevc_q_iq_ssd_var_rnd_fact_fptr;
    ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_ft *ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_fptr;
    ihevc_quant_iquant_ssd_ft *ihevc_quant_iquant_fptr;
    ihevc_quant_iquant_ssd_rdoq_ft *ihevc_quant_iquant_rdoq_fptr;
    ihevc_quant_iquant_ssd_flat_scale_mat_ft *ihevc_quant_iquant_flat_scale_mat_fptr;
    ihevc_quant_iquant_ssd_flat_scale_mat_rdoq_ft *ihevc_quant_iquant_flat_scale_mat_rdoq_fptr;
    ihevc_q_iq_ssd_var_rnd_fact_ft *ihevc_q_iq_var_rnd_fact_fptr;
    ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_ft *ihevc_q_iq_flat_scale_mat_var_rnd_fact_fptr;
    ihevc_pad_horz_luma_ft *ihevc_pad_horz_luma_fptr;
    ihevc_pad_horz_chroma_ft *ihevc_pad_horz_chroma_fptr;
    ihevc_pad_vert_ft *ihevc_pad_vert_fptr;
    ihevc_pad_top_ft *ihevc_pad_top_fptr;
    ihevc_pad_bottom_ft *ihevc_pad_bottom_fptr;
    ihevc_pad_left_luma_ft *ihevc_pad_left_luma_fptr;
    ihevc_pad_left_chroma_ft *ihevc_pad_left_chroma_fptr;
    ihevc_pad_right_luma_ft *ihevc_pad_right_luma_fptr;
    ihevc_pad_right_chroma_ft *ihevc_pad_right_chroma_fptr;
    ihevc_sao_edge_offset_class0_ft *ihevc_sao_edge_offset_class0_fptr;
    ihevc_sao_edge_offset_class1_ft *ihevc_sao_edge_offset_class1_fptr;
    ihevc_sao_edge_offset_class2_ft *ihevc_sao_edge_offset_class2_fptr;
    ihevc_sao_edge_offset_class3_ft *ihevc_sao_edge_offset_class3_fptr;

    ihevc_sao_edge_offset_class0_chroma_ft *ihevc_sao_edge_offset_class0_chroma_fptr;
    ihevc_sao_edge_offset_class1_chroma_ft *ihevc_sao_edge_offset_class1_chroma_fptr;
    ihevc_sao_edge_offset_class2_chroma_ft *ihevc_sao_edge_offset_class2_chroma_fptr;
    ihevc_sao_edge_offset_class3_chroma_ft *ihevc_sao_edge_offset_class3_chroma_fptr;
} func_selector_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
IV_ARCH_T ihevce_default_arch(void);

void ihevce_init_function_ptr(void *pv_enc_ctxt, IV_ARCH_T e_processor_arch);

#endif /* _IHEVCE_FUNCTION_SELECTOR_H_ */
