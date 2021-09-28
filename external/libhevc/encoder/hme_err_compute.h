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
* \file hme_err_compute.h
*
* \brief
*    contains prototypes for functions that compute error or best results or
*    return fxn ptrs for the same.
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_ERR_COMPUTE_H_
#define _HME_ERR_COMPUTE_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define NUM_4X4 16
#define NUM_4X4_IN_8x8 4
#define NUM_4X4_IN_16x16 16
#define NUM_8X8_IN_16x16 4
#define NUM_8X8_IN_32x32 16
#define NUM_8X8_IN_64x64 64
#define NUM_16X16_IN_64x64 16
#define NUM_ROWS_IN_4X4 4
#define NUM_PIXELS_IN_ROW 4
#define NUM_CANDIDATES_IN_GRID 9

// 0 => best + good;
// 1 => 1st and 2nd best;
// good => worse or equal to second best
#define BESTEST 0

#define COST(a, b, c, d, e) (a)

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/
void hme_evalsad_pt_npu_MxN_16bit(err_prms_t *ps_prms);

#define compute_sad_16bit hme_evalsad_pt_npu_MxN_16bit

/**
********************************************************************************
*  @fn    S32 hme_update_results_grid_pu_bestn(result_upd_prms_t *ps_result_prms);
*
*  @brief  Updates the best N results based on a grid SAD for enabled partitions
*
*  @param[in,out]  ps_result_prms : contains parametrs pertaining to the results
*
*  @return None
********************************************************************************
*/
void hme_update_results_grid_pu_bestn(result_upd_prms_t *ps_result_prms);

void hme_update_results_grid_pu_bestn_xtreme_speed(result_upd_prms_t *ps_result_prms);

/**
********************************************************************************
*  @fn     hme_update_results_grid_pu_bestn_no_encode(result_upd_prms_t *ps_result_prms)
*
*  @brief  Updates results for the case where 1 best result is to be updated
*          for a given pt, for several parts
*          Note : The function is replicated for CLIPing the cost to 16bit to make
*                  bit match with SIMD version
*
*  @param[in]  result_upd_prms_t : Contains the input parameters to this fxn
*
*  @return   The result_upd_prms_t structure is updated for all the active
*            parts in case the current candt has results for any given part
*             that is the best result for that part
********************************************************************************
*/
void hme_update_results_grid_pu_bestn_no_encode(result_upd_prms_t *ps_result_prms);

/**
********************************************************************************
*  @fn     hme_get_result_fxn(i4_grid_mask, i4_part_mask, i4_num_results)
*
*  @brief  Implements predictive search with square grid refinement. In this
*           case, the square grid is of step 1 always. since this is considered
*           to be more of a refinement search
*
*  @param[in]  i4_grid_mask : Mask containing which of 9 grid pts active
*
*  @param[in]  i4_part_mask : Mask containing which of the 17 parts active
*
*  @param[in]  i4_num_results: Number of active results
*
*  @return   Pointer to the appropriate result update function
*             (type PF_RESULT_FXN_T)
********************************************************************************
*/
PF_RESULT_FXN_T hme_get_result_fxn(S32 i4_grid_mask, S32 i4_part_mask, S32 i4_num_results);

void compute_satd_16bit(err_prms_t *ps_prms);

void compute_satd_8bit(err_prms_t *ps_prms);

void compute_sad_16bit(err_prms_t *ps_prms);

S32 compute_mv_cost(search_node_t *ps_search_node, pred_ctxt_t *ps_pred_ctxt, BLK_SIZE_T e_blk_size);

void hme_init_pred_ctxt_no_encode(
    pred_ctxt_t *ps_pred_ctxt,
    search_results_t *ps_search_results,
    search_node_t *ps_top_candts,
    search_node_t *ps_left_candts,
    search_node_t **pps_proj_coloc_candts,
    search_node_t *ps_coloc_candts,
    search_node_t *ps_zeromv_candt,
    S32 pred_lx,
    S32 lambda,
    S32 lambda_q_shift,
    U08 **ppu1_ref_bits_tlu,
    S16 *pi2_ref_scf);

void hme_init_pred_ctxt_encode(
    pred_ctxt_t *ps_pred_ctxt,
    search_results_t *ps_search_results,
    search_node_t *ps_coloc_candts,
    search_node_t *ps_zeromv_candt,
    mv_grid_t *ps_mv_grid,
    S32 pred_lx,
    S32 lambda,
    S32 lambda_q_shift,
    U08 **ppu1_ref_bits_tlu,
    S16 *pi2_ref_scf);

/**
********************************************************************************
*  @fn     compute_mv_cost_coarse(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for coarse explicit search in coarsest layer
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_coarse(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

/**
********************************************************************************
*  @fn     compute_mv_cost_coarse(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for coarse explicit search in coarsest layer
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_coarse_high_speed(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

/**
********************************************************************************
*  @fn     compute_mv_cost_coarse(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for coarse explicit search in coarsest layer
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_refine(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

/**
********************************************************************************
*  @fn     compute_mv_cost_explicit(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for explicit search in layers not encoded
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_explicit(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

S32 compute_mv_cost_implicit(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

S32 compute_mv_cost_implicit_high_speed(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

S32 compute_mv_cost_implicit_high_speed_modified(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel);

void hme_evalsad_grid_pu_16x16(err_prms_t *ps_prms);

void hme_evalsatd_pt_pu_8x8(err_prms_t *ps_prms);

WORD32 hme_evalsatd_pt_pu_8x8_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector);

void hme_evalsatd_update_1_best_result_pt_pu_16x16(
    err_prms_t *ps_prms, result_upd_prms_t *ps_result_prms);

WORD32 hme_evalsatd_pt_pu_32x32_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector);

void hme_evalsatd_pt_pu_32x32(err_prms_t *ps_prms);

void hme_evalsatd_pt_pu_64x64(err_prms_t *ps_prms);

WORD32 hme_evalsatd_pt_pu_64x64_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector);

WORD32 hme_evalsatd_pt_pu_16x16_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector);

void ihevce_had_32x32_r(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 **ppi4_tu_split,
    WORD32 **ppi4_tu_early_cbf,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    WORD32 i4_cur_depth,
    WORD32 i4_max_depth,
    WORD32 i4_max_tr_size,
    WORD32 *pi4_tu_split_cost,
    me_func_selector_t *ps_func_selector);

void hme_update_results_pt_pu_best1_subpel_hs(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms);

void hme_set_mvp_node(
    search_results_t *ps_search_results,
    search_node_t *ps_candt_prj_coloc,
    U08 u1_pred_lx,
    U08 u1_default_ref_id);

S32 hme_cmp_nodes(search_node_t *ps_best_node1, search_node_t *ps_best_node2);

#endif /* #ifndef _HME_SEARCH_ALGO_H_*/
