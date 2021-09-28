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
/*!
******************************************************************************
* \file ihevce_enc_loop_utils.h
*
* \brief
*    This file contains interface defination of frame proceswsing pass
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENC_LOOP_UTILS_H_
#define _IHEVCE_ENC_LOOP_UTILS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define INTRA_ENC_DBG_L0 1  // Frame Level
#define INTRA_ENC_DBG_L1 1  // CTB Row Level
#define INTRA_ENC_DBG_L2 0  // CTB/CU Level
#define INTRA_ENC_DBG_L3 0  // PU/TU Level
#define INTRA_ENC_DBG_L4 0  // Pixel Level
/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

#define CABAC_FRAC_BITS_Q_SHIFT (1 << CABAC_FRAC_BITS_Q)
#define LAMDA_Q_SHIFT_FACT 20

#define QUANT_ROUND_FACTOR(out, r1, r0, lambda)                                                    \
    {                                                                                              \
        LWORD64 temp3_m;                                                                           \
        LWORD64 temp;                                                                              \
        temp3_m = (((r1 - r0) * lambda));                                                          \
        temp = (CLIP3(                                                                             \
            ((CABAC_FRAC_BITS_Q_SHIFT -                                                            \
              ((((LWORD64)(temp3_m) + ((LWORD64)CABAC_FRAC_BITS_Q_SHIFT << LAMDA_Q_SHIFT_FACT)) /  \
                2) >>                                                                              \
               LAMDA_Q_SHIFT_FACT))),                                                              \
            0,                                                                                     \
            (CABAC_FRAC_BITS_Q_SHIFT >> 1)));                                                      \
        out = ((WORD32)(temp * (1 << QUANT_ROUND_FACTOR_Q))) >> CABAC_FRAC_BITS_Q;                 \
    }

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_get_cl_cu_lambda_prms(ihevce_enc_loop_ctxt_t *ps_ctxt, WORD32 i4_cur_cu_qp);

void ihevce_populate_cl_cu_lambda_prms(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 i4_slice_type,
    WORD32 i4_temporal_lyr_id,
    WORD32 i4_lambda_type);

void ihevce_compute_quant_rel_param(ihevce_enc_loop_ctxt_t *ps_ctxt, WORD8 i1_cu_qp);

void ihevce_compute_cu_level_QP(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD32 i4_activity_for_qp,
    WORD32 i4_activity_for_lamda,
    WORD32 i4_reduce_qp);

void ihevce_update_cu_level_qp_lamda(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_analyse_t *ps_cu_analyse,
    WORD32 trans_size,
    WORD32 is_intra);

WORD32 ihevce_scan_coeffs(
    WORD16 *pi2_quant_coeffs,
    WORD32 *pi4_subBlock2csbfId_map,
    WORD32 scan_idx,
    WORD32 trans_size,
    UWORD8 *pu1_out_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 i4_csbf_stride);

void ihevce_populate_intra_pred_mode(
    WORD32 top_intra_mode,
    WORD32 left_intra_mode,
    WORD32 available_top,
    WORD32 available_left,
    WORD32 cu_pos_y,
    WORD32 *ps_cand_mode_list);

void ihevce_intra_pred_mode_signaling(
    WORD32 top_intra_mode,
    WORD32 left_intra_mode,
    WORD32 available_top,
    WORD32 available_left,
    WORD32 cu_pos_y,
    WORD32 luma_intra_pred_mode_current,
    intra_prev_rem_flags_t *ps_intra_pred_mode_current);
void ihevce_chroma_interleave_2d_copy(
    UWORD8 *pu1_uv_src_bp,
    WORD32 src_strd,
    UWORD8 *pu1_uv_dst_bp,
    WORD32 dst_strd,
    WORD32 w,
    WORD32 h,
    CHROMA_PLANE_ID_T e_chroma_plane);

WORD32 ihevce_t_q_iq_ssd_scan_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_src,
    WORD32 src_strd,
    WORD16 *pi2_deq_data,
    WORD32 deq_data_strd,
    UWORD8 *pu1_recon,
    WORD32 i4_recon_stride,
    UWORD8 *pu1_ecd_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 csbf_strd,
    WORD32 trans_size,
    WORD32 packed_pred_mode,
    LWORD64 *pi8_cost,
    WORD32 *pi4_coeff_off,
    WORD32 *pi4_tu_bits,
    UWORD32 *pu4_blk_sad,
    WORD32 *pi4_zero_col,
    WORD32 *pi4_zero_row,
    UWORD8 *pu1_is_recon_available,
    WORD32 i4_perform_rdoq,
    WORD32 i4_perform_sbh,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    SSD_TYPE_T e_ssd_type,
    WORD32 early_cbf);

void ihevce_quant_rounding_factor_gen(
    WORD32 i4_trans_size,
    WORD32 is_luma,
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    WORD32 *pi4_quant_round_0_1,
    WORD32 *pi4_quant_round_1_2,
    double i4_lamda_modifier,
    UWORD8 i4_is_tu_level_quant_rounding);

void ihevce_it_recon_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD16 *pi2_deq_data,
    WORD32 deq_dat_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_recon,
    WORD32 recon_strd,
    UWORD8 *pu1_ecd_data,
    WORD32 trans_size,
    WORD32 packed_pred_mode,
    WORD32 cbf,
    WORD32 zero_cols,
    WORD32 zero_rows);

void ihevce_chroma_it_recon_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD16 *pi2_deq_data,
    WORD32 deq_dat_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_recon,
    WORD32 recon_strd,
    UWORD8 *pu1_ecd_data,
    WORD32 trans_size,
    WORD32 cbf,
    WORD32 zero_cols,
    WORD32 zero_rows,
    CHROMA_PLANE_ID_T e_chroma_plane);

void ihevce_mpm_idx_based_filter_RDOPT_cand(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_analyse_t *ps_cu_analyse,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    UWORD8 *pu1_luma_mode,
    UWORD8 *pu1_eval_mark);

LWORD64 ihevce_intra_rdopt_cu_ntu(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_pred_org,
    WORD32 pred_strd_org,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    UWORD8 *pu1_luma_mode,
    cu_analyse_t *ps_cu_analyse,
    void *pv_curr_src,
    void *pv_cu_left,
    void *pv_cu_top,
    void *pv_cu_top_left,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 cu_left_stride,
    WORD32 curr_buf_idx,
    WORD32 func_proc_mode,
    WORD32 i4_alpha_stim_multiplier);
LWORD64 ihevce_inter_rdopt_cu_ntu(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_src,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 curr_buf_idx,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_inter_cand_t *ps_inter_cand,
    cu_analyse_t *ps_cu_analyse,
    WORD32 i4_alpha_stim_multiplier);

LWORD64 ihevce_inter_tu_tree_selector_and_rdopt_cost_computer(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_src,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 curr_buf_idx,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_inter_cand_t *ps_inter_cand,
    cu_analyse_t *ps_cu_analyse,
    WORD32 i4_alpha_stim_multiplier);

LWORD64 ihevce_inter_rdopt_cu_mc_mvp(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_inter_cand_t *ps_inter_cand,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_topleft_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 curr_buf_idx);
void ihevce_intra_chroma_pred_mode_selector(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_analyse_t *ps_cu_analyse,
    WORD32 rd_opt_curr_idx,
    WORD32 tu_mode,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy);

LWORD64 ihevce_chroma_cu_prcs_rdopt(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD32 rd_opt_curr_idx,
    WORD32 func_proc_mode,
    UWORD8 *pu1_chrm_src,
    WORD32 chrm_src_stride,
    UWORD8 *pu1_cu_left,
    UWORD8 *pu1_cu_top,
    UWORD8 *pu1_cu_top_left,
    WORD32 cu_left_stride,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 *pi4_chrm_tu_bits,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy);

void ihevce_set_eval_flags(
    ihevce_enc_loop_ctxt_t *ps_ctxt, enc_loop_cu_final_prms_t *ps_enc_loop_bestprms);

void ihevce_final_rdopt_mode_prcs(
    ihevce_enc_loop_ctxt_t *ps_ctxt, final_mode_process_prms_t *ps_prms);

WORD32 ihevce_set_flags_to_regulate_reevaluation(
    cu_final_recon_flags_t *ps_cu_recon_flags,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    UWORD8 *pu1_deviant_cu_regions,
    WORD32 i4_num_deviant_cus,
    WORD8 i1_qp_past,
    WORD8 i1_qp_present,
    UWORD8 u1_is_422);

void ihevce_err_compute(
    UWORD8 *pu1_inp,
    UWORD8 *pu1_interp_out_buf,
    WORD32 *pi4_sad_grid,
    WORD32 *pi4_tu_split_flags,
    WORD32 inp_stride,
    WORD32 out_stride,
    WORD32 blk_size,
    WORD32 part_mask,
    WORD32 use_satd_for_err_calc);
void ihevce_determine_children_cost_of_32x32_cu(
    block_merge_input_t *ps_merge_in,
    WORD32 *pi4_cost_children,
    WORD32 idx_of_tl_child,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y);

WORD32 ihevce_determine_children_cost_of_cu_from_me_results(
    block_merge_input_t *ps_merge_in,
    cur_ctb_cu_tree_t *ps_cu_tree_root,
    WORD32 *pi4_ref_bits,
    WORD32 *pi4_cost_children,
    WORD32 idx_of_tl_child,
    CU_SIZE_T e_cu_size_parent);

void *ihevce_tu_tree_update(
    tu_prms_t *ps_tu_prms,
    WORD32 *pnum_tu_in_cu,
    WORD32 depth,
    WORD32 tu_split_flag,
    WORD32 tu_early_cbf,
    WORD32 i4_x_off,
    WORD32 i4_y_off);
WORD32 ihevce_shrink_inter_tu_tree(
    tu_enc_loop_out_t *ps_tu_enc_loop,
    tu_enc_loop_temp_prms_t *ps_tu_enc_loop_temp_prms,
    recon_datastore_t *ps_recon_datastore,
    WORD32 num_tu_in_cu,
    UWORD8 u1_is_422);
UWORD8 ihevce_intra_mode_nxn_hash_updater(
    UWORD8 *pu1_mode_array, UWORD8 *pu1_hash_table, UWORD8 u1_num_ipe_modes);

#if ENABLE_TU_TREE_DETERMINATION_IN_RDOPT
WORD32 ihevce_determine_tu_tree_distribution(
    cu_inter_cand_t *ps_cu_data,
    me_func_selector_t *ps_func_selector,
    WORD16 *pi2_scratch_mem,
    UWORD8 *pu1_inp,
    WORD32 i4_inp_stride,
    WORD32 i4_lambda,
    UWORD8 u1_lambda_q_shift,
    UWORD8 u1_cu_size,
    UWORD8 u1_max_tr_depth);
#endif

void ihevce_populate_nbr_4x4_with_pu_data(
    nbr_4x4_t *ps_nbr_4x4, pu_t *ps_pu, WORD32 i4_nbr_buf_stride);

void ihevce_call_luma_inter_pred_rdopt_pass1(
    ihevce_enc_loop_ctxt_t *ps_ctxt, cu_inter_cand_t *ps_inter_cand, WORD32 cu_size);

LWORD64 ihevce_it_recon_ssd(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    UWORD8 *pu1_src,
    WORD32 i4_src_strd,
    UWORD8 *pu1_pred,
    WORD32 i4_pred_strd,
    WORD16 *pi2_deq_data,
    WORD32 i4_deq_data_strd,
    UWORD8 *pu1_recon,
    WORD32 i4_recon_stride,
    UWORD8 *pu1_ecd_data,
    UWORD8 u1_trans_size,
    UWORD8 u1_pred_mode,
    WORD32 i4_cbf,
    WORD32 i4_zero_col,
    WORD32 i4_zero_row,
    CHROMA_PLANE_ID_T e_chroma_plane);

WORD32 ihevce_chroma_t_q_iq_ssd_scan_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_src,
    WORD32 src_strd,
    WORD16 *pi2_deq_data,
    WORD32 deq_data_strd,
    UWORD8 *pu1_recon,
    WORD32 i4_recon_stride,
    UWORD8 *pu1_ecd_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 csbf_strd,
    WORD32 trans_size,
    WORD32 i4_scan_idx,
    WORD32 intra_flag,
    WORD32 *pi4_coeff_off,
    WORD32 *pi4_tu_bits,
    WORD32 *pi4_zero_col,
    WORD32 *pi4_zero_row,
    UWORD8 *pu1_is_recon_available,
    WORD32 i4_perform_sbh,
    WORD32 i4_perform_rdoq,
    LWORD64 *pi8_cost,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_is_skip,
    SSD_TYPE_T e_ssd_type,
    CHROMA_PLANE_ID_T e_chroma_plane);
void ihevce_update_pred_qp(ihevce_enc_loop_ctxt_t *ps_ctxt, WORD32 cu_pos_x, WORD32 cu_pos_y);
#endif /* _IHEVCE_ENC_LOOP_UTILS_H_ */
