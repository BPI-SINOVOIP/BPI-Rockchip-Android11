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
* \file ihevce_enc_cu_recursion.h
*
* \brief
*    This file contains interface declarations of encoder normative loop pass
*    related functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENC_CU_RECURION_H_
#define _IHEVCE_ENC_CU_RECURION_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define ENABLE_TREE_DUMP 0

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void ihevce_store_cu_final(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_enc_loop_out_t *ps_cu_final,
    UWORD8 *pu1_ecd_data,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms);

void ihevce_populate_cu_tree(
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    cur_ctb_cu_tree_t *ps_cu_tree,
    WORD32 tree_depth,
    IHEVCE_QUALITY_CONFIG_T e_quality_preset,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos);

void ihevce_update_final_cu_results(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    pu_col_mv_t **pps_row_col_pu,
    WORD32 *pi4_col_pu_map_idx,
    cu_final_update_prms *ps_cu_update_prms,
    WORD32 ctb_ctr,
    WORD32 vert_ctb_ctr);

WORD32 ihevce_cu_recurse_decide(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    cur_ctb_cu_tree_t *ps_cu_tree_analyse,
    cur_ctb_cu_tree_t *ps_cu_tree_analyse_parent,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    me_ctb_data_t *ps_cu_me_data,
    pu_col_mv_t **pps_col_pu,
    cu_final_update_prms *ps_cu_update_prms,
    UWORD8 *pu1_col_pu_map,
    WORD32 *pi4_col_start_pu_idx,
    WORD32 i4_tree_depth,
    WORD32 i4_ctb_x_off,
    WORD32 i4_ctb_y_off,
    WORD32 cur_ctb_ht);

void ihevce_store_cu_results(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    final_mode_state_t *ps_final_state);

void ihevce_enc_loop_cu_bot_copy(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    WORD32 curr_cu_pos_in_row,
    WORD32 curr_cu_pos_in_ctb);

void ihevce_intra_and_inter_cuTree_merger(
    cur_ctb_cu_tree_t *ps_merged_tree,
    cur_ctb_cu_tree_t *ps_intra_tree,
    cur_ctb_cu_tree_t *ps_inter_tree,
    WORD8 *pi1_8x8CULevel_intraData_availability_indicator);

#endif /* _IHEVCE_ENC_CU_RECURION_H_ */
