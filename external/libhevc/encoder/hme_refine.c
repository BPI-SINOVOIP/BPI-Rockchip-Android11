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
* @file hme_refine.c
*
* @brief
*    Contains the implementation of the refinement layer searches and related
*    functionality like CU merge.
*
* @author
*    Ittiam
*
*
* List of Functions
*
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
#include "hme_fullpel.h"
#include "hme_subpel.h"
#include "hme_refine.h"
#include "hme_err_compute.h"
#include "hme_common_utils.h"
#include "hme_search_algo.h"
#include "ihevce_stasino_helpers.h"
#include "ihevce_common_utils.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

/* brief: mapping buffer to convert raster scan indices into z-scan oder in a ctb */
UWORD8 gau1_raster_scan_to_ctb[4][4] = {
    { 0, 4, 16, 20 }, { 8, 12, 24, 28 }, { 32, 36, 48, 52 }, { 40, 44, 56, 60 }
};

/*****************************************************************************/
/* Extern Fucntion declaration                                               */
/*****************************************************************************/
extern ctb_boundary_attrs_t *
    get_ctb_attrs(S32 ctb_start_x, S32 ctb_start_y, S32 pic_wd, S32 pic_ht, me_frm_ctxt_t *ps_ctxt);

typedef void (*PF_HME_PROJECT_COLOC_CANDT_FXN)(
    search_node_t *ps_search_node,
    layer_ctxt_t *ps_curr_layer,
    layer_ctxt_t *ps_coarse_layer,
    S32 i4_pos_x,
    S32 i4_pos_y,
    S08 i1_ref_id,
    S32 i4_result_id);

typedef void (*PF_HME_PROJECT_COLOC_CANDT_L0_ME_FXN)(
    search_node_t *ps_search_node,
    layer_ctxt_t *ps_curr_layer,
    layer_ctxt_t *ps_coarse_layer,
    S32 i4_pos_x,
    S32 i4_pos_y,
    S32 i4_num_act_ref_l0,
    U08 u1_pred_dir,
    U08 u1_default_ref_id,
    S32 i4_result_id);

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

void ihevce_no_wt_copy(
    coarse_me_ctxt_t *ps_ctxt,
    layer_ctxt_t *ps_curr_layer,
    pu_t *ps_pu,
    UWORD8 *pu1_temp_pred,
    WORD32 temp_stride,
    WORD32 blk_x,
    WORD32 blk_y)
{
    UWORD8 *pu1_ref;
    WORD32 ref_stride, ref_offset;
    WORD32 row, col, i4_tmp;

    ASSERT((ps_pu->b2_pred_mode == PRED_L0) || (ps_pu->b2_pred_mode == PRED_L1));

    if(ps_pu->b2_pred_mode == PRED_L0)
    {
        WORD8 i1_ref_idx;

        i1_ref_idx = ps_pu->mv.i1_l0_ref_idx;
        pu1_ref = ps_curr_layer->ppu1_list_inp[i1_ref_idx];

        ref_stride = ps_curr_layer->i4_inp_stride;

        ref_offset = ((blk_y << 3) + ps_pu->mv.s_l0_mv.i2_mvy) * ref_stride;
        ref_offset += (blk_x << 3) + ps_pu->mv.s_l0_mv.i2_mvx;

        pu1_ref += ref_offset;

        for(row = 0; row < temp_stride; row++)
        {
            for(col = 0; col < temp_stride; col++)
            {
                i4_tmp = pu1_ref[col];
                pu1_temp_pred[col] = CLIP_U8(i4_tmp);
            }

            pu1_ref += ref_stride;
            pu1_temp_pred += temp_stride;
        }
    }
    else
    {
        WORD8 i1_ref_idx;

        i1_ref_idx = ps_pu->mv.i1_l1_ref_idx;
        pu1_ref = ps_curr_layer->ppu1_list_inp[i1_ref_idx];

        ref_stride = ps_curr_layer->i4_inp_stride;

        ref_offset = ((blk_y << 3) + ps_pu->mv.s_l1_mv.i2_mvy) * ref_stride;
        ref_offset += (blk_x << 3) + ps_pu->mv.s_l1_mv.i2_mvx;

        pu1_ref += ref_offset;

        for(row = 0; row < temp_stride; row++)
        {
            for(col = 0; col < temp_stride; col++)
            {
                i4_tmp = pu1_ref[col];
                pu1_temp_pred[col] = CLIP_U8(i4_tmp);
            }

            pu1_ref += ref_stride;
            pu1_temp_pred += temp_stride;
        }
    }
}

static WORD32 hme_add_clustered_mvs_as_merge_cands(
    cluster_data_t *ps_cluster_base,
    search_node_t *ps_merge_cand,
    range_prms_t **pps_range_prms,
    U08 *pu1_refid_to_pred_dir_list,
    WORD32 i4_num_clusters,
    U08 u1_pred_dir)
{
    WORD32 i, j, k;
    WORD32 i4_num_cands_added = 0;
    WORD32 i4_num_mvs_in_cluster;

    for(i = 0; i < i4_num_clusters; i++)
    {
        cluster_data_t *ps_data = &ps_cluster_base[i];

        if(u1_pred_dir == !pu1_refid_to_pred_dir_list[ps_data->ref_id])
        {
            i4_num_mvs_in_cluster = ps_data->num_mvs;

            for(j = 0; j < i4_num_mvs_in_cluster; j++)
            {
                ps_merge_cand[i4_num_cands_added].s_mv.i2_mvx = ps_data->as_mv[j].mvx;
                ps_merge_cand[i4_num_cands_added].s_mv.i2_mvy = ps_data->as_mv[j].mvy;
                ps_merge_cand[i4_num_cands_added].i1_ref_idx = ps_data->ref_id;

                CLIP_MV_WITHIN_RANGE(
                    ps_merge_cand[i4_num_cands_added].s_mv.i2_mvx,
                    ps_merge_cand[i4_num_cands_added].s_mv.i2_mvy,
                    pps_range_prms[ps_data->ref_id],
                    0,
                    0,
                    0);

                for(k = 0; k < i4_num_cands_added; k++)
                {
                    if((ps_merge_cand[k].s_mv.i2_mvx == ps_data->as_mv[j].mvx) &&
                       (ps_merge_cand[k].s_mv.i2_mvy == ps_data->as_mv[j].mvy) &&
                       (ps_merge_cand[k].i1_ref_idx == ps_data->ref_id))
                    {
                        break;
                    }
                }

                if(k == i4_num_cands_added)
                {
                    i4_num_cands_added++;
                }
            }
        }
    }

    return i4_num_cands_added;
}

static WORD32 hme_add_me_best_as_merge_cands(
    search_results_t **pps_child_data_array,
    inter_cu_results_t *ps_8x8cu_results,
    search_node_t *ps_merge_cand,
    range_prms_t **pps_range_prms,
    U08 *pu1_refid_to_pred_dir_list,
    S08 *pi1_past_list,
    S08 *pi1_future_list,
    BLK_SIZE_T e_blk_size,
    ME_QUALITY_PRESETS_T e_quality_preset,
    S32 i4_num_cands_added,
    U08 u1_pred_dir)
{
    WORD32 i, j, k;
    WORD32 i4_max_cands_to_add;

    WORD32 i4_result_id = 0;

    ASSERT(!pps_child_data_array[0]->u1_split_flag || (BLK_64x64 != e_blk_size));
    ASSERT(!pps_child_data_array[1]->u1_split_flag || (BLK_64x64 != e_blk_size));
    ASSERT(!pps_child_data_array[2]->u1_split_flag || (BLK_64x64 != e_blk_size));
    ASSERT(!pps_child_data_array[3]->u1_split_flag || (BLK_64x64 != e_blk_size));

    switch(e_quality_preset)
    {
    case ME_PRISTINE_QUALITY:
    {
        i4_max_cands_to_add = MAX_MERGE_CANDTS;

        break;
    }
    case ME_HIGH_QUALITY:
    {
        /* All 4 children are split and each grandchild contributes an MV */
        /* and 2 best results per grandchild */
        i4_max_cands_to_add = 4 * 4 * 2;

        break;
    }
    case ME_MEDIUM_SPEED:
    {
        i4_max_cands_to_add = 4 * 2 * 2;

        break;
    }
    case ME_HIGH_SPEED:
    case ME_XTREME_SPEED:
    case ME_XTREME_SPEED_25:
    {
        i4_max_cands_to_add = 4 * 2 * 1;

        break;
    }
    }

    while(i4_result_id < 4)
    {
        for(i = 0; i < 4; i++)
        {
            inter_cu_results_t *ps_child_data = pps_child_data_array[i]->ps_cu_results;
            inter_cu_results_t *ps_grandchild_data = &ps_8x8cu_results[i << 2];

            if(!pps_child_data_array[i]->u1_split_flag)
            {
                part_type_results_t *ps_data = &ps_child_data->ps_best_results[i4_result_id];

                if(ps_child_data->u1_num_best_results <= i4_result_id)
                {
                    continue;
                }

                if(ps_data->as_pu_results->pu.b1_intra_flag)
                {
                    continue;
                }

                for(j = 0; j <= (ps_data->u1_part_type != PRT_2Nx2N); j++)
                {
                    mv_t *ps_mv;

                    S08 i1_ref_idx;

                    pu_t *ps_pu = &ps_data->as_pu_results[j].pu;

                    if(u1_pred_dir !=
                       ((ps_pu->b2_pred_mode == 2) ? u1_pred_dir : ps_pu->b2_pred_mode))
                    {
                        continue;
                    }

                    if(u1_pred_dir)
                    {
                        ps_mv = &ps_pu->mv.s_l1_mv;
                        i1_ref_idx = pi1_future_list[ps_pu->mv.i1_l1_ref_idx];
                    }
                    else
                    {
                        ps_mv = &ps_pu->mv.s_l0_mv;
                        i1_ref_idx = pi1_past_list[ps_pu->mv.i1_l0_ref_idx];
                    }

                    if(-1 == i1_ref_idx)
                    {
                        continue;
                    }

                    ps_merge_cand[i4_num_cands_added].s_mv.i2_mvx = ps_mv->i2_mvx;
                    ps_merge_cand[i4_num_cands_added].s_mv.i2_mvy = ps_mv->i2_mvy;
                    ps_merge_cand[i4_num_cands_added].i1_ref_idx = i1_ref_idx;

                    CLIP_MV_WITHIN_RANGE(
                        ps_merge_cand[i4_num_cands_added].s_mv.i2_mvx,
                        ps_merge_cand[i4_num_cands_added].s_mv.i2_mvy,
                        pps_range_prms[i1_ref_idx],
                        0,
                        0,
                        0);

                    for(k = 0; k < i4_num_cands_added; k++)
                    {
                        if((ps_merge_cand[k].s_mv.i2_mvx == ps_mv->i2_mvx) &&
                           (ps_merge_cand[k].s_mv.i2_mvy == ps_mv->i2_mvy) &&
                           (ps_merge_cand[k].i1_ref_idx == i1_ref_idx))
                        {
                            break;
                        }
                    }

                    if(k == i4_num_cands_added)
                    {
                        i4_num_cands_added++;

                        if(i4_max_cands_to_add <= i4_num_cands_added)
                        {
                            return i4_num_cands_added;
                        }
                    }
                }
            }
            else
            {
                for(j = 0; j < 4; j++)
                {
                    mv_t *ps_mv;

                    S08 i1_ref_idx;

                    part_type_results_t *ps_data = ps_grandchild_data[j].ps_best_results;
                    pu_t *ps_pu = &ps_data->as_pu_results[0].pu;

                    ASSERT(ps_data->u1_part_type == PRT_2Nx2N);

                    if(ps_grandchild_data[j].u1_num_best_results <= i4_result_id)
                    {
                        continue;
                    }

                    if(ps_data->as_pu_results->pu.b1_intra_flag)
                    {
                        continue;
                    }

                    if(u1_pred_dir !=
                       ((ps_pu->b2_pred_mode == 2) ? u1_pred_dir : ps_pu->b2_pred_mode))
                    {
                        continue;
                    }

                    if(u1_pred_dir)
                    {
                        ps_mv = &ps_pu->mv.s_l1_mv;
                        i1_ref_idx = pi1_future_list[ps_pu->mv.i1_l1_ref_idx];
                    }
                    else
                    {
                        ps_mv = &ps_pu->mv.s_l0_mv;
                        i1_ref_idx = pi1_past_list[ps_pu->mv.i1_l0_ref_idx];
                    }

                    ps_merge_cand[i4_num_cands_added].s_mv.i2_mvx = ps_mv->i2_mvx;
                    ps_merge_cand[i4_num_cands_added].s_mv.i2_mvy = ps_mv->i2_mvy;
                    ps_merge_cand[i4_num_cands_added].i1_ref_idx = i1_ref_idx;

                    CLIP_MV_WITHIN_RANGE(
                        ps_merge_cand[i4_num_cands_added].s_mv.i2_mvx,
                        ps_merge_cand[i4_num_cands_added].s_mv.i2_mvy,
                        pps_range_prms[i1_ref_idx],
                        0,
                        0,
                        0);

                    for(k = 0; k < i4_num_cands_added; k++)
                    {
                        if((ps_merge_cand[k].s_mv.i2_mvx == ps_mv->i2_mvx) &&
                           (ps_merge_cand[k].s_mv.i2_mvy == ps_mv->i2_mvy) &&
                           (ps_merge_cand[k].i1_ref_idx == i1_ref_idx))
                        {
                            break;
                        }
                    }

                    if(k == i4_num_cands_added)
                    {
                        i4_num_cands_added++;

                        if(i4_max_cands_to_add <= i4_num_cands_added)
                        {
                            return i4_num_cands_added;
                        }
                    }
                }
            }
        }

        i4_result_id++;
    }

    return i4_num_cands_added;
}

WORD32 hme_add_cands_for_merge_eval(
    ctb_cluster_info_t *ps_cluster_info,
    search_results_t **pps_child_data_array,
    inter_cu_results_t *ps_8x8cu_results,
    range_prms_t **pps_range_prms,
    search_node_t *ps_merge_cand,
    U08 *pu1_refid_to_pred_dir_list,
    S08 *pi1_past_list,
    S08 *pi1_future_list,
    ME_QUALITY_PRESETS_T e_quality_preset,
    BLK_SIZE_T e_blk_size,
    U08 u1_pred_dir,
    U08 u1_blk_id)
{
    WORD32 i4_num_cands_added = 0;

    if(ME_PRISTINE_QUALITY == e_quality_preset)
    {
        cluster_data_t *ps_cluster_primo;

        WORD32 i4_num_clusters;

        if(BLK_32x32 == e_blk_size)
        {
            ps_cluster_primo = ps_cluster_info->ps_32x32_blk[u1_blk_id].as_cluster_data;
            i4_num_clusters = ps_cluster_info->ps_32x32_blk[u1_blk_id].num_clusters;
        }
        else
        {
            ps_cluster_primo = ps_cluster_info->ps_64x64_blk->as_cluster_data;
            i4_num_clusters = ps_cluster_info->ps_64x64_blk->num_clusters;
        }

        i4_num_cands_added = hme_add_clustered_mvs_as_merge_cands(
            ps_cluster_primo,
            ps_merge_cand,
            pps_range_prms,
            pu1_refid_to_pred_dir_list,
            i4_num_clusters,
            u1_pred_dir);
    }

    i4_num_cands_added = hme_add_me_best_as_merge_cands(
        pps_child_data_array,
        ps_8x8cu_results,
        ps_merge_cand,
        pps_range_prms,
        pu1_refid_to_pred_dir_list,
        pi1_past_list,
        pi1_future_list,
        e_blk_size,
        e_quality_preset,
        i4_num_cands_added,
        u1_pred_dir);

    return i4_num_cands_added;
}

/**
********************************************************************************
*  @fn   void hme_pick_refine_merge_candts(hme_merge_prms_t *ps_merge_prms,
*                                           S08 i1_ref_idx,
*                                           S32 i4_best_part_type,
*                                           S32 i4_is_vert)
*
*  @brief  Given a target partition orientation in the merged CU, and the
*          partition type of most likely partition this fxn picks up
*          candidates from the 4 constituent CUs and does refinement search
*          to identify best results for the merge CU across active partitions
*
*  @param[in,out] ps_merge_prms : Parameters sent from higher layers. Out of
*                  these params, the search result structure is also derived and
*                 updated during the search
*
*  @param[in] i1_ref_idx : ID of the buffer within the search results to update.
*               Will be 0 if all refidx collapsed to one buf, else it'll be 0/1
*
*  @param[in] i4_best_part_type : partition type of potential partition in the
*              merged CU, -1 if the merge process has not yet been able to
*              determine this.
*
*  @param[in] i4_is_vert : Whether target partition of merged CU is vertical
*             orientation or horizontal orientation.
*
*  @return Number of merge candidates
********************************************************************************
*/
WORD32 hme_pick_eval_merge_candts(
    hme_merge_prms_t *ps_merge_prms,
    hme_subpel_prms_t *ps_subpel_prms,
    S32 i4_search_idx,
    S32 i4_best_part_type,
    S32 i4_is_vert,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 i4_frm_qstep,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    S32 x_off, y_off;
    search_node_t *ps_search_node;
    S32 ai4_valid_part_ids[TOT_NUM_PARTS + 1];
    S32 i4_num_valid_parts;
    pred_ctxt_t *ps_pred_ctxt;

    search_node_t as_merge_unique_node[MAX_MERGE_CANDTS];
    S32 num_unique_nodes_cu_merge = 0;

    search_results_t *ps_search_results = ps_merge_prms->ps_results_merge;
    CU_SIZE_T e_cu_size = ps_search_results->e_cu_size;
    S32 i4_part_mask = ps_search_results->i4_part_mask;

    search_results_t *aps_child_results[4];
    layer_ctxt_t *ps_curr_layer = ps_merge_prms->ps_layer_ctxt;

    S32 i4_ref_stride, i, j;
    result_upd_prms_t s_result_prms;

    BLK_SIZE_T e_blk_size = ge_cu_size_to_blk_size[e_cu_size];
    S32 i4_offset;

    /*************************************************************************/
    /* Function pointer for SAD/SATD, array and prms structure to pass to    */
    /* This function                                                         */
    /*************************************************************************/
    PF_SAD_FXN_T pf_err_compute;
    S32 ai4_sad_grid[9][17];
    err_prms_t s_err_prms;

    /*************************************************************************/
    /* Allowed MV RANGE                                                      */
    /*************************************************************************/
    range_prms_t **pps_range_prms = ps_merge_prms->aps_mv_range;
    PF_INTERP_FXN_T pf_qpel_interp;
    PF_MV_COST_FXN pf_mv_cost_compute;
    WORD32 pred_lx;
    U08 *apu1_hpel_ref[4];

    interp_prms_t s_interp_prms;
    S32 i4_interp_buf_id;

    S32 i4_ctb_x_off = ps_merge_prms->i4_ctb_x_off;
    S32 i4_ctb_y_off = ps_merge_prms->i4_ctb_y_off;

    /* Sanity checks */
    ASSERT((e_blk_size == BLK_64x64) || (e_blk_size == BLK_32x32));

    s_err_prms.ps_cmn_utils_optimised_function_list = ps_cmn_utils_optimised_function_list;

    /* Initialize all the ptrs to child CUs for merge decision */
    aps_child_results[0] = ps_merge_prms->ps_results_tl;
    aps_child_results[1] = ps_merge_prms->ps_results_tr;
    aps_child_results[2] = ps_merge_prms->ps_results_bl;
    aps_child_results[3] = ps_merge_prms->ps_results_br;

    num_unique_nodes_cu_merge = 0;

    pf_mv_cost_compute = compute_mv_cost_implicit_high_speed;

    if(ME_PRISTINE_QUALITY == ps_merge_prms->e_quality_preset)
    {
        num_unique_nodes_cu_merge = hme_add_cands_for_merge_eval(
            ps_merge_prms->ps_cluster_info,
            aps_child_results,
            ps_merge_prms->ps_8x8_cu_results,
            pps_range_prms,
            as_merge_unique_node,
            ps_search_results->pu1_is_past,
            ps_merge_prms->pi1_past_list,
            ps_merge_prms->pi1_future_list,
            ps_merge_prms->e_quality_preset,
            e_blk_size,
            i4_search_idx,
            (ps_merge_prms->ps_results_merge->u1_x_off >> 5) +
                (ps_merge_prms->ps_results_merge->u1_y_off >> 4));
    }
    else
    {
        /*************************************************************************/
        /* Populate the list of unique search nodes in the child CUs for merge   */
        /* evaluation                                                            */
        /*************************************************************************/
        for(i = 0; i < 4; i++)
        {
            search_node_t s_search_node;

            PART_TYPE_T e_part_type;
            PART_ID_T e_part_id;

            WORD32 part_num;

            search_results_t *ps_child = aps_child_results[i];

            if(ps_child->ps_cu_results->u1_num_best_results)
            {
                if(!((ps_child->ps_cu_results->ps_best_results->as_pu_results->pu.b1_intra_flag) &&
                     (1 == ps_child->ps_cu_results->u1_num_best_results)))
                {
                    e_part_type =
                        (PART_TYPE_T)ps_child->ps_cu_results->ps_best_results[0].u1_part_type;

                    ASSERT(num_unique_nodes_cu_merge < MAX_MERGE_CANDTS);

                    /* Insert mvs of NxN partitions. */
                    for(part_num = 0; part_num < gau1_num_parts_in_part_type[((S32)e_part_type)];
                        part_num++)
                    {
                        e_part_id = ge_part_type_to_part_id[e_part_type][part_num];

                        if(ps_child->aps_part_results[i4_search_idx][e_part_id]->i1_ref_idx != -1)
                        {
                            s_search_node = *ps_child->aps_part_results[i4_search_idx][e_part_id];
                            if(s_search_node.s_mv.i2_mvx != INTRA_MV)
                            {
                                CLIP_MV_WITHIN_RANGE(
                                    s_search_node.s_mv.i2_mvx,
                                    s_search_node.s_mv.i2_mvy,
                                    pps_range_prms[s_search_node.i1_ref_idx],
                                    0,
                                    0,
                                    0);

                                INSERT_NEW_NODE_NOMAP(
                                    as_merge_unique_node,
                                    num_unique_nodes_cu_merge,
                                    s_search_node,
                                    1);
                            }
                        }
                    }
                }
            }
            else if(!((ps_merge_prms->ps_results_grandchild[(i << 2)]
                           .ps_cu_results->ps_best_results->as_pu_results->pu.b1_intra_flag) &&
                      (1 == ps_merge_prms->ps_results_grandchild[(i << 2)]
                                .ps_cu_results->u1_num_best_results)))
            {
                search_results_t *ps_results_root = &ps_merge_prms->ps_results_grandchild[(i << 2)];

                for(j = 0; j < 4; j++)
                {
                    e_part_type = (PART_TYPE_T)ps_results_root[j]
                                      .ps_cu_results->ps_best_results[0]
                                      .u1_part_type;

                    ASSERT(num_unique_nodes_cu_merge < MAX_MERGE_CANDTS);

                    /* Insert mvs of NxN partitions. */
                    for(part_num = 0; part_num < gau1_num_parts_in_part_type[((S32)e_part_type)];
                        part_num++)
                    {
                        e_part_id = ge_part_type_to_part_id[e_part_type][part_num];

                        if((ps_results_root[j]
                                .aps_part_results[i4_search_idx][e_part_id]
                                ->i1_ref_idx != -1) &&
                           (!ps_child->ps_cu_results->ps_best_results->as_pu_results->pu
                                 .b1_intra_flag))
                        {
                            s_search_node =
                                *ps_results_root[j].aps_part_results[i4_search_idx][e_part_id];
                            if(s_search_node.s_mv.i2_mvx != INTRA_MV)
                            {
                                CLIP_MV_WITHIN_RANGE(
                                    s_search_node.s_mv.i2_mvx,
                                    s_search_node.s_mv.i2_mvy,
                                    pps_range_prms[s_search_node.i1_ref_idx],
                                    0,
                                    0,
                                    0);

                                INSERT_NEW_NODE_NOMAP(
                                    as_merge_unique_node,
                                    num_unique_nodes_cu_merge,
                                    s_search_node,
                                    1);
                            }
                        }
                    }
                }
            }
        }
    }

    if(0 == num_unique_nodes_cu_merge)
    {
        return 0;
    }

    /*************************************************************************/
    /* Appropriate Err compute fxn, depends on SAD/SATD, blk size and remains*/
    /* fixed through this subpel refinement for this partition.              */
    /* Note, we do not enable grid sads since one pt is evaluated per node   */
    /* Hence, part mask is also nearly dont care and we use 2Nx2N enabled.   */
    /*************************************************************************/
    i4_part_mask = ps_search_results->i4_part_mask;

    /* Need to add the corresponding SAD functions for EXTREME SPEED : Lokesh */
    if(ps_subpel_prms->i4_use_satd)
    {
        if(BLK_32x32 == e_blk_size)
        {
            pf_err_compute = hme_evalsatd_pt_pu_32x32;
        }
        else
        {
            pf_err_compute = hme_evalsatd_pt_pu_64x64;
        }
    }
    else
    {
        pf_err_compute = (PF_SAD_FXN_T)hme_evalsad_grid_pu_MxM;
    }

    i4_ref_stride = ps_curr_layer->i4_rec_stride;

    x_off = ps_merge_prms->ps_results_tl->u1_x_off;
    y_off = ps_merge_prms->ps_results_tl->u1_y_off;
    i4_offset = x_off + i4_ctb_x_off + ((y_off + i4_ctb_y_off) * i4_ref_stride);

    /*************************************************************************/
    /* This array stores the ids of the partitions whose                     */
    /* SADs are updated. Since the partitions whose SADs are updated may not */
    /* be in contiguous order, we supply another level of indirection.       */
    /*************************************************************************/
    i4_num_valid_parts = hme_create_valid_part_ids(i4_part_mask, ai4_valid_part_ids);

    /* Initialize result params used for partition update */
    s_result_prms.pf_mv_cost_compute = NULL;
    s_result_prms.ps_search_results = ps_search_results;
    s_result_prms.pi4_valid_part_ids = ai4_valid_part_ids;
    s_result_prms.i1_ref_idx = i4_search_idx;
    s_result_prms.i4_part_mask = i4_part_mask;
    s_result_prms.pi4_sad_grid = &ai4_sad_grid[0][0];
    s_result_prms.i4_grid_mask = 1;

    /* One time Initialization of error params used for SAD/SATD compute */
    s_err_prms.i4_inp_stride = ps_subpel_prms->i4_inp_stride;
    s_err_prms.i4_ref_stride = i4_ref_stride;
    s_err_prms.i4_part_mask = (ENABLE_2Nx2N);
    s_err_prms.i4_grid_mask = 1;
    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0][0];
    s_err_prms.i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
    s_err_prms.i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];
    s_err_prms.i4_step = 1;

    /*************************************************************************/
    /* One time preparation of non changing interpolation params.            */
    /*************************************************************************/
    s_interp_prms.i4_ref_stride = i4_ref_stride;
    s_interp_prms.i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
    s_interp_prms.i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];
    s_interp_prms.apu1_interp_out[0] = ps_subpel_prms->pu1_wkg_mem;
    s_interp_prms.i4_out_stride = gau1_blk_size_to_wd[e_blk_size];
    i4_interp_buf_id = 0;

    pf_qpel_interp = ps_subpel_prms->pf_qpel_interp;

    /***************************************************************************/
    /* Compute SATD/SAD for all unique nodes of children CUs to get best merge */
    /* results                                                                 */
    /***************************************************************************/
    for(i = 0; i < num_unique_nodes_cu_merge; i++)
    {
        WORD8 i1_ref_idx;
        ps_search_node = &as_merge_unique_node[i];

        /*********************************************************************/
        /* Compute the base pointer for input, interpolated buffers          */
        /* The base pointers point as follows:                               */
        /* fx fy : 0, 0 :: fx, hy : 0, 0.5, hx, fy: 0.5, 0, hx, fy: 0.5, 0.5 */
        /* To these, we need to add the offset of the current node           */
        /*********************************************************************/
        i1_ref_idx = ps_search_node->i1_ref_idx;
        apu1_hpel_ref[0] = ps_curr_layer->ppu1_list_rec_fxfy[i1_ref_idx] + i4_offset;
        apu1_hpel_ref[1] = ps_curr_layer->ppu1_list_rec_hxfy[i1_ref_idx] + i4_offset;
        apu1_hpel_ref[2] = ps_curr_layer->ppu1_list_rec_fxhy[i1_ref_idx] + i4_offset;
        apu1_hpel_ref[3] = ps_curr_layer->ppu1_list_rec_hxhy[i1_ref_idx] + i4_offset;

        s_interp_prms.ppu1_ref = &apu1_hpel_ref[0];

        pf_qpel_interp(
            &s_interp_prms,
            ps_search_node->s_mv.i2_mvx,
            ps_search_node->s_mv.i2_mvy,
            i4_interp_buf_id);

        pred_lx = i4_search_idx;
        ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];

        s_result_prms.u1_pred_lx = pred_lx;
        s_result_prms.ps_search_node_base = ps_search_node;
        s_err_prms.pu1_inp =
            ps_wt_inp_prms->apu1_wt_inp[i1_ref_idx] + x_off + y_off * ps_subpel_prms->i4_inp_stride;
        s_err_prms.pu1_ref = s_interp_prms.pu1_final_out;
        s_err_prms.i4_ref_stride = s_interp_prms.i4_final_out_stride;

        /* Carry out the SAD/SATD. This call also does the TU RECURSION.
        Here the tu recursion logic is restricted with the size of the PU*/
        pf_err_compute(&s_err_prms);

        if(ps_subpel_prms->u1_is_cu_noisy &&
           ps_merge_prms->ps_inter_ctb_prms->i4_alpha_stim_multiplier)
        {
            ps_me_optimised_function_list->pf_compute_stim_injected_distortion_for_all_parts(
                s_err_prms.pu1_ref,
                s_err_prms.i4_ref_stride,
                ai4_valid_part_ids,
                ps_merge_prms->ps_inter_ctb_prms->pu8_part_src_sigmaX,
                ps_merge_prms->ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                s_err_prms.pi4_sad_grid,
                ps_merge_prms->ps_inter_ctb_prms->i4_alpha_stim_multiplier,
                ps_wt_inp_prms->a_inv_wpred_wt[i1_ref_idx],
                ps_wt_inp_prms->ai4_shift_val[i1_ref_idx],
                i4_num_valid_parts,
                ps_wt_inp_prms->wpred_log_wdc,
                (BLK_32x32 == e_blk_size) ? 32 : 64);
        }

        /* Update the mv's */
        s_result_prms.i2_mv_x = ps_search_node->s_mv.i2_mvx;
        s_result_prms.i2_mv_y = ps_search_node->s_mv.i2_mvy;

        /* Update best results */
        hme_update_results_pt_pu_best1_subpel_hs(&s_err_prms, &s_result_prms);
    }

    /************************************************************************/
    /* Update mv cost and total cost for each valid partition in the CU     */
    /************************************************************************/
    for(i = 0; i < TOT_NUM_PARTS; i++)
    {
        if(i4_part_mask & (1 << i))
        {
            WORD32 j;
            WORD32 i4_mv_cost;

            ps_search_node = ps_search_results->aps_part_results[i4_search_idx][i];

            for(j = 0;
                j < MIN(ps_search_results->u1_num_results_per_part, num_unique_nodes_cu_merge);
                j++)
            {
                if(ps_search_node->i1_ref_idx != -1)
                {
                    pred_lx = i4_search_idx;
                    ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];

                    /* Prediction context should now deal with qpel units */
                    HME_SET_MVPRED_RES(ps_pred_ctxt, MV_RES_QPEL);

                    ps_search_node->u1_subpel_done = 1;
                    ps_search_node->u1_is_avail = 1;

                    i4_mv_cost =
                        pf_mv_cost_compute(ps_search_node, ps_pred_ctxt, (PART_ID_T)i, MV_RES_QPEL);

                    ps_search_node->i4_tot_cost = i4_mv_cost + ps_search_node->i4_sad;
                    ps_search_node->i4_mv_cost = i4_mv_cost;

                    ps_search_node++;
                }
            }
        }
    }

    return num_unique_nodes_cu_merge;
}

#define CU_MERGE_MAX_INTRA_PARTS 4

/**
********************************************************************************
*  @fn     hme_try_merge_high_speed
*
*  @brief  Attempts to merge 4 NxN candts to a 2Nx2N candt, either as a single
entity or with partititons for high speed preset
*
*  @param[in,out]  hme_merge_prms_t: Params for CU merge
*
*  @return MERGE_RESULT_T type result of merge (CU_MERGED/CU_SPLIT)
********************************************************************************
*/
CU_MERGE_RESULT_T hme_try_merge_high_speed(
    me_ctxt_t *ps_thrd_ctxt,
    me_frm_ctxt_t *ps_ctxt,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    hme_subpel_prms_t *ps_subpel_prms,
    hme_merge_prms_t *ps_merge_prms,
    inter_pu_results_t *ps_pu_results,
    pu_result_t *ps_pu_result)
{
    search_results_t *ps_results_tl, *ps_results_tr;
    search_results_t *ps_results_bl, *ps_results_br;

    S32 i;
    S32 i4_search_idx;
    S32 i4_cost_parent;
    S32 intra_cu_size;
    ULWORD64 au8_final_src_sigmaX[17], au8_final_src_sigmaXSquared[17];

    search_results_t *ps_results_merge = ps_merge_prms->ps_results_merge;
    wgt_pred_ctxt_t *ps_wt_inp_prms = &ps_ctxt->s_wt_pred;

    S32 i4_part_mask = ENABLE_ALL_PARTS - ENABLE_NxN;
    S32 is_vert = 0, i4_best_part_type = -1;
    S32 i4_intra_parts = 0; /* Keeps track of intra percentage before merge */
    S32 i4_cost_children = 0;
    S32 i4_frm_qstep = ps_ctxt->frm_qstep;
    S32 i4_num_merge_cands_evaluated = 0;
    U08 u1_x_off = ps_results_merge->u1_x_off;
    U08 u1_y_off = ps_results_merge->u1_y_off;
    S32 i4_32x32_id = (u1_y_off >> 4) + (u1_x_off >> 5);

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list =
        ps_thrd_ctxt->ps_cmn_utils_optimised_function_list;
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list =
        ((ihevce_me_optimised_function_list_t *)ps_thrd_ctxt->pv_me_optimised_function_list);
    ps_results_tl = ps_merge_prms->ps_results_tl;
    ps_results_tr = ps_merge_prms->ps_results_tr;
    ps_results_bl = ps_merge_prms->ps_results_bl;
    ps_results_br = ps_merge_prms->ps_results_br;

    if(ps_merge_prms->e_quality_preset == ME_XTREME_SPEED)
    {
        i4_part_mask &= ~ENABLE_AMP;
    }

    if(ps_merge_prms->e_quality_preset == ME_XTREME_SPEED_25)
    {
        i4_part_mask &= ~ENABLE_AMP;

        i4_part_mask &= ~ENABLE_SMP;
    }

    ps_merge_prms->i4_num_pred_dir_actual = 0;

    /*************************************************************************/
    /* The logic for High speed CU merge goes as follows:                    */
    /*                                                                       */
    /* 1. Early exit with CU_SPLIT if sum of best partitions of children CUs */
    /*    exceed 7                                                           */
    /* 2. Early exit with CU_MERGE if mvs of best partitions of children CUs */
    /*    are identical                                                      */
    /* 3. Find the all unique mvs of best partitions of children CUs and     */
    /*    evaluate partial SATDs (all 17 partitions) for each unique mv. If  */
    /*    best parent cost is lower than sum of the best children costs      */
    /*    return CU_MERGE after seeding the best results else return CU_SPLIT*/
    /*                                                                       */
    /*************************************************************************/

    /* Count the number of best partitions in child CUs, early exit if > 7 */
    if((ps_merge_prms->e_quality_preset != ME_PRISTINE_QUALITY) ||
       (CU_32x32 == ps_results_merge->e_cu_size))
    {
        S32 num_parts_in_32x32 = 0;
        WORD32 i4_part_type;

        if(ps_results_tl->u1_split_flag)
        {
            num_parts_in_32x32 += 4;

#define COST_INTERCHANGE 0
            i4_cost_children = ps_merge_prms->ps_8x8_cu_results[0].ps_best_results->i4_tot_cost +
                               ps_merge_prms->ps_8x8_cu_results[1].ps_best_results->i4_tot_cost +
                               ps_merge_prms->ps_8x8_cu_results[2].ps_best_results->i4_tot_cost +
                               ps_merge_prms->ps_8x8_cu_results[3].ps_best_results->i4_tot_cost;
        }
        else
        {
            i4_part_type = ps_results_tl->ps_cu_results->ps_best_results[0].u1_part_type;
            num_parts_in_32x32 += gau1_num_parts_in_part_type[i4_part_type];
            i4_cost_children = ps_results_tl->ps_cu_results->ps_best_results[0].i4_tot_cost;
        }

        if(ps_results_tr->u1_split_flag)
        {
            num_parts_in_32x32 += 4;

            i4_cost_children += ps_merge_prms->ps_8x8_cu_results[4].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[5].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[6].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[7].ps_best_results->i4_tot_cost;
        }
        else
        {
            i4_part_type = ps_results_tr->ps_cu_results->ps_best_results[0].u1_part_type;
            num_parts_in_32x32 += gau1_num_parts_in_part_type[i4_part_type];
            i4_cost_children += ps_results_tr->ps_cu_results->ps_best_results[0].i4_tot_cost;
        }

        if(ps_results_bl->u1_split_flag)
        {
            num_parts_in_32x32 += 4;

            i4_cost_children += ps_merge_prms->ps_8x8_cu_results[8].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[9].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[10].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[11].ps_best_results->i4_tot_cost;
        }
        else
        {
            i4_part_type = ps_results_bl->ps_cu_results->ps_best_results[0].u1_part_type;
            num_parts_in_32x32 += gau1_num_parts_in_part_type[i4_part_type];
            i4_cost_children += ps_results_bl->ps_cu_results->ps_best_results[0].i4_tot_cost;
        }

        if(ps_results_br->u1_split_flag)
        {
            num_parts_in_32x32 += 4;

            i4_cost_children += ps_merge_prms->ps_8x8_cu_results[12].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[13].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[14].ps_best_results->i4_tot_cost +
                                ps_merge_prms->ps_8x8_cu_results[15].ps_best_results->i4_tot_cost;
        }
        else
        {
            i4_part_type = ps_results_br->ps_cu_results->ps_best_results[0].u1_part_type;
            num_parts_in_32x32 += gau1_num_parts_in_part_type[i4_part_type];
            i4_cost_children += ps_results_br->ps_cu_results->ps_best_results[0].i4_tot_cost;
        }

        if((num_parts_in_32x32 > 7) && (ps_merge_prms->e_quality_preset != ME_PRISTINE_QUALITY))
        {
            return CU_SPLIT;
        }

        if((num_parts_in_32x32 > MAX_NUM_CONSTITUENT_MVS_TO_ENABLE_32MERGE_IN_XS25) &&
           (ps_merge_prms->e_quality_preset == ME_XTREME_SPEED_25))
        {
            return CU_SPLIT;
        }
    }

    /* Accumulate intra percentage before merge for early CU_SPLIT decision     */
    /* Note : Each intra part represent a NxN unit of the children CUs          */
    /* This is essentially 1/16th of the CUsize under consideration for merge   */
    if(ME_PRISTINE_QUALITY == ps_merge_prms->e_quality_preset)
    {
        if(CU_64x64 == ps_results_merge->e_cu_size)
        {
            i4_intra_parts =
                (!ps_merge_prms->ps_cluster_info->ps_cu_tree_root->u1_inter_eval_enable)
                    ? 16
                    : ps_merge_prms->ps_cluster_info->ps_cu_tree_root->u1_intra_eval_enable;
        }
        else
        {
            switch((ps_results_merge->u1_x_off >> 5) + ((ps_results_merge->u1_y_off >> 4)))
            {
            case 0:
            {
                i4_intra_parts = (!ps_merge_prms->ps_cluster_info->ps_cu_tree_root->ps_child_node_tl
                                       ->u1_inter_eval_enable)
                                     ? 16
                                     : (ps_merge_prms->ps_cluster_info->ps_cu_tree_root
                                            ->ps_child_node_tl->u1_intra_eval_enable);

                break;
            }
            case 1:
            {
                i4_intra_parts = (!ps_merge_prms->ps_cluster_info->ps_cu_tree_root->ps_child_node_tr
                                       ->u1_inter_eval_enable)
                                     ? 16
                                     : (ps_merge_prms->ps_cluster_info->ps_cu_tree_root
                                            ->ps_child_node_tr->u1_intra_eval_enable);

                break;
            }
            case 2:
            {
                i4_intra_parts = (!ps_merge_prms->ps_cluster_info->ps_cu_tree_root->ps_child_node_bl
                                       ->u1_inter_eval_enable)
                                     ? 16
                                     : (ps_merge_prms->ps_cluster_info->ps_cu_tree_root
                                            ->ps_child_node_bl->u1_intra_eval_enable);

                break;
            }
            case 3:
            {
                i4_intra_parts = (!ps_merge_prms->ps_cluster_info->ps_cu_tree_root->ps_child_node_br
                                       ->u1_inter_eval_enable)
                                     ? 16
                                     : (ps_merge_prms->ps_cluster_info->ps_cu_tree_root
                                            ->ps_child_node_br->u1_intra_eval_enable);

                break;
            }
            }
        }
    }
    else
    {
        for(i = 0; i < 4; i++)
        {
            search_results_t *ps_results =
                (i == 0) ? ps_results_tl
                         : ((i == 1) ? ps_results_tr : ((i == 2) ? ps_results_bl : ps_results_br));

            part_type_results_t *ps_best_res = &ps_results->ps_cu_results->ps_best_results[0];

            if(ps_results->u1_split_flag)
            {
                U08 u1_x_off = ps_results->u1_x_off;
                U08 u1_y_off = ps_results->u1_y_off;
                U08 u1_8x8_zscan_id = gau1_ctb_raster_to_zscan[(u1_x_off >> 2) + (u1_y_off << 2)] >>
                                      2;

                /* Special case to handle 8x8 CUs when 16x16 is split */
                ASSERT(ps_results->e_cu_size == CU_16x16);

                ps_best_res = &ps_ctxt->as_cu8x8_results[u1_8x8_zscan_id].ps_best_results[0];

                if(ps_best_res->as_pu_results[0].pu.b1_intra_flag)
                    i4_intra_parts += 1;

                ps_best_res = &ps_ctxt->as_cu8x8_results[u1_8x8_zscan_id + 1].ps_best_results[0];

                if(ps_best_res->as_pu_results[0].pu.b1_intra_flag)
                    i4_intra_parts += 1;

                ps_best_res = &ps_ctxt->as_cu8x8_results[u1_8x8_zscan_id + 2].ps_best_results[0];

                if(ps_best_res->as_pu_results[0].pu.b1_intra_flag)
                    i4_intra_parts += 1;

                ps_best_res = &ps_ctxt->as_cu8x8_results[u1_8x8_zscan_id + 3].ps_best_results[0];

                if(ps_best_res->as_pu_results[0].pu.b1_intra_flag)
                    i4_intra_parts += 1;
            }
            else if(ps_best_res[0].as_pu_results[0].pu.b1_intra_flag)
            {
                i4_intra_parts += 4;
            }
        }
    }

    /* Determine the max intra CU size indicated by IPE */
    intra_cu_size = CU_64x64;
    if(ps_cur_ipe_ctb->u1_split_flag)
    {
        intra_cu_size = CU_32x32;
        if(ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id].b1_split_flag)
        {
            intra_cu_size = CU_16x16;
        }
    }

    if(((i4_intra_parts > CU_MERGE_MAX_INTRA_PARTS) &&
        (intra_cu_size < ps_results_merge->e_cu_size) &&
        (ME_PRISTINE_QUALITY != ps_merge_prms->e_quality_preset)) ||
       (i4_intra_parts == 16))
    {
        S32 i4_merge_outcome;

        i4_merge_outcome = (CU_32x32 == ps_results_merge->e_cu_size)
                               ? (!ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id].b1_split_flag &&
                                  ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id].b1_valid_cu)
                               : (!ps_cur_ipe_ctb->u1_split_flag);

        i4_merge_outcome = i4_merge_outcome ||
                           (ME_PRISTINE_QUALITY == ps_merge_prms->e_quality_preset);

        i4_merge_outcome = i4_merge_outcome &&
                           !(ps_subpel_prms->u1_is_cu_noisy && DISABLE_INTRA_WHEN_NOISY);

        if(i4_merge_outcome)
        {
            inter_cu_results_t *ps_cu_results = ps_results_merge->ps_cu_results;
            part_type_results_t *ps_best_result = ps_cu_results->ps_best_results;
            pu_t *ps_pu = &ps_best_result->as_pu_results->pu;

            ps_cu_results->u1_num_best_results = 1;
            ps_cu_results->u1_cu_size = ps_results_merge->e_cu_size;
            ps_cu_results->u1_x_off = u1_x_off;
            ps_cu_results->u1_y_off = u1_y_off;

            ps_best_result->u1_part_type = PRT_2Nx2N;
            ps_best_result->ai4_tu_split_flag[0] = 0;
            ps_best_result->ai4_tu_split_flag[1] = 0;
            ps_best_result->ai4_tu_split_flag[2] = 0;
            ps_best_result->ai4_tu_split_flag[3] = 0;
            ps_best_result->i4_tot_cost =
                (CU_64x64 == ps_results_merge->e_cu_size)
                    ? ps_cur_ipe_ctb->i4_best64x64_intra_cost
                    : ps_cur_ipe_ctb->ai4_best32x32_intra_cost[i4_32x32_id];

            ps_pu->b1_intra_flag = 1;
            ps_pu->b4_pos_x = u1_x_off >> 2;
            ps_pu->b4_pos_y = u1_y_off >> 2;
            ps_pu->b4_wd = (1 << (ps_results_merge->e_cu_size + 1)) - 1;
            ps_pu->b4_ht = ps_pu->b4_wd;
            ps_pu->mv.i1_l0_ref_idx = -1;
            ps_pu->mv.i1_l1_ref_idx = -1;
            ps_pu->mv.s_l0_mv.i2_mvx = INTRA_MV;
            ps_pu->mv.s_l0_mv.i2_mvy = INTRA_MV;
            ps_pu->mv.s_l1_mv.i2_mvx = INTRA_MV;
            ps_pu->mv.s_l1_mv.i2_mvy = INTRA_MV;

            return CU_MERGED;
        }
        else
        {
            return CU_SPLIT;
        }
    }

    if(i4_intra_parts)
    {
        i4_part_mask = ENABLE_2Nx2N;
    }

    ps_results_merge->u1_num_active_ref = (ps_ctxt->s_frm_prms.bidir_enabled) ? 2 : 1;

    hme_reset_search_results(ps_results_merge, i4_part_mask, MV_RES_QPEL);

    ps_results_merge->u1_num_active_ref = ps_merge_prms->i4_num_ref;
    ps_merge_prms->i4_num_pred_dir_actual = 0;

    if(ps_subpel_prms->u1_is_cu_noisy && ps_merge_prms->ps_inter_ctb_prms->i4_alpha_stim_multiplier)
    {
        S32 ai4_valid_part_ids[TOT_NUM_PARTS + 1];
        S32 i4_num_valid_parts;
        S32 i4_sigma_array_offset;

        i4_num_valid_parts = hme_create_valid_part_ids(i4_part_mask, ai4_valid_part_ids);

        /*********************************************************************************************************************************************/
        /* i4_sigma_array_offset : takes care of pointing to the appropriate 4x4 block's sigmaX and sigmaX-squared value in a CTB out of 256 values  */
        /* Logic is x/4 + ((y/4) x 16) : every 4 pixel increase in x equals one 4x4 block increment, every 4 pixel increase in y equals 16 4x4 block */
        /* increment as there will be 256 4x4 blocks in a CTB                                                                                        */
        /*********************************************************************************************************************************************/
        i4_sigma_array_offset = (ps_merge_prms->ps_results_merge->u1_x_off / 4) +
                                (ps_merge_prms->ps_results_merge->u1_y_off * 4);

        for(i = 0; i < i4_num_valid_parts; i++)
        {
            S32 i4_part_id = ai4_valid_part_ids[i];

            hme_compute_final_sigma_of_pu_from_base_blocks(
                ps_ctxt->au4_4x4_src_sigmaX + i4_sigma_array_offset,
                ps_ctxt->au4_4x4_src_sigmaXSquared + i4_sigma_array_offset,
                au8_final_src_sigmaX,
                au8_final_src_sigmaXSquared,
                (CU_32x32 == ps_results_merge->e_cu_size) ? 32 : 64,
                4,
                i4_part_id,
                16);
        }

        ps_merge_prms->ps_inter_ctb_prms->pu8_part_src_sigmaX = au8_final_src_sigmaX;
        ps_merge_prms->ps_inter_ctb_prms->pu8_part_src_sigmaXSquared = au8_final_src_sigmaXSquared;
    }

    /*************************************************************************/
    /* Loop through all ref idx and pick the merge candts and refine based   */
    /* on the active partitions. At this stage num ref will be 1 or 2        */
    /*************************************************************************/
    for(i4_search_idx = 0; i4_search_idx < ps_merge_prms->i4_num_ref; i4_search_idx++)
    {
        S32 i4_cands;
        U08 u1_pred_dir = 0;

        if((2 == ps_merge_prms->i4_num_ref) || (!ps_ctxt->s_frm_prms.bidir_enabled))
        {
            u1_pred_dir = i4_search_idx;
        }
        else if(ps_ctxt->s_frm_prms.u1_num_active_ref_l0 == 0)
        {
            u1_pred_dir = 1;
        }
        else if(ps_ctxt->s_frm_prms.u1_num_active_ref_l1 == 0)
        {
            u1_pred_dir = 0;
        }
        else
        {
            ASSERT(0);
        }

        /* call the function to pick and evaluate the merge candts, given */
        /* a ref id and a part mask.                                      */
        i4_cands = hme_pick_eval_merge_candts(
            ps_merge_prms,
            ps_subpel_prms,
            u1_pred_dir,
            i4_best_part_type,
            is_vert,
            ps_wt_inp_prms,
            i4_frm_qstep,
            ps_cmn_utils_optimised_function_list,
            ps_me_optimised_function_list);

        if(i4_cands)
        {
            ps_merge_prms->au1_pred_dir_searched[ps_merge_prms->i4_num_pred_dir_actual] =
                u1_pred_dir;
            ps_merge_prms->i4_num_pred_dir_actual++;
        }

        i4_num_merge_cands_evaluated += i4_cands;
    }

    /* Call the decide_part_types function here */
    /* Populate the new PU struct with the results post subpel refinement*/
    if(i4_num_merge_cands_evaluated)
    {
        inter_cu_results_t *ps_cu_results = ps_results_merge->ps_cu_results;

        hme_reset_wkg_mem(&ps_ctxt->s_buf_mgr);

        ps_merge_prms->ps_inter_ctb_prms->i4_ctb_x_off = ps_merge_prms->i4_ctb_x_off;
        ps_merge_prms->ps_inter_ctb_prms->i4_ctb_y_off = ps_merge_prms->i4_ctb_y_off;

        hme_populate_pus(
            ps_thrd_ctxt,
            ps_ctxt,
            ps_subpel_prms,
            ps_results_merge,
            ps_cu_results,
            ps_pu_results,
            ps_pu_result,
            ps_merge_prms->ps_inter_ctb_prms,
            &ps_ctxt->s_wt_pred,
            ps_merge_prms->ps_layer_ctxt,
            ps_merge_prms->au1_pred_dir_searched,
            ps_merge_prms->i4_num_pred_dir_actual);

        ps_cu_results->i4_inp_offset = (ps_cu_results->u1_x_off) + (ps_cu_results->u1_y_off * 64);

        hme_decide_part_types(
            ps_cu_results,
            ps_pu_results,
            ps_merge_prms->ps_inter_ctb_prms,
            ps_ctxt,
            ps_cmn_utils_optimised_function_list,
            ps_me_optimised_function_list

        );

        /*****************************************************************/
        /* INSERT INTRA RESULTS AT 32x32/64x64 LEVEL.                    */
        /*****************************************************************/
#if DISABLE_INTRA_IN_BPICS
        if(1 != ((ME_XTREME_SPEED_25 == ps_merge_prms->e_quality_preset) &&
                 (ps_ctxt->s_frm_prms.i4_temporal_layer_id > TEMPORAL_LAYER_DISABLE)))
#endif
        {
            if(!(DISABLE_INTRA_WHEN_NOISY && ps_merge_prms->ps_inter_ctb_prms->u1_is_cu_noisy))
            {
                hme_insert_intra_nodes_post_bipred(
                    ps_cu_results, ps_cur_ipe_ctb, ps_ctxt->frm_qstep);
            }
        }
    }
    else
    {
        return CU_SPLIT;
    }

    /* We check the best result of ref idx 0 and compare for parent vs child */
    if((ps_merge_prms->e_quality_preset != ME_PRISTINE_QUALITY) ||
       (CU_32x32 == ps_results_merge->e_cu_size))
    {
        i4_cost_parent = ps_results_merge->ps_cu_results->ps_best_results[0].i4_tot_cost;
        /*********************************************************************/
        /* Add the cost of signaling the CU tree bits.                       */
        /* Assuming parent is not split, then we signal 1 bit for this parent*/
        /* CU. If split, then 1 bit for parent CU + 4 bits for each child CU */
        /* So, 4*lambda is extra for children cost. :Lokesh                  */
        /*********************************************************************/
        {
            pred_ctxt_t *ps_pred_ctxt = &ps_results_merge->as_pred_ctxt[0];

            i4_cost_children += ((4 * ps_pred_ctxt->lambda) >> (ps_pred_ctxt->lambda_q_shift));
        }

        if(i4_cost_parent < i4_cost_children)
        {
            return CU_MERGED;
        }

        return CU_SPLIT;
    }
    else
    {
        return CU_MERGED;
    }
}

#define COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, ps_search_node, shift)                              \
    {                                                                                              \
        (ps_mv)->i2_mv_x = (ps_search_node)->s_mv.i2_mvx >> (shift);                               \
        (ps_mv)->i2_mv_y = (ps_search_node)->s_mv.i2_mvy >> (shift);                               \
        *(pi1_ref_idx) = (ps_search_node)->i1_ref_idx;                                             \
    }

/**
********************************************************************************
*  @fn     hme_update_mv_bank_noencode(search_results_t *ps_search_results,
*                               layer_mv_t *ps_layer_mv,
*                               S32 i4_search_blk_x,
*                               S32 i4_search_blk_y,
*                               mvbank_update_prms_t *ps_prms)
*
*  @brief  Updates the mv bank in case there is no further encodign to be done
*
*  @param[in]  ps_search_results: contains results for the block just searched
*
*  @param[in,out]  ps_layer_mv : Has pointer to mv bank amongst other things
*
*  @param[in] i4_search_blk_x  : col num of blk being searched
*
*  @param[in] i4_search_blk_y : row num of blk being searched
*
*  @param[in] ps_prms : contains certain parameters which govern how updatedone
*
*  @return None
********************************************************************************
*/

void hme_update_mv_bank_noencode(
    search_results_t *ps_search_results,
    layer_mv_t *ps_layer_mv,
    S32 i4_search_blk_x,
    S32 i4_search_blk_y,
    mvbank_update_prms_t *ps_prms)
{
    hme_mv_t *ps_mv;
    hme_mv_t *ps_mv1, *ps_mv2, *ps_mv3, *ps_mv4;
    S08 *pi1_ref_idx, *pi1_ref_idx1, *pi1_ref_idx2, *pi1_ref_idx3, *pi1_ref_idx4;
    S32 i4_blk_x, i4_blk_y, i4_offset;
    S32 i4_j, i4_ref_id;
    search_node_t *ps_search_node;
    search_node_t *ps_search_node_8x8, *ps_search_node_4x4_1;
    search_node_t *ps_search_node_4x4_2, *ps_search_node_4x4_3;
    search_node_t *ps_search_node_4x4_4;

    i4_blk_x = i4_search_blk_x << ps_prms->i4_shift;
    i4_blk_y = i4_search_blk_y << ps_prms->i4_shift;
    i4_offset = i4_blk_x + i4_blk_y * ps_layer_mv->i4_num_blks_per_row;

    i4_offset *= ps_layer_mv->i4_num_mvs_per_blk;

    /* Identify the correct offset in the mvbank and the reference id buf */
    ps_mv = ps_layer_mv->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mv->pi1_ref_idx + i4_offset;

    /*************************************************************************/
    /* Supposing we store the mvs in the same blk size as we searched (e.g.  */
    /* we searched 8x8 blks and store results for 8x8 blks), then we can     */
    /* do a straightforward single update of results. This will have a 1-1   */
    /* correspondence.                                                       */
    /*************************************************************************/
    if(ps_layer_mv->e_blk_size == ps_prms->e_search_blk_size)
    {
        for(i4_ref_id = 0; i4_ref_id < (S32)ps_prms->i4_num_ref; i4_ref_id++)
        {
            ps_search_node = ps_search_results->aps_part_results[i4_ref_id][PART_ID_2Nx2N];
            for(i4_j = 0; i4_j < ps_layer_mv->i4_num_mvs_per_ref; i4_j++)
            {
                COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, ps_search_node, 0);
                ps_mv++;
                pi1_ref_idx++;
                ps_search_node++;
            }
        }
        return;
    }

    /*************************************************************************/
    /* Case where search blk size is 8x8, but we update 4x4 results. In this */
    /* case, we need to have NxN partitions enabled in search.               */
    /* Further, we update on a 1-1 basis the 4x4 blk mvs from the respective */
    /* NxN partition. We also update the 8x8 result into each of the 4x4 bank*/
    /*************************************************************************/
    ASSERT(ps_layer_mv->e_blk_size == BLK_4x4);
    ASSERT(ps_prms->e_search_blk_size == BLK_8x8);
    ASSERT((ps_search_results->i4_part_mask & (ENABLE_NxN)) == (ENABLE_NxN));

    /*************************************************************************/
    /* For every 4x4 blk we store corresponding 4x4 results and 1 8x8 result */
    /* hence the below check.                                                */
    /*************************************************************************/
    ASSERT(ps_layer_mv->i4_num_mvs_per_ref <= ps_search_results->u1_num_results_per_part + 1);

    ps_mv1 = ps_mv;
    ps_mv2 = ps_mv1 + ps_layer_mv->i4_num_mvs_per_blk;
    ps_mv3 = ps_mv1 + (ps_layer_mv->i4_num_mvs_per_row);
    ps_mv4 = ps_mv3 + (ps_layer_mv->i4_num_mvs_per_blk);
    pi1_ref_idx1 = pi1_ref_idx;
    pi1_ref_idx2 = pi1_ref_idx1 + ps_layer_mv->i4_num_mvs_per_blk;
    pi1_ref_idx3 = pi1_ref_idx1 + (ps_layer_mv->i4_num_mvs_per_row);
    pi1_ref_idx4 = pi1_ref_idx3 + (ps_layer_mv->i4_num_mvs_per_blk);

    for(i4_ref_id = 0; i4_ref_id < (S32)ps_search_results->u1_num_active_ref; i4_ref_id++)
    {
        ps_search_node_8x8 = ps_search_results->aps_part_results[i4_ref_id][PART_ID_2Nx2N];

        ps_search_node_4x4_1 = ps_search_results->aps_part_results[i4_ref_id][PART_ID_NxN_TL];

        ps_search_node_4x4_2 = ps_search_results->aps_part_results[i4_ref_id][PART_ID_NxN_TR];

        ps_search_node_4x4_3 = ps_search_results->aps_part_results[i4_ref_id][PART_ID_NxN_BL];

        ps_search_node_4x4_4 = ps_search_results->aps_part_results[i4_ref_id][PART_ID_NxN_BR];

        COPY_SEARCH_RESULT(ps_mv1, pi1_ref_idx1, ps_search_node_4x4_1, 0);
        ps_mv1++;
        pi1_ref_idx1++;
        ps_search_node_4x4_1++;
        COPY_SEARCH_RESULT(ps_mv2, pi1_ref_idx2, ps_search_node_4x4_2, 0);
        ps_mv2++;
        pi1_ref_idx2++;
        ps_search_node_4x4_2++;
        COPY_SEARCH_RESULT(ps_mv3, pi1_ref_idx3, ps_search_node_4x4_3, 0);
        ps_mv3++;
        pi1_ref_idx3++;
        ps_search_node_4x4_3++;
        COPY_SEARCH_RESULT(ps_mv4, pi1_ref_idx4, ps_search_node_4x4_4, 0);
        ps_mv4++;
        pi1_ref_idx4++;
        ps_search_node_4x4_4++;

        if(ps_layer_mv->i4_num_mvs_per_ref > 1)
        {
            COPY_SEARCH_RESULT(ps_mv1, pi1_ref_idx1, ps_search_node_8x8, 0);
            ps_mv1++;
            pi1_ref_idx1++;
            COPY_SEARCH_RESULT(ps_mv2, pi1_ref_idx2, ps_search_node_8x8, 0);
            ps_mv2++;
            pi1_ref_idx2++;
            COPY_SEARCH_RESULT(ps_mv3, pi1_ref_idx3, ps_search_node_8x8, 0);
            ps_mv3++;
            pi1_ref_idx3++;
            COPY_SEARCH_RESULT(ps_mv4, pi1_ref_idx4, ps_search_node_8x8, 0);
            ps_mv4++;
            pi1_ref_idx4++;
        }

        for(i4_j = 2; i4_j < ps_layer_mv->i4_num_mvs_per_ref; i4_j++)
        {
            COPY_SEARCH_RESULT(ps_mv1, pi1_ref_idx1, ps_search_node_4x4_1, 0);
            ps_mv1++;
            pi1_ref_idx1++;
            ps_search_node_4x4_1++;
            COPY_SEARCH_RESULT(ps_mv2, pi1_ref_idx2, ps_search_node_4x4_2, 0);
            ps_mv2++;
            pi1_ref_idx2++;
            ps_search_node_4x4_2++;
            COPY_SEARCH_RESULT(ps_mv3, pi1_ref_idx3, ps_search_node_4x4_3, 0);
            ps_mv3++;
            pi1_ref_idx3++;
            ps_search_node_4x4_3++;
            COPY_SEARCH_RESULT(ps_mv4, pi1_ref_idx4, ps_search_node_4x4_4, 0);
            ps_mv4++;
            pi1_ref_idx4++;
            ps_search_node_4x4_4++;
        }
    }
}

void hme_update_mv_bank_encode(
    search_results_t *ps_search_results,
    layer_mv_t *ps_layer_mv,
    S32 i4_search_blk_x,
    S32 i4_search_blk_y,
    mvbank_update_prms_t *ps_prms,
    U08 *pu1_pred_dir_searched,
    S32 i4_num_act_ref_l0)
{
    hme_mv_t *ps_mv;
    hme_mv_t *ps_mv1, *ps_mv2, *ps_mv3, *ps_mv4;
    S08 *pi1_ref_idx, *pi1_ref_idx1, *pi1_ref_idx2, *pi1_ref_idx3, *pi1_ref_idx4;
    S32 i4_blk_x, i4_blk_y, i4_offset;
    S32 j, i, num_parts;
    search_node_t *ps_search_node_tl, *ps_search_node_tr;
    search_node_t *ps_search_node_bl, *ps_search_node_br;
    search_node_t s_zero_mv;
    WORD32 i4_part_type = ps_search_results->ps_cu_results->ps_best_results[0].u1_part_type;

    i4_blk_x = i4_search_blk_x << ps_prms->i4_shift;
    i4_blk_y = i4_search_blk_y << ps_prms->i4_shift;
    i4_offset = i4_blk_x + i4_blk_y * ps_layer_mv->i4_num_blks_per_row;

    i4_offset *= ps_layer_mv->i4_num_mvs_per_blk;

    /* Identify the correct offset in the mvbank and the reference id buf */
    ps_mv = ps_layer_mv->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mv->pi1_ref_idx + i4_offset;

    ASSERT(ps_layer_mv->e_blk_size == BLK_8x8);
    ASSERT(ps_prms->e_search_blk_size == BLK_16x16);

    /*************************************************************************/
    /* For every 4x4 blk we store corresponding 4x4 results and 1 8x8 result */
    /* hence the below check.                                                */
    /*************************************************************************/
    ASSERT(ps_layer_mv->i4_num_mvs_per_ref <= ps_search_results->u1_num_best_results);

    ps_mv1 = ps_mv;
    ps_mv2 = ps_mv1 + ps_layer_mv->i4_num_mvs_per_blk;
    ps_mv3 = ps_mv1 + (ps_layer_mv->i4_num_mvs_per_row);
    ps_mv4 = ps_mv3 + (ps_layer_mv->i4_num_mvs_per_blk);
    pi1_ref_idx1 = pi1_ref_idx;
    pi1_ref_idx2 = pi1_ref_idx1 + ps_layer_mv->i4_num_mvs_per_blk;
    pi1_ref_idx3 = pi1_ref_idx1 + (ps_layer_mv->i4_num_mvs_per_row);
    pi1_ref_idx4 = pi1_ref_idx3 + (ps_layer_mv->i4_num_mvs_per_blk);

    /* Initialize zero mv: default mv used for intra mvs */
    s_zero_mv.s_mv.i2_mvx = 0;
    s_zero_mv.s_mv.i2_mvy = 0;
    s_zero_mv.i1_ref_idx = 0;

    if((ps_search_results->e_cu_size == CU_16x16) && (ps_search_results->u1_split_flag) &&
       (ps_search_results->i4_part_mask & ENABLE_NxN))
    {
        i4_part_type = PRT_NxN;
    }

    for(i = 0; i < ps_prms->i4_num_ref; i++)
    {
        for(j = 0; j < ps_layer_mv->i4_num_mvs_per_ref; j++)
        {
            WORD32 i4_part_id = ge_part_type_to_part_id[i4_part_type][0];

            num_parts = gau1_num_parts_in_part_type[i4_part_type];

            ps_search_node_tl =
                ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id];

            if(num_parts == 1)
            {
                ps_search_node_tr = ps_search_node_tl;
                ps_search_node_bl = ps_search_node_tl;
                ps_search_node_br = ps_search_node_tl;
            }
            else if(num_parts == 2)
            {
                /* For vertically oriented partitions, tl, bl pt to same result */
                /* For horizontally oriented partition, tl, tr pt to same result */
                /* This means for AMP, 2 of the 8x8 blks in mv bank have ambiguous */
                /* result, e.g. for 4x16L. Here left 2 8x8 have the 4x16L partition */
                /* and right 2 8x8 have 12x16R partition */
                if(gau1_is_vert_part[i4_part_type])
                {
                    ps_search_node_tr =
                        ps_search_results
                            ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1];
                    ps_search_node_bl = ps_search_node_tl;
                }
                else
                {
                    ps_search_node_tr = ps_search_node_tl;
                    ps_search_node_bl =
                        ps_search_results
                            ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1];
                }
                ps_search_node_br =
                    ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1];
            }
            else
            {
                /* 4 unique results */
                ps_search_node_tr =
                    ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1];
                ps_search_node_bl =
                    ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 2];
                ps_search_node_br =
                    ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 3];
            }

            if(ps_search_node_tl->s_mv.i2_mvx == INTRA_MV)
                ps_search_node_tl++;
            if(ps_search_node_tr->s_mv.i2_mvx == INTRA_MV)
                ps_search_node_tr++;
            if(ps_search_node_bl->s_mv.i2_mvx == INTRA_MV)
                ps_search_node_bl++;
            if(ps_search_node_br->s_mv.i2_mvx == INTRA_MV)
                ps_search_node_br++;

            COPY_SEARCH_RESULT(ps_mv1, pi1_ref_idx1, ps_search_node_tl, 0);
            ps_mv1++;
            pi1_ref_idx1++;
            COPY_SEARCH_RESULT(ps_mv2, pi1_ref_idx2, ps_search_node_tr, 0);
            ps_mv2++;
            pi1_ref_idx2++;
            COPY_SEARCH_RESULT(ps_mv3, pi1_ref_idx3, ps_search_node_bl, 0);
            ps_mv3++;
            pi1_ref_idx3++;
            COPY_SEARCH_RESULT(ps_mv4, pi1_ref_idx4, ps_search_node_br, 0);
            ps_mv4++;
            pi1_ref_idx4++;

            if(ps_prms->i4_num_results_to_store > 1)
            {
                ps_search_node_tl =
                    &ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id][1];

                if(num_parts == 1)
                {
                    ps_search_node_tr = ps_search_node_tl;
                    ps_search_node_bl = ps_search_node_tl;
                    ps_search_node_br = ps_search_node_tl;
                }
                else if(num_parts == 2)
                {
                    /* For vertically oriented partitions, tl, bl pt to same result */
                    /* For horizontally oriented partition, tl, tr pt to same result */
                    /* This means for AMP, 2 of the 8x8 blks in mv bank have ambiguous */
                    /* result, e.g. for 4x16L. Here left 2 8x8 have the 4x16L partition */
                    /* and right 2 8x8 have 12x16R partition */
                    if(gau1_is_vert_part[i4_part_type])
                    {
                        ps_search_node_tr =
                            &ps_search_results
                                 ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1][1];
                        ps_search_node_bl = ps_search_node_tl;
                    }
                    else
                    {
                        ps_search_node_tr = ps_search_node_tl;
                        ps_search_node_bl =
                            &ps_search_results
                                 ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1][1];
                    }
                    ps_search_node_br =
                        &ps_search_results
                             ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1][1];
                }
                else
                {
                    /* 4 unique results */
                    ps_search_node_tr =
                        &ps_search_results
                             ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 1][1];
                    ps_search_node_bl =
                        &ps_search_results
                             ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 2][1];
                    ps_search_node_br =
                        &ps_search_results
                             ->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id + 3][1];
                }

                if(ps_search_node_tl->s_mv.i2_mvx == INTRA_MV)
                    ps_search_node_tl++;
                if(ps_search_node_tr->s_mv.i2_mvx == INTRA_MV)
                    ps_search_node_tr++;
                if(ps_search_node_bl->s_mv.i2_mvx == INTRA_MV)
                    ps_search_node_bl++;
                if(ps_search_node_br->s_mv.i2_mvx == INTRA_MV)
                    ps_search_node_br++;

                COPY_SEARCH_RESULT(ps_mv1, pi1_ref_idx1, ps_search_node_tl, 0);
                ps_mv1++;
                pi1_ref_idx1++;
                COPY_SEARCH_RESULT(ps_mv2, pi1_ref_idx2, ps_search_node_tr, 0);
                ps_mv2++;
                pi1_ref_idx2++;
                COPY_SEARCH_RESULT(ps_mv3, pi1_ref_idx3, ps_search_node_bl, 0);
                ps_mv3++;
                pi1_ref_idx3++;
                COPY_SEARCH_RESULT(ps_mv4, pi1_ref_idx4, ps_search_node_br, 0);
                ps_mv4++;
                pi1_ref_idx4++;
            }
        }
    }
}

/**
********************************************************************************
*  @fn     hme_update_mv_bank_noencode(search_results_t *ps_search_results,
*                               layer_mv_t *ps_layer_mv,
*                               S32 i4_search_blk_x,
*                               S32 i4_search_blk_y,
*                               mvbank_update_prms_t *ps_prms)
*
*  @brief  Updates the mv bank in case there is no further encodign to be done
*
*  @param[in]  ps_search_results: contains results for the block just searched
*
*  @param[in,out]  ps_layer_mv : Has pointer to mv bank amongst other things
*
*  @param[in] i4_search_blk_x  : col num of blk being searched
*
*  @param[in] i4_search_blk_y : row num of blk being searched
*
*  @param[in] ps_prms : contains certain parameters which govern how updatedone
*
*  @return None
********************************************************************************
*/

void hme_update_mv_bank_in_l1_me(
    search_results_t *ps_search_results,
    layer_mv_t *ps_layer_mv,
    S32 i4_search_blk_x,
    S32 i4_search_blk_y,
    mvbank_update_prms_t *ps_prms)
{
    hme_mv_t *ps_mv;
    hme_mv_t *ps_mv1, *ps_mv2, *ps_mv3, *ps_mv4;
    S08 *pi1_ref_idx, *pi1_ref_idx1, *pi1_ref_idx2, *pi1_ref_idx3, *pi1_ref_idx4;
    S32 i4_blk_x, i4_blk_y, i4_offset;
    S32 i4_j, i4_ref_id;
    search_node_t *ps_search_node;
    search_node_t *ps_search_node_8x8, *ps_search_node_4x4;

    i4_blk_x = i4_search_blk_x << ps_prms->i4_shift;
    i4_blk_y = i4_search_blk_y << ps_prms->i4_shift;
    i4_offset = i4_blk_x + i4_blk_y * ps_layer_mv->i4_num_blks_per_row;

    i4_offset *= ps_layer_mv->i4_num_mvs_per_blk;

    /* Identify the correct offset in the mvbank and the reference id buf */
    ps_mv = ps_layer_mv->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mv->pi1_ref_idx + i4_offset;

    /*************************************************************************/
    /* Supposing we store the mvs in the same blk size as we searched (e.g.  */
    /* we searched 8x8 blks and store results for 8x8 blks), then we can     */
    /* do a straightforward single update of results. This will have a 1-1   */
    /* correspondence.                                                       */
    /*************************************************************************/
    if(ps_layer_mv->e_blk_size == ps_prms->e_search_blk_size)
    {
        search_node_t *aps_result_nodes_sorted[2][MAX_NUM_REF * 2];

        hme_mv_t *ps_mv_l0_root = ps_mv;
        hme_mv_t *ps_mv_l1_root =
            ps_mv + (ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);

        U32 u4_num_l0_results_updated = 0;
        U32 u4_num_l1_results_updated = 0;

        S08 *pi1_ref_idx_l0_root = pi1_ref_idx;
        S08 *pi1_ref_idx_l1_root =
            pi1_ref_idx_l0_root + (ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);

        for(i4_ref_id = 0; i4_ref_id < (S32)ps_prms->i4_num_ref; i4_ref_id++)
        {
            U32 *pu4_num_results_updated;
            search_node_t **pps_result_nodes;

            U08 u1_pred_dir_of_cur_ref = !ps_search_results->pu1_is_past[i4_ref_id];

            if(u1_pred_dir_of_cur_ref)
            {
                pu4_num_results_updated = &u4_num_l1_results_updated;
                pps_result_nodes = &aps_result_nodes_sorted[1][0];
            }
            else
            {
                pu4_num_results_updated = &u4_num_l0_results_updated;
                pps_result_nodes = &aps_result_nodes_sorted[0][0];
            }

            ps_search_node = ps_search_results->aps_part_results[i4_ref_id][PART_ID_2Nx2N];

            for(i4_j = 0; i4_j < ps_layer_mv->i4_num_mvs_per_ref; i4_j++)
            {
                hme_add_new_node_to_a_sorted_array(
                    &ps_search_node[i4_j], pps_result_nodes, NULL, *pu4_num_results_updated, 0);

                ASSERT(ps_search_node[i4_j].i1_ref_idx == i4_ref_id);
                (*pu4_num_results_updated)++;
            }
        }

        for(i4_j = 0; i4_j < (S32)u4_num_l0_results_updated; i4_j++)
        {
            COPY_SEARCH_RESULT(
                &ps_mv_l0_root[i4_j],
                &pi1_ref_idx_l0_root[i4_j],
                aps_result_nodes_sorted[0][i4_j],
                0);
        }

        for(i4_j = 0; i4_j < (S32)u4_num_l1_results_updated; i4_j++)
        {
            COPY_SEARCH_RESULT(
                &ps_mv_l1_root[i4_j],
                &pi1_ref_idx_l1_root[i4_j],
                aps_result_nodes_sorted[1][i4_j],
                0);
        }

        return;
    }

    /*************************************************************************/
    /* Case where search blk size is 8x8, but we update 4x4 results. In this */
    /* case, we need to have NxN partitions enabled in search.               */
    /* Further, we update on a 1-1 basis the 4x4 blk mvs from the respective */
    /* NxN partition. We also update the 8x8 result into each of the 4x4 bank*/
    /*************************************************************************/
    ASSERT(ps_layer_mv->e_blk_size == BLK_4x4);
    ASSERT(ps_prms->e_search_blk_size == BLK_8x8);
    ASSERT((ps_search_results->i4_part_mask & (ENABLE_NxN)) == (ENABLE_NxN));

    /*************************************************************************/
    /* For every 4x4 blk we store corresponding 4x4 results and 1 8x8 result */
    /* hence the below check.                                                */
    /*************************************************************************/
    ASSERT(ps_layer_mv->i4_num_mvs_per_ref <= ps_search_results->u1_num_results_per_part + 1);

    ps_mv1 = ps_mv;
    ps_mv2 = ps_mv1 + ps_layer_mv->i4_num_mvs_per_blk;
    ps_mv3 = ps_mv1 + (ps_layer_mv->i4_num_mvs_per_row);
    ps_mv4 = ps_mv3 + (ps_layer_mv->i4_num_mvs_per_blk);
    pi1_ref_idx1 = pi1_ref_idx;
    pi1_ref_idx2 = pi1_ref_idx1 + ps_layer_mv->i4_num_mvs_per_blk;
    pi1_ref_idx3 = pi1_ref_idx1 + (ps_layer_mv->i4_num_mvs_per_row);
    pi1_ref_idx4 = pi1_ref_idx3 + (ps_layer_mv->i4_num_mvs_per_blk);

    {
        search_node_t *aps_result_nodes_sorted[2][MAX_NUM_REF * 4];
        U08 au1_cost_shifts_for_sorted_node[2][MAX_NUM_REF * 4];

        S32 i;

        hme_mv_t *ps_mv1_l0_root = ps_mv1;
        hme_mv_t *ps_mv1_l1_root =
            ps_mv1 + (ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);
        hme_mv_t *ps_mv2_l0_root = ps_mv2;
        hme_mv_t *ps_mv2_l1_root =
            ps_mv2 + (ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);
        hme_mv_t *ps_mv3_l0_root = ps_mv3;
        hme_mv_t *ps_mv3_l1_root =
            ps_mv3 + (ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);
        hme_mv_t *ps_mv4_l0_root = ps_mv4;
        hme_mv_t *ps_mv4_l1_root =
            ps_mv4 + (ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);

        U32 u4_num_l0_results_updated = 0;
        U32 u4_num_l1_results_updated = 0;

        S08 *pi1_ref_idx1_l0_root = pi1_ref_idx1;
        S08 *pi1_ref_idx1_l1_root = pi1_ref_idx1_l0_root + (ps_prms->i4_num_active_ref_l0 *
                                                            ps_layer_mv->i4_num_mvs_per_ref);
        S08 *pi1_ref_idx2_l0_root = pi1_ref_idx2;
        S08 *pi1_ref_idx2_l1_root = pi1_ref_idx2_l0_root + (ps_prms->i4_num_active_ref_l0 *
                                                            ps_layer_mv->i4_num_mvs_per_ref);
        S08 *pi1_ref_idx3_l0_root = pi1_ref_idx3;
        S08 *pi1_ref_idx3_l1_root = pi1_ref_idx3_l0_root + (ps_prms->i4_num_active_ref_l0 *
                                                            ps_layer_mv->i4_num_mvs_per_ref);
        S08 *pi1_ref_idx4_l0_root = pi1_ref_idx4;
        S08 *pi1_ref_idx4_l1_root = pi1_ref_idx4_l0_root + (ps_prms->i4_num_active_ref_l0 *
                                                            ps_layer_mv->i4_num_mvs_per_ref);

        for(i = 0; i < 4; i++)
        {
            hme_mv_t *ps_mv_l0_root;
            hme_mv_t *ps_mv_l1_root;

            S08 *pi1_ref_idx_l0_root;
            S08 *pi1_ref_idx_l1_root;

            for(i4_ref_id = 0; i4_ref_id < ps_search_results->u1_num_active_ref; i4_ref_id++)
            {
                U32 *pu4_num_results_updated;
                search_node_t **pps_result_nodes;
                U08 *pu1_cost_shifts_for_sorted_node;

                U08 u1_pred_dir_of_cur_ref = !ps_search_results->pu1_is_past[i4_ref_id];

                if(u1_pred_dir_of_cur_ref)
                {
                    pu4_num_results_updated = &u4_num_l1_results_updated;
                    pps_result_nodes = &aps_result_nodes_sorted[1][0];
                    pu1_cost_shifts_for_sorted_node = &au1_cost_shifts_for_sorted_node[1][0];
                }
                else
                {
                    pu4_num_results_updated = &u4_num_l0_results_updated;
                    pps_result_nodes = &aps_result_nodes_sorted[0][0];
                    pu1_cost_shifts_for_sorted_node = &au1_cost_shifts_for_sorted_node[1][0];
                }

                ps_search_node_8x8 = ps_search_results->aps_part_results[i4_ref_id][PART_ID_2Nx2N];

                ps_search_node_4x4 =
                    ps_search_results->aps_part_results[i4_ref_id][PART_ID_NxN_TL + i];

                for(i4_j = 0; i4_j < ps_layer_mv->i4_num_mvs_per_ref; i4_j++)
                {
                    hme_add_new_node_to_a_sorted_array(
                        &ps_search_node_4x4[i4_j],
                        pps_result_nodes,
                        pu1_cost_shifts_for_sorted_node,
                        *pu4_num_results_updated,
                        0);

                    (*pu4_num_results_updated)++;

                    hme_add_new_node_to_a_sorted_array(
                        &ps_search_node_8x8[i4_j],
                        pps_result_nodes,
                        pu1_cost_shifts_for_sorted_node,
                        *pu4_num_results_updated,
                        2);

                    (*pu4_num_results_updated)++;
                }
            }

            switch(i)
            {
            case 0:
            {
                ps_mv_l0_root = ps_mv1_l0_root;
                ps_mv_l1_root = ps_mv1_l1_root;

                pi1_ref_idx_l0_root = pi1_ref_idx1_l0_root;
                pi1_ref_idx_l1_root = pi1_ref_idx1_l1_root;

                break;
            }
            case 1:
            {
                ps_mv_l0_root = ps_mv2_l0_root;
                ps_mv_l1_root = ps_mv2_l1_root;

                pi1_ref_idx_l0_root = pi1_ref_idx2_l0_root;
                pi1_ref_idx_l1_root = pi1_ref_idx2_l1_root;

                break;
            }
            case 2:
            {
                ps_mv_l0_root = ps_mv3_l0_root;
                ps_mv_l1_root = ps_mv3_l1_root;

                pi1_ref_idx_l0_root = pi1_ref_idx3_l0_root;
                pi1_ref_idx_l1_root = pi1_ref_idx3_l1_root;

                break;
            }
            case 3:
            {
                ps_mv_l0_root = ps_mv4_l0_root;
                ps_mv_l1_root = ps_mv4_l1_root;

                pi1_ref_idx_l0_root = pi1_ref_idx4_l0_root;
                pi1_ref_idx_l1_root = pi1_ref_idx4_l1_root;

                break;
            }
            }

            u4_num_l0_results_updated =
                MIN((S32)u4_num_l0_results_updated,
                    ps_prms->i4_num_active_ref_l0 * ps_layer_mv->i4_num_mvs_per_ref);

            u4_num_l1_results_updated =
                MIN((S32)u4_num_l1_results_updated,
                    ps_prms->i4_num_active_ref_l1 * ps_layer_mv->i4_num_mvs_per_ref);

            for(i4_j = 0; i4_j < (S32)u4_num_l0_results_updated; i4_j++)
            {
                COPY_SEARCH_RESULT(
                    &ps_mv_l0_root[i4_j],
                    &pi1_ref_idx_l0_root[i4_j],
                    aps_result_nodes_sorted[0][i4_j],
                    0);
            }

            for(i4_j = 0; i4_j < (S32)u4_num_l1_results_updated; i4_j++)
            {
                COPY_SEARCH_RESULT(
                    &ps_mv_l1_root[i4_j],
                    &pi1_ref_idx_l1_root[i4_j],
                    aps_result_nodes_sorted[1][i4_j],
                    0);
            }
        }
    }
}

/**
******************************************************************************
*  @brief Scales motion vector component projecte from a diff layer in same
*         picture (so no ref id related delta poc scaling required)
******************************************************************************
*/

#define SCALE_MV_COMP_RES(mvcomp_p, dim_c, dim_p)                                                  \
    ((((mvcomp_p) * (dim_c)) + ((SIGN((mvcomp_p)) * (dim_p)) >> 1)) / (dim_p))
/**
********************************************************************************
*  @fn     hme_project_coloc_candt(search_node_t *ps_search_node,
*                                   layer_ctxt_t *ps_curr_layer,
*                                   layer_ctxt_t *ps_coarse_layer,
*                                   S32 i4_pos_x,
*                                   S32 i4_pos_y,
*                                   S08 i1_ref_id,
*                                   S08 i1_result_id)
*
*  @brief  From a coarser layer, projects a candidated situated at "colocated"
*          position in the picture (e.g. given x, y it will be x/2, y/2 dyadic
*
*  @param[out]  ps_search_node : contains the projected result
*
*  @param[in]   ps_curr_layer : current layer context
*
*  @param[in]   ps_coarse_layer  : coarser layer context
*
*  @param[in]   i4_pos_x  : x Position where mv is required (w.r.t. curr layer)
*
*  @param[in]   i4_pos_y  : y Position where mv is required (w.r.t. curr layer)
*
*  @param[in]   i1_ref_id : reference id for which the candidate required
*
*  @param[in]   i4_result_id : result id for which the candidate required
*                              (0 : best result, 1 : next best)
*
*  @return None
********************************************************************************
*/

void hme_project_coloc_candt(
    search_node_t *ps_search_node,
    layer_ctxt_t *ps_curr_layer,
    layer_ctxt_t *ps_coarse_layer,
    S32 i4_pos_x,
    S32 i4_pos_y,
    S08 i1_ref_id,
    S32 i4_result_id)
{
    S32 wd_c, ht_c, wd_p, ht_p;
    S32 blksize_p, blk_x, blk_y, i4_offset;
    layer_mv_t *ps_layer_mvbank;
    hme_mv_t *ps_mv;
    S08 *pi1_ref_idx;

    /* Width and ht of current and prev layers */
    wd_c = ps_curr_layer->i4_wd;
    ht_c = ps_curr_layer->i4_ht;
    wd_p = ps_coarse_layer->i4_wd;
    ht_p = ps_coarse_layer->i4_ht;

    ps_layer_mvbank = ps_coarse_layer->ps_layer_mvbank;
    blksize_p = (S32)gau1_blk_size_to_wd[ps_layer_mvbank->e_blk_size];

    /* Safety check to avoid uninitialized access across temporal layers */
    i4_pos_x = CLIP3(i4_pos_x, 0, (wd_c - blksize_p));
    i4_pos_y = CLIP3(i4_pos_y, 0, (ht_c - blksize_p));

    /* Project the positions to prev layer */
    /* TODO: convert these to scale factors at pic level */
    blk_x = (i4_pos_x * wd_p) / (wd_c * blksize_p);
    blk_y = (i4_pos_y * ht_p) / (ht_c * blksize_p);

    /* Pick up the mvs from the location */
    i4_offset = (blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
    i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y);

    ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

    ps_mv += (i1_ref_id * ps_layer_mvbank->i4_num_mvs_per_ref);
    pi1_ref_idx += (i1_ref_id * ps_layer_mvbank->i4_num_mvs_per_ref);

    ps_search_node->s_mv.i2_mvx = SCALE_MV_COMP_RES(ps_mv[i4_result_id].i2_mv_x, wd_c, wd_p);
    ps_search_node->s_mv.i2_mvy = SCALE_MV_COMP_RES(ps_mv[i4_result_id].i2_mv_y, ht_c, ht_p);
    ps_search_node->i1_ref_idx = pi1_ref_idx[i4_result_id];
    ps_search_node->u1_subpel_done = 0;
    if((ps_search_node->i1_ref_idx < 0) || (ps_search_node->s_mv.i2_mvx == INTRA_MV))
    {
        ps_search_node->i1_ref_idx = i1_ref_id;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
    }
}

/**
********************************************************************************
*  @fn     hme_project_coloc_candt_dyadic(search_node_t *ps_search_node,
*                                   layer_ctxt_t *ps_curr_layer,
*                                   layer_ctxt_t *ps_coarse_layer,
*                                   S32 i4_pos_x,
*                                   S32 i4_pos_y,
*                                   S08 i1_ref_id,
*                                   S08 i1_result_id)
*
*  @brief  From a coarser layer, projects a candidated situated at "colocated"
*          position in the picture when the ratios are dyadic
*
*  @param[out]  ps_search_node : contains the projected result
*
*  @param[in]   ps_curr_layer : current layer context
*
*  @param[in]   ps_coarse_layer  : coarser layer context
*
*  @param[in]   i4_pos_x  : x Position where mv is required (w.r.t. curr layer)
*
*  @param[in]   i4_pos_y  : y Position where mv is required (w.r.t. curr layer)
*
*  @param[in]   i1_ref_id : reference id for which the candidate required
*
*  @param[in]   i4_result_id : result id for which the candidate required
*                              (0 : best result, 1 : next best)
*
*  @return None
********************************************************************************
*/

void hme_project_coloc_candt_dyadic(
    search_node_t *ps_search_node,
    layer_ctxt_t *ps_curr_layer,
    layer_ctxt_t *ps_coarse_layer,
    S32 i4_pos_x,
    S32 i4_pos_y,
    S08 i1_ref_id,
    S32 i4_result_id)
{
    S32 wd_c, ht_c, wd_p, ht_p;
    S32 blksize_p, blk_x, blk_y, i4_offset;
    layer_mv_t *ps_layer_mvbank;
    hme_mv_t *ps_mv;
    S08 *pi1_ref_idx;

    /* Width and ht of current and prev layers */
    wd_c = ps_curr_layer->i4_wd;
    ht_c = ps_curr_layer->i4_ht;
    wd_p = ps_coarse_layer->i4_wd;
    ht_p = ps_coarse_layer->i4_ht;

    ps_layer_mvbank = ps_coarse_layer->ps_layer_mvbank;
    /* blksize_p = log2(wd) + 1 */
    blksize_p = (S32)gau1_blk_size_to_wd_shift[ps_layer_mvbank->e_blk_size];

    /* ASSERT for valid sizes */
    ASSERT((blksize_p == 3) || (blksize_p == 4) || (blksize_p == 5));

    /* Safety check to avoid uninitialized access across temporal layers */
    i4_pos_x = CLIP3(i4_pos_x, 0, (wd_c - blksize_p));
    i4_pos_y = CLIP3(i4_pos_y, 0, (ht_c - blksize_p));

    /* Project the positions to prev layer */
    /* TODO: convert these to scale factors at pic level */
    blk_x = i4_pos_x >> blksize_p;  // (2 * blksize_p);
    blk_y = i4_pos_y >> blksize_p;  // (2 * blksize_p);

    /* Pick up the mvs from the location */
    i4_offset = (blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
    i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y);

    ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

    ps_mv += (i1_ref_id * ps_layer_mvbank->i4_num_mvs_per_ref);
    pi1_ref_idx += (i1_ref_id * ps_layer_mvbank->i4_num_mvs_per_ref);

    ps_search_node->s_mv.i2_mvx = ps_mv[i4_result_id].i2_mv_x << 1;
    ps_search_node->s_mv.i2_mvy = ps_mv[i4_result_id].i2_mv_y << 1;
    ps_search_node->i1_ref_idx = pi1_ref_idx[i4_result_id];
    if((ps_search_node->i1_ref_idx < 0) || (ps_search_node->s_mv.i2_mvx == INTRA_MV))
    {
        ps_search_node->i1_ref_idx = i1_ref_id;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
    }
}

void hme_project_coloc_candt_dyadic_implicit(
    search_node_t *ps_search_node,
    layer_ctxt_t *ps_curr_layer,
    layer_ctxt_t *ps_coarse_layer,
    S32 i4_pos_x,
    S32 i4_pos_y,
    S32 i4_num_act_ref_l0,
    U08 u1_pred_dir,
    U08 u1_default_ref_id,
    S32 i4_result_id)
{
    S32 wd_c, ht_c, wd_p, ht_p;
    S32 blksize_p, blk_x, blk_y, i4_offset;
    layer_mv_t *ps_layer_mvbank;
    hme_mv_t *ps_mv;
    S08 *pi1_ref_idx;

    /* Width and ht of current and prev layers */
    wd_c = ps_curr_layer->i4_wd;
    ht_c = ps_curr_layer->i4_ht;
    wd_p = ps_coarse_layer->i4_wd;
    ht_p = ps_coarse_layer->i4_ht;

    ps_layer_mvbank = ps_coarse_layer->ps_layer_mvbank;
    blksize_p = (S32)gau1_blk_size_to_wd_shift[ps_layer_mvbank->e_blk_size];

    /* ASSERT for valid sizes */
    ASSERT((blksize_p == 3) || (blksize_p == 4) || (blksize_p == 5));

    /* Safety check to avoid uninitialized access across temporal layers */
    i4_pos_x = CLIP3(i4_pos_x, 0, (wd_c - blksize_p));
    i4_pos_y = CLIP3(i4_pos_y, 0, (ht_c - blksize_p));
    /* Project the positions to prev layer */
    /* TODO: convert these to scale factors at pic level */
    blk_x = i4_pos_x >> blksize_p;  // (2 * blksize_p);
    blk_y = i4_pos_y >> blksize_p;  // (2 * blksize_p);

    /* Pick up the mvs from the location */
    i4_offset = (blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
    i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y);

    ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

    if(u1_pred_dir == 1)
    {
        ps_mv += (i4_num_act_ref_l0 * ps_layer_mvbank->i4_num_mvs_per_ref);
        pi1_ref_idx += (i4_num_act_ref_l0 * ps_layer_mvbank->i4_num_mvs_per_ref);
    }

    ps_search_node->s_mv.i2_mvx = ps_mv[i4_result_id].i2_mv_x << 1;
    ps_search_node->s_mv.i2_mvy = ps_mv[i4_result_id].i2_mv_y << 1;
    ps_search_node->i1_ref_idx = pi1_ref_idx[i4_result_id];
    if((ps_search_node->i1_ref_idx < 0) || (ps_search_node->s_mv.i2_mvx == INTRA_MV))
    {
        ps_search_node->i1_ref_idx = u1_default_ref_id;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
    }
}

#define SCALE_RANGE_PRMS(prm1, prm2, shift)                                                        \
    {                                                                                              \
        prm1.i2_min_x = prm2.i2_min_x << shift;                                                    \
        prm1.i2_max_x = prm2.i2_max_x << shift;                                                    \
        prm1.i2_min_y = prm2.i2_min_y << shift;                                                    \
        prm1.i2_max_y = prm2.i2_max_y << shift;                                                    \
    }

#define SCALE_RANGE_PRMS_POINTERS(prm1, prm2, shift)                                               \
    {                                                                                              \
        prm1->i2_min_x = prm2->i2_min_x << shift;                                                  \
        prm1->i2_max_x = prm2->i2_max_x << shift;                                                  \
        prm1->i2_min_y = prm2->i2_min_y << shift;                                                  \
        prm1->i2_max_y = prm2->i2_max_y << shift;                                                  \
    }

/**
********************************************************************************
*  @fn   void hme_refine_frm_init(me_ctxt_t *ps_ctxt,
*                       refine_layer_prms_t *ps_refine_prms)
*
*  @brief  Frame init of refinemnet layers in ME
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_refine_prms : refinement layer prms
*
*  @return None
********************************************************************************
*/
void hme_refine_frm_init(
    layer_ctxt_t *ps_curr_layer, refine_prms_t *ps_refine_prms, layer_ctxt_t *ps_coarse_layer)
{
    /* local variables */
    BLK_SIZE_T e_result_blk_size = BLK_8x8;
    S32 i4_num_ref_fpel, i4_num_ref_prev_layer;

    i4_num_ref_prev_layer = ps_coarse_layer->ps_layer_mvbank->i4_num_ref;

    if(ps_refine_prms->explicit_ref)
    {
        i4_num_ref_fpel = i4_num_ref_prev_layer;
    }
    else
    {
        i4_num_ref_fpel = 2;
    }

    if(ps_refine_prms->i4_enable_4x4_part)
    {
        e_result_blk_size = BLK_4x4;
    }

    i4_num_ref_fpel = MIN(i4_num_ref_fpel, i4_num_ref_prev_layer);

    hme_init_mv_bank(
        ps_curr_layer,
        e_result_blk_size,
        i4_num_ref_fpel,
        ps_refine_prms->i4_num_mvbank_results,
        ps_refine_prms->i4_layer_id > 0 ? 0 : 1);
}

#if 1  //ENABLE_CU_RECURSION || TEST_AND_EVALUATE_CU_RECURSION
/**
********************************************************************************
*  @fn   void hme_init_clusters_16x16
*               (
*                   cluster_16x16_blk_t *ps_cluster_blk_16x16
*               )
*
*  @brief  Intialisations for the structs used in clustering algorithm
*
*  @param[in/out]  ps_cluster_blk_16x16: pointer to structure containing clusters
*                                        of 16x16 block
*
*  @return None
********************************************************************************
*/
static __inline void
    hme_init_clusters_16x16(cluster_16x16_blk_t *ps_cluster_blk_16x16, S32 bidir_enabled)
{
    S32 i;

    ps_cluster_blk_16x16->num_clusters = 0;
    ps_cluster_blk_16x16->intra_mv_area = 0;
    ps_cluster_blk_16x16->best_inter_cost = 0;

    for(i = 0; i < MAX_NUM_CLUSTERS_16x16; i++)
    {
        ps_cluster_blk_16x16->as_cluster_data[i].max_dist_from_centroid =
            bidir_enabled ? MAX_DISTANCE_FROM_CENTROID_16x16_B : MAX_DISTANCE_FROM_CENTROID_16x16;

        ps_cluster_blk_16x16->as_cluster_data[i].is_valid_cluster = 0;

        ps_cluster_blk_16x16->as_cluster_data[i].bi_mv_pixel_area = 0;
        ps_cluster_blk_16x16->as_cluster_data[i].uni_mv_pixel_area = 0;
    }
    for(i = 0; i < MAX_NUM_REF; i++)
    {
        ps_cluster_blk_16x16->au1_num_clusters[i] = 0;
    }
}

/**
********************************************************************************
*  @fn   void hme_init_clusters_32x32
*               (
*                   cluster_32x32_blk_t *ps_cluster_blk_32x32
*               )
*
*  @brief  Intialisations for the structs used in clustering algorithm
*
*  @param[in/out]  ps_cluster_blk_32x32: pointer to structure containing clusters
*                                        of 32x32 block
*
*  @return None
********************************************************************************
*/
static __inline void
    hme_init_clusters_32x32(cluster_32x32_blk_t *ps_cluster_blk_32x32, S32 bidir_enabled)
{
    S32 i;

    ps_cluster_blk_32x32->num_clusters = 0;
    ps_cluster_blk_32x32->intra_mv_area = 0;
    ps_cluster_blk_32x32->best_alt_ref = -1;
    ps_cluster_blk_32x32->best_uni_ref = -1;
    ps_cluster_blk_32x32->best_inter_cost = 0;
    ps_cluster_blk_32x32->num_clusters_with_weak_sdi_density = 0;

    for(i = 0; i < MAX_NUM_CLUSTERS_32x32; i++)
    {
        ps_cluster_blk_32x32->as_cluster_data[i].max_dist_from_centroid =
            bidir_enabled ? MAX_DISTANCE_FROM_CENTROID_32x32_B : MAX_DISTANCE_FROM_CENTROID_32x32;
        ps_cluster_blk_32x32->as_cluster_data[i].is_valid_cluster = 0;

        ps_cluster_blk_32x32->as_cluster_data[i].bi_mv_pixel_area = 0;
        ps_cluster_blk_32x32->as_cluster_data[i].uni_mv_pixel_area = 0;
    }
    for(i = 0; i < MAX_NUM_REF; i++)
    {
        ps_cluster_blk_32x32->au1_num_clusters[i] = 0;
    }
}

/**
********************************************************************************
*  @fn   void hme_init_clusters_64x64
*               (
*                   cluster_64x64_blk_t *ps_cluster_blk_64x64
*               )
*
*  @brief  Intialisations for the structs used in clustering algorithm
*
*  @param[in/out]  ps_cluster_blk_64x64: pointer to structure containing clusters
*                                        of 64x64 block
*
*  @return None
********************************************************************************
*/
static __inline void
    hme_init_clusters_64x64(cluster_64x64_blk_t *ps_cluster_blk_64x64, S32 bidir_enabled)
{
    S32 i;

    ps_cluster_blk_64x64->num_clusters = 0;
    ps_cluster_blk_64x64->intra_mv_area = 0;
    ps_cluster_blk_64x64->best_alt_ref = -1;
    ps_cluster_blk_64x64->best_uni_ref = -1;
    ps_cluster_blk_64x64->best_inter_cost = 0;

    for(i = 0; i < MAX_NUM_CLUSTERS_64x64; i++)
    {
        ps_cluster_blk_64x64->as_cluster_data[i].max_dist_from_centroid =
            bidir_enabled ? MAX_DISTANCE_FROM_CENTROID_64x64_B : MAX_DISTANCE_FROM_CENTROID_64x64;
        ps_cluster_blk_64x64->as_cluster_data[i].is_valid_cluster = 0;

        ps_cluster_blk_64x64->as_cluster_data[i].bi_mv_pixel_area = 0;
        ps_cluster_blk_64x64->as_cluster_data[i].uni_mv_pixel_area = 0;
    }
    for(i = 0; i < MAX_NUM_REF; i++)
    {
        ps_cluster_blk_64x64->au1_num_clusters[i] = 0;
    }
}

/**
********************************************************************************
*  @fn   void hme_sort_and_assign_top_ref_ids_areawise
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info
*               )
*
*  @brief  Finds best_uni_ref and best_alt_ref
*
*  @param[in/out]  ps_ctb_cluster_info: structure that points to ctb data
*
*  @param[in]  bidir_enabled: flag that indicates whether or not bi-pred is
*                             enabled
*
*  @param[in]  block_width: width of the block in pels
*
*  @param[in]  e_cu_pos: position of the block within the CTB
*
*  @return None
********************************************************************************
*/
void hme_sort_and_assign_top_ref_ids_areawise(
    ctb_cluster_info_t *ps_ctb_cluster_info, S32 bidir_enabled, S32 block_width, CU_POS_T e_cu_pos)
{
    cluster_32x32_blk_t *ps_32x32 = NULL;
    cluster_64x64_blk_t *ps_64x64 = NULL;
    cluster_data_t *ps_data;

    S32 j, k;

    S32 ai4_uni_area[MAX_NUM_REF];
    S32 ai4_bi_area[MAX_NUM_REF];
    S32 ai4_ref_id_found[MAX_NUM_REF];
    S32 ai4_ref_id[MAX_NUM_REF];

    S32 best_uni_ref = -1, best_alt_ref = -1;
    S32 num_clusters;
    S32 num_ref = 0;
    S32 num_clusters_evaluated = 0;
    S32 is_cur_blk_valid;

    if(32 == block_width)
    {
        is_cur_blk_valid = (ps_ctb_cluster_info->blk_32x32_mask & (1 << e_cu_pos)) || 0;
        ps_32x32 = &ps_ctb_cluster_info->ps_32x32_blk[e_cu_pos];
        num_clusters = ps_32x32->num_clusters;
        ps_data = &ps_32x32->as_cluster_data[0];
    }
    else
    {
        is_cur_blk_valid = (ps_ctb_cluster_info->blk_32x32_mask == 0xf);
        ps_64x64 = ps_ctb_cluster_info->ps_64x64_blk;
        num_clusters = ps_64x64->num_clusters;
        ps_data = &ps_64x64->as_cluster_data[0];
    }

#if !ENABLE_4CTB_EVALUATION
    if((num_clusters > MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK))
    {
        return;
    }
#endif
    if(num_clusters == 0)
    {
        return;
    }
    else if(!is_cur_blk_valid)
    {
        return;
    }

    memset(ai4_uni_area, 0, sizeof(S32) * MAX_NUM_REF);
    memset(ai4_bi_area, 0, sizeof(S32) * MAX_NUM_REF);
    memset(ai4_ref_id_found, 0, sizeof(S32) * MAX_NUM_REF);
    memset(ai4_ref_id, -1, sizeof(S32) * MAX_NUM_REF);

    for(j = 0; num_clusters_evaluated < num_clusters; j++, ps_data++)
    {
        S32 ref_id;

        if(!ps_data->is_valid_cluster)
        {
            continue;
        }

        ref_id = ps_data->ref_id;

        num_clusters_evaluated++;

        ai4_uni_area[ref_id] += ps_data->uni_mv_pixel_area;
        ai4_bi_area[ref_id] += ps_data->bi_mv_pixel_area;

        if(!ai4_ref_id_found[ref_id])
        {
            ai4_ref_id[ref_id] = ref_id;
            ai4_ref_id_found[ref_id] = 1;
            num_ref++;
        }
    }

    {
        S32 ai4_ref_id_temp[MAX_NUM_REF];

        memcpy(ai4_ref_id_temp, ai4_ref_id, sizeof(S32) * MAX_NUM_REF);

        for(k = 1; k < MAX_NUM_REF; k++)
        {
            if(ai4_uni_area[k] > ai4_uni_area[0])
            {
                SWAP_HME(ai4_uni_area[k], ai4_uni_area[0], S32);
                SWAP_HME(ai4_ref_id_temp[k], ai4_ref_id_temp[0], S32);
            }
        }

        best_uni_ref = ai4_ref_id_temp[0];
    }

    if(bidir_enabled)
    {
        for(k = 1; k < MAX_NUM_REF; k++)
        {
            if(ai4_bi_area[k] > ai4_bi_area[0])
            {
                SWAP_HME(ai4_bi_area[k], ai4_bi_area[0], S32);
                SWAP_HME(ai4_ref_id[k], ai4_ref_id[0], S32);
            }
        }

        if(!ai4_bi_area[0])
        {
            best_alt_ref = -1;

            if(32 == block_width)
            {
                SET_VALUES_FOR_TOP_REF_IDS(ps_32x32, best_uni_ref, best_alt_ref, num_ref);
            }
            else
            {
                SET_VALUES_FOR_TOP_REF_IDS(ps_64x64, best_uni_ref, best_alt_ref, num_ref);
            }

            return;
        }

        if(best_uni_ref == ai4_ref_id[0])
        {
            for(k = 2; k < MAX_NUM_REF; k++)
            {
                if(ai4_bi_area[k] > ai4_bi_area[1])
                {
                    SWAP_HME(ai4_bi_area[k], ai4_bi_area[1], S32);
                    SWAP_HME(ai4_ref_id[k], ai4_ref_id[1], S32);
                }
            }

            best_alt_ref = ai4_ref_id[1];
        }
        else
        {
            best_alt_ref = ai4_ref_id[0];
        }
    }

    if(32 == block_width)
    {
        SET_VALUES_FOR_TOP_REF_IDS(ps_32x32, best_uni_ref, best_alt_ref, num_ref);
    }
    else
    {
        SET_VALUES_FOR_TOP_REF_IDS(ps_64x64, best_uni_ref, best_alt_ref, num_ref);
    }
}

/**
********************************************************************************
*  @fn   void hme_find_top_ref_ids
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info
*               )
*
*  @brief  Finds best_uni_ref and best_alt_ref
*
*  @param[in/out]  ps_ctb_cluster_info: structure that points to ctb data
*
*  @return None
********************************************************************************
*/
void hme_find_top_ref_ids(
    ctb_cluster_info_t *ps_ctb_cluster_info, S32 bidir_enabled, S32 block_width)
{
    S32 i;

    if(32 == block_width)
    {
        for(i = 0; i < 4; i++)
        {
            hme_sort_and_assign_top_ref_ids_areawise(
                ps_ctb_cluster_info, bidir_enabled, block_width, (CU_POS_T)i);
        }
    }
    else if(64 == block_width)
    {
        hme_sort_and_assign_top_ref_ids_areawise(
            ps_ctb_cluster_info, bidir_enabled, block_width, POS_NA);
    }
}

/**
********************************************************************************
*  @fn   void hme_boot_out_outlier
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info
*               )
*
*  @brief  Removes outlier clusters before CU tree population
*
*  @param[in/out]  ps_ctb_cluster_info: structure that points to ctb data
*
*  @return None
********************************************************************************
*/
void hme_boot_out_outlier(ctb_cluster_info_t *ps_ctb_cluster_info, S32 blk_width)
{
    cluster_32x32_blk_t *ps_32x32;

    S32 i;

    cluster_64x64_blk_t *ps_64x64 = &ps_ctb_cluster_info->ps_64x64_blk[0];

    S32 sdi_threshold = ps_ctb_cluster_info->sdi_threshold;

    if(32 == blk_width)
    {
        /* 32x32 clusters */
        for(i = 0; i < 4; i++)
        {
            ps_32x32 = &ps_ctb_cluster_info->ps_32x32_blk[i];

            if(ps_32x32->num_clusters > MAX_NUM_CLUSTERS_IN_ONE_REF_IDX)
            {
                BUMP_OUTLIER_CLUSTERS(ps_32x32, sdi_threshold);
            }
        }
    }
    else if(64 == blk_width)
    {
        /* 64x64 clusters */
        if(ps_64x64->num_clusters > MAX_NUM_CLUSTERS_IN_ONE_REF_IDX)
        {
            BUMP_OUTLIER_CLUSTERS(ps_64x64, sdi_threshold);
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_update_cluster_attributes
*               (
*                   cluster_data_t *ps_cluster_data,
*                   S32 mvx,
*                   S32 mvy,
*                   PART_ID_T e_part_id
*               )
*
*  @brief  Implementation fo the clustering algorithm
*
*  @param[in/out]  ps_cluster_data: pointer to cluster_data_t struct
*
*  @param[in]  mvx : x co-ordinate of the motion vector
*
*  @param[in]  mvy : y co-ordinate of the motion vector
*
*  @param[in]  ref_idx : ref_id of the motion vector
*
*  @param[in]  e_part_id : partition id of the motion vector
*
*  @return None
********************************************************************************
*/
static __inline void hme_update_cluster_attributes(
    cluster_data_t *ps_cluster_data,
    S32 mvx,
    S32 mvy,
    S32 mvdx,
    S32 mvdy,
    S32 ref_id,
    S32 sdi,
    U08 is_part_of_bi,
    PART_ID_T e_part_id)
{
    LWORD64 i8_mvx_sum_q8;
    LWORD64 i8_mvy_sum_q8;

    S32 centroid_posx_q8 = ps_cluster_data->s_centroid.i4_pos_x_q8;
    S32 centroid_posy_q8 = ps_cluster_data->s_centroid.i4_pos_y_q8;

    if((mvdx > 0) && (ps_cluster_data->min_x > mvx))
    {
        ps_cluster_data->min_x = mvx;
    }
    else if((mvdx < 0) && (ps_cluster_data->max_x < mvx))
    {
        ps_cluster_data->max_x = mvx;
    }

    if((mvdy > 0) && (ps_cluster_data->min_y > mvy))
    {
        ps_cluster_data->min_y = mvy;
    }
    else if((mvdy < 0) && (ps_cluster_data->max_y < mvy))
    {
        ps_cluster_data->max_y = mvy;
    }

    {
        S32 num_mvs = ps_cluster_data->num_mvs;

        ps_cluster_data->as_mv[num_mvs].pixel_count = gai4_partition_area[e_part_id];
        ps_cluster_data->as_mv[num_mvs].mvx = mvx;
        ps_cluster_data->as_mv[num_mvs].mvy = mvy;

        /***************************/
        ps_cluster_data->as_mv[num_mvs].is_uni = !is_part_of_bi;
        ps_cluster_data->as_mv[num_mvs].sdi = sdi;
        /**************************/
    }

    /* Updation of centroid */
    {
        i8_mvx_sum_q8 = (LWORD64)centroid_posx_q8 * ps_cluster_data->num_mvs + (mvx << 8);
        i8_mvy_sum_q8 = (LWORD64)centroid_posy_q8 * ps_cluster_data->num_mvs + (mvy << 8);

        ps_cluster_data->num_mvs++;

        ps_cluster_data->s_centroid.i4_pos_x_q8 =
            (WORD32)((i8_mvx_sum_q8) / ps_cluster_data->num_mvs);
        ps_cluster_data->s_centroid.i4_pos_y_q8 =
            (WORD32)((i8_mvy_sum_q8) / ps_cluster_data->num_mvs);
    }

    ps_cluster_data->area_in_pixels += gai4_partition_area[e_part_id];

    if(is_part_of_bi)
    {
        ps_cluster_data->bi_mv_pixel_area += gai4_partition_area[e_part_id];
    }
    else
    {
        ps_cluster_data->uni_mv_pixel_area += gai4_partition_area[e_part_id];
    }
}

/**
********************************************************************************
*  @fn   void hme_try_cluster_merge
*               (
*                   cluster_data_t *ps_cluster_data,
*                   S32 *pi4_num_clusters,
*                   S32 idx_of_updated_cluster
*               )
*
*  @brief  Implementation fo the clustering algorithm
*
*  @param[in/out]  ps_cluster_data: pointer to cluster_data_t struct
*
*  @param[in/out]  pi4_num_clusters : pointer to number of clusters
*
*  @param[in]  idx_of_updated_cluster : index of the cluster most recently
*                                       updated
*
*  @return Nothing
********************************************************************************
*/
void hme_try_cluster_merge(
    cluster_data_t *ps_cluster_data, U08 *pu1_num_clusters, S32 idx_of_updated_cluster)
{
    centroid_t *ps_centroid;

    S32 cur_pos_x_q8;
    S32 cur_pos_y_q8;
    S32 i;
    S32 max_dist_from_centroid;
    S32 mvd;
    S32 mvdx_q8;
    S32 mvdx;
    S32 mvdy_q8;
    S32 mvdy;
    S32 num_clusters, num_clusters_evaluated;
    S32 other_pos_x_q8;
    S32 other_pos_y_q8;

    cluster_data_t *ps_root = ps_cluster_data;
    cluster_data_t *ps_cur_cluster = &ps_cluster_data[idx_of_updated_cluster];
    centroid_t *ps_cur_centroid = &ps_cur_cluster->s_centroid;

    /* Merge is superfluous if num_clusters is 1 */
    if(*pu1_num_clusters == 1)
    {
        return;
    }

    cur_pos_x_q8 = ps_cur_centroid->i4_pos_x_q8;
    cur_pos_y_q8 = ps_cur_centroid->i4_pos_y_q8;

    max_dist_from_centroid = ps_cur_cluster->max_dist_from_centroid;

    num_clusters = *pu1_num_clusters;
    num_clusters_evaluated = 0;

    for(i = 0; num_clusters_evaluated < num_clusters; i++, ps_cluster_data++)
    {
        if(!ps_cluster_data->is_valid_cluster)
        {
            continue;
        }
        if((ps_cluster_data->ref_id != ps_cur_cluster->ref_id) || (i == idx_of_updated_cluster))
        {
            num_clusters_evaluated++;
            continue;
        }

        ps_centroid = &ps_cluster_data->s_centroid;

        other_pos_x_q8 = ps_centroid->i4_pos_x_q8;
        other_pos_y_q8 = ps_centroid->i4_pos_y_q8;

        mvdx_q8 = (cur_pos_x_q8 - other_pos_x_q8);
        mvdy_q8 = (cur_pos_y_q8 - other_pos_y_q8);
        mvdx = (mvdx_q8 + (1 << 7)) >> 8;
        mvdy = (mvdy_q8 + (1 << 7)) >> 8;

        mvd = ABS(mvdx) + ABS(mvdy);

        if(mvd <= (max_dist_from_centroid >> 1))
        {
            /* 0 => no updates */
            /* 1 => min updated */
            /* 2 => max updated */
            S32 minmax_x_update_id;
            S32 minmax_y_update_id;

            LWORD64 i8_mv_x_sum_self = (LWORD64)cur_pos_x_q8 * ps_cur_cluster->num_mvs;
            LWORD64 i8_mv_y_sum_self = (LWORD64)cur_pos_y_q8 * ps_cur_cluster->num_mvs;
            LWORD64 i8_mv_x_sum_cousin = (LWORD64)other_pos_x_q8 * ps_cluster_data->num_mvs;
            LWORD64 i8_mv_y_sum_cousin = (LWORD64)other_pos_y_q8 * ps_cluster_data->num_mvs;

            (*pu1_num_clusters)--;

            ps_cluster_data->is_valid_cluster = 0;

            memcpy(
                &ps_cur_cluster->as_mv[ps_cur_cluster->num_mvs],
                ps_cluster_data->as_mv,
                sizeof(mv_data_t) * ps_cluster_data->num_mvs);

            ps_cur_cluster->num_mvs += ps_cluster_data->num_mvs;
            ps_cur_cluster->area_in_pixels += ps_cluster_data->area_in_pixels;
            ps_cur_cluster->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
            ps_cur_cluster->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;
            i8_mv_x_sum_self += i8_mv_x_sum_cousin;
            i8_mv_y_sum_self += i8_mv_y_sum_cousin;

            ps_cur_centroid->i4_pos_x_q8 = (WORD32)(i8_mv_x_sum_self / ps_cur_cluster->num_mvs);
            ps_cur_centroid->i4_pos_y_q8 = (WORD32)(i8_mv_y_sum_self / ps_cur_cluster->num_mvs);

            minmax_x_update_id = (ps_cur_cluster->min_x < ps_cluster_data->min_x)
                                     ? ((ps_cur_cluster->max_x > ps_cluster_data->max_x) ? 0 : 2)
                                     : 1;
            minmax_y_update_id = (ps_cur_cluster->min_y < ps_cluster_data->min_y)
                                     ? ((ps_cur_cluster->max_y > ps_cluster_data->max_y) ? 0 : 2)
                                     : 1;

            /* Updation of centroid spread */
            switch(minmax_x_update_id + (minmax_y_update_id << 2))
            {
            case 1:
            {
                S32 mvd, mvd_q8;

                ps_cur_cluster->min_x = ps_cluster_data->min_x;

                mvd_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster->min_x << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (max_dist_from_centroid))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 2:
            {
                S32 mvd, mvd_q8;

                ps_cur_cluster->max_x = ps_cluster_data->max_x;

                mvd_q8 = (ps_cur_cluster->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (max_dist_from_centroid))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 4:
            {
                S32 mvd, mvd_q8;

                ps_cur_cluster->min_y = ps_cluster_data->min_y;

                mvd_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster->min_y << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (max_dist_from_centroid))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 5:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                ps_cur_cluster->min_x = ps_cluster_data->min_x;
                ps_cur_cluster->min_y = ps_cluster_data->min_y;

                if(mvd > max_dist_from_centroid)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 6:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = (ps_cur_cluster->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                ps_cur_cluster->max_x = ps_cluster_data->max_x;
                ps_cur_cluster->min_y = ps_cluster_data->min_y;

                if(mvd > max_dist_from_centroid)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 8:
            {
                S32 mvd, mvd_q8;

                ps_cur_cluster->max_y = ps_cluster_data->max_y;

                mvd_q8 = (ps_cur_cluster->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (max_dist_from_centroid))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 9:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cur_cluster->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                ps_cur_cluster->min_x = ps_cluster_data->min_x;
                ps_cur_cluster->max_y = ps_cluster_data->max_y;

                if(mvd > max_dist_from_centroid)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 10:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = (ps_cur_cluster->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cur_cluster->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                ps_cur_cluster->max_x = ps_cluster_data->max_x;
                ps_cur_cluster->max_y = ps_cluster_data->max_y;

                if(mvd > ps_cluster_data->max_dist_from_centroid)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            default:
            {
                break;
            }
            }

            hme_try_cluster_merge(ps_root, pu1_num_clusters, idx_of_updated_cluster);

            return;
        }

        num_clusters_evaluated++;
    }
}

/**
********************************************************************************
*  @fn   void hme_find_and_update_clusters
*               (
*                   cluster_data_t *ps_cluster_data,
*                   S32 *pi4_num_clusters,
*                   S32 mvx,
*                   S32 mvy,
*                   S32 ref_idx,
*                   PART_ID_T e_part_id
*               )
*
*  @brief  Implementation fo the clustering algorithm
*
*  @param[in/out]  ps_cluster_data: pointer to cluster_data_t struct
*
*  @param[in/out]  pi4_num_clusters : pointer to number of clusters
*
*  @param[in]  mvx : x co-ordinate of the motion vector
*
*  @param[in]  mvy : y co-ordinate of the motion vector
*
*  @param[in]  ref_idx : ref_id of the motion vector
*
*  @param[in]  e_part_id : partition id of the motion vector
*
*  @return None
********************************************************************************
*/
void hme_find_and_update_clusters(
    cluster_data_t *ps_cluster_data,
    U08 *pu1_num_clusters,
    S16 i2_mv_x,
    S16 i2_mv_y,
    U08 i1_ref_idx,
    S32 i4_sdi,
    PART_ID_T e_part_id,
    U08 is_part_of_bi)
{
    S32 i;
    S32 min_mvd_cluster_id = -1;
    S32 mvd, mvd_limit, mvdx, mvdy;
    S32 min_mvdx, min_mvdy;

    S32 min_mvd = MAX_32BIT_VAL;
    S32 num_clusters = *pu1_num_clusters;

    S32 mvx = i2_mv_x;
    S32 mvy = i2_mv_y;
    S32 ref_idx = i1_ref_idx;
    S32 sdi = i4_sdi;
    S32 new_cluster_idx = MAX_NUM_CLUSTERS_16x16;

    if(num_clusters == 0)
    {
        cluster_data_t *ps_data = &ps_cluster_data[num_clusters];

        ps_data->num_mvs = 1;
        ps_data->s_centroid.i4_pos_x_q8 = mvx << 8;
        ps_data->s_centroid.i4_pos_y_q8 = mvy << 8;
        ps_data->ref_id = ref_idx;
        ps_data->area_in_pixels = gai4_partition_area[e_part_id];
        ps_data->as_mv[0].pixel_count = gai4_partition_area[e_part_id];
        ps_data->as_mv[0].mvx = mvx;
        ps_data->as_mv[0].mvy = mvy;

        /***************************/
        ps_data->as_mv[0].is_uni = !is_part_of_bi;
        ps_data->as_mv[0].sdi = sdi;
        if(is_part_of_bi)
        {
            ps_data->bi_mv_pixel_area += ps_data->area_in_pixels;
        }
        else
        {
            ps_data->uni_mv_pixel_area += ps_data->area_in_pixels;
        }
        /**************************/
        ps_data->max_x = mvx;
        ps_data->min_x = mvx;
        ps_data->max_y = mvy;
        ps_data->min_y = mvy;

        ps_data->is_valid_cluster = 1;

        *pu1_num_clusters = 1;
    }
    else
    {
        S32 num_clusters_evaluated = 0;

        for(i = 0; num_clusters_evaluated < num_clusters; i++)
        {
            cluster_data_t *ps_data = &ps_cluster_data[i];

            centroid_t *ps_centroid;

            S32 mvx_q8;
            S32 mvy_q8;
            S32 posx_q8;
            S32 posy_q8;
            S32 mvdx_q8;
            S32 mvdy_q8;

            /* In anticipation of a possible merging of clusters */
            if(ps_data->is_valid_cluster == 0)
            {
                new_cluster_idx = i;
                continue;
            }

            if(ref_idx != ps_data->ref_id)
            {
                num_clusters_evaluated++;
                continue;
            }

            ps_centroid = &ps_data->s_centroid;
            posx_q8 = ps_centroid->i4_pos_x_q8;
            posy_q8 = ps_centroid->i4_pos_y_q8;

            mvx_q8 = mvx << 8;
            mvy_q8 = mvy << 8;

            mvdx_q8 = posx_q8 - mvx_q8;
            mvdy_q8 = posy_q8 - mvy_q8;

            mvdx = (((mvdx_q8 + (1 << 7)) >> 8));
            mvdy = (((mvdy_q8 + (1 << 7)) >> 8));

            mvd = ABS(mvdx) + ABS(mvdy);

            if(mvd < min_mvd)
            {
                min_mvd = mvd;
                min_mvdx = mvdx;
                min_mvdy = mvdy;
                min_mvd_cluster_id = i;
            }

            num_clusters_evaluated++;
        }

        mvd_limit = (min_mvd_cluster_id == -1)
                        ? ps_cluster_data[0].max_dist_from_centroid
                        : ps_cluster_data[min_mvd_cluster_id].max_dist_from_centroid;

        /* This condition implies that min_mvd has been updated */
        if(min_mvd <= mvd_limit)
        {
            hme_update_cluster_attributes(
                &ps_cluster_data[min_mvd_cluster_id],
                mvx,
                mvy,
                min_mvdx,
                min_mvdy,
                ref_idx,
                sdi,
                is_part_of_bi,
                e_part_id);

            if(PRT_NxN == ge_part_id_to_part_type[e_part_id])
            {
                hme_try_cluster_merge(ps_cluster_data, pu1_num_clusters, min_mvd_cluster_id);
            }
        }
        else
        {
            cluster_data_t *ps_data = (new_cluster_idx == MAX_NUM_CLUSTERS_16x16)
                                          ? &ps_cluster_data[num_clusters]
                                          : &ps_cluster_data[new_cluster_idx];

            ps_data->num_mvs = 1;
            ps_data->s_centroid.i4_pos_x_q8 = mvx << 8;
            ps_data->s_centroid.i4_pos_y_q8 = mvy << 8;
            ps_data->ref_id = ref_idx;
            ps_data->area_in_pixels = gai4_partition_area[e_part_id];
            ps_data->as_mv[0].pixel_count = gai4_partition_area[e_part_id];
            ps_data->as_mv[0].mvx = mvx;
            ps_data->as_mv[0].mvy = mvy;

            /***************************/
            ps_data->as_mv[0].is_uni = !is_part_of_bi;
            ps_data->as_mv[0].sdi = sdi;
            if(is_part_of_bi)
            {
                ps_data->bi_mv_pixel_area += ps_data->area_in_pixels;
            }
            else
            {
                ps_data->uni_mv_pixel_area += ps_data->area_in_pixels;
            }
            /**************************/
            ps_data->max_x = mvx;
            ps_data->min_x = mvx;
            ps_data->max_y = mvy;
            ps_data->min_y = mvy;

            ps_data->is_valid_cluster = 1;

            num_clusters++;
            *pu1_num_clusters = num_clusters;
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_update_32x32_cluster_attributes
*               (
*                   cluster_32x32_blk_t *ps_blk_32x32,
*                   cluster_data_t *ps_cluster_data
*               )
*
*  @brief  Updates attributes for 32x32 clusters based on the attributes of
*          the constituent 16x16 clusters
*
*  @param[out]  ps_blk_32x32: structure containing 32x32 block results
*
*  @param[in]  ps_cluster_data : structure containing 16x16 block results
*
*  @return None
********************************************************************************
*/
void hme_update_32x32_cluster_attributes(
    cluster_32x32_blk_t *ps_blk_32x32, cluster_data_t *ps_cluster_data)
{
    cluster_data_t *ps_cur_cluster_32;

    S32 i;
    S32 mvd_limit;

    S32 num_clusters = ps_blk_32x32->num_clusters;

    if(0 == num_clusters)
    {
        ps_cur_cluster_32 = &ps_blk_32x32->as_cluster_data[0];

        ps_blk_32x32->num_clusters++;
        ps_blk_32x32->au1_num_clusters[ps_cluster_data->ref_id]++;

        ps_cur_cluster_32->is_valid_cluster = 1;

        ps_cur_cluster_32->area_in_pixels = ps_cluster_data->area_in_pixels;
        ps_cur_cluster_32->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
        ps_cur_cluster_32->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;

        memcpy(
            ps_cur_cluster_32->as_mv,
            ps_cluster_data->as_mv,
            sizeof(mv_data_t) * ps_cluster_data->num_mvs);

        ps_cur_cluster_32->num_mvs = ps_cluster_data->num_mvs;

        ps_cur_cluster_32->ref_id = ps_cluster_data->ref_id;

        ps_cur_cluster_32->max_x = ps_cluster_data->max_x;
        ps_cur_cluster_32->max_y = ps_cluster_data->max_y;
        ps_cur_cluster_32->min_x = ps_cluster_data->min_x;
        ps_cur_cluster_32->min_y = ps_cluster_data->min_y;

        ps_cur_cluster_32->s_centroid = ps_cluster_data->s_centroid;
    }
    else
    {
        centroid_t *ps_centroid;

        S32 cur_posx_q8, cur_posy_q8;
        S32 min_mvd_cluster_id = -1;
        S32 mvd;
        S32 mvdx;
        S32 mvdy;
        S32 mvdx_min;
        S32 mvdy_min;
        S32 mvdx_q8;
        S32 mvdy_q8;

        S32 num_clusters_evaluated = 0;

        S32 mvd_min = MAX_32BIT_VAL;

        S32 mvx_inp_q8 = ps_cluster_data->s_centroid.i4_pos_x_q8;
        S32 mvy_inp_q8 = ps_cluster_data->s_centroid.i4_pos_y_q8;

        for(i = 0; num_clusters_evaluated < num_clusters; i++)
        {
            ps_cur_cluster_32 = &ps_blk_32x32->as_cluster_data[i];

            if(ps_cur_cluster_32->ref_id != ps_cluster_data->ref_id)
            {
                num_clusters_evaluated++;
                continue;
            }
            if(!ps_cluster_data->is_valid_cluster)
            {
                continue;
            }

            num_clusters_evaluated++;

            ps_centroid = &ps_cur_cluster_32->s_centroid;

            cur_posx_q8 = ps_centroid->i4_pos_x_q8;
            cur_posy_q8 = ps_centroid->i4_pos_y_q8;

            mvdx_q8 = cur_posx_q8 - mvx_inp_q8;
            mvdy_q8 = cur_posy_q8 - mvy_inp_q8;

            mvdx = (mvdx_q8 + (1 << 7)) >> 8;
            mvdy = (mvdy_q8 + (1 << 7)) >> 8;

            mvd = ABS(mvdx) + ABS(mvdy);

            if(mvd < mvd_min)
            {
                mvd_min = mvd;
                mvdx_min = mvdx;
                mvdy_min = mvdy;
                min_mvd_cluster_id = i;
            }
        }

        ps_cur_cluster_32 = &ps_blk_32x32->as_cluster_data[0];

        mvd_limit = (min_mvd_cluster_id == -1)
                        ? ps_cur_cluster_32[0].max_dist_from_centroid
                        : ps_cur_cluster_32[min_mvd_cluster_id].max_dist_from_centroid;

        if(mvd_min <= mvd_limit)
        {
            LWORD64 i8_updated_posx;
            LWORD64 i8_updated_posy;
            WORD32 minmax_updated_x = 0;
            WORD32 minmax_updated_y = 0;

            ps_cur_cluster_32 = &ps_blk_32x32->as_cluster_data[min_mvd_cluster_id];

            ps_centroid = &ps_cur_cluster_32->s_centroid;

            ps_cur_cluster_32->is_valid_cluster = 1;

            ps_cur_cluster_32->area_in_pixels += ps_cluster_data->area_in_pixels;
            ps_cur_cluster_32->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
            ps_cur_cluster_32->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;

            memcpy(
                &ps_cur_cluster_32->as_mv[ps_cur_cluster_32->num_mvs],
                ps_cluster_data->as_mv,
                sizeof(mv_data_t) * ps_cluster_data->num_mvs);

            if((mvdx_min > 0) && ((ps_cur_cluster_32->min_x << 8) > mvx_inp_q8))
            {
                ps_cur_cluster_32->min_x = (mvx_inp_q8 + ((1 << 7))) >> 8;
                minmax_updated_x = 1;
            }
            else if((mvdx_min < 0) && ((ps_cur_cluster_32->max_x << 8) < mvx_inp_q8))
            {
                ps_cur_cluster_32->max_x = (mvx_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_x = 2;
            }

            if((mvdy_min > 0) && ((ps_cur_cluster_32->min_y << 8) > mvy_inp_q8))
            {
                ps_cur_cluster_32->min_y = (mvy_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_y = 1;
            }
            else if((mvdy_min < 0) && ((ps_cur_cluster_32->max_y << 8) < mvy_inp_q8))
            {
                ps_cur_cluster_32->max_y = (mvy_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_y = 2;
            }

            switch((minmax_updated_y << 2) + minmax_updated_x)
            {
            case 1:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster_32->min_x << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 2:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = (ps_cur_cluster_32->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 4:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster_32->min_y << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 5:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster_32->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster_32->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 6:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster_32->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = (ps_cur_cluster_32->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 8:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = (ps_cur_cluster_32->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 9:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster_32->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cur_cluster_32->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 10:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = (ps_cur_cluster_32->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cur_cluster_32->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > ps_cur_cluster_32->max_dist_from_centroid)
                {
                    ps_cur_cluster_32->max_dist_from_centroid = mvd;
                }
                break;
            }
            default:
            {
                break;
            }
            }

            i8_updated_posx = ((LWORD64)ps_centroid->i4_pos_x_q8 * ps_cur_cluster_32->num_mvs) +
                              ((LWORD64)mvx_inp_q8 * ps_cluster_data->num_mvs);
            i8_updated_posy = ((LWORD64)ps_centroid->i4_pos_y_q8 * ps_cur_cluster_32->num_mvs) +
                              ((LWORD64)mvy_inp_q8 * ps_cluster_data->num_mvs);

            ps_cur_cluster_32->num_mvs += ps_cluster_data->num_mvs;

            ps_centroid->i4_pos_x_q8 = (WORD32)(i8_updated_posx / ps_cur_cluster_32->num_mvs);
            ps_centroid->i4_pos_y_q8 = (WORD32)(i8_updated_posy / ps_cur_cluster_32->num_mvs);
        }
        else if(num_clusters < MAX_NUM_CLUSTERS_32x32)
        {
            ps_cur_cluster_32 = &ps_blk_32x32->as_cluster_data[num_clusters];

            ps_blk_32x32->num_clusters++;
            ps_blk_32x32->au1_num_clusters[ps_cluster_data->ref_id]++;

            ps_cur_cluster_32->is_valid_cluster = 1;

            ps_cur_cluster_32->area_in_pixels = ps_cluster_data->area_in_pixels;
            ps_cur_cluster_32->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
            ps_cur_cluster_32->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;

            memcpy(
                ps_cur_cluster_32->as_mv,
                ps_cluster_data->as_mv,
                sizeof(mv_data_t) * ps_cluster_data->num_mvs);

            ps_cur_cluster_32->num_mvs = ps_cluster_data->num_mvs;

            ps_cur_cluster_32->ref_id = ps_cluster_data->ref_id;

            ps_cur_cluster_32->max_x = ps_cluster_data->max_x;
            ps_cur_cluster_32->max_y = ps_cluster_data->max_y;
            ps_cur_cluster_32->min_x = ps_cluster_data->min_x;
            ps_cur_cluster_32->min_y = ps_cluster_data->min_y;

            ps_cur_cluster_32->s_centroid = ps_cluster_data->s_centroid;
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_update_64x64_cluster_attributes
*               (
*                   cluster_64x64_blk_t *ps_blk_32x32,
*                   cluster_data_t *ps_cluster_data
*               )
*
*  @brief  Updates attributes for 64x64 clusters based on the attributes of
*          the constituent 16x16 clusters
*
*  @param[out]  ps_blk_64x64: structure containing 64x64 block results
*
*  @param[in]  ps_cluster_data : structure containing 32x32 block results
*
*  @return None
********************************************************************************
*/
void hme_update_64x64_cluster_attributes(
    cluster_64x64_blk_t *ps_blk_64x64, cluster_data_t *ps_cluster_data)
{
    cluster_data_t *ps_cur_cluster_64;

    S32 i;
    S32 mvd_limit;

    S32 num_clusters = ps_blk_64x64->num_clusters;

    if(0 == num_clusters)
    {
        ps_cur_cluster_64 = &ps_blk_64x64->as_cluster_data[0];

        ps_blk_64x64->num_clusters++;
        ps_blk_64x64->au1_num_clusters[ps_cluster_data->ref_id]++;

        ps_cur_cluster_64->is_valid_cluster = 1;

        ps_cur_cluster_64->area_in_pixels = ps_cluster_data->area_in_pixels;
        ps_cur_cluster_64->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
        ps_cur_cluster_64->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;

        memcpy(
            ps_cur_cluster_64->as_mv,
            ps_cluster_data->as_mv,
            sizeof(mv_data_t) * ps_cluster_data->num_mvs);

        ps_cur_cluster_64->num_mvs = ps_cluster_data->num_mvs;

        ps_cur_cluster_64->ref_id = ps_cluster_data->ref_id;

        ps_cur_cluster_64->max_x = ps_cluster_data->max_x;
        ps_cur_cluster_64->max_y = ps_cluster_data->max_y;
        ps_cur_cluster_64->min_x = ps_cluster_data->min_x;
        ps_cur_cluster_64->min_y = ps_cluster_data->min_y;

        ps_cur_cluster_64->s_centroid = ps_cluster_data->s_centroid;
    }
    else
    {
        centroid_t *ps_centroid;

        S32 cur_posx_q8, cur_posy_q8;
        S32 min_mvd_cluster_id = -1;
        S32 mvd;
        S32 mvdx;
        S32 mvdy;
        S32 mvdx_min;
        S32 mvdy_min;
        S32 mvdx_q8;
        S32 mvdy_q8;

        S32 num_clusters_evaluated = 0;

        S32 mvd_min = MAX_32BIT_VAL;

        S32 mvx_inp_q8 = ps_cluster_data->s_centroid.i4_pos_x_q8;
        S32 mvy_inp_q8 = ps_cluster_data->s_centroid.i4_pos_y_q8;

        for(i = 0; num_clusters_evaluated < num_clusters; i++)
        {
            ps_cur_cluster_64 = &ps_blk_64x64->as_cluster_data[i];

            if(ps_cur_cluster_64->ref_id != ps_cluster_data->ref_id)
            {
                num_clusters_evaluated++;
                continue;
            }

            if(!ps_cur_cluster_64->is_valid_cluster)
            {
                continue;
            }

            num_clusters_evaluated++;

            ps_centroid = &ps_cur_cluster_64->s_centroid;

            cur_posx_q8 = ps_centroid->i4_pos_x_q8;
            cur_posy_q8 = ps_centroid->i4_pos_y_q8;

            mvdx_q8 = cur_posx_q8 - mvx_inp_q8;
            mvdy_q8 = cur_posy_q8 - mvy_inp_q8;

            mvdx = (mvdx_q8 + (1 << 7)) >> 8;
            mvdy = (mvdy_q8 + (1 << 7)) >> 8;

            mvd = ABS(mvdx) + ABS(mvdy);

            if(mvd < mvd_min)
            {
                mvd_min = mvd;
                mvdx_min = mvdx;
                mvdy_min = mvdy;
                min_mvd_cluster_id = i;
            }
        }

        ps_cur_cluster_64 = ps_blk_64x64->as_cluster_data;

        mvd_limit = (min_mvd_cluster_id == -1)
                        ? ps_cur_cluster_64[0].max_dist_from_centroid
                        : ps_cur_cluster_64[min_mvd_cluster_id].max_dist_from_centroid;

        if(mvd_min <= mvd_limit)
        {
            LWORD64 i8_updated_posx;
            LWORD64 i8_updated_posy;
            WORD32 minmax_updated_x = 0;
            WORD32 minmax_updated_y = 0;

            ps_cur_cluster_64 = &ps_blk_64x64->as_cluster_data[min_mvd_cluster_id];

            ps_centroid = &ps_cur_cluster_64->s_centroid;

            ps_cur_cluster_64->is_valid_cluster = 1;

            ps_cur_cluster_64->area_in_pixels += ps_cluster_data->area_in_pixels;
            ps_cur_cluster_64->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
            ps_cur_cluster_64->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;

            memcpy(
                &ps_cur_cluster_64->as_mv[ps_cur_cluster_64->num_mvs],
                ps_cluster_data->as_mv,
                sizeof(mv_data_t) * ps_cluster_data->num_mvs);

            if((mvdx_min > 0) && ((ps_cur_cluster_64->min_x << 8) > mvx_inp_q8))
            {
                ps_cur_cluster_64->min_x = (mvx_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_x = 1;
            }
            else if((mvdx_min < 0) && ((ps_cur_cluster_64->max_x << 8) < mvx_inp_q8))
            {
                ps_cur_cluster_64->max_x = (mvx_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_x = 2;
            }

            if((mvdy_min > 0) && ((ps_cur_cluster_64->min_y << 8) > mvy_inp_q8))
            {
                ps_cur_cluster_64->min_y = (mvy_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_y = 1;
            }
            else if((mvdy_min < 0) && ((ps_cur_cluster_64->max_y << 8) < mvy_inp_q8))
            {
                ps_cur_cluster_64->max_y = (mvy_inp_q8 + (1 << 7)) >> 8;
                minmax_updated_y = 2;
            }

            switch((minmax_updated_y << 2) + minmax_updated_x)
            {
            case 1:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster_64->min_x << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 2:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = (ps_cur_cluster_64->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 4:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster_64->min_y << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 5:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster_64->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster_64->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 6:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_centroid->i4_pos_y_q8 - (ps_cur_cluster_64->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = (ps_cur_cluster_64->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 8:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = (ps_cur_cluster_64->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 9:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = ps_centroid->i4_pos_x_q8 - (ps_cur_cluster_64->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cur_cluster_64->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 10:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = (ps_cur_cluster_64->max_x << 8) - ps_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cur_cluster_64->max_y << 8) - ps_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > ps_cur_cluster_64->max_dist_from_centroid)
                {
                    ps_cur_cluster_64->max_dist_from_centroid = mvd;
                }
                break;
            }
            default:
            {
                break;
            }
            }

            i8_updated_posx = ((LWORD64)ps_centroid->i4_pos_x_q8 * ps_cur_cluster_64->num_mvs) +
                              ((LWORD64)mvx_inp_q8 * ps_cluster_data->num_mvs);
            i8_updated_posy = ((LWORD64)ps_centroid->i4_pos_y_q8 * ps_cur_cluster_64->num_mvs) +
                              ((LWORD64)mvy_inp_q8 * ps_cluster_data->num_mvs);

            ps_cur_cluster_64->num_mvs += ps_cluster_data->num_mvs;

            ps_centroid->i4_pos_x_q8 = (WORD32)(i8_updated_posx / ps_cur_cluster_64->num_mvs);
            ps_centroid->i4_pos_y_q8 = (WORD32)(i8_updated_posy / ps_cur_cluster_64->num_mvs);
        }
        else if(num_clusters < MAX_NUM_CLUSTERS_64x64)
        {
            ps_cur_cluster_64 = &ps_blk_64x64->as_cluster_data[num_clusters];

            ps_blk_64x64->num_clusters++;
            ps_blk_64x64->au1_num_clusters[ps_cluster_data->ref_id]++;

            ps_cur_cluster_64->is_valid_cluster = 1;

            ps_cur_cluster_64->area_in_pixels = ps_cluster_data->area_in_pixels;
            ps_cur_cluster_64->bi_mv_pixel_area += ps_cluster_data->bi_mv_pixel_area;
            ps_cur_cluster_64->uni_mv_pixel_area += ps_cluster_data->uni_mv_pixel_area;

            memcpy(
                &ps_cur_cluster_64->as_mv[0],
                ps_cluster_data->as_mv,
                sizeof(mv_data_t) * ps_cluster_data->num_mvs);

            ps_cur_cluster_64->num_mvs = ps_cluster_data->num_mvs;

            ps_cur_cluster_64->ref_id = ps_cluster_data->ref_id;

            ps_cur_cluster_64->max_x = ps_cluster_data->max_x;
            ps_cur_cluster_64->max_y = ps_cluster_data->max_y;
            ps_cur_cluster_64->min_x = ps_cluster_data->min_x;
            ps_cur_cluster_64->min_y = ps_cluster_data->min_y;

            ps_cur_cluster_64->s_centroid = ps_cluster_data->s_centroid;
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_update_32x32_clusters
*               (
*                   cluster_32x32_blk_t *ps_blk_32x32,
*                   cluster_16x16_blk_t *ps_blk_16x16
*               )
*
*  @brief  Updates attributes for 32x32 clusters based on the attributes of
*          the constituent 16x16 clusters
*
*  @param[out]  ps_blk_32x32: structure containing 32x32 block results
*
*  @param[in]  ps_blk_16x16 : structure containing 16x16 block results
*
*  @return None
********************************************************************************
*/
static __inline void
    hme_update_32x32_clusters(cluster_32x32_blk_t *ps_blk_32x32, cluster_16x16_blk_t *ps_blk_16x16)
{
    cluster_16x16_blk_t *ps_blk_16x16_cur;
    cluster_data_t *ps_cur_cluster;

    S32 i, j;
    S32 num_clusters_cur_16x16_blk;

    for(i = 0; i < 4; i++)
    {
        S32 num_clusters_evaluated = 0;

        ps_blk_16x16_cur = &ps_blk_16x16[i];

        num_clusters_cur_16x16_blk = ps_blk_16x16_cur->num_clusters;

        ps_blk_32x32->intra_mv_area += ps_blk_16x16_cur->intra_mv_area;

        ps_blk_32x32->best_inter_cost += ps_blk_16x16_cur->best_inter_cost;

        for(j = 0; num_clusters_evaluated < num_clusters_cur_16x16_blk; j++)
        {
            ps_cur_cluster = &ps_blk_16x16_cur->as_cluster_data[j];

            if(!ps_cur_cluster->is_valid_cluster)
            {
                continue;
            }

            hme_update_32x32_cluster_attributes(ps_blk_32x32, ps_cur_cluster);

            num_clusters_evaluated++;
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_update_64x64_clusters
*               (
*                   cluster_64x64_blk_t *ps_blk_64x64,
*                   cluster_32x32_blk_t *ps_blk_32x32
*               )
*
*  @brief  Updates attributes for 64x64 clusters based on the attributes of
*          the constituent 16x16 clusters
*
*  @param[out]  ps_blk_64x64: structure containing 32x32 block results
*
*  @param[in]  ps_blk_32x32 : structure containing 16x16 block results
*
*  @return None
********************************************************************************
*/
static __inline void
    hme_update_64x64_clusters(cluster_64x64_blk_t *ps_blk_64x64, cluster_32x32_blk_t *ps_blk_32x32)
{
    cluster_32x32_blk_t *ps_blk_32x32_cur;
    cluster_data_t *ps_cur_cluster;

    S32 i, j;
    S32 num_clusters_cur_32x32_blk;

    for(i = 0; i < 4; i++)
    {
        S32 num_clusters_evaluated = 0;

        ps_blk_32x32_cur = &ps_blk_32x32[i];

        num_clusters_cur_32x32_blk = ps_blk_32x32_cur->num_clusters;

        ps_blk_64x64->intra_mv_area += ps_blk_32x32_cur->intra_mv_area;
        ps_blk_64x64->best_inter_cost += ps_blk_32x32_cur->best_inter_cost;

        for(j = 0; num_clusters_evaluated < num_clusters_cur_32x32_blk; j++)
        {
            ps_cur_cluster = &ps_blk_32x32_cur->as_cluster_data[j];

            if(!ps_cur_cluster->is_valid_cluster)
            {
                continue;
            }

            hme_update_64x64_cluster_attributes(ps_blk_64x64, ps_cur_cluster);

            num_clusters_evaluated++;
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_try_merge_clusters_blksize_gt_16
*               (
*                   cluster_data_t *ps_cluster_data,
*                   S32 num_clusters
*               )
*
*  @brief  Merging clusters from blocks of size 32x32 and greater
*
*  @param[in/out]  ps_cluster_data: structure containing cluster data
*
*  @param[in/out]  pi4_num_clusters : pointer to number of clusters
*
*  @return Success or failure
********************************************************************************
*/
S32 hme_try_merge_clusters_blksize_gt_16(cluster_data_t *ps_cluster_data, S32 num_clusters)
{
    centroid_t *ps_cur_centroid;
    cluster_data_t *ps_cur_cluster;

    S32 i, mvd;
    S32 mvdx, mvdy, mvdx_q8, mvdy_q8;

    centroid_t *ps_centroid = &ps_cluster_data->s_centroid;

    S32 mvd_limit = ps_cluster_data->max_dist_from_centroid;
    S32 ref_id = ps_cluster_data->ref_id;

    S32 node0_posx_q8 = ps_centroid->i4_pos_x_q8;
    S32 node0_posy_q8 = ps_centroid->i4_pos_y_q8;
    S32 num_clusters_evaluated = 1;
    S32 ret_value = 0;

    if(1 >= num_clusters)
    {
        return ret_value;
    }

    for(i = 1; num_clusters_evaluated < num_clusters; i++)
    {
        S32 cur_posx_q8;
        S32 cur_posy_q8;

        ps_cur_cluster = &ps_cluster_data[i];

        if((ref_id != ps_cur_cluster->ref_id))
        {
            num_clusters_evaluated++;
            continue;
        }

        if((!ps_cur_cluster->is_valid_cluster))
        {
            continue;
        }

        num_clusters_evaluated++;

        ps_cur_centroid = &ps_cur_cluster->s_centroid;

        cur_posx_q8 = ps_cur_centroid->i4_pos_x_q8;
        cur_posy_q8 = ps_cur_centroid->i4_pos_y_q8;

        mvdx_q8 = cur_posx_q8 - node0_posx_q8;
        mvdy_q8 = cur_posy_q8 - node0_posy_q8;

        mvdx = (mvdx_q8 + (1 << 7)) >> 8;
        mvdy = (mvdy_q8 + (1 << 7)) >> 8;

        mvd = ABS(mvdx) + ABS(mvdy);

        if(mvd <= (mvd_limit >> 1))
        {
            LWORD64 i8_updated_posx;
            LWORD64 i8_updated_posy;
            WORD32 minmax_updated_x = 0;
            WORD32 minmax_updated_y = 0;

            ps_cur_cluster->is_valid_cluster = 0;

            ps_cluster_data->area_in_pixels += ps_cur_cluster->area_in_pixels;
            ps_cluster_data->bi_mv_pixel_area += ps_cur_cluster->bi_mv_pixel_area;
            ps_cluster_data->uni_mv_pixel_area += ps_cur_cluster->uni_mv_pixel_area;

            memcpy(
                &ps_cluster_data->as_mv[ps_cluster_data->num_mvs],
                ps_cur_cluster->as_mv,
                sizeof(mv_data_t) * ps_cur_cluster->num_mvs);

            if(mvdx > 0)
            {
                ps_cluster_data->min_x = (cur_posx_q8 + (1 << 7)) >> 8;
                minmax_updated_x = 1;
            }
            else
            {
                ps_cluster_data->max_x = (cur_posx_q8 + (1 << 7)) >> 8;
                minmax_updated_x = 2;
            }

            if(mvdy > 0)
            {
                ps_cluster_data->min_y = (cur_posy_q8 + (1 << 7)) >> 8;
                minmax_updated_y = 1;
            }
            else
            {
                ps_cluster_data->max_y = (cur_posy_q8 + (1 << 7)) >> 8;
                minmax_updated_y = 2;
            }

            switch((minmax_updated_y << 2) + minmax_updated_x)
            {
            case 1:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = ps_cur_centroid->i4_pos_x_q8 - (ps_cluster_data->min_x << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 2:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = (ps_cluster_data->max_x << 8) - ps_cur_centroid->i4_pos_x_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 4:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = ps_cur_centroid->i4_pos_y_q8 - (ps_cluster_data->min_y << 8);
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 5:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_cur_centroid->i4_pos_y_q8 - (ps_cluster_data->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = ps_cur_centroid->i4_pos_x_q8 - (ps_cluster_data->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 6:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdy_q8 = ps_cur_centroid->i4_pos_y_q8 - (ps_cluster_data->min_y << 8);
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvdx_q8 = (ps_cluster_data->max_x << 8) - ps_cur_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 8:
            {
                S32 mvd, mvd_q8;

                mvd_q8 = (ps_cluster_data->max_y << 8) - ps_cur_centroid->i4_pos_y_q8;
                mvd = (mvd_q8 + (1 << 7)) >> 8;

                if(mvd > (mvd_limit))
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 9:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = ps_cur_centroid->i4_pos_x_q8 - (ps_cluster_data->min_x << 8);
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cluster_data->max_y << 8) - ps_cur_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > mvd_limit)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            case 10:
            {
                S32 mvd;
                S32 mvdx, mvdx_q8;
                S32 mvdy, mvdy_q8;

                mvdx_q8 = (ps_cluster_data->max_x << 8) - ps_cur_centroid->i4_pos_x_q8;
                mvdx = (mvdx_q8 + (1 << 7)) >> 8;

                mvdy_q8 = (ps_cluster_data->max_y << 8) - ps_cur_centroid->i4_pos_y_q8;
                mvdy = (mvdy_q8 + (1 << 7)) >> 8;

                mvd = (mvdx > mvdy) ? mvdx : mvdy;

                if(mvd > ps_cluster_data->max_dist_from_centroid)
                {
                    ps_cluster_data->max_dist_from_centroid = mvd;
                }
                break;
            }
            default:
            {
                break;
            }
            }

            i8_updated_posx = ((LWORD64)ps_centroid->i4_pos_x_q8 * ps_cluster_data->num_mvs) +
                              ((LWORD64)cur_posx_q8 * ps_cur_cluster->num_mvs);
            i8_updated_posy = ((LWORD64)ps_centroid->i4_pos_y_q8 * ps_cluster_data->num_mvs) +
                              ((LWORD64)cur_posy_q8 * ps_cur_cluster->num_mvs);

            ps_cluster_data->num_mvs += ps_cur_cluster->num_mvs;

            ps_centroid->i4_pos_x_q8 = (WORD32)(i8_updated_posx / ps_cluster_data->num_mvs);
            ps_centroid->i4_pos_y_q8 = (WORD32)(i8_updated_posy / ps_cluster_data->num_mvs);

            if(MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK >= num_clusters)
            {
                num_clusters--;
                num_clusters_evaluated = 1;
                i = 0;
                ret_value++;
            }
            else
            {
                ret_value++;

                return ret_value;
            }
        }
    }

    if(ret_value)
    {
        for(i = 1; i < (num_clusters + ret_value); i++)
        {
            if(ps_cluster_data[i].is_valid_cluster)
            {
                break;
            }
        }
        if(i == (num_clusters + ret_value))
        {
            return ret_value;
        }
    }
    else
    {
        i = 1;
    }

    return (hme_try_merge_clusters_blksize_gt_16(&ps_cluster_data[i], num_clusters - 1)) +
           ret_value;
}

/**
********************************************************************************
*  @fn   S32 hme_determine_validity_32x32
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info
*               )
*
*  @brief  Determines whther current 32x32 block needs to be evaluated in enc_loop
*           while recursing through the CU tree or not
*
*  @param[in]  ps_cluster_data: structure containing cluster data
*
*  @return Success or failure
********************************************************************************
*/
__inline S32 hme_determine_validity_32x32(
    ctb_cluster_info_t *ps_ctb_cluster_info,
    S32 *pi4_children_nodes_required,
    S32 blk_validity_wrt_pic_bndry,
    S32 parent_blk_validity_wrt_pic_bndry)
{
    cluster_data_t *ps_data;

    cluster_32x32_blk_t *ps_32x32_blk = ps_ctb_cluster_info->ps_32x32_blk;
    cluster_64x64_blk_t *ps_64x64_blk = ps_ctb_cluster_info->ps_64x64_blk;

    S32 num_clusters = ps_32x32_blk->num_clusters;
    S32 num_clusters_parent = ps_64x64_blk->num_clusters;

    if(!blk_validity_wrt_pic_bndry)
    {
        *pi4_children_nodes_required = 1;
        return 0;
    }

    if(!parent_blk_validity_wrt_pic_bndry)
    {
        *pi4_children_nodes_required = 1;
        return 1;
    }

    if(num_clusters > MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK)
    {
        *pi4_children_nodes_required = 1;
        return 0;
    }

    if(num_clusters_parent > MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK)
    {
        *pi4_children_nodes_required = 1;

        return 1;
    }
    else if(num_clusters_parent < MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK)
    {
        *pi4_children_nodes_required = 0;

        return 1;
    }
    else
    {
        if(num_clusters < MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK)
        {
            *pi4_children_nodes_required = 0;
            return 1;
        }
        else
        {
            S32 i;

            S32 area_of_parent = gai4_partition_area[PART_ID_2Nx2N] << 4;
            S32 min_area = MAX_32BIT_VAL;
            S32 num_clusters_evaluated = 0;

            for(i = 0; num_clusters_evaluated < num_clusters; i++)
            {
                ps_data = &ps_32x32_blk->as_cluster_data[i];

                if(!ps_data->is_valid_cluster)
                {
                    continue;
                }

                num_clusters_evaluated++;

                if(ps_data->area_in_pixels < min_area)
                {
                    min_area = ps_data->area_in_pixels;
                }
            }

            if((min_area << 4) < area_of_parent)
            {
                *pi4_children_nodes_required = 1;
                return 0;
            }
            else
            {
                *pi4_children_nodes_required = 0;
                return 1;
            }
        }
    }
}

/**
********************************************************************************
*  @fn   S32 hme_determine_validity_16x16
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info
*               )
*
*  @brief  Determines whther current 16x16 block needs to be evaluated in enc_loop
*           while recursing through the CU tree or not
*
*  @param[in]  ps_cluster_data: structure containing cluster data
*
*  @return Success or failure
********************************************************************************
*/
__inline S32 hme_determine_validity_16x16(
    ctb_cluster_info_t *ps_ctb_cluster_info,
    S32 *pi4_children_nodes_required,
    S32 blk_validity_wrt_pic_bndry,
    S32 parent_blk_validity_wrt_pic_bndry)
{
    cluster_data_t *ps_data;

    cluster_16x16_blk_t *ps_16x16_blk = ps_ctb_cluster_info->ps_16x16_blk;
    cluster_32x32_blk_t *ps_32x32_blk = ps_ctb_cluster_info->ps_32x32_blk;
    cluster_64x64_blk_t *ps_64x64_blk = ps_ctb_cluster_info->ps_64x64_blk;

    S32 num_clusters = ps_16x16_blk->num_clusters;
    S32 num_clusters_parent = ps_32x32_blk->num_clusters;
    S32 num_clusters_grandparent = ps_64x64_blk->num_clusters;

    if(!blk_validity_wrt_pic_bndry)
    {
        *pi4_children_nodes_required = 1;
        return 0;
    }

    if(!parent_blk_validity_wrt_pic_bndry)
    {
        *pi4_children_nodes_required = 1;
        return 1;
    }

    if((num_clusters_parent > MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK) &&
       (num_clusters_grandparent > MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK))
    {
        *pi4_children_nodes_required = 1;
        return 1;
    }

    /* Implies nc_64 <= 3 when num_clusters_parent > 3 & */
    /* implies nc_64 > 3 when num_clusters_parent < 3 & */
    if(num_clusters_parent != MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK)
    {
        if(num_clusters <= MAX_NUM_CLUSTERS_IN_VALID_16x16_BLK)
        {
            *pi4_children_nodes_required = 0;

            return 1;
        }
        else
        {
            *pi4_children_nodes_required = 1;

            return 0;
        }
    }
    /* Implies nc_64 >= 3 */
    else
    {
        if(num_clusters < MAX_NUM_CLUSTERS_IN_VALID_16x16_BLK)
        {
            *pi4_children_nodes_required = 0;
            return 1;
        }
        else if(num_clusters > MAX_NUM_CLUSTERS_IN_VALID_16x16_BLK)
        {
            *pi4_children_nodes_required = 1;
            return 0;
        }
        else
        {
            S32 i;

            S32 area_of_parent = gai4_partition_area[PART_ID_2Nx2N] << 2;
            S32 min_area = MAX_32BIT_VAL;
            S32 num_clusters_evaluated = 0;

            for(i = 0; num_clusters_evaluated < num_clusters; i++)
            {
                ps_data = &ps_16x16_blk->as_cluster_data[i];

                if(!ps_data->is_valid_cluster)
                {
                    continue;
                }

                num_clusters_evaluated++;

                if(ps_data->area_in_pixels < min_area)
                {
                    min_area = ps_data->area_in_pixels;
                }
            }

            if((min_area << 4) < area_of_parent)
            {
                *pi4_children_nodes_required = 1;
                return 0;
            }
            else
            {
                *pi4_children_nodes_required = 0;
                return 1;
            }
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_build_cu_tree
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info,
*                   cur_ctb_cu_tree_t *ps_cu_tree,
*                   S32 tree_depth,
*                   CU_POS_T e_grand_parent_blk_pos,
*                   CU_POS_T e_parent_blk_pos,
*                   CU_POS_T e_cur_blk_pos
*               )
*
*  @brief  Recursive function for CU tree initialisation
*
*  @param[in]  ps_ctb_cluster_info: structure containing pointers to clusters
*                                   corresponding to all block sizes from 64x64
*                                   to 16x16
*
*  @param[in]  e_parent_blk_pos: position of parent block wrt its parent, if
*                                applicable
*
*  @param[in]  e_cur_blk_pos: position of current block wrt parent
*
*  @param[out]  ps_cu_tree : represents CU tree used in CU recursion
*
*  @param[in]  tree_depth : specifies depth of the CU tree
*
*  @return Nothing
********************************************************************************
*/
void hme_build_cu_tree(
    ctb_cluster_info_t *ps_ctb_cluster_info,
    cur_ctb_cu_tree_t *ps_cu_tree,
    S32 tree_depth,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos)
{
    ihevce_cu_tree_init(
        ps_cu_tree,
        ps_ctb_cluster_info->ps_cu_tree_root,
        &ps_ctb_cluster_info->nodes_created_in_cu_tree,
        tree_depth,
        e_grandparent_blk_pos,
        e_parent_blk_pos,
        e_cur_blk_pos);
}

/**
********************************************************************************
*  @fn   S32 hme_sdi_based_cluster_spread_eligibility
*               (
*                   cluster_32x32_blk_t *ps_blk_32x32
*               )
*
*  @brief  Determines whether the spread of high SDI MV's around each cluster
*          center is below a pre-determined threshold
*
*  @param[in]  ps_blk_32x32: structure containing pointers to clusters
*                                   corresponding to all block sizes from 64x64
*                                   to 16x16
*
*  @return 1 if the spread is constrained, else 0
********************************************************************************
*/
__inline S32
    hme_sdi_based_cluster_spread_eligibility(cluster_32x32_blk_t *ps_blk_32x32, S32 sdi_threshold)
{
    S32 cumulative_mv_distance;
    S32 i, j;
    S32 num_high_sdi_mvs;

    S32 num_clusters = ps_blk_32x32->num_clusters;

    for(i = 0; i < num_clusters; i++)
    {
        cluster_data_t *ps_data = &ps_blk_32x32->as_cluster_data[i];

        num_high_sdi_mvs = 0;
        cumulative_mv_distance = 0;

        for(j = 0; j < ps_data->num_mvs; j++)
        {
            mv_data_t *ps_mv = &ps_data->as_mv[j];

            if(ps_mv->sdi >= sdi_threshold)
            {
                num_high_sdi_mvs++;

                COMPUTE_MVD(ps_mv, ps_data, cumulative_mv_distance);
            }
        }

        if(cumulative_mv_distance > ((ps_data->max_dist_from_centroid >> 1) * num_high_sdi_mvs))
        {
            return 0;
        }
    }

    return 1;
}

/**
********************************************************************************
*  @fn   S32 hme_populate_cu_tree
*               (
*                   ctb_cluster_info_t *ps_ctb_cluster_info,
*                   ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
*                   cur_ctb_cu_tree_t *ps_cu_tree,
*                   S32 tree_depth,
*                   CU_POS_T e_parent_blk_pos,
*                   CU_POS_T e_cur_blk_pos
*               )
*
*  @brief  Recursive function for CU tree population based on output of
*          clustering algorithm
*
*  @param[in]  ps_ctb_cluster_info: structure containing pointers to clusters
*                                   corresponding to all block sizes from 64x64
*                                   to 16x16
*
*  @param[in]  e_parent_blk_pos: position of parent block wrt its parent, if
applicable
*
*  @param[in]  e_cur_blk_pos: position of current block wrt parent
*
*  @param[in]  ps_cur_ipe_ctb : output container for ipe analyses
*
*  @param[out]  ps_cu_tree : represents CU tree used in CU recursion
*
*  @param[in]  tree_depth : specifies depth of the CU tree
*
*  @param[in]  ipe_decision_precedence : specifies whether precedence should
*               be given to decisions made either by IPE(1) or clustering algos.
*
*  @return 1 if re-evaluation of parent node's validity is not required,
else 0
********************************************************************************
*/
void hme_populate_cu_tree(
    ctb_cluster_info_t *ps_ctb_cluster_info,
    cur_ctb_cu_tree_t *ps_cu_tree,
    S32 tree_depth,
    ME_QUALITY_PRESETS_T e_quality_preset,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos)
{
    S32 area_of_cur_blk;
    S32 area_limit_for_me_decision_precedence;
    S32 children_nodes_required;
    S32 intra_mv_area;
    S32 intra_eval_enable;
    S32 inter_eval_enable;
    S32 ipe_decision_precedence;
    S32 node_validity;
    S32 num_clusters;

    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb = ps_ctb_cluster_info->ps_cur_ipe_ctb;

    if(NULL == ps_cu_tree)
    {
        return;
    }

    switch(tree_depth)
    {
    case 0:
    {
        /* 64x64 block */
        S32 blk_32x32_mask = ps_ctb_cluster_info->blk_32x32_mask;

        cluster_64x64_blk_t *ps_blk_64x64 = ps_ctb_cluster_info->ps_64x64_blk;

        area_of_cur_blk = gai4_partition_area[PART_ID_2Nx2N] << 4;
        area_limit_for_me_decision_precedence = (area_of_cur_blk * MAX_INTRA_PERCENTAGE) / 100;
        children_nodes_required = 0;
        intra_mv_area = ps_blk_64x64->intra_mv_area;

        ipe_decision_precedence = (intra_mv_area >= area_limit_for_me_decision_precedence);

        intra_eval_enable = ipe_decision_precedence;
        inter_eval_enable = !!ps_blk_64x64->num_clusters;

#if 1  //!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
        if(e_quality_preset >= ME_HIGH_QUALITY)
        {
            inter_eval_enable = 1;
            node_validity = (blk_32x32_mask == 0xf);
#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
            ps_cu_tree->u1_inter_eval_enable = !(intra_mv_area == area_of_cur_blk);
#endif
            break;
        }
#endif

#if ENABLE_4CTB_EVALUATION
        node_validity = (blk_32x32_mask == 0xf);

        break;
#else
        {
            S32 i;

            num_clusters = ps_blk_64x64->num_clusters;

            node_validity = (ipe_decision_precedence)
                                ? (!ps_cur_ipe_ctb->u1_split_flag)
                                : (num_clusters <= MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK);

            for(i = 0; i < MAX_NUM_REF; i++)
            {
                node_validity = node_validity && (ps_blk_64x64->au1_num_clusters[i] <=
                                                  MAX_NUM_CLUSTERS_IN_ONE_REF_IDX);
            }

            node_validity = node_validity && (blk_32x32_mask == 0xf);
        }
        break;
#endif
    }
    case 1:
    {
        /* 32x32 block */
        S32 is_percent_intra_area_gt_threshold;

        cluster_32x32_blk_t *ps_blk_32x32 = &ps_ctb_cluster_info->ps_32x32_blk[e_cur_blk_pos];

        S32 blk_32x32_mask = ps_ctb_cluster_info->blk_32x32_mask;

#if !ENABLE_4CTB_EVALUATION
        S32 best_inter_cost = ps_blk_32x32->best_inter_cost;
        S32 best_intra_cost =
            ((ps_ctb_cluster_info->ps_cur_ipe_ctb->ai4_best32x32_intra_cost[e_cur_blk_pos] +
              ps_ctb_cluster_info->i4_frame_qstep * ps_ctb_cluster_info->i4_frame_qstep_multiplier *
                  4) < 0)
                ? MAX_32BIT_VAL
                : (ps_ctb_cluster_info->ps_cur_ipe_ctb->ai4_best32x32_intra_cost[e_cur_blk_pos] +
                   ps_ctb_cluster_info->i4_frame_qstep *
                       ps_ctb_cluster_info->i4_frame_qstep_multiplier * 4);
        S32 best_cost = (best_inter_cost > best_intra_cost) ? best_intra_cost : best_inter_cost;
        S32 cost_differential = (best_inter_cost - best_cost);
#endif

        area_of_cur_blk = gai4_partition_area[PART_ID_2Nx2N] << 2;
        area_limit_for_me_decision_precedence = (area_of_cur_blk * MAX_INTRA_PERCENTAGE) / 100;
        intra_mv_area = ps_blk_32x32->intra_mv_area;
        is_percent_intra_area_gt_threshold =
            (intra_mv_area > area_limit_for_me_decision_precedence);
        ipe_decision_precedence = (intra_mv_area >= area_limit_for_me_decision_precedence);

        intra_eval_enable = ipe_decision_precedence;
        inter_eval_enable = !!ps_blk_32x32->num_clusters;
        children_nodes_required = 1;

#if 1  //!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
        if(e_quality_preset >= ME_HIGH_QUALITY)
        {
            inter_eval_enable = 1;
            node_validity = (((blk_32x32_mask) & (1 << e_cur_blk_pos)) || 0);
#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
            ps_cu_tree->u1_inter_eval_enable = !(intra_mv_area == area_of_cur_blk);
#endif
            break;
        }
#endif

#if ENABLE_4CTB_EVALUATION
        node_validity = (((blk_32x32_mask) & (1 << e_cur_blk_pos)) || 0);

        break;
#else
        {
            S32 i;
            num_clusters = ps_blk_32x32->num_clusters;

            if(ipe_decision_precedence)
            {
                node_validity = (ps_cur_ipe_ctb->as_intra32_analyse[e_cur_blk_pos].b1_merge_flag);
                node_validity = node_validity && (((blk_32x32_mask) & (1 << e_cur_blk_pos)) || 0);
            }
            else
            {
                node_validity =
                    ((ALL_INTER_COST_DIFF_THR * best_cost) >= (100 * cost_differential)) &&
                    (num_clusters <= MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK) &&
                    (((blk_32x32_mask) & (1 << e_cur_blk_pos)) || 0);

                for(i = 0; (i < MAX_NUM_REF) && (node_validity); i++)
                {
                    node_validity = node_validity && (ps_blk_32x32->au1_num_clusters[i] <=
                                                      MAX_NUM_CLUSTERS_IN_ONE_REF_IDX);
                }

                if(node_validity)
                {
                    node_validity = node_validity &&
                                    hme_sdi_based_cluster_spread_eligibility(
                                        ps_blk_32x32, ps_ctb_cluster_info->sdi_threshold);
                }
            }
        }

        break;
#endif
    }
    case 2:
    {
        cluster_16x16_blk_t *ps_blk_16x16 =
            &ps_ctb_cluster_info->ps_16x16_blk[e_cur_blk_pos + (e_parent_blk_pos << 2)];

        S32 blk_8x8_mask =
            ps_ctb_cluster_info->pi4_blk_8x8_mask[(S32)(e_parent_blk_pos << 2) + e_cur_blk_pos];

        area_of_cur_blk = gai4_partition_area[PART_ID_2Nx2N];
        area_limit_for_me_decision_precedence = (area_of_cur_blk * MAX_INTRA_PERCENTAGE) / 100;
        children_nodes_required = 1;
        intra_mv_area = ps_blk_16x16->intra_mv_area;
        ipe_decision_precedence = (intra_mv_area >= area_limit_for_me_decision_precedence);
        num_clusters = ps_blk_16x16->num_clusters;

        intra_eval_enable = ipe_decision_precedence;
        inter_eval_enable = 1;

#if 1  //!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
        if(e_quality_preset >= ME_HIGH_QUALITY)
        {
            node_validity =
                !ps_ctb_cluster_info
                     ->au1_is_16x16_blk_split[(S32)(e_parent_blk_pos << 2) + e_cur_blk_pos];
            children_nodes_required = !node_validity;
            break;
        }
#endif

#if ENABLE_4CTB_EVALUATION
        node_validity = (blk_8x8_mask == 0xf);

#if ENABLE_CU_TREE_CULLING
        {
            cur_ctb_cu_tree_t *ps_32x32_root;

            switch(e_parent_blk_pos)
            {
            case POS_TL:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tl;

                break;
            }
            case POS_TR:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tr;

                break;
            }
            case POS_BL:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_bl;

                break;
            }
            case POS_BR:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_br;

                break;
            }
            }

            if(ps_32x32_root->is_node_valid)
            {
                node_validity =
                    node_validity &&
                    !ps_ctb_cluster_info
                         ->au1_is_16x16_blk_split[(S32)(e_parent_blk_pos << 2) + e_cur_blk_pos];
                children_nodes_required = !node_validity;
            }
        }
#endif

        break;
#else

        if(ipe_decision_precedence)
        {
            S32 merge_flag_16 = (ps_cur_ipe_ctb->as_intra32_analyse[e_parent_blk_pos]
                                     .as_intra16_analyse[e_cur_blk_pos]
                                     .b1_merge_flag);
            S32 valid_flag = (blk_8x8_mask == 0xf);

            node_validity = merge_flag_16 && valid_flag;
        }
        else
        {
            node_validity = (blk_8x8_mask == 0xf);
        }

        break;
#endif
    }
    case 3:
    {
        S32 blk_8x8_mask =
            ps_ctb_cluster_info
                ->pi4_blk_8x8_mask[(S32)(e_grandparent_blk_pos << 2) + e_parent_blk_pos];
        S32 merge_flag_16 = (ps_cur_ipe_ctb->as_intra32_analyse[e_grandparent_blk_pos]
                                 .as_intra16_analyse[e_parent_blk_pos]
                                 .b1_merge_flag);
        S32 merge_flag_32 =
            (ps_cur_ipe_ctb->as_intra32_analyse[e_grandparent_blk_pos].b1_merge_flag);

        intra_eval_enable = !merge_flag_16 || !merge_flag_32;
        inter_eval_enable = 1;
        children_nodes_required = 0;

#if 1  //!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
        if(e_quality_preset >= ME_HIGH_QUALITY)
        {
            node_validity = ((blk_8x8_mask & (1 << e_cur_blk_pos)) || 0);
            break;
        }
#endif

#if ENABLE_4CTB_EVALUATION
        node_validity = ((blk_8x8_mask & (1 << e_cur_blk_pos)) || 0);

        break;
#else
        {
            cur_ctb_cu_tree_t *ps_32x32_root;
            cur_ctb_cu_tree_t *ps_16x16_root;
            cluster_32x32_blk_t *ps_32x32_blk;

            switch(e_grandparent_blk_pos)
            {
            case POS_TL:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tl;

                break;
            }
            case POS_TR:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tr;

                break;
            }
            case POS_BL:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_bl;

                break;
            }
            case POS_BR:
            {
                ps_32x32_root = ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_br;

                break;
            }
            }

            switch(e_parent_blk_pos)
            {
            case POS_TL:
            {
                ps_16x16_root = ps_32x32_root->ps_child_node_tl;

                break;
            }
            case POS_TR:
            {
                ps_16x16_root = ps_32x32_root->ps_child_node_tr;

                break;
            }
            case POS_BL:
            {
                ps_16x16_root = ps_32x32_root->ps_child_node_bl;

                break;
            }
            case POS_BR:
            {
                ps_16x16_root = ps_32x32_root->ps_child_node_br;

                break;
            }
            }

            ps_32x32_blk = &ps_ctb_cluster_info->ps_32x32_blk[e_grandparent_blk_pos];

            node_validity = ((blk_8x8_mask & (1 << e_cur_blk_pos)) || 0) &&
                            ((!ps_32x32_root->is_node_valid) ||
                             (ps_32x32_blk->num_clusters_with_weak_sdi_density > 0) ||
                             (!ps_16x16_root->is_node_valid));

            break;
        }
#endif
    }
    }

    /* Fill the current cu_tree node */
    ps_cu_tree->is_node_valid = node_validity;
    ps_cu_tree->u1_intra_eval_enable = intra_eval_enable;
    ps_cu_tree->u1_inter_eval_enable = inter_eval_enable;

    if(children_nodes_required)
    {
        tree_depth++;

        hme_populate_cu_tree(
            ps_ctb_cluster_info,
            ps_cu_tree->ps_child_node_tl,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TL);

        hme_populate_cu_tree(
            ps_ctb_cluster_info,
            ps_cu_tree->ps_child_node_tr,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TR);

        hme_populate_cu_tree(
            ps_ctb_cluster_info,
            ps_cu_tree->ps_child_node_bl,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BL);

        hme_populate_cu_tree(
            ps_ctb_cluster_info,
            ps_cu_tree->ps_child_node_br,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BR);
    }
}

/**
********************************************************************************
*  @fn   void hme_analyse_mv_clustering
*               (
*                   search_results_t *ps_search_results,
*                   ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
*                   cur_ctb_cu_tree_t *ps_cu_tree
*               )
*
*  @brief  Implementation for the clustering algorithm
*
*  @param[in]  ps_search_results: structure containing 16x16 block results
*
*  @param[in]  ps_cur_ipe_ctb : output container for ipe analyses
*
*  @param[out]  ps_cu_tree : represents CU tree used in CU recursion
*
*  @return None
********************************************************************************
*/
void hme_analyse_mv_clustering(
    search_results_t *ps_search_results,
    inter_cu_results_t *ps_16x16_cu_results,
    inter_cu_results_t *ps_8x8_cu_results,
    ctb_cluster_info_t *ps_ctb_cluster_info,
    S08 *pi1_future_list,
    S08 *pi1_past_list,
    S32 bidir_enabled,
    ME_QUALITY_PRESETS_T e_quality_preset)
{
    cluster_16x16_blk_t *ps_blk_16x16;
    cluster_32x32_blk_t *ps_blk_32x32;
    cluster_64x64_blk_t *ps_blk_64x64;

    part_type_results_t *ps_best_result;
    pu_result_t *aps_part_result[MAX_NUM_PARTS];
    pu_result_t *aps_inferior_parts[MAX_NUM_PARTS];

    PART_ID_T e_part_id;
    PART_TYPE_T e_part_type;

    S32 enable_64x64_merge;
    S32 i, j, k;
    S32 mvx, mvy;
    S32 num_parts;
    S32 ref_idx;
    S32 ai4_pred_mode[MAX_NUM_PARTS];

    S32 num_32x32_merges = 0;

    /*****************************************/
    /*****************************************/
    /********* Enter ye who is HQ ************/
    /*****************************************/
    /*****************************************/

    ps_blk_64x64 = ps_ctb_cluster_info->ps_64x64_blk;

    /* Initialise data in each of the clusters */
    for(i = 0; i < 16; i++)
    {
        ps_blk_16x16 = &ps_ctb_cluster_info->ps_16x16_blk[i];

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
        if(e_quality_preset < ME_HIGH_QUALITY)
        {
            hme_init_clusters_16x16(ps_blk_16x16, bidir_enabled);
        }
        else
        {
            ps_blk_16x16->best_inter_cost = 0;
            ps_blk_16x16->intra_mv_area = 0;
        }
#else
        hme_init_clusters_16x16(ps_blk_16x16, bidir_enabled);
#endif
    }

    for(i = 0; i < 4; i++)
    {
        ps_blk_32x32 = &ps_ctb_cluster_info->ps_32x32_blk[i];

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
        if(e_quality_preset < ME_HIGH_QUALITY)
        {
            hme_init_clusters_32x32(ps_blk_32x32, bidir_enabled);
        }
        else
        {
            ps_blk_32x32->best_inter_cost = 0;
            ps_blk_32x32->intra_mv_area = 0;
        }
#else
        hme_init_clusters_32x32(ps_blk_32x32, bidir_enabled);
#endif
    }

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
    if(e_quality_preset < ME_HIGH_QUALITY)
    {
        hme_init_clusters_64x64(ps_blk_64x64, bidir_enabled);
    }
    else
    {
        ps_blk_64x64->best_inter_cost = 0;
        ps_blk_64x64->intra_mv_area = 0;
    }
#else
    hme_init_clusters_64x64(ps_blk_64x64, bidir_enabled);
#endif

    /* Initialise data for all nodes in the CU tree */
    hme_build_cu_tree(
        ps_ctb_cluster_info, ps_ctb_cluster_info->ps_cu_tree_root, 0, POS_NA, POS_NA, POS_NA);

    if(e_quality_preset >= ME_HIGH_QUALITY)
    {
        memset(ps_ctb_cluster_info->au1_is_16x16_blk_split, 1, 16 * sizeof(U08));
    }

#if ENABLE_UNIFORM_CU_SIZE_16x16 || ENABLE_UNIFORM_CU_SIZE_8x8
    return;
#endif

    for(i = 0; i < 16; i++)
    {
        S32 blk_8x8_mask;
        S32 is_16x16_blk_valid;
        S32 num_clusters_updated;
        S32 num_clusters;

        blk_8x8_mask = ps_ctb_cluster_info->pi4_blk_8x8_mask[i];

        ps_blk_16x16 = &ps_ctb_cluster_info->ps_16x16_blk[i];

        is_16x16_blk_valid = (blk_8x8_mask == 0xf);

        if(is_16x16_blk_valid)
        {
            /* Use 8x8 data when 16x16 CU is split */
            if(ps_search_results[i].u1_split_flag)
            {
                S32 blk_8x8_idx = i << 2;

                num_parts = 4;
                e_part_type = PRT_NxN;

                for(j = 0; j < num_parts; j++, blk_8x8_idx++)
                {
                    /* Only 2Nx2N partition supported for 8x8 block */
                    ASSERT(
                        ps_8x8_cu_results[blk_8x8_idx].ps_best_results[0].u1_part_type ==
                        ((PART_TYPE_T)PRT_2Nx2N));

                    aps_part_result[j] =
                        &ps_8x8_cu_results[blk_8x8_idx].ps_best_results[0].as_pu_results[0];
                    aps_inferior_parts[j] =
                        &ps_8x8_cu_results[blk_8x8_idx].ps_best_results[1].as_pu_results[0];
                    ai4_pred_mode[j] = (aps_part_result[j]->pu.b2_pred_mode);
                }
            }
            else
            {
                ps_best_result = &ps_16x16_cu_results[i].ps_best_results[0];

                e_part_type = (PART_TYPE_T)ps_best_result->u1_part_type;
                num_parts = gau1_num_parts_in_part_type[e_part_type];

                for(j = 0; j < num_parts; j++)
                {
                    aps_part_result[j] = &ps_best_result->as_pu_results[j];
                    aps_inferior_parts[j] = &ps_best_result[1].as_pu_results[j];
                    ai4_pred_mode[j] = (aps_part_result[j]->pu.b2_pred_mode);
                }

                ps_ctb_cluster_info->au1_is_16x16_blk_split[i] = 0;
            }

            for(j = 0; j < num_parts; j++)
            {
                pu_result_t *ps_part_result = aps_part_result[j];

                S32 num_mvs = ((ai4_pred_mode[j] > 1) + 1);

                e_part_id = ge_part_type_to_part_id[e_part_type][j];

                /* Skip clustering if best mode is intra */
                if((ps_part_result->pu.b1_intra_flag))
                {
                    ps_blk_16x16->intra_mv_area += gai4_partition_area[e_part_id];
                    ps_blk_16x16->best_inter_cost += aps_inferior_parts[j]->i4_tot_cost;
                    continue;
                }
                else
                {
                    ps_blk_16x16->best_inter_cost += ps_part_result->i4_tot_cost;
                }

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
                if(e_quality_preset >= ME_HIGH_QUALITY)
                {
                    continue;
                }
#endif

                for(k = 0; k < num_mvs; k++)
                {
                    mv_t *ps_mv;

                    pu_mv_t *ps_pu_mv = &ps_part_result->pu.mv;

                    S32 is_l0_mv = ((ai4_pred_mode[j] == 2) && !k) || (ai4_pred_mode[j] == 0);

                    ps_mv = (is_l0_mv) ? (&ps_pu_mv->s_l0_mv) : (&ps_pu_mv->s_l1_mv);

                    mvx = ps_mv->i2_mvx;
                    mvy = ps_mv->i2_mvy;

                    ref_idx = (is_l0_mv) ? pi1_past_list[ps_pu_mv->i1_l0_ref_idx]
                                         : pi1_future_list[ps_pu_mv->i1_l1_ref_idx];

                    num_clusters = ps_blk_16x16->num_clusters;

                    hme_find_and_update_clusters(
                        ps_blk_16x16->as_cluster_data,
                        &(ps_blk_16x16->num_clusters),
                        mvx,
                        mvy,
                        ref_idx,
                        ps_part_result->i4_sdi,
                        e_part_id,
                        (ai4_pred_mode[j] == 2));

                    num_clusters_updated = (ps_blk_16x16->num_clusters);

                    ps_blk_16x16->au1_num_clusters[ref_idx] +=
                        (num_clusters_updated - num_clusters);
                }
            }
        }
    }

    /* Search for 32x32 clusters */
    for(i = 0; i < 4; i++)
    {
        S32 num_clusters_merged;

        S32 is_32x32_blk_valid = (ps_ctb_cluster_info->blk_32x32_mask & (1 << i)) || 0;

        if(is_32x32_blk_valid)
        {
            ps_blk_32x32 = &ps_ctb_cluster_info->ps_32x32_blk[i];
            ps_blk_16x16 = &ps_ctb_cluster_info->ps_16x16_blk[i << 2];

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
            if(e_quality_preset >= ME_HIGH_QUALITY)
            {
                for(j = 0; j < 4; j++, ps_blk_16x16++)
                {
                    ps_blk_32x32->intra_mv_area += ps_blk_16x16->intra_mv_area;

                    ps_blk_32x32->best_inter_cost += ps_blk_16x16->best_inter_cost;
                }
                continue;
            }
#endif

            hme_update_32x32_clusters(ps_blk_32x32, ps_blk_16x16);

            if((ps_blk_32x32->num_clusters >= MAX_NUM_CLUSTERS_IN_VALID_32x32_BLK))
            {
                num_clusters_merged = hme_try_merge_clusters_blksize_gt_16(
                    ps_blk_32x32->as_cluster_data, (ps_blk_32x32->num_clusters));

                if(num_clusters_merged)
                {
                    ps_blk_32x32->num_clusters -= num_clusters_merged;

                    UPDATE_CLUSTER_METADATA_POST_MERGE(ps_blk_32x32);
                }
            }
        }
    }

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
    /* Eliminate outlier 32x32 clusters */
    if(e_quality_preset < ME_HIGH_QUALITY)
#endif
    {
        hme_boot_out_outlier(ps_ctb_cluster_info, 32);

        /* Find best_uni_ref and best_alt_ref */
        hme_find_top_ref_ids(ps_ctb_cluster_info, bidir_enabled, 32);
    }

    /* Populate the CU tree for depths 1 and higher */
    {
        cur_ctb_cu_tree_t *ps_tree_root = ps_ctb_cluster_info->ps_cu_tree_root;
        cur_ctb_cu_tree_t *ps_tl = ps_tree_root->ps_child_node_tl;
        cur_ctb_cu_tree_t *ps_tr = ps_tree_root->ps_child_node_tr;
        cur_ctb_cu_tree_t *ps_bl = ps_tree_root->ps_child_node_bl;
        cur_ctb_cu_tree_t *ps_br = ps_tree_root->ps_child_node_br;

        hme_populate_cu_tree(
            ps_ctb_cluster_info, ps_tl, 1, e_quality_preset, POS_NA, POS_NA, POS_TL);

        num_32x32_merges += (ps_tl->is_node_valid == 1);

        hme_populate_cu_tree(
            ps_ctb_cluster_info, ps_tr, 1, e_quality_preset, POS_NA, POS_NA, POS_TR);

        num_32x32_merges += (ps_tr->is_node_valid == 1);

        hme_populate_cu_tree(
            ps_ctb_cluster_info, ps_bl, 1, e_quality_preset, POS_NA, POS_NA, POS_BL);

        num_32x32_merges += (ps_bl->is_node_valid == 1);

        hme_populate_cu_tree(
            ps_ctb_cluster_info, ps_br, 1, e_quality_preset, POS_NA, POS_NA, POS_BR);

        num_32x32_merges += (ps_br->is_node_valid == 1);
    }

#if !ENABLE_4CTB_EVALUATION
    if(e_quality_preset < ME_HIGH_QUALITY)
    {
        enable_64x64_merge = (num_32x32_merges >= 3);
    }
#else
    if(e_quality_preset < ME_HIGH_QUALITY)
    {
        enable_64x64_merge = 1;
    }
#endif

#if 1  //!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    if(e_quality_preset >= ME_HIGH_QUALITY)
    {
        enable_64x64_merge = 1;
    }
#else
    if(e_quality_preset >= ME_HIGH_QUALITY)
    {
        enable_64x64_merge = (num_32x32_merges >= 3);
    }
#endif

    if(enable_64x64_merge)
    {
        S32 num_clusters_merged;

        ps_blk_32x32 = &ps_ctb_cluster_info->ps_32x32_blk[0];

#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
        if(e_quality_preset >= ME_HIGH_QUALITY)
        {
            for(j = 0; j < 4; j++, ps_blk_32x32++)
            {
                ps_blk_64x64->intra_mv_area += ps_blk_32x32->intra_mv_area;

                ps_blk_64x64->best_inter_cost += ps_blk_32x32->best_inter_cost;
            }
        }
        else
#endif
        {
            hme_update_64x64_clusters(ps_blk_64x64, ps_blk_32x32);

            if((ps_blk_64x64->num_clusters >= MAX_NUM_CLUSTERS_IN_VALID_64x64_BLK))
            {
                num_clusters_merged = hme_try_merge_clusters_blksize_gt_16(
                    ps_blk_64x64->as_cluster_data, (ps_blk_64x64->num_clusters));

                if(num_clusters_merged)
                {
                    ps_blk_64x64->num_clusters -= num_clusters_merged;

                    UPDATE_CLUSTER_METADATA_POST_MERGE(ps_blk_64x64);
                }
            }
        }

#if !ENABLE_4CTB_EVALUATION
        if(e_quality_preset < ME_HIGH_QUALITY)
        {
            S32 best_inter_cost = ps_blk_64x64->best_inter_cost;
            S32 best_intra_cost =
                ((ps_ctb_cluster_info->ps_cur_ipe_ctb->i4_best64x64_intra_cost +
                  ps_ctb_cluster_info->i4_frame_qstep *
                      ps_ctb_cluster_info->i4_frame_qstep_multiplier * 16) < 0)
                    ? MAX_32BIT_VAL
                    : (ps_ctb_cluster_info->ps_cur_ipe_ctb->i4_best64x64_intra_cost +
                       ps_ctb_cluster_info->i4_frame_qstep *
                           ps_ctb_cluster_info->i4_frame_qstep_multiplier * 16);
            S32 best_cost = (best_inter_cost > best_intra_cost) ? best_intra_cost : best_inter_cost;
            S32 cost_differential = (best_inter_cost - best_cost);

            enable_64x64_merge =
                ((ALL_INTER_COST_DIFF_THR * best_cost) >= (100 * cost_differential));
        }
#endif
    }

    if(enable_64x64_merge)
    {
#if !USE_CLUSTER_DATA_AS_BLK_MERGE_CANDTS
        if(e_quality_preset < ME_HIGH_QUALITY)
#endif
        {
            hme_boot_out_outlier(ps_ctb_cluster_info, 64);

            hme_find_top_ref_ids(ps_ctb_cluster_info, bidir_enabled, 64);
        }

        hme_populate_cu_tree(
            ps_ctb_cluster_info,
            ps_ctb_cluster_info->ps_cu_tree_root,
            0,
            e_quality_preset,
            POS_NA,
            POS_NA,
            POS_NA);
    }
}
#endif

static __inline void hme_merge_prms_init(
    hme_merge_prms_t *ps_prms,
    layer_ctxt_t *ps_curr_layer,
    refine_prms_t *ps_refine_prms,
    me_frm_ctxt_t *ps_me_ctxt,
    range_prms_t *ps_range_prms_rec,
    range_prms_t *ps_range_prms_inp,
    mv_grid_t **pps_mv_grid,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    S32 i4_num_pred_dir,
    S32 i4_32x32_id,
    BLK_SIZE_T e_blk_size,
    ME_QUALITY_PRESETS_T e_me_quality_presets)
{
    S32 i4_use_rec = ps_refine_prms->i4_use_rec_in_fpel;
    S32 i4_cu_16x16 = (BLK_32x32 == e_blk_size) ? (i4_32x32_id << 2) : 0;

    /* Currently not enabling segmentation info from prev layers */
    ps_prms->i4_seg_info_avail = 0;
    ps_prms->i4_part_mask = 0;

    /* Number of reference pics in which to do merge */
    ps_prms->i4_num_ref = i4_num_pred_dir;

    /* Layer ctxt info */
    ps_prms->ps_layer_ctxt = ps_curr_layer;

    ps_prms->ps_inter_ctb_prms = ps_inter_ctb_prms;

    /* Top left, top right, bottom left and bottom right 16x16 units */
    if(BLK_32x32 == e_blk_size)
    {
        ps_prms->ps_results_tl = &ps_me_ctxt->as_search_results_16x16[i4_cu_16x16];
        ps_prms->ps_results_tr = &ps_me_ctxt->as_search_results_16x16[i4_cu_16x16 + 1];
        ps_prms->ps_results_bl = &ps_me_ctxt->as_search_results_16x16[i4_cu_16x16 + 2];
        ps_prms->ps_results_br = &ps_me_ctxt->as_search_results_16x16[i4_cu_16x16 + 3];

        /* Merge results stored here */
        ps_prms->ps_results_merge = &ps_me_ctxt->as_search_results_32x32[i4_32x32_id];

        /* This could be lesser than the number of 16x16results generated*/
        /* For now, keeping it to be same                                */
        ps_prms->i4_num_inp_results = ps_refine_prms->i4_num_fpel_results;
        ps_prms->ps_8x8_cu_results = &ps_me_ctxt->as_cu8x8_results[i4_32x32_id << 4];
        ps_prms->ps_results_grandchild = NULL;
    }
    else
    {
        ps_prms->ps_results_tl = &ps_me_ctxt->as_search_results_32x32[0];
        ps_prms->ps_results_tr = &ps_me_ctxt->as_search_results_32x32[1];
        ps_prms->ps_results_bl = &ps_me_ctxt->as_search_results_32x32[2];
        ps_prms->ps_results_br = &ps_me_ctxt->as_search_results_32x32[3];

        /* Merge results stored here */
        ps_prms->ps_results_merge = &ps_me_ctxt->s_search_results_64x64;

        ps_prms->i4_num_inp_results = ps_refine_prms->i4_num_32x32_merge_results;
        ps_prms->ps_8x8_cu_results = &ps_me_ctxt->as_cu8x8_results[0];
        ps_prms->ps_results_grandchild = ps_me_ctxt->as_search_results_16x16;
    }

    if(i4_use_rec)
    {
        WORD32 ref_ctr;

        for(ref_ctr = 0; ref_ctr < MAX_NUM_REF; ref_ctr++)
        {
            ps_prms->aps_mv_range[ref_ctr] = &ps_range_prms_rec[ref_ctr];
        }
    }
    else
    {
        WORD32 ref_ctr;

        for(ref_ctr = 0; ref_ctr < MAX_NUM_REF; ref_ctr++)
        {
            ps_prms->aps_mv_range[ref_ctr] = &ps_range_prms_inp[ref_ctr];
        }
    }
    ps_prms->i4_use_rec = i4_use_rec;

    ps_prms->pf_mv_cost_compute = compute_mv_cost_implicit_high_speed;

    ps_prms->pps_mv_grid = pps_mv_grid;

    ps_prms->log_ctb_size = ps_me_ctxt->log_ctb_size;

    ps_prms->e_quality_preset = e_me_quality_presets;
    ps_prms->pi1_future_list = ps_me_ctxt->ai1_future_list;
    ps_prms->pi1_past_list = ps_me_ctxt->ai1_past_list;
    ps_prms->ps_cluster_info = ps_me_ctxt->ps_ctb_cluster_info;
}

/**
********************************************************************************
*  @fn   void hme_refine(me_ctxt_t *ps_ctxt,
*                       refine_layer_prms_t *ps_refine_prms)
*
*  @brief  Top level entry point for refinement ME
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_refine_prms : refinement layer prms
*
*  @return None
********************************************************************************
*/
void hme_refine(
    me_ctxt_t *ps_thrd_ctxt,
    refine_prms_t *ps_refine_prms,
    PF_EXT_UPDATE_FXN_T pf_ext_update_fxn,
    layer_ctxt_t *ps_coarse_layer,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    S32 lyr_job_type,
    S32 thrd_id,
    S32 me_frm_id,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_l0_ipe_input)
{
    inter_ctb_prms_t s_common_frm_prms;

    BLK_SIZE_T e_search_blk_size, e_result_blk_size;
    WORD32 i4_me_frm_id = me_frm_id % MAX_NUM_ME_PARALLEL;
    me_frm_ctxt_t *ps_ctxt = ps_thrd_ctxt->aps_me_frm_prms[i4_me_frm_id];
    ME_QUALITY_PRESETS_T e_me_quality_presets =
        ps_thrd_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets;

    WORD32 num_rows_proc = 0;
    WORD32 num_act_ref_pics;
    WORD16 i2_prev_enc_frm_max_mv_y;
    WORD32 i4_idx_dvsr_p = ps_multi_thrd_ctxt->i4_idx_dvsr_p;

    /*************************************************************************/
    /* Complexity of search: Low to High                                     */
    /*************************************************************************/
    SEARCH_COMPLEXITY_T e_search_complexity;

    /*************************************************************************/
    /* to store the PU results which are passed to the decide_part_types     */
    /* as input prms. Multiplied by 4 as the max number of Ref in a List is 4*/
    /*************************************************************************/

    pu_result_t as_pu_results[2][TOT_NUM_PARTS][MAX_NUM_RESULTS_PER_PART_LIST];
    inter_pu_results_t as_inter_pu_results[4];
    inter_pu_results_t *ps_pu_results = as_inter_pu_results;

    /*************************************************************************/
    /* Config parameter structures for varius ME submodules                  */
    /*************************************************************************/
    hme_merge_prms_t s_merge_prms_32x32_tl, s_merge_prms_32x32_tr;
    hme_merge_prms_t s_merge_prms_32x32_bl, s_merge_prms_32x32_br;
    hme_merge_prms_t s_merge_prms_64x64;
    hme_search_prms_t s_search_prms_blk;
    mvbank_update_prms_t s_mv_update_prms;
    hme_ctb_prms_t s_ctb_prms;
    hme_subpel_prms_t s_subpel_prms;
    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt = ps_ctxt->ps_fullpel_refine_ctxt;
    ctb_cluster_info_t *ps_ctb_cluster_info;
    fpel_srch_cand_init_data_t s_srch_cand_init_data;

    /* 4 bits (LSBs) of this variable control merge of 4 32x32 CUs in CTB */
    S32 en_merge_32x32;
    /* 5 lsb's specify whether or not merge algorithm is required */
    /* to be executed or not. Relevant only in PQ. Ought to be */
    /* used in conjunction with en_merge_32x32 and */
    /* ps_ctb_bound_attrs->u1_merge_to_64x64_flag. This is */
    /* required when all children are deemed to be intras */
    S32 en_merge_execution;

    /*************************************************************************/
    /* All types of search candidates for predictor based search.            */
    /*************************************************************************/
    S32 num_init_candts = 0;
    S32 i4_num_act_ref_l0 = ps_ctxt->s_frm_prms.u1_num_active_ref_l0;
    S32 i4_num_act_ref_l1 = ps_ctxt->s_frm_prms.u1_num_active_ref_l1;
    search_candt_t *ps_search_candts, as_search_candts[MAX_INIT_CANDTS];
    search_node_t as_top_neighbours[4], as_left_neighbours[3];

    pf_get_wt_inp fp_get_wt_inp;

    search_node_t as_unique_search_nodes[MAX_INIT_CANDTS * 9];
    U32 au4_unique_node_map[MAP_X_MAX * 2];

    /* Controls the boundary attributes of CTB, whether it has 64x64 or not */
    ctb_boundary_attrs_t *ps_ctb_bound_attrs;

    /*************************************************************************/
    /* points ot the search results for the blk level search (8x8/16x16)     */
    /*************************************************************************/
    search_results_t *ps_search_results;

    /*************************************************************************/
    /* Coordinates                                                           */
    /*************************************************************************/
    S32 blk_x, blk_y, i4_ctb_x, i4_ctb_y, tile_col_idx, blk_id_in_ctb;
    S32 pos_x, pos_y;
    S32 blk_id_in_full_ctb;

    /*************************************************************************/
    /* Related to dimensions of block being searched and pic dimensions      */
    /*************************************************************************/
    S32 blk_4x4_to_16x16;
    S32 blk_wd, blk_ht, blk_size_shift;
    S32 i4_pic_wd, i4_pic_ht, num_blks_in_this_ctb;
    S32 num_results_prev_layer;

    /*************************************************************************/
    /* Size of a basic unit for this layer. For non encode layers, we search */
    /* in block sizes of 8x8. For encode layers, though we search 16x16s the */
    /* basic unit size is the ctb size.                                      */
    /*************************************************************************/
    S32 unit_size;

    /*************************************************************************/
    /* Local variable storing results of any 4 CU merge to bigger CU         */
    /*************************************************************************/
    CU_MERGE_RESULT_T e_merge_result;

    /*************************************************************************/
    /* This mv grid stores results during and after fpel search, during      */
    /* merge, subpel and bidirect refinements stages. 2 instances of this are*/
    /* meant for the 2 directions of search (l0 and l1).                     */
    /*************************************************************************/
    mv_grid_t *aps_mv_grid[2];

    /*************************************************************************/
    /* Pointers to context in current and coarser layers                     */
    /*************************************************************************/
    layer_ctxt_t *ps_curr_layer, *ps_prev_layer;

    /*************************************************************************/
    /* to store mv range per blk, and picture limit, allowed search range    */
    /* range prms in hpel and qpel units as well                             */
    /*************************************************************************/
    range_prms_t as_range_prms_inp[MAX_NUM_REF], as_range_prms_rec[MAX_NUM_REF];
    range_prms_t s_pic_limit_inp, s_pic_limit_rec, as_mv_limit[MAX_NUM_REF];
    range_prms_t as_range_prms_hpel[MAX_NUM_REF], as_range_prms_qpel[MAX_NUM_REF];

    /*************************************************************************/
    /* These variables are used to track number of references at different   */
    /* stages of ME.                                                         */
    /*************************************************************************/
    S32 i4_num_pred_dir;
    S32 i4_num_ref_each_dir, i, i4_num_ref_prev_layer;
    S32 lambda_recon = ps_refine_prms->lambda_recon;

    /* Counts successful merge to 32x32 every CTB (0-4) */
    S32 merge_count_32x32;

    S32 ai4_id_coloc[14], ai4_id_Z[2];
    U08 au1_search_candidate_list_index[2];
    S32 ai4_num_coloc_cands[2];
    U08 u1_pred_dir, u1_pred_dir_ctr;

    /*************************************************************************/
    /* Input pointer and stride                                              */
    /*************************************************************************/
    U08 *pu1_inp;
    S32 i4_inp_stride;
    S32 end_of_frame;
    S32 num_sync_units_in_row, num_sync_units_in_tile;

    /*************************************************************************/
    /* Indicates whether the all 4 8x8 blks are valid in the 16x16 blk in the*/
    /* encode layer. If not 15, then 1 or more 8x8 blks not valid. Means that*/
    /* we need to stop merges and force 8x8 CUs for that 16x16 blk           */
    /*************************************************************************/
    S32 blk_8x8_mask;
    S32 ai4_blk_8x8_mask[16];
    U08 au1_is_64x64Blk_noisy[1];
    U08 au1_is_32x32Blk_noisy[4];
    U08 au1_is_16x16Blk_noisy[16];

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list =
        ps_thrd_ctxt->ps_cmn_utils_optimised_function_list;
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list =
        ((ihevce_me_optimised_function_list_t *)ps_thrd_ctxt->pv_me_optimised_function_list);

    ASSERT(ps_refine_prms->i4_layer_id < ps_ctxt->num_layers - 1);

    /*************************************************************************/
    /* Pointers to current and coarse layer are needed for projection */
    /* Pointer to prev layer are needed for other candts like coloc   */
    /*************************************************************************/
    ps_curr_layer = ps_ctxt->ps_curr_descr->aps_layers[ps_refine_prms->i4_layer_id];

    ps_prev_layer = hme_get_past_layer_ctxt(
        ps_thrd_ctxt, ps_ctxt, ps_refine_prms->i4_layer_id, ps_multi_thrd_ctxt->i4_num_me_frm_pllel);

    num_results_prev_layer = ps_coarse_layer->ps_layer_mvbank->i4_num_mvs_per_ref;

    /* Function pointer is selected based on the C vc X86 macro */

    fp_get_wt_inp = ps_me_optimised_function_list->pf_get_wt_inp_ctb;

    i4_inp_stride = ps_curr_layer->i4_inp_stride;
    i4_pic_wd = ps_curr_layer->i4_wd;
    i4_pic_ht = ps_curr_layer->i4_ht;
    e_search_complexity = ps_refine_prms->e_search_complexity;
    end_of_frame = 0;

    /* This points to all the initial candts */
    ps_search_candts = &as_search_candts[0];

    /* mv grid being huge strucutre is part of context */
    aps_mv_grid[0] = &ps_ctxt->as_mv_grid[0];
    aps_mv_grid[1] = &ps_ctxt->as_mv_grid[1];

    /*************************************************************************/
    /* If the current layer is encoded (since it may be multicast or final   */
    /* layer (finest)), then we use 16x16 blk size with some selected parts  */
    /* If the current layer is not encoded, then we use 8x8 blk size, with   */
    /* enable or disable of 4x4 partitions depending on the input prms       */
    /*************************************************************************/
    e_search_blk_size = BLK_16x16;
    blk_wd = blk_ht = 16;
    blk_size_shift = 4;
    e_result_blk_size = BLK_8x8;
    s_mv_update_prms.i4_shift = 1;

    if(ps_coarse_layer->ps_layer_mvbank->e_blk_size == BLK_4x4)
    {
        blk_4x4_to_16x16 = 1;
    }
    else
    {
        blk_4x4_to_16x16 = 0;
    }

    unit_size = 1 << ps_ctxt->log_ctb_size;
    s_search_prms_blk.i4_inp_stride = unit_size;

    /* This is required to properly update the layer mv bank */
    s_mv_update_prms.e_search_blk_size = e_search_blk_size;
    s_search_prms_blk.e_blk_size = e_search_blk_size;

    /*************************************************************************/
    /* If current layer is explicit, then the number of ref frames are to    */
    /* be same as previous layer. Else it will be 2                          */
    /*************************************************************************/
    i4_num_ref_prev_layer = ps_coarse_layer->ps_layer_mvbank->i4_num_ref;
    i4_num_pred_dir =
        (ps_ctxt->s_frm_prms.bidir_enabled && (i4_num_act_ref_l0 > 0) && (i4_num_act_ref_l1 > 0)) +
        1;

#if USE_MODIFIED == 1
    s_search_prms_blk.pf_mv_cost_compute = compute_mv_cost_implicit_high_speed_modified;
#else
    s_search_prms_blk.pf_mv_cost_compute = compute_mv_cost_implicit_high_speed;
#endif

    i4_num_pred_dir = MIN(i4_num_pred_dir, i4_num_ref_prev_layer);
    if(i4_num_ref_prev_layer <= 2)
    {
        i4_num_ref_each_dir = 1;
    }
    else
    {
        i4_num_ref_each_dir = i4_num_ref_prev_layer >> 1;
    }

    s_mv_update_prms.i4_num_ref = i4_num_pred_dir;
    s_mv_update_prms.i4_num_results_to_store =
        MIN((ps_ctxt->s_frm_prms.bidir_enabled) ? ps_curr_layer->ps_layer_mvbank->i4_num_mvs_per_ref
                                                : (i4_num_act_ref_l0 > 1) + 1,
            ps_refine_prms->i4_num_results_per_part);

    /*************************************************************************/
    /* Initialization of merge params for 16x16 to 32x32 merge.              */
    /* There are 4 32x32 units in a CTB, so 4 param structures initialized   */
    /*************************************************************************/
    {
        hme_merge_prms_t *aps_merge_prms[4];
        aps_merge_prms[0] = &s_merge_prms_32x32_tl;
        aps_merge_prms[1] = &s_merge_prms_32x32_tr;
        aps_merge_prms[2] = &s_merge_prms_32x32_bl;
        aps_merge_prms[3] = &s_merge_prms_32x32_br;
        for(i = 0; i < 4; i++)
        {
            hme_merge_prms_init(
                aps_merge_prms[i],
                ps_curr_layer,
                ps_refine_prms,
                ps_ctxt,
                as_range_prms_rec,
                as_range_prms_inp,
                &aps_mv_grid[0],
                &s_common_frm_prms,
                i4_num_pred_dir,
                i,
                BLK_32x32,
                e_me_quality_presets);
        }
    }

    /*************************************************************************/
    /* Initialization of merge params for 32x32 to 64x64 merge.              */
    /* There are 4 32x32 units in a CTB, so only 1 64x64 CU can be in CTB    */
    /*************************************************************************/
    {
        hme_merge_prms_init(
            &s_merge_prms_64x64,
            ps_curr_layer,
            ps_refine_prms,
            ps_ctxt,
            as_range_prms_rec,
            as_range_prms_inp,
            &aps_mv_grid[0],
            &s_common_frm_prms,
            i4_num_pred_dir,
            0,
            BLK_64x64,
            e_me_quality_presets);
    }

    /* Pointers to cu_results are initialised here */
    {
        WORD32 i;

        ps_ctxt->s_search_results_64x64.ps_cu_results = &ps_ctxt->s_cu64x64_results;

        for(i = 0; i < 4; i++)
        {
            ps_ctxt->as_search_results_32x32[i].ps_cu_results = &ps_ctxt->as_cu32x32_results[i];
        }

        for(i = 0; i < 16; i++)
        {
            ps_ctxt->as_search_results_16x16[i].ps_cu_results = &ps_ctxt->as_cu16x16_results[i];
        }
    }

    /*************************************************************************/
    /* SUBPEL Params initialized here                                        */
    /*************************************************************************/
    {
        s_subpel_prms.ps_search_results_16x16 = &ps_ctxt->as_search_results_16x16[0];
        s_subpel_prms.ps_search_results_32x32 = &ps_ctxt->as_search_results_32x32[0];
        s_subpel_prms.ps_search_results_64x64 = &ps_ctxt->s_search_results_64x64;

        s_subpel_prms.i4_num_16x16_candts = ps_refine_prms->i4_num_fpel_results;
        s_subpel_prms.i4_num_32x32_candts = ps_refine_prms->i4_num_32x32_merge_results;
        s_subpel_prms.i4_num_64x64_candts = ps_refine_prms->i4_num_64x64_merge_results;

        s_subpel_prms.i4_num_steps_hpel_refine = ps_refine_prms->i4_num_steps_hpel_refine;
        s_subpel_prms.i4_num_steps_qpel_refine = ps_refine_prms->i4_num_steps_qpel_refine;

        s_subpel_prms.i4_use_satd = ps_refine_prms->i4_use_satd_subpel;

        s_subpel_prms.i4_inp_stride = unit_size;

        s_subpel_prms.u1_max_subpel_candts_2Nx2N = ps_refine_prms->u1_max_subpel_candts_2Nx2N;
        s_subpel_prms.u1_max_subpel_candts_NxN = ps_refine_prms->u1_max_subpel_candts_NxN;
        s_subpel_prms.u1_subpel_candt_threshold = ps_refine_prms->u1_subpel_candt_threshold;

        s_subpel_prms.pf_qpel_interp = ps_me_optimised_function_list->pf_qpel_interp_avg_generic;

        {
            WORD32 ref_ctr;
            for(ref_ctr = 0; ref_ctr < MAX_NUM_REF; ref_ctr++)
            {
                s_subpel_prms.aps_mv_range_hpel[ref_ctr] = &as_range_prms_hpel[ref_ctr];
                s_subpel_prms.aps_mv_range_qpel[ref_ctr] = &as_range_prms_qpel[ref_ctr];
            }
        }
        s_subpel_prms.pi2_inp_bck = ps_ctxt->pi2_inp_bck;

#if USE_MODIFIED == 0
        s_subpel_prms.pf_mv_cost_compute = compute_mv_cost_implicit_high_speed;
#else
        s_subpel_prms.pf_mv_cost_compute = compute_mv_cost_implicit_high_speed_modified;
#endif
        s_subpel_prms.e_me_quality_presets = e_me_quality_presets;

        /* BI Refinement done only if this field is 1 */
        s_subpel_prms.bidir_enabled = ps_refine_prms->bidir_enabled;

        s_subpel_prms.u1_num_ref = ps_ctxt->num_ref_future + ps_ctxt->num_ref_past;

        s_subpel_prms.i4_num_act_ref_l0 = ps_ctxt->s_frm_prms.u1_num_active_ref_l0;
        s_subpel_prms.i4_num_act_ref_l1 = ps_ctxt->s_frm_prms.u1_num_active_ref_l1;
        s_subpel_prms.u1_max_num_subpel_refine_centers =
            ps_refine_prms->u1_max_num_subpel_refine_centers;
    }

    /* inter_ctb_prms_t struct initialisation */
    {
        inter_ctb_prms_t *ps_inter_ctb_prms = &s_common_frm_prms;
        hme_subpel_prms_t *ps_subpel_prms = &s_subpel_prms;

        ps_inter_ctb_prms->pps_rec_list_l0 = ps_ctxt->ps_hme_ref_map->pps_rec_list_l0;
        ps_inter_ctb_prms->pps_rec_list_l1 = ps_ctxt->ps_hme_ref_map->pps_rec_list_l1;
        ps_inter_ctb_prms->wpred_log_wdc = ps_ctxt->s_wt_pred.wpred_log_wdc;
        ps_inter_ctb_prms->u1_max_tr_depth = ps_thrd_ctxt->s_init_prms.u1_max_tr_depth;
        ps_inter_ctb_prms->i1_quality_preset = e_me_quality_presets;
        ps_inter_ctb_prms->i4_bidir_enabled = ps_subpel_prms->bidir_enabled;
        ps_inter_ctb_prms->i4_inp_stride = ps_subpel_prms->i4_inp_stride;
        ps_inter_ctb_prms->u1_num_ref = ps_subpel_prms->u1_num_ref;
        ps_inter_ctb_prms->u1_use_satd = ps_subpel_prms->i4_use_satd;
        ps_inter_ctb_prms->i4_rec_stride = ps_curr_layer->i4_rec_stride;
        ps_inter_ctb_prms->u1_num_active_ref_l0 = ps_ctxt->s_frm_prms.u1_num_active_ref_l0;
        ps_inter_ctb_prms->u1_num_active_ref_l1 = ps_ctxt->s_frm_prms.u1_num_active_ref_l1;
        ps_inter_ctb_prms->i4_lamda = lambda_recon;
        ps_inter_ctb_prms->u1_lamda_qshift = ps_refine_prms->lambda_q_shift;
        ps_inter_ctb_prms->i4_qstep_ls8 = ps_ctxt->ps_hme_frm_prms->qstep_ls8;
        ps_inter_ctb_prms->pi4_inv_wt = ps_ctxt->s_wt_pred.a_inv_wpred_wt;
        ps_inter_ctb_prms->pi1_past_list = ps_ctxt->ai1_past_list;
        ps_inter_ctb_prms->pi1_future_list = ps_ctxt->ai1_future_list;
        ps_inter_ctb_prms->pu4_src_variance = s_search_prms_blk.au4_src_variance;
        ps_inter_ctb_prms->u1_max_2nx2n_tu_recur_cands =
            ps_refine_prms->u1_max_2nx2n_tu_recur_cands;
    }

    for(i = 0; i < MAX_INIT_CANDTS; i++)
    {
        ps_search_candts[i].ps_search_node = &ps_ctxt->s_init_search_node[i];
        ps_search_candts[i].ps_search_node->ps_mv = &ps_ctxt->as_search_cand_mv[i];

        INIT_SEARCH_NODE(ps_search_candts[i].ps_search_node, 0);
    }
    num_act_ref_pics =
        ps_ctxt->s_frm_prms.u1_num_active_ref_l0 + ps_ctxt->s_frm_prms.u1_num_active_ref_l1;

    if(num_act_ref_pics)
    {
        hme_search_cand_data_init(
            ai4_id_Z,
            ai4_id_coloc,
            ai4_num_coloc_cands,
            au1_search_candidate_list_index,
            i4_num_act_ref_l0,
            i4_num_act_ref_l1,
            ps_ctxt->s_frm_prms.bidir_enabled,
            blk_4x4_to_16x16);
    }

    if(!ps_ctxt->s_frm_prms.bidir_enabled && (i4_num_act_ref_l0 > 1))
    {
        ps_search_candts[ai4_id_Z[0]].ps_search_node->i1_ref_idx = ps_ctxt->ai1_past_list[0];
        ps_search_candts[ai4_id_Z[1]].ps_search_node->i1_ref_idx = ps_ctxt->ai1_past_list[1];
    }
    else if(!ps_ctxt->s_frm_prms.bidir_enabled && (i4_num_act_ref_l0 == 1))
    {
        ps_search_candts[ai4_id_Z[0]].ps_search_node->i1_ref_idx = ps_ctxt->ai1_past_list[0];
    }

    for(i = 0; i < 3; i++)
    {
        search_node_t *ps_search_node;
        ps_search_node = &as_left_neighbours[i];
        INIT_SEARCH_NODE(ps_search_node, 0);
        ps_search_node = &as_top_neighbours[i];
        INIT_SEARCH_NODE(ps_search_node, 0);
    }

    INIT_SEARCH_NODE(&as_top_neighbours[3], 0);
    as_left_neighbours[2].u1_is_avail = 0;

    /*************************************************************************/
    /* Initialize all the search results structure here. We update all the   */
    /* search results to default values, and configure things like blk sizes */
    /*************************************************************************/
    if(num_act_ref_pics)
    {
        S32 i4_x, i4_y;
        /* 16x16 results */
        for(i = 0; i < 16; i++)
        {
            search_results_t *ps_search_results;
            S32 pred_lx;
            ps_search_results = &ps_ctxt->as_search_results_16x16[i];
            i4_x = (S32)gau1_encode_to_raster_x[i];
            i4_y = (S32)gau1_encode_to_raster_y[i];
            i4_x <<= 4;
            i4_y <<= 4;

            hme_init_search_results(
                ps_search_results,
                i4_num_pred_dir,
                ps_refine_prms->i4_num_fpel_results,
                ps_refine_prms->i4_num_results_per_part,
                e_search_blk_size,
                i4_x,
                i4_y,
                &ps_ctxt->au1_is_past[0]);

            for(pred_lx = 0; pred_lx < 2; pred_lx++)
            {
                pred_ctxt_t *ps_pred_ctxt;

                ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];

                hme_init_pred_ctxt_encode(
                    ps_pred_ctxt,
                    ps_search_results,
                    ps_search_candts[ai4_id_coloc[0]].ps_search_node,
                    ps_search_candts[ai4_id_Z[0]].ps_search_node,
                    aps_mv_grid[pred_lx],
                    pred_lx,
                    lambda_recon,
                    ps_refine_prms->lambda_q_shift,
                    &ps_ctxt->apu1_ref_bits_tlu_lc[0],
                    &ps_ctxt->ai2_ref_scf[0]);
            }
        }

        for(i = 0; i < 4; i++)
        {
            search_results_t *ps_search_results;
            S32 pred_lx;
            ps_search_results = &ps_ctxt->as_search_results_32x32[i];

            i4_x = (S32)gau1_encode_to_raster_x[i];
            i4_y = (S32)gau1_encode_to_raster_y[i];
            i4_x <<= 5;
            i4_y <<= 5;

            hme_init_search_results(
                ps_search_results,
                i4_num_pred_dir,
                ps_refine_prms->i4_num_32x32_merge_results,
                ps_refine_prms->i4_num_results_per_part,
                BLK_32x32,
                i4_x,
                i4_y,
                &ps_ctxt->au1_is_past[0]);

            for(pred_lx = 0; pred_lx < 2; pred_lx++)
            {
                pred_ctxt_t *ps_pred_ctxt;

                ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];

                hme_init_pred_ctxt_encode(
                    ps_pred_ctxt,
                    ps_search_results,
                    ps_search_candts[ai4_id_coloc[0]].ps_search_node,
                    ps_search_candts[ai4_id_Z[0]].ps_search_node,
                    aps_mv_grid[pred_lx],
                    pred_lx,
                    lambda_recon,
                    ps_refine_prms->lambda_q_shift,
                    &ps_ctxt->apu1_ref_bits_tlu_lc[0],
                    &ps_ctxt->ai2_ref_scf[0]);
            }
        }

        {
            search_results_t *ps_search_results;
            S32 pred_lx;
            ps_search_results = &ps_ctxt->s_search_results_64x64;

            hme_init_search_results(
                ps_search_results,
                i4_num_pred_dir,
                ps_refine_prms->i4_num_64x64_merge_results,
                ps_refine_prms->i4_num_results_per_part,
                BLK_64x64,
                0,
                0,
                &ps_ctxt->au1_is_past[0]);

            for(pred_lx = 0; pred_lx < 2; pred_lx++)
            {
                pred_ctxt_t *ps_pred_ctxt;

                ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];

                hme_init_pred_ctxt_encode(
                    ps_pred_ctxt,
                    ps_search_results,
                    ps_search_candts[ai4_id_coloc[0]].ps_search_node,
                    ps_search_candts[ai4_id_Z[0]].ps_search_node,
                    aps_mv_grid[pred_lx],
                    pred_lx,
                    lambda_recon,
                    ps_refine_prms->lambda_q_shift,
                    &ps_ctxt->apu1_ref_bits_tlu_lc[0],
                    &ps_ctxt->ai2_ref_scf[0]);
            }
        }
    }

    /* Initialise the structure used in clustering  */
    if(ME_PRISTINE_QUALITY == e_me_quality_presets)
    {
        ps_ctb_cluster_info = ps_ctxt->ps_ctb_cluster_info;

        ps_ctb_cluster_info->ps_16x16_blk = ps_ctxt->ps_blk_16x16;
        ps_ctb_cluster_info->ps_32x32_blk = ps_ctxt->ps_blk_32x32;
        ps_ctb_cluster_info->ps_64x64_blk = ps_ctxt->ps_blk_64x64;
        ps_ctb_cluster_info->pi4_blk_8x8_mask = ai4_blk_8x8_mask;
        ps_ctb_cluster_info->sdi_threshold = ps_refine_prms->sdi_threshold;
        ps_ctb_cluster_info->i4_frame_qstep = ps_ctxt->frm_qstep;
        ps_ctb_cluster_info->i4_frame_qstep_multiplier = 16;
    }

    /*********************************************************************/
    /* Initialize the dyn. search range params. for each reference index */
    /* in current layer ctxt                                             */
    /*********************************************************************/

    /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
    if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
    {
        WORD32 ref_ctr;
        /* set no. of act ref in L0 for further use at frame level */
        ps_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i4_num_act_ref_in_l0 =
            ps_ctxt->s_frm_prms.u1_num_active_ref_l0;

        for(ref_ctr = 0; ref_ctr < ps_ctxt->s_frm_prms.u1_num_active_ref_l0; ref_ctr++)
        {
            INIT_DYN_SEARCH_PRMS(
                &ps_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].as_dyn_range_prms[ref_ctr],
                ps_ctxt->ai4_ref_idx_to_poc_lc[ref_ctr]);
        }
    }
    /*************************************************************************/
    /* Now that the candidates have been ordered, to choose the right number */
    /* of initial candidates.                                                */
    /*************************************************************************/
    if(blk_4x4_to_16x16)
    {
        if(i4_num_ref_prev_layer > 2)
        {
            if(e_search_complexity == SEARCH_CX_LOW)
                num_init_candts = 7 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_MED)
                num_init_candts = 14 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_HIGH)
                num_init_candts = 21 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else
                ASSERT(0);
        }
        else if(i4_num_ref_prev_layer == 2)
        {
            if(e_search_complexity == SEARCH_CX_LOW)
                num_init_candts = 5 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_MED)
                num_init_candts = 12 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_HIGH)
                num_init_candts = 19 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else
                ASSERT(0);
        }
        else
        {
            if(e_search_complexity == SEARCH_CX_LOW)
                num_init_candts = 5;
            else if(e_search_complexity == SEARCH_CX_MED)
                num_init_candts = 12;
            else if(e_search_complexity == SEARCH_CX_HIGH)
                num_init_candts = 19;
            else
                ASSERT(0);
        }
    }
    else
    {
        if(i4_num_ref_prev_layer > 2)
        {
            if(e_search_complexity == SEARCH_CX_LOW)
                num_init_candts = 7 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_MED)
                num_init_candts = 13 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_HIGH)
                num_init_candts = 18 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else
                ASSERT(0);
        }
        else if(i4_num_ref_prev_layer == 2)
        {
            if(e_search_complexity == SEARCH_CX_LOW)
                num_init_candts = 5 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_MED)
                num_init_candts = 11 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else if(e_search_complexity == SEARCH_CX_HIGH)
                num_init_candts = 16 * (!ps_ctxt->s_frm_prms.bidir_enabled + 1);
            else
                ASSERT(0);
        }
        else
        {
            if(e_search_complexity == SEARCH_CX_LOW)
                num_init_candts = 5;
            else if(e_search_complexity == SEARCH_CX_MED)
                num_init_candts = 11;
            else if(e_search_complexity == SEARCH_CX_HIGH)
                num_init_candts = 16;
            else
                ASSERT(0);
        }
    }

    /*************************************************************************/
    /* The following search parameters are fixed throughout the search across*/
    /* all blks. So these are configured outside processing loop             */
    /*************************************************************************/
    s_search_prms_blk.i4_num_init_candts = num_init_candts;
    s_search_prms_blk.i4_start_step = 1;
    s_search_prms_blk.i4_use_satd = 0;
    s_search_prms_blk.i4_num_steps_post_refine = ps_refine_prms->i4_num_steps_post_refine_fpel;
    /* we use recon only for encoded layers, otherwise it is not available */
    s_search_prms_blk.i4_use_rec = ps_refine_prms->i4_encode & ps_refine_prms->i4_use_rec_in_fpel;

    s_search_prms_blk.ps_search_candts = ps_search_candts;
    if(s_search_prms_blk.i4_use_rec)
    {
        WORD32 ref_ctr;
        for(ref_ctr = 0; ref_ctr < MAX_NUM_REF; ref_ctr++)
            s_search_prms_blk.aps_mv_range[ref_ctr] = &as_range_prms_rec[ref_ctr];
    }
    else
    {
        WORD32 ref_ctr;
        for(ref_ctr = 0; ref_ctr < MAX_NUM_REF; ref_ctr++)
            s_search_prms_blk.aps_mv_range[ref_ctr] = &as_range_prms_inp[ref_ctr];
    }

    /*************************************************************************/
    /* Initialize coordinates. Meaning as follows                            */
    /* blk_x : x coordinate of the 16x16 blk, in terms of number of blks     */
    /* blk_y : same as above, y coord.                                       */
    /* num_blks_in_this_ctb : number of blks in this given ctb that starts   */
    /* at i4_ctb_x, i4_ctb_y. This may not be 16 at picture boundaries.      */
    /* i4_ctb_x, i4_ctb_y: pixel coordinate of the ctb realtive to top left  */
    /* corner of the picture. Always multiple of 64.                         */
    /* blk_id_in_ctb : encode order id of the blk in the ctb.                */
    /*************************************************************************/
    blk_y = 0;
    blk_id_in_ctb = 0;
    i4_ctb_y = 0;

    /*************************************************************************/
    /* Picture limit on all 4 sides. This will be used to set mv limits for  */
    /* every block given its coordinate. Note thsi assumes that the min amt  */
    /* of padding to right of pic is equal to the blk size. If we go all the */
    /* way upto 64x64, then the min padding on right size of picture should  */
    /* be 64, and also on bottom side of picture.                            */
    /*************************************************************************/
    SET_PIC_LIMIT(
        s_pic_limit_inp,
        ps_curr_layer->i4_pad_x_rec,
        ps_curr_layer->i4_pad_y_rec,
        ps_curr_layer->i4_wd,
        ps_curr_layer->i4_ht,
        s_search_prms_blk.i4_num_steps_post_refine);

    SET_PIC_LIMIT(
        s_pic_limit_rec,
        ps_curr_layer->i4_pad_x_rec,
        ps_curr_layer->i4_pad_y_rec,
        ps_curr_layer->i4_wd,
        ps_curr_layer->i4_ht,
        s_search_prms_blk.i4_num_steps_post_refine);

    /*************************************************************************/
    /* set the MV limit per ref. pic.                                        */
    /*    - P pic. : Based on the config params.                             */
    /*    - B/b pic: Based on the Max/Min MV from prev. P and config. param. */
    /*************************************************************************/
    hme_set_mv_limit_using_dvsr_data(
        ps_ctxt, ps_curr_layer, as_mv_limit, &i2_prev_enc_frm_max_mv_y, num_act_ref_pics);
    s_srch_cand_init_data.pu1_num_fpel_search_cands = ps_refine_prms->au1_num_fpel_search_cands;
    s_srch_cand_init_data.i4_num_act_ref_l0 = ps_ctxt->s_frm_prms.u1_num_active_ref_l0;
    s_srch_cand_init_data.i4_num_act_ref_l1 = ps_ctxt->s_frm_prms.u1_num_active_ref_l1;
    s_srch_cand_init_data.ps_coarse_layer = ps_coarse_layer;
    s_srch_cand_init_data.ps_curr_layer = ps_curr_layer;
    s_srch_cand_init_data.i4_max_num_init_cands = num_init_candts;
    s_srch_cand_init_data.ps_search_cands = ps_search_candts;
    s_srch_cand_init_data.u1_num_results_in_mvbank = s_mv_update_prms.i4_num_results_to_store;
    s_srch_cand_init_data.pi4_ref_id_lc_to_l0_map = ps_ctxt->a_ref_idx_lc_to_l0;
    s_srch_cand_init_data.pi4_ref_id_lc_to_l1_map = ps_ctxt->a_ref_idx_lc_to_l1;
    s_srch_cand_init_data.e_search_blk_size = e_search_blk_size;

    while(0 == end_of_frame)
    {
        job_queue_t *ps_job;
        frm_ctb_ctxt_t *ps_frm_ctb_prms;
        ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb;

        WORD32 i4_max_mv_x_in_ctb;
        WORD32 i4_max_mv_y_in_ctb;
        void *pv_dep_mngr_encloop_dep_me;
        WORD32 offset_val, check_dep_pos, set_dep_pos;
        WORD32 left_ctb_in_diff_tile, i4_first_ctb_x = 0;

        pv_dep_mngr_encloop_dep_me = ps_ctxt->pv_dep_mngr_encloop_dep_me;

        ps_frm_ctb_prms = (frm_ctb_ctxt_t *)ps_thrd_ctxt->pv_ext_frm_prms;

        /* Get the current row from the job queue */
        ps_job = (job_queue_t *)ihevce_enc_grp_get_next_job(
            ps_multi_thrd_ctxt, lyr_job_type, 1, me_frm_id);

        /* If all rows are done, set the end of process flag to 1, */
        /* and the current row to -1 */
        if(NULL == ps_job)
        {
            blk_y = -1;
            i4_ctb_y = -1;
            tile_col_idx = -1;
            end_of_frame = 1;

            continue;
        }

        /* set the output dependency after picking up the row */
        ihevce_enc_grp_job_set_out_dep(ps_multi_thrd_ctxt, ps_job, me_frm_id);

        /* Obtain the current row's details from the job */
        {
            ihevce_tile_params_t *ps_col_tile_params;

            i4_ctb_y = ps_job->s_job_info.s_me_job_info.i4_vert_unit_row_no;
            /* Obtain the current colum tile index from the job */
            tile_col_idx = ps_job->s_job_info.s_me_job_info.i4_tile_col_idx;

            /* in encode layer block are 16x16 and CTB is 64 x 64 */
            /* note if ctb is 32x32 the this calc needs to be changed */
            num_sync_units_in_row = (i4_pic_wd + ((1 << ps_ctxt->log_ctb_size) - 1)) >>
                                    ps_ctxt->log_ctb_size;

            /* The tile parameter for the col. idx. Use only the properties
            which is same for all the bottom tiles like width, start_x, etc.
            Don't use height, start_y, etc.                                  */
            ps_col_tile_params =
                ((ihevce_tile_params_t *)ps_thrd_ctxt->pv_tile_params_base + tile_col_idx);
            /* in encode layer block are 16x16 and CTB is 64 x 64 */
            /* note if ctb is 32x32 the this calc needs to be changed */
            num_sync_units_in_tile =
                (ps_col_tile_params->i4_curr_tile_width + ((1 << ps_ctxt->log_ctb_size) - 1)) >>
                ps_ctxt->log_ctb_size;

            i4_first_ctb_x = ps_col_tile_params->i4_first_ctb_x;
            i4_ctb_x = i4_first_ctb_x;

            if(!num_act_ref_pics)
            {
                for(i4_ctb_x = i4_first_ctb_x;
                    i4_ctb_x < (ps_col_tile_params->i4_first_ctb_x + num_sync_units_in_tile);
                    i4_ctb_x++)
                {
                    S32 blk_i = 0, blk_j = 0;
                    /* set the dependency for the corresponding row in enc loop */
                    ihevce_dmgr_set_row_row_sync(
                        pv_dep_mngr_encloop_dep_me,
                        (i4_ctb_x + 1),
                        i4_ctb_y,
                        tile_col_idx /* Col Tile No. */);
                }

                continue;
            }

            /* increment the number of rows proc */
            num_rows_proc++;

            /* Set Variables for Dep. Checking and Setting */
            set_dep_pos = i4_ctb_y + 1;
            if(i4_ctb_y > 0)
            {
                offset_val = 2;
                check_dep_pos = i4_ctb_y - 1;
            }
            else
            {
                /* First row should run without waiting */
                offset_val = -1;
                check_dep_pos = 0;
            }

            /* row ctb out pointer  */
            ps_ctxt->ps_ctb_analyse_curr_row =
                ps_ctxt->ps_ctb_analyse_base + i4_ctb_y * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /* Row level CU Tree buffer */
            ps_ctxt->ps_cu_tree_curr_row =
                ps_ctxt->ps_cu_tree_base +
                i4_ctb_y * ps_frm_ctb_prms->i4_num_ctbs_horz * MAX_NUM_NODES_CU_TREE;

            ps_ctxt->ps_me_ctb_data_curr_row =
                ps_ctxt->ps_me_ctb_data_base + i4_ctb_y * ps_frm_ctb_prms->i4_num_ctbs_horz;
        }

        /* This flag says the CTB under processing is at the start of tile in horz dir.*/
        left_ctb_in_diff_tile = 1;

        /* To make sure no 64-bit overflow happens when inv_wt is multiplied with un-normalized src_var,                                 */
        /* the shift value will be passed onto the functions wherever inv_wt isused so that inv_wt is appropriately shift and multiplied */
        {
            S32 i4_ref_id, i4_bits_req;

            for(i4_ref_id = 0; i4_ref_id < (ps_ctxt->s_frm_prms.u1_num_active_ref_l0 +
                                            ps_ctxt->s_frm_prms.u1_num_active_ref_l1);
                i4_ref_id++)
            {
                GETRANGE(i4_bits_req, ps_ctxt->s_wt_pred.a_inv_wpred_wt[i4_ref_id]);

                if(i4_bits_req > 12)
                {
                    ps_ctxt->s_wt_pred.ai4_shift_val[i4_ref_id] = (i4_bits_req - 12);
                }
                else
                {
                    ps_ctxt->s_wt_pred.ai4_shift_val[i4_ref_id] = 0;
                }
            }

            s_common_frm_prms.pi4_inv_wt_shift_val = ps_ctxt->s_wt_pred.ai4_shift_val;
        }

        /* if non-encode layer then i4_ctb_x will be same as blk_x */
        /* loop over all the units is a row                        */
        for(i4_ctb_x = i4_first_ctb_x; i4_ctb_x < (i4_first_ctb_x + num_sync_units_in_tile);
            i4_ctb_x++)
        {
            ihevce_ctb_noise_params *ps_ctb_noise_params =
                &ps_ctxt->ps_ctb_analyse_curr_row[i4_ctb_x].s_ctb_noise_params;

            s_common_frm_prms.i4_ctb_x_off = i4_ctb_x << 6;
            s_common_frm_prms.i4_ctb_y_off = i4_ctb_y << 6;

            ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_y = i4_ctb_y << 6;
            ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_x = i4_ctb_x << 6;
            /* Initialize ptr to current IPE CTB */
            ps_cur_ipe_ctb = ps_ctxt->ps_ipe_l0_ctb_frm_base + i4_ctb_x +
                             i4_ctb_y * ps_frm_ctb_prms->i4_num_ctbs_horz;
            {
                ps_ctb_bound_attrs =
                    get_ctb_attrs(i4_ctb_x << 6, i4_ctb_y << 6, i4_pic_wd, i4_pic_ht, ps_ctxt);

                en_merge_32x32 = ps_ctb_bound_attrs->u1_merge_to_32x32_flag;
                num_blks_in_this_ctb = ps_ctb_bound_attrs->u1_num_blks_in_ctb;
            }

            /* Block to initialise pointers to part_type_results_t */
            /* in each size-specific inter_cu_results_t  */
            {
                WORD32 i;

                for(i = 0; i < 64; i++)
                {
                    ps_ctxt->as_cu8x8_results[i].ps_best_results =
                        ps_ctxt->ps_me_ctb_data_curr_row[i4_ctb_x]
                            .as_8x8_block_data[i]
                            .as_best_results;
                    ps_ctxt->as_cu8x8_results[i].u1_num_best_results = 0;
                }

                for(i = 0; i < 16; i++)
                {
                    ps_ctxt->as_cu16x16_results[i].ps_best_results =
                        ps_ctxt->ps_me_ctb_data_curr_row[i4_ctb_x].as_block_data[i].as_best_results;
                    ps_ctxt->as_cu16x16_results[i].u1_num_best_results = 0;
                }

                for(i = 0; i < 4; i++)
                {
                    ps_ctxt->as_cu32x32_results[i].ps_best_results =
                        ps_ctxt->ps_me_ctb_data_curr_row[i4_ctb_x]
                            .as_32x32_block_data[i]
                            .as_best_results;
                    ps_ctxt->as_cu32x32_results[i].u1_num_best_results = 0;
                }

                ps_ctxt->s_cu64x64_results.ps_best_results =
                    ps_ctxt->ps_me_ctb_data_curr_row[i4_ctb_x].s_64x64_block_data.as_best_results;
                ps_ctxt->s_cu64x64_results.u1_num_best_results = 0;
            }

            if(ME_PRISTINE_QUALITY == e_me_quality_presets)
            {
                ps_ctb_cluster_info->blk_32x32_mask = en_merge_32x32;
                ps_ctb_cluster_info->ps_cur_ipe_ctb = ps_cur_ipe_ctb;
                ps_ctb_cluster_info->ps_cu_tree_root =
                    ps_ctxt->ps_cu_tree_curr_row + (i4_ctb_x * MAX_NUM_NODES_CU_TREE);
                ps_ctb_cluster_info->nodes_created_in_cu_tree = 1;
            }

            if(ME_PRISTINE_QUALITY != e_me_quality_presets)
            {
                S32 i4_nodes_created_in_cu_tree = 1;

                ihevce_cu_tree_init(
                    (ps_ctxt->ps_cu_tree_curr_row + (i4_ctb_x * MAX_NUM_NODES_CU_TREE)),
                    (ps_ctxt->ps_cu_tree_curr_row + (i4_ctb_x * MAX_NUM_NODES_CU_TREE)),
                    &i4_nodes_created_in_cu_tree,
                    0,
                    POS_NA,
                    POS_NA,
                    POS_NA);
            }

            memset(ai4_blk_8x8_mask, 0, 16 * sizeof(S32));

            if(ps_refine_prms->u1_use_lambda_derived_from_min_8x8_act_in_ctb)
            {
                S32 j;

                ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb;

                ps_cur_ipe_ctb =
                    ps_ctxt->ps_ipe_l0_ctb_frm_base + i4_ctb_x + i4_ctb_y * num_sync_units_in_row;
                lambda_recon =
                    hme_recompute_lambda_from_min_8x8_act_in_ctb(ps_ctxt, ps_cur_ipe_ctb);

                lambda_recon = ((float)lambda_recon * (100.0f - ME_LAMBDA_DISCOUNT) / 100.0f);

                for(i = 0; i < 4; i++)
                {
                    ps_search_results = &ps_ctxt->as_search_results_32x32[i];

                    for(j = 0; j < 2; j++)
                    {
                        ps_search_results->as_pred_ctxt[j].lambda = lambda_recon;
                    }
                }
                ps_search_results = &ps_ctxt->s_search_results_64x64;

                for(j = 0; j < 2; j++)
                {
                    ps_search_results->as_pred_ctxt[j].lambda = lambda_recon;
                }

                s_common_frm_prms.i4_lamda = lambda_recon;
            }
            else
            {
                lambda_recon = ps_refine_prms->lambda_recon;
            }

            /*********************************************************************/
            /* replicate the inp buffer at blk or ctb level for each ref id,     */
            /* Instead of searching with wk * ref(k), we search with Ik = I / wk */
            /* thereby avoiding a bloat up of memory. If we did all references   */
            /* weighted pred, we will end up with a duplicate copy of each ref   */
            /* at each layer, since we need to preserve the original reference.  */
            /* ToDo: Need to observe performance with this mechanism and compare */
            /* with case where ref is weighted.                                  */
            /*********************************************************************/
            fp_get_wt_inp(
                ps_curr_layer,
                &ps_ctxt->s_wt_pred,
                unit_size,
                s_common_frm_prms.i4_ctb_x_off,
                s_common_frm_prms.i4_ctb_y_off,
                unit_size,
                ps_ctxt->num_ref_future + ps_ctxt->num_ref_past,
                ps_ctxt->i4_wt_pred_enable_flag);

            if(ps_thrd_ctxt->s_init_prms.u1_is_stasino_enabled)
            {
#if TEMPORAL_NOISE_DETECT
                {
                    WORD32 had_block_size = 16;
                    WORD32 ctb_width = ((i4_pic_wd - s_common_frm_prms.i4_ctb_x_off) >= 64)
                                           ? 64
                                           : i4_pic_wd - s_common_frm_prms.i4_ctb_x_off;
                    WORD32 ctb_height = ((i4_pic_ht - s_common_frm_prms.i4_ctb_y_off) >= 64)
                                            ? 64
                                            : i4_pic_ht - s_common_frm_prms.i4_ctb_y_off;
                    WORD32 num_pred_dir = i4_num_pred_dir;
                    WORD32 i4_x_off = s_common_frm_prms.i4_ctb_x_off;
                    WORD32 i4_y_off = s_common_frm_prms.i4_ctb_y_off;

                    WORD32 i;
                    WORD32 noise_detected;
                    WORD32 ctb_size;
                    WORD32 num_comp_had_blocks;
                    WORD32 noisy_block_cnt;
                    WORD32 index_8x8_block;
                    WORD32 num_8x8_in_ctb_row;

                    WORD32 ht_offset;
                    WORD32 wd_offset;
                    WORD32 block_ht;
                    WORD32 block_wd;

                    WORD32 num_horz_blocks;
                    WORD32 num_vert_blocks;

                    WORD32 mean;
                    UWORD32 variance_8x8;

                    WORD32 hh_energy_percent;

                    /* variables to hold the constant values. The variable values held are decided by the HAD block size */
                    WORD32 min_noisy_block_cnt;
                    WORD32 min_coeffs_above_avg;
                    WORD32 min_coeff_avg_energy;

                    /* to store the mean and variance of each 8*8 block and find the variance of any higher block sizes later on. block */
                    WORD32 i4_cu_x_off, i4_cu_y_off;
                    WORD32 is_noisy;

                    /* intialise the variables holding the constants */
                    if(had_block_size == 8)
                    {
                        min_noisy_block_cnt = MIN_NOISY_BLOCKS_CNT_8x8;  //6;//
                        min_coeffs_above_avg = MIN_NUM_COEFFS_ABOVE_AVG_8x8;
                        min_coeff_avg_energy = MIN_COEFF_AVG_ENERGY_8x8;
                    }
                    else
                    {
                        min_noisy_block_cnt = MIN_NOISY_BLOCKS_CNT_16x16;  //7;//
                        min_coeffs_above_avg = MIN_NUM_COEFFS_ABOVE_AVG_16x16;
                        min_coeff_avg_energy = MIN_COEFF_AVG_ENERGY_16x16;
                    }

                    /* initialize the variables */
                    noise_detected = 0;
                    noisy_block_cnt = 0;
                    hh_energy_percent = 0;
                    variance_8x8 = 0;
                    block_ht = ctb_height;
                    block_wd = ctb_width;

                    mean = 0;

                    ctb_size = block_ht * block_wd;  //ctb_width * ctb_height;
                    num_comp_had_blocks = ctb_size / (had_block_size * had_block_size);

                    num_horz_blocks = block_wd / had_block_size;  //ctb_width / had_block_size;
                    num_vert_blocks = block_ht / had_block_size;  //ctb_height / had_block_size;

                    ht_offset = -had_block_size;
                    wd_offset = -had_block_size;

                    num_8x8_in_ctb_row = block_wd / 8;  // number of 8x8 in this ctb
                    for(i = 0; i < num_comp_had_blocks; i++)
                    {
                        if(i % num_horz_blocks == 0)
                        {
                            wd_offset = -had_block_size;
                            ht_offset += had_block_size;
                        }
                        wd_offset += had_block_size;

                        /* CU level offsets */
                        i4_cu_x_off = i4_x_off + (i % 4) * 16;  //+ (i % 4) * 16
                        i4_cu_y_off = i4_y_off + (i / 4) * 16;

                        /* if 50 % or more of the CU is noisy then the return value is 1 */
                        is_noisy = ihevce_determine_cu_noise_based_on_8x8Blk_data(
                            ps_ctb_noise_params->au1_is_8x8Blk_noisy,
                            (i % 4) * 16,
                            (i / 4) * 16,
                            16);

                        /* only if the CU is noisy then check the temporal noise detect call is made on the CU */
                        if(is_noisy)
                        {
                            index_8x8_block = (i / num_horz_blocks) * 2 * num_8x8_in_ctb_row +
                                              (i % num_horz_blocks) * 2;
                            noisy_block_cnt += ihevce_16x16block_temporal_noise_detect(
                                16,
                                ((i4_pic_wd - s_common_frm_prms.i4_ctb_x_off) >= 64)
                                    ? 64
                                    : i4_pic_wd - s_common_frm_prms.i4_ctb_x_off,
                                ((i4_pic_ht - s_common_frm_prms.i4_ctb_y_off) >= 64)
                                    ? 64
                                    : i4_pic_ht - s_common_frm_prms.i4_ctb_y_off,
                                ps_ctb_noise_params,
                                &s_srch_cand_init_data,
                                &s_search_prms_blk,
                                ps_ctxt,
                                num_pred_dir,
                                i4_num_act_ref_l0,
                                i4_num_act_ref_l1,
                                i4_cu_x_off,
                                i4_cu_y_off,
                                &ps_ctxt->s_wt_pred,
                                unit_size,
                                index_8x8_block,
                                num_horz_blocks,
                                /*num_8x8_in_ctb_row*/ 8,  // this should be a variable extra
                                i);
                        } /* if 16x16 is noisy */
                    } /* loop over for all 16x16*/

                    if(noisy_block_cnt >= min_noisy_block_cnt)
                    {
                        noise_detected = 1;
                    }

                    /* write back the noise presence detected for the current CTB to the structure */
                    ps_ctb_noise_params->i4_noise_present = noise_detected;
                }
#endif

#if EVERYWHERE_NOISY && USE_NOISE_TERM_IN_L0_ME
                if(ps_thrd_ctxt->s_init_prms.u1_is_stasino_enabled &&
                   ps_ctb_noise_params->i4_noise_present)
                {
                    memset(
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy,
                        1,
                        sizeof(ps_ctb_noise_params->au1_is_8x8Blk_noisy));
                }
#endif

                for(i = 0; i < 16; i++)
                {
                    au1_is_16x16Blk_noisy[i] = ihevce_determine_cu_noise_based_on_8x8Blk_data(
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy, (i % 4) * 16, (i / 4) * 16, 16);
                }

                for(i = 0; i < 4; i++)
                {
                    au1_is_32x32Blk_noisy[i] = ihevce_determine_cu_noise_based_on_8x8Blk_data(
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy, (i % 2) * 32, (i / 2) * 32, 32);
                }

                for(i = 0; i < 1; i++)
                {
                    au1_is_64x64Blk_noisy[i] = ihevce_determine_cu_noise_based_on_8x8Blk_data(
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy, 0, 0, 64);
                }

                if(ps_ctxt->s_frm_prms.bidir_enabled &&
                   (ps_ctxt->s_frm_prms.i4_temporal_layer_id <=
                    MAX_LAYER_ID_OF_B_PICS_WITHOUT_NOISE_DETECTION))
                {
                    ps_ctb_noise_params->i4_noise_present = 0;
                    memset(
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy,
                        0,
                        sizeof(ps_ctb_noise_params->au1_is_8x8Blk_noisy));
                }

#if ME_LAMBDA_DISCOUNT_WHEN_NOISY
                for(i = 0; i < 4; i++)
                {
                    S32 j;
                    S32 lambda;

                    if(au1_is_32x32Blk_noisy[i])
                    {
                        lambda = lambda_recon;
                        lambda =
                            ((float)lambda * (100.0f - ME_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);

                        ps_search_results = &ps_ctxt->as_search_results_32x32[i];

                        for(j = 0; j < 2; j++)
                        {
                            ps_search_results->as_pred_ctxt[j].lambda = lambda;
                        }
                    }
                }

                {
                    S32 j;
                    S32 lambda;

                    if(au1_is_64x64Blk_noisy[0])
                    {
                        lambda = lambda_recon;
                        lambda =
                            ((float)lambda * (100.0f - ME_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);

                        ps_search_results = &ps_ctxt->s_search_results_64x64;

                        for(j = 0; j < 2; j++)
                        {
                            ps_search_results->as_pred_ctxt[j].lambda = lambda;
                        }
                    }
                }
#endif
                if(au1_is_64x64Blk_noisy[0])
                {
                    U08 *pu1_inp = ps_curr_layer->pu1_inp + (s_common_frm_prms.i4_ctb_x_off +
                                                             (s_common_frm_prms.i4_ctb_y_off *
                                                              ps_curr_layer->i4_inp_stride));

                    hme_compute_sigmaX_and_sigmaXSquared(
                        pu1_inp,
                        ps_curr_layer->i4_inp_stride,
                        ps_ctxt->au4_4x4_src_sigmaX,
                        ps_ctxt->au4_4x4_src_sigmaXSquared,
                        4,
                        4,
                        64,
                        64,
                        1,
                        16);
                }
                else
                {
                    for(i = 0; i < 4; i++)
                    {
                        if(au1_is_32x32Blk_noisy[i])
                        {
                            U08 *pu1_inp =
                                ps_curr_layer->pu1_inp +
                                (s_common_frm_prms.i4_ctb_x_off +
                                 (s_common_frm_prms.i4_ctb_y_off * ps_curr_layer->i4_inp_stride));

                            U08 u1_cu_size = 32;
                            WORD32 i4_inp_buf_offset =
                                (((i / 2) * (u1_cu_size * ps_curr_layer->i4_inp_stride)) +
                                 ((i % 2) * u1_cu_size));

                            U16 u2_sigma_arr_start_index_of_3rd_32x32_blk_in_ctb = 128;
                            U16 u2_sigma_arr_start_index_of_2nd_32x32_blk_in_ctb = 8;
                            S32 i4_sigma_arr_offset =
                                (((i / 2) * u2_sigma_arr_start_index_of_3rd_32x32_blk_in_ctb) +
                                 ((i % 2) * u2_sigma_arr_start_index_of_2nd_32x32_blk_in_ctb));

                            hme_compute_sigmaX_and_sigmaXSquared(
                                pu1_inp + i4_inp_buf_offset,
                                ps_curr_layer->i4_inp_stride,
                                ps_ctxt->au4_4x4_src_sigmaX + i4_sigma_arr_offset,
                                ps_ctxt->au4_4x4_src_sigmaXSquared + i4_sigma_arr_offset,
                                4,
                                4,
                                32,
                                32,
                                1,
                                16);
                        }
                        else
                        {
                            S32 j;

                            U08 u1_16x16_blk_start_index_in_3rd_32x32_blk_of_ctb = 8;
                            U08 u1_16x16_blk_start_index_in_2nd_32x32_blk_of_ctb = 2;
                            S32 i4_16x16_blk_start_index_in_i_th_32x32_blk =
                                (((i / 2) * u1_16x16_blk_start_index_in_3rd_32x32_blk_of_ctb) +
                                 ((i % 2) * u1_16x16_blk_start_index_in_2nd_32x32_blk_of_ctb));

                            for(j = 0; j < 4; j++)
                            {
                                U08 u1_3rd_16x16_blk_index_in_32x32_blk = 4;
                                U08 u1_2nd_16x16_blk_index_in_32x32_blk = 1;
                                S32 i4_16x16_blk_index_in_ctb =
                                    i4_16x16_blk_start_index_in_i_th_32x32_blk +
                                    ((j % 2) * u1_2nd_16x16_blk_index_in_32x32_blk) +
                                    ((j / 2) * u1_3rd_16x16_blk_index_in_32x32_blk);

                                //S32 k = (((i / 2) * 8) + ((i % 2) * 2)) + ((j % 2) * 1) + ((j / 2) * 4);

                                if(au1_is_16x16Blk_noisy[i4_16x16_blk_index_in_ctb])
                                {
                                    U08 *pu1_inp =
                                        ps_curr_layer->pu1_inp + (s_common_frm_prms.i4_ctb_x_off +
                                                                  (s_common_frm_prms.i4_ctb_y_off *
                                                                   ps_curr_layer->i4_inp_stride));

                                    U08 u1_cu_size = 16;
                                    WORD32 i4_inp_buf_offset =
                                        (((i4_16x16_blk_index_in_ctb % 4) * u1_cu_size) +
                                         ((i4_16x16_blk_index_in_ctb / 4) *
                                          (u1_cu_size * ps_curr_layer->i4_inp_stride)));

                                    U16 u2_sigma_arr_start_index_of_3rd_16x16_blk_in_32x32_blk = 64;
                                    U16 u2_sigma_arr_start_index_of_2nd_16x16_blk_in_32x32_blk = 4;
                                    S32 i4_sigma_arr_offset =
                                        (((i4_16x16_blk_index_in_ctb % 4) *
                                          u2_sigma_arr_start_index_of_2nd_16x16_blk_in_32x32_blk) +
                                         ((i4_16x16_blk_index_in_ctb / 4) *
                                          u2_sigma_arr_start_index_of_3rd_16x16_blk_in_32x32_blk));

                                    hme_compute_sigmaX_and_sigmaXSquared(
                                        pu1_inp + i4_inp_buf_offset,
                                        ps_curr_layer->i4_inp_stride,
                                        (ps_ctxt->au4_4x4_src_sigmaX + i4_sigma_arr_offset),
                                        (ps_ctxt->au4_4x4_src_sigmaXSquared + i4_sigma_arr_offset),
                                        4,
                                        4,
                                        16,
                                        16,
                                        1,
                                        16);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                memset(au1_is_16x16Blk_noisy, 0, sizeof(au1_is_16x16Blk_noisy));

                memset(au1_is_32x32Blk_noisy, 0, sizeof(au1_is_32x32Blk_noisy));

                memset(au1_is_64x64Blk_noisy, 0, sizeof(au1_is_64x64Blk_noisy));
            }

            for(blk_id_in_ctb = 0; blk_id_in_ctb < num_blks_in_this_ctb; blk_id_in_ctb++)
            {
                S32 ref_ctr;
                U08 au1_pred_dir_searched[2];
                U08 u1_is_cu_noisy;
                ULWORD64 au8_final_src_sigmaX[17], au8_final_src_sigmaXSquared[17];

                {
                    blk_x = (i4_ctb_x << 2) +
                            (ps_ctb_bound_attrs->as_blk_attrs[blk_id_in_ctb].u1_blk_x);
                    blk_y = (i4_ctb_y << 2) +
                            (ps_ctb_bound_attrs->as_blk_attrs[blk_id_in_ctb].u1_blk_y);

                    blk_id_in_full_ctb =
                        ps_ctb_bound_attrs->as_blk_attrs[blk_id_in_ctb].u1_blk_id_in_full_ctb;
                    blk_8x8_mask = ps_ctb_bound_attrs->as_blk_attrs[blk_id_in_ctb].u1_blk_8x8_mask;
                    ai4_blk_8x8_mask[blk_id_in_full_ctb] = blk_8x8_mask;
                    s_search_prms_blk.i4_cu_x_off = (blk_x << blk_size_shift) - (i4_ctb_x << 6);
                    s_search_prms_blk.i4_cu_y_off = (blk_y << blk_size_shift) - (i4_ctb_y << 6);
                }

                /* get the current input blk point */
                pos_x = blk_x << blk_size_shift;
                pos_y = blk_y << blk_size_shift;
                pu1_inp = ps_curr_layer->pu1_inp + pos_x + (pos_y * i4_inp_stride);

                /*********************************************************************/
                /* For every blk in the picture, the search range needs to be derived*/
                /* Any blk can have any mv, but practical search constraints are     */
                /* imposed by the picture boundary and amt of padding.               */
                /*********************************************************************/
                /* MV limit is different based on ref. PIC */
                for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                {
                    if(!s_search_prms_blk.i4_use_rec)
                    {
                        hme_derive_search_range(
                            &as_range_prms_inp[ref_ctr],
                            &s_pic_limit_inp,
                            &as_mv_limit[ref_ctr],
                            pos_x,
                            pos_y,
                            blk_wd,
                            blk_ht);
                    }
                    else
                    {
                        hme_derive_search_range(
                            &as_range_prms_rec[ref_ctr],
                            &s_pic_limit_rec,
                            &as_mv_limit[ref_ctr],
                            pos_x,
                            pos_y,
                            blk_wd,
                            blk_ht);
                    }
                }
                s_search_prms_blk.i4_x_off = blk_x << blk_size_shift;
                s_search_prms_blk.i4_y_off = blk_y << blk_size_shift;
                /* Select search results from a suitable search result in the context */
                {
                    ps_search_results = &ps_ctxt->as_search_results_16x16[blk_id_in_full_ctb];

                    if(ps_refine_prms->u1_use_lambda_derived_from_min_8x8_act_in_ctb)
                    {
                        S32 i;

                        for(i = 0; i < 2; i++)
                        {
                            ps_search_results->as_pred_ctxt[i].lambda = lambda_recon;
                        }
                    }
                }

                u1_is_cu_noisy = au1_is_16x16Blk_noisy
                    [(s_search_prms_blk.i4_cu_x_off >> 4) + (s_search_prms_blk.i4_cu_y_off >> 2)];

                s_subpel_prms.u1_is_cu_noisy = u1_is_cu_noisy;

#if ME_LAMBDA_DISCOUNT_WHEN_NOISY
                if(u1_is_cu_noisy)
                {
                    S32 j;
                    S32 lambda;

                    lambda = lambda_recon;
                    lambda = ((float)lambda * (100.0f - ME_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);

                    for(j = 0; j < 2; j++)
                    {
                        ps_search_results->as_pred_ctxt[j].lambda = lambda;
                    }
                }
                else
                {
                    S32 j;
                    S32 lambda;

                    lambda = lambda_recon;

                    for(j = 0; j < 2; j++)
                    {
                        ps_search_results->as_pred_ctxt[j].lambda = lambda;
                    }
                }
#endif

                s_search_prms_blk.ps_search_results = ps_search_results;

                s_search_prms_blk.i4_part_mask = hme_part_mask_populator(
                    pu1_inp,
                    i4_inp_stride,
                    ps_refine_prms->limit_active_partitions,
                    ps_ctxt->ps_hme_frm_prms->bidir_enabled,
                    ps_ctxt->u1_is_curFrame_a_refFrame,
                    blk_8x8_mask,
                    e_me_quality_presets);

                if(ME_PRISTINE_QUALITY == e_me_quality_presets)
                {
                    ps_ctb_cluster_info->ai4_part_mask[blk_id_in_full_ctb] =
                        s_search_prms_blk.i4_part_mask;
                }

                /* RESET ALL SEARCH RESULTS FOR THE NEW BLK */
                {
                    /* Setting u1_num_active_refs to 2 */
                    /* for the sole purpose of the */
                    /* function called below */
                    ps_search_results->u1_num_active_ref = (ps_refine_prms->bidir_enabled) ? 2 : 1;

                    hme_reset_search_results(
                        ps_search_results, s_search_prms_blk.i4_part_mask, MV_RES_FPEL);

                    ps_search_results->u1_num_active_ref = i4_num_pred_dir;
                }

                if(0 == blk_id_in_ctb)
                {
                    UWORD8 u1_ctr;
                    for(u1_ctr = 0; u1_ctr < (ps_ctxt->s_frm_prms.u1_num_active_ref_l0 +
                                              ps_ctxt->s_frm_prms.u1_num_active_ref_l1);
                        u1_ctr++)
                    {
                        WORD32 i4_max_dep_ctb_y;
                        WORD32 i4_max_dep_ctb_x;

                        /* Set max mv in ctb units */
                        i4_max_mv_x_in_ctb =
                            (ps_curr_layer->i2_max_mv_x + ((1 << ps_ctxt->log_ctb_size) - 1)) >>
                            ps_ctxt->log_ctb_size;

                        i4_max_mv_y_in_ctb =
                            (as_mv_limit[u1_ctr].i2_max_y + ((1 << ps_ctxt->log_ctb_size) - 1)) >>
                            ps_ctxt->log_ctb_size;
                        /********************************************************************/
                        /* Set max ctb_x and ctb_y dependency on reference picture          */
                        /* Note +1 is due to delayed deblock, SAO, subpel plan dependency   */
                        /********************************************************************/
                        i4_max_dep_ctb_x = CLIP3(
                            (i4_ctb_x + i4_max_mv_x_in_ctb + 1),
                            0,
                            ps_frm_ctb_prms->i4_num_ctbs_horz - 1);
                        i4_max_dep_ctb_y = CLIP3(
                            (i4_ctb_y + i4_max_mv_y_in_ctb + 1),
                            0,
                            ps_frm_ctb_prms->i4_num_ctbs_vert - 1);

                        ihevce_dmgr_map_chk_sync(
                            ps_curr_layer->ppv_dep_mngr_recon[u1_ctr],
                            ps_ctxt->thrd_id,
                            i4_ctb_x,
                            i4_ctb_y,
                            i4_max_mv_x_in_ctb,
                            i4_max_mv_y_in_ctb);
                    }
                }

                /* Loop across different Ref IDx */
                for(u1_pred_dir_ctr = 0; u1_pred_dir_ctr < i4_num_pred_dir; u1_pred_dir_ctr++)
                {
                    S32 resultid;
                    S08 u1_default_ref_id;
                    S32 i4_num_srch_cands = 0;
                    S32 i4_num_refinement_iterations;
                    S32 i4_refine_iter_ctr;

                    if((i4_num_pred_dir == 2) || (!ps_ctxt->s_frm_prms.bidir_enabled) ||
                       (ps_ctxt->s_frm_prms.u1_num_active_ref_l1 == 0))
                    {
                        u1_pred_dir = u1_pred_dir_ctr;
                    }
                    else if(ps_ctxt->s_frm_prms.u1_num_active_ref_l0 == 0)
                    {
                        u1_pred_dir = 1;
                    }

                    u1_default_ref_id = (u1_pred_dir == 0) ? ps_ctxt->ai1_past_list[0]
                                                           : ps_ctxt->ai1_future_list[0];
                    au1_pred_dir_searched[u1_pred_dir_ctr] = u1_pred_dir;

                    i4_num_srch_cands = 0;
                    resultid = 0;

                    /* START OF NEW CTB MEANS FILL UP NEOGHBOURS IN 18x18 GRID */
                    if(0 == blk_id_in_ctb)
                    {
                        /*****************************************************************/
                        /* Initialize the mv grid with results of neighbours for the next*/
                        /* ctb.                                                          */
                        /*****************************************************************/
                        hme_fill_ctb_neighbour_mvs(
                            ps_curr_layer,
                            blk_x,
                            blk_y,
                            aps_mv_grid[u1_pred_dir],
                            u1_pred_dir_ctr,
                            u1_default_ref_id,
                            ps_ctxt->s_frm_prms.u1_num_active_ref_l0);
                    }

                    s_search_prms_blk.i1_ref_idx = u1_pred_dir;

                    {
                        if((blk_id_in_full_ctb % 4) == 0)
                        {
                            ps_ctxt->as_search_results_32x32[blk_id_in_full_ctb >> 2]
                                .as_pred_ctxt[u1_pred_dir]
                                .proj_used = (blk_id_in_full_ctb == 8) ? 0 : 1;
                        }

                        if(blk_id_in_full_ctb == 0)
                        {
                            ps_ctxt->s_search_results_64x64.as_pred_ctxt[u1_pred_dir].proj_used = 1;
                        }

                        ps_search_results->as_pred_ctxt[u1_pred_dir].proj_used =
                            !gau1_encode_to_raster_y[blk_id_in_full_ctb];
                    }

                    {
                        S32 x = gau1_encode_to_raster_x[blk_id_in_full_ctb];
                        S32 y = gau1_encode_to_raster_y[blk_id_in_full_ctb];
                        U08 u1_is_blk_at_ctb_boundary = !y;

                        s_srch_cand_init_data.u1_is_left_available =
                            !(left_ctb_in_diff_tile && !s_search_prms_blk.i4_cu_x_off);

                        if(u1_is_blk_at_ctb_boundary)
                        {
                            s_srch_cand_init_data.u1_is_topRight_available = 0;
                            s_srch_cand_init_data.u1_is_topLeft_available = 0;
                            s_srch_cand_init_data.u1_is_top_available = 0;
                        }
                        else
                        {
                            s_srch_cand_init_data.u1_is_topRight_available =
                                gau1_cu_tr_valid[y][x] && ((pos_x + blk_wd) < i4_pic_wd);
                            s_srch_cand_init_data.u1_is_top_available = 1;
                            s_srch_cand_init_data.u1_is_topLeft_available =
                                s_srch_cand_init_data.u1_is_left_available;
                        }
                    }

                    s_srch_cand_init_data.i1_default_ref_id = u1_default_ref_id;
                    s_srch_cand_init_data.i1_alt_default_ref_id = ps_ctxt->ai1_past_list[1];
                    s_srch_cand_init_data.i4_pos_x = pos_x;
                    s_srch_cand_init_data.i4_pos_y = pos_y;
                    s_srch_cand_init_data.u1_pred_dir = u1_pred_dir;
                    s_srch_cand_init_data.u1_pred_dir_ctr = u1_pred_dir_ctr;
                    s_srch_cand_init_data.u1_search_candidate_list_index =
                        au1_search_candidate_list_index[u1_pred_dir];

                    i4_num_srch_cands = hme_populate_search_candidates(&s_srch_cand_init_data);

                    /* Note this block also clips the MV range for all candidates */
                    {
                        S08 i1_check_for_mult_refs;

                        i1_check_for_mult_refs = u1_pred_dir ? (ps_ctxt->num_ref_future > 1)
                                                             : (ps_ctxt->num_ref_past > 1);

                        ps_me_optimised_function_list->pf_mv_clipper(
                            &s_search_prms_blk,
                            i4_num_srch_cands,
                            i1_check_for_mult_refs,
                            ps_refine_prms->i4_num_steps_fpel_refine,
                            ps_refine_prms->i4_num_steps_hpel_refine,
                            ps_refine_prms->i4_num_steps_qpel_refine);
                    }

#if ENABLE_EXPLICIT_SEARCH_IN_P_IN_L0
                    i4_num_refinement_iterations =
                        ((!ps_ctxt->s_frm_prms.bidir_enabled) && (i4_num_act_ref_l0 > 1))
                            ? ((e_me_quality_presets == ME_HIGH_QUALITY) ? 2 : i4_num_act_ref_l0)
                            : 1;
#else
                    i4_num_refinement_iterations =
                        ((!ps_ctxt->s_frm_prms.bidir_enabled) && (i4_num_act_ref_l0 > 1)) ? 2 : 1;
#endif

#if ENABLE_EXPLICIT_SEARCH_IN_PQ
                    if(e_me_quality_presets == ME_PRISTINE_QUALITY)
                    {
                        i4_num_refinement_iterations = (u1_pred_dir == 0) ? i4_num_act_ref_l0
                                                                          : i4_num_act_ref_l1;
                    }
#endif

                    for(i4_refine_iter_ctr = 0; i4_refine_iter_ctr < i4_num_refinement_iterations;
                        i4_refine_iter_ctr++)
                    {
                        S32 center_x;
                        S32 center_y;
                        S32 center_ref_idx;

                        S08 *pi1_pred_dir_to_ref_idx =
                            (u1_pred_dir == 0) ? ps_ctxt->ai1_past_list : ps_ctxt->ai1_future_list;

                        {
                            WORD32 i4_i;

                            for(i4_i = 0; i4_i < TOT_NUM_PARTS; i4_i++)
                            {
                                ps_fullpel_refine_ctxt->i2_tot_cost[0][i4_i] = MAX_SIGNED_16BIT_VAL;
                                ps_fullpel_refine_ctxt->i2_mv_cost[0][i4_i] = MAX_SIGNED_16BIT_VAL;
                                ps_fullpel_refine_ctxt->i2_stim_injected_cost[0][i4_i] =
                                    MAX_SIGNED_16BIT_VAL;
                                ps_fullpel_refine_ctxt->i2_mv_x[0][i4_i] = 0;
                                ps_fullpel_refine_ctxt->i2_mv_y[0][i4_i] = 0;
                                ps_fullpel_refine_ctxt->i2_ref_idx[0][i4_i] = u1_default_ref_id;

                                if(ps_refine_prms->i4_num_results_per_part == 2)
                                {
                                    ps_fullpel_refine_ctxt->i2_tot_cost[1][i4_i] =
                                        MAX_SIGNED_16BIT_VAL;
                                    ps_fullpel_refine_ctxt->i2_mv_cost[1][i4_i] =
                                        MAX_SIGNED_16BIT_VAL;
                                    ps_fullpel_refine_ctxt->i2_stim_injected_cost[1][i4_i] =
                                        MAX_SIGNED_16BIT_VAL;
                                    ps_fullpel_refine_ctxt->i2_mv_x[1][i4_i] = 0;
                                    ps_fullpel_refine_ctxt->i2_mv_y[1][i4_i] = 0;
                                    ps_fullpel_refine_ctxt->i2_ref_idx[1][i4_i] = u1_default_ref_id;
                                }
                            }

                            s_search_prms_blk.ps_fullpel_refine_ctxt = ps_fullpel_refine_ctxt;
                            s_subpel_prms.ps_subpel_refine_ctxt = ps_fullpel_refine_ctxt;
                        }

                        {
                            search_node_t *ps_coloc_node;

                            S32 i = 0;

                            if(i4_num_refinement_iterations > 1)
                            {
                                for(i = 0; i < ai4_num_coloc_cands[u1_pred_dir]; i++)
                                {
                                    ps_coloc_node =
                                        s_search_prms_blk.ps_search_candts[ai4_id_coloc[i]]
                                            .ps_search_node;

                                    if(pi1_pred_dir_to_ref_idx[i4_refine_iter_ctr] ==
                                       ps_coloc_node->i1_ref_idx)
                                    {
                                        break;
                                    }
                                }

                                if(i == ai4_num_coloc_cands[u1_pred_dir])
                                {
                                    i = 0;
                                }
                            }
                            else
                            {
                                ps_coloc_node = s_search_prms_blk.ps_search_candts[ai4_id_coloc[0]]
                                                    .ps_search_node;
                            }

                            hme_set_mvp_node(
                                ps_search_results,
                                ps_coloc_node,
                                u1_pred_dir,
                                (i4_num_refinement_iterations > 1)
                                    ? pi1_pred_dir_to_ref_idx[i4_refine_iter_ctr]
                                    : u1_default_ref_id);

                            center_x = ps_coloc_node->ps_mv->i2_mvx;
                            center_y = ps_coloc_node->ps_mv->i2_mvy;
                            center_ref_idx = ps_coloc_node->i1_ref_idx;
                        }

                        /* Full-Pel search */
                        {
                            S32 num_unique_nodes;

                            memset(au4_unique_node_map, 0, sizeof(au4_unique_node_map));

                            num_unique_nodes = hme_remove_duplicate_fpel_search_candidates(
                                as_unique_search_nodes,
                                s_search_prms_blk.ps_search_candts,
                                au4_unique_node_map,
                                pi1_pred_dir_to_ref_idx,
                                i4_num_srch_cands,
                                s_search_prms_blk.i4_num_init_candts,
                                i4_refine_iter_ctr,
                                i4_num_refinement_iterations,
                                i4_num_act_ref_l0,
                                center_ref_idx,
                                center_x,
                                center_y,
                                ps_ctxt->s_frm_prms.bidir_enabled,
                                e_me_quality_presets);

                            /*************************************************************************/
                            /* This array stores the ids of the partitions whose                     */
                            /* SADs are updated. Since the partitions whose SADs are updated may not */
                            /* be in contiguous order, we supply another level of indirection.       */
                            /*************************************************************************/
                            ps_fullpel_refine_ctxt->i4_num_valid_parts = hme_create_valid_part_ids(
                                s_search_prms_blk.i4_part_mask,
                                &ps_fullpel_refine_ctxt->ai4_part_id[0]);

                            if(!i4_refine_iter_ctr && !u1_pred_dir_ctr && u1_is_cu_noisy)
                            {
                                S32 i;
                                /*i4_sigma_array_offset : takes care of pointing to the appropriate 4x4 block's sigmaX and sigmaX-squared value in a CTB out of 256 values*/
                                S32 i4_sigma_array_offset = (s_search_prms_blk.i4_cu_x_off / 4) +
                                                            (s_search_prms_blk.i4_cu_y_off * 4);

                                for(i = 0; i < ps_fullpel_refine_ctxt->i4_num_valid_parts; i++)
                                {
                                    S32 i4_part_id = ps_fullpel_refine_ctxt->ai4_part_id[i];

                                    hme_compute_final_sigma_of_pu_from_base_blocks(
                                        ps_ctxt->au4_4x4_src_sigmaX + i4_sigma_array_offset,
                                        ps_ctxt->au4_4x4_src_sigmaXSquared + i4_sigma_array_offset,
                                        au8_final_src_sigmaX,
                                        au8_final_src_sigmaXSquared,
                                        16,
                                        4,
                                        i4_part_id,
                                        16);
                                }

                                s_common_frm_prms.pu8_part_src_sigmaX = au8_final_src_sigmaX;
                                s_common_frm_prms.pu8_part_src_sigmaXSquared =
                                    au8_final_src_sigmaXSquared;

                                s_search_prms_blk.pu8_part_src_sigmaX = au8_final_src_sigmaX;
                                s_search_prms_blk.pu8_part_src_sigmaXSquared =
                                    au8_final_src_sigmaXSquared;
                            }

                            if(0 == num_unique_nodes)
                            {
                                continue;
                            }

                            if(num_unique_nodes >= 2)
                            {
                                s_search_prms_blk.ps_search_nodes = &as_unique_search_nodes[0];
                                s_search_prms_blk.i4_num_search_nodes = num_unique_nodes;
                                if(ps_ctxt->i4_pic_type != IV_P_FRAME)
                                {
                                    if(ps_ctxt->i4_temporal_layer == 1)
                                    {
                                        hme_fullpel_cand_sifter(
                                            &s_search_prms_blk,
                                            ps_curr_layer,
                                            &ps_ctxt->s_wt_pred,
                                            ALPHA_FOR_NOISE_TERM_IN_ME,
                                            u1_is_cu_noisy,
                                            ps_me_optimised_function_list);
                                    }
                                    else
                                    {
                                        hme_fullpel_cand_sifter(
                                            &s_search_prms_blk,
                                            ps_curr_layer,
                                            &ps_ctxt->s_wt_pred,
                                            ALPHA_FOR_NOISE_TERM_IN_ME,
                                            u1_is_cu_noisy,
                                            ps_me_optimised_function_list);
                                    }
                                }
                                else
                                {
                                    hme_fullpel_cand_sifter(
                                        &s_search_prms_blk,
                                        ps_curr_layer,
                                        &ps_ctxt->s_wt_pred,
                                        ALPHA_FOR_NOISE_TERM_IN_ME_P,
                                        u1_is_cu_noisy,
                                        ps_me_optimised_function_list);
                                }
                            }

                            s_search_prms_blk.ps_search_nodes = &as_unique_search_nodes[0];

                            hme_fullpel_refine(
                                ps_refine_prms,
                                &s_search_prms_blk,
                                ps_curr_layer,
                                &ps_ctxt->s_wt_pred,
                                au4_unique_node_map,
                                num_unique_nodes,
                                blk_8x8_mask,
                                center_x,
                                center_y,
                                center_ref_idx,
                                e_me_quality_presets,
                                ps_me_optimised_function_list);
                        }

                        /* Sub-Pel search */
                        {
                            hme_reset_wkg_mem(&ps_ctxt->s_buf_mgr);

                            s_subpel_prms.pu1_wkg_mem = (U08 *)hme_get_wkg_mem(
                                &ps_ctxt->s_buf_mgr,
                                INTERP_INTERMED_BUF_SIZE + INTERP_OUT_BUF_SIZE);
                            /* MV limit is different based on ref. PIC */
                            for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                            {
                                SCALE_RANGE_PRMS(
                                    as_range_prms_hpel[ref_ctr], as_range_prms_rec[ref_ctr], 1);
                                SCALE_RANGE_PRMS(
                                    as_range_prms_qpel[ref_ctr], as_range_prms_rec[ref_ctr], 2);
                            }
                            s_subpel_prms.i4_ctb_x_off = i4_ctb_x << 6;
                            s_subpel_prms.i4_ctb_y_off = i4_ctb_y << 6;

                            hme_subpel_refine_cu_hs(
                                &s_subpel_prms,
                                ps_curr_layer,
                                ps_search_results,
                                u1_pred_dir,
                                &ps_ctxt->s_wt_pred,
                                blk_8x8_mask,
                                ps_ctxt->ps_func_selector,
                                ps_cmn_utils_optimised_function_list,
                                ps_me_optimised_function_list);
                        }
                    }
                }
                /* Populate the new PU struct with the results post subpel refinement*/
                {
                    inter_cu_results_t *ps_cu_results;
                    WORD32 best_inter_cost, intra_cost, posx, posy;

                    UWORD8 intra_8x8_enabled = 0;

                    /*  cost of 16x16 cu parent  */
                    WORD32 parent_cost = MAX_32BIT_VAL;

                    /*  cost of 8x8 cu children  */
                    /*********************************************************************/
                    /* Assuming parent is not split, then we signal 1 bit for this parent*/
                    /* CU. If split, then 1 bit for parent CU + 4 bits for each child CU */
                    /* So, 4*lambda is extra for children cost.                          */
                    /*********************************************************************/
                    WORD32 child_cost = 0;

                    ps_cu_results = ps_search_results->ps_cu_results;

                    /* Initialize the pu_results pointers to the first struct in the stack array */
                    ps_pu_results = as_inter_pu_results;

                    hme_reset_wkg_mem(&ps_ctxt->s_buf_mgr);

                    hme_populate_pus(
                        ps_thrd_ctxt,
                        ps_ctxt,
                        &s_subpel_prms,
                        ps_search_results,
                        ps_cu_results,
                        ps_pu_results,
                        &(as_pu_results[0][0][0]),
                        &s_common_frm_prms,
                        &ps_ctxt->s_wt_pred,
                        ps_curr_layer,
                        au1_pred_dir_searched,
                        i4_num_pred_dir);

                    ps_cu_results->i4_inp_offset =
                        (ps_cu_results->u1_x_off) + (ps_cu_results->u1_y_off * 64);

                    hme_decide_part_types(
                        ps_cu_results,
                        ps_pu_results,
                        &s_common_frm_prms,
                        ps_ctxt,
                        ps_cmn_utils_optimised_function_list,
                        ps_me_optimised_function_list

                    );

                    /* UPDATE the MIN and MAX MVs for Dynamical Search Range for each ref. pic. */
                    /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
                    if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
                    {
                        WORD32 res_ctr;

                        for(res_ctr = 0; res_ctr < ps_cu_results->u1_num_best_results; res_ctr++)
                        {
                            WORD32 num_part = 2, part_ctr;
                            part_type_results_t *ps_best_results =
                                &ps_cu_results->ps_best_results[res_ctr];

                            if(PRT_2Nx2N == ps_best_results->u1_part_type)
                                num_part = 1;

                            for(part_ctr = 0; part_ctr < num_part; part_ctr++)
                            {
                                pu_result_t *ps_pu_results =
                                    &ps_best_results->as_pu_results[part_ctr];

                                ASSERT(PRED_L0 == ps_pu_results->pu.b2_pred_mode);

                                hme_update_dynamic_search_params(
                                    &ps_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p]
                                         .as_dyn_range_prms[ps_pu_results->pu.mv.i1_l0_ref_idx],
                                    ps_pu_results->pu.mv.s_l0_mv.i2_mvy);

                                /* Sanity Check */
                                ASSERT(
                                    ps_pu_results->pu.mv.i1_l0_ref_idx <
                                    ps_ctxt->s_frm_prms.u1_num_active_ref_l0);

                                /* No L1 for P Pic. */
                                ASSERT(PRED_L1 != ps_pu_results->pu.b2_pred_mode);
                                /* No BI for P Pic. */
                                ASSERT(PRED_BI != ps_pu_results->pu.b2_pred_mode);
                            }
                        }
                    }

                    /*****************************************************************/
                    /* INSERT INTRA RESULTS AT 16x16 LEVEL.                          */
                    /*****************************************************************/

#if DISABLE_INTRA_IN_BPICS
                    if(1 != ((ME_XTREME_SPEED_25 == e_me_quality_presets) &&
                             (ps_ctxt->s_frm_prms.i4_temporal_layer_id > TEMPORAL_LAYER_DISABLE)))
#endif
                    {
                        if(!(DISABLE_INTRA_WHEN_NOISY && s_common_frm_prms.u1_is_cu_noisy))
                        {
                            hme_insert_intra_nodes_post_bipred(
                                ps_cu_results, ps_cur_ipe_ctb, ps_ctxt->frm_qstep);
                        }
                    }

#if DISABLE_INTRA_IN_BPICS
                    if((ME_XTREME_SPEED_25 == e_me_quality_presets) &&
                       (ps_ctxt->s_frm_prms.i4_temporal_layer_id > TEMPORAL_LAYER_DISABLE))
                    {
                        intra_8x8_enabled = 0;
                    }
                    else
#endif
                    {
                        /*TRAQO intra flag updation*/
                        if(1 == ps_cu_results->ps_best_results->as_pu_results[0].pu.b1_intra_flag)
                        {
                            best_inter_cost =
                                ps_cu_results->ps_best_results->as_pu_results[1].i4_tot_cost;
                            intra_cost =
                                ps_cu_results->ps_best_results->as_pu_results[0].i4_tot_cost;
                            /*@16x16 level*/
                            posx = (ps_cu_results->ps_best_results->as_pu_results[1].pu.b4_pos_x
                                    << 2) >>
                                   4;
                            posy = (ps_cu_results->ps_best_results->as_pu_results[1].pu.b4_pos_y
                                    << 2) >>
                                   4;
                        }
                        else
                        {
                            best_inter_cost =
                                ps_cu_results->ps_best_results->as_pu_results[0].i4_tot_cost;
                            posx = (ps_cu_results->ps_best_results->as_pu_results[0].pu.b4_pos_x
                                    << 2) >>
                                   3;
                            posy = (ps_cu_results->ps_best_results->as_pu_results[0].pu.b4_pos_y
                                    << 2) >>
                                   3;
                        }

                        /* Disable intra16/32/64 flags based on split flags recommended by IPE */
                        if(ps_cur_ipe_ctb->u1_split_flag)
                        {
                            /* Id of the 32x32 block, 16x16 block in a CTB */
                            WORD32 i4_32x32_id =
                                (ps_cu_results->u1_y_off >> 5) * 2 + (ps_cu_results->u1_x_off >> 5);
                            WORD32 i4_16x16_id = ((ps_cu_results->u1_y_off >> 4) & 0x1) * 2 +
                                                 ((ps_cu_results->u1_x_off >> 4) & 0x1);

                            if(ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id].b1_split_flag)
                            {
                                if(ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id]
                                       .as_intra16_analyse[i4_16x16_id]
                                       .b1_split_flag)
                                {
                                    intra_8x8_enabled =
                                        ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id]
                                            .as_intra16_analyse[i4_16x16_id]
                                            .as_intra8_analyse[0]
                                            .b1_valid_cu;
                                    intra_8x8_enabled &=
                                        ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id]
                                            .as_intra16_analyse[i4_16x16_id]
                                            .as_intra8_analyse[1]
                                            .b1_valid_cu;
                                    intra_8x8_enabled &=
                                        ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id]
                                            .as_intra16_analyse[i4_16x16_id]
                                            .as_intra8_analyse[2]
                                            .b1_valid_cu;
                                    intra_8x8_enabled &=
                                        ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id]
                                            .as_intra16_analyse[i4_16x16_id]
                                            .as_intra8_analyse[3]
                                            .b1_valid_cu;
                                }
                            }
                        }
                    }

                    if(blk_8x8_mask == 0xf)
                    {
                        parent_cost =
                            ps_search_results->ps_cu_results->ps_best_results[0].i4_tot_cost;
                        ps_search_results->u1_split_flag = 0;
                    }
                    else
                    {
                        ps_search_results->u1_split_flag = 1;
                    }

                    ps_cu_results = &ps_ctxt->as_cu8x8_results[blk_id_in_full_ctb << 2];

                    if(s_common_frm_prms.u1_is_cu_noisy)
                    {
                        intra_8x8_enabled = 0;
                    }

                    /* Evalaute 8x8 if NxN part id is enabled */
                    if((ps_search_results->i4_part_mask & ENABLE_NxN) || intra_8x8_enabled)
                    {
                        /* Populates the PU's for the 4 8x8's in one call */
                        hme_populate_pus_8x8_cu(
                            ps_thrd_ctxt,
                            ps_ctxt,
                            &s_subpel_prms,
                            ps_search_results,
                            ps_cu_results,
                            ps_pu_results,
                            &(as_pu_results[0][0][0]),
                            &s_common_frm_prms,
                            au1_pred_dir_searched,
                            i4_num_pred_dir,
                            blk_8x8_mask);

                        /* Re-initialize the pu_results pointers to the first struct in the stack array */
                        ps_pu_results = as_inter_pu_results;

                        for(i = 0; i < 4; i++)
                        {
                            if((blk_8x8_mask & (1 << i)))
                            {
                                if(ps_cu_results->i4_part_mask)
                                {
                                    hme_decide_part_types(
                                        ps_cu_results,
                                        ps_pu_results,
                                        &s_common_frm_prms,
                                        ps_ctxt,
                                        ps_cmn_utils_optimised_function_list,
                                        ps_me_optimised_function_list

                                    );
                                }
                                /*****************************************************************/
                                /* INSERT INTRA RESULTS AT 8x8 LEVEL.                          */
                                /*****************************************************************/
#if DISABLE_INTRA_IN_BPICS
                                if(1 != ((ME_XTREME_SPEED_25 == e_me_quality_presets) &&
                                         (ps_ctxt->s_frm_prms.i4_temporal_layer_id >
                                          TEMPORAL_LAYER_DISABLE)))
#endif
                                {
                                    if(!(DISABLE_INTRA_WHEN_NOISY &&
                                         s_common_frm_prms.u1_is_cu_noisy))
                                    {
                                        hme_insert_intra_nodes_post_bipred(
                                            ps_cu_results, ps_cur_ipe_ctb, ps_ctxt->frm_qstep);
                                    }
                                }

                                child_cost += ps_cu_results->ps_best_results[0].i4_tot_cost;
                            }

                            ps_cu_results++;
                            ps_pu_results++;
                        }

                        /* Compare 16x16 vs 8x8 cost */
                        if(child_cost < parent_cost)
                        {
                            ps_search_results->best_cu_cost = child_cost;
                            ps_search_results->u1_split_flag = 1;
                        }
                    }
                }

                hme_update_mv_bank_encode(
                    ps_search_results,
                    ps_curr_layer->ps_layer_mvbank,
                    blk_x,
                    blk_y,
                    &s_mv_update_prms,
                    au1_pred_dir_searched,
                    i4_num_act_ref_l0);

                /*********************************************************************/
                /* Map the best results to an MV Grid. This is a 18x18 grid that is  */
                /* useful for doing things like predictor for cost calculation or    */
                /* also for merge calculations if need be.                           */
                /*********************************************************************/
                hme_map_mvs_to_grid(
                    &aps_mv_grid[0], ps_search_results, au1_pred_dir_searched, i4_num_pred_dir);
            }

            /* Set the CU tree nodes appropriately */
            if(e_me_quality_presets != ME_PRISTINE_QUALITY)
            {
                WORD32 i, j;

                for(i = 0; i < 16; i++)
                {
                    cur_ctb_cu_tree_t *ps_tree_node =
                        ps_ctxt->ps_cu_tree_curr_row + (i4_ctb_x * MAX_NUM_NODES_CU_TREE);
                    search_results_t *ps_results = &ps_ctxt->as_search_results_16x16[i];

                    switch(i >> 2)
                    {
                    case 0:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_tl;

                        break;
                    }
                    case 1:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_tr;

                        break;
                    }
                    case 2:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_bl;

                        break;
                    }
                    case 3:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_br;

                        break;
                    }
                    }

                    switch(i % 4)
                    {
                    case 0:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_tl;

                        break;
                    }
                    case 1:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_tr;

                        break;
                    }
                    case 2:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_bl;

                        break;
                    }
                    case 3:
                    {
                        ps_tree_node = ps_tree_node->ps_child_node_br;

                        break;
                    }
                    }

                    if(ai4_blk_8x8_mask[i] == 15)
                    {
                        if(!ps_results->u1_split_flag)
                        {
                            ps_tree_node->is_node_valid = 1;
                            NULLIFY_THE_CHILDREN_NODES(ps_tree_node);
                        }
                        else
                        {
                            ps_tree_node->is_node_valid = 0;
                            ENABLE_THE_CHILDREN_NODES(ps_tree_node);
                        }
                    }
                    else
                    {
                        cur_ctb_cu_tree_t *ps_tree_child;

                        ps_tree_node->is_node_valid = 0;

                        for(j = 0; j < 4; j++)
                        {
                            switch(j)
                            {
                            case 0:
                            {
                                ps_tree_child = ps_tree_node->ps_child_node_tl;

                                break;
                            }
                            case 1:
                            {
                                ps_tree_child = ps_tree_node->ps_child_node_tr;

                                break;
                            }
                            case 2:
                            {
                                ps_tree_child = ps_tree_node->ps_child_node_bl;

                                break;
                            }
                            case 3:
                            {
                                ps_tree_child = ps_tree_node->ps_child_node_br;

                                break;
                            }
                            }

                            ps_tree_child->is_node_valid = !!(ai4_blk_8x8_mask[i] & (1 << j));
                        }
                    }
                }
            }

            if(ME_PRISTINE_QUALITY == e_me_quality_presets)
            {
                cur_ctb_cu_tree_t *ps_tree = ps_ctb_cluster_info->ps_cu_tree_root;

                hme_analyse_mv_clustering(
                    ps_ctxt->as_search_results_16x16,
                    ps_ctxt->as_cu16x16_results,
                    ps_ctxt->as_cu8x8_results,
                    ps_ctxt->ps_ctb_cluster_info,
                    ps_ctxt->ai1_future_list,
                    ps_ctxt->ai1_past_list,
                    ps_ctxt->s_frm_prms.bidir_enabled,
                    e_me_quality_presets);

#if DISABLE_BLK_MERGE_WHEN_NOISY
                ps_tree->ps_child_node_tl->is_node_valid = !au1_is_32x32Blk_noisy[0];
                ps_tree->ps_child_node_tr->is_node_valid = !au1_is_32x32Blk_noisy[1];
                ps_tree->ps_child_node_bl->is_node_valid = !au1_is_32x32Blk_noisy[2];
                ps_tree->ps_child_node_br->is_node_valid = !au1_is_32x32Blk_noisy[3];
                ps_tree->ps_child_node_tl->u1_inter_eval_enable = !au1_is_32x32Blk_noisy[0];
                ps_tree->ps_child_node_tr->u1_inter_eval_enable = !au1_is_32x32Blk_noisy[1];
                ps_tree->ps_child_node_bl->u1_inter_eval_enable = !au1_is_32x32Blk_noisy[2];
                ps_tree->ps_child_node_br->u1_inter_eval_enable = !au1_is_32x32Blk_noisy[3];
                ps_tree->is_node_valid = !au1_is_64x64Blk_noisy[0];
                ps_tree->u1_inter_eval_enable = !au1_is_64x64Blk_noisy[0];
#endif

                en_merge_32x32 = (ps_tree->ps_child_node_tl->is_node_valid << 0) |
                                 (ps_tree->ps_child_node_tr->is_node_valid << 1) |
                                 (ps_tree->ps_child_node_bl->is_node_valid << 2) |
                                 (ps_tree->ps_child_node_br->is_node_valid << 3);

                en_merge_execution = (ps_tree->ps_child_node_tl->u1_inter_eval_enable << 0) |
                                     (ps_tree->ps_child_node_tr->u1_inter_eval_enable << 1) |
                                     (ps_tree->ps_child_node_bl->u1_inter_eval_enable << 2) |
                                     (ps_tree->ps_child_node_br->u1_inter_eval_enable << 3) |
                                     (ps_tree->u1_inter_eval_enable << 4);
            }
            else
            {
                en_merge_execution = 0x1f;

#if DISABLE_BLK_MERGE_WHEN_NOISY
                en_merge_32x32 = ((!au1_is_32x32Blk_noisy[0] << 0) & (en_merge_32x32 & 1)) |
                                 ((!au1_is_32x32Blk_noisy[1] << 1) & (en_merge_32x32 & 2)) |
                                 ((!au1_is_32x32Blk_noisy[2] << 2) & (en_merge_32x32 & 4)) |
                                 ((!au1_is_32x32Blk_noisy[3] << 3) & (en_merge_32x32 & 8));
#endif
            }

            /* Re-initialize the pu_results pointers to the first struct in the stack array */
            ps_pu_results = as_inter_pu_results;

            {
                WORD32 ref_ctr;

                s_ctb_prms.i4_ctb_x = i4_ctb_x << 6;
                s_ctb_prms.i4_ctb_y = i4_ctb_y << 6;

                /* MV limit is different based on ref. PIC */
                for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                {
                    SCALE_RANGE_PRMS(as_range_prms_hpel[ref_ctr], as_range_prms_rec[ref_ctr], 1);
                    SCALE_RANGE_PRMS(as_range_prms_qpel[ref_ctr], as_range_prms_rec[ref_ctr], 2);
                }

                e_merge_result = CU_SPLIT;
                merge_count_32x32 = 0;

                if((en_merge_32x32 & 1) && (en_merge_execution & 1))
                {
                    range_prms_t *ps_pic_limit;
                    if(s_merge_prms_32x32_tl.i4_use_rec == 1)
                    {
                        ps_pic_limit = &s_pic_limit_rec;
                    }
                    else
                    {
                        ps_pic_limit = &s_pic_limit_inp;
                    }
                    /* MV limit is different based on ref. PIC */
                    for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                    {
                        hme_derive_search_range(
                            s_merge_prms_32x32_tl.aps_mv_range[ref_ctr],
                            ps_pic_limit,
                            &as_mv_limit[ref_ctr],
                            i4_ctb_x << 6,
                            i4_ctb_y << 6,
                            32,
                            32);

                        SCALE_RANGE_PRMS_POINTERS(
                            s_merge_prms_32x32_tl.aps_mv_range[ref_ctr],
                            s_merge_prms_32x32_tl.aps_mv_range[ref_ctr],
                            2);
                    }
                    s_merge_prms_32x32_tl.i4_ctb_x_off = i4_ctb_x << 6;
                    s_merge_prms_32x32_tl.i4_ctb_y_off = i4_ctb_y << 6;
                    s_subpel_prms.u1_is_cu_noisy = au1_is_32x32Blk_noisy[0];

                    e_merge_result = hme_try_merge_high_speed(
                        ps_thrd_ctxt,
                        ps_ctxt,
                        ps_cur_ipe_ctb,
                        &s_subpel_prms,
                        &s_merge_prms_32x32_tl,
                        ps_pu_results,
                        &as_pu_results[0][0][0]);

                    if(e_merge_result == CU_MERGED)
                    {
                        inter_cu_results_t *ps_cu_results =
                            s_merge_prms_32x32_tl.ps_results_merge->ps_cu_results;

                        if(!((ps_cu_results->u1_num_best_results == 1) &&
                             (ps_cu_results->ps_best_results->as_pu_results->pu.b1_intra_flag)))
                        {
                            hme_map_mvs_to_grid(
                                &aps_mv_grid[0],
                                s_merge_prms_32x32_tl.ps_results_merge,
                                s_merge_prms_32x32_tl.au1_pred_dir_searched,
                                s_merge_prms_32x32_tl.i4_num_pred_dir_actual);
                        }

                        if(ME_PRISTINE_QUALITY != e_me_quality_presets)
                        {
                            ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                .ps_child_node_tl->is_node_valid = 1;
                            NULLIFY_THE_CHILDREN_NODES(
                                ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                    .ps_child_node_tl);
                        }

                        merge_count_32x32++;
                        e_merge_result = CU_SPLIT;
                    }
                    else if(ME_PRISTINE_QUALITY == e_me_quality_presets)
                    {
#if ENABLE_CU_TREE_CULLING
                        cur_ctb_cu_tree_t *ps_tree =
                            ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tl;

                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                        ENABLE_THE_CHILDREN_NODES(ps_tree);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif
                    }
                }
                else if((en_merge_32x32 & 1) && (!(en_merge_execution & 1)))
                {
#if ENABLE_CU_TREE_CULLING
                    cur_ctb_cu_tree_t *ps_tree =
                        ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tl;

                    ENABLE_THE_CHILDREN_NODES(ps_tree);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif

                    if(au1_is_32x32Blk_noisy[0] && DISABLE_INTRA_WHEN_NOISY)
                    {
                        ps_tree->is_node_valid = 0;
                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                    }
                }

                if((en_merge_32x32 & 2) && (en_merge_execution & 2))
                {
                    range_prms_t *ps_pic_limit;
                    if(s_merge_prms_32x32_tr.i4_use_rec == 1)
                    {
                        ps_pic_limit = &s_pic_limit_rec;
                    }
                    else
                    {
                        ps_pic_limit = &s_pic_limit_inp;
                    }
                    /* MV limit is different based on ref. PIC */
                    for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                    {
                        hme_derive_search_range(
                            s_merge_prms_32x32_tr.aps_mv_range[ref_ctr],
                            ps_pic_limit,
                            &as_mv_limit[ref_ctr],
                            (i4_ctb_x << 6) + 32,
                            i4_ctb_y << 6,
                            32,
                            32);
                        SCALE_RANGE_PRMS_POINTERS(
                            s_merge_prms_32x32_tr.aps_mv_range[ref_ctr],
                            s_merge_prms_32x32_tr.aps_mv_range[ref_ctr],
                            2);
                    }
                    s_merge_prms_32x32_tr.i4_ctb_x_off = i4_ctb_x << 6;
                    s_merge_prms_32x32_tr.i4_ctb_y_off = i4_ctb_y << 6;
                    s_subpel_prms.u1_is_cu_noisy = au1_is_32x32Blk_noisy[1];

                    e_merge_result = hme_try_merge_high_speed(
                        ps_thrd_ctxt,
                        ps_ctxt,
                        ps_cur_ipe_ctb,
                        &s_subpel_prms,
                        &s_merge_prms_32x32_tr,
                        ps_pu_results,
                        &as_pu_results[0][0][0]);

                    if(e_merge_result == CU_MERGED)
                    {
                        inter_cu_results_t *ps_cu_results =
                            s_merge_prms_32x32_tr.ps_results_merge->ps_cu_results;

                        if(!((ps_cu_results->u1_num_best_results == 1) &&
                             (ps_cu_results->ps_best_results->as_pu_results->pu.b1_intra_flag)))
                        {
                            hme_map_mvs_to_grid(
                                &aps_mv_grid[0],
                                s_merge_prms_32x32_tr.ps_results_merge,
                                s_merge_prms_32x32_tr.au1_pred_dir_searched,
                                s_merge_prms_32x32_tr.i4_num_pred_dir_actual);
                        }

                        if(ME_PRISTINE_QUALITY != e_me_quality_presets)
                        {
                            ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                .ps_child_node_tr->is_node_valid = 1;
                            NULLIFY_THE_CHILDREN_NODES(
                                ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                    .ps_child_node_tr);
                        }

                        merge_count_32x32++;
                        e_merge_result = CU_SPLIT;
                    }
                    else if(ME_PRISTINE_QUALITY == e_me_quality_presets)
                    {
#if ENABLE_CU_TREE_CULLING
                        cur_ctb_cu_tree_t *ps_tree =
                            ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tr;

                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                        ENABLE_THE_CHILDREN_NODES(ps_tree);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif
                    }
                }
                else if((en_merge_32x32 & 2) && (!(en_merge_execution & 2)))
                {
#if ENABLE_CU_TREE_CULLING
                    cur_ctb_cu_tree_t *ps_tree =
                        ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_tr;

                    ENABLE_THE_CHILDREN_NODES(ps_tree);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif

                    if(au1_is_32x32Blk_noisy[1] && DISABLE_INTRA_WHEN_NOISY)
                    {
                        ps_tree->is_node_valid = 0;
                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                    }
                }

                if((en_merge_32x32 & 4) && (en_merge_execution & 4))
                {
                    range_prms_t *ps_pic_limit;
                    if(s_merge_prms_32x32_bl.i4_use_rec == 1)
                    {
                        ps_pic_limit = &s_pic_limit_rec;
                    }
                    else
                    {
                        ps_pic_limit = &s_pic_limit_inp;
                    }
                    /* MV limit is different based on ref. PIC */
                    for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                    {
                        hme_derive_search_range(
                            s_merge_prms_32x32_bl.aps_mv_range[ref_ctr],
                            ps_pic_limit,
                            &as_mv_limit[ref_ctr],
                            i4_ctb_x << 6,
                            (i4_ctb_y << 6) + 32,
                            32,
                            32);
                        SCALE_RANGE_PRMS_POINTERS(
                            s_merge_prms_32x32_bl.aps_mv_range[ref_ctr],
                            s_merge_prms_32x32_bl.aps_mv_range[ref_ctr],
                            2);
                    }
                    s_merge_prms_32x32_bl.i4_ctb_x_off = i4_ctb_x << 6;
                    s_merge_prms_32x32_bl.i4_ctb_y_off = i4_ctb_y << 6;
                    s_subpel_prms.u1_is_cu_noisy = au1_is_32x32Blk_noisy[2];

                    e_merge_result = hme_try_merge_high_speed(
                        ps_thrd_ctxt,
                        ps_ctxt,
                        ps_cur_ipe_ctb,
                        &s_subpel_prms,
                        &s_merge_prms_32x32_bl,
                        ps_pu_results,
                        &as_pu_results[0][0][0]);

                    if(e_merge_result == CU_MERGED)
                    {
                        inter_cu_results_t *ps_cu_results =
                            s_merge_prms_32x32_bl.ps_results_merge->ps_cu_results;

                        if(!((ps_cu_results->u1_num_best_results == 1) &&
                             (ps_cu_results->ps_best_results->as_pu_results->pu.b1_intra_flag)))
                        {
                            hme_map_mvs_to_grid(
                                &aps_mv_grid[0],
                                s_merge_prms_32x32_bl.ps_results_merge,
                                s_merge_prms_32x32_bl.au1_pred_dir_searched,
                                s_merge_prms_32x32_bl.i4_num_pred_dir_actual);
                        }

                        if(ME_PRISTINE_QUALITY != e_me_quality_presets)
                        {
                            ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                .ps_child_node_bl->is_node_valid = 1;
                            NULLIFY_THE_CHILDREN_NODES(
                                ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                    .ps_child_node_bl);
                        }

                        merge_count_32x32++;
                        e_merge_result = CU_SPLIT;
                    }
                    else if(ME_PRISTINE_QUALITY == e_me_quality_presets)
                    {
#if ENABLE_CU_TREE_CULLING
                        cur_ctb_cu_tree_t *ps_tree =
                            ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_bl;

                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                        ENABLE_THE_CHILDREN_NODES(ps_tree);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif
                    }
                }
                else if((en_merge_32x32 & 4) && (!(en_merge_execution & 4)))
                {
#if ENABLE_CU_TREE_CULLING
                    cur_ctb_cu_tree_t *ps_tree =
                        ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_bl;

                    ENABLE_THE_CHILDREN_NODES(ps_tree);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif

                    if(au1_is_32x32Blk_noisy[2] && DISABLE_INTRA_WHEN_NOISY)
                    {
                        ps_tree->is_node_valid = 0;
                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                    }
                }

                if((en_merge_32x32 & 8) && (en_merge_execution & 8))
                {
                    range_prms_t *ps_pic_limit;
                    if(s_merge_prms_32x32_br.i4_use_rec == 1)
                    {
                        ps_pic_limit = &s_pic_limit_rec;
                    }
                    else
                    {
                        ps_pic_limit = &s_pic_limit_inp;
                    }
                    /* MV limit is different based on ref. PIC */
                    for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                    {
                        hme_derive_search_range(
                            s_merge_prms_32x32_br.aps_mv_range[ref_ctr],
                            ps_pic_limit,
                            &as_mv_limit[ref_ctr],
                            (i4_ctb_x << 6) + 32,
                            (i4_ctb_y << 6) + 32,
                            32,
                            32);

                        SCALE_RANGE_PRMS_POINTERS(
                            s_merge_prms_32x32_br.aps_mv_range[ref_ctr],
                            s_merge_prms_32x32_br.aps_mv_range[ref_ctr],
                            2);
                    }
                    s_merge_prms_32x32_br.i4_ctb_x_off = i4_ctb_x << 6;
                    s_merge_prms_32x32_br.i4_ctb_y_off = i4_ctb_y << 6;
                    s_subpel_prms.u1_is_cu_noisy = au1_is_32x32Blk_noisy[3];

                    e_merge_result = hme_try_merge_high_speed(
                        ps_thrd_ctxt,
                        ps_ctxt,
                        ps_cur_ipe_ctb,
                        &s_subpel_prms,
                        &s_merge_prms_32x32_br,
                        ps_pu_results,
                        &as_pu_results[0][0][0]);

                    if(e_merge_result == CU_MERGED)
                    {
                        /*inter_cu_results_t *ps_cu_results = s_merge_prms_32x32_br.ps_results_merge->ps_cu_results;

                        if(!((ps_cu_results->u1_num_best_results == 1) &&
                        (ps_cu_results->ps_best_results->as_pu_results->pu.b1_intra_flag)))
                        {
                        hme_map_mvs_to_grid
                        (
                        &aps_mv_grid[0],
                        s_merge_prms_32x32_br.ps_results_merge,
                        s_merge_prms_32x32_br.au1_pred_dir_searched,
                        s_merge_prms_32x32_br.i4_num_pred_dir_actual
                        );
                        }*/

                        if(ME_PRISTINE_QUALITY != e_me_quality_presets)
                        {
                            ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                .ps_child_node_br->is_node_valid = 1;
                            NULLIFY_THE_CHILDREN_NODES(
                                ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                    .ps_child_node_br);
                        }

                        merge_count_32x32++;
                        e_merge_result = CU_SPLIT;
                    }
                    else if(ME_PRISTINE_QUALITY == e_me_quality_presets)
                    {
#if ENABLE_CU_TREE_CULLING
                        cur_ctb_cu_tree_t *ps_tree =
                            ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_br;

                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                        ENABLE_THE_CHILDREN_NODES(ps_tree);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                        ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif
                    }
                }
                else if((en_merge_32x32 & 8) && (!(en_merge_execution & 8)))
                {
#if ENABLE_CU_TREE_CULLING
                    cur_ctb_cu_tree_t *ps_tree =
                        ps_ctb_cluster_info->ps_cu_tree_root->ps_child_node_br;

                    ENABLE_THE_CHILDREN_NODES(ps_tree);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_tr);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_bl);
                    ENABLE_THE_CHILDREN_NODES(ps_tree->ps_child_node_br);
#endif

                    if(au1_is_32x32Blk_noisy[3] && DISABLE_INTRA_WHEN_NOISY)
                    {
                        ps_tree->is_node_valid = 0;
                        ps_ctb_cluster_info->ps_cu_tree_root->is_node_valid = 0;
                        en_merge_execution = (en_merge_execution & (~(1 << 4)));
                    }
                }

                /* Try merging all 32x32 to 64x64 candts */
                if(((en_merge_32x32 & 0xf) == 0xf) &&
                   (((merge_count_32x32 == 4) && (e_me_quality_presets != ME_PRISTINE_QUALITY)) ||
                    ((en_merge_execution & 16) && (e_me_quality_presets == ME_PRISTINE_QUALITY))))
                    if((((e_me_quality_presets == ME_XTREME_SPEED_25) &&
                         !DISABLE_64X64_BLOCK_MERGE_IN_ME_IN_XS25) ||
                        (e_me_quality_presets != ME_XTREME_SPEED_25)))
                    {
                        range_prms_t *ps_pic_limit;
                        if(s_merge_prms_64x64.i4_use_rec == 1)
                        {
                            ps_pic_limit = &s_pic_limit_rec;
                        }
                        else
                        {
                            ps_pic_limit = &s_pic_limit_inp;
                        }
                        /* MV limit is different based on ref. PIC */
                        for(ref_ctr = 0; ref_ctr < num_act_ref_pics; ref_ctr++)
                        {
                            hme_derive_search_range(
                                s_merge_prms_64x64.aps_mv_range[ref_ctr],
                                ps_pic_limit,
                                &as_mv_limit[ref_ctr],
                                i4_ctb_x << 6,
                                i4_ctb_y << 6,
                                64,
                                64);

                            SCALE_RANGE_PRMS_POINTERS(
                                s_merge_prms_64x64.aps_mv_range[ref_ctr],
                                s_merge_prms_64x64.aps_mv_range[ref_ctr],
                                2);
                        }
                        s_merge_prms_64x64.i4_ctb_x_off = i4_ctb_x << 6;
                        s_merge_prms_64x64.i4_ctb_y_off = i4_ctb_y << 6;
                        s_subpel_prms.u1_is_cu_noisy = au1_is_64x64Blk_noisy[0];

                        e_merge_result = hme_try_merge_high_speed(
                            ps_thrd_ctxt,
                            ps_ctxt,
                            ps_cur_ipe_ctb,
                            &s_subpel_prms,
                            &s_merge_prms_64x64,
                            ps_pu_results,
                            &as_pu_results[0][0][0]);

                        if((e_merge_result == CU_MERGED) &&
                           (ME_PRISTINE_QUALITY != e_me_quality_presets))
                        {
                            ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                .is_node_valid = 1;
                            NULLIFY_THE_CHILDREN_NODES(
                                ps_ctxt->ps_cu_tree_curr_row + (i4_ctb_x * MAX_NUM_NODES_CU_TREE));
                        }
                        else if(
                            (e_merge_result == CU_SPLIT) &&
                            (ME_PRISTINE_QUALITY == e_me_quality_presets))
                        {
                            ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]
                                .is_node_valid = 0;
                        }
                    }

                /*****************************************************************/
                /* UPDATION OF RESULT TO EXTERNAL STRUCTURES                     */
                /*****************************************************************/
                pf_ext_update_fxn((void *)ps_thrd_ctxt, (void *)ps_ctxt, i4_ctb_x, i4_ctb_y);

                {
#ifdef _DEBUG
                    S32 wd = ((i4_pic_wd - s_common_frm_prms.i4_ctb_x_off) >= 64)
                                 ? 64
                                 : i4_pic_wd - s_common_frm_prms.i4_ctb_x_off;
                    S32 ht = ((i4_pic_ht - s_common_frm_prms.i4_ctb_y_off) >= 64)
                                 ? 64
                                 : i4_pic_ht - s_common_frm_prms.i4_ctb_y_off;
                    ASSERT(
                        (wd * ht) ==
                        ihevce_compute_area_of_valid_cus_in_ctb(
                            &ps_ctxt->ps_cu_tree_curr_row[(i4_ctb_x * MAX_NUM_NODES_CU_TREE)]));
#endif
                }
            }

            /* set the dependency for the corresponding row in enc loop */
            ihevce_dmgr_set_row_row_sync(
                pv_dep_mngr_encloop_dep_me,
                (i4_ctb_x + 1),
                i4_ctb_y,
                tile_col_idx /* Col Tile No. */);

            left_ctb_in_diff_tile = 0;
        }
    }
}

/**
********************************************************************************
*  @fn   void hme_refine_no_encode(coarse_me_ctxt_t *ps_ctxt,
*                       refine_layer_prms_t *ps_refine_prms)
*
*  @brief  Top level entry point for refinement ME
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_refine_prms : refinement layer prms
*
*  @return None
********************************************************************************
*/
void hme_refine_no_encode(
    coarse_me_ctxt_t *ps_ctxt,
    refine_prms_t *ps_refine_prms,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    S32 lyr_job_type,
    WORD32 i4_ping_pong,
    void **ppv_dep_mngr_hme_sync)
{
    BLK_SIZE_T e_search_blk_size, e_result_blk_size;
    ME_QUALITY_PRESETS_T e_me_quality_presets =
        ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets;

    /*************************************************************************/
    /* Complexity of search: Low to High                                     */
    /*************************************************************************/
    SEARCH_COMPLEXITY_T e_search_complexity;

    /*************************************************************************/
    /* Config parameter structures for varius ME submodules                  */
    /*************************************************************************/
    hme_search_prms_t s_search_prms_blk;
    mvbank_update_prms_t s_mv_update_prms;

    /*************************************************************************/
    /* All types of search candidates for predictor based search.            */
    /*************************************************************************/
    S32 num_init_candts = 0;
    search_candt_t *ps_search_candts, as_search_candts[MAX_INIT_CANDTS];
    search_node_t as_top_neighbours[4], as_left_neighbours[3];
    search_node_t *ps_candt_zeromv, *ps_candt_tl, *ps_candt_tr;
    search_node_t *ps_candt_l, *ps_candt_t;
    search_node_t *ps_candt_prj_br[2], *ps_candt_prj_b[2], *ps_candt_prj_r[2];
    search_node_t *ps_candt_prj_bl[2];
    search_node_t *ps_candt_prj_tr[2], *ps_candt_prj_t[2], *ps_candt_prj_tl[2];
    search_node_t *ps_candt_prj_coloc[2];

    pf_get_wt_inp fp_get_wt_inp;

    search_node_t as_unique_search_nodes[MAX_INIT_CANDTS * 9];
    U32 au4_unique_node_map[MAP_X_MAX * 2];

    /*EIID */
    WORD32 i4_num_inter_wins = 0;  //debug code to find stat of
    WORD32 i4_num_comparisions = 0;  //debug code
    WORD32 i4_threshold_multiplier;
    WORD32 i4_threshold_divider;
    WORD32 i4_temporal_layer =
        ps_multi_thrd_ctxt->aps_curr_inp_pre_enc[i4_ping_pong]->s_lap_out.i4_temporal_lyr_id;

    /*************************************************************************/
    /* points ot the search results for the blk level search (8x8/16x16)     */
    /*************************************************************************/
    search_results_t *ps_search_results;

    /*************************************************************************/
    /* Coordinates                                                           */
    /*************************************************************************/
    S32 blk_x, i4_ctb_x, blk_id_in_ctb;
    //S32 i4_ctb_y;
    S32 pos_x, pos_y;
    S32 blk_id_in_full_ctb;
    S32 i4_num_srch_cands;

    S32 blk_y;

    /*************************************************************************/
    /* Related to dimensions of block being searched and pic dimensions      */
    /*************************************************************************/
    S32 blk_wd, blk_ht, blk_size_shift, num_blks_in_row, num_blks_in_pic;
    S32 i4_pic_wd, i4_pic_ht, num_blks_in_this_ctb;
    S32 num_results_prev_layer;

    /*************************************************************************/
    /* Size of a basic unit for this layer. For non encode layers, we search */
    /* in block sizes of 8x8. For encode layers, though we search 16x16s the */
    /* basic unit size is the ctb size.                                      */
    /*************************************************************************/
    S32 unit_size;

    /*************************************************************************/
    /* Pointers to context in current and coarser layers                     */
    /*************************************************************************/
    layer_ctxt_t *ps_curr_layer, *ps_coarse_layer;

    /*************************************************************************/
    /* to store mv range per blk, and picture limit, allowed search range    */
    /* range prms in hpel and qpel units as well                             */
    /*************************************************************************/
    range_prms_t s_range_prms_inp, s_range_prms_rec;
    range_prms_t s_pic_limit_inp, s_pic_limit_rec, as_mv_limit[MAX_NUM_REF];
    /*************************************************************************/
    /* These variables are used to track number of references at different   */
    /* stages of ME.                                                         */
    /*************************************************************************/
    S32 i4_num_ref_fpel, i4_num_ref_before_merge;
    S32 i4_num_ref_each_dir, i, i4_num_ref_prev_layer;
    S32 lambda_inp = ps_refine_prms->lambda_inp;

    /*************************************************************************/
    /* When a layer is implicit, it means that it searches on 1 or 2 ref idx */
    /* Explicit means it searches on all active ref idx.                     */
    /*************************************************************************/
    S32 curr_layer_implicit, prev_layer_implicit;

    /*************************************************************************/
    /* Variables for loop counts                                             */
    /*************************************************************************/
    S32 id;
    S08 i1_ref_idx;

    /*************************************************************************/
    /* Input pointer and stride                                              */
    /*************************************************************************/
    U08 *pu1_inp;
    S32 i4_inp_stride;

    S32 end_of_frame;

    S32 num_sync_units_in_row;

    PF_HME_PROJECT_COLOC_CANDT_FXN pf_hme_project_coloc_candt;
    ASSERT(ps_refine_prms->i4_layer_id < ps_ctxt->num_layers - 1);

    /*************************************************************************/
    /* Pointers to current and coarse layer are needed for projection */
    /* Pointer to prev layer are needed for other candts like coloc   */
    /*************************************************************************/
    ps_curr_layer = ps_ctxt->ps_curr_descr->aps_layers[ps_refine_prms->i4_layer_id];

    ps_coarse_layer = ps_ctxt->ps_curr_descr->aps_layers[ps_refine_prms->i4_layer_id + 1];

    num_results_prev_layer = ps_coarse_layer->ps_layer_mvbank->i4_num_mvs_per_ref;

    /* Function pointer is selected based on the C vc X86 macro */

    fp_get_wt_inp = ((ihevce_me_optimised_function_list_t *)ps_ctxt->pv_me_optimised_function_list)
                        ->pf_get_wt_inp_8x8;

    i4_inp_stride = ps_curr_layer->i4_inp_stride;
    i4_pic_wd = ps_curr_layer->i4_wd;
    i4_pic_ht = ps_curr_layer->i4_ht;
    e_search_complexity = ps_refine_prms->e_search_complexity;

    end_of_frame = 0;

    /* If the previous layer is non-encode layer, then use dyadic projection */
    if(0 == ps_ctxt->u1_encode[ps_refine_prms->i4_layer_id + 1])
        pf_hme_project_coloc_candt = hme_project_coloc_candt_dyadic;
    else
        pf_hme_project_coloc_candt = hme_project_coloc_candt;

    /* This points to all the initial candts */
    ps_search_candts = &as_search_candts[0];

    {
        e_search_blk_size = BLK_8x8;
        blk_wd = blk_ht = 8;
        blk_size_shift = 3;
        s_mv_update_prms.i4_shift = 0;
        /*********************************************************************/
        /* In case we do not encode this layer, we search 8x8 with or without*/
        /* enable 4x4 SAD.                                                   */
        /*********************************************************************/
        {
            S32 i4_mask = (ENABLE_2Nx2N);

            e_result_blk_size = BLK_8x8;
            if(ps_refine_prms->i4_enable_4x4_part)
            {
                i4_mask |= (ENABLE_NxN);
                e_result_blk_size = BLK_4x4;
                s_mv_update_prms.i4_shift = 1;
            }

            s_search_prms_blk.i4_part_mask = i4_mask;
        }

        unit_size = blk_wd;
        s_search_prms_blk.i4_inp_stride = unit_size;
    }

    /* This is required to properly update the layer mv bank */
    s_mv_update_prms.e_search_blk_size = e_search_blk_size;
    s_search_prms_blk.e_blk_size = e_search_blk_size;

    /*************************************************************************/
    /* If current layer is explicit, then the number of ref frames are to    */
    /* be same as previous layer. Else it will be 2                          */
    /*************************************************************************/
    i4_num_ref_prev_layer = ps_coarse_layer->ps_layer_mvbank->i4_num_ref;
    if(ps_refine_prms->explicit_ref)
    {
        curr_layer_implicit = 0;
        i4_num_ref_fpel = i4_num_ref_prev_layer;
        /* 100578 : Using same mv cost fun. for all presets. */
        s_search_prms_blk.pf_mv_cost_compute = compute_mv_cost_refine;
    }
    else
    {
        i4_num_ref_fpel = 2;
        curr_layer_implicit = 1;
        {
            if(ME_MEDIUM_SPEED > e_me_quality_presets)
            {
                s_search_prms_blk.pf_mv_cost_compute = compute_mv_cost_implicit;
            }
            else
            {
#if USE_MODIFIED == 1
                s_search_prms_blk.pf_mv_cost_compute = compute_mv_cost_implicit_high_speed_modified;
#else
                s_search_prms_blk.pf_mv_cost_compute = compute_mv_cost_implicit_high_speed;
#endif
            }
        }
    }

    i4_num_ref_fpel = MIN(i4_num_ref_fpel, i4_num_ref_prev_layer);
    if(ps_multi_thrd_ctxt->aps_curr_inp_pre_enc[i4_ping_pong]->s_lap_out.i4_pic_type ==
           IV_IDR_FRAME ||
       ps_multi_thrd_ctxt->aps_curr_inp_pre_enc[i4_ping_pong]->s_lap_out.i4_pic_type == IV_I_FRAME)
    {
        i4_num_ref_fpel = 1;
    }
    if(i4_num_ref_prev_layer <= 2)
    {
        prev_layer_implicit = 1;
        curr_layer_implicit = 1;
        i4_num_ref_each_dir = 1;
    }
    else
    {
        /* It is assumed that we have equal number of references in each dir */
        //ASSERT(!(i4_num_ref_prev_layer & 1));
        prev_layer_implicit = 0;
        i4_num_ref_each_dir = i4_num_ref_prev_layer >> 1;
    }
    s_mv_update_prms.i4_num_ref = i4_num_ref_fpel;
    s_mv_update_prms.i4_num_active_ref_l0 = ps_ctxt->s_frm_prms.u1_num_active_ref_l0;
    s_mv_update_prms.i4_num_active_ref_l1 = ps_ctxt->s_frm_prms.u1_num_active_ref_l1;

    /* this can be kept to 1 or 2 */
    i4_num_ref_before_merge = 2;
    i4_num_ref_before_merge = MIN(i4_num_ref_before_merge, i4_num_ref_fpel);

    /* Set up place holders to hold the search nodes of each initial candt */
    for(i = 0; i < MAX_INIT_CANDTS; i++)
    {
        ps_search_candts[i].ps_search_node = &ps_ctxt->s_init_search_node[i];
        INIT_SEARCH_NODE(ps_search_candts[i].ps_search_node, 0);
    }

    /* redundant, but doing it here since it is used in pred ctxt init */
    ps_candt_zeromv = ps_search_candts[0].ps_search_node;
    for(i = 0; i < 3; i++)
    {
        search_node_t *ps_search_node;
        ps_search_node = &as_left_neighbours[i];
        INIT_SEARCH_NODE(ps_search_node, 0);
        ps_search_node = &as_top_neighbours[i];
        INIT_SEARCH_NODE(ps_search_node, 0);
    }

    INIT_SEARCH_NODE(&as_top_neighbours[3], 0);
    /* bottom left node always not available for the blk being searched */
    as_left_neighbours[2].u1_is_avail = 0;
    /*************************************************************************/
    /* Initialize all the search results structure here. We update all the   */
    /* search results to default values, and configure things like blk sizes */
    /*************************************************************************/
    if(ps_refine_prms->i4_encode == 0)
    {
        S32 pred_lx;
        search_results_t *ps_search_results;

        ps_search_results = &ps_ctxt->s_search_results_8x8;
        hme_init_search_results(
            ps_search_results,
            i4_num_ref_fpel,
            ps_refine_prms->i4_num_fpel_results,
            ps_refine_prms->i4_num_results_per_part,
            e_search_blk_size,
            0,
            0,
            &ps_ctxt->au1_is_past[0]);
        for(pred_lx = 0; pred_lx < 2; pred_lx++)
        {
            hme_init_pred_ctxt_no_encode(
                &ps_search_results->as_pred_ctxt[pred_lx],
                ps_search_results,
                &as_top_neighbours[0],
                &as_left_neighbours[0],
                &ps_candt_prj_coloc[0],
                ps_candt_zeromv,
                ps_candt_zeromv,
                pred_lx,
                lambda_inp,
                ps_refine_prms->lambda_q_shift,
                &ps_ctxt->apu1_ref_bits_tlu_lc[0],
                &ps_ctxt->ai2_ref_scf[0]);
        }
    }

    /*********************************************************************/
    /* Initialize the dyn. search range params. for each reference index */
    /* in current layer ctxt                                             */
    /*********************************************************************/
    /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
    if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
    {
        WORD32 ref_ctr;

        for(ref_ctr = 0; ref_ctr < s_mv_update_prms.i4_num_ref; ref_ctr++)
        {
            INIT_DYN_SEARCH_PRMS(
                &ps_ctxt->s_coarse_dyn_range_prms
                     .as_dyn_range_prms[ps_refine_prms->i4_layer_id][ref_ctr],
                ps_ctxt->ai4_ref_idx_to_poc_lc[ref_ctr]);
        }
    }

    /* Next set up initial candidates according to a given set of rules.   */
    /* The number of initial candidates affects the quality of ME in the   */
    /* case of motion with multiple degrees of freedom. In case of simple  */
    /* translational motion, a current and a few causal and non causal     */
    /* candts would suffice. More candidates help to cover more complex    */
    /* cases like partitions, rotation/zoom, occlusion in/out, fine motion */
    /* where multiple ref helps etc.                                       */
    /* The candidate choice also depends on the following parameters.      */
    /* e_search_complexity: SRCH_CX_LOW, SRCH_CX_MED, SRCH_CX_HIGH         */
    /* Whether we encode or not, and the type of search across reference   */
    /* i.e. the previous layer may have been explicit/implicit and curr    */
    /* layer may be explicit/implicit                                      */

    /* 0, 0, L, T, projected coloc best always presnt by default */
    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(ZERO_MV, e_me_quality_presets);
    ps_candt_zeromv = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 0;
    ps_candt_zeromv->s_mv.i2_mvx = 0;
    ps_candt_zeromv->s_mv.i2_mvy = 0;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(SPATIAL_LEFT0, e_me_quality_presets);
    ps_candt_l = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 0;

    /* Even in ME_HIGH_SPEED mode, in layer 0, blocks */
    /* not at the CTB boundary use the causal T and */
    /* not the projected T, although the candidate is */
    /* still pointed to by ps_candt_prj_t[0] */
    if(ME_MEDIUM_SPEED <= e_me_quality_presets)
    {
        /* Using Projected top to eliminate sync */
        id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
            PROJECTED_TOP0, e_me_quality_presets);
        ps_candt_prj_t[0] = ps_search_candts[id].ps_search_node;
        ps_search_candts[id].u1_num_steps_refine = 1;
    }
    else
    {
        id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
            SPATIAL_TOP0, e_me_quality_presets);
        ps_candt_t = ps_search_candts[id].ps_search_node;
        ps_search_candts[id].u1_num_steps_refine = 0;
    }

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_COLOC0, e_me_quality_presets);
    ps_candt_prj_coloc[0] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_COLOC1, e_me_quality_presets);
    ps_candt_prj_coloc[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    if(ME_MEDIUM_SPEED <= e_me_quality_presets)
    {
        id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
            PROJECTED_TOP_RIGHT0, e_me_quality_presets);
        ps_candt_prj_tr[0] = ps_search_candts[id].ps_search_node;
        ps_search_candts[id].u1_num_steps_refine = 1;

        id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
            PROJECTED_TOP_LEFT0, e_me_quality_presets);
        ps_candt_prj_tl[0] = ps_search_candts[id].ps_search_node;
        ps_search_candts[id].u1_num_steps_refine = 1;
    }
    else
    {
        id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
            SPATIAL_TOP_RIGHT0, e_me_quality_presets);
        ps_candt_tr = ps_search_candts[id].ps_search_node;
        ps_search_candts[id].u1_num_steps_refine = 0;

        id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
            SPATIAL_TOP_LEFT0, e_me_quality_presets);
        ps_candt_tl = ps_search_candts[id].ps_search_node;
        ps_search_candts[id].u1_num_steps_refine = 0;
    }

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_RIGHT0, e_me_quality_presets);
    ps_candt_prj_r[0] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_BOTTOM0, e_me_quality_presets);
    ps_candt_prj_b[0] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_BOTTOM_RIGHT0, e_me_quality_presets);
    ps_candt_prj_br[0] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_BOTTOM_LEFT0, e_me_quality_presets);
    ps_candt_prj_bl[0] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_RIGHT1, e_me_quality_presets);
    ps_candt_prj_r[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_BOTTOM1, e_me_quality_presets);
    ps_candt_prj_b[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_BOTTOM_RIGHT1, e_me_quality_presets);
    ps_candt_prj_br[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_BOTTOM_LEFT1, e_me_quality_presets);
    ps_candt_prj_bl[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(PROJECTED_TOP1, e_me_quality_presets);
    ps_candt_prj_t[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_TOP_RIGHT1, e_me_quality_presets);
    ps_candt_prj_tr[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    id = hme_decide_search_candidate_priority_in_l1_and_l2_me(
        PROJECTED_TOP_LEFT1, e_me_quality_presets);
    ps_candt_prj_tl[1] = ps_search_candts[id].ps_search_node;
    ps_search_candts[id].u1_num_steps_refine = 1;

    /*************************************************************************/
    /* Now that the candidates have been ordered, to choose the right number */
    /* of initial candidates.                                                */
    /*************************************************************************/
    if(curr_layer_implicit && !prev_layer_implicit)
    {
        if(e_search_complexity == SEARCH_CX_LOW)
            num_init_candts = 7;
        else if(e_search_complexity == SEARCH_CX_MED)
            num_init_candts = 13;
        else if(e_search_complexity == SEARCH_CX_HIGH)
            num_init_candts = 18;
        else
            ASSERT(0);
    }
    else
    {
        if(e_search_complexity == SEARCH_CX_LOW)
            num_init_candts = 5;
        else if(e_search_complexity == SEARCH_CX_MED)
            num_init_candts = 11;
        else if(e_search_complexity == SEARCH_CX_HIGH)
            num_init_candts = 16;
        else
            ASSERT(0);
    }

    if(ME_XTREME_SPEED_25 == e_me_quality_presets)
    {
        num_init_candts = NUM_INIT_SEARCH_CANDS_IN_L1_AND_L2_ME_IN_XS25;
    }

    /*************************************************************************/
    /* The following search parameters are fixed throughout the search across*/
    /* all blks. So these are configured outside processing loop             */
    /*************************************************************************/
    s_search_prms_blk.i4_num_init_candts = num_init_candts;
    s_search_prms_blk.i4_start_step = 1;
    s_search_prms_blk.i4_use_satd = 0;
    s_search_prms_blk.i4_num_steps_post_refine = ps_refine_prms->i4_num_steps_post_refine_fpel;
    /* we use recon only for encoded layers, otherwise it is not available */
    s_search_prms_blk.i4_use_rec = ps_refine_prms->i4_encode & ps_refine_prms->i4_use_rec_in_fpel;

    s_search_prms_blk.ps_search_candts = ps_search_candts;
    /* We use the same mv_range for all ref. pic. So assign to member 0 */
    if(s_search_prms_blk.i4_use_rec)
        s_search_prms_blk.aps_mv_range[0] = &s_range_prms_rec;
    else
        s_search_prms_blk.aps_mv_range[0] = &s_range_prms_inp;
    /*************************************************************************/
    /* Initialize coordinates. Meaning as follows                            */
    /* blk_x : x coordinate of the 16x16 blk, in terms of number of blks     */
    /* blk_y : same as above, y coord.                                       */
    /* num_blks_in_this_ctb : number of blks in this given ctb that starts   */
    /* at i4_ctb_x, i4_ctb_y. This may not be 16 at picture boundaries.      */
    /* i4_ctb_x, i4_ctb_y: pixel coordinate of the ctb realtive to top left  */
    /* corner of the picture. Always multiple of 64.                         */
    /* blk_id_in_ctb : encode order id of the blk in the ctb.                */
    /*************************************************************************/
    blk_y = 0;
    blk_id_in_ctb = 0;

    GET_NUM_BLKS_IN_PIC(i4_pic_wd, i4_pic_ht, blk_size_shift, num_blks_in_row, num_blks_in_pic);

    /* Get the number of sync units in a row based on encode/non enocde layer */
    num_sync_units_in_row = num_blks_in_row;

    /*************************************************************************/
    /* Picture limit on all 4 sides. This will be used to set mv limits for  */
    /* every block given its coordinate. Note thsi assumes that the min amt  */
    /* of padding to right of pic is equal to the blk size. If we go all the */
    /* way upto 64x64, then the min padding on right size of picture should  */
    /* be 64, and also on bottom side of picture.                            */
    /*************************************************************************/
    SET_PIC_LIMIT(
        s_pic_limit_inp,
        ps_curr_layer->i4_pad_x_inp,
        ps_curr_layer->i4_pad_y_inp,
        ps_curr_layer->i4_wd,
        ps_curr_layer->i4_ht,
        s_search_prms_blk.i4_num_steps_post_refine);

    SET_PIC_LIMIT(
        s_pic_limit_rec,
        ps_curr_layer->i4_pad_x_rec,
        ps_curr_layer->i4_pad_y_rec,
        ps_curr_layer->i4_wd,
        ps_curr_layer->i4_ht,
        s_search_prms_blk.i4_num_steps_post_refine);

    /*************************************************************************/
    /* set the MV limit per ref. pic.                                        */
    /*    - P pic. : Based on the config params.                             */
    /*    - B/b pic: Based on the Max/Min MV from prev. P and config. param. */
    /*************************************************************************/
    {
        WORD32 ref_ctr;
        /* Only for B/b pic. */
        if(1 == ps_ctxt->s_frm_prms.bidir_enabled)
        {
            WORD16 i2_mv_y_per_poc, i2_max_mv_y;
            WORD32 cur_poc, ref_poc, abs_poc_diff;

            cur_poc = ps_ctxt->i4_curr_poc;

            /* Get abs MAX for symmetric search */
            i2_mv_y_per_poc = MAX(
                ps_ctxt->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[ps_refine_prms->i4_layer_id],
                (ABS(ps_ctxt->s_coarse_dyn_range_prms
                         .i2_dyn_min_y_per_poc[ps_refine_prms->i4_layer_id])));

            for(ref_ctr = 0; ref_ctr < i4_num_ref_fpel; ref_ctr++)
            {
                ref_poc = ps_ctxt->ai4_ref_idx_to_poc_lc[ref_ctr];
                abs_poc_diff = ABS((cur_poc - ref_poc));
                /* Get the cur. max MV based on POC distance */
                i2_max_mv_y = i2_mv_y_per_poc * abs_poc_diff;
                i2_max_mv_y = MIN(i2_max_mv_y, ps_curr_layer->i2_max_mv_y);

                as_mv_limit[ref_ctr].i2_min_x = -ps_curr_layer->i2_max_mv_x;
                as_mv_limit[ref_ctr].i2_min_y = -i2_max_mv_y;
                as_mv_limit[ref_ctr].i2_max_x = ps_curr_layer->i2_max_mv_x;
                as_mv_limit[ref_ctr].i2_max_y = i2_max_mv_y;
            }
        }
        else
        {
            /* Set the Config. File Params for P pic. */
            for(ref_ctr = 0; ref_ctr < i4_num_ref_fpel; ref_ctr++)
            {
                as_mv_limit[ref_ctr].i2_min_x = -ps_curr_layer->i2_max_mv_x;
                as_mv_limit[ref_ctr].i2_min_y = -ps_curr_layer->i2_max_mv_y;
                as_mv_limit[ref_ctr].i2_max_x = ps_curr_layer->i2_max_mv_x;
                as_mv_limit[ref_ctr].i2_max_y = ps_curr_layer->i2_max_mv_y;
            }
        }
    }

    /* EIID: Calculate threshold based on quality preset and/or temporal layers */
    if(e_me_quality_presets == ME_MEDIUM_SPEED)
    {
        i4_threshold_multiplier = 1;
        i4_threshold_divider = 4;
    }
    else if(e_me_quality_presets == ME_HIGH_SPEED)
    {
        i4_threshold_multiplier = 1;
        i4_threshold_divider = 2;
    }
    else if((e_me_quality_presets == ME_XTREME_SPEED) || (e_me_quality_presets == ME_XTREME_SPEED_25))
    {
#if OLD_XTREME_SPEED
        /* Hard coding the temporal ID value to 1, if it is older xtreme speed */
        i4_temporal_layer = 1;
#endif
        if(i4_temporal_layer == 0)
        {
            i4_threshold_multiplier = 3;
            i4_threshold_divider = 4;
        }
        else if(i4_temporal_layer == 1)
        {
            i4_threshold_multiplier = 3;
            i4_threshold_divider = 4;
        }
        else if(i4_temporal_layer == 2)
        {
            i4_threshold_multiplier = 1;
            i4_threshold_divider = 1;
        }
        else
        {
            i4_threshold_multiplier = 5;
            i4_threshold_divider = 4;
        }
    }
    else if(e_me_quality_presets == ME_HIGH_QUALITY)
    {
        i4_threshold_multiplier = 1;
        i4_threshold_divider = 1;
    }

    /*************************************************************************/
    /*************************************************************************/
    /*************************************************************************/
    /* START OF THE CORE LOOP                                                */
    /* If Encode is 0, then we just loop over each blk                       */
    /*************************************************************************/
    /*************************************************************************/
    /*************************************************************************/
    while(0 == end_of_frame)
    {
        job_queue_t *ps_job;
        ihevce_ed_blk_t *ps_ed_blk_ctxt_curr_row;  //EIID
        WORD32 i4_ctb_row_ctr;  //counter to calculate CTB row counter. It's (row_ctr /4)
        WORD32 i4_num_ctbs_in_row = (num_blks_in_row + 3) / 4;  //calculations verified for L1 only
        //+3 to get ceil values when divided by 4
        WORD32 i4_num_4x4_blocks_in_ctb_at_l1 =
            8 * 8;  //considering CTB size 32x32 at L1. hardcoded for now
        //if there is variable for ctb size use that and this variable can be derived
        WORD32 offset_val, check_dep_pos, set_dep_pos;
        void *pv_hme_dep_mngr;
        ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_row;

        /* Get the current layer HME Dep Mngr       */
        /* Note : Use layer_id - 1 in HME layers    */

        pv_hme_dep_mngr = ppv_dep_mngr_hme_sync[ps_refine_prms->i4_layer_id - 1];

        /* Get the current row from the job queue */
        ps_job = (job_queue_t *)ihevce_pre_enc_grp_get_next_job(
            ps_multi_thrd_ctxt, lyr_job_type, 1, i4_ping_pong);

        /* If all rows are done, set the end of process flag to 1, */
        /* and the current row to -1 */
        if(NULL == ps_job)
        {
            blk_y = -1;
            end_of_frame = 1;

            continue;
        }

        if(1 == ps_ctxt->s_frm_prms.is_i_pic)
        {
            /* set the output dependency of current row */
            ihevce_pre_enc_grp_job_set_out_dep(ps_multi_thrd_ctxt, ps_job, i4_ping_pong);
            continue;
        }

        blk_y = ps_job->s_job_info.s_me_job_info.i4_vert_unit_row_no;
        blk_x = 0;
        i4_ctb_x = 0;

        /* wait for Corresponding Pre intra Job to be completed */
        if(1 == ps_refine_prms->i4_layer_id)
        {
            volatile UWORD32 i4_l1_done;
            volatile UWORD32 *pi4_l1_done;
            pi4_l1_done = (volatile UWORD32 *)&ps_multi_thrd_ctxt
                              ->aai4_l1_pre_intra_done[i4_ping_pong][blk_y >> 2];
            i4_l1_done = *pi4_l1_done;
            while(!i4_l1_done)
            {
                i4_l1_done = *pi4_l1_done;
            }
        }
        /* Set Variables for Dep. Checking and Setting */
        set_dep_pos = blk_y + 1;
        if(blk_y > 0)
        {
            offset_val = 2;
            check_dep_pos = blk_y - 1;
        }
        else
        {
            /* First row should run without waiting */
            offset_val = -1;
            check_dep_pos = 0;
        }

        /* EIID: calculate ed_blk_ctxt pointer for current row */
        /* valid for only layer-1. not varified and used for other layers */
        i4_ctb_row_ctr = blk_y / 4;
        ps_ed_blk_ctxt_curr_row =
            ps_ctxt->ps_ed_blk + (i4_ctb_row_ctr * i4_num_ctbs_in_row *
                                  i4_num_4x4_blocks_in_ctb_at_l1);  //valid for L1 only
        ps_ed_ctb_l1_row = ps_ctxt->ps_ed_ctb_l1 + (i4_ctb_row_ctr * i4_num_ctbs_in_row);

        /* if non-encode layer then i4_ctb_x will be same as blk_x */
        /* loop over all the units is a row                        */
        for(; i4_ctb_x < num_sync_units_in_row; i4_ctb_x++)
        {
            ihevce_ed_blk_t *ps_ed_blk_ctxt_curr_ctb;  //EIDD
            ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_curr;
            WORD32 i4_ctb_blk_ctr = i4_ctb_x / 4;

            /* Wait till top row block is processed   */
            /* Currently checking till top right block*/

            /* Disabled since all candidates, except for */
            /* L and C, are projected from the coarser layer, */
            /* only in ME_HIGH_SPEED mode */
            if((ME_MEDIUM_SPEED > e_me_quality_presets))
            {
                if(i4_ctb_x < (num_sync_units_in_row - 1))
                {
                    ihevce_dmgr_chk_row_row_sync(
                        pv_hme_dep_mngr,
                        i4_ctb_x,
                        offset_val,
                        check_dep_pos,
                        0, /* Col Tile No. : Not supported in PreEnc*/
                        ps_ctxt->thrd_id);
                }
            }

            {
                /* for non encoder layer only one block is processed */
                num_blks_in_this_ctb = 1;
            }

            /* EIID: derive ed_ctxt ptr for current CTB */
            ps_ed_blk_ctxt_curr_ctb =
                ps_ed_blk_ctxt_curr_row +
                (i4_ctb_blk_ctr *
                 i4_num_4x4_blocks_in_ctb_at_l1);  //currently valid for l1 layer only
            ps_ed_ctb_l1_curr = ps_ed_ctb_l1_row + i4_ctb_blk_ctr;

            /* loop over all the blocks in CTB will always be 1 */
            for(blk_id_in_ctb = 0; blk_id_in_ctb < num_blks_in_this_ctb; blk_id_in_ctb++)
            {
                {
                    /* non encode layer */
                    blk_x = i4_ctb_x;
                    blk_id_in_full_ctb = 0;
                    s_search_prms_blk.i4_cu_x_off = s_search_prms_blk.i4_cu_y_off = 0;
                }

                /* get the current input blk point */
                pos_x = blk_x << blk_size_shift;
                pos_y = blk_y << blk_size_shift;
                pu1_inp = ps_curr_layer->pu1_inp + pos_x + (pos_y * i4_inp_stride);

                /*********************************************************************/
                /* replicate the inp buffer at blk or ctb level for each ref id,     */
                /* Instead of searching with wk * ref(k), we search with Ik = I / wk */
                /* thereby avoiding a bloat up of memory. If we did all references   */
                /* weighted pred, we will end up with a duplicate copy of each ref   */
                /* at each layer, since we need to preserve the original reference.  */
                /* ToDo: Need to observe performance with this mechanism and compare */
                /* with case where ref is weighted.                                  */
                /*********************************************************************/
                if(blk_id_in_ctb == 0)
                {
                    fp_get_wt_inp(
                        ps_curr_layer,
                        &ps_ctxt->s_wt_pred,
                        unit_size,
                        pos_x,
                        pos_y,
                        unit_size,
                        ps_ctxt->num_ref_future + ps_ctxt->num_ref_past,
                        ps_ctxt->i4_wt_pred_enable_flag);
                }

                s_search_prms_blk.i4_x_off = blk_x << blk_size_shift;
                s_search_prms_blk.i4_y_off = blk_y << blk_size_shift;
                /* Select search results from a suitable search result in the context */
                {
                    ps_search_results = &ps_ctxt->s_search_results_8x8;
                }

                s_search_prms_blk.ps_search_results = ps_search_results;

                /* RESET ALL SEARCH RESULTS FOR THE NEW BLK */
                hme_reset_search_results(
                    ps_search_results, s_search_prms_blk.i4_part_mask, MV_RES_FPEL);

                /* Loop across different Ref IDx */
                for(i1_ref_idx = 0; i1_ref_idx < i4_num_ref_fpel; i1_ref_idx++)
                {
                    S32 next_blk_offset = (e_search_blk_size == BLK_16x16) ? 22 : 12;
                    S32 prev_blk_offset = 6;
                    S32 resultid;

                    /*********************************************************************/
                    /* For every blk in the picture, the search range needs to be derived*/
                    /* Any blk can have any mv, but practical search constraints are     */
                    /* imposed by the picture boundary and amt of padding.               */
                    /*********************************************************************/
                    /* MV limit is different based on ref. PIC */
                    hme_derive_search_range(
                        &s_range_prms_inp,
                        &s_pic_limit_inp,
                        &as_mv_limit[i1_ref_idx],
                        pos_x,
                        pos_y,
                        blk_wd,
                        blk_ht);
                    hme_derive_search_range(
                        &s_range_prms_rec,
                        &s_pic_limit_rec,
                        &as_mv_limit[i1_ref_idx],
                        pos_x,
                        pos_y,
                        blk_wd,
                        blk_ht);

                    s_search_prms_blk.i1_ref_idx = i1_ref_idx;
                    ps_candt_zeromv->i1_ref_idx = i1_ref_idx;

                    i4_num_srch_cands = 1;

                    if(1 != ps_refine_prms->i4_layer_id)
                    {
                        S32 x, y;
                        x = gau1_encode_to_raster_x[blk_id_in_full_ctb];
                        y = gau1_encode_to_raster_y[blk_id_in_full_ctb];

                        if(ME_MEDIUM_SPEED > e_me_quality_presets)
                        {
                            hme_get_spatial_candt(
                                ps_curr_layer,
                                e_search_blk_size,
                                blk_x,
                                blk_y,
                                i1_ref_idx,
                                &as_top_neighbours[0],
                                &as_left_neighbours[0],
                                0,
                                ((ps_refine_prms->i4_encode) ? gau1_cu_tr_valid[y][x] : 1),
                                0,
                                ps_refine_prms->i4_encode);

                            *ps_candt_tr = as_top_neighbours[3];
                            *ps_candt_t = as_top_neighbours[1];
                            *ps_candt_tl = as_top_neighbours[0];
                            i4_num_srch_cands += 3;
                        }
                        else
                        {
                            layer_mv_t *ps_layer_mvbank = ps_curr_layer->ps_layer_mvbank;
                            S32 i4_blk_size1 = gau1_blk_size_to_wd[ps_layer_mvbank->e_blk_size];
                            S32 i4_blk_size2 = gau1_blk_size_to_wd[e_search_blk_size];
                            search_node_t *ps_search_node;
                            S32 i4_offset, blk_x_temp = blk_x, blk_y_temp = blk_y;
                            hme_mv_t *ps_mv, *ps_mv_base;
                            S08 *pi1_ref_idx, *pi1_ref_idx_base;
                            S32 jump = 1, mvs_in_blk, mvs_in_row;
                            S32 shift = (ps_refine_prms->i4_encode ? 2 : 0);

                            if(i4_blk_size1 != i4_blk_size2)
                            {
                                blk_x_temp <<= 1;
                                blk_y_temp <<= 1;
                                jump = 2;
                                if((i4_blk_size1 << 2) == i4_blk_size2)
                                {
                                    blk_x_temp <<= 1;
                                    blk_y_temp <<= 1;
                                    jump = 4;
                                }
                            }

                            mvs_in_blk = ps_layer_mvbank->i4_num_mvs_per_blk;
                            mvs_in_row = ps_layer_mvbank->i4_num_mvs_per_row;

                            /* Adjust teh blk coord to point to top left locn */
                            blk_x_temp -= 1;
                            blk_y_temp -= 1;

                            /* Pick up the mvs from the location */
                            i4_offset = (blk_x_temp * ps_layer_mvbank->i4_num_mvs_per_blk);
                            i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y_temp);

                            ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
                            pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

                            ps_mv += (i1_ref_idx * ps_layer_mvbank->i4_num_mvs_per_ref);
                            pi1_ref_idx += (i1_ref_idx * ps_layer_mvbank->i4_num_mvs_per_ref);

                            ps_mv_base = ps_mv;
                            pi1_ref_idx_base = pi1_ref_idx;

                            ps_search_node = &as_left_neighbours[0];
                            ps_mv = ps_mv_base + mvs_in_row;
                            pi1_ref_idx = pi1_ref_idx_base + mvs_in_row;
                            COPY_MV_TO_SEARCH_NODE(
                                ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);

                            i4_num_srch_cands++;
                        }
                    }
                    else
                    {
                        S32 x, y;
                        x = gau1_encode_to_raster_x[blk_id_in_full_ctb];
                        y = gau1_encode_to_raster_y[blk_id_in_full_ctb];

                        if(ME_MEDIUM_SPEED > e_me_quality_presets)
                        {
                            hme_get_spatial_candt_in_l1_me(
                                ps_curr_layer,
                                e_search_blk_size,
                                blk_x,
                                blk_y,
                                i1_ref_idx,
                                !ps_search_results->pu1_is_past[i1_ref_idx],
                                &as_top_neighbours[0],
                                &as_left_neighbours[0],
                                0,
                                ((ps_refine_prms->i4_encode) ? gau1_cu_tr_valid[y][x] : 1),
                                0,
                                ps_ctxt->s_frm_prms.u1_num_active_ref_l0,
                                ps_ctxt->s_frm_prms.u1_num_active_ref_l1);

                            *ps_candt_tr = as_top_neighbours[3];
                            *ps_candt_t = as_top_neighbours[1];
                            *ps_candt_tl = as_top_neighbours[0];

                            i4_num_srch_cands += 3;
                        }
                        else
                        {
                            layer_mv_t *ps_layer_mvbank = ps_curr_layer->ps_layer_mvbank;
                            S32 i4_blk_size1 = gau1_blk_size_to_wd[ps_layer_mvbank->e_blk_size];
                            S32 i4_blk_size2 = gau1_blk_size_to_wd[e_search_blk_size];
                            S32 i4_mv_pos_in_implicit_array;
                            search_node_t *ps_search_node;
                            S32 i4_offset, blk_x_temp = blk_x, blk_y_temp = blk_y;
                            hme_mv_t *ps_mv, *ps_mv_base;
                            S08 *pi1_ref_idx, *pi1_ref_idx_base;
                            S32 jump = 1, mvs_in_blk, mvs_in_row;
                            S32 shift = (ps_refine_prms->i4_encode ? 2 : 0);
                            U08 u1_pred_dir = !ps_search_results->pu1_is_past[i1_ref_idx];
                            S32 i4_num_results_in_given_dir =
                                ((u1_pred_dir == 1) ? (ps_layer_mvbank->i4_num_mvs_per_ref *
                                                       ps_ctxt->s_frm_prms.u1_num_active_ref_l1)
                                                    : (ps_layer_mvbank->i4_num_mvs_per_ref *
                                                       ps_ctxt->s_frm_prms.u1_num_active_ref_l0));

                            if(i4_blk_size1 != i4_blk_size2)
                            {
                                blk_x_temp <<= 1;
                                blk_y_temp <<= 1;
                                jump = 2;
                                if((i4_blk_size1 << 2) == i4_blk_size2)
                                {
                                    blk_x_temp <<= 1;
                                    blk_y_temp <<= 1;
                                    jump = 4;
                                }
                            }

                            mvs_in_blk = ps_layer_mvbank->i4_num_mvs_per_blk;
                            mvs_in_row = ps_layer_mvbank->i4_num_mvs_per_row;

                            /* Adjust teh blk coord to point to top left locn */
                            blk_x_temp -= 1;
                            blk_y_temp -= 1;

                            /* Pick up the mvs from the location */
                            i4_offset = (blk_x_temp * ps_layer_mvbank->i4_num_mvs_per_blk);
                            i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y_temp);

                            i4_offset +=
                                ((u1_pred_dir == 1) ? (ps_layer_mvbank->i4_num_mvs_per_ref *
                                                       ps_ctxt->s_frm_prms.u1_num_active_ref_l0)
                                                    : 0);

                            ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
                            pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

                            ps_mv_base = ps_mv;
                            pi1_ref_idx_base = pi1_ref_idx;

                            {
                                /* ps_mv and pi1_ref_idx now point to the top left locn */
                                ps_search_node = &as_left_neighbours[0];
                                ps_mv = ps_mv_base + mvs_in_row;
                                pi1_ref_idx = pi1_ref_idx_base + mvs_in_row;

                                i4_mv_pos_in_implicit_array =
                                    hme_find_pos_of_implicitly_stored_ref_id(
                                        pi1_ref_idx, i1_ref_idx, 0, i4_num_results_in_given_dir);

                                if(-1 != i4_mv_pos_in_implicit_array)
                                {
                                    COPY_MV_TO_SEARCH_NODE(
                                        ps_search_node,
                                        &ps_mv[i4_mv_pos_in_implicit_array],
                                        &pi1_ref_idx[i4_mv_pos_in_implicit_array],
                                        i1_ref_idx,
                                        shift);
                                }
                                else
                                {
                                    ps_search_node->u1_is_avail = 0;
                                    ps_search_node->s_mv.i2_mvx = 0;
                                    ps_search_node->s_mv.i2_mvy = 0;
                                    ps_search_node->i1_ref_idx = i1_ref_idx;
                                }

                                i4_num_srch_cands++;
                            }
                        }
                    }

                    *ps_candt_l = as_left_neighbours[0];

                    /* when 16x16 is searched in an encode layer, and the prev layer */
                    /* stores results for 4x4 blks, we project 5 candts corresponding */
                    /* to (2,2), (2,14), (14,2), 14,14) and 2nd best of (2,2) */
                    /* However in other cases, only 2,2 best and 2nd best reqd */
                    resultid = 0;
                    pf_hme_project_coloc_candt(
                        ps_candt_prj_coloc[0],
                        ps_curr_layer,
                        ps_coarse_layer,
                        pos_x + 2,
                        pos_y + 2,
                        i1_ref_idx,
                        resultid);

                    i4_num_srch_cands++;

                    resultid = 1;
                    if(num_results_prev_layer > 1)
                    {
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_coloc[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x + 2,
                            pos_y + 2,
                            i1_ref_idx,
                            resultid);

                        i4_num_srch_cands++;
                    }

                    resultid = 0;

                    if(ME_MEDIUM_SPEED <= e_me_quality_presets)
                    {
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_t[0],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x,
                            pos_y - prev_blk_offset,
                            i1_ref_idx,
                            resultid);

                        i4_num_srch_cands++;
                    }

                    {
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_br[0],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x + next_blk_offset,
                            pos_y + next_blk_offset,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_bl[0],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x - prev_blk_offset,
                            pos_y + next_blk_offset,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_r[0],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x + next_blk_offset,
                            pos_y,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_b[0],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x,
                            pos_y + next_blk_offset,
                            i1_ref_idx,
                            resultid);

                        i4_num_srch_cands += 4;

                        if(ME_MEDIUM_SPEED <= e_me_quality_presets)
                        {
                            pf_hme_project_coloc_candt(
                                ps_candt_prj_tr[0],
                                ps_curr_layer,
                                ps_coarse_layer,
                                pos_x + next_blk_offset,
                                pos_y - prev_blk_offset,
                                i1_ref_idx,
                                resultid);
                            pf_hme_project_coloc_candt(
                                ps_candt_prj_tl[0],
                                ps_curr_layer,
                                ps_coarse_layer,
                                pos_x - prev_blk_offset,
                                pos_y - prev_blk_offset,
                                i1_ref_idx,
                                resultid);

                            i4_num_srch_cands += 2;
                        }
                    }
                    if((num_results_prev_layer > 1) && (e_search_complexity >= SEARCH_CX_MED))
                    {
                        resultid = 1;
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_br[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x + next_blk_offset,
                            pos_y + next_blk_offset,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_bl[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x - prev_blk_offset,
                            pos_y + next_blk_offset,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_r[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x + next_blk_offset,
                            pos_y,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_b[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x,
                            pos_y + next_blk_offset,
                            i1_ref_idx,
                            resultid);

                        i4_num_srch_cands += 4;

                        pf_hme_project_coloc_candt(
                            ps_candt_prj_tr[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x + next_blk_offset,
                            pos_y - prev_blk_offset,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_tl[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x - prev_blk_offset,
                            pos_y - prev_blk_offset,
                            i1_ref_idx,
                            resultid);
                        pf_hme_project_coloc_candt(
                            ps_candt_prj_t[1],
                            ps_curr_layer,
                            ps_coarse_layer,
                            pos_x,
                            pos_y - prev_blk_offset,
                            i1_ref_idx,
                            resultid);

                        i4_num_srch_cands += 3;
                    }

                    /* Note this block also clips the MV range for all candidates */
#ifdef _DEBUG
                    {
                        S32 candt;
                        range_prms_t *ps_range_prms;

                        S32 num_ref_valid = ps_ctxt->num_ref_future + ps_ctxt->num_ref_past;
                        for(candt = 0; candt < i4_num_srch_cands; candt++)
                        {
                            search_node_t *ps_search_node;

                            ps_search_node =
                                s_search_prms_blk.ps_search_candts[candt].ps_search_node;

                            ps_range_prms = s_search_prms_blk.aps_mv_range[0];

                            if((ps_search_node->i1_ref_idx >= num_ref_valid) ||
                               (ps_search_node->i1_ref_idx < 0))
                            {
                                ASSERT(0);
                            }
                        }
                    }
#endif

                    {
                        S32 srch_cand;
                        S32 num_unique_nodes = 0;
                        S32 num_nodes_searched = 0;
                        S32 num_best_cand = 0;
                        S08 i1_grid_enable = 0;
                        search_node_t as_best_two_proj_node[TOT_NUM_PARTS * 2];
                        /* has list of valid partition to search terminated by -1 */
                        S32 ai4_valid_part_ids[TOT_NUM_PARTS + 1];
                        S32 center_x;
                        S32 center_y;

                        /* indicates if the centre point of grid needs to be explicitly added for search */
                        S32 add_centre = 0;

                        memset(au4_unique_node_map, 0, sizeof(au4_unique_node_map));
                        center_x = ps_candt_prj_coloc[0]->s_mv.i2_mvx;
                        center_y = ps_candt_prj_coloc[0]->s_mv.i2_mvy;

                        for(srch_cand = 0;
                            (srch_cand < i4_num_srch_cands) &&
                            (num_unique_nodes <= s_search_prms_blk.i4_num_init_candts);
                            srch_cand++)
                        {
                            search_node_t s_search_node_temp =
                                s_search_prms_blk.ps_search_candts[srch_cand].ps_search_node[0];

                            s_search_node_temp.i1_ref_idx = i1_ref_idx;  //TEMP FIX;

                            /* Clip the motion vectors as well here since after clipping
                            two candidates can become same and they will be removed during deduplication */
                            CLIP_MV_WITHIN_RANGE(
                                s_search_node_temp.s_mv.i2_mvx,
                                s_search_node_temp.s_mv.i2_mvy,
                                s_search_prms_blk.aps_mv_range[0],
                                ps_refine_prms->i4_num_steps_fpel_refine,
                                ps_refine_prms->i4_num_steps_hpel_refine,
                                ps_refine_prms->i4_num_steps_qpel_refine);

                            /* PT_C */
                            INSERT_NEW_NODE(
                                as_unique_search_nodes,
                                num_unique_nodes,
                                s_search_node_temp,
                                0,
                                au4_unique_node_map,
                                center_x,
                                center_y,
                                1);

                            num_nodes_searched += 1;
                        }
                        num_unique_nodes =
                            MIN(num_unique_nodes, s_search_prms_blk.i4_num_init_candts);

                        /* If number of candidates projected/number of candidates to be refined are more than 2,
                        then filter out and choose the best two here */
                        if(num_unique_nodes >= 2)
                        {
                            S32 num_results;
                            S32 cnt;
                            S32 *pi4_valid_part_ids;
                            s_search_prms_blk.ps_search_nodes = &as_unique_search_nodes[0];
                            s_search_prms_blk.i4_num_search_nodes = num_unique_nodes;
                            pi4_valid_part_ids = &ai4_valid_part_ids[0];

                            /* pi4_valid_part_ids is updated inside */
                            hme_pred_search_no_encode(
                                &s_search_prms_blk,
                                ps_curr_layer,
                                &ps_ctxt->s_wt_pred,
                                pi4_valid_part_ids,
                                1,
                                e_me_quality_presets,
                                i1_grid_enable,
                                (ihevce_me_optimised_function_list_t *)
                                    ps_ctxt->pv_me_optimised_function_list

                            );

                            num_best_cand = 0;
                            cnt = 0;
                            num_results = ps_search_results->u1_num_results_per_part;

                            while((id = pi4_valid_part_ids[cnt++]) >= 0)
                            {
                                num_results =
                                    MIN(ps_refine_prms->pu1_num_best_results[id], num_results);

                                for(i = 0; i < num_results; i++)
                                {
                                    search_node_t s_search_node_temp;
                                    s_search_node_temp =
                                        *(ps_search_results->aps_part_results[i1_ref_idx][id] + i);
                                    if(s_search_node_temp.i1_ref_idx >= 0)
                                    {
                                        INSERT_NEW_NODE_NOMAP(
                                            as_best_two_proj_node,
                                            num_best_cand,
                                            s_search_node_temp,
                                            0);
                                    }
                                }
                            }
                        }
                        else
                        {
                            add_centre = 1;
                            num_best_cand = num_unique_nodes;
                            as_best_two_proj_node[0] = as_unique_search_nodes[0];
                        }

                        num_unique_nodes = 0;
                        num_nodes_searched = 0;

                        if(1 == num_best_cand)
                        {
                            search_node_t s_search_node_temp = as_best_two_proj_node[0];
                            S16 i2_mv_x = s_search_node_temp.s_mv.i2_mvx;
                            S16 i2_mv_y = s_search_node_temp.s_mv.i2_mvy;
                            S08 i1_ref_idx = s_search_node_temp.i1_ref_idx;

                            i1_grid_enable = 1;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x - 1;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y - 1;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y - 1;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x + 1;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y - 1;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x - 1;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x + 1;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x - 1;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y + 1;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y + 1;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x + 1;
                            as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y + 1;
                            as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;

                            if(add_centre)
                            {
                                as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvx = i2_mv_x;
                                as_unique_search_nodes[num_unique_nodes].s_mv.i2_mvy = i2_mv_y;
                                as_unique_search_nodes[num_unique_nodes++].i1_ref_idx = i1_ref_idx;
                            }
                        }
                        else
                        {
                            /* For the candidates where refinement was required, choose the best two */
                            for(srch_cand = 0; srch_cand < num_best_cand; srch_cand++)
                            {
                                search_node_t s_search_node_temp = as_best_two_proj_node[srch_cand];
                                WORD32 mv_x = s_search_node_temp.s_mv.i2_mvx;
                                WORD32 mv_y = s_search_node_temp.s_mv.i2_mvy;

                                /* Because there may not be two best unique candidates (because of clipping),
                                second best candidate can be uninitialized, ignore that */
                                if(s_search_node_temp.s_mv.i2_mvx == INTRA_MV ||
                                   s_search_node_temp.i1_ref_idx < 0)
                                {
                                    num_nodes_searched++;
                                    continue;
                                }

                                /* PT_C */
                                /* Since the center point has already be evaluated and best results are persistent,
                                it will not be evaluated again */
                                if(add_centre) /* centre point added explicitly again if search results is not updated */
                                {
                                    INSERT_NEW_NODE(
                                        as_unique_search_nodes,
                                        num_unique_nodes,
                                        s_search_node_temp,
                                        0,
                                        au4_unique_node_map,
                                        center_x,
                                        center_y,
                                        1);
                                }

                                /* PT_L */
                                s_search_node_temp.s_mv.i2_mvx = mv_x - 1;
                                s_search_node_temp.s_mv.i2_mvy = mv_y;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_T */
                                s_search_node_temp.s_mv.i2_mvx = mv_x;
                                s_search_node_temp.s_mv.i2_mvy = mv_y - 1;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_R */
                                s_search_node_temp.s_mv.i2_mvx = mv_x + 1;
                                s_search_node_temp.s_mv.i2_mvy = mv_y;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_B */
                                s_search_node_temp.s_mv.i2_mvx = mv_x;
                                s_search_node_temp.s_mv.i2_mvy = mv_y + 1;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_TL */
                                s_search_node_temp.s_mv.i2_mvx = mv_x - 1;
                                s_search_node_temp.s_mv.i2_mvy = mv_y - 1;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_TR */
                                s_search_node_temp.s_mv.i2_mvx = mv_x + 1;
                                s_search_node_temp.s_mv.i2_mvy = mv_y - 1;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_BL */
                                s_search_node_temp.s_mv.i2_mvx = mv_x - 1;
                                s_search_node_temp.s_mv.i2_mvy = mv_y + 1;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);

                                /* PT_BR */
                                s_search_node_temp.s_mv.i2_mvx = mv_x + 1;
                                s_search_node_temp.s_mv.i2_mvy = mv_y + 1;
                                INSERT_NEW_NODE(
                                    as_unique_search_nodes,
                                    num_unique_nodes,
                                    s_search_node_temp,
                                    0,
                                    au4_unique_node_map,
                                    center_x,
                                    center_y,
                                    1);
                            }
                        }

                        s_search_prms_blk.ps_search_nodes = &as_unique_search_nodes[0];
                        s_search_prms_blk.i4_num_search_nodes = num_unique_nodes;

                        /*****************************************************************/
                        /* Call the search algorithm, this includes:                     */
                        /* Pre-Search-Refinement (for coarse candts)                     */
                        /* Search on each candidate                                      */
                        /* Post Search Refinement on winners/other new candidates        */
                        /*****************************************************************/

                        hme_pred_search_no_encode(
                            &s_search_prms_blk,
                            ps_curr_layer,
                            &ps_ctxt->s_wt_pred,
                            ai4_valid_part_ids,
                            0,
                            e_me_quality_presets,
                            i1_grid_enable,
                            (ihevce_me_optimised_function_list_t *)
                                ps_ctxt->pv_me_optimised_function_list);

                        i1_grid_enable = 0;
                    }
                }

                /* for non encode layer update MV and end processing for block */
                {
                    WORD32 i4_ref_id, min_cost = 0x7fffffff, min_sad = 0;
                    search_node_t *ps_search_node;
                    /* now update the reqd results back to the layer mv bank. */
                    if(1 == ps_refine_prms->i4_layer_id)
                    {
                        hme_update_mv_bank_in_l1_me(
                            ps_search_results,
                            ps_curr_layer->ps_layer_mvbank,
                            blk_x,
                            blk_y,
                            &s_mv_update_prms);
                    }
                    else
                    {
                        hme_update_mv_bank_noencode(
                            ps_search_results,
                            ps_curr_layer->ps_layer_mvbank,
                            blk_x,
                            blk_y,
                            &s_mv_update_prms);
                    }

                    /* UPDATE the MIN and MAX MVs for Dynamical Search Range for each ref. pic. */
                    /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
                    if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
                    {
                        WORD32 i4_j;
                        layer_mv_t *ps_layer_mv = ps_curr_layer->ps_layer_mvbank;

                        //if (ps_layer_mv->e_blk_size == s_mv_update_prms.e_search_blk_size)
                        /* Not considering this for Dyn. Search Update */
                        {
                            for(i4_ref_id = 0; i4_ref_id < (S32)s_mv_update_prms.i4_num_ref;
                                i4_ref_id++)
                            {
                                ps_search_node =
                                    ps_search_results->aps_part_results[i4_ref_id][PART_ID_2Nx2N];

                                for(i4_j = 0; i4_j < ps_layer_mv->i4_num_mvs_per_ref; i4_j++)
                                {
                                    hme_update_dynamic_search_params(
                                        &ps_ctxt->s_coarse_dyn_range_prms
                                             .as_dyn_range_prms[ps_refine_prms->i4_layer_id]
                                                               [i4_ref_id],
                                        ps_search_node->s_mv.i2_mvy);

                                    ps_search_node++;
                                }
                            }
                        }
                    }

                    if(1 == ps_refine_prms->i4_layer_id)
                    {
                        WORD32 wt_pred_val, log_wt_pred_val;
                        WORD32 ref_id_of_nearest_poc = 0;
                        WORD32 max_val = 0x7fffffff;
                        WORD32 max_l0_val = 0x7fffffff;
                        WORD32 max_l1_val = 0x7fffffff;
                        WORD32 cur_val;
                        WORD32 i4_local_weighted_sad, i4_local_cost_weighted_pred;

                        WORD32 bestl0_sad = 0x7fffffff;
                        WORD32 bestl1_sad = 0x7fffffff;
                        search_node_t *ps_best_l0_blk = NULL, *ps_best_l1_blk = NULL;

                        for(i4_ref_id = 0; i4_ref_id < (S32)s_mv_update_prms.i4_num_ref;
                            i4_ref_id++)
                        {
                            wt_pred_val = ps_ctxt->s_wt_pred.a_wpred_wt[i4_ref_id];
                            log_wt_pred_val = ps_ctxt->s_wt_pred.wpred_log_wdc;

                            ps_search_node =
                                ps_search_results->aps_part_results[i4_ref_id][PART_ID_2Nx2N];

                            i4_local_weighted_sad = ((ps_search_node->i4_sad * wt_pred_val) +
                                                     ((1 << log_wt_pred_val) >> 1)) >>
                                                    log_wt_pred_val;

                            i4_local_cost_weighted_pred =
                                i4_local_weighted_sad +
                                (ps_search_node->i4_tot_cost - ps_search_node->i4_sad);
                            //the loop is redundant as the results are already sorted based on total cost
                            //for (i4_j = 0; i4_j < ps_curr_layer->ps_layer_mvbank->i4_num_mvs_per_ref; i4_j++)
                            {
                                if(i4_local_cost_weighted_pred < min_cost)
                                {
                                    min_cost = i4_local_cost_weighted_pred;
                                    min_sad = i4_local_weighted_sad;
                                }
                            }

                            /* For P frame, calculate the nearest poc which is either P or I frame*/
                            if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
                            {
                                if(-1 != ps_coarse_layer->ai4_ref_id_to_poc_lc[i4_ref_id])
                                {
                                    cur_val =
                                        ABS(ps_ctxt->i4_curr_poc -
                                            ps_coarse_layer->ai4_ref_id_to_poc_lc[i4_ref_id]);
                                    if(cur_val < max_val)
                                    {
                                        max_val = cur_val;
                                        ref_id_of_nearest_poc = i4_ref_id;
                                    }
                                }
                            }
                        }
                        /*Store me cost wrt. to past frame only for P frame  */
                        if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
                        {
                            if(-1 != ps_coarse_layer->ai4_ref_id_to_poc_lc[ref_id_of_nearest_poc])
                            {
                                WORD16 i2_mvx, i2_mvy;

                                WORD32 i4_diff_col_ctr = blk_x - (i4_ctb_blk_ctr * 4);
                                WORD32 i4_diff_row_ctr = blk_y - (i4_ctb_row_ctr * 4);
                                WORD32 z_scan_idx =
                                    gau1_raster_scan_to_ctb[i4_diff_row_ctr][i4_diff_col_ctr];
                                WORD32 wt, log_wt;

                                /*ASSERT((ps_ctxt->i4_curr_poc - ps_coarse_layer->ai4_ref_id_to_poc_lc[ref_id_of_nearest_poc])
                                <= (1 + ps_ctxt->num_b_frms));*/

                                /*obtain mvx and mvy */
                                i2_mvx =
                                    ps_search_results
                                        ->aps_part_results[ref_id_of_nearest_poc][PART_ID_2Nx2N]
                                        ->s_mv.i2_mvx;
                                i2_mvy =
                                    ps_search_results
                                        ->aps_part_results[ref_id_of_nearest_poc][PART_ID_2Nx2N]
                                        ->s_mv.i2_mvy;

                                /*register the min cost for l1 me in blk context */
                                wt = ps_ctxt->s_wt_pred.a_wpred_wt[ref_id_of_nearest_poc];
                                log_wt = ps_ctxt->s_wt_pred.wpred_log_wdc;

                                /*register the min cost for l1 me in blk context */
                                ps_ed_ctb_l1_curr->i4_sad_me_for_ref[z_scan_idx >> 2] =
                                    ((ps_search_results
                                          ->aps_part_results[ref_id_of_nearest_poc][PART_ID_2Nx2N]
                                          ->i4_sad *
                                      wt) +
                                     ((1 << log_wt) >> 1)) >>
                                    log_wt;
                                ps_ed_ctb_l1_curr->i4_sad_cost_me_for_ref[z_scan_idx >> 2] =
                                    ps_ed_ctb_l1_curr->i4_sad_me_for_ref[z_scan_idx >> 2] +
                                    (ps_search_results
                                         ->aps_part_results[ref_id_of_nearest_poc][PART_ID_2Nx2N]
                                         ->i4_tot_cost -
                                     ps_search_results
                                         ->aps_part_results[ref_id_of_nearest_poc][PART_ID_2Nx2N]
                                         ->i4_sad);
                                /*for complexity change detection*/
                                ps_ctxt->i4_num_blks++;
                                if(ps_ed_ctb_l1_curr->i4_sad_cost_me_for_ref[z_scan_idx >> 2] >
                                   (8 /*blk width*/ * 8 /*blk height*/ * (1 + ps_ctxt->num_b_frms)))
                                {
                                    ps_ctxt->i4_num_blks_high_sad++;
                                }
                            }
                        }
                    }

                    /* EIID: Early inter intra decisions */
                    /* tap L1 level SAD for inter intra decisions */
                    if((e_me_quality_presets >= ME_MEDIUM_SPEED) &&
                       (!ps_ctxt->s_frm_prms
                             .is_i_pic))  //for high-quality preset->disable early decisions
                    {
                        if(1 == ps_refine_prms->i4_layer_id)
                        {
                            WORD32 i4_min_sad_cost_8x8_block = min_cost;
                            ihevce_ed_blk_t *ps_curr_ed_blk_ctxt;
                            WORD32 i4_diff_col_ctr = blk_x - (i4_ctb_blk_ctr * 4);
                            WORD32 i4_diff_row_ctr = blk_y - (i4_ctb_row_ctr * 4);
                            WORD32 z_scan_idx =
                                gau1_raster_scan_to_ctb[i4_diff_row_ctr][i4_diff_col_ctr];
                            ps_curr_ed_blk_ctxt = ps_ed_blk_ctxt_curr_ctb + z_scan_idx;

                            /*register the min cost for l1 me in blk context */
                            ps_ed_ctb_l1_curr->i4_best_sad_cost_8x8_l1_me[z_scan_idx >> 2] =
                                i4_min_sad_cost_8x8_block;
                            i4_num_comparisions++;

                            /* take early inter-intra decision here */
                            ps_curr_ed_blk_ctxt->intra_or_inter = 3; /*init saying eval both */
#if DISABLE_INTRA_IN_BPICS
                            if((e_me_quality_presets == ME_XTREME_SPEED_25) &&
                               (ps_ctxt->s_frm_prms.i4_temporal_layer_id > TEMPORAL_LAYER_DISABLE))
                            {
                                ps_curr_ed_blk_ctxt->intra_or_inter =
                                    2; /*eval only inter if inter cost is less */
                                i4_num_inter_wins++;
                            }
                            else
#endif
                            {
                                if(ps_ed_ctb_l1_curr->i4_best_sad_cost_8x8_l1_me[z_scan_idx >> 2] <
                                   ((ps_ed_ctb_l1_curr->i4_best_sad_cost_8x8_l1_ipe[z_scan_idx >> 2] *
                                     i4_threshold_multiplier) /
                                    i4_threshold_divider))
                                {
                                    ps_curr_ed_blk_ctxt->intra_or_inter =
                                        2; /*eval only inter if inter cost is less */
                                    i4_num_inter_wins++;
                                }
                            }

                            //{
                            //  DBG_PRINTF ("(blk x, blk y):(%d, %d)\t me:(ctb_x, ctb_y):(%d, %d)\t intra_SAD_COST: %d\tInter_SAD_COST: %d\n",
                            //      blk_x,blk_y,
                            //      i4_ctb_blk_ctr, i4_ctb_row_ctr,
                            //      ps_curr_ed_blk_ctxt->i4_best_sad_8x8_l1_ipe,
                            //      i4_min_sad_cost_8x8_block
                            //      );
                            //}

                        }  //end of layer-1
                    }  //end of if (e_me_quality_presets >= ME_MEDIUM_SPEED)
                    else
                    {
                        if(1 == ps_refine_prms->i4_layer_id)
                        {
                            WORD32 i4_min_sad_cost_8x8_block = min_cost;
                            WORD32 i4_diff_col_ctr = blk_x - (i4_ctb_blk_ctr * 4);
                            WORD32 i4_diff_row_ctr = blk_y - (i4_ctb_row_ctr * 4);
                            WORD32 z_scan_idx =
                                gau1_raster_scan_to_ctb[i4_diff_row_ctr][i4_diff_col_ctr];

                            /*register the min cost for l1 me in blk context */
                            ps_ed_ctb_l1_curr->i4_best_sad_cost_8x8_l1_me[z_scan_idx >> 2] =
                                i4_min_sad_cost_8x8_block;
                        }
                    }
                    if(1 == ps_refine_prms->i4_layer_id)
                    {
                        WORD32 i4_diff_col_ctr = blk_x - (i4_ctb_blk_ctr * 4);
                        WORD32 i4_diff_row_ctr = blk_y - (i4_ctb_row_ctr * 4);
                        WORD32 z_scan_idx =
                            gau1_raster_scan_to_ctb[i4_diff_row_ctr][i4_diff_col_ctr];

                        ps_ed_ctb_l1_curr->i4_best_sad_8x8_l1_me_for_decide[z_scan_idx >> 2] =
                            min_sad;

                        if(min_cost <
                           ps_ed_ctb_l1_curr->i4_best_sad_cost_8x8_l1_ipe[z_scan_idx >> 2])
                        {
                            ps_ctxt->i4_L1_hme_best_cost += min_cost;
                            ps_ctxt->i4_L1_hme_sad += min_sad;
                            ps_ed_ctb_l1_curr->i4_best_sad_8x8_l1_me[z_scan_idx >> 2] = min_sad;
                        }
                        else
                        {
                            ps_ctxt->i4_L1_hme_best_cost +=
                                ps_ed_ctb_l1_curr->i4_best_sad_cost_8x8_l1_ipe[z_scan_idx >> 2];
                            ps_ctxt->i4_L1_hme_sad +=
                                ps_ed_ctb_l1_curr->i4_best_sad_8x8_l1_ipe[z_scan_idx >> 2];
                            ps_ed_ctb_l1_curr->i4_best_sad_8x8_l1_me[z_scan_idx >> 2] =
                                ps_ed_ctb_l1_curr->i4_best_sad_8x8_l1_ipe[z_scan_idx >> 2];
                        }
                    }
                }
            }

            /* Update the number of blocks processed in the current row */
            if((ME_MEDIUM_SPEED > e_me_quality_presets))
            {
                ihevce_dmgr_set_row_row_sync(
                    pv_hme_dep_mngr,
                    (i4_ctb_x + 1),
                    blk_y,
                    0 /* Col Tile No. : Not supported in PreEnc*/);
            }
        }

        /* set the output dependency after completion of row */
        ihevce_pre_enc_grp_job_set_out_dep(ps_multi_thrd_ctxt, ps_job, i4_ping_pong);
    }
}
