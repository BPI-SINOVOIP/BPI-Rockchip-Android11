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
******************************************************************************
* @file hme_subpel.c
*
* @brief
*    Fullpel search and refinement
*
* @author
*    Ittiam
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>

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
#include "ihevce_entropy_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_global_tables.h"
#include "ihevce_dep_mngr_interface.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_globals.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_refine.h"
#include "hme_err_compute.h"
#include "hme_common_utils.h"
#include "hme_search_algo.h"
#include "ihevce_stasino_helpers.h"

/**
********************************************************************************
*  @fn     hme_fullpel_cand_sifter
*
*  @brief  Given a list of search candidates and valid partition types,
*          this function finds the two best candidates for each partition type.
*
*  @return None
********************************************************************************
*/
void hme_fullpel_cand_sifter(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 i4_alpha_stim_multiplier,
    U08 u1_is_cu_noisy,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    S32 i4_i;
    S16 i2_temp_tot_cost, i2_temp_stim_injected_cost, i2_temp_mv_cost, i2_temp_mv_x, i2_temp_mv_y,
        i2_temp_ref_idx;

    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;
    S32 i4_temp_part_mask;

    ps_search_prms->i4_alpha_stim_multiplier = i4_alpha_stim_multiplier;
    ps_search_prms->u1_is_cu_noisy = u1_is_cu_noisy;

    if(u1_is_cu_noisy)
    {
        i4_temp_part_mask = ps_search_prms->i4_part_mask;
        ps_search_prms->i4_part_mask &= ((ENABLE_2Nx2N) | (ENABLE_NxN));

        ps_fullpel_refine_ctxt->i4_num_valid_parts = hme_create_valid_part_ids(
            (ps_search_prms->i4_part_mask) & ((ENABLE_2Nx2N) | (ENABLE_NxN)),
            &ps_fullpel_refine_ctxt->ai4_part_id[0]);
    }

    ps_search_prms->u1_is_cu_noisy = u1_is_cu_noisy;

    hme_pred_search(
        ps_search_prms, ps_layer_ctxt, ps_wt_inp_prms, 0, ps_me_optimised_function_list);

    if(u1_is_cu_noisy)
    {
        if(ps_search_prms->ps_search_results->u1_num_results_per_part == 2)
        {
            for(i4_i = 0; i4_i < ps_fullpel_refine_ctxt->i4_num_valid_parts; i4_i++)
            {
                if(ps_fullpel_refine_ctxt->i2_tot_cost[0][i4_i] >
                   ps_fullpel_refine_ctxt->i2_tot_cost[1][i4_i])
                {
                    i2_temp_tot_cost = ps_fullpel_refine_ctxt->i2_tot_cost[0][i4_i];
                    i2_temp_stim_injected_cost =
                        ps_fullpel_refine_ctxt->i2_stim_injected_cost[0][i4_i];
                    i2_temp_mv_cost = ps_fullpel_refine_ctxt->i2_mv_cost[0][i4_i];
                    i2_temp_mv_x = ps_fullpel_refine_ctxt->i2_mv_x[0][i4_i];
                    i2_temp_mv_y = ps_fullpel_refine_ctxt->i2_mv_y[0][i4_i];
                    i2_temp_ref_idx = ps_fullpel_refine_ctxt->i2_ref_idx[0][i4_i];

                    ps_fullpel_refine_ctxt->i2_tot_cost[0][i4_i] =
                        ps_fullpel_refine_ctxt->i2_tot_cost[1][i4_i];
                    ps_fullpel_refine_ctxt->i2_stim_injected_cost[0][i4_i] =
                        ps_fullpel_refine_ctxt->i2_stim_injected_cost[1][i4_i];
                    ps_fullpel_refine_ctxt->i2_mv_cost[0][i4_i] =
                        ps_fullpel_refine_ctxt->i2_mv_cost[1][i4_i];
                    ps_fullpel_refine_ctxt->i2_mv_x[0][i4_i] =
                        ps_fullpel_refine_ctxt->i2_mv_x[1][i4_i];
                    ps_fullpel_refine_ctxt->i2_mv_y[0][i4_i] =
                        ps_fullpel_refine_ctxt->i2_mv_y[1][i4_i];
                    ps_fullpel_refine_ctxt->i2_ref_idx[0][i4_i] =
                        ps_fullpel_refine_ctxt->i2_ref_idx[1][i4_i];

                    ps_fullpel_refine_ctxt->i2_tot_cost[1][i4_i] = i2_temp_tot_cost;
                    ps_fullpel_refine_ctxt->i2_stim_injected_cost[1][i4_i] =
                        i2_temp_stim_injected_cost;
                    ps_fullpel_refine_ctxt->i2_mv_cost[1][i4_i] = i2_temp_mv_cost;
                    ps_fullpel_refine_ctxt->i2_mv_x[1][i4_i] = i2_temp_mv_x;
                    ps_fullpel_refine_ctxt->i2_mv_y[1][i4_i] = i2_temp_mv_y;
                    ps_fullpel_refine_ctxt->i2_ref_idx[1][i4_i] = i2_temp_ref_idx;
                }
            }
        }

        ps_search_prms->i4_part_mask = i4_temp_part_mask;

        ps_fullpel_refine_ctxt->i4_num_valid_parts = hme_create_valid_part_ids(
            ps_search_prms->i4_part_mask, &ps_fullpel_refine_ctxt->ai4_part_id[0]);
    }
}

