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
* \file hme_fullpel.h
*
* \brief
*    contains prototypes for fullpel functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_FULLPEL_H_
#define _HME_FULLPEL_H_

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

void hme_fullpel_cand_sifter(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 i4_alpha_stim_multiplier,
    U08 u1_is_cu_noisy,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

void hme_fullpel_refine(
    refine_prms_t *ps_refine_prms,
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    U32 *pu4_unique_node_map,
    U08 u1_num_init_search_cands,
    U08 u1_8x8_blk_mask,
    S32 i4_unique_node_map_center_x,
    S32 i4_unique_node_map_center_y,
    S08 i1_unique_node_map_ref_idx,
    ME_QUALITY_PRESETS_T e_quality_preset,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

S32 hme_remove_duplicate_fpel_search_candidates(
    search_node_t *ps_unique_search_nodes,
    search_candt_t *ps_search_candts,
    U32 *pu4_unique_node_map,
    S08 *pi1_pred_dir_to_ref_idx,
    S32 i4_num_srch_cands,
    S32 i4_num_init_candts,
    S32 i4_refine_iter_ctr,
    S32 i4_num_refinement_iterations,
    S32 i4_num_act_ref_l0,
    S08 i1_unique_node_map_ref_idx,
    S32 i4_unique_node_map_center_x,
    S32 i4_unique_node_map_center_y,
    U08 u1_is_bidir_enabled,
    ME_QUALITY_PRESETS_T e_quality_preset);

#endif