static void hme_add_fpel_refine_candidates_to_search_cand_array(
    search_node_t *ps_unique_search_nodes,
    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt,
    S32 *pi4_num_unique_nodes,
    U32 *pu4_unique_node_map,
    S32 i4_fpel_search_result_id,
    S32 i4_fpel_search_result_array_index,
    S32 i4_unique_node_map_center_x,
    S32 i4_unique_node_map_center_y,
    S08 i1_unique_node_map_ref_idx,
    U08 u1_add_refine_grid_center_to_search_cand_array,
    U08 u1_do_not_check_for_duplicates)
{
    search_node_t s_refine_grid_center;

    U08 u1_use_hashing, i;

    S32 i2_mvx =
        ps_fullpel_refine_ctxt->i2_mv_x[i4_fpel_search_result_id][i4_fpel_search_result_array_index];
    S32 i2_mvy =
        ps_fullpel_refine_ctxt->i2_mv_y[i4_fpel_search_result_id][i4_fpel_search_result_array_index];
    S08 i1_ref_idx = ps_fullpel_refine_ctxt
                         ->i2_ref_idx[i4_fpel_search_result_id][i4_fpel_search_result_array_index];

    if(!u1_do_not_check_for_duplicates)
    {
        s_refine_grid_center.s_mv.i2_mvx = i2_mvx;
        s_refine_grid_center.s_mv.i2_mvy = i2_mvy;
        s_refine_grid_center.i1_ref_idx = i1_ref_idx;

        u1_use_hashing = (s_refine_grid_center.i1_ref_idx == i1_unique_node_map_ref_idx);

        for(i = 0; i < NUM_POINTS_IN_RECTANGULAR_GRID; i++)
        {
            S08 i1_offset_x = gai1_mv_offsets_from_center_in_rect_grid[i][0];
            S08 i1_offset_y = gai1_mv_offsets_from_center_in_rect_grid[i][1];

            if(i1_offset_x || i1_offset_y)
            {
                s_refine_grid_center.s_mv.i2_mvx = i2_mvx + i1_offset_x;
                s_refine_grid_center.s_mv.i2_mvy = i2_mvy + i1_offset_y;

                INSERT_NEW_NODE(
                    ps_unique_search_nodes,
                    pi4_num_unique_nodes[0],
                    s_refine_grid_center,
                    1,
                    pu4_unique_node_map,
                    i4_unique_node_map_center_x,
                    i4_unique_node_map_center_y,
                    u1_use_hashing);
            }
            else if(u1_add_refine_grid_center_to_search_cand_array)
            {
                s_refine_grid_center.s_mv.i2_mvx = i2_mvx;
                s_refine_grid_center.s_mv.i2_mvy = i2_mvy;

                INSERT_NEW_NODE(
                    ps_unique_search_nodes,
                    pi4_num_unique_nodes[0],
                    s_refine_grid_center,
                    1,
                    pu4_unique_node_map,
                    i4_unique_node_map_center_x,
                    i4_unique_node_map_center_y,
                    0);
            }
        }
    }
    else
    {
        for(i = 0; i < NUM_POINTS_IN_RECTANGULAR_GRID; i++)
        {
            S08 i1_offset_x = gai1_mv_offsets_from_center_in_rect_grid[i][0];
            S08 i1_offset_y = gai1_mv_offsets_from_center_in_rect_grid[i][1];

            if(i1_offset_x || i1_offset_y)
            {
                ps_unique_search_nodes[pi4_num_unique_nodes[0]].s_mv.i2_mvx = i2_mvx + i1_offset_x;
                ps_unique_search_nodes[pi4_num_unique_nodes[0]].s_mv.i2_mvy = i2_mvy + i1_offset_y;
                ps_unique_search_nodes[pi4_num_unique_nodes[0]++].i1_ref_idx = i1_ref_idx;
            }
            else if(u1_add_refine_grid_center_to_search_cand_array)
            {
                ps_unique_search_nodes[pi4_num_unique_nodes[0]].s_mv.i2_mvx = i2_mvx;
                ps_unique_search_nodes[pi4_num_unique_nodes[0]].s_mv.i2_mvy = i2_mvy;
                ps_unique_search_nodes[pi4_num_unique_nodes[0]++].i1_ref_idx = i1_ref_idx;
            }
        }
    }
}

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
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    S32 i, j;
    S32 i4_num_results;
    U08 u1_num_complete_grids = 0;
    U08 u1_num_grids = 0;

    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;

    S32 i4_num_unique_nodes = 0;

    search_node_t *ps_unique_search_nodes = ps_search_prms->ps_search_nodes;

    if(u1_num_init_search_cands >= 2)
    {
        S32 i4_max_num_results = (15 == u1_8x8_blk_mask)
                                     ? ps_refine_prms->u1_max_num_fpel_refine_centers
                                     : ((ME_XTREME_SPEED_25 == e_quality_preset)
                                            ? MAX_NUM_CANDS_FOR_FPEL_REFINE_IN_XS25
                                            : INT_MAX);

        for(i = 0; i < ps_fullpel_refine_ctxt->i4_num_valid_parts; i++)
        {
            S32 i4_part_id;
            S32 i4_index;

            i4_part_id = ps_fullpel_refine_ctxt->ai4_part_id[i];
            i4_index = (ps_fullpel_refine_ctxt->i4_num_valid_parts > 8) ? i4_part_id : i;
            i4_num_results = (15 == u1_8x8_blk_mask)
                                 ? MIN(ps_search_prms->ps_search_results->u1_num_results_per_part,
                                       ps_refine_prms->pu1_num_best_results[i4_part_id])
                                 : ps_search_prms->ps_search_results->u1_num_results_per_part;

            ASSERT(i4_num_results <= 2);

            for(j = 0; j < i4_num_results; j++)
            {
                if((ps_fullpel_refine_ctxt->i2_ref_idx[j][i4_index] >= 0) &&
                   (ps_fullpel_refine_ctxt->i2_mv_x[j][i4_index] != INTRA_MV))
                {
                    S32 i4_num_nodes_added = i4_num_unique_nodes;

                    hme_add_fpel_refine_candidates_to_search_cand_array(
                        ps_unique_search_nodes,
                        ps_fullpel_refine_ctxt,
                        &i4_num_unique_nodes,
                        pu4_unique_node_map,
                        j,
                        i4_index,
                        i4_unique_node_map_center_x,
                        i4_unique_node_map_center_y,
                        i1_unique_node_map_ref_idx,
                        0,
                        0);

                    i4_num_nodes_added = i4_num_unique_nodes - i4_num_nodes_added;

                    u1_num_complete_grids +=
                        (i4_num_nodes_added >= (NUM_POINTS_IN_RECTANGULAR_GRID - 1));
                    u1_num_grids += (!!i4_num_nodes_added);

                    i4_max_num_results--;
                }

                if(i4_max_num_results <= 0)
                {
                    break;
                }
            }

            if(i4_max_num_results <= 0)
            {
                break;
            }
        }
    }
    else if((1 == u1_num_init_search_cands) && (ps_refine_prms->u1_max_num_fpel_refine_centers >= 1))
    {
        ps_fullpel_refine_ctxt->i2_mv_x[0][0] = ps_unique_search_nodes[0].s_mv.i2_mvx;
        ps_fullpel_refine_ctxt->i2_mv_y[0][0] = ps_unique_search_nodes[0].s_mv.i2_mvy;
        ps_fullpel_refine_ctxt->i2_ref_idx[0][0] = ps_unique_search_nodes[0].i1_ref_idx;

        if((ps_fullpel_refine_ctxt->i2_ref_idx[0][0] >= 0) &&
           (ps_fullpel_refine_ctxt->i2_mv_x[0][0] != INTRA_MV))
        {
            hme_add_fpel_refine_candidates_to_search_cand_array(
                ps_unique_search_nodes,
                ps_fullpel_refine_ctxt,
                &i4_num_unique_nodes,
                pu4_unique_node_map,
                0,
                0,
                i4_unique_node_map_center_x,
                i4_unique_node_map_center_y,
                i1_unique_node_map_ref_idx,
                1,
                1);

            u1_num_complete_grids++;
        }
    }

    if(i4_num_unique_nodes > 0)
    {
        ps_search_prms->i4_num_search_nodes = i4_num_unique_nodes;
        ps_search_prms->u1_is_cu_noisy = 0;

        hme_pred_search(
            ps_search_prms,
            ps_layer_ctxt,
            ps_wt_inp_prms,
            (1 == u1_num_complete_grids) && (u1_num_grids == u1_num_complete_grids),
            ps_me_optimised_function_list

        );
    }
}

/**
********************************************************************************
*  @fn     hme_remove_duplicate_fpel_search_candidates
*
*  @brief  Function name is self-explanatory
*
*  @return Number of unique candidates
********************************************************************************
*/
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
    ME_QUALITY_PRESETS_T e_quality_preset)
{
    S32 i;

    S32 i4_max_num_cands = ((!u1_is_bidir_enabled) && (i4_num_act_ref_l0 > 1))
                               ? (i4_num_init_candts >> 1)
                               : i4_num_init_candts;
    S32 i4_num_unique_nodes = 0;

    for(i = 0; (i < i4_num_srch_cands) && (i4_num_unique_nodes < i4_max_num_cands); i++)
    {
        search_node_t *ps_cur_cand = ps_search_candts[i].ps_search_node;

        U08 u1_use_hashing = (ps_cur_cand->i1_ref_idx == i1_unique_node_map_ref_idx);

        if(i4_num_refinement_iterations > 1)
        {
#if !ENABLE_EXPLICIT_SEARCH_IN_P_IN_L0
            /* Ref0 evaluated during the first iteration */
            /* All other Ref's evaluated during the second iteration */
            if((ps_cur_cand->i1_ref_idx != pi1_pred_dir_to_ref_idx[0]) && (i4_refine_iter_ctr == 0))
            {
                continue;
            }
#else
            if(e_quality_preset == ME_HIGH_QUALITY)
            {
                if((ps_cur_cand->i1_ref_idx != pi1_pred_dir_to_ref_idx[0]) &&
                   (i4_refine_iter_ctr == 0))
                {
                    continue;
                }
            }
            else
            {
                if(ps_cur_cand->i1_ref_idx != pi1_pred_dir_to_ref_idx[i4_refine_iter_ctr])
                {
                    continue;
                }
            }
#endif
        }

        INSERT_UNIQUE_NODE(
            ps_unique_search_nodes,
            i4_num_unique_nodes,
            ps_cur_cand[0],
            pu4_unique_node_map,
            i4_unique_node_map_center_x,
            i4_unique_node_map_center_y,
            u1_use_hashing);
    }

    return i4_num_unique_nodes;
}
