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
#include "ihevce_inter_pred.h"
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
/* Macros                                                                    */
/*****************************************************************************/
#define UNI_SATD_SCALE 1

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void ihevce_open_loop_pred_data(
    me_frm_ctxt_t *ps_ctxt,
    inter_pu_results_t *ps_pu_results,
    U08 *pu1_src,
    U08 *pu1_temp_pred,
    S32 stride,
    S32 src_strd,
    UWORD8 e_part_id)
{
    S32 best_sad_l0 = -1, best_sad_l1 = -1;
    S32 sad_diff, status;
    inter_pred_me_ctxt_t *ps_inter_pred_me_ctxt;
    U08 enable_bi = 0;
    pu_t s_pu;

    ps_inter_pred_me_ctxt = &ps_ctxt->s_mc_ctxt;
    ps_ctxt->i4_count++;
    /* L0*/
    if(ps_pu_results->u1_num_results_per_part_l0[e_part_id])
    {
        pu_result_t *ps_best_l0_pu;
        ps_best_l0_pu = ps_pu_results->aps_pu_results[0][PRT_2Nx2N];
        best_sad_l0 = ps_best_l0_pu->i4_tot_cost - ps_best_l0_pu->i4_mv_cost;
        s_pu.b2_pred_mode = PRED_L0;
        s_pu.b4_ht = ps_best_l0_pu->pu.b4_ht;
        s_pu.b4_wd = ps_best_l0_pu->pu.b4_wd;
        s_pu.b4_pos_x = ps_best_l0_pu->pu.b4_pos_x;
        s_pu.b4_pos_y = ps_best_l0_pu->pu.b4_pos_y;
        s_pu.b1_intra_flag = 0;
        s_pu.mv.s_l0_mv.i2_mvx = ps_best_l0_pu->pu.mv.s_l0_mv.i2_mvx;
        s_pu.mv.s_l0_mv.i2_mvy = ps_best_l0_pu->pu.mv.s_l0_mv.i2_mvy;
        s_pu.mv.i1_l0_ref_idx = ps_best_l0_pu->pu.mv.i1_l0_ref_idx;
    }
    /*L1*/
    if(ps_pu_results->u1_num_results_per_part_l1[e_part_id])
    {
        pu_result_t *ps_best_l1_pu;
        ps_best_l1_pu = ps_pu_results->aps_pu_results[1][PRT_2Nx2N];
        best_sad_l1 = ps_best_l1_pu->i4_tot_cost - ps_best_l1_pu->i4_mv_cost;
        s_pu.b2_pred_mode = PRED_L1;
        s_pu.b4_ht = ps_best_l1_pu->pu.b4_ht;
        s_pu.b4_wd = ps_best_l1_pu->pu.b4_wd;
        s_pu.b4_pos_x = ps_best_l1_pu->pu.b4_pos_x;
        s_pu.b4_pos_y = ps_best_l1_pu->pu.b4_pos_y;
        s_pu.b1_intra_flag = 0;
        s_pu.mv.s_l1_mv.i2_mvx = ps_best_l1_pu->pu.mv.s_l1_mv.i2_mvx;
        s_pu.mv.s_l1_mv.i2_mvy = ps_best_l1_pu->pu.mv.s_l1_mv.i2_mvy;
        s_pu.mv.i1_l1_ref_idx = ps_best_l1_pu->pu.mv.i1_l1_ref_idx;
    }
    ASSERT((best_sad_l0 != -1) || (best_sad_l1 != -1));
    /*bi selection*/
    if((best_sad_l0 != -1) && (best_sad_l1 != -1))
    {
        sad_diff = abs(best_sad_l0 - best_sad_l1);
        if((sad_diff < (best_sad_l0 * 0.15)) && (sad_diff < (best_sad_l1 * 0.15)))
        {
            enable_bi = 1;
            s_pu.b2_pred_mode = PRED_BI;
        }
        if(!enable_bi)
        {
            if(best_sad_l0 < best_sad_l1)
            {
                s_pu.b2_pred_mode = PRED_L0;
            }
            else
            {
                s_pu.b2_pred_mode = PRED_L1;
            }
        }
    }
    status = ihevce_luma_inter_pred_pu(ps_inter_pred_me_ctxt, &s_pu, pu1_temp_pred, stride, 1);
    if(status == -1)
    {
        ASSERT(0);
    }
}

/**
********************************************************************************
*  @fn     void *hme_get_wkg_mem(buf_mgr_t *ps_buf_mgr, S32 i4_size)
*
*  @brief  Allocates a block of size = i4_size from working memory and returns
*
*  @param[in,out] ps_buf_mgr: Buffer manager for wkg memory
*
*  @param[in]  i4_size : size required
*
*  @return void pointer to allocated memory, NULL if failure
********************************************************************************
*/
void *hme_get_wkg_mem(buf_mgr_t *ps_buf_mgr, S32 i4_size)
{
    U08 *pu1_mem;

    if(ps_buf_mgr->i4_used + i4_size > ps_buf_mgr->i4_total)
        return NULL;

    pu1_mem = ps_buf_mgr->pu1_wkg_mem + ps_buf_mgr->i4_used;
    ps_buf_mgr->i4_used += i4_size;

    return ((void *)pu1_mem);
}

/**
********************************************************************************
*  @fn     hme_init_histogram(
*
*  @brief  Top level entry point for Coarse ME. Runs across blocks and does the
*          needful by calling other low level routines.
*
*  @param[in,out]  ps_hist : the histogram structure
*
*  @param[in]  i4_max_mv_x : Maximum mv allowed in x direction (fpel units)
*
*  @param[in]  i4_max_mv_y : Maximum mv allowed in y direction (fpel units)
*
*  @return None
********************************************************************************
*/

void hme_init_histogram(mv_hist_t *ps_hist, S32 i4_max_mv_x, S32 i4_max_mv_y)
{
    S32 i4_num_bins, i4_num_cols, i4_num_rows;
    S32 i4_shift_x, i4_shift_y, i, i4_range, i4_val;

    /*************************************************************************/
    /* Evaluate the shift_x and shift_y. For this, we use the following logic*/
    /* Assuming that we use up all MAX_NUM_BINS. Then the number of bins is  */
    /* given by formula ((max_mv_x * 2) >> shift_x)*((max_mv_y * 2)>>shift_y)*/
    /* or shift_x + shift_y is log ((max_mv_x * max_mv_y * 4) / MAX_NUM_BINS)*/
    /* if above quantity is negative, then we make it zero.                  */
    /* If result is odd, then shift_y is result >> 1, shift_x is shift_y + 1 */
    /*************************************************************************/
    i4_val = i4_max_mv_x * i4_max_mv_y * 4;
    i4_range = (hme_get_range(i4_val - 1)) + 1;
    if(i4_range > LOG_MAX_NUM_BINS)
    {
        i4_shift_y = (i4_range - LOG_MAX_NUM_BINS);
        i4_shift_x = (i4_shift_y + 1) >> 1;
        i4_shift_y >>= 1;
    }
    else
    {
        i4_shift_y = 0;
        i4_shift_x = 0;
    }

    /* we assume the mv range is -max_mv_x to +max_mv_x, ditto for y */
    /* So number of columns is 2*max_mv_x >> i4_shift_x. Ditto for rows */
    /* this helps us compute num bins that are active for this histo session */
    i4_num_cols = (i4_max_mv_x << 1) >> i4_shift_x;
    i4_num_rows = (i4_max_mv_y << 1) >> i4_shift_y;
    i4_num_bins = i4_num_rows * i4_num_cols;

    ASSERT(i4_num_bins <= MAX_NUM_BINS);

    ps_hist->i4_num_rows = i4_num_rows;
    ps_hist->i4_num_cols = i4_num_cols;
    ps_hist->i4_min_x = -i4_max_mv_x;
    ps_hist->i4_min_y = -i4_max_mv_y;
    ps_hist->i4_shift_x = i4_shift_x;
    ps_hist->i4_shift_y = i4_shift_y;
    ps_hist->i4_lobe1_size = 5;
    ps_hist->i4_lobe2_size = 3;

    ps_hist->i4_num_bins = i4_num_bins;

    for(i = 0; i < i4_num_bins; i++)
    {
        ps_hist->ai4_bin_count[i] = 0;
    }
}

/**
********************************************************************************
*  @fn     hme_update_histogram(
*
*  @brief  Updates the histogram given an mv entry
*
*  @param[in,out]  ps_hist : the histogram structure
*
*  @param[in]  i4_mv_x : x component of the mv (fpel units)
*
*  @param[in]  i4_mv_y : y component of the mv (fpel units)
*
*  @return None
********************************************************************************
*/
void hme_update_histogram(mv_hist_t *ps_hist, S32 i4_mv_x, S32 i4_mv_y)
{
    S32 i4_bin_index, i4_col, i4_row;

    i4_col = (i4_mv_x - ps_hist->i4_min_x) >> ps_hist->i4_shift_x;
    i4_row = (i4_mv_y - ps_hist->i4_min_y) >> ps_hist->i4_shift_y;

    i4_bin_index = i4_col + (i4_row * ps_hist->i4_num_cols);
    /* Sanity Check */
    ASSERT(i4_bin_index < MAX_NUM_BINS);

    ps_hist->ai4_bin_count[i4_bin_index]++;
}

/**
********************************************************************************
*  @fn     hme_get_global_mv(
*
*  @brief  returns the global mv of a previous picture. Accounts for the fact
*          that the delta poc of the previous picture may have been different
*          from delta poc of current picture. Delta poc is POC difference
*          between a picture and its reference.
*
*  @param[out]  ps_mv: mv_t structure where the motion vector is returned
*
*  @param[in]  i4_delta_poc: the delta poc for the current pic w.r.t. reference
*
*  @return None
********************************************************************************
*/
void hme_get_global_mv(layer_ctxt_t *ps_prev_layer, hme_mv_t *ps_mv, S32 i4_delta_poc)
{
    S16 i2_mv_x, i2_mv_y;
    S32 i4_delta_poc_prev;
    S32 i4_poc_prev = ps_prev_layer->i4_poc;
    S32 i4_poc_prev_ref = ps_prev_layer->ai4_ref_id_to_poc_lc[0];

    i4_delta_poc_prev = i4_poc_prev - i4_poc_prev_ref;
    i2_mv_x = ps_prev_layer->s_global_mv[0][GMV_THICK_LOBE].i2_mv_x;
    i2_mv_y = ps_prev_layer->s_global_mv[0][GMV_THICK_LOBE].i2_mv_y;

    i2_mv_x = (S16)((i2_mv_x * i4_delta_poc) / i4_delta_poc_prev);
    i2_mv_y = (S16)((i2_mv_y * i4_delta_poc) / i4_delta_poc_prev);

    ps_mv->i2_mv_x = i2_mv_x;
    ps_mv->i2_mv_y = i2_mv_y;
}

/**
********************************************************************************
*  @fn     hme_calculate_global_mv(
*
*  @brief  Calculates global mv for a given histogram
*
*  @param[in]  ps_hist : the histogram structure
*
*  @param[in]  ps_mv : used to return the global mv
*
*  @param[in]  e_lobe_type : refer to GMV_MVTYPE_T
*
*  @return None
********************************************************************************
*/
void hme_calculate_global_mv(mv_hist_t *ps_hist, hme_mv_t *ps_mv, GMV_MVTYPE_T e_lobe_type)
{
    S32 i4_offset, i4_lobe_size, i4_y, i4_x, *pi4_bin_count;
    S32 i4_max_sum = -1;
    S32 i4_max_x = 0, i4_max_y = 0;

    if(e_lobe_type == GMV_THICK_LOBE)
        i4_lobe_size = ps_hist->i4_lobe1_size;
    else
        i4_lobe_size = ps_hist->i4_lobe2_size;

    i4_offset = i4_lobe_size >> 1;
    for(i4_y = i4_offset; i4_y < ps_hist->i4_num_rows - i4_offset; i4_y++)
    {
        for(i4_x = i4_offset; i4_x < ps_hist->i4_num_cols - i4_offset; i4_x++)
        {
            S32 i4_bin_id, i4_sum;
            i4_bin_id = (i4_x - 2) + ((i4_y - 2) * ps_hist->i4_num_cols);

            pi4_bin_count = &ps_hist->ai4_bin_count[i4_bin_id];
            i4_sum = hme_compute_2d_sum_unsigned(
                (void *)pi4_bin_count,
                i4_lobe_size,
                i4_lobe_size,
                ps_hist->i4_num_cols,
                sizeof(U32));

            if(i4_sum > i4_max_sum)
            {
                i4_max_x = i4_x;
                i4_max_y = i4_y;
                i4_max_sum = i4_sum;
            }
        }
    }

    ps_mv->i2_mv_y = (S16)((i4_max_y << ps_hist->i4_shift_y) + ps_hist->i4_min_y);
    ps_mv->i2_mv_x = (S16)((i4_max_x << ps_hist->i4_shift_x) + ps_hist->i4_min_x);
}

/**
********************************************************************************
*  @fn    ctb_node_t *hme_get_ctb_node(ctb_mem_mgr_t *ps_mem_mgr)
*
*  @brief  returns a new ctb node usable for creating a new ctb candidate
*
*  @param[in] ps_mem_mgr : memory manager holding all ctb nodes
*
*  @return NULL if no free nodes, else ptr to the new ctb node
********************************************************************************
*/
ctb_node_t *hme_get_ctb_node(ctb_mem_mgr_t *ps_mem_mgr)
{
    U08 *pu1_ret;
    if((ps_mem_mgr->i4_used + ps_mem_mgr->i4_size) > ps_mem_mgr->i4_tot)
        return (NULL);
    pu1_ret = ps_mem_mgr->pu1_mem + ps_mem_mgr->i4_used;
    ps_mem_mgr->i4_used += ps_mem_mgr->i4_size;
    return ((ctb_node_t *)pu1_ret);
}

/**
********************************************************************************
*  @fn     hme_map_mvs_to_grid(mv_grid_t **pps_mv_grid,
search_results_t *ps_search_results, S32 i4_num_ref)
*
*  @brief  For a given CU whose results are in ps_search_results, the 17x17
*          mv grid is updated for future use within the CTB
*
*  @param[in] ps_search_results : Search results data structure
*
*  @param[out] pps_mv_grid: The mv grid (as many as num ref)
*
*  @param[in]  i4_num_ref: nuber of search iterations to update
*
*  @return None
********************************************************************************
*/
void hme_map_mvs_to_grid(
    mv_grid_t **pps_mv_grid,
    search_results_t *ps_search_results,
    U08 *pu1_pred_dir_searched,
    S32 i4_num_pred_dir)
{
    S32 i4_cu_start_offset;
    /*************************************************************************/
    /* Start x, y offset of CU relative to CTB. To update the mv grid which  */
    /* stores 1 mv per 4x4, we convert pixel offset to 4x4 blk offset        */
    /*************************************************************************/
    S32 i4_cu_offset_x = (S32)ps_search_results->u1_x_off >> 2;
    S32 i4_cu_offset_y = (S32)ps_search_results->u1_y_off >> 2;

    /* Controls the attribute of a given partition within CU   */
    /* , i.e. start locn, size                                 */
    part_attr_t *ps_part_attr;

    S32 i4_part, i4_part_id, num_parts, i4_stride;
    S16 i2_mv_x, i2_mv_y;
    S08 i1_ref_idx;

    /* Per partition, attributes w.r.t. CU start */
    S32 x_start, y_start, x_end, y_end, i4_x, i4_y;
    PART_TYPE_T e_part_type;

    /* Points to exact mv structures within the grid to be udpated */
    search_node_t *ps_grid_node, *ps_grid_node_tmp;

    /* points to exact mv grid (based on search iteration) to be updated */
    mv_grid_t *ps_mv_grid;

    search_node_t *ps_search_node;

    S32 shift, i, mv_shift = 2;
    /* Proportional to the size of CU, controls the number of 4x4 blks */
    /* to be updated                                                   */
    shift = ps_search_results->e_cu_size;
    ASSERT(i4_num_pred_dir <= 2);

    e_part_type = (PART_TYPE_T)ps_search_results->ps_cu_results->ps_best_results[0].u1_part_type;

    if((ps_search_results->e_cu_size == CU_16x16) && (ps_search_results->u1_split_flag) &&
       (ps_search_results->i4_part_mask & ENABLE_NxN))
    {
        e_part_type = PRT_NxN;
    }

    for(i = 0; i < i4_num_pred_dir; i++)
    {
        num_parts = gau1_num_parts_in_part_type[e_part_type];
        ps_mv_grid = pps_mv_grid[pu1_pred_dir_searched[i]];
        i4_stride = ps_mv_grid->i4_stride;

        i4_cu_start_offset =
            i4_cu_offset_x + i4_cu_offset_y * i4_stride + ps_mv_grid->i4_start_offset;

        /* Move to the appropriate 2d locn of CU start within Grid */
        ps_grid_node = &ps_mv_grid->as_node[i4_cu_start_offset];

        for(i4_part = 0; i4_part < num_parts; i4_part++)
        {
            i4_part_id = ge_part_type_to_part_id[e_part_type][i4_part];

            /* Pick the mvx and y and ref id corresponding to this partition */
            ps_search_node =
                ps_search_results->aps_part_results[pu1_pred_dir_searched[i]][i4_part_id];

            i2_mv_x = ps_search_node->s_mv.i2_mvx;
            i2_mv_y = ps_search_node->s_mv.i2_mvy;
            i1_ref_idx = ps_search_node->i1_ref_idx;

            /* Move to the appropriate location within the CU */
            ps_part_attr = &gas_part_attr_in_cu[i4_part_id];
            x_start = ps_part_attr->u1_x_start;
            x_end = x_start + ps_part_attr->u1_x_count;
            y_start = ps_part_attr->u1_y_start;
            y_end = y_start + ps_part_attr->u1_y_count;

            /* Convert attributes from 8x8 CU size to given CU size */
            x_start = (x_start << shift) >> mv_shift;
            x_end = (x_end << shift) >> mv_shift;
            y_start = (y_start << shift) >> mv_shift;
            y_end = (y_end << shift) >> mv_shift;

            ps_grid_node_tmp = ps_grid_node + y_start * i4_stride;

            /* Update all 4x4 blk mvs with the part mv */
            /* For e.g. we update 4 units in case of NxN for 16x16 CU */
            for(i4_y = y_start; i4_y < y_end; i4_y++)
            {
                for(i4_x = x_start; i4_x < x_end; i4_x++)
                {
                    ps_grid_node_tmp[i4_x].s_mv.i2_mvx = i2_mv_x;
                    ps_grid_node_tmp[i4_x].s_mv.i2_mvy = i2_mv_y;
                    ps_grid_node_tmp[i4_x].i1_ref_idx = i1_ref_idx;
                    ps_grid_node_tmp[i4_x].u1_subpel_done = 1;
                }
                ps_grid_node_tmp += i4_stride;
            }
        }
    }
}

void hme_set_ctb_pred_attr(ctb_node_t *ps_parent, U08 *pu1_pred0, U08 *pu1_pred1, S32 i4_stride)
{
    ps_parent->apu1_pred[0] = pu1_pred0;
    ps_parent->apu1_pred[1] = pu1_pred1;
    ps_parent->i4_pred_stride = i4_stride;
    if(ps_parent->ps_tl != NULL)
    {
        S32 blk_wd = (S32)ps_parent->ps_tr->u1_x_off;
        blk_wd -= (S32)ps_parent->u1_x_off;

        hme_set_ctb_pred_attr(ps_parent->ps_tl, pu1_pred0, pu1_pred1, i4_stride >> 1);

        hme_set_ctb_pred_attr(
            ps_parent->ps_tr, pu1_pred0 + blk_wd, pu1_pred1 + blk_wd, i4_stride >> 1);

        hme_set_ctb_pred_attr(
            ps_parent->ps_bl,
            pu1_pred0 + (blk_wd * i4_stride),
            pu1_pred1 + (blk_wd * i4_stride),
            i4_stride >> 1);

        hme_set_ctb_pred_attr(
            ps_parent->ps_tr,
            pu1_pred0 + (blk_wd * (1 + i4_stride)),
            pu1_pred1 + (blk_wd * (1 + i4_stride)),
            i4_stride >> 1);
    }
}

/**
********************************************************************************
*  @fn     hme_create_valid_part_ids(S32 i4_part_mask, S32 *pi4_valid_part_ids)
*
*  @brief  Expands the part mask to a list of valid part ids terminated by -1
*
*  @param[in] i4_part_mask : bit mask of active partitino ids
*
*  @param[out] pi4_valid_part_ids : array, each entry has one valid part id
*               Terminated by -1 to signal end.
*
*  @return number of partitions
********************************************************************************
*/
S32 hme_create_valid_part_ids(S32 i4_part_mask, S32 *pi4_valid_part_ids)
{
    S32 id = 0, i;
    for(i = 0; i < TOT_NUM_PARTS; i++)
    {
        if(i4_part_mask & (1 << i))
        {
            pi4_valid_part_ids[id] = i;
            id++;
        }
    }
    pi4_valid_part_ids[id] = -1;

    return id;
}

ctb_boundary_attrs_t *
    get_ctb_attrs(S32 ctb_start_x, S32 ctb_start_y, S32 pic_wd, S32 pic_ht, me_frm_ctxt_t *ps_ctxt)
{
    S32 horz_crop, vert_crop;
    ctb_boundary_attrs_t *ps_attrs;

    horz_crop = ((ctb_start_x + 64) > pic_wd) ? 2 : 0;
    vert_crop = ((ctb_start_y + 64) > pic_ht) ? 1 : 0;
    switch(horz_crop + vert_crop)
    {
    case 0:
        ps_attrs = &ps_ctxt->as_ctb_bound_attrs[CTB_CENTRE];
        break;
    case 1:
        ps_attrs = &ps_ctxt->as_ctb_bound_attrs[CTB_BOT_PIC_BOUNDARY];
        break;
    case 2:
        ps_attrs = &ps_ctxt->as_ctb_bound_attrs[CTB_RT_PIC_BOUNDARY];
        break;
    case 3:
        ps_attrs = &ps_ctxt->as_ctb_bound_attrs[CTB_BOT_RT_PIC_BOUNDARY];
        break;
    }
    return (ps_attrs);
}

/**
********************************************************************************
*  @fn     hevc_avg_2d(U08 *pu1_src1,
*                   U08 *pu1_src2,
*                   S32 i4_src1_stride,
*                   S32 i4_src2_stride,
*                   S32 i4_blk_wd,
*                   S32 i4_blk_ht,
*                   U08 *pu1_dst,
*                   S32 i4_dst_stride)
*
*
*  @brief  point wise average of two buffers into a third buffer
*
*  @param[in] pu1_src1 : first source buffer
*
*  @param[in] pu1_src2 : 2nd source buffer
*
*  @param[in] i4_src1_stride : stride of source 1 buffer
*
*  @param[in] i4_src2_stride : stride of source 2 buffer
*
*  @param[in] i4_blk_wd : block width
*
*  @param[in] i4_blk_ht : block height
*
*  @param[out] pu1_dst : destination buffer
*
*  @param[in] i4_dst_stride : stride of the destination buffer
*
*  @return void
********************************************************************************
*/
void hevc_avg_2d(
    U08 *pu1_src1,
    U08 *pu1_src2,
    S32 i4_src1_stride,
    S32 i4_src2_stride,
    S32 i4_blk_wd,
    S32 i4_blk_ht,
    U08 *pu1_dst,
    S32 i4_dst_stride)
{
    S32 i, j;

    for(i = 0; i < i4_blk_ht; i++)
    {
        for(j = 0; j < i4_blk_wd; j++)
        {
            pu1_dst[j] = (pu1_src1[j] + pu1_src2[j] + 1) >> 1;
        }
        pu1_src1 += i4_src1_stride;
        pu1_src2 += i4_src2_stride;
        pu1_dst += i4_dst_stride;
    }
}
/**
********************************************************************************
*  @fn     hme_pick_back_search_node(search_results_t *ps_search_results,
*                                   search_node_t *ps_search_node_fwd,
*                                   S32 i4_part_idx,
*                                   layer_ctxt_t *ps_curr_layer)
*
*
*  @brief  returns the search node corresponding to a ref idx in same or
*          opp direction. Preference is given to opp direction, but if that
*          does not yield results, same direction is attempted.
*
*  @param[in] ps_search_results: search results overall
*
*  @param[in] ps_search_node_fwd: search node corresponding to "fwd" direction
*
*  @param[in] i4_part_idx : partition id
*
*  @param[in] ps_curr_layer : layer context for current layer.
*
*  @return search node corresponding to hte "other direction"
********************************************************************************
*/
//#define PICK_L1_REF_SAME_DIR
search_node_t *hme_pick_back_search_node(
    search_results_t *ps_search_results,
    search_node_t *ps_search_node_fwd,
    S32 i4_part_idx,
    layer_ctxt_t *ps_curr_layer)
{
    S32 is_past_l0, is_past_l1, id, i, i4_poc;
    S32 *pi4_ref_id_to_poc_lc = ps_curr_layer->ai4_ref_id_to_poc_lc;
    //ref_attr_t *ps_ref_attr_lc;
    S08 i1_ref_idx_fwd;
    S16 i2_mv_x, i2_mv_y;
    search_node_t *ps_search_node;

    i1_ref_idx_fwd = ps_search_node_fwd->i1_ref_idx;
    i2_mv_x = ps_search_node_fwd->s_mv.i2_mvx;
    i2_mv_y = ps_search_node_fwd->s_mv.i2_mvy;
    i4_poc = ps_curr_layer->i4_poc;

    //ps_ref_attr_lc = &ps_curr_layer->as_ref_attr_lc[0];
    /* If the ref id already picked up maps to a past pic, then we pick */
    /* a result corresponding to future pic. If such a result is not    */
    /* to be found, then we pick a result corresponding to a past pic   */
    //is_past = ps_ref_attr_lc[i1_ref_idx_fwd].u1_is_past;
    is_past_l0 = (i4_poc > pi4_ref_id_to_poc_lc[i1_ref_idx_fwd]) ? 1 : 0;

    ASSERT(ps_search_results->u1_num_active_ref <= 2);

    /* pick the right iteration of search nodes to pick up */
#ifdef PICK_L1_REF_SAME_DIR
    if(ps_search_results->u1_num_active_ref == 2)
        id = !is_past_l0;
#else
    if(ps_search_results->u1_num_active_ref == 2)
        id = is_past_l0;
#endif
    else
        id = 0;

    ps_search_node = ps_search_results->aps_part_results[id][i4_part_idx];

    for(i = 0; i < ps_search_results->u1_num_results_per_part; i++)
    {
        S08 i1_ref_test = ps_search_node[i].i1_ref_idx;
        is_past_l1 = (pi4_ref_id_to_poc_lc[i1_ref_test] < i4_poc) ? 1 : 0;
        //if (ps_ref_attr_lc[ps_search_node[i].i1_ref_idx].u1_is_past != is_past)
#ifdef PICK_L1_REF_SAME_DIR
        if(is_past_l1 == is_past_l0)
#else
        if(is_past_l1 != is_past_l0)
#endif
        {
            /* belongs to same direction as the ref idx passed, so continue */
            return (ps_search_node + i);
        }
    }

    /* Unable to find best result in opp direction, so try same direction */
    /* However we need to ensure that we do not pick up same result       */
    for(i = 0; i < ps_search_results->u1_num_results_per_part; i++)
    {
        if((ps_search_node->i1_ref_idx != i1_ref_idx_fwd) ||
           (ps_search_node->s_mv.i2_mvx != i2_mv_x) || (ps_search_node->s_mv.i2_mvy != i2_mv_y))
        {
            return (ps_search_node);
        }
        ps_search_node++;
    }

    //ASSERT(0);
    return (ps_search_results->aps_part_results[id][i4_part_idx]);

    //return (NULL);
}

/**
********************************************************************************
*  @fn     hme_study_input_segmentation(U08 *pu1_inp, S32 i4_inp_stride)
*
*
*  @brief  Examines input 16x16 for possible edges and orientations of those,
*          and returns a bit mask of partitions that should be searched for
*
*  @param[in] pu1_inp : input buffer
*
*  @param[in] i4_inp_stride: input stride
*
*  @return part mask (bit mask of active partitions to search)
********************************************************************************
*/

S32 hme_study_input_segmentation(U08 *pu1_inp, S32 i4_inp_stride, S32 limit_active_partitions)
{
    S32 i4_rsum[16], i4_csum[16];
    U08 *pu1_tmp, u1_tmp;
    S32 i4_max_ridx, i4_max_cidx, i4_tmp;
    S32 i, j, i4_ret;
    S32 i4_max_rp[4], i4_max_cp[4];
    S32 i4_seg_lutc[4] = { 0, ENABLE_nLx2N, ENABLE_Nx2N, ENABLE_nRx2N };
    S32 i4_seg_lutr[4] = { 0, ENABLE_2NxnU, ENABLE_2NxN, ENABLE_2NxnD };
#define EDGE_THR (15 * 16)
#define HI_PASS(ptr, i) (2 * (ptr[i] - ptr[i - 1]) + (ptr[i + 1] - ptr[i - 2]))

    if(0 == limit_active_partitions)
    {
        /*********************************************************************/
        /* In this case, we do not optimize on active partitions and search  */
        /* brute force. This way, 17 partitinos would be enabled.            */
        /*********************************************************************/
        return (ENABLE_ALL_PARTS);
    }

    /*************************************************************************/
    /* Control passes below in case we wish to optimize on active partitions.*/
    /* This is based on input characteristics, check how an edge passes along*/
    /* an input 16x16 area, if at all, and decide active partitinos.         */
    /*************************************************************************/

    /* Initialize row and col sums */
    for(i = 0; i < 16; i++)
    {
        i4_rsum[i] = 0;
        i4_csum[i] = 0;
    }
    pu1_tmp = pu1_inp;
    for(i = 0; i < 16; i++)
    {
        for(j = 0; j < 16; j++)
        {
            u1_tmp = *pu1_tmp++;
            i4_rsum[i] += u1_tmp;
            i4_csum[j] += u1_tmp;
        }
        pu1_tmp += (i4_inp_stride - 16);
    }

    /* 0 is dummy; 1 is 4; 2 is 8; 3 is 12 */
    i4_max_rp[0] = 0;
    i4_max_cp[0] = 0;
    i4_max_rp[1] = 0;
    i4_max_cp[1] = 0;
    i4_max_rp[2] = 0;
    i4_max_cp[2] = 0;
    i4_max_rp[3] = 0;
    i4_max_cp[3] = 0;

    /* Get Max edge strength across (2,3) (3,4) (4,5) */
    for(i = 3; i < 6; i++)
    {
        /* Run [-1 -2 2 1] filter through rsum/csum */
        i4_tmp = HI_PASS(i4_rsum, i);
        if(ABS(i4_tmp) > i4_max_rp[1])
            i4_max_rp[1] = i4_tmp;

        i4_tmp = HI_PASS(i4_csum, i);
        if(ABS(i4_tmp) > i4_max_cp[1])
            i4_max_cp[1] = i4_tmp;
    }

    /* Get Max edge strength across (6,7) (7,8) (8,9) */
    for(i = 7; i < 10; i++)
    {
        /* Run [-1 -2 2 1] filter through rsum/csum */
        i4_tmp = HI_PASS(i4_rsum, i);
        if(ABS(i4_tmp) > i4_max_rp[2])
            i4_max_rp[2] = i4_tmp;

        i4_tmp = HI_PASS(i4_csum, i);
        if(ABS(i4_tmp) > i4_max_cp[2])
            i4_max_cp[2] = i4_tmp;
    }

    /* Get Max edge strength across (10,11) (11,12) (12,13) */
    for(i = 11; i < 14; i++)
    {
        /* Run [-1 -2 2 1] filter through rsum/csum */
        i4_tmp = HI_PASS(i4_rsum, i);
        if(ABS(i4_tmp) > i4_max_rp[3])
            i4_max_rp[3] = i4_tmp;

        i4_tmp = HI_PASS(i4_csum, i);
        if(ABS(i4_tmp) > i4_max_cp[3])
            i4_max_cp[3] = i4_tmp;
    }

    /* Find the maximum across the 3 and see whether the strength qualifies as edge */
    i4_max_ridx = 1;
    i4_max_cidx = 1;
    for(i = 2; i <= 3; i++)
    {
        if(i4_max_rp[i] > i4_max_rp[i4_max_ridx])
            i4_max_ridx = i;

        if(i4_max_cp[i] > i4_max_cp[i4_max_cidx])
            i4_max_cidx = i;
    }

    if(EDGE_THR > i4_max_rp[i4_max_ridx])
    {
        i4_max_ridx = 0;
    }

    if(EDGE_THR > i4_max_cp[i4_max_cidx])
    {
        i4_max_cidx = 0;
    }

    i4_ret = ENABLE_2Nx2N;

    /* If only vertical discontinuity, go with one of 2Nx? */
    if(0 == (i4_max_ridx + i4_max_cidx))
    {
        //num_me_parts++;
        return i4_ret;
    }

    if(i4_max_ridx && (i4_max_cidx == 0))
    {
        //num_me_parts += 3;
        return ((i4_ret | i4_seg_lutr[i4_max_ridx]));
    }

    /* If only horizontal discontinuity, go with one of ?x2N */
    if(i4_max_cidx && (i4_max_ridx == 0))
    {
        //num_me_parts += 3;
        return ((i4_ret | i4_seg_lutc[i4_max_cidx]));
    }

    /* If middle is dominant in both directions, go with NxN */
    if((2 == i4_max_cidx) && (2 == i4_max_ridx))
    {
        //num_me_parts += 5;
        return ((i4_ret | ENABLE_NxN));
    }

    /* Otherwise, conservatively, enable NxN and the 2 AMPs */
    //num_me_parts += 9;
    return (i4_ret | ENABLE_NxN | i4_seg_lutr[i4_max_ridx] | i4_seg_lutc[i4_max_cidx]);
}

/**
********************************************************************************
*  @fn     hme_init_search_results(search_results_t *ps_search_results,
*                           S32 i4_num_ref,
*                           S32 i4_num_best_results,
*                           S32 i4_num_results_per_part,
*                           BLK_SIZE_T e_blk_size,
*                           S32 i4_x_off,
*                           S32 i4_y_off)
*
*  @brief  Initializes the search results structure with some key attributes
*
*  @param[out] ps_search_results : search results structure to initialise
*
*  @param[in] i4_num_Ref: corresponds to the number of ref ids searched
*
*  @param[in] i4_num_best_results: Number of best results for the CU to
*               be maintained in the result structure
*
*  @param[in] i4_num_results_per_part: Per active partition the number of best
*               results to be maintained
*
*  @param[in] e_blk_size: blk size of the CU for which this structure used
*
*  @param[in] i4_x_off: x offset of the top left of CU from CTB top left
*
*  @param[in] i4_y_off: y offset of the top left of CU from CTB top left
*
*  @param[in] pu1_is_past : points ot an array that tells whether a given ref id
*              has prominence in L0 or in L1 list (past or future )
*
*  @return void
********************************************************************************
*/
void hme_init_search_results(
    search_results_t *ps_search_results,
    S32 i4_num_ref,
    S32 i4_num_best_results,
    S32 i4_num_results_per_part,
    BLK_SIZE_T e_blk_size,
    S32 i4_x_off,
    S32 i4_y_off,
    U08 *pu1_is_past)
{
    CU_SIZE_T e_cu_size = ge_blk_size_to_cu_size[e_blk_size];

    ASSERT(e_cu_size != -1);
    ps_search_results->e_cu_size = e_cu_size;
    ps_search_results->u1_x_off = (U08)i4_x_off;
    ps_search_results->u1_y_off = (U08)i4_y_off;
    ps_search_results->u1_num_active_ref = (U08)i4_num_ref;
    ps_search_results->u1_num_best_results = (U08)i4_num_best_results;
    ps_search_results->u1_num_results_per_part = (U08)i4_num_results_per_part;
    ps_search_results->pu1_is_past = pu1_is_past;
    ps_search_results->u1_split_flag = 0;
    ps_search_results->best_cu_cost = MAX_32BIT_VAL;
}

/**
********************************************************************************
*  @fn     hme_reset_search_results((search_results_t *ps_search_results,
*                               S32 i4_part_mask)
*
*
*  @brief  Resets the best results to maximum values, so as to allow search
*          for the new CU's partitions. The existing results may be from an
*          older CU using same structure.
*
*  @param[in] ps_search_results: search results structure
*
*  @param[in] i4_part_mask : bit mask of active partitions
*
*  @return part mask (bit mask of active partitions to search)
********************************************************************************
*/
void hme_reset_search_results(search_results_t *ps_search_results, S32 i4_part_mask, S32 mv_res)
{
    S32 i4_num_ref = (S32)ps_search_results->u1_num_active_ref;
    S08 i1_ref_idx;
    S32 i, j;
    search_node_t *ps_search_node;

    /* store this for future use */
    ps_search_results->i4_part_mask = i4_part_mask;

    /* Reset the spli_flag to zero */
    ps_search_results->u1_split_flag = 0;

    HME_SET_MVPRED_RES((&ps_search_results->as_pred_ctxt[0]), mv_res);
    HME_SET_MVPRED_RES((&ps_search_results->as_pred_ctxt[1]), mv_res);

    for(i1_ref_idx = 0; i1_ref_idx < i4_num_ref; i1_ref_idx++)
    {
        /* Reset the individual partitino results */
        for(i = 0; i < TOT_NUM_PARTS; i++)
        {
            if(!(i4_part_mask & (1 << i)))
                continue;

            ps_search_node = ps_search_results->aps_part_results[i1_ref_idx][i];

            for(j = 0; j < ps_search_results->u1_num_results_per_part; j++)
            {
                ps_search_node[j].s_mv.i2_mvx = 0;
                ps_search_node[j].s_mv.i2_mvy = 0;
                ps_search_node[j].i4_tot_cost = MAX_32BIT_VAL;
                ps_search_node[j].i4_sad = MAX_32BIT_VAL;
                ps_search_node[j].i4_sdi = 0;
                ps_search_node[j].i1_ref_idx = -1;
                ps_search_node[j].u1_subpel_done = 0;
                ps_search_node[j].u1_is_avail = 1;
                ps_search_node[j].i4_mv_cost = 0;
            }
        }
    }
}
/**
********************************************************************************
*  @fn     hme_clamp_grid_by_mvrange(search_node_t *ps_search_node,
*                               S32 i4_step,
*                               range_prms_t *ps_mvrange)
*
*  @brief  Given a central pt within mv range, and a grid of points surrounding
*           this pt, this function returns a grid mask of pts within search rng
*
*  @param[in] ps_search_node: the centre pt of the grid
*
*  @param[in] i4_step: step size of grid
*
*  @param[in] ps_mvrange: structure containing the current mv range
*
*  @return bitmask of the  pts in grid within search range
********************************************************************************
*/
S32 hme_clamp_grid_by_mvrange(search_node_t *ps_search_node, S32 i4_step, range_prms_t *ps_mvrange)
{
    S32 i4_mask = GRID_ALL_PTS_VALID;
    if(ps_search_node->s_mv.i2_mvx + i4_step >= ps_mvrange->i2_max_x)
    {
        i4_mask &= (GRID_RT_3_INVALID);
    }
    if(ps_search_node->s_mv.i2_mvx - i4_step < ps_mvrange->i2_min_x)
    {
        i4_mask &= (GRID_LT_3_INVALID);
    }
    if(ps_search_node->s_mv.i2_mvy + i4_step >= ps_mvrange->i2_max_y)
    {
        i4_mask &= (GRID_BOT_3_INVALID);
    }
    if(ps_search_node->s_mv.i2_mvy - i4_step < ps_mvrange->i2_min_y)
    {
        i4_mask &= (GRID_TOP_3_INVALID);
    }
    return i4_mask;
}

/**
********************************************************************************
*  @fn    layer_ctxt_t *hme_get_past_layer_ctxt(me_ctxt_t *ps_ctxt,
S32 i4_layer_id)
*
*  @brief  returns the layer ctxt of the layer with given id from the temporally
*          previous frame
*
*  @param[in] ps_ctxt : ME context
*
*  @param[in] i4_layer_id : id of layer required
*
*  @return layer ctxt of given layer id in temporally previous frame
********************************************************************************
*/
layer_ctxt_t *hme_get_past_layer_ctxt(
    me_ctxt_t *ps_ctxt, me_frm_ctxt_t *ps_frm_ctxt, S32 i4_layer_id, S32 i4_num_me_frm_pllel)
{
    S32 i4_poc = ps_frm_ctxt->ai4_ref_idx_to_poc_lc[0];
    S32 i;
    layers_descr_t *ps_desc;

    for(i = 0; i < (ps_ctxt->aps_me_frm_prms[0]->max_num_ref * i4_num_me_frm_pllel) + 1; i++)
    {
        ps_desc = &ps_ctxt->as_ref_descr[i];
        if(i4_poc == ps_desc->aps_layers[i4_layer_id]->i4_poc)
            return (ps_desc->aps_layers[i4_layer_id]);
    }
    return NULL;
}

/**
********************************************************************************
*  @fn    layer_ctxt_t *hme_coarse_get_past_layer_ctxt(me_ctxt_t *ps_ctxt,
S32 i4_layer_id)
*
*  @brief  returns the layer ctxt of the layer with given id from the temporally
*          previous frame
*
*  @param[in] ps_ctxt : ME context
*
*  @param[in] i4_layer_id : id of layer required
*
*  @return layer ctxt of given layer id in temporally previous frame
********************************************************************************
*/
layer_ctxt_t *hme_coarse_get_past_layer_ctxt(coarse_me_ctxt_t *ps_ctxt, S32 i4_layer_id)
{
    S32 i4_poc = ps_ctxt->ai4_ref_idx_to_poc_lc[0];
    S32 i;
    layers_descr_t *ps_desc;

    for(i = 0; i < ps_ctxt->max_num_ref + 1 + NUM_BUFS_DECOMP_HME; i++)
    {
        ps_desc = &ps_ctxt->as_ref_descr[i];
        if(i4_poc == ps_desc->aps_layers[i4_layer_id]->i4_poc)
            return (ps_desc->aps_layers[i4_layer_id]);
    }
    return NULL;
}

/**
********************************************************************************
*  @fn    void hme_init_mv_bank(layer_ctxt_t *ps_layer_ctxt,
BLK_SIZE_T e_blk_size,
S32 i4_num_ref,
S32 i4_num_results_per_part)
*
*  @brief  Given a blk size to be used for this layer, this function initialize
*          the mv bank to make it ready to store and return results.
*
*  @param[in, out] ps_layer_ctxt: pointer to layer ctxt
*
*  @param[in] e_blk_size : resolution at which mvs are stored
*
*  @param[in] i4_num_ref: number of reference frames corresponding to which
*              results are stored.
*
*  @param[in] e_blk_size : resolution at which mvs are stored
*
*  @param[in] i4_num_results_per_part : Number of results to be stored per
*               ref idx. So these many best results stored
*
*  @return void
********************************************************************************
*/
void hme_init_mv_bank(
    layer_ctxt_t *ps_layer_ctxt,
    BLK_SIZE_T e_blk_size,
    S32 i4_num_ref,
    S32 i4_num_results_per_part,
    U08 u1_enc)
{
    layer_mv_t *ps_mv_bank;
    hme_mv_t *ps_mv1, *ps_mv2;
    S08 *pi1_ref_id1, *pi1_ref_id2;
    S32 blk_wd, mvs_in_blk, blks_in_row, mvs_in_row, blks_in_col;
    S32 i4_i, i4_j, blk_ht;

    ps_mv_bank = ps_layer_ctxt->ps_layer_mvbank;
    ps_mv_bank->i4_num_mvs_per_ref = i4_num_results_per_part;
    ps_mv_bank->i4_num_ref = i4_num_ref;
    mvs_in_blk = i4_num_ref * i4_num_results_per_part;
    ps_mv_bank->i4_num_mvs_per_blk = mvs_in_blk;

    /*************************************************************************/
    /* Store blk size, from blk size derive blk width and use this to compute*/
    /* number of blocks every row. We also pad to left and top by 1, to      */
    /* support the prediction mechanism.                                     */
    /*************************************************************************/
    ps_mv_bank->e_blk_size = e_blk_size;
    blk_wd = gau1_blk_size_to_wd[e_blk_size];
    blk_ht = gau1_blk_size_to_ht[e_blk_size];

    blks_in_row = (ps_layer_ctxt->i4_wd + (blk_wd - 1)) / blk_wd;
    blks_in_col = (ps_layer_ctxt->i4_ht + (blk_ht - 1)) / blk_ht;

    if(u1_enc)
    {
        /* TODO: CTB64x64 is assumed. FIX according to actual CTB */
        WORD32 num_ctb_cols = ((ps_layer_ctxt->i4_wd + 63) >> 6);
        WORD32 num_ctb_rows = ((ps_layer_ctxt->i4_ht + 63) >> 6);

        blks_in_row = (num_ctb_cols << 3);
        blks_in_col = (num_ctb_rows << 3);
    }

    blks_in_row += 2;
    mvs_in_row = blks_in_row * mvs_in_blk;

    ps_mv_bank->i4_num_blks_per_row = blks_in_row;
    ps_mv_bank->i4_num_mvs_per_row = mvs_in_row;

    /* To ensure run time requirements fall within allocation time request */
    ASSERT(ps_mv_bank->i4_num_mvs_per_row <= ps_mv_bank->max_num_mvs_per_row);

    /*************************************************************************/
    /* Increment by one full row at top for padding and one column in left   */
    /* this gives us the actual start of mv for 0,0 blk                      */
    /*************************************************************************/
    ps_mv_bank->ps_mv = ps_mv_bank->ps_mv_base + mvs_in_row + mvs_in_blk;
    ps_mv_bank->pi1_ref_idx = ps_mv_bank->pi1_ref_idx_base + mvs_in_row + mvs_in_blk;

    memset(ps_mv_bank->ps_mv_base, 0, mvs_in_row * sizeof(hme_mv_t));
    memset(ps_mv_bank->pi1_ref_idx_base, -1, mvs_in_row * sizeof(U08));

    /*************************************************************************/
    /* Initialize top row, left col and right col with zeros since these are */
    /* used as candidates during searches.                                   */
    /*************************************************************************/
    ps_mv1 = ps_mv_bank->ps_mv_base + mvs_in_row;
    ps_mv2 = ps_mv1 + mvs_in_row - mvs_in_blk;
    pi1_ref_id1 = ps_mv_bank->pi1_ref_idx_base + mvs_in_row;
    pi1_ref_id2 = pi1_ref_id1 + mvs_in_row - mvs_in_blk;
    for(i4_i = 0; i4_i < blks_in_col; i4_i++)
    {
        for(i4_j = 0; i4_j < mvs_in_blk; i4_j++)
        {
            ps_mv1[i4_j].i2_mv_x = 0;
            ps_mv1[i4_j].i2_mv_y = 0;
            ps_mv2[i4_j].i2_mv_x = 0;
            ps_mv2[i4_j].i2_mv_y = 0;
            pi1_ref_id1[i4_j] = -1;
            pi1_ref_id2[i4_j] = -1;
        }
        ps_mv1 += mvs_in_row;
        ps_mv2 += mvs_in_row;
        pi1_ref_id1 += mvs_in_row;
        pi1_ref_id2 += mvs_in_row;
    }
}
void hme_fill_mvbank_intra(layer_ctxt_t *ps_layer_ctxt)
{
    layer_mv_t *ps_mv_bank;
    hme_mv_t *ps_mv;
    S08 *pi1_ref_id;
    S32 blk_wd, blks_in_row, mvs_in_row, blks_in_col;
    S32 i, j, blk_ht;
    BLK_SIZE_T e_blk_size;

    ps_mv_bank = ps_layer_ctxt->ps_layer_mvbank;

    /*************************************************************************/
    /* Store blk size, from blk size derive blk width and use this to compute*/
    /* number of blocks every row. We also pad to left and top by 1, to      */
    /* support the prediction mechanism.                                     */
    /*************************************************************************/
    e_blk_size = ps_mv_bank->e_blk_size;
    blk_wd = gau1_blk_size_to_wd[e_blk_size];
    blk_ht = gau1_blk_size_to_wd[e_blk_size];
    blks_in_row = ps_layer_ctxt->i4_wd / blk_wd;
    blks_in_col = ps_layer_ctxt->i4_ht / blk_ht;
    mvs_in_row = blks_in_row * ps_mv_bank->i4_num_mvs_per_blk;

    /*************************************************************************/
    /* Increment by one full row at top for padding and one column in left   */
    /* this gives us the actual start of mv for 0,0 blk                      */
    /*************************************************************************/
    ps_mv = ps_mv_bank->ps_mv;
    pi1_ref_id = ps_mv_bank->pi1_ref_idx;

    for(i = 0; i < blks_in_col; i++)
    {
        for(j = 0; j < blks_in_row; j++)
        {
            ps_mv[j].i2_mv_x = INTRA_MV;
            ps_mv[j].i2_mv_y = INTRA_MV;
            pi1_ref_id[j] = -1;
        }
        ps_mv += ps_mv_bank->i4_num_mvs_per_row;
        pi1_ref_id += ps_mv_bank->i4_num_mvs_per_row;
    }
}

/**
********************************************************************************
*  @fn    void hme_derive_search_range(range_prms_t *ps_range,
*                                   range_prms_t *ps_pic_limit,
*                                   range_prms_t *ps_mv_limit,
*                                   S32 i4_x,
*                                   S32 i4_y,
*                                   S32 blk_wd,
*                                   S32 blk_ht)
*
*  @brief  given picture limits and blk dimensions and mv search limits, obtains
*          teh valid search range such that the blk stays within pic boundaries,
*          where picture boundaries include padded portions of picture
*
*  @param[out] ps_range: updated with actual search range
*
*  @param[in] ps_pic_limit : picture boundaries
*
*  @param[in] ps_mv_limit: Search range limits for the mvs
*
*  @param[in] i4_x : x coordinate of the blk
*
*  @param[in] i4_y : y coordinate of the blk
*
*  @param[in] blk_wd : blk width
*
*  @param[in] blk_ht : blk height
*
*  @return void
********************************************************************************
*/
void hme_derive_search_range(
    range_prms_t *ps_range,
    range_prms_t *ps_pic_limit,
    range_prms_t *ps_mv_limit,
    S32 i4_x,
    S32 i4_y,
    S32 blk_wd,
    S32 blk_ht)
{
    ps_range->i2_max_x =
        MIN((ps_pic_limit->i2_max_x - (S16)blk_wd - (S16)i4_x), ps_mv_limit->i2_max_x);
    ps_range->i2_min_x = MAX((ps_pic_limit->i2_min_x - (S16)i4_x), ps_mv_limit->i2_min_x);
    ps_range->i2_max_y =
        MIN((ps_pic_limit->i2_max_y - (S16)blk_ht - (S16)i4_y), ps_mv_limit->i2_max_y);
    ps_range->i2_min_y = MAX((ps_pic_limit->i2_min_y - (S16)i4_y), ps_mv_limit->i2_min_y);
}

/**
********************************************************************************
*  @fn    void hme_get_spatial_candt(search_node_t *ps_search_node,
*                                   layer_ctxt_t *ps_curr_layer,
*                                   S32 i4_blk_x,
*                                   S32 i4_blk_y,
*                                   S08 i1_ref_id,
*                                   S32 i4_result_id)
*
*  @brief  obtains a candt from the same mv bank as the current one, its called
*          spatial candt as it does not require scaling for temporal distances
*
*  @param[out] ps_search_node: mv and ref id updated here of the candt
*
*  @param[in] ps_curr_layer: layer ctxt, has the mv bank structure pointer
*
*  @param[in] i4_blk_x : x coordinate of the block in mv bank
*
*  @param[in] i4_blk_y : y coordinate of the block in mv bank
*
*  @param[in] i1_ref_id : Corresponds to ref idx from which to pick up mv
*              results, useful if multiple ref idx candts maintained separately.
*
*  @param[in] i4_result_id : If multiple results stored per ref idx, this
*              pts to the id of the result
*
*  @param[in] tr_avail : top right availability of the block
*
*  @param[in] bl_avail : bottom left availability of the block
*
*  @return void
********************************************************************************
*/
void hme_get_spatial_candt(
    layer_ctxt_t *ps_curr_layer,
    BLK_SIZE_T e_search_blk_size,
    S32 i4_blk_x,
    S32 i4_blk_y,
    S08 i1_ref_idx,
    search_node_t *ps_top_neighbours,
    search_node_t *ps_left_neighbours,
    S32 i4_result_id,
    S32 tr_avail,
    S32 bl_avail,
    S32 encode)

{
    layer_mv_t *ps_layer_mvbank = ps_curr_layer->ps_layer_mvbank;
    S32 i4_blk_size1 = gau1_blk_size_to_wd[ps_layer_mvbank->e_blk_size];
    S32 i4_blk_size2 = gau1_blk_size_to_wd[e_search_blk_size];
    search_node_t *ps_search_node;
    S32 i4_offset;
    hme_mv_t *ps_mv, *ps_mv_base;
    S08 *pi1_ref_idx, *pi1_ref_idx_base;
    S32 jump = 1, mvs_in_blk, mvs_in_row;
    S32 shift = (encode ? 2 : 0);

    if(i4_blk_size1 != i4_blk_size2)
    {
        i4_blk_x <<= 1;
        i4_blk_y <<= 1;
        jump = 2;
        if((i4_blk_size1 << 2) == i4_blk_size2)
        {
            i4_blk_x <<= 1;
            i4_blk_y <<= 1;
            jump = 4;
        }
    }

    mvs_in_blk = ps_layer_mvbank->i4_num_mvs_per_blk;
    mvs_in_row = ps_layer_mvbank->i4_num_mvs_per_row;

    /* Adjust teh blk coord to point to top left locn */
    i4_blk_x -= 1;
    i4_blk_y -= 1;
    /* Pick up the mvs from the location */
    i4_offset = (i4_blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
    i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * i4_blk_y);

    ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

    ps_mv += (i1_ref_idx * ps_layer_mvbank->i4_num_mvs_per_ref) + i4_result_id;
    pi1_ref_idx += (i1_ref_idx * ps_layer_mvbank->i4_num_mvs_per_ref) + i4_result_id;

    ps_mv_base = ps_mv;
    pi1_ref_idx_base = pi1_ref_idx;

    /* ps_mv and pi1_ref_idx now point to the top left locn */
    /* Get 4 mvs as follows:                                */
    ps_search_node = ps_top_neighbours;
    COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);

    /* Move to top */
    ps_search_node++;
    ps_mv += mvs_in_blk;
    pi1_ref_idx += mvs_in_blk;
    COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);

    /* Move to t1 : relevant for 4x4 part searches or for partitions i 16x16 */
    if(ps_layer_mvbank->i4_num_mvs_per_ref > 1)
    {
        ps_search_node++;
        ps_mv += (mvs_in_blk * (jump >> 1));
        pi1_ref_idx += (mvs_in_blk * (jump >> 1));
        COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);
    }
    else
    {
        ps_search_node++;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
        ps_search_node->u1_is_avail = 0;
        ps_search_node->u1_subpel_done = 0;
    }

    /* Move to tr: this will be tr w.r.t. the blk being searched */
    ps_search_node++;
    if(tr_avail == 0)
    {
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
        ps_search_node->u1_is_avail = 0;
        ps_search_node->u1_subpel_done = 0;
    }
    else
    {
        ps_mv = ps_mv_base + (mvs_in_blk * (1 + jump));
        pi1_ref_idx = pi1_ref_idx_base + (mvs_in_blk * (1 + jump));
        COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);
    }

    /* Move to left */
    ps_search_node = ps_left_neighbours;
    ps_mv = ps_mv_base + mvs_in_row;
    pi1_ref_idx = pi1_ref_idx_base + mvs_in_row;
    COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);

    /* Move to l1 */
    if(ps_layer_mvbank->i4_num_mvs_per_ref > 1)
    {
        ps_search_node++;
        ps_mv += (mvs_in_row * (jump >> 1));
        pi1_ref_idx += (mvs_in_row * (jump >> 1));
        COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);
    }
    else
    {
        ps_search_node++;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
        ps_search_node->u1_is_avail = 0;
        ps_search_node->u1_subpel_done = 0;
    }

    /* Move to bl */
    ps_search_node++;
    if(bl_avail == 0)
    {
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
        ps_search_node->u1_is_avail = 0;
    }
    else
    {
        ps_mv = ps_mv_base + (mvs_in_row * (1 + jump));
        pi1_ref_idx = pi1_ref_idx_base + (mvs_in_row * (1 + jump));
        COPY_MV_TO_SEARCH_NODE(ps_search_node, ps_mv, pi1_ref_idx, i1_ref_idx, shift);
    }
}

void hme_get_spatial_candt_in_l1_me(
    layer_ctxt_t *ps_curr_layer,
    BLK_SIZE_T e_search_blk_size,
    S32 i4_blk_x,
    S32 i4_blk_y,
    S08 i1_ref_idx,
    U08 u1_pred_dir,
    search_node_t *ps_top_neighbours,
    search_node_t *ps_left_neighbours,
    S32 i4_result_id,
    S32 tr_avail,
    S32 bl_avail,
    S32 i4_num_act_ref_l0,
    S32 i4_num_act_ref_l1)
{
    search_node_t *ps_search_node;
    hme_mv_t *ps_mv, *ps_mv_base;

    S32 i4_offset;
    S32 mvs_in_blk, mvs_in_row;
    S08 *pi1_ref_idx, *pi1_ref_idx_base;
    S32 i4_mv_pos_in_implicit_array;

    layer_mv_t *ps_layer_mvbank = ps_curr_layer->ps_layer_mvbank;

    S32 i4_blk_size1 = gau1_blk_size_to_wd[ps_layer_mvbank->e_blk_size];
    S32 i4_blk_size2 = gau1_blk_size_to_wd[e_search_blk_size];
    S32 jump = 1;
    S32 shift = 0;
    S32 i4_num_results_in_given_dir =
        ((u1_pred_dir == 1) ? (ps_layer_mvbank->i4_num_mvs_per_ref * i4_num_act_ref_l1)
                            : (ps_layer_mvbank->i4_num_mvs_per_ref * i4_num_act_ref_l0));

    if(i4_blk_size1 != i4_blk_size2)
    {
        i4_blk_x <<= 1;
        i4_blk_y <<= 1;
        jump = 2;
        if((i4_blk_size1 << 2) == i4_blk_size2)
        {
            i4_blk_x <<= 1;
            i4_blk_y <<= 1;
            jump = 4;
        }
    }

    mvs_in_blk = ps_layer_mvbank->i4_num_mvs_per_blk;
    mvs_in_row = ps_layer_mvbank->i4_num_mvs_per_row;

    /* Adjust the blk coord to point to top left locn */
    i4_blk_x -= 1;
    i4_blk_y -= 1;
    /* Pick up the mvs from the location */
    i4_offset = (i4_blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
    i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * i4_blk_y);

    i4_offset +=
        ((u1_pred_dir == 1) ? (ps_layer_mvbank->i4_num_mvs_per_ref * i4_num_act_ref_l0) : 0);

    ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

    ps_mv_base = ps_mv;
    pi1_ref_idx_base = pi1_ref_idx;

    /* TL */
    {
        /* ps_mv and pi1_ref_idx now point to the top left locn */
        ps_search_node = ps_top_neighbours;

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }

    /* Move to top */
    {
        /* ps_mv and pi1_ref_idx now point to the top left locn */
        ps_search_node++;
        ps_mv += mvs_in_blk;
        pi1_ref_idx += mvs_in_blk;

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }

    /* Move to t1 : relevant for 4x4 part searches or for partitions i 16x16 */
    if(ps_layer_mvbank->i4_num_mvs_per_ref > 1)
    {
        ps_search_node++;
        ps_mv += (mvs_in_blk * (jump >> 1));
        pi1_ref_idx += (mvs_in_blk * (jump >> 1));

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }
    else
    {
        ps_search_node++;
        ps_search_node->u1_is_avail = 0;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
    }

    /* Move to tr: this will be tr w.r.t. the blk being searched */
    ps_search_node++;
    if(tr_avail == 0)
    {
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
        ps_search_node->u1_is_avail = 0;
        ps_search_node->u1_subpel_done = 0;
    }
    else
    {
        /* ps_mv and pi1_ref_idx now point to the top left locn */
        ps_mv = ps_mv_base + (mvs_in_blk * (1 + jump));
        pi1_ref_idx = pi1_ref_idx_base + (mvs_in_blk * (1 + jump));

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }

    /* Move to left */
    {
        /* ps_mv and pi1_ref_idx now point to the top left locn */
        ps_search_node = ps_left_neighbours;
        ps_mv = ps_mv_base + mvs_in_row;
        pi1_ref_idx = pi1_ref_idx_base + mvs_in_row;

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }

    /* Move to l1 */
    if(ps_layer_mvbank->i4_num_mvs_per_ref > 1)
    {
        /* ps_mv and pi1_ref_idx now point to the top left locn */
        ps_search_node++;
        ps_mv += (mvs_in_row * (jump >> 1));
        pi1_ref_idx += (mvs_in_row * (jump >> 1));

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }
    else
    {
        ps_search_node++;
        ps_search_node->u1_is_avail = 0;
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
    }

    /* Move to bl */
    ps_search_node++;
    if(bl_avail == 0)
    {
        ps_search_node->s_mv.i2_mvx = 0;
        ps_search_node->s_mv.i2_mvy = 0;
        ps_search_node->i1_ref_idx = i1_ref_idx;
        ps_search_node->u1_is_avail = 0;
    }
    else
    {
        /* ps_mv and pi1_ref_idx now point to the top left locn */
        ps_mv = ps_mv_base + (mvs_in_row * (1 + jump));
        pi1_ref_idx = pi1_ref_idx_base + (mvs_in_row * (1 + jump));

        i4_mv_pos_in_implicit_array = hme_find_pos_of_implicitly_stored_ref_id(
            pi1_ref_idx, i1_ref_idx, i4_result_id, i4_num_results_in_given_dir);

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
    }
}

/**
********************************************************************************
*  @fn    void hme_fill_ctb_neighbour_mvs(layer_ctxt_t *ps_curr_layer,
*                                   S32 i4_blk_x,
*                                   S32 i4_blk_y,
*                                   mvgrid_t *ps_mv_grid ,
*                                   S32 i1_ref_id)
*
*  @brief  The 18x18 MV grid for a ctb, is filled in first row and 1st col
*          this corresponds to neighbours (TL, T, TR, L, BL)
*
*  @param[in] ps_curr_layer: layer ctxt, has the mv bank structure pointer
*
*  @param[in] blk_x : x coordinate of the block in mv bank
*
*  @param[in] blk_y : y coordinate of the block in mv bank
*
*  @param[in] ps_mv_grid : Grid (18x18 mvs at 4x4 level)
*
*  @param[in] i1_ref_idx : Corresponds to ref idx from which to pick up mv
*              results, useful if multiple ref idx candts maintained separately.
*
*  @return void
********************************************************************************
*/
void hme_fill_ctb_neighbour_mvs(
    layer_ctxt_t *ps_curr_layer,
    S32 blk_x,
    S32 blk_y,
    mv_grid_t *ps_mv_grid,
    U08 u1_pred_dir_ctr,
    U08 u1_default_ref_id,
    S32 i4_num_act_ref_l0)
{
    search_node_t *ps_grid_node;
    layer_mv_t *ps_layer_mvbank = ps_curr_layer->ps_layer_mvbank;
    S32 i4_offset;
    hme_mv_t *ps_mv, *ps_mv_base;
    S08 *pi1_ref_idx, *pi1_ref_idx_base;
    S32 jump = 0, inc, i, mvs_in_blk, mvs_in_row;

    if(ps_layer_mvbank->e_blk_size == BLK_4x4)
    {
        /* searching 16x16, mvs are for 4x4 */
        jump = 1;
        blk_x <<= 2;
        blk_y <<= 2;
    }
    else
    {
        /* Searching 16x16, mvs are for 8x8 */
        blk_x <<= 1;
        blk_y <<= 1;
    }
    ASSERT(ps_layer_mvbank->e_blk_size != BLK_16x16);

    mvs_in_blk = ps_layer_mvbank->i4_num_mvs_per_blk;
    mvs_in_row = ps_layer_mvbank->i4_num_mvs_per_row;

    /* Adjust the blk coord to point to top left locn */
    blk_x -= 1;
    blk_y -= 1;

    /* Pick up the mvs from the location */
    i4_offset = (blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
    i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y);

    i4_offset += (u1_pred_dir_ctr == 1);

    ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
    pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

    ps_mv_base = ps_mv;
    pi1_ref_idx_base = pi1_ref_idx;

    /* the 0, 0 entry of the grid pts to top left for the ctb */
    ps_grid_node = &ps_mv_grid->as_node[0];

    /* Copy 18 mvs at 4x4 level including top left, 16 top mvs for ctb, 1 tr */
    for(i = 0; i < 18; i++)
    {
        COPY_MV_TO_SEARCH_NODE(ps_grid_node, ps_mv, pi1_ref_idx, u1_default_ref_id, 0);
        ps_grid_node++;
        inc = 1;
        /* If blk size is 8x8, then every 2 grid nodes are updated with same mv */
        if(i & 1)
            inc = jump;

        ps_mv += (mvs_in_blk * inc);
        pi1_ref_idx += (mvs_in_blk * inc);
    }

    ps_mv = ps_mv_base + mvs_in_row;
    pi1_ref_idx = pi1_ref_idx_base + mvs_in_row;

    /* now copy left 16 left mvs */
    ps_grid_node = &ps_mv_grid->as_node[0];
    ps_grid_node += (ps_mv_grid->i4_stride);
    for(i = 0; i < 16; i++)
    {
        COPY_MV_TO_SEARCH_NODE(ps_grid_node, ps_mv, pi1_ref_idx, u1_default_ref_id, 0);
        ps_grid_node += ps_mv_grid->i4_stride;
        inc = 1;
        /* If blk size is 8x8, then every 2 grid nodes are updated with same mv */
        if(!(i & 1))
            inc = jump;

        ps_mv += (mvs_in_row * inc);
        pi1_ref_idx += (mvs_in_row * inc);
    }
    /* last one set to invalid as bottom left not yet encoded */
    ps_grid_node->u1_is_avail = 0;
}

void hme_reset_wkg_mem(buf_mgr_t *ps_buf_mgr)
{
    ps_buf_mgr->i4_used = 0;
}
void hme_init_wkg_mem(buf_mgr_t *ps_buf_mgr, U08 *pu1_mem, S32 size)
{
    ps_buf_mgr->pu1_wkg_mem = pu1_mem;
    ps_buf_mgr->i4_total = size;
    hme_reset_wkg_mem(ps_buf_mgr);
}

void hme_init_mv_grid(mv_grid_t *ps_mv_grid)
{
    S32 i, j;
    search_node_t *ps_search_node;
    /*************************************************************************/
    /* We have a 64x64 CTB in the worst case. For this, we have 16x16 4x4 MVs*/
    /* Additionally, we have 1 neighbour on each side. This makes it a 18x18 */
    /* MV Grid. The boundary of this Grid on all sides are neighbours and the*/
    /* left and top edges of this grid is filled run time. The center portion*/
    /* represents the actual CTB MVs (16x16) and is also filled run time.    */
    /* However, the availability is always set as available (init time)      */
    /*************************************************************************/
    ps_mv_grid->i4_stride = NUM_COLUMNS_IN_CTB_GRID;
    ps_mv_grid->i4_start_offset = ps_mv_grid->i4_stride + CTB_MV_GRID_PAD;
    ps_search_node = &ps_mv_grid->as_node[ps_mv_grid->i4_start_offset];
    for(i = 0; i < 16; i++)
    {
        for(j = 0; j < 16; j++)
        {
            ps_search_node[j].u1_is_avail = 1;
        }

        ps_search_node += ps_mv_grid->i4_stride;
    }
}
/**
********************************************************************************
*  @fn    void hme_pad_left(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht)
*
*  @brief  Pads horizontally to left side. Each pixel replicated across a line
*
*  @param[in] pu1_dst : destination pointer. Points to the pixel to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_wd : Amt of horizontal padding to be done
*
*  @param[in] pad_ht : Number of lines for which horizontal padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_left(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht)
{
    S32 i, j;
    U08 u1_val;
    for(i = 0; i < pad_ht; i++)
    {
        u1_val = pu1_dst[0];
        for(j = -pad_wd; j < 0; j++)
            pu1_dst[j] = u1_val;

        pu1_dst += stride;
    }
}
/**
********************************************************************************
*  @fn    void hme_pad_right(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht)
*
*  @brief  Pads horizontally to rt side. Each pixel replicated across a line
*
*  @param[in] pu1_dst : destination pointer. Points to the pixel to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_wd : Amt of horizontal padding to be done
*
*  @param[in] pad_ht : Number of lines for which horizontal padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_right(U08 *pu1_dst, S32 stride, S32 pad_wd, S32 pad_ht)
{
    S32 i, j;
    U08 u1_val;
    for(i = 0; i < pad_ht; i++)
    {
        u1_val = pu1_dst[0];
        for(j = 1; j <= pad_wd; j++)
            pu1_dst[j] = u1_val;

        pu1_dst += stride;
    }
}
/**
********************************************************************************
*  @fn    void hme_pad_top(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd)
*
*  @brief  Pads vertically on the top. Repeats the top line for top padding
*
*  @param[in] pu1_dst : destination pointer. Points to the line to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_ht : Amt of vertical padding to be done
*
*  @param[in] pad_wd : Number of columns for which vertical padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_top(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd)
{
    S32 i;
    for(i = 1; i <= pad_ht; i++)
        memcpy(pu1_dst - (i * stride), pu1_dst, pad_wd);
}
/**
********************************************************************************
*  @fn    void hme_pad_bot(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd)
*
*  @brief  Pads vertically on the bot. Repeats the top line for top padding
*
*  @param[in] pu1_dst : destination pointer. Points to the line to be repeated
*
*  @param[in] stride : stride of destination buffer
*
*  @param[in] pad_ht : Amt of vertical padding to be done
*
*  @param[in] pad_wd : Number of columns for which vertical padding to be done
*
*  @return void
********************************************************************************
*/
void hme_pad_bot(U08 *pu1_dst, S32 stride, S32 pad_ht, S32 pad_wd)
{
    S32 i;
    for(i = 1; i <= pad_ht; i++)
        memcpy(pu1_dst + (i * stride), pu1_dst, pad_wd);
}

/**
********************************************************************************
*  @fn    void hme_get_wt_inp(layer_ctxt_t *ps_curr_layer,  S32 pos_x,
*                           S32 pos_y, S32 size)
*
*  @brief  Does weighting of the input in case the search needs to happen
*          with reference frames weighted
*
*  @param[in] ps_curr_layer: layer ctxt
*
*  @param[in] pos_x : x coordinate of the input blk in the picture
*
*  @param[in] pos_y : y coordinate of hte input blk in the picture
*
*  @param[in] size : size of the input block
*
*  @param[in] num_ref : Number of reference frames
*
*  @return void
********************************************************************************
*/
void hme_get_wt_inp(
    layer_ctxt_t *ps_curr_layer,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 dst_stride,
    S32 pos_x,
    S32 pos_y,
    S32 size,
    S32 num_ref,
    U08 u1_is_wt_pred_on)
{
    S32 ref, i, j;
    U08 *pu1_src, *pu1_dst, *pu1_src_tmp;
    S32 log_wdc = ps_wt_inp_prms->wpred_log_wdc;
    S32 x_count, y_count;

    /* Fixed source */
    pu1_src = ps_curr_layer->pu1_inp;

    /* Make sure the start positions of block are inside frame limits */
    pos_x = MIN(pos_x, ps_curr_layer->i4_wd - 1);
    pos_y = MIN(pos_y, ps_curr_layer->i4_ht - 1);

    pu1_src += (pos_x + (pos_y * ps_curr_layer->i4_inp_stride));

    /* In case we handle imcomplete CTBs, we copy only as much as reqd */
    /* from input buffers to prevent out of bound accesses. In this    */
    /* case, we do padding in x or y or both dirns */
    x_count = MIN(size, (ps_curr_layer->i4_wd - pos_x));
    y_count = MIN(size, (ps_curr_layer->i4_ht - pos_y));

    for(i = 0; i < num_ref + 1; i++)
    {
        ps_wt_inp_prms->apu1_wt_inp[i] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];
    }

    /* Run thro all ref ids */
    for(ref = 0; ref < num_ref + 1; ref++)
    {
        S32 wt, off;
        S32 inv_wt;

        pu1_src_tmp = pu1_src;

        /* Each ref id may have differnet wt/offset. */
        /* So we have unique inp buf for each ref id */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[ref];

        if(ref == num_ref)
        {
            /* last ref will be non weighted input */
            for(i = 0; i < y_count; i++)
            {
                for(j = 0; j < x_count; j++)
                {
                    pu1_dst[j] = pu1_src_tmp[j];
                }
                pu1_src_tmp += ps_curr_layer->i4_inp_stride;
                pu1_dst += dst_stride;
            }
        }
        else
        {
            /* Wt and off specific to this ref id */
            wt = ps_wt_inp_prms->a_wpred_wt[ref];
            inv_wt = ps_wt_inp_prms->a_inv_wpred_wt[ref];
            off = ps_wt_inp_prms->a_wpred_off[ref];

            /* Generate size*size worth of modified input samples */
            for(i = 0; i < y_count; i++)
            {
                for(j = 0; j < x_count; j++)
                {
                    S32 tmp;

                    /* Since we scale input, we use inverse transform of wt pred */
                    //tmp = HME_INV_WT_PRED(pu1_src_tmp[j], wt, off, log_wdc);
                    tmp = HME_INV_WT_PRED1(pu1_src_tmp[j], inv_wt, off, log_wdc);
                    pu1_dst[j] = (U08)(HME_CLIP(tmp, 0, 255));
                }
                pu1_src_tmp += ps_curr_layer->i4_inp_stride;
                pu1_dst += dst_stride;
            }
        }

        /* Check and do padding in right direction if need be */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[ref];
        if(x_count != size)
        {
            hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
        }

        /* Check and do padding in bottom directino if need be */
        if(y_count != size)
        {
            hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
        }
    }
}
/**
****************************************************************************************
*  @fn     hme_pick_best_pu_cand(pu_result_t *ps_pu_results_dst,
*                                pu_result_t *ps_pu_results_inp,
*                                UWORD8 u1_num_results_per_part,
*                                UWORD8 u1_num_best_cand)
*
*  @brief  Does the candidate evaluation across all the current candidates and returns
*           the best two or one candidates across given lists
*
*  @param[in]  - ps_pu_results_inp : Pointer to the input candidates
*              - u1_num_results_per_part: Number of available candidates
*
*  @param[out] - ps_pu_results_dst : Pointer to best PU results
*
****************************************************************************************
*/
void hme_pick_best_pu_cand(
    pu_result_t *ps_pu_results_dst,
    pu_result_t *ps_pu_results_list0,
    pu_result_t *ps_pu_results_list1,
    UWORD8 u1_num_results_per_part_l0,
    UWORD8 u1_num_results_per_part_l1,
    UWORD8 u1_candidate_rank)
{
    struct cand_pos_data
    {
        U08 u1_cand_list_id;

        U08 u1_cand_id_in_cand_list;
    } as_cand_pos_data[MAX_NUM_RESULTS_PER_PART_LIST << 1];

    S32 ai4_costs[MAX_NUM_RESULTS_PER_PART_LIST << 1];
    U08 i, j;

    for(i = 0; i < u1_num_results_per_part_l0; i++)
    {
        ai4_costs[i] = ps_pu_results_list0[i].i4_tot_cost;
        as_cand_pos_data[i].u1_cand_id_in_cand_list = i;
        as_cand_pos_data[i].u1_cand_list_id = 0;
    }

    for(i = 0, j = u1_num_results_per_part_l0; i < u1_num_results_per_part_l1; i++, j++)
    {
        ai4_costs[j] = ps_pu_results_list1[i].i4_tot_cost;
        as_cand_pos_data[j].u1_cand_id_in_cand_list = i;
        as_cand_pos_data[j].u1_cand_list_id = 1;
    }

    SORT_PRIMARY_INTTYPE_ARRAY_AND_REORDER_GENERIC_COMPANION_ARRAY(
        ai4_costs,
        as_cand_pos_data,
        u1_num_results_per_part_l0 + u1_num_results_per_part_l1,
        struct cand_pos_data);

    if(as_cand_pos_data[u1_candidate_rank].u1_cand_list_id)
    {
        ps_pu_results_dst[0] =
            ps_pu_results_list1[as_cand_pos_data[u1_candidate_rank].u1_cand_id_in_cand_list];
    }
    else
    {
        ps_pu_results_dst[0] =
            ps_pu_results_list0[as_cand_pos_data[u1_candidate_rank].u1_cand_id_in_cand_list];
    }
}

/* Returns the number of candidates */
static S32 hme_tu_recur_cand_harvester(
    part_type_results_t *ps_cand_container,
    inter_pu_results_t *ps_pu_data,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    S32 i4_part_mask)
{
    part_type_results_t s_cand_data;

    U08 i, j;
    PART_ID_T e_part_id;

    S32 i4_num_cands = 0;

    /* 2Nx2N part_type decision part */
    if(i4_part_mask & ENABLE_2Nx2N)
    {
        U08 u1_num_candt_to_pick;

        e_part_id = ge_part_type_to_part_id[PRT_2Nx2N][0];

        ASSERT(ps_inter_ctb_prms->u1_max_2nx2n_tu_recur_cands >= 1);

        if(!ps_inter_ctb_prms->i4_bidir_enabled || (i4_part_mask == ENABLE_2Nx2N))
        {
            u1_num_candt_to_pick =
                MIN(ps_inter_ctb_prms->u1_max_2nx2n_tu_recur_cands,
                    ps_pu_data->u1_num_results_per_part_l0[e_part_id] +
                        ps_pu_data->u1_num_results_per_part_l1[e_part_id]);
        }
        else
        {
            u1_num_candt_to_pick =
                MIN(1,
                    ps_pu_data->u1_num_results_per_part_l0[e_part_id] +
                        ps_pu_data->u1_num_results_per_part_l1[e_part_id]);
        }

        if(ME_XTREME_SPEED_25 == ps_inter_ctb_prms->i1_quality_preset)
        {
            u1_num_candt_to_pick = MIN(u1_num_candt_to_pick, MAX_NUM_TU_RECUR_CANDS_IN_XS25);
        }

        for(i = 0; i < u1_num_candt_to_pick; i++)
        {
            /* Picks the best two candidates of all the available ones */
            hme_pick_best_pu_cand(
                ps_cand_container[i4_num_cands].as_pu_results,
                ps_pu_data->aps_pu_results[0][e_part_id],
                ps_pu_data->aps_pu_results[1][e_part_id],
                ps_pu_data->u1_num_results_per_part_l0[e_part_id],
                ps_pu_data->u1_num_results_per_part_l1[e_part_id],
                i);

            /* Update the other params part_type and total_cost in part_type_results */
            ps_cand_container[i4_num_cands].u1_part_type = e_part_id;
            ps_cand_container[i4_num_cands].i4_tot_cost =
                ps_cand_container[i4_num_cands].as_pu_results->i4_tot_cost;

            i4_num_cands++;
        }
    }

    /* SMP */
    {
        S32 i4_total_cost;

        S32 num_part_types = PRT_Nx2N - PRT_2NxN + 1;
        S32 start_part_type = PRT_2NxN;
        S32 best_cost = MAX_32BIT_VAL;
        S32 part_type_cnt = 0;

        for(j = 0; j < num_part_types; j++)
        {
            if(!(i4_part_mask & gai4_part_type_to_part_mask[j + start_part_type]))
            {
                continue;
            }

            for(i = 0; i < gau1_num_parts_in_part_type[j + start_part_type]; i++)
            {
                e_part_id = ge_part_type_to_part_id[j + start_part_type][i];

                /* Pick the best candidate for the partition acroos lists */
                hme_pick_best_pu_cand(
                    &s_cand_data.as_pu_results[i],
                    ps_pu_data->aps_pu_results[0][e_part_id],
                    ps_pu_data->aps_pu_results[1][e_part_id],
                    ps_pu_data->u1_num_results_per_part_l0[e_part_id],
                    ps_pu_data->u1_num_results_per_part_l1[e_part_id],
                    0);
            }

            i4_total_cost =
                s_cand_data.as_pu_results[0].i4_tot_cost + s_cand_data.as_pu_results[1].i4_tot_cost;

            if(i4_total_cost < best_cost)
            {
                /* Stores the index of the best part_type in the sub-catoegory */
                best_cost = i4_total_cost;

                ps_cand_container[i4_num_cands] = s_cand_data;

                ps_cand_container[i4_num_cands].u1_part_type = j + start_part_type;
                ps_cand_container[i4_num_cands].i4_tot_cost = i4_total_cost;
            }

            part_type_cnt++;
        }

        i4_num_cands = (part_type_cnt) ? (i4_num_cands + 1) : i4_num_cands;
    }

    /* AMP */
    {
        S32 i4_total_cost;

        S32 num_part_types = PRT_nRx2N - PRT_2NxnU + 1;
        S32 start_part_type = PRT_2NxnU;
        S32 best_cost = MAX_32BIT_VAL;
        S32 part_type_cnt = 0;

        for(j = 0; j < num_part_types; j++)
        {
            if(!(i4_part_mask & gai4_part_type_to_part_mask[j + start_part_type]))
            {
                continue;
            }

            for(i = 0; i < gau1_num_parts_in_part_type[j + start_part_type]; i++)
            {
                e_part_id = ge_part_type_to_part_id[j + start_part_type][i];

                /* Pick the best candidate for the partition acroos lists */
                hme_pick_best_pu_cand(
                    &s_cand_data.as_pu_results[i],
                    ps_pu_data->aps_pu_results[0][e_part_id],
                    ps_pu_data->aps_pu_results[1][e_part_id],
                    ps_pu_data->u1_num_results_per_part_l0[e_part_id],
                    ps_pu_data->u1_num_results_per_part_l1[e_part_id],
                    0);
            }

            i4_total_cost =
                s_cand_data.as_pu_results[0].i4_tot_cost + s_cand_data.as_pu_results[1].i4_tot_cost;

            if(i4_total_cost < best_cost)
            {
                /* Stores the index of the best part_type in the sub-catoegory */
                best_cost = i4_total_cost;

                ps_cand_container[i4_num_cands] = s_cand_data;

                ps_cand_container[i4_num_cands].u1_part_type = j + start_part_type;
                ps_cand_container[i4_num_cands].i4_tot_cost = i4_total_cost;
            }

            part_type_cnt++;
        }

        i4_num_cands = (part_type_cnt) ? (i4_num_cands + 1) : i4_num_cands;
    }

    return i4_num_cands;
}

/**
*****************************************************************************
*  @fn     hme_decide_part_types(search_results_t *ps_search_results)
*
*  @brief  Does uni/bi evaluation accross various partition types,
*          decides best inter partition types for the CU, compares
*          intra cost and decides the best K results for the CU
*
*          This is called post subpel refinmenent for 16x16s, 8x8s and
*          for post merge evaluation for 32x32,64x64 CUs
*
*  @param[in,out] ps_search_results : Search results data structure
*                 - In : 2 lists of upto 2mvs & refids, active partition mask
*                 - Out: Best results for final rdo evaluation of the cu
*
*  @param[in]     ps_subpel_prms : Sub pel params data structure
*
*
*  @par Description
*    --------------------------------------------------------------------------------
*     Flow:
*            for each category (SMP,AMP,2Nx2N based on part mask)
*            {
*                for each part_type
*                {
*                    for each part
*                        pick best candidate from each list
*                    combine uni part type
*                    update best results for part type
*                }
*                pick the best part type for given category (for SMP & AMP)
*            }
*                    ||
*                    ||
*                    \/
*           Bi-Pred evaluation:
*            for upto 4 best part types
*            {
*                for each part
*                {
*                    compute fixed size had for all uni and remember coeffs
*                    compute bisatd
*                    uni vs bi and gives upto two results
*                    also gives the pt level pred buffer
*                }
*             }
*                    ||
*                    ||
*                    \/
*            select X candidates for tu recursion as per the Note below
*               tu_rec_on_part_type (reuse transform coeffs)
*                    ||
*                    ||
*                    \/
*            insert intra nodes at appropriate result id
*                    ||
*                    ||
*                    \/
*            populate y best resuls for rdo based on preset
*
*     Note :
*     number of TU rec for P pics : 2 2nx2n + 1 smp + 1 amp for ms or 9 for hq
*     number of TU rec for B pics : 1 2nx2n + 1 smp + 1 amp for ms or 2 uni 2nx2n + 1 smp + 1 amp for ms or 9 for hq
*     --------------------------------------------------------------------------------
*
*  @return None
********************************************************************************
*/
void hme_decide_part_types(
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    me_frm_ctxt_t *ps_ctxt,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list

)
{
    S32 i, j;
    S32 i4_part_mask;
    ULWORD64 au8_pred_sigmaXSquare[NUM_BEST_ME_OUTPUTS][NUM_INTER_PU_PARTS];
    ULWORD64 au8_pred_sigmaX[NUM_BEST_ME_OUTPUTS][NUM_INTER_PU_PARTS];
    S32 i4_noise_term;
    WORD32 e_part_id;

    PF_SAD_FXN_TU_REC apf_err_compute[4];

    part_type_results_t as_part_type_results[NUM_BEST_ME_OUTPUTS];
    part_type_results_t *ps_part_type_results;

    S32 num_best_cand = 0;
    const S32 i4_default_src_wt = ((1 << 15) + (WGHT_DEFAULT >> 1)) / WGHT_DEFAULT;

    i4_part_mask = ps_cu_results->i4_part_mask;

    num_best_cand = hme_tu_recur_cand_harvester(
        as_part_type_results, ps_pu_results, ps_inter_ctb_prms, i4_part_mask);

    /* Partition ID for the current PU */
    e_part_id = (UWORD8)ge_part_type_to_part_id[PRT_2Nx2N][0];

    ps_part_type_results = as_part_type_results;
    for(i = 0; i < num_best_cand; i++)
    {
        hme_compute_pred_and_evaluate_bi(
            ps_cu_results,
            ps_pu_results,
            ps_inter_ctb_prms,
            &(ps_part_type_results[i]),
            au8_pred_sigmaXSquare[i],
            au8_pred_sigmaX[i],
            ps_cmn_utils_optimised_function_list,
            ps_me_optimised_function_list

        );
    }
    /* Perform TU_REC on the best candidates selected */
    {
        WORD32 i4_sad_grid;
        WORD32 ai4_tu_split_flag[4];
        WORD32 ai4_tu_early_cbf[4];

        WORD32 best_cost[NUM_BEST_ME_OUTPUTS];
        WORD32 ai4_final_idx[NUM_BEST_ME_OUTPUTS];
        WORD16 i2_wght;
        WORD32 i4_satd;

        err_prms_t s_err_prms;
        err_prms_t *ps_err_prms = &s_err_prms;

        /* Default cost and final idx initialization */
        for(i = 0; i < num_best_cand; i++)
        {
            best_cost[i] = MAX_32BIT_VAL;
            ai4_final_idx[i] = -1;
        }

        /* Assign the stad function to the err_compute function pointer :
        Implemented only for 32x32 and 64x64, hence 16x16 and 8x8 are kept NULL */
        apf_err_compute[CU_64x64] = hme_evalsatd_pt_pu_64x64_tu_rec;
        apf_err_compute[CU_32x32] = hme_evalsatd_pt_pu_32x32_tu_rec;
        apf_err_compute[CU_16x16] = hme_evalsatd_pt_pu_16x16_tu_rec;
        apf_err_compute[CU_8x8] = hme_evalsatd_pt_pu_8x8_tu_rec;

        ps_err_prms->pi4_sad_grid = &i4_sad_grid;
        ps_err_prms->pi4_tu_split_flags = ai4_tu_split_flag;
        ps_err_prms->u1_max_tr_depth = ps_inter_ctb_prms->u1_max_tr_depth;
        ps_err_prms->pi4_tu_early_cbf = ai4_tu_early_cbf;
        ps_err_prms->i4_grid_mask = 1;
        ps_err_prms->pu1_wkg_mem = ps_inter_ctb_prms->pu1_wkg_mem;
        ps_err_prms->u1_max_tr_size = 32;

        if(ps_inter_ctb_prms->u1_is_cu_noisy)
        {
            ps_err_prms->u1_max_tr_size = MAX_TU_SIZE_WHEN_NOISY;
        }

        /* TU_REC for the best candidates, as mentioned in NOTE above (except candidates that
        are disabled by Part_mask */
        for(i = 0; i < num_best_cand; i++)
        {
            part_type_results_t *ps_best_results;
            pu_result_t *ps_pu_result;
            WORD32 part_type_cost;
            WORD32 cand_idx;

            WORD32 pred_dir;
            S32 i4_inp_off;

            S32 lambda;
            U08 lambda_qshift;
            U08 *apu1_inp[MAX_NUM_INTER_PARTS];
            S16 ai2_wt[MAX_NUM_INTER_PARTS];
            S32 ai4_inv_wt[MAX_NUM_INTER_PARTS];
            S32 ai4_inv_wt_shift_val[MAX_NUM_INTER_PARTS];

            WORD32 part_type = ps_part_type_results[i].u1_part_type;
            WORD32 e_cu_size = ps_cu_results->u1_cu_size;
            WORD32 e_blk_size = ge_cu_size_to_blk_size[e_cu_size];
            U08 u1_num_parts = gau1_num_parts_in_part_type[part_type];
            U08 u1_inp_buf_idx = UCHAR_MAX;

            ps_err_prms->i4_part_mask = i4_part_mask;
            ps_err_prms->i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
            ps_err_prms->i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];
            ps_err_prms->pu1_ref = ps_part_type_results[i].pu1_pred;
            ps_err_prms->i4_ref_stride = ps_part_type_results[i].i4_pred_stride;

            /* Current offset for the present part type */
            i4_inp_off = ps_cu_results->i4_inp_offset;

            ps_best_results = &(ps_part_type_results[i]);

            part_type_cost = 0;
            lambda = ps_inter_ctb_prms->i4_lamda;
            lambda_qshift = ps_inter_ctb_prms->u1_lamda_qshift;

            for(j = 0; j < u1_num_parts; j++)
            {
                ps_pu_result = &(ps_best_results->as_pu_results[j]);

                pred_dir = ps_pu_result->pu.b2_pred_mode;

                if(PRED_L0 == pred_dir)
                {
                    apu1_inp[j] =
                        ps_inter_ctb_prms->apu1_wt_inp[PRED_L0][ps_pu_result->pu.mv.i1_l0_ref_idx] +
                        i4_inp_off;
                    ai2_wt[j] =
                        ps_inter_ctb_prms->pps_rec_list_l0[ps_pu_result->pu.mv.i1_l0_ref_idx]
                            ->s_weight_offset.i2_luma_weight;
                    ai4_inv_wt[j] =
                        ps_inter_ctb_prms->pi4_inv_wt
                            [ps_inter_ctb_prms->pi1_past_list[ps_pu_result->pu.mv.i1_l0_ref_idx]];
                    ai4_inv_wt_shift_val[j] =
                        ps_inter_ctb_prms->pi4_inv_wt_shift_val
                            [ps_inter_ctb_prms->pi1_past_list[ps_pu_result->pu.mv.i1_l0_ref_idx]];
                }
                else if(PRED_L1 == pred_dir)
                {
                    apu1_inp[j] =
                        ps_inter_ctb_prms->apu1_wt_inp[PRED_L1][ps_pu_result->pu.mv.i1_l1_ref_idx] +
                        i4_inp_off;
                    ai2_wt[j] =
                        ps_inter_ctb_prms->pps_rec_list_l1[ps_pu_result->pu.mv.i1_l1_ref_idx]
                            ->s_weight_offset.i2_luma_weight;
                    ai4_inv_wt[j] =
                        ps_inter_ctb_prms->pi4_inv_wt
                            [ps_inter_ctb_prms->pi1_future_list[ps_pu_result->pu.mv.i1_l1_ref_idx]];
                    ai4_inv_wt_shift_val[j] =
                        ps_inter_ctb_prms->pi4_inv_wt_shift_val
                            [ps_inter_ctb_prms->pi1_future_list[ps_pu_result->pu.mv.i1_l1_ref_idx]];
                }
                else if(PRED_BI == pred_dir)
                {
                    apu1_inp[j] = ps_inter_ctb_prms->pu1_non_wt_inp + i4_inp_off;
                    ai2_wt[j] = 1 << ps_inter_ctb_prms->wpred_log_wdc;
                    ai4_inv_wt[j] = i4_default_src_wt;
                    ai4_inv_wt_shift_val[j] = 0;
                }
                else
                {
                    ASSERT(0);
                }

                part_type_cost += ps_pu_result->i4_mv_cost;
            }

            if((u1_num_parts == 1) || (ai2_wt[0] == ai2_wt[1]))
            {
                ps_err_prms->pu1_inp = apu1_inp[0];
                ps_err_prms->i4_inp_stride = ps_inter_ctb_prms->i4_inp_stride;
                i2_wght = ai2_wt[0];
            }
            else
            {
                if(1 != ihevce_get_free_pred_buf_indices(
                            &u1_inp_buf_idx,
                            &ps_inter_ctb_prms->s_pred_buf_mngr.u4_pred_buf_usage_indicator,
                            1))
                {
                    ASSERT(0);
                }
                else
                {
                    U08 *pu1_dst =
                        ps_inter_ctb_prms->s_pred_buf_mngr.apu1_pred_bufs[u1_inp_buf_idx];
                    U08 *pu1_src = apu1_inp[0];
                    U08 u1_pu1_wd = (ps_part_type_results[i].as_pu_results[0].pu.b4_wd + 1) << 2;
                    U08 u1_pu1_ht = (ps_part_type_results[i].as_pu_results[0].pu.b4_ht + 1) << 2;
                    U08 u1_pu2_wd = (ps_part_type_results[i].as_pu_results[1].pu.b4_wd + 1) << 2;
                    U08 u1_pu2_ht = (ps_part_type_results[i].as_pu_results[1].pu.b4_ht + 1) << 2;

                    ps_cmn_utils_optimised_function_list->pf_copy_2d(
                        pu1_dst,
                        MAX_CU_SIZE,
                        pu1_src,
                        ps_inter_ctb_prms->i4_inp_stride,
                        u1_pu1_wd,
                        u1_pu1_ht);

                    pu1_dst +=
                        (gai1_is_part_vertical[ge_part_type_to_part_id[part_type][0]]
                             ? u1_pu1_ht * MAX_CU_SIZE
                             : u1_pu1_wd);
                    pu1_src =
                        apu1_inp[1] + (gai1_is_part_vertical[ge_part_type_to_part_id[part_type][0]]
                                           ? u1_pu1_ht * ps_inter_ctb_prms->i4_inp_stride
                                           : u1_pu1_wd);

                    ps_cmn_utils_optimised_function_list->pf_copy_2d(
                        pu1_dst,
                        MAX_CU_SIZE,
                        pu1_src,
                        ps_inter_ctb_prms->i4_inp_stride,
                        u1_pu2_wd,
                        u1_pu2_ht);

                    ps_err_prms->pu1_inp =
                        ps_inter_ctb_prms->s_pred_buf_mngr.apu1_pred_bufs[u1_inp_buf_idx];
                    ps_err_prms->i4_inp_stride = MAX_CU_SIZE;
                    i2_wght = ai2_wt[1];
                }
            }

#if !DISABLE_TU_RECURSION
            i4_satd = apf_err_compute[e_cu_size](
                ps_err_prms,
                lambda,
                lambda_qshift,
                ps_inter_ctb_prms->i4_qstep_ls8,
                ps_ctxt->ps_func_selector);
#else
            ps_err_prms->pi4_sad_grid = &i4_satd;

            pf_err_compute(ps_err_prms);

            if((part_type == PRT_2Nx2N) || (e_cu_size != CU_64x64))
            {
                ai4_tu_split_flag[0] = 1;
                ai4_tu_split_flag[1] = 1;
                ai4_tu_split_flag[2] = 1;
                ai4_tu_split_flag[3] = 1;

                ps_err_prms->i4_tu_split_cost = 0;
            }
            else
            {
                ai4_tu_split_flag[0] = 1;
                ai4_tu_split_flag[1] = 1;
                ai4_tu_split_flag[2] = 1;
                ai4_tu_split_flag[3] = 1;

                ps_err_prms->i4_tu_split_cost = 0;
            }
#endif

#if UNI_SATD_SCALE
            i4_satd = (i4_satd * i2_wght) >> ps_inter_ctb_prms->wpred_log_wdc;
#endif

            if(ps_inter_ctb_prms->u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
            {
                ULWORD64 u8_temp_var, u8_temp_var1, u8_pred_sigmaSquaredX;
                ULWORD64 u8_src_variance, u8_pred_variance;
                unsigned long u4_shift_val;
                S32 i4_bits_req;
                S32 i4_q_level = STIM_Q_FORMAT + ALPHA_Q_FORMAT;

                if(1 == u1_num_parts)
                {
                    u8_pred_sigmaSquaredX = au8_pred_sigmaX[i][0] * au8_pred_sigmaX[i][0];
                    u8_pred_variance = au8_pred_sigmaXSquare[i][0] - u8_pred_sigmaSquaredX;

                    if(e_cu_size == CU_8x8)
                    {
                        PART_ID_T e_part_id = (PART_ID_T)(
                            (PART_ID_NxN_TL) + (ps_cu_results->u1_x_off & 1) +
                            ((ps_cu_results->u1_y_off & 1) << 1));

                        u4_shift_val = ihevce_calc_stim_injected_variance(
                            ps_inter_ctb_prms->pu8_part_src_sigmaX,
                            ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                            &u8_src_variance,
                            ai4_inv_wt[0],
                            ai4_inv_wt_shift_val[0],
                            ps_inter_ctb_prms->wpred_log_wdc,
                            e_part_id);
                    }
                    else
                    {
                        u4_shift_val = ihevce_calc_stim_injected_variance(
                            ps_inter_ctb_prms->pu8_part_src_sigmaX,
                            ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                            &u8_src_variance,
                            ai4_inv_wt[0],
                            ai4_inv_wt_shift_val[0],
                            ps_inter_ctb_prms->wpred_log_wdc,
                            e_part_id);
                    }

                    u8_pred_variance = u8_pred_variance >> u4_shift_val;

                    GETRANGE64(i4_bits_req, u8_pred_variance);

                    if(i4_bits_req > 27)
                    {
                        u8_pred_variance = u8_pred_variance >> (i4_bits_req - 27);
                        u8_src_variance = u8_src_variance >> (i4_bits_req - 27);
                    }

                    if(u8_src_variance == u8_pred_variance)
                    {
                        u8_temp_var = (1 << STIM_Q_FORMAT);
                    }
                    else
                    {
                        u8_temp_var = (2 * u8_src_variance * u8_pred_variance);
                        u8_temp_var = (u8_temp_var * (1 << STIM_Q_FORMAT));
                        u8_temp_var1 = (u8_src_variance * u8_src_variance) +
                                       (u8_pred_variance * u8_pred_variance);
                        u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                        u8_temp_var = (u8_temp_var / u8_temp_var1);
                    }

                    i4_noise_term = (UWORD32)u8_temp_var;

                    ASSERT(i4_noise_term >= 0);

                    i4_noise_term *= ps_inter_ctb_prms->i4_alpha_stim_multiplier;

                    u8_temp_var = i4_satd;
                    u8_temp_var *= ((1 << (i4_q_level)) - (i4_noise_term));
                    u8_temp_var += (1 << ((i4_q_level)-1));
                    i4_satd = (UWORD32)(u8_temp_var >> (i4_q_level));
                }
                else /*if(e_cu_size <= CU_16x16)*/
                {
                    unsigned long temp_shift_val;
                    PART_ID_T ae_part_id[MAX_NUM_INTER_PARTS] = {
                        ge_part_type_to_part_id[part_type][0], ge_part_type_to_part_id[part_type][1]
                    };

                    u4_shift_val = ihevce_calc_variance_for_diff_weights(
                        ps_inter_ctb_prms->pu8_part_src_sigmaX,
                        ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                        &u8_src_variance,
                        ai4_inv_wt,
                        ai4_inv_wt_shift_val,
                        ps_best_results->as_pu_results,
                        ps_inter_ctb_prms->wpred_log_wdc,
                        ae_part_id,
                        gau1_blk_size_to_wd[e_blk_size],
                        u1_num_parts,
                        1);

                    temp_shift_val = u4_shift_val;

                    u4_shift_val = ihevce_calc_variance_for_diff_weights(
                        au8_pred_sigmaX[i],
                        au8_pred_sigmaXSquare[i],
                        &u8_pred_variance,
                        ai4_inv_wt,
                        ai4_inv_wt_shift_val,
                        ps_best_results->as_pu_results,
                        0,
                        ae_part_id,
                        gau1_blk_size_to_wd[e_blk_size],
                        u1_num_parts,
                        0);

                    u8_pred_variance = u8_pred_variance >> temp_shift_val;

                    GETRANGE64(i4_bits_req, u8_pred_variance);

                    if(i4_bits_req > 27)
                    {
                        u8_pred_variance = u8_pred_variance >> (i4_bits_req - 27);
                        u8_src_variance = u8_src_variance >> (i4_bits_req - 27);
                    }

                    if(u8_src_variance == u8_pred_variance)
                    {
                        u8_temp_var = (1 << STIM_Q_FORMAT);
                    }
                    else
                    {
                        u8_temp_var = (2 * u8_src_variance * u8_pred_variance);
                        u8_temp_var = (u8_temp_var * (1 << STIM_Q_FORMAT));
                        u8_temp_var1 = (u8_src_variance * u8_src_variance) +
                                       (u8_pred_variance * u8_pred_variance);
                        u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                        u8_temp_var = (u8_temp_var / u8_temp_var1);
                    }

                    i4_noise_term = (UWORD32)u8_temp_var;

                    ASSERT(i4_noise_term >= 0);
                    ASSERT(i4_noise_term <= (1 << (STIM_Q_FORMAT + ALPHA_Q_FORMAT)));

                    i4_noise_term *= ps_inter_ctb_prms->i4_alpha_stim_multiplier;

                    u8_temp_var = i4_satd;
                    u8_temp_var *= ((1 << (i4_q_level)) - (i4_noise_term));
                    u8_temp_var += (1 << ((i4_q_level)-1));
                    i4_satd = (UWORD32)(u8_temp_var >> (i4_q_level));

                    ASSERT(i4_satd >= 0);
                }
            }

            if(u1_inp_buf_idx != UCHAR_MAX)
            {
                ihevce_set_pred_buf_as_free(
                    &ps_inter_ctb_prms->s_pred_buf_mngr.u4_pred_buf_usage_indicator,
                    u1_inp_buf_idx);
            }

            part_type_cost += i4_satd;

            /*Update the best results with the new results */
            ps_best_results->i4_tot_cost = part_type_cost;

            ps_best_results->i4_tu_split_cost = ps_err_prms->i4_tu_split_cost;

            ASSERT(ai4_tu_split_flag[0] >= 0);
            if(e_cu_size == CU_64x64)
            {
                ps_best_results->ai4_tu_split_flag[0] = ai4_tu_split_flag[0];
                ps_best_results->ai4_tu_split_flag[1] = ai4_tu_split_flag[1];
                ps_best_results->ai4_tu_split_flag[2] = ai4_tu_split_flag[2];
                ps_best_results->ai4_tu_split_flag[3] = ai4_tu_split_flag[3];

                /* Update the TU early cbf flags into the best results structure */
                ps_best_results->ai4_tu_early_cbf[0] = ai4_tu_early_cbf[0];
                ps_best_results->ai4_tu_early_cbf[1] = ai4_tu_early_cbf[1];
                ps_best_results->ai4_tu_early_cbf[2] = ai4_tu_early_cbf[2];
                ps_best_results->ai4_tu_early_cbf[3] = ai4_tu_early_cbf[3];
            }
            else
            {
                ps_best_results->ai4_tu_split_flag[0] = ai4_tu_split_flag[0];
                ps_best_results->ai4_tu_early_cbf[0] = ai4_tu_early_cbf[0];
            }

            if(part_type_cost < best_cost[num_best_cand - 1])
            {
                /* Push and sort current part type if it is one of the num_best_cand */
                for(cand_idx = 0; cand_idx < i; cand_idx++)
                {
                    if(part_type_cost <= best_cost[cand_idx])
                    {
                        memmove(
                            &ai4_final_idx[cand_idx + 1],
                            &ai4_final_idx[cand_idx],
                            sizeof(WORD32) * (i - cand_idx));
                        memmove(
                            &best_cost[cand_idx + 1],
                            &best_cost[cand_idx],
                            sizeof(WORD32) * (i - cand_idx));
                        break;
                    }
                }

                ai4_final_idx[cand_idx] = i;
                best_cost[cand_idx] = part_type_cost;
            }
        }

        ps_cu_results->u1_num_best_results = num_best_cand;

        for(i = 0; i < num_best_cand; i++)
        {
            ASSERT(ai4_final_idx[i] < num_best_cand);

            if(ai4_final_idx[i] != -1)
            {
                memcpy(
                    &(ps_cu_results->ps_best_results[i]),
                    &(ps_part_type_results[ai4_final_idx[i]]),
                    sizeof(part_type_results_t));
            }
        }
    }

    for(i = 0; i < (MAX_NUM_PRED_BUFS_USED_FOR_PARTTYPE_DECISIONS)-2; i++)
    {
        ihevce_set_pred_buf_as_free(
            &ps_inter_ctb_prms->s_pred_buf_mngr.u4_pred_buf_usage_indicator, i);
    }
}

/**
**************************************************************************************************
*  @fn     hme_populate_pus(search_results_t *ps_search_results, inter_cu_results_t *ps_cu_results)
*
*  @brief Does the population of the inter_cu_results structure with the results after the
*           subpel refinement
*
*          This is called post subpel refinmenent for 16x16s, 8x8s and
*          for post merge evaluation for 32x32,64x64 CUs
*
*  @param[in,out] ps_search_results : Search results data structure
*                 - ps_cu_results : cu_results data structure
*                   ps_pu_result  : Pointer to the memory for storing PU's
*
****************************************************************************************************
*/
void hme_populate_pus(
    me_ctxt_t *ps_thrd_ctxt,
    me_frm_ctxt_t *ps_ctxt,
    hme_subpel_prms_t *ps_subpel_prms,
    search_results_t *ps_search_results,
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    pu_result_t *ps_pu_result,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    wgt_pred_ctxt_t *ps_wt_prms,
    layer_ctxt_t *ps_curr_layer,
    U08 *pu1_pred_dir_searched,
    WORD32 i4_num_active_ref)
{
    WORD32 i, j, k;
    WORD32 i4_part_mask;
    WORD32 i4_ref;
    UWORD8 e_part_id;
    pu_result_t *ps_curr_pu;
    search_node_t *ps_search_node;
    part_attr_t *ps_part_attr;
    UWORD8 e_cu_size = ps_search_results->e_cu_size;
    WORD32 num_results_per_part_l0 = 0;
    WORD32 num_results_per_part_l1 = 0;
    WORD32 i4_ref_id;
    WORD32 i4_total_act_ref;

    i4_part_mask = ps_search_results->i4_part_mask;

    /* pred_buf_mngr init */
    {
        hme_get_wkg_mem(&ps_ctxt->s_buf_mgr, MAX_WKG_MEM_SIZE_PER_THREAD);

        ps_inter_ctb_prms->s_pred_buf_mngr.u4_pred_buf_usage_indicator = UINT_MAX;

        for(i = 0; i < MAX_NUM_PRED_BUFS_USED_FOR_PARTTYPE_DECISIONS - 2; i++)
        {
            ps_inter_ctb_prms->s_pred_buf_mngr.apu1_pred_bufs[i] =
                ps_ctxt->s_buf_mgr.pu1_wkg_mem + i * INTERP_OUT_BUF_SIZE;
            ps_inter_ctb_prms->s_pred_buf_mngr.u4_pred_buf_usage_indicator &= ~(1 << i);
        }

        ps_inter_ctb_prms->pu1_wkg_mem = ps_ctxt->s_buf_mgr.pu1_wkg_mem + i * INTERP_OUT_BUF_SIZE;
    }

    ps_inter_ctb_prms->i4_alpha_stim_multiplier = ALPHA_FOR_NOISE_TERM_IN_ME;
    ps_inter_ctb_prms->u1_is_cu_noisy = ps_subpel_prms->u1_is_cu_noisy;
    ps_inter_ctb_prms->i4_lamda = ps_search_results->as_pred_ctxt[0].lambda;

    /* Populate the CU level parameters */
    ps_cu_results->u1_cu_size = ps_search_results->e_cu_size;
    ps_cu_results->u1_num_best_results = ps_search_results->u1_num_best_results;
    ps_cu_results->i4_part_mask = ps_search_results->i4_part_mask;
    ps_cu_results->u1_x_off = ps_search_results->u1_x_off;
    ps_cu_results->u1_y_off = ps_search_results->u1_y_off;

    i4_total_act_ref =
        ps_ctxt->s_frm_prms.u1_num_active_ref_l0 + ps_ctxt->s_frm_prms.u1_num_active_ref_l1;
    /*Populate the partition results
    Loop across all the active references that are enabled right now */
    for(i = 0; i < MAX_PART_TYPES; i++)
    {
        if(!(i4_part_mask & gai4_part_type_to_part_mask[i]))
        {
            continue;
        }

        for(j = 0; j < gau1_num_parts_in_part_type[i]; j++)
        {
            /* Partition ID for the current PU */
            e_part_id = (UWORD8)ge_part_type_to_part_id[i][j];
            ps_part_attr = &gas_part_attr_in_cu[e_part_id];

            num_results_per_part_l0 = 0;
            num_results_per_part_l1 = 0;

            ps_pu_results->aps_pu_results[0][e_part_id] =
                ps_pu_result + (e_part_id * MAX_NUM_RESULTS_PER_PART_LIST);
            ps_pu_results->aps_pu_results[1][e_part_id] =
                ps_pu_result + ((e_part_id + TOT_NUM_PARTS) * MAX_NUM_RESULTS_PER_PART_LIST);

            for(i4_ref = 0; i4_ref < i4_num_active_ref; i4_ref++)
            {
                U08 u1_pred_dir = pu1_pred_dir_searched[i4_ref];

                for(k = 0; k < ps_search_results->u1_num_results_per_part; k++)
                {
                    ps_search_node =
                        &ps_search_results->aps_part_results[u1_pred_dir][e_part_id][k];

                    /* If subpel is done then the node is a valid candidate else break the loop */
                    if(ps_search_node->u1_subpel_done)
                    {
                        i4_ref_id = ps_search_node->i1_ref_idx;

                        ASSERT(i4_ref_id >= 0);

                        /* Check whether current ref_id is past or future and assign the pointers to L0 or L1 list accordingly */
                        if(!u1_pred_dir)
                        {
                            ps_curr_pu = ps_pu_results->aps_pu_results[0][e_part_id] +
                                         num_results_per_part_l0;

                            ASSERT(
                                ps_ctxt->a_ref_idx_lc_to_l0[i4_ref_id] <
                                ps_inter_ctb_prms->u1_num_active_ref_l0);

                            /* Always populate the ref_idx value in l0_ref_idx */
                            ps_curr_pu->pu.mv.i1_l0_ref_idx =
                                ps_ctxt->a_ref_idx_lc_to_l0[i4_ref_id];
                            ps_curr_pu->pu.mv.s_l0_mv = ps_search_node->s_mv;
                            ps_curr_pu->pu.mv.i1_l1_ref_idx = -1;
                            ps_curr_pu->pu.b2_pred_mode = PRED_L0;

                            ps_inter_ctb_prms->apu1_wt_inp[0][ps_curr_pu->pu.mv.i1_l0_ref_idx] =
                                ps_wt_prms->apu1_wt_inp[i4_ref_id];

                            num_results_per_part_l0++;
                        }
                        else
                        {
                            ps_curr_pu = ps_pu_results->aps_pu_results[1][e_part_id] +
                                         num_results_per_part_l1;

                            ASSERT(
                                ps_ctxt->a_ref_idx_lc_to_l1[i4_ref_id] <
                                ps_inter_ctb_prms->u1_num_active_ref_l1);

                            /* populate the ref_idx value in l1_ref_idx */
                            ps_curr_pu->pu.mv.i1_l1_ref_idx =
                                ps_ctxt->a_ref_idx_lc_to_l1[i4_ref_id];
                            ps_curr_pu->pu.mv.s_l1_mv = ps_search_node->s_mv;
                            ps_curr_pu->pu.mv.i1_l0_ref_idx = -1;
                            ps_curr_pu->pu.b2_pred_mode = PRED_L1;

                            /* Copy the values from weighted params to common_frm_aprams */
                            ps_inter_ctb_prms->apu1_wt_inp[1][ps_curr_pu->pu.mv.i1_l1_ref_idx] =
                                ps_wt_prms->apu1_wt_inp[i4_ref_id];

                            num_results_per_part_l1++;
                        }
                        ps_curr_pu->i4_mv_cost = ps_search_node->i4_mv_cost;
                        ps_curr_pu->i4_sdi = ps_search_node->i4_sdi;

#if UNI_SATD_SCALE
                        /*SATD is scaled by weight. Hence rescale the SATD */
                        ps_curr_pu->i4_tot_cost =
                            ((ps_search_node->i4_sad *
                                  ps_ctxt->s_wt_pred.a_wpred_wt[ps_search_node->i1_ref_idx] +
                              (1 << (ps_inter_ctb_prms->wpred_log_wdc - 1))) >>
                             ps_inter_ctb_prms->wpred_log_wdc) +
                            ps_search_node->i4_mv_cost;
#endif

                        /* Packed format of the width and height */
                        ps_curr_pu->pu.b4_wd = ((ps_part_attr->u1_x_count << e_cu_size) >> 2) - 1;
                        ps_curr_pu->pu.b4_ht = ((ps_part_attr->u1_y_count << e_cu_size) >> 2) - 1;

                        ps_curr_pu->pu.b4_pos_x =
                            (((ps_part_attr->u1_x_start << e_cu_size) + ps_cu_results->u1_x_off) >>
                             2);
                        ps_curr_pu->pu.b4_pos_y =
                            (((ps_part_attr->u1_y_start << e_cu_size) + ps_cu_results->u1_y_off) >>
                             2);

                        ps_curr_pu->pu.b1_intra_flag = 0;

                        /* Unweighted input */
                        ps_inter_ctb_prms->pu1_non_wt_inp =
                            ps_wt_prms->apu1_wt_inp[i4_total_act_ref];

                        ps_search_node++;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            ps_pu_results->u1_num_results_per_part_l0[e_part_id] = num_results_per_part_l0;
            ps_pu_results->u1_num_results_per_part_l1[e_part_id] = num_results_per_part_l1;
        }
    }
}

/**
*********************************************************************************************************
*  @fn     hme_populate_pus_8x8_cu(search_results_t *ps_search_results, inter_cu_results_t *ps_cu_results)
*
*  @brief Does the population of the inter_cu_results structure with the results after the
*           subpel refinement
*
*          This is called post subpel refinmenent for 16x16s, 8x8s and
*          for post merge evaluation for 32x32,64x64 CUs
*
*  @param[in,out] ps_search_results : Search results data structure
*                 - ps_cu_results : cu_results data structure
*                   ps_pu_results : Pointer for the PU's
*                   ps_pu_result  : Pointer to the memory for storing PU's
*
*********************************************************************************************************
*/
void hme_populate_pus_8x8_cu(
    me_ctxt_t *ps_thrd_ctxt,
    me_frm_ctxt_t *ps_ctxt,
    hme_subpel_prms_t *ps_subpel_prms,
    search_results_t *ps_search_results,
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    pu_result_t *ps_pu_result,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    U08 *pu1_pred_dir_searched,
    WORD32 i4_num_active_ref,
    U08 u1_blk_8x8_mask)
{
    WORD32 i, k;
    WORD32 i4_part_mask;
    WORD32 i4_ref;
    pu_result_t *ps_curr_pu;
    search_node_t *ps_search_node;
    WORD32 i4_ref_id;
    WORD32 x_off, y_off;

    /* Make part mask available as only 2Nx2N
    Later support for 4x8 and 8x4 needs to be added */
    i4_part_mask = ENABLE_2Nx2N;

    x_off = ps_search_results->u1_x_off;
    y_off = ps_search_results->u1_y_off;

    for(i = 0; i < 4; i++)
    {
        if(u1_blk_8x8_mask & (1 << i))
        {
            UWORD8 u1_x_pos, u1_y_pos;

            WORD32 num_results_per_part_l0 = 0;
            WORD32 num_results_per_part_l1 = 0;

            ps_cu_results->u1_cu_size = CU_8x8;
            ps_cu_results->u1_num_best_results = ps_search_results->u1_num_best_results;
            ps_cu_results->i4_part_mask = i4_part_mask;
            ps_cu_results->u1_x_off = x_off + (i & 1) * 8;
            ps_cu_results->u1_y_off = y_off + (i >> 1) * 8;
            ps_cu_results->i4_inp_offset = ps_cu_results->u1_x_off + (ps_cu_results->u1_y_off * 64);

            ps_cu_results->ps_best_results[0].i4_tot_cost = MAX_32BIT_VAL;
            ps_cu_results->ps_best_results[0].i4_tu_split_cost = 0;

            u1_x_pos = ps_cu_results->u1_x_off >> 2;
            u1_y_pos = ps_cu_results->u1_y_off >> 2;

            if(!(ps_search_results->i4_part_mask & ENABLE_NxN))
            {
                ps_curr_pu = &ps_cu_results->ps_best_results[0].as_pu_results[0];

                ps_cu_results->i4_part_mask = 0;
                ps_cu_results->u1_num_best_results = 0;

                ps_curr_pu->i4_tot_cost = MAX_32BIT_VAL;

                ps_curr_pu->pu.b4_wd = 1;
                ps_curr_pu->pu.b4_ht = 1;
                ps_curr_pu->pu.b4_pos_x = u1_x_pos;
                ps_curr_pu->pu.b4_pos_y = u1_y_pos;
                ps_cu_results->ps_best_results[0].i4_tu_split_cost = 0;

                ps_cu_results++;
                ps_pu_results++;

                continue;
            }

            ps_pu_results->aps_pu_results[0][0] =
                ps_pu_result + (i * MAX_NUM_RESULTS_PER_PART_LIST);
            ps_pu_results->aps_pu_results[1][0] =
                ps_pu_result + ((i + TOT_NUM_PARTS) * MAX_NUM_RESULTS_PER_PART_LIST);

            for(i4_ref = 0; i4_ref < i4_num_active_ref; i4_ref++)
            {
                U08 u1_pred_dir = pu1_pred_dir_searched[i4_ref];

                /* Select the NxN partition node for the current ref_idx in the search results*/
                ps_search_node =
                    ps_search_results->aps_part_results[u1_pred_dir][PART_ID_NxN_TL + i];

                for(k = 0; k < ps_search_results->u1_num_results_per_part; k++)
                {
                    /* If subpel is done then the node is a valid candidate else break the loop */
                    if((ps_search_node->u1_is_avail) || (ps_search_node->u1_subpel_done))
                    {
                        i4_ref_id = ps_search_node->i1_ref_idx;

                        ASSERT(i4_ref_id >= 0);

                        if(!u1_pred_dir)
                        {
                            ps_curr_pu =
                                ps_pu_results->aps_pu_results[0][0] + num_results_per_part_l0;

                            ASSERT(
                                ps_ctxt->a_ref_idx_lc_to_l0[i4_ref_id] <
                                ps_inter_ctb_prms->u1_num_active_ref_l0);

                            ps_curr_pu->pu.mv.i1_l0_ref_idx =
                                ps_ctxt->a_ref_idx_lc_to_l0[i4_ref_id];
                            ps_curr_pu->pu.mv.s_l0_mv = ps_search_node->s_mv;
                            ps_curr_pu->pu.mv.i1_l1_ref_idx = -1;
                            ps_curr_pu->pu.b2_pred_mode = PRED_L0;

                            num_results_per_part_l0++;
                        }
                        else
                        {
                            ps_curr_pu =
                                ps_pu_results->aps_pu_results[1][0] + num_results_per_part_l1;

                            ASSERT(
                                ps_ctxt->a_ref_idx_lc_to_l1[i4_ref_id] <
                                ps_inter_ctb_prms->u1_num_active_ref_l1);

                            ps_curr_pu->pu.mv.i1_l1_ref_idx =
                                ps_ctxt->a_ref_idx_lc_to_l1[i4_ref_id];
                            ps_curr_pu->pu.mv.s_l1_mv = ps_search_node->s_mv;
                            ps_curr_pu->pu.mv.i1_l0_ref_idx = -1;
                            ps_curr_pu->pu.b2_pred_mode = PRED_L1;

                            num_results_per_part_l1++;
                        }
                        ps_curr_pu->i4_mv_cost = ps_search_node->i4_mv_cost;
                        ps_curr_pu->i4_sdi = ps_search_node->i4_sdi;

#if UNI_SATD_SCALE
                        /*SATD is scaled by weight. Hence rescale the SATD */
                        ps_curr_pu->i4_tot_cost =
                            ((ps_search_node->i4_sad *
                                  ps_ctxt->s_wt_pred.a_wpred_wt[ps_search_node->i1_ref_idx] +
                              (1 << (ps_inter_ctb_prms->wpred_log_wdc - 1))) >>
                             ps_inter_ctb_prms->wpred_log_wdc) +
                            ps_search_node->i4_mv_cost;
#endif

                        ps_curr_pu->pu.b4_wd = 1;
                        ps_curr_pu->pu.b4_ht = 1;
                        ps_curr_pu->pu.b4_pos_x = u1_x_pos;
                        ps_curr_pu->pu.b4_pos_y = u1_y_pos;
                        ps_curr_pu->pu.b1_intra_flag = 0;

                        ps_search_node++;
                    }
                    else
                    {
                        /* if NxN was not evaluated at 16x16 level, assign max cost to 8x8 CU
                        to remove 8x8's as possible candidates during evaluation */

                        ps_curr_pu = ps_pu_results->aps_pu_results[0][0] + num_results_per_part_l0;

                        ps_curr_pu->i4_tot_cost = MAX_32BIT_VAL;

                        ps_curr_pu = ps_pu_results->aps_pu_results[1][0] + num_results_per_part_l1;

                        ps_curr_pu->i4_tot_cost = MAX_32BIT_VAL;

                        break;
                    }
                }
            }

            /* Update the num_results per_part across lists L0 and L1 */
            ps_pu_results->u1_num_results_per_part_l0[0] = num_results_per_part_l0;
            ps_pu_results->u1_num_results_per_part_l1[0] = num_results_per_part_l1;
        }
        ps_cu_results++;
        ps_pu_results++;
    }
}

/**
********************************************************************************
*  @fn     hme_insert_intra_nodes_post_bipred
*
*  @brief  Compares intra costs (populated by IPE) with the best inter costs
*          (populated after evaluating bi-pred) and updates the best results
*          if intra cost is better
*
*  @param[in,out]  ps_cu_results    [inout] : Best results structure of CU
*                  ps_cur_ipe_ctb   [in]    : intra results for the current CTB
*                  i4_frm_qstep     [in]    : current frame quantizer(qscale)*
*
*  @return None
********************************************************************************
*/
void hme_insert_intra_nodes_post_bipred(
    inter_cu_results_t *ps_cu_results,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    WORD32 i4_frm_qstep)
{
    WORD32 i;
    WORD32 num_results;
    WORD32 cu_size = ps_cu_results->u1_cu_size;
    UWORD8 u1_x_off = ps_cu_results->u1_x_off;
    UWORD8 u1_y_off = ps_cu_results->u1_y_off;

    /* Id of the 32x32 block, 16x16 block in a CTB */
    WORD32 i4_32x32_id = (u1_y_off >> 5) * 2 + (u1_x_off >> 5);
    WORD32 i4_16x16_id = ((u1_y_off >> 4) & 0x1) * 2 + ((u1_x_off >> 4) & 0x1);

    /* Flags to indicate if intra64/intra32/intra16 cusize are invalid as per IPE decision */
    WORD32 disable_intra64 = 0;
    WORD32 disable_intra32 = 0;
    WORD32 disable_intra16 = 0;

    S32 i4_intra_2nx2n_cost;

    /* ME final results for this CU (post seeding of best uni/bi pred results) */
    part_type_results_t *ps_best_result;

    i4_frm_qstep *= !L0ME_IN_OPENLOOP_MODE;

    /*If inter candidates are enabled then enter the for loop to update the intra candidate */

    if((ps_cu_results->u1_num_best_results == 0) && (CU_8x8 == ps_cu_results->u1_cu_size))
    {
        ps_cu_results->u1_num_best_results = 1;
    }

    num_results = ps_cu_results->u1_num_best_results;

    ps_best_result = &ps_cu_results->ps_best_results[0];

    /* Disable intra16/32/64 flags based on split flags recommended by IPE */
    if(ps_cur_ipe_ctb->u1_split_flag)
    {
        disable_intra64 = 1;
        if(ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id].b1_split_flag)
        {
            disable_intra32 = 1;

            if(ps_cur_ipe_ctb->as_intra32_analyse[i4_32x32_id]
                   .as_intra16_analyse[i4_16x16_id]
                   .b1_split_flag)
            {
                disable_intra16 = 1;
            }
        }
    }

    /* Derive the intra cost based on current cu size and offset */
    switch(cu_size)
    {
    case CU_8x8:
    {
        i4_intra_2nx2n_cost = ps_cur_ipe_ctb->ai4_best8x8_intra_cost[u1_y_off + (u1_x_off >> 3)];

        /* Accounting for coding noise in the open loop IPE cost */
        i4_intra_2nx2n_cost +=
            ((i4_frm_qstep * 16) >> 2) /*+ ((i4_frm_qstep*i4_intra_2nx2n_cost)/256) */;

        break;
    }

    case CU_16x16:
    {
        i4_intra_2nx2n_cost =
            ps_cur_ipe_ctb->ai4_best16x16_intra_cost[(u1_y_off >> 4) * 4 + (u1_x_off >> 4)];

        /* Accounting for coding noise in the open loop IPE cost */
        i4_intra_2nx2n_cost +=
            ((i4_frm_qstep * 16)); /* + ((i4_frm_qstep*i4_intra_2nx2n_cost)/256) */

        if(disable_intra16)
        {
            /* Disable intra 2Nx2N (intra 16) as IPE suggested best mode as 8x8 */
            i4_intra_2nx2n_cost = MAX_32BIT_VAL;
        }
        break;
    }

    case CU_32x32:
    {
        i4_intra_2nx2n_cost =
            ps_cur_ipe_ctb->ai4_best32x32_intra_cost[(u1_y_off >> 5) * 2 + (u1_x_off >> 5)];

        /* Accounting for coding noise in the open loop IPE cost */
        i4_intra_2nx2n_cost +=
            (i4_frm_qstep * 16 * 4) /* + ((i4_frm_qstep*i4_intra_2nx2n_cost)/256) */;

        if(disable_intra32)
        {
            /* Disable intra 2Nx2N (intra 32) as IPE suggested best mode as 16x16 or 8x8 */
            i4_intra_2nx2n_cost = MAX_32BIT_VAL;
        }
        break;
    }

    case CU_64x64:
    {
        i4_intra_2nx2n_cost = ps_cur_ipe_ctb->i4_best64x64_intra_cost;

        /* Accounting for coding noise in the open loop IPE cost */
        i4_intra_2nx2n_cost +=
            (i4_frm_qstep * 16 * 16) /* + ((i4_frm_qstep*i4_intra_2nx2n_cost)/256) */;

        if(disable_intra64)
        {
            /* Disable intra 2Nx2N (intra 64) as IPE suggested best mode as 32x32 /16x16 / 8x8 */
            i4_intra_2nx2n_cost = MAX_32BIT_VAL;
        }
        break;
    }

    default:
        ASSERT(0);
    }

    {
        /*****************************************************************/
        /* Intra / Inter cost comparison for  2Nx2N : cu size 8/16/32/64 */
        /* Identify where the current result isto be placed. Basically   */
        /* find the node which has cost just higher than node under test */
        /*****************************************************************/
        for(i = 0; i < num_results; i++)
        {
            /* Subtrqact the tu_spli_flag_cost from total_inter_cost for fair comparision */
            WORD32 inter_cost = ps_best_result[i].i4_tot_cost - ps_best_result[i].i4_tu_split_cost;

            if(i4_intra_2nx2n_cost < inter_cost)
            {
                if(i < (num_results - 1))
                {
                    memmove(
                        ps_best_result + i + 1,
                        ps_best_result + i,
                        sizeof(ps_best_result[0]) * (num_results - 1 - i));
                }

                /* Insert the intra node result */
                ps_best_result[i].u1_part_type = PRT_2Nx2N;
                ps_best_result[i].i4_tot_cost = i4_intra_2nx2n_cost;
                ps_best_result[i].ai4_tu_split_flag[0] = 0;
                ps_best_result[i].ai4_tu_split_flag[1] = 0;
                ps_best_result[i].ai4_tu_split_flag[2] = 0;
                ps_best_result[i].ai4_tu_split_flag[3] = 0;

                /* Populate intra flag, cost and default mvs, refidx for intra pu */
                ps_best_result[i].as_pu_results[0].i4_tot_cost = i4_intra_2nx2n_cost;
                //ps_best_result[i].as_pu_results[0].i4_sad = i4_intra_2nx2n_cost;
                ps_best_result[i].as_pu_results[0].i4_mv_cost = 0;
                ps_best_result[i].as_pu_results[0].pu.b1_intra_flag = 1;
                ps_best_result[i].as_pu_results[0].pu.mv.i1_l0_ref_idx = -1;
                ps_best_result[i].as_pu_results[0].pu.mv.i1_l1_ref_idx = -1;
                ps_best_result[i].as_pu_results[0].pu.mv.s_l0_mv.i2_mvx = INTRA_MV;
                ps_best_result[i].as_pu_results[0].pu.mv.s_l0_mv.i2_mvy = INTRA_MV;
                ps_best_result[i].as_pu_results[0].pu.mv.s_l1_mv.i2_mvx = INTRA_MV;
                ps_best_result[i].as_pu_results[0].pu.mv.s_l1_mv.i2_mvy = INTRA_MV;

                break;
            }
        }
    }
}

S32 hme_recompute_lambda_from_min_8x8_act_in_ctb(
    me_frm_ctxt_t *ps_ctxt, ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb)
{
    double lambda;
    double lambda_modifier;
    WORD32 i4_cu_qp;
    frm_lambda_ctxt_t *ps_frm_lambda_ctxt;
    //ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb;
    WORD32 i4_frame_qp;
    rc_quant_t *ps_rc_quant_ctxt;
    WORD32 i4_is_bpic;

    ps_frm_lambda_ctxt = &ps_ctxt->s_frm_lambda_ctxt;
    //ps_cur_ipe_ctb = ps_ctxt->ps_ipe_l0_ctb_frm_base;
    i4_frame_qp = ps_ctxt->s_frm_prms.i4_frame_qp;
    ps_rc_quant_ctxt = ps_ctxt->ps_rc_quant_ctxt;
    i4_is_bpic = ps_ctxt->s_frm_prms.bidir_enabled;

    i4_cu_qp = ps_rc_quant_ctxt->pi4_qp_to_qscale[i4_frame_qp + ps_rc_quant_ctxt->i1_qp_offset];

    {
        if(ps_ctxt->i4_l0me_qp_mod)
        {
#if MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON
#if LAMDA_BASED_ON_QUANT
            WORD32 i4_activity = ps_cur_ipe_ctb->i4_64x64_act_factor[2][0];
#else
            WORD32 i4_activity = ps_cur_ipe_ctb->i4_64x64_act_factor[3][0];
#endif
            i4_cu_qp = (((i4_cu_qp)*i4_activity) + (1 << (QP_LEVEL_MOD_ACT_FACTOR - 1))) >>
                       QP_LEVEL_MOD_ACT_FACTOR;

#endif
        }
        if(i4_cu_qp > ps_rc_quant_ctxt->i2_max_qscale)
            i4_cu_qp = ps_rc_quant_ctxt->i2_max_qscale;
        else if(i4_cu_qp < ps_rc_quant_ctxt->i2_min_qscale)
            i4_cu_qp = ps_rc_quant_ctxt->i2_min_qscale;

        i4_cu_qp = ps_rc_quant_ctxt->pi4_qscale_to_qp[i4_cu_qp];
    }

    if(i4_cu_qp > ps_rc_quant_ctxt->i2_max_qp)
        i4_cu_qp = ps_rc_quant_ctxt->i2_max_qp;
    else if(i4_cu_qp < ps_rc_quant_ctxt->i2_min_qp)
        i4_cu_qp = ps_rc_quant_ctxt->i2_min_qp;

    lambda = pow(2.0, (((double)(i4_cu_qp - 12)) / 3));

    lambda_modifier = ps_frm_lambda_ctxt->lambda_modifier;

    if(i4_is_bpic)
    {
        lambda_modifier = lambda_modifier * CLIP3((((double)(i4_cu_qp - 12)) / 6.0), 2.00, 4.00);
    }
    if(ps_ctxt->i4_use_const_lamda_modifier)
    {
        if(ps_ctxt->s_frm_prms.is_i_pic)
        {
            lambda_modifier = ps_ctxt->f_i_pic_lamda_modifier;
        }
        else
        {
            lambda_modifier = CONST_LAMDA_MOD_VAL;
        }
    }
    lambda *= lambda_modifier;

    return ((WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT)));
}

/**
********************************************************************************
*  @fn     hme_update_dynamic_search_params
*
*  @brief  Update the Dynamic search params based on the current MVs
*
*  @param[in,out]  ps_dyn_range_prms    [inout] : Dyn. Range Param str.
*                  i2_mvy               [in]    : current MV y comp.
*
*  @return None
********************************************************************************
*/
void hme_update_dynamic_search_params(dyn_range_prms_t *ps_dyn_range_prms, WORD16 i2_mvy)
{
    /* If MV is up large, update i2_dyn_max_y */
    if(i2_mvy > ps_dyn_range_prms->i2_dyn_max_y)
        ps_dyn_range_prms->i2_dyn_max_y = i2_mvy;
    /* If MV is down large, update i2_dyn_min_y */
    if(i2_mvy < ps_dyn_range_prms->i2_dyn_min_y)
        ps_dyn_range_prms->i2_dyn_min_y = i2_mvy;
}

void hme_add_new_node_to_a_sorted_array(
    search_node_t *ps_result_node,
    search_node_t **pps_sorted_array,
    U08 *pu1_shifts,
    U32 u4_num_results_updated,
    U08 u1_shift)
{
    U32 i;

    if(NULL == pu1_shifts)
    {
        S32 i4_cur_node_cost = ps_result_node->i4_tot_cost;

        for(i = 0; i < u4_num_results_updated; i++)
        {
            if(i4_cur_node_cost < pps_sorted_array[i]->i4_tot_cost)
            {
                memmove(
                    &pps_sorted_array[i + 1],
                    &pps_sorted_array[i],
                    (u4_num_results_updated - i) * sizeof(search_node_t *));

                break;
            }
        }
    }
    else
    {
        S32 i4_cur_node_cost =
            (u1_shift == 0) ? ps_result_node->i4_tot_cost
                            : (ps_result_node->i4_tot_cost + (1 << (u1_shift - 1))) >> u1_shift;

        for(i = 0; i < u4_num_results_updated; i++)
        {
            S32 i4_prev_node_cost = (pu1_shifts[i] == 0) ? pps_sorted_array[i]->i4_tot_cost
                                                         : (pps_sorted_array[i]->i4_tot_cost +
                                                            (1 << (pu1_shifts[i] - 1))) >>
                                                               pu1_shifts[i];

            if(i4_cur_node_cost < i4_prev_node_cost)
            {
                memmove(
                    &pps_sorted_array[i + 1],
                    &pps_sorted_array[i],
                    (u4_num_results_updated - i) * sizeof(search_node_t *));
                memmove(
                    &pu1_shifts[i + 1], &pu1_shifts[i], (u4_num_results_updated - i) * sizeof(U08));

                break;
            }
        }

        pu1_shifts[i] = u1_shift;
    }

    pps_sorted_array[i] = ps_result_node;
}

S32 hme_find_pos_of_implicitly_stored_ref_id(
    S08 *pi1_ref_idx, S08 i1_ref_idx, S32 i4_result_id, S32 i4_num_results)
{
    S32 i;

    for(i = 0; i < i4_num_results; i++)
    {
        if(i1_ref_idx == pi1_ref_idx[i])
        {
            if(0 == i4_result_id)
            {
                return i;
            }
            else
            {
                i4_result_id--;
            }
        }
    }

    return -1;
}

static __inline void hme_search_node_populator(
    search_node_t *ps_search_node, hme_mv_t *ps_mv, S08 i1_ref_idx, S08 i1_mv_magnitude_shift)
{
    ps_search_node->ps_mv->i2_mvx = SHL_NEG((WORD16)ps_mv->i2_mv_x, i1_mv_magnitude_shift);
    ps_search_node->ps_mv->i2_mvy = SHL_NEG((WORD16)ps_mv->i2_mv_y, i1_mv_magnitude_shift);
    ps_search_node->i1_ref_idx = i1_ref_idx;
    ps_search_node->u1_is_avail = 1;
    ps_search_node->u1_subpel_done = 0;
}

S32 hme_populate_search_candidates(fpel_srch_cand_init_data_t *ps_ctxt)
{
    hme_mv_t *ps_mv;

    S32 wd_c, ht_c, wd_p, ht_p;
    S32 blksize_p, blksize_c;
    S32 i;
    S08 *pi1_ref_idx;
    /* Cache for storing offsets */
    S32 ai4_cand_offsets[NUM_SEARCH_CAND_LOCATIONS];

    layer_ctxt_t *ps_curr_layer = ps_ctxt->ps_curr_layer;
    layer_ctxt_t *ps_coarse_layer = ps_ctxt->ps_coarse_layer;
    layer_mv_t *ps_coarse_layer_mvbank = ps_coarse_layer->ps_layer_mvbank;
    layer_mv_t *ps_curr_layer_mvbank = ps_curr_layer->ps_layer_mvbank;
    search_candt_t *ps_search_cands = ps_ctxt->ps_search_cands;
    hme_mv_t s_zero_mv = { 0 };

    S32 i4_pos_x = ps_ctxt->i4_pos_x;
    S32 i4_pos_y = ps_ctxt->i4_pos_y;
    S32 i4_num_act_ref_l0 = ps_ctxt->i4_num_act_ref_l0;
    S32 i4_num_act_ref_l1 = ps_ctxt->i4_num_act_ref_l1;
    U08 u1_pred_dir = ps_ctxt->u1_pred_dir;
    U08 u1_pred_dir_ctr = ps_ctxt->u1_pred_dir_ctr;
    U08 u1_num_results_in_curr_mvbank = ps_ctxt->u1_num_results_in_mvbank;
    U08 u1_num_results_in_coarse_mvbank =
        (u1_pred_dir == 0) ? (i4_num_act_ref_l0 * ps_coarse_layer_mvbank->i4_num_mvs_per_ref)
                           : (i4_num_act_ref_l1 * ps_coarse_layer_mvbank->i4_num_mvs_per_ref);
    S32 i4_init_offset_projected =
        (u1_pred_dir == 1) ? (i4_num_act_ref_l0 * ps_coarse_layer_mvbank->i4_num_mvs_per_ref) : 0;
    S32 i4_init_offset_spatial =
        (u1_pred_dir_ctr == 1)
            ? (ps_curr_layer_mvbank->i4_num_mvs_per_ref * u1_num_results_in_curr_mvbank)
            : 0;
    U08 u1_search_candidate_list_index = ps_ctxt->u1_search_candidate_list_index;
    U08 u1_max_num_search_cands =
        gau1_max_num_search_cands_in_l0_me[u1_search_candidate_list_index];
    S32 i4_num_srch_cands = MIN(u1_max_num_search_cands, ps_ctxt->i4_max_num_init_cands << 1);
    U16 u2_is_offset_available = 0;
    U08 u1_search_blk_to_spatial_mvbank_blk_size_factor = 1;

    /* Width and ht of current and prev layers */
    wd_c = ps_curr_layer->i4_wd;
    ht_c = ps_curr_layer->i4_ht;
    wd_p = ps_coarse_layer->i4_wd;
    ht_p = ps_coarse_layer->i4_ht;

    blksize_p = gau1_blk_size_to_wd_shift[ps_coarse_layer_mvbank->e_blk_size];
    blksize_c = gau1_blk_size_to_wd_shift[ps_curr_layer_mvbank->e_blk_size];

    /* ASSERT for valid sizes */
    ASSERT((blksize_p == 3) || (blksize_p == 4) || (blksize_p == 5));

    {
        S32 x = i4_pos_x >> 4;
        S32 y = i4_pos_y >> 4;

        if(blksize_c != gau1_blk_size_to_wd_shift[ps_ctxt->e_search_blk_size])
        {
            x *= 2;
            y *= 2;

            u1_search_blk_to_spatial_mvbank_blk_size_factor = 2;
        }

        i4_init_offset_spatial += (x + y * ps_curr_layer_mvbank->i4_num_blks_per_row) *
                                  ps_curr_layer_mvbank->i4_num_mvs_per_blk;
    }

    for(i = 0; i < i4_num_srch_cands; i++)
    {
        SEARCH_CANDIDATE_TYPE_T e_search_cand_type =
            gae_search_cand_priority_to_search_cand_type_map_in_l0_me[u1_search_candidate_list_index]
                                                                     [i];
        SEARCH_CAND_LOCATIONS_T e_search_cand_loc =
            gae_search_cand_type_to_location_map[e_search_cand_type];
        S08 i1_result_id = MIN(
            gai1_search_cand_type_to_result_id_map[e_search_cand_type],
            (e_search_cand_loc < 0 ? 0
                                   : ps_ctxt->pu1_num_fpel_search_cands[e_search_cand_loc] - 1));
        U08 u1_is_spatial_cand = (1 == gau1_search_cand_type_to_spatiality_map[e_search_cand_type]);
        U08 u1_is_proj_cand = (0 == gau1_search_cand_type_to_spatiality_map[e_search_cand_type]);
        U08 u1_is_zeroMV_cand = (ZERO_MV == e_search_cand_type) ||
                                (ZERO_MV_ALTREF == e_search_cand_type);

        /* When spatial candidates are available, use them, else use the projected candidates */
        /* This is required since some blocks will never have certain spatial candidates, and in order */
        /* to accomodate such instances in 'gae_search_cand_priority_to_search_cand_type_map_in_l0_me' list,  */
        /* all candidates apart from the 'LEFT' have been marked as projected */
        if(((e_search_cand_loc == TOPLEFT) || (e_search_cand_loc == TOP) ||
            (e_search_cand_loc == TOPRIGHT)) &&
           (i1_result_id < u1_num_results_in_curr_mvbank) && u1_is_proj_cand)
        {
            if(e_search_cand_loc == TOPLEFT)
            {
                u1_is_spatial_cand = ps_ctxt->u1_is_topLeft_available ||
                                     !ps_ctxt->u1_is_left_available;
            }
            else if(e_search_cand_loc == TOPRIGHT)
            {
                u1_is_spatial_cand = ps_ctxt->u1_is_topRight_available;
            }
            else
            {
                u1_is_spatial_cand = ps_ctxt->u1_is_top_available;
            }

            u1_is_proj_cand = !u1_is_spatial_cand;
        }

        switch(u1_is_zeroMV_cand + (u1_is_spatial_cand << 1) + (u1_is_proj_cand << 2))
        {
        case 1:
        {
            hme_search_node_populator(
                ps_search_cands[i].ps_search_node,
                &s_zero_mv,
                (ZERO_MV == e_search_cand_type) ? ps_ctxt->i1_default_ref_id
                                                : ps_ctxt->i1_alt_default_ref_id,
                0);

            break;
        }
        case 2:
        {
            S08 i1_mv_magnitude_shift = 0;

            S32 i4_offset = i4_init_offset_spatial;

            i1_result_id = MIN(i1_result_id, u1_num_results_in_curr_mvbank - 1);
            i4_offset += i1_result_id;

            switch(e_search_cand_loc)
            {
            case LEFT:
            {
                if(ps_ctxt->u1_is_left_available)
                {
                    i1_mv_magnitude_shift = -2;

                    i4_offset -= ps_curr_layer_mvbank->i4_num_mvs_per_blk;

                    ps_mv = ps_curr_layer_mvbank->ps_mv + i4_offset;
                    pi1_ref_idx = ps_curr_layer_mvbank->pi1_ref_idx + i4_offset;
                }
                else
                {
                    i1_mv_magnitude_shift = 0;

                    ps_mv = &s_zero_mv;
                    pi1_ref_idx = &ps_ctxt->i1_default_ref_id;
                }

                break;
            }
            case TOPLEFT:
            {
                if(ps_ctxt->u1_is_topLeft_available)
                {
                    i1_mv_magnitude_shift = -2;

                    i4_offset -= ps_curr_layer_mvbank->i4_num_mvs_per_blk;
                    i4_offset -= ps_curr_layer_mvbank->i4_num_mvs_per_row;

                    ps_mv = ps_curr_layer_mvbank->ps_mv + i4_offset;
                    pi1_ref_idx = ps_curr_layer_mvbank->pi1_ref_idx + i4_offset;
                }
                else
                {
                    i1_mv_magnitude_shift = 0;

                    ps_mv = &s_zero_mv;
                    pi1_ref_idx = &ps_ctxt->i1_default_ref_id;
                }

                break;
            }
            case TOP:
            {
                if(ps_ctxt->u1_is_top_available)
                {
                    i1_mv_magnitude_shift = -2;

                    i4_offset -= ps_curr_layer_mvbank->i4_num_mvs_per_row;

                    ps_mv = ps_curr_layer_mvbank->ps_mv + i4_offset;
                    pi1_ref_idx = ps_curr_layer_mvbank->pi1_ref_idx + i4_offset;
                }
                else
                {
                    i1_mv_magnitude_shift = 0;

                    ps_mv = &s_zero_mv;
                    pi1_ref_idx = &ps_ctxt->i1_default_ref_id;
                }

                break;
            }
            case TOPRIGHT:
            {
                if(ps_ctxt->u1_is_topRight_available)
                {
                    i1_mv_magnitude_shift = -2;

                    i4_offset += ps_curr_layer_mvbank->i4_num_mvs_per_blk *
                                 u1_search_blk_to_spatial_mvbank_blk_size_factor;
                    i4_offset -= ps_curr_layer_mvbank->i4_num_mvs_per_row;

                    ps_mv = ps_curr_layer_mvbank->ps_mv + i4_offset;
                    pi1_ref_idx = ps_curr_layer_mvbank->pi1_ref_idx + i4_offset;
                }
                else
                {
                    i1_mv_magnitude_shift = 0;
                    ps_mv = &s_zero_mv;
                    pi1_ref_idx = &ps_ctxt->i1_default_ref_id;
                }

                break;
            }
            default:
            {
                /* AiyAiyYo!! */
                ASSERT(0);
            }
            }

            hme_search_node_populator(
                ps_search_cands[i].ps_search_node, ps_mv, pi1_ref_idx[0], i1_mv_magnitude_shift);

            break;
        }
        case 4:
        {
            ASSERT(ILLUSORY_CANDIDATE != e_search_cand_type);
            ASSERT(ILLUSORY_LOCATION != e_search_cand_loc);

            i1_result_id = MIN(i1_result_id, u1_num_results_in_coarse_mvbank - 1);

            if(!(u2_is_offset_available & (1 << e_search_cand_loc)))
            {
                S32 x, y;

                x = i4_pos_x + gai4_search_cand_location_to_x_offset_map[e_search_cand_loc];
                y = i4_pos_y + gai4_search_cand_location_to_y_offset_map[e_search_cand_loc];

                /* Safety check to avoid uninitialized access across temporal layers */
                x = CLIP3(x, 0, (wd_c - blksize_p));
                y = CLIP3(y, 0, (ht_c - blksize_p));

                /* Project the positions to prev layer */
                x = x >> blksize_p;
                y = y >> blksize_p;

                ai4_cand_offsets[e_search_cand_loc] =
                    (x * ps_coarse_layer_mvbank->i4_num_mvs_per_blk);
                ai4_cand_offsets[e_search_cand_loc] +=
                    (y * ps_coarse_layer_mvbank->i4_num_mvs_per_row);
                ai4_cand_offsets[e_search_cand_loc] += i4_init_offset_projected;

                u2_is_offset_available |= (1 << e_search_cand_loc);
            }

            ps_mv =
                ps_coarse_layer_mvbank->ps_mv + ai4_cand_offsets[e_search_cand_loc] + i1_result_id;
            pi1_ref_idx = ps_coarse_layer_mvbank->pi1_ref_idx +
                          ai4_cand_offsets[e_search_cand_loc] + i1_result_id;

            hme_search_node_populator(ps_search_cands[i].ps_search_node, ps_mv, pi1_ref_idx[0], 1);

            break;
        }
        default:
        {
            /* NoNoNoNoNooooooooNO! */
            ASSERT(0);
        }
        }

        ASSERT(ps_search_cands[i].ps_search_node->i1_ref_idx >= 0);
        ASSERT(
            !u1_pred_dir
                ? (ps_ctxt->pi4_ref_id_lc_to_l0_map[ps_search_cands[i].ps_search_node->i1_ref_idx] <
                   i4_num_act_ref_l0)
                : (ps_ctxt->pi4_ref_id_lc_to_l1_map[ps_search_cands[i].ps_search_node->i1_ref_idx] <
                   ps_ctxt->i4_num_act_ref_l1));
    }

    return i4_num_srch_cands;
}

void hme_mv_clipper(
    hme_search_prms_t *ps_search_prms_blk,
    S32 i4_num_srch_cands,
    S08 i1_check_for_mult_refs,
    U08 u1_fpel_refine_extent,
    U08 u1_hpel_refine_extent,
    U08 u1_qpel_refine_extent)
{
    S32 candt;
    range_prms_t *ps_range_prms;

    for(candt = 0; candt < i4_num_srch_cands; candt++)
    {
        search_node_t *ps_search_node;

        ps_search_node = ps_search_prms_blk->ps_search_candts[candt].ps_search_node;
        ps_range_prms = ps_search_prms_blk->aps_mv_range[ps_search_node->i1_ref_idx];

        /* Clip the motion vectors as well here since after clipping
        two candidates can become same and they will be removed during deduplication */
        CLIP_MV_WITHIN_RANGE(
            ps_search_node->ps_mv->i2_mvx,
            ps_search_node->ps_mv->i2_mvy,
            ps_range_prms,
            u1_fpel_refine_extent,
            u1_hpel_refine_extent,
            u1_qpel_refine_extent);
    }
}

void hme_init_pred_buf_info(
    hme_pred_buf_info_t (*ps_info)[MAX_NUM_INTER_PARTS],
    hme_pred_buf_mngr_t *ps_buf_mngr,
    U08 u1_pu1_wd,
    U08 u1_pu1_ht,
    PART_TYPE_T e_part_type)
{
    U08 u1_pred_buf_array_id;

    if(1 != ihevce_get_free_pred_buf_indices(
                &u1_pred_buf_array_id, &ps_buf_mngr->u4_pred_buf_usage_indicator, 1))
    {
        ASSERT(0);
    }
    else
    {
        ps_info[0][0].i4_pred_stride = MAX_CU_SIZE;
        ps_info[0][0].pu1_pred = ps_buf_mngr->apu1_pred_bufs[u1_pred_buf_array_id];
        ps_info[0][0].u1_pred_buf_array_id = u1_pred_buf_array_id;

        if(PRT_2Nx2N != e_part_type)
        {
            ps_info[0][1].i4_pred_stride = MAX_CU_SIZE;
            ps_info[0][1].pu1_pred = ps_buf_mngr->apu1_pred_bufs[u1_pred_buf_array_id] +
                                     (gai1_is_part_vertical[ge_part_type_to_part_id[e_part_type][0]]
                                          ? u1_pu1_ht * ps_info[0][1].i4_pred_stride
                                          : u1_pu1_wd);
            ps_info[0][1].u1_pred_buf_array_id = u1_pred_buf_array_id;
        }
    }
}

void hme_debrief_bipred_eval(
    part_type_results_t *ps_part_type_result,
    hme_pred_buf_info_t (*ps_pred_buf_info)[MAX_NUM_INTER_PARTS],
    hme_pred_buf_mngr_t *ps_pred_buf_mngr,
    U08 *pu1_allocated_pred_buf_array_indixes,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list

)
{
    PART_TYPE_T e_part_type = (PART_TYPE_T)ps_part_type_result->u1_part_type;

    U32 *pu4_pred_buf_usage_indicator = &ps_pred_buf_mngr->u4_pred_buf_usage_indicator;
    U08 u1_is_part_vertical = gai1_is_part_vertical[ge_part_type_to_part_id[e_part_type][0]];

    if(0 == ps_part_type_result->u1_part_type)
    {
        if(ps_part_type_result->as_pu_results->pu.b2_pred_mode == PRED_BI)
        {
            ASSERT(UCHAR_MAX != ps_pred_buf_info[2][0].u1_pred_buf_array_id);

            ps_part_type_result->pu1_pred = ps_pred_buf_info[2][0].pu1_pred;
            ps_part_type_result->i4_pred_stride = ps_pred_buf_info[2][0].i4_pred_stride;

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[0]);

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[1]);
        }
        else
        {
            ps_part_type_result->pu1_pred = ps_pred_buf_info[0][0].pu1_pred;
            ps_part_type_result->i4_pred_stride = ps_pred_buf_info[0][0].i4_pred_stride;

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[2]);

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[1]);

            if(UCHAR_MAX == ps_pred_buf_info[0][0].u1_pred_buf_array_id)
            {
                ihevce_set_pred_buf_as_free(
                    pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[0]);
            }
        }
    }
    else
    {
        U08 *pu1_src_pred;
        U08 *pu1_dst_pred;
        S32 i4_src_pred_stride;
        S32 i4_dst_pred_stride;

        U08 u1_pu1_wd = (ps_part_type_result->as_pu_results[0].pu.b4_wd + 1) << 2;
        U08 u1_pu1_ht = (ps_part_type_result->as_pu_results[0].pu.b4_ht + 1) << 2;
        U08 u1_pu2_wd = (ps_part_type_result->as_pu_results[1].pu.b4_wd + 1) << 2;
        U08 u1_pu2_ht = (ps_part_type_result->as_pu_results[1].pu.b4_ht + 1) << 2;

        U08 u1_condition_for_switch =
            (ps_part_type_result->as_pu_results[0].pu.b2_pred_mode == PRED_BI) |
            ((ps_part_type_result->as_pu_results[1].pu.b2_pred_mode == PRED_BI) << 1);

        switch(u1_condition_for_switch)
        {
        case 0:
        {
            ps_part_type_result->pu1_pred =
                ps_pred_buf_mngr->apu1_pred_bufs[pu1_allocated_pred_buf_array_indixes[0]];
            ps_part_type_result->i4_pred_stride = MAX_CU_SIZE;

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[2]);

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[1]);

            if(UCHAR_MAX == ps_pred_buf_info[0][0].u1_pred_buf_array_id)
            {
                pu1_src_pred = ps_pred_buf_info[0][0].pu1_pred;
                pu1_dst_pred = ps_part_type_result->pu1_pred;
                i4_src_pred_stride = ps_pred_buf_info[0][0].i4_pred_stride;
                i4_dst_pred_stride = ps_part_type_result->i4_pred_stride;

                ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_dst_pred,
                    i4_dst_pred_stride,
                    pu1_src_pred,
                    i4_src_pred_stride,
                    u1_pu1_wd,
                    u1_pu1_ht);
            }

            if(UCHAR_MAX == ps_pred_buf_info[0][1].u1_pred_buf_array_id)
            {
                pu1_src_pred = ps_pred_buf_info[0][1].pu1_pred;
                pu1_dst_pred = ps_part_type_result->pu1_pred +
                               (u1_is_part_vertical
                                    ? u1_pu1_ht * ps_part_type_result->i4_pred_stride
                                    : u1_pu1_wd);
                i4_src_pred_stride = ps_pred_buf_info[0][1].i4_pred_stride;
                i4_dst_pred_stride = ps_part_type_result->i4_pred_stride;

                ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_dst_pred,
                    i4_dst_pred_stride,
                    pu1_src_pred,
                    i4_src_pred_stride,
                    u1_pu2_wd,
                    u1_pu2_ht);
            }

            break;
        }
        case 1:
        {
            ASSERT(UCHAR_MAX != ps_pred_buf_info[2][0].u1_pred_buf_array_id);

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[1]);

            /* Copy PU1 pred into PU2's pred buf */
            if(((u1_pu1_ht < u1_pu2_ht) || (u1_pu1_wd < u1_pu2_wd)) &&
               (UCHAR_MAX != ps_pred_buf_info[0][1].u1_pred_buf_array_id))
            {
                ps_part_type_result->pu1_pred =
                    ps_pred_buf_info[0][1].pu1_pred -
                    (u1_is_part_vertical ? u1_pu1_ht * ps_pred_buf_info[0][1].i4_pred_stride
                                         : u1_pu1_wd);
                ps_part_type_result->i4_pred_stride = ps_pred_buf_info[0][1].i4_pred_stride;

                ihevce_set_pred_buf_as_free(
                    pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[2]);

                pu1_src_pred = ps_pred_buf_info[2][0].pu1_pred;
                pu1_dst_pred = ps_part_type_result->pu1_pred;
                i4_src_pred_stride = ps_pred_buf_info[2][0].i4_pred_stride;
                i4_dst_pred_stride = ps_part_type_result->i4_pred_stride;

                ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_dst_pred,
                    i4_dst_pred_stride,
                    pu1_src_pred,
                    i4_src_pred_stride,
                    u1_pu1_wd,
                    u1_pu1_ht);
            }
            else
            {
                ps_part_type_result->pu1_pred = ps_pred_buf_info[2][0].pu1_pred;
                ps_part_type_result->i4_pred_stride = ps_pred_buf_info[2][0].i4_pred_stride;

                ihevce_set_pred_buf_as_free(
                    pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[0]);

                pu1_src_pred = ps_pred_buf_info[0][1].pu1_pred;
                pu1_dst_pred = ps_part_type_result->pu1_pred;
                i4_src_pred_stride = ps_pred_buf_info[0][1].i4_pred_stride;
                i4_dst_pred_stride = ps_part_type_result->i4_pred_stride;

                ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_dst_pred,
                    i4_dst_pred_stride,
                    pu1_src_pred,
                    i4_src_pred_stride,
                    u1_pu2_wd,
                    u1_pu2_ht);
            }

            break;
        }
        case 2:
        {
            ASSERT(UCHAR_MAX != ps_pred_buf_info[2][1].u1_pred_buf_array_id);

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[1]);

            /* Copy PU2 pred into PU1's pred buf */
            if(((u1_pu1_ht > u1_pu2_ht) || (u1_pu1_wd > u1_pu2_wd)) &&
               (UCHAR_MAX != ps_pred_buf_info[0][0].u1_pred_buf_array_id))
            {
                ps_part_type_result->pu1_pred = ps_pred_buf_info[0][0].pu1_pred;
                ps_part_type_result->i4_pred_stride = ps_pred_buf_info[0][0].i4_pred_stride;

                ihevce_set_pred_buf_as_free(
                    pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[2]);

                pu1_src_pred = ps_pred_buf_info[2][1].pu1_pred;
                pu1_dst_pred = ps_part_type_result->pu1_pred +
                               (u1_is_part_vertical
                                    ? u1_pu1_ht * ps_part_type_result->i4_pred_stride
                                    : u1_pu1_wd);
                i4_src_pred_stride = ps_pred_buf_info[2][1].i4_pred_stride;
                i4_dst_pred_stride = ps_part_type_result->i4_pred_stride;

                ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_dst_pred,
                    i4_dst_pred_stride,
                    pu1_src_pred,
                    i4_src_pred_stride,
                    u1_pu2_wd,
                    u1_pu2_ht);
            }
            else
            {
                ps_part_type_result->pu1_pred =
                    ps_pred_buf_info[2][1].pu1_pred -
                    (u1_is_part_vertical ? u1_pu1_ht * ps_pred_buf_info[2][1].i4_pred_stride
                                         : u1_pu1_wd);
                ps_part_type_result->i4_pred_stride = ps_pred_buf_info[2][1].i4_pred_stride;

                ihevce_set_pred_buf_as_free(
                    pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[0]);

                pu1_src_pred = ps_pred_buf_info[0][0].pu1_pred;
                pu1_dst_pred = ps_part_type_result->pu1_pred;
                i4_src_pred_stride = ps_pred_buf_info[0][0].i4_pred_stride;
                i4_dst_pred_stride = ps_part_type_result->i4_pred_stride;

                ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_dst_pred,
                    i4_dst_pred_stride,
                    pu1_src_pred,
                    i4_src_pred_stride,
                    u1_pu1_wd,
                    u1_pu1_ht);
            }

            break;
        }
        case 3:
        {
            ASSERT(UCHAR_MAX != ps_pred_buf_info[2][0].u1_pred_buf_array_id);
            ASSERT(UCHAR_MAX != ps_pred_buf_info[2][1].u1_pred_buf_array_id);
            ASSERT(
                ps_pred_buf_info[2][1].u1_pred_buf_array_id ==
                ps_pred_buf_info[2][0].u1_pred_buf_array_id);

            ps_part_type_result->pu1_pred = ps_pred_buf_info[2][0].pu1_pred;
            ps_part_type_result->i4_pred_stride = ps_pred_buf_info[2][0].i4_pred_stride;

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_allocated_pred_buf_array_indixes[0]);

            break;
        }
        }
    }
}

U08 hme_decide_search_candidate_priority_in_l1_and_l2_me(
    SEARCH_CANDIDATE_TYPE_T e_cand_type, ME_QUALITY_PRESETS_T e_quality_preset)
{
    U08 u1_priority_val =
        gau1_search_cand_priority_in_l1_and_l2_me[e_quality_preset >= ME_MEDIUM_SPEED][e_cand_type];

    if(UCHAR_MAX == u1_priority_val)
    {
        ASSERT(0);
    }

    ASSERT(u1_priority_val <= MAX_INIT_CANDTS);

    return u1_priority_val;
}

U08 hme_decide_search_candidate_priority_in_l0_me(SEARCH_CANDIDATE_TYPE_T e_cand_type, U08 u1_index)
{
    U08 u1_priority_val = gau1_search_cand_priority_in_l0_me[u1_index][e_cand_type];

    if(UCHAR_MAX == u1_priority_val)
    {
        ASSERT(0);
    }

    ASSERT(u1_priority_val <= MAX_INIT_CANDTS);

    return u1_priority_val;
}

void hme_search_cand_data_init(
    S32 *pi4_id_Z,
    S32 *pi4_id_coloc,
    S32 *pi4_num_coloc_cands,
    U08 *pu1_search_candidate_list_index,
    S32 i4_num_act_ref_l0,
    S32 i4_num_act_ref_l1,
    U08 u1_is_bidir_enabled,
    U08 u1_4x4_blk_in_l1me)
{
    S32 i, j;
    S32 i4_num_coloc_cands;

    U08 u1_search_candidate_list_index;

    if(!u1_is_bidir_enabled && !u1_4x4_blk_in_l1me)
    {
        S32 i;

        u1_search_candidate_list_index = (i4_num_act_ref_l0 - 1) * 2;
        i4_num_coloc_cands = i4_num_act_ref_l0 * 2;

        switch(i4_num_act_ref_l0)
        {
        case 1:
        {
            for(i = 0; i < 2; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            break;
        }
        case 2:
        {
            for(i = 0; i < 4; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            break;
        }
        case 3:
        {
            for(i = 0; i < 6; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            break;
        }
        case 4:
        {
            for(i = 0; i < 8; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            break;
        }
        default:
        {
            ASSERT(0);
        }
        }

        *pi4_num_coloc_cands = i4_num_coloc_cands;
        *pu1_search_candidate_list_index = u1_search_candidate_list_index;
    }
    else if(!u1_is_bidir_enabled && u1_4x4_blk_in_l1me)
    {
        S32 i;

        i4_num_coloc_cands = i4_num_act_ref_l0 * 2;
        u1_search_candidate_list_index = (i4_num_act_ref_l0 - 1) * 2 + 1;

        switch(i4_num_act_ref_l0)
        {
        case 1:
        {
            for(i = 0; i < 2; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 1] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 2] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR0, u1_search_candidate_list_index);

            i4_num_coloc_cands += 3;

            break;
        }
        case 2:
        {
            for(i = 0; i < 4; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 1] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 2] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 3] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR1, u1_search_candidate_list_index);

            pi4_id_coloc[i + 4] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL1, u1_search_candidate_list_index);

            pi4_id_coloc[i + 5] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR1, u1_search_candidate_list_index);

            i4_num_coloc_cands += 6;

            break;
        }
        case 3:
        {
            for(i = 0; i < 6; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 1] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 2] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 3] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR1, u1_search_candidate_list_index);

            pi4_id_coloc[i + 4] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL1, u1_search_candidate_list_index);

            pi4_id_coloc[i + 5] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR1, u1_search_candidate_list_index);

            i4_num_coloc_cands += 6;

            break;
        }
        case 4:
        {
            for(i = 0; i < 8; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 1] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 2] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 3] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR1, u1_search_candidate_list_index);

            pi4_id_coloc[i + 4] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL1, u1_search_candidate_list_index);

            pi4_id_coloc[i + 5] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR1, u1_search_candidate_list_index);

            i4_num_coloc_cands += 6;

            break;
        }
        default:
        {
            ASSERT(0);
        }
        }

        *pi4_num_coloc_cands = i4_num_coloc_cands;
        *pu1_search_candidate_list_index = u1_search_candidate_list_index;
    }
    else
    {
        /* The variable 'u1_search_candidate_list_index' is hardcoded */
        /* to 10 and 11 respectively. But, these values are not returned */
        /* by this function since the actual values are dependent on */
        /* the number of refs in L0 and L1 respectively */
        /* Hence, the actual return values are being recomputed */
        /* in the latter part of this block */

        if(!u1_4x4_blk_in_l1me)
        {
            u1_search_candidate_list_index = 10;

            i4_num_coloc_cands = 2 + (2 * ((i4_num_act_ref_l0 > 1) || (i4_num_act_ref_l1 > 1)));

            for(i = 0; i < i4_num_coloc_cands; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }
        }
        else
        {
            u1_search_candidate_list_index = 11;

            i4_num_coloc_cands = 2 + (2 * ((i4_num_act_ref_l0 > 1) || (i4_num_act_ref_l1 > 1)));

            for(i = 0; i < i4_num_coloc_cands; i++)
            {
                pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                    (SEARCH_CANDIDATE_TYPE_T)(PROJECTED_COLOC0 + i),
                    u1_search_candidate_list_index);
            }

            pi4_id_coloc[i] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_TR0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 1] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BL0, u1_search_candidate_list_index);

            pi4_id_coloc[i + 2] = hme_decide_search_candidate_priority_in_l0_me(
                PROJECTED_COLOC_BR0, u1_search_candidate_list_index);
        }

        for(j = 0; j < 2; j++)
        {
            if(0 == j)
            {
                pu1_search_candidate_list_index[j] =
                    8 + ((i4_num_act_ref_l0 > 1) * 2) + u1_4x4_blk_in_l1me;
                pi4_num_coloc_cands[j] =
                    (u1_4x4_blk_in_l1me * 3) + 2 + ((i4_num_act_ref_l0 > 1) * 2);
            }
            else
            {
                pu1_search_candidate_list_index[j] =
                    8 + ((i4_num_act_ref_l1 > 1) * 2) + u1_4x4_blk_in_l1me;
                pi4_num_coloc_cands[j] =
                    (u1_4x4_blk_in_l1me * 3) + 2 + ((i4_num_act_ref_l1 > 1) * 2);
            }
        }
    }

    if(i4_num_act_ref_l0 || i4_num_act_ref_l1)
    {
        pi4_id_Z[0] = hme_decide_search_candidate_priority_in_l0_me(
            (SEARCH_CANDIDATE_TYPE_T)ZERO_MV, pu1_search_candidate_list_index[0]);
    }

    if((i4_num_act_ref_l0 > 1) && !u1_is_bidir_enabled)
    {
        pi4_id_Z[1] = hme_decide_search_candidate_priority_in_l0_me(
            (SEARCH_CANDIDATE_TYPE_T)ZERO_MV_ALTREF, pu1_search_candidate_list_index[0]);
    }
}

static U08
    hme_determine_base_block_size(S32 *pi4_valid_part_array, S32 i4_num_valid_parts, U08 u1_cu_size)
{
    ASSERT(i4_num_valid_parts > 0);

    if(1 == i4_num_valid_parts)
    {
        ASSERT(pi4_valid_part_array[i4_num_valid_parts - 1] == PART_ID_2Nx2N);

        return u1_cu_size;
    }
    else
    {
        if(pi4_valid_part_array[i4_num_valid_parts - 1] <= PART_ID_NxN_BR)
        {
            return u1_cu_size / 2;
        }
        else if(pi4_valid_part_array[i4_num_valid_parts - 1] <= PART_ID_nRx2N_R)
        {
            return u1_cu_size / 4;
        }
    }

    return u1_cu_size / 4;
}

static U32 hme_compute_variance_of_pu_from_base_blocks(
    ULWORD64 *pu8_SigmaX,
    ULWORD64 *pu8_SigmaXSquared,
    U08 u1_cu_size,
    U08 u1_base_block_size,
    S32 i4_part_id)
{
    U08 i, j;
    ULWORD64 u8_final_variance;

    U08 u1_part_dimension_multiplier = (u1_cu_size >> 4);
    S32 i4_part_wd = gai1_part_wd_and_ht[i4_part_id][0] * u1_part_dimension_multiplier;
    S32 i4_part_ht = gai1_part_wd_and_ht[i4_part_id][1] * u1_part_dimension_multiplier;
    U08 u1_num_base_blocks_in_pu_row = i4_part_wd / u1_base_block_size;
    U08 u1_num_base_blocks_in_pu_column = i4_part_ht / u1_base_block_size;
    U08 u1_num_base_blocks_in_cu_row = u1_cu_size / u1_base_block_size;
    U08 u1_num_base_blocks = (u1_num_base_blocks_in_pu_row * u1_num_base_blocks_in_pu_column);
    U32 u4_num_pixels_in_base_block = u1_base_block_size * u1_base_block_size;
    ULWORD64 u8_final_SigmaXSquared = 0;
    ULWORD64 u8_final_SigmaX = 0;

    if(ge_part_id_to_part_type[i4_part_id] != PRT_NxN)
    {
        U08 u1_column_start_index = gau1_part_id_to_part_num[i4_part_id]
                                        ? (gai1_is_part_vertical[i4_part_id]
                                               ? 0
                                               : (u1_cu_size - i4_part_wd) / u1_base_block_size)
                                        : 0;
        U08 u1_row_start_index = gau1_part_id_to_part_num[i4_part_id]
                                     ? (gai1_is_part_vertical[i4_part_id]
                                            ? (u1_cu_size - i4_part_ht) / u1_base_block_size
                                            : 0)
                                     : 0;
        U08 u1_column_end_index = u1_column_start_index + u1_num_base_blocks_in_pu_row;
        U08 u1_row_end_index = u1_row_start_index + u1_num_base_blocks_in_pu_column;

        for(i = u1_row_start_index; i < u1_row_end_index; i++)
        {
            for(j = u1_column_start_index; j < u1_column_end_index; j++)
            {
                u8_final_SigmaXSquared += pu8_SigmaXSquared[j + i * u1_num_base_blocks_in_cu_row];
                u8_final_SigmaX += pu8_SigmaX[j + i * u1_num_base_blocks_in_cu_row];
            }
        }

        u8_final_variance =
            u1_num_base_blocks * u4_num_pixels_in_base_block * u8_final_SigmaXSquared;
        u8_final_variance -= u8_final_SigmaX * u8_final_SigmaX;
        u8_final_variance +=
            ((u1_num_base_blocks * u4_num_pixels_in_base_block) *
             (u1_num_base_blocks * u4_num_pixels_in_base_block) / 2);
        u8_final_variance /= (u1_num_base_blocks * u4_num_pixels_in_base_block) *
                             (u1_num_base_blocks * u4_num_pixels_in_base_block);

        ASSERT(u8_final_variance <= UINT_MAX);
    }
    else
    {
        U08 u1_row_start_index;
        U08 u1_column_start_index;
        U08 u1_row_end_index;
        U08 u1_column_end_index;

        switch(gau1_part_id_to_part_num[i4_part_id])
        {
        case 0:
        {
            u1_row_start_index = 0;
            u1_column_start_index = 0;

            break;
        }
        case 1:
        {
            u1_row_start_index = 0;
            u1_column_start_index = u1_num_base_blocks_in_pu_row;

            break;
        }
        case 2:
        {
            u1_row_start_index = u1_num_base_blocks_in_pu_column;
            u1_column_start_index = 0;

            break;
        }
        case 3:
        {
            u1_row_start_index = u1_num_base_blocks_in_pu_column;
            u1_column_start_index = u1_num_base_blocks_in_pu_row;

            break;
        }
        }

        u1_column_end_index = u1_column_start_index + u1_num_base_blocks_in_pu_row;
        u1_row_end_index = u1_row_start_index + u1_num_base_blocks_in_pu_column;

        for(i = u1_row_start_index; i < u1_row_end_index; i++)
        {
            for(j = u1_column_start_index; j < u1_column_end_index; j++)
            {
                u8_final_SigmaXSquared += pu8_SigmaXSquared[j + i * u1_num_base_blocks_in_cu_row];
                u8_final_SigmaX += pu8_SigmaX[j + i * u1_num_base_blocks_in_cu_row];
            }
        }

        u8_final_variance =
            u1_num_base_blocks * u4_num_pixels_in_base_block * u8_final_SigmaXSquared;
        u8_final_variance -= u8_final_SigmaX * u8_final_SigmaX;
        u8_final_variance +=
            ((u1_num_base_blocks * u4_num_pixels_in_base_block) *
             (u1_num_base_blocks * u4_num_pixels_in_base_block) / 2);
        u8_final_variance /= (u1_num_base_blocks * u4_num_pixels_in_base_block) *
                             (u1_num_base_blocks * u4_num_pixels_in_base_block);

        ASSERT(u8_final_variance <= UINT_MAX);
    }

    return u8_final_variance;
}

void hme_compute_variance_for_all_parts(
    U08 *pu1_data,
    S32 i4_data_stride,
    S32 *pi4_valid_part_array,
    U32 *pu4_variance,
    S32 i4_num_valid_parts,
    U08 u1_cu_size)
{
    ULWORD64 au8_SigmaX[16];
    ULWORD64 au8_SigmaXSquared[16];
    U08 i, j, k, l;
    U08 u1_base_block_size;
    U08 u1_num_base_blocks_in_cu_row;
    U08 u1_num_base_blocks_in_cu_column;

    u1_base_block_size =
        hme_determine_base_block_size(pi4_valid_part_array, i4_num_valid_parts, u1_cu_size);

    u1_num_base_blocks_in_cu_row = u1_num_base_blocks_in_cu_column =
        u1_cu_size / u1_base_block_size;

    ASSERT(u1_num_base_blocks_in_cu_row <= 4);

    for(i = 0; i < u1_num_base_blocks_in_cu_column; i++)
    {
        for(j = 0; j < u1_num_base_blocks_in_cu_row; j++)
        {
            U08 *pu1_buf =
                pu1_data + (u1_base_block_size * j) + (u1_base_block_size * i * i4_data_stride);

            au8_SigmaX[j + i * u1_num_base_blocks_in_cu_row] = 0;
            au8_SigmaXSquared[j + i * u1_num_base_blocks_in_cu_row] = 0;

            for(k = 0; k < u1_base_block_size; k++)
            {
                for(l = 0; l < u1_base_block_size; l++)
                {
                    au8_SigmaX[j + i * u1_num_base_blocks_in_cu_row] +=
                        pu1_buf[l + k * i4_data_stride];
                    au8_SigmaXSquared[j + i * u1_num_base_blocks_in_cu_row] +=
                        pu1_buf[l + k * i4_data_stride] * pu1_buf[l + k * i4_data_stride];
                }
            }
        }
    }

    for(i = 0; i < i4_num_valid_parts; i++)
    {
        pu4_variance[pi4_valid_part_array[i]] = hme_compute_variance_of_pu_from_base_blocks(
            au8_SigmaX, au8_SigmaXSquared, u1_cu_size, u1_base_block_size, pi4_valid_part_array[i]);
    }
}

void hme_compute_final_sigma_of_pu_from_base_blocks(
    U32 *pu4_SigmaX,
    U32 *pu4_SigmaXSquared,
    ULWORD64 *pu8_final_sigmaX,
    ULWORD64 *pu8_final_sigmaX_Squared,
    U08 u1_cu_size,
    U08 u1_base_block_size,
    S32 i4_part_id,
    U08 u1_base_blk_array_stride)
{
    U08 i, j;
    //U08 u1_num_base_blocks_in_cu_row;

    U08 u1_part_dimension_multiplier = (u1_cu_size >> 4);
    S32 i4_part_wd = gai1_part_wd_and_ht[i4_part_id][0] * u1_part_dimension_multiplier;
    S32 i4_part_ht = gai1_part_wd_and_ht[i4_part_id][1] * u1_part_dimension_multiplier;
    U08 u1_num_base_blocks_in_pu_row = i4_part_wd / u1_base_block_size;
    U08 u1_num_base_blocks_in_pu_column = i4_part_ht / u1_base_block_size;
    U16 u2_num_base_blocks = (u1_num_base_blocks_in_pu_row * u1_num_base_blocks_in_pu_column);
    U32 u4_num_pixels_in_base_block = u1_base_block_size * u1_base_block_size;
    U32 u4_N = (u2_num_base_blocks * u4_num_pixels_in_base_block);

    /*if (u1_is_for_src)
    {
    u1_num_base_blocks_in_cu_row = 16;
    }
    else
    {
    u1_num_base_blocks_in_cu_row = u1_cu_size / u1_base_block_size;
    }*/

    pu8_final_sigmaX[i4_part_id] = 0;
    pu8_final_sigmaX_Squared[i4_part_id] = 0;

    if(ge_part_id_to_part_type[i4_part_id] != PRT_NxN)
    {
        U08 u1_column_start_index = gau1_part_id_to_part_num[i4_part_id]
                                        ? (gai1_is_part_vertical[i4_part_id]
                                               ? 0
                                               : (u1_cu_size - i4_part_wd) / u1_base_block_size)
                                        : 0;
        U08 u1_row_start_index = gau1_part_id_to_part_num[i4_part_id]
                                     ? (gai1_is_part_vertical[i4_part_id]
                                            ? (u1_cu_size - i4_part_ht) / u1_base_block_size
                                            : 0)
                                     : 0;
        U08 u1_column_end_index = u1_column_start_index + u1_num_base_blocks_in_pu_row;
        U08 u1_row_end_index = u1_row_start_index + u1_num_base_blocks_in_pu_column;

        for(i = u1_row_start_index; i < u1_row_end_index; i++)
        {
            for(j = u1_column_start_index; j < u1_column_end_index; j++)
            {
                pu8_final_sigmaX_Squared[i4_part_id] +=
                    pu4_SigmaXSquared[j + i * u1_base_blk_array_stride];
                pu8_final_sigmaX[i4_part_id] += pu4_SigmaX[j + i * u1_base_blk_array_stride];
            }
        }
    }
    else
    {
        U08 u1_row_start_index;
        U08 u1_column_start_index;
        U08 u1_row_end_index;
        U08 u1_column_end_index;

        switch(gau1_part_id_to_part_num[i4_part_id])
        {
        case 0:
        {
            u1_row_start_index = 0;
            u1_column_start_index = 0;

            break;
        }
        case 1:
        {
            u1_row_start_index = 0;
            u1_column_start_index = u1_num_base_blocks_in_pu_row;

            break;
        }
        case 2:
        {
            u1_row_start_index = u1_num_base_blocks_in_pu_column;
            u1_column_start_index = 0;

            break;
        }
        case 3:
        {
            u1_row_start_index = u1_num_base_blocks_in_pu_column;
            u1_column_start_index = u1_num_base_blocks_in_pu_row;

            break;
        }
        }

        u1_column_end_index = u1_column_start_index + u1_num_base_blocks_in_pu_row;
        u1_row_end_index = u1_row_start_index + u1_num_base_blocks_in_pu_column;

        for(i = u1_row_start_index; i < u1_row_end_index; i++)
        {
            for(j = u1_column_start_index; j < u1_column_end_index; j++)
            {
                pu8_final_sigmaX_Squared[i4_part_id] +=
                    pu4_SigmaXSquared[j + i * u1_base_blk_array_stride];
                pu8_final_sigmaX[i4_part_id] += pu4_SigmaX[j + i * u1_base_blk_array_stride];
            }
        }
    }

    pu8_final_sigmaX_Squared[i4_part_id] *= u4_N;
}

void hme_compute_stim_injected_distortion_for_all_parts(
    U08 *pu1_pred,
    S32 i4_pred_stride,
    S32 *pi4_valid_part_array,
    ULWORD64 *pu8_src_sigmaX,
    ULWORD64 *pu8_src_sigmaXSquared,
    S32 *pi4_sad_array,
    S32 i4_alpha_stim_multiplier,
    S32 i4_inv_wt,
    S32 i4_inv_wt_shift_val,
    S32 i4_num_valid_parts,
    S32 i4_wpred_log_wdc,
    U08 u1_cu_size)
{
    U32 au4_sigmaX[16], au4_sigmaXSquared[16];
    ULWORD64 au8_final_ref_sigmaX[17], au8_final_ref_sigmaXSquared[17];
    S32 i4_noise_term;
    U16 i2_count;

    ULWORD64 u8_temp_var, u8_temp_var1, u8_pure_dist;
    ULWORD64 u8_ref_X_Square, u8_src_var, u8_ref_var;

    U08 u1_base_block_size;

    WORD32 i4_q_level = STIM_Q_FORMAT + ALPHA_Q_FORMAT;

    u1_base_block_size =
        hme_determine_base_block_size(pi4_valid_part_array, i4_num_valid_parts, u1_cu_size);

    ASSERT(u1_cu_size >= 16);

    hme_compute_sigmaX_and_sigmaXSquared(
        pu1_pred,
        i4_pred_stride,
        au4_sigmaX,
        au4_sigmaXSquared,
        u1_base_block_size,
        u1_base_block_size,
        u1_cu_size,
        u1_cu_size,
        1,
        u1_cu_size / u1_base_block_size);

    /* Noise Term Computation */
    for(i2_count = 0; i2_count < i4_num_valid_parts; i2_count++)
    {
        unsigned long u4_shift_val;
        S32 i4_bits_req;
        S32 part_id = pi4_valid_part_array[i2_count];

        if(i4_alpha_stim_multiplier)
        {
            /* Final SigmaX and SigmaX-Squared Calculation */
            hme_compute_final_sigma_of_pu_from_base_blocks(
                au4_sigmaX,
                au4_sigmaXSquared,
                au8_final_ref_sigmaX,
                au8_final_ref_sigmaXSquared,
                u1_cu_size,
                u1_base_block_size,
                part_id,
                (u1_cu_size / u1_base_block_size));

            u8_ref_X_Square = (au8_final_ref_sigmaX[part_id] * au8_final_ref_sigmaX[part_id]);
            u8_ref_var = (au8_final_ref_sigmaXSquared[part_id] - u8_ref_X_Square);

            u4_shift_val = ihevce_calc_stim_injected_variance(
                pu8_src_sigmaX,
                pu8_src_sigmaXSquared,
                &u8_src_var,
                i4_inv_wt,
                i4_inv_wt_shift_val,
                i4_wpred_log_wdc,
                part_id);

            u8_ref_var = u8_ref_var >> u4_shift_val;

            GETRANGE64(i4_bits_req, u8_ref_var);

            if(i4_bits_req > 27)
            {
                u8_ref_var = u8_ref_var >> (i4_bits_req - 27);
                u8_src_var = u8_src_var >> (i4_bits_req - 27);
            }

            if(u8_src_var == u8_ref_var)
            {
                u8_temp_var = (1 << STIM_Q_FORMAT);
            }
            else
            {
                u8_temp_var = (u8_src_var * u8_ref_var * (1 << STIM_Q_FORMAT));
                u8_temp_var1 = (u8_src_var * u8_src_var) + (u8_ref_var * u8_ref_var);
                u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                u8_temp_var = (u8_temp_var / u8_temp_var1);
                u8_temp_var = (2 * u8_temp_var);
            }

            i4_noise_term = (UWORD32)u8_temp_var;

            ASSERT(i4_noise_term >= 0);

            i4_noise_term *= i4_alpha_stim_multiplier;
        }
        else
        {
            i4_noise_term = 0;
        }

        u8_pure_dist = pi4_sad_array[part_id];
        u8_pure_dist *= ((1 << (i4_q_level)) - (i4_noise_term));
        u8_pure_dist += (1 << ((i4_q_level)-1));
        pi4_sad_array[part_id] = (UWORD32)(u8_pure_dist >> (i4_q_level));
    }
}

void hme_compute_sigmaX_and_sigmaXSquared(
    U08 *pu1_data,
    S32 i4_buf_stride,
    void *pv_sigmaX,
    void *pv_sigmaXSquared,
    U08 u1_base_blk_wd,
    U08 u1_base_blk_ht,
    U08 u1_blk_wd,
    U08 u1_blk_ht,
    U08 u1_is_sigma_pointer_size_32_bit,
    U08 u1_array_stride)
{
    U08 i, j, k, l;
    U08 u1_num_base_blks_in_row;
    U08 u1_num_base_blks_in_column;

    u1_num_base_blks_in_row = u1_blk_wd / u1_base_blk_wd;
    u1_num_base_blks_in_column = u1_blk_ht / u1_base_blk_ht;

    if(u1_is_sigma_pointer_size_32_bit)
    {
        U32 *sigmaX, *sigmaXSquared;

        sigmaX = (U32 *)pv_sigmaX;
        sigmaXSquared = (U32 *)pv_sigmaXSquared;

        /* Loop to compute the sigma_X and sigma_X_Squared */
        for(i = 0; i < u1_num_base_blks_in_column; i++)
        {
            for(j = 0; j < u1_num_base_blks_in_row; j++)
            {
                U32 u4_sigmaX = 0, u4_sigmaXSquared = 0;
                U08 *pu1_buf =
                    pu1_data + (u1_base_blk_wd * j) + (u1_base_blk_ht * i * i4_buf_stride);

                for(k = 0; k < u1_base_blk_ht; k++)
                {
                    for(l = 0; l < u1_base_blk_wd; l++)
                    {
                        u4_sigmaX += pu1_buf[l + k * i4_buf_stride];
                        u4_sigmaXSquared +=
                            (pu1_buf[l + k * i4_buf_stride] * pu1_buf[l + k * i4_buf_stride]);
                    }
                }

                sigmaX[j + i * u1_array_stride] = u4_sigmaX;
                sigmaXSquared[j + i * u1_array_stride] = u4_sigmaXSquared;
            }
        }
    }
    else
    {
        ULWORD64 *sigmaX, *sigmaXSquared;

        sigmaX = (ULWORD64 *)pv_sigmaX;
        sigmaXSquared = (ULWORD64 *)pv_sigmaXSquared;

        /* Loop to compute the sigma_X and sigma_X_Squared */
        for(i = 0; i < u1_num_base_blks_in_column; i++)
        {
            for(j = 0; j < u1_num_base_blks_in_row; j++)
            {
                ULWORD64 u8_sigmaX = 0, u8_sigmaXSquared = 0;
                U08 *pu1_buf =
                    pu1_data + (u1_base_blk_wd * j) + (u1_base_blk_ht * i * i4_buf_stride);

                for(k = 0; k < u1_base_blk_ht; k++)
                {
                    for(l = 0; l < u1_base_blk_wd; l++)
                    {
                        u8_sigmaX += pu1_buf[l + k * i4_buf_stride];
                        u8_sigmaXSquared +=
                            (pu1_buf[l + k * i4_buf_stride] * pu1_buf[l + k * i4_buf_stride]);
                    }
                }

                u8_sigmaXSquared = u8_sigmaXSquared * u1_blk_wd * u1_blk_ht;

                sigmaX[j + i * u1_array_stride] = u8_sigmaX;
                sigmaXSquared[j + i * u1_array_stride] = u8_sigmaXSquared;
            }
        }
    }
}

#if TEMPORAL_NOISE_DETECT
WORD32 ihevce_16x16block_temporal_noise_detect(
    WORD32 had_block_size,
    WORD32 ctb_width,
    WORD32 ctb_height,
    ihevce_ctb_noise_params *ps_ctb_noise_params,
    fpel_srch_cand_init_data_t *s_proj_srch_cand_init_data,
    hme_search_prms_t *s_search_prms_blk,
    me_frm_ctxt_t *ps_ctxt,
    WORD32 num_pred_dir,
    WORD32 i4_num_act_ref_l0,
    WORD32 i4_num_act_ref_l1,
    WORD32 i4_cu_x_off,
    WORD32 i4_cu_y_off,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    WORD32 input_stride,
    WORD32 index_8x8_block,
    WORD32 num_horz_blocks,
    WORD32 num_8x8_in_ctb_row,
    WORD32 i4_16x16_index)
{
    WORD32 i;
    WORD32 noise_detected;

    UWORD8 *pu1_l0_block;
    UWORD8 *pu1_l1_block;

    WORD32 mean;
    UWORD32 variance_8x8;

    /* to store the mean and variance of each 8*8 block and find the variance of any higher block sizes later on. block */
    WORD16 pi2_residue_16x16[256];
    WORD32 mean_16x16;
    UWORD32 variance_16x16[2];

    /* throw errors in case of un- supported arguments */
    /* assumptions size is 8 or 16 or 32 */
    assert(
        (had_block_size == 8) || (had_block_size == 16) || (had_block_size == 32));  //ihevc_assert

    /* initialize the variables */
    noise_detected = 0;
    variance_8x8 = 0;

    mean = 0;

    {
        i = 0;
        /* get the ref/pred and source using the MV of both directions */
        /* pick the best candidates in each direction */
        /* Colocated cands */
        {
            // steps to be done
            /* pick the candidates */
            /* do motion compoensation using the candidates got from prev step : pick from the offset */
            /* get the ref or the pred from the offset*/
            /* get the source data */
            /* send the pred - source to noise detect */
            /* do noise detect on the residue of source and pred */

            layer_mv_t *ps_layer_mvbank;
            hme_mv_t *ps_mv;

            //S32 i;
            S32 wd_c, ht_c, wd_p, ht_p;
            S32 blksize_p, blk_x, blk_y, i4_offset;
            S08 *pi1_ref_idx;
            fpel_srch_cand_init_data_t *ps_ctxt_2 = s_proj_srch_cand_init_data;
            layer_ctxt_t *ps_curr_layer = ps_ctxt_2->ps_curr_layer;
            layer_ctxt_t *ps_coarse_layer = ps_ctxt_2->ps_coarse_layer;
            err_prms_t s_err_prms;
            S32 i4_blk_wd;
            S32 i4_blk_ht;
            BLK_SIZE_T e_blk_size;
            hme_search_prms_t *ps_search_prms;
            S32 i4_part_mask;
            S32 *pi4_valid_part_ids;

            /* has list of valid partition to search terminated by -1 */
            S32 ai4_valid_part_ids[TOT_NUM_PARTS + 1];

            /*SEARCH_COMPLEXITY_T e_search_complexity = ps_ctxt->e_search_complexity;*/

            S32 i4_pos_x;
            S32 i4_pos_y;
            U08 u1_pred_dir;  // = ps_ctxt_2->u1_pred_dir;
            U08 u1_default_ref_id = 0;  //ps_ctxt_2->u1_default_ref_id;
            S32 i4_inp_off, i4_ref_offset, i4_ref_stride;

            /* The reference is actually an array of ptrs since there are several    */
            /* reference id. So an array gets passed form calling function           */
            U08 **ppu1_ref;

            /* Atributes of input candidates */
            search_node_t as_search_node[2];
            wgt_pred_ctxt_t *ps_wt_inp_prms;

            S32 posx;
            S32 posy;
            S32 i4_num_results_to_proj;
            S32 ai4_sad_grid[9 * TOT_NUM_PARTS];
            S32 i4_inp_stride;

            /* intialize variables */
            /* Width and ht of current and prev layers */
            wd_c = ps_curr_layer->i4_wd;
            ht_c = ps_curr_layer->i4_ht;
            wd_p = ps_coarse_layer->i4_wd;
            ht_p = ps_coarse_layer->i4_ht;

            ps_search_prms = s_search_prms_blk;

            ps_wt_inp_prms = &ps_ctxt->s_wt_pred;
            e_blk_size = ps_search_prms->e_blk_size;
            i4_part_mask = ps_search_prms->i4_part_mask;

            i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
            i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];

            ps_layer_mvbank = ps_coarse_layer->ps_layer_mvbank;
            blksize_p = gau1_blk_size_to_wd_shift[ps_layer_mvbank->e_blk_size];

            /* ASSERT for valid sizes */
            ASSERT((blksize_p == 3) || (blksize_p == 4) || (blksize_p == 5));

            i4_pos_x = i4_cu_x_off;
            i4_pos_y = i4_cu_y_off;
            posx = i4_pos_x + 2;
            posy = i4_pos_y + 2;

            i4_inp_stride = ps_search_prms->i4_inp_stride;
            /* Move to the location of the search blk in inp buffer */
            //i4_inp_off = i4_cu_x_off;
            //i4_inp_off += i4_cu_y_off * i4_inp_stride;
            i4_inp_off = (i4_16x16_index % 4) * 16;
            i4_inp_off += (i4_16x16_index / 4) * 16 * i4_inp_stride;

            /***********pick the candidates**************************************/
            for(u1_pred_dir = 0; u1_pred_dir < num_pred_dir; u1_pred_dir++)
            {
                WORD32 actual_pred_dir = 0;

                if(u1_pred_dir == 0 && i4_num_act_ref_l0 == 0)
                {
                    actual_pred_dir = 1;
                }
                else if(u1_pred_dir == 0 && i4_num_act_ref_l0 != 0)
                {
                    actual_pred_dir = 0;
                }
                else if(u1_pred_dir == 1)
                {
                    actual_pred_dir = 1;
                }

                i4_num_results_to_proj = 1;  // only the best proj

                /* Safety check to avoid uninitialized access across temporal layers */
                posx = CLIP3(posx, 0, (wd_c - blksize_p)); /* block position withing frAME */
                posy = CLIP3(posy, 0, (ht_c - blksize_p));

                /* Project the positions to prev layer */
                blk_x = posx >> blksize_p;
                blk_y = posy >> blksize_p;

                /* Pick up the mvs from the location */
                i4_offset = (blk_x * ps_layer_mvbank->i4_num_mvs_per_blk);
                i4_offset += (ps_layer_mvbank->i4_num_mvs_per_row * blk_y);

                ps_mv = ps_layer_mvbank->ps_mv + i4_offset;
                pi1_ref_idx = ps_layer_mvbank->pi1_ref_idx + i4_offset;

                if(actual_pred_dir == 1)
                {
                    ps_mv += (i4_num_act_ref_l0 * ps_layer_mvbank->i4_num_mvs_per_ref);
                    pi1_ref_idx += (i4_num_act_ref_l0 * ps_layer_mvbank->i4_num_mvs_per_ref);
                }

                {
                    as_search_node[actual_pred_dir].s_mv.i2_mvx = ps_mv[0].i2_mv_x << 1;
                    as_search_node[actual_pred_dir].s_mv.i2_mvy = ps_mv[0].i2_mv_y << 1;
                    as_search_node[actual_pred_dir].i1_ref_idx = pi1_ref_idx[0];

                    if((as_search_node[actual_pred_dir].i1_ref_idx < 0) ||
                       (as_search_node[actual_pred_dir].s_mv.i2_mvx == INTRA_MV))
                    {
                        as_search_node[actual_pred_dir].i1_ref_idx = u1_default_ref_id;
                        as_search_node[actual_pred_dir].s_mv.i2_mvx = 0;
                        as_search_node[actual_pred_dir].s_mv.i2_mvy = 0;
                    }
                }

                /********************************************************************************************/
                {
                    /* declare the variables */
                    //ps_fullpel_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;

                    pi4_valid_part_ids = ai4_valid_part_ids;
                    i4_ref_stride = ps_curr_layer->i4_rec_stride;
                    s_err_prms.i4_inp_stride = i4_inp_stride;
                    s_err_prms.i4_ref_stride = i4_ref_stride;
                    s_err_prms.i4_part_mask = i4_part_mask;
                    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0];
                    s_err_prms.i4_blk_wd = i4_blk_wd;
                    s_err_prms.i4_blk_ht = i4_blk_ht;
                    s_err_prms.i4_step = 1;
                    s_err_prms.pi4_valid_part_ids = pi4_valid_part_ids;
                    //s_err_prms.i4_num_partitions = ps_fullpel_refine_ctxt->i4_num_valid_parts;

                    /*************************************************************************/
                    /* Depending on flag i4_use_rec, we use either input of previously       */
                    /* encoded pictures or we use recon of previously encoded pictures.      */
                    i4_ref_stride = ps_curr_layer->i4_rec_stride;
                    ppu1_ref = ps_curr_layer->ppu1_list_rec_fxfy;  // pointer to the pred

                    i4_ref_offset = (i4_ref_stride * i4_cu_y_off) + i4_cu_x_off;  //i4_x_off;

                    s_err_prms.pu1_ref =
                        ppu1_ref[as_search_node[actual_pred_dir].i1_ref_idx] + i4_ref_offset;
                    s_err_prms.pu1_ref += as_search_node[actual_pred_dir].s_mv.i2_mvx;
                    s_err_prms.pu1_ref +=
                        as_search_node[actual_pred_dir].s_mv.i2_mvy * i4_ref_stride;

                    /*get the source */
                    s_err_prms.pu1_inp =
                        ps_wt_inp_prms->apu1_wt_inp[as_search_node[actual_pred_dir].i1_ref_idx] +
                        i4_inp_off;  //pu1_src_input + i4_inp_off;//ps_wt_inp_prms->apu1_wt_inp[as_search_node[actual_pred_dir].i1_ref_idx] + i4_inp_off;

                    /* send the pred - source to noise detect */
                    // noise_detect_hme(noise_structure, s_err_prms.pu1_inp, s_err_prms.pu1_ref);
                }
                /* change the l0/l1 blcok pointer names accrodingle */

                /* get memory pointers the input and the reference */
                pu1_l0_block = s_err_prms.pu1_inp;
                pu1_l1_block = s_err_prms.pu1_ref;

                {
                    WORD32 i2, j2;
                    WORD32 dim = 16;
                    UWORD8 *buf1;
                    UWORD8 *buf2;
                    for(i2 = 0; i2 < dim; i2++)
                    {
                        buf1 = pu1_l0_block + i2 * i4_inp_stride;
                        buf2 = pu1_l1_block + i2 * i4_ref_stride;

                        for(j2 = 0; j2 < dim; j2++)
                        {
                            pi2_residue_16x16[i2 * dim + j2] = (WORD16)(buf1[j2] - buf2[j2]);
                        }
                    }

                    ihevce_calc_variance_signed(
                        pi2_residue_16x16, 16, &mean_16x16, &variance_16x16[u1_pred_dir], 16, 16);

                    /* compare the source and residue variance for this block ps_ctb_noise_params->i4_variance_src_16x16 */
                    if(variance_16x16[u1_pred_dir] >
                       ((TEMPORAL_VARIANCE_FACTOR *
                         ps_ctb_noise_params->au4_variance_src_16x16[i4_16x16_index]) >>
                        Q_TEMPORAL_VARIANCE_FACTOR))
                    {
                        /* update noisy block count only if all  best MV in diff directions indicates noise */
                        if(u1_pred_dir == num_pred_dir - 1)
                        {
                            ps_ctb_noise_params->au1_is_8x8Blk_noisy[index_8x8_block] = 1;
                            ps_ctb_noise_params->au1_is_8x8Blk_noisy[index_8x8_block + 1] = 1;
                            ps_ctb_noise_params
                                ->au1_is_8x8Blk_noisy[index_8x8_block + num_8x8_in_ctb_row] = 1;
                            ps_ctb_noise_params
                                ->au1_is_8x8Blk_noisy[index_8x8_block + num_8x8_in_ctb_row + 1] = 1;
                            noise_detected = 1;
                        }
                    }
                    else /* if any one of the direction mv says it as non noise then dont check for the other directions MV , move for next block*/
                    {
                        noise_detected = 0;
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy[index_8x8_block] = 0;
                        ps_ctb_noise_params->au1_is_8x8Blk_noisy[index_8x8_block + 1] = 0;
                        ps_ctb_noise_params
                            ->au1_is_8x8Blk_noisy[index_8x8_block + num_8x8_in_ctb_row] = 0;
                        ps_ctb_noise_params
                            ->au1_is_8x8Blk_noisy[index_8x8_block + num_8x8_in_ctb_row + 1] = 0;
                        break;
                    }
                }  // variance analysis and calculation
            }  // for each direction
        }  // HME code

    }  // for each 16x16 block

    return (noise_detected);
}
#endif

void hme_qpel_interp_avg_1pt(
    interp_prms_t *ps_prms,
    S32 i4_mv_x,
    S32 i4_mv_y,
    S32 i4_buf_id,
    U08 **ppu1_final,
    S32 *pi4_final_stride)
{
    U08 *pu1_src1, *pu1_src2, *pu1_dst;
    qpel_input_buf_cfg_t *ps_inp_cfg;
    S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;

    /*************************************************************************/
    /* For a given QPEL pt, we need to determine the 2 source pts that are   */
    /* needed to do the QPEL averaging. The logic to do this is as follows   */
    /* i4_mv_x and i4_mv_y are the motion vectors in QPEL units that are     */
    /* pointing to the pt of interest. Obviously, they are w.r.t. the 0,0    */
    /* pt of th reference blk that is colocated to the inp blk.              */
    /*    A j E k B                                                          */
    /*    l m n o p                                                          */
    /*    F q G r H                                                          */
    /*    s t u v w                                                          */
    /*    C x I y D                                                          */
    /* In above diagram, A. B, C, D are full pts at offsets (0,0),(1,0),(0,1)*/
    /* and (1,1) respectively in the fpel buffer (id = 0)                    */
    /* E and I are hxfy pts in offsets (0,0),(0,1) respectively in hxfy buf  */
    /* F and H are fxhy pts in offsets (0,0),(1,0) respectively in fxhy buf  */
    /* G is hxhy pt in offset 0,0 in hxhy buf                                */
    /* All above offsets are computed w.r.t. motion displaced pt in          */
    /* respective bufs. This means that A corresponds to (i4_mv_x >> 2) and  */
    /* (i4_mv_y >> 2) in fxfy buf. Ditto with E, F and G                     */
    /* fxfy buf is buf id 0, hxfy is buf id 1, fxhy is buf id 2, hxhy is 3   */
    /* If we consider pt v to be derived. v has a fractional comp of 3, 3    */
    /* v is avg of H and I. So the table look up of v should give following  */
    /* buf 1 (H) : offset = (1, 0) buf id = 2.                               */
    /* buf 2 (I) : offset = 0 , 1) buf id = 1.                               */
    /* NOTE: For pts that are fxfy/hxfy/fxhy/hxhy, bufid 1 will be -1.       */
    /*************************************************************************/
    i4_mv_x_frac = i4_mv_x & 3;
    i4_mv_y_frac = i4_mv_y & 3;

    i4_offset = (i4_mv_x >> 2) + (i4_mv_y >> 2) * ps_prms->i4_ref_stride;

    /* Derive the descriptor that has all offset and size info */
    ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

    pu1_src1 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
    pu1_src1 += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
    pu1_src1 += (ps_inp_cfg->i1_buf_yoff1 * ps_prms->i4_ref_stride);

    pu1_src2 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id2];
    pu1_src2 += ps_inp_cfg->i1_buf_xoff2 + i4_offset;
    pu1_src2 += (ps_inp_cfg->i1_buf_yoff2 * ps_prms->i4_ref_stride);

    pu1_dst = ps_prms->apu1_interp_out[i4_buf_id];
    hevc_avg_2d(
        pu1_src1,
        pu1_src2,
        ps_prms->i4_ref_stride,
        ps_prms->i4_ref_stride,
        ps_prms->i4_blk_wd,
        ps_prms->i4_blk_ht,
        pu1_dst,
        ps_prms->i4_out_stride);
    ppu1_final[i4_buf_id] = pu1_dst;
    pi4_final_stride[i4_buf_id] = ps_prms->i4_out_stride;
}

void hme_qpel_interp_avg_2pt_vert_with_reuse(
    interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, U08 **ppu1_final, S32 *pi4_final_stride)
{
    hme_qpel_interp_avg_1pt(ps_prms, i4_mv_x, i4_mv_y + 1, 3, ppu1_final, pi4_final_stride);

    hme_qpel_interp_avg_1pt(ps_prms, i4_mv_x, i4_mv_y - 1, 1, ppu1_final, pi4_final_stride);
}

void hme_qpel_interp_avg_2pt_horz_with_reuse(
    interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, U08 **ppu1_final, S32 *pi4_final_stride)
{
    hme_qpel_interp_avg_1pt(ps_prms, i4_mv_x + 1, i4_mv_y, 2, ppu1_final, pi4_final_stride);

    hme_qpel_interp_avg_1pt(ps_prms, i4_mv_x - 1, i4_mv_y, 0, ppu1_final, pi4_final_stride);
}

void hme_set_mv_limit_using_dvsr_data(
    me_frm_ctxt_t *ps_ctxt,
    layer_ctxt_t *ps_curr_layer,
    range_prms_t *ps_mv_limit,
    S16 *pi2_prev_enc_frm_max_mv_y,
    U08 u1_num_act_ref_pics)
{
    WORD32 ref_ctr;

    /* Only for B/b pic. */
    if(1 == ps_ctxt->s_frm_prms.bidir_enabled)
    {
        WORD16 i2_mv_y_per_poc, i2_max_mv_y;
        WORD32 cur_poc, prev_poc, ref_poc, abs_poc_diff;
        WORD32 prev_poc_count = 0;
        WORD32 i4_p_idx;

        pi2_prev_enc_frm_max_mv_y[0] = 0;

        cur_poc = ps_ctxt->i4_curr_poc;

        i4_p_idx = 0;

        /* Get abs MAX for symmetric search */
        i2_mv_y_per_poc = ps_curr_layer->i2_max_mv_y;
        /* Assuming P to P distance as 4 */
        i2_mv_y_per_poc = (i2_mv_y_per_poc + 2) >> 2;

        for(ref_ctr = 0; ref_ctr < u1_num_act_ref_pics; ref_ctr++)
        {
            /* Get the prev. encoded frame POC */
            prev_poc = ps_ctxt->i4_prev_poc;

            ref_poc = ps_ctxt->ai4_ref_idx_to_poc_lc[ref_ctr];
            abs_poc_diff = ABS((cur_poc - ref_poc));
            /* Get the cur. max MV based on POC distance */
            i2_max_mv_y = i2_mv_y_per_poc * abs_poc_diff;
            i2_max_mv_y = MIN(i2_max_mv_y, ps_curr_layer->i2_max_mv_y);

            ps_mv_limit[ref_ctr].i2_min_x = -ps_curr_layer->i2_max_mv_x;
            ps_mv_limit[ref_ctr].i2_min_y = -i2_max_mv_y;
            ps_mv_limit[ref_ctr].i2_max_x = ps_curr_layer->i2_max_mv_x;
            ps_mv_limit[ref_ctr].i2_max_y = i2_max_mv_y;

            /* Find the MAX MV for the prev. encoded frame to optimize */
            /* the reverse dependency of ME on Enc.Loop                */
            if(ref_poc == prev_poc)
            {
                /* TO DO : Same thing for horz. search also */
                pi2_prev_enc_frm_max_mv_y[0] = i2_max_mv_y;
                prev_poc_count++;
            }
        }
    }
    else
    {
        ASSERT(0 == ps_ctxt->s_frm_prms.u1_num_active_ref_l1);

        /* Set the Config. File Params for P pic. */
        for(ref_ctr = 0; ref_ctr < ps_ctxt->s_frm_prms.u1_num_active_ref_l0; ref_ctr++)
        {
            ps_mv_limit[ref_ctr].i2_min_x = -ps_curr_layer->i2_max_mv_x;
            ps_mv_limit[ref_ctr].i2_min_y = -ps_curr_layer->i2_max_mv_y;
            ps_mv_limit[ref_ctr].i2_max_x = ps_curr_layer->i2_max_mv_x;
            ps_mv_limit[ref_ctr].i2_max_y = ps_curr_layer->i2_max_mv_y;
        }

        /* For P PIC., go with  Config. File Params */
        pi2_prev_enc_frm_max_mv_y[0] = ps_curr_layer->i2_max_mv_y;
    }
}

S32 hme_part_mask_populator(
    U08 *pu1_inp,
    S32 i4_inp_stride,
    U08 u1_limit_active_partitions,
    U08 u1_is_bPic,
    U08 u1_is_refPic,
    U08 u1_blk_8x8_mask,
    ME_QUALITY_PRESETS_T e_me_quality_preset)
{
    if(15 != u1_blk_8x8_mask)
    {
        return ENABLE_NxN;
    }
    else
    {
        U08 u1_call_inp_segmentation_based_part_mask_populator =
            (ME_XTREME_SPEED_25 != e_me_quality_preset) ||
            (!u1_is_bPic && !DISABLE_8X8CUS_IN_PPICS_IN_P6) ||
            (u1_is_bPic && u1_is_refPic && !DISABLE_8X8CUS_IN_REFBPICS_IN_P6) ||
            (u1_is_bPic && !u1_is_refPic && !DISABLE_8X8CUS_IN_NREFBPICS_IN_P6);

        if(u1_call_inp_segmentation_based_part_mask_populator)
        {
            S32 i4_part_mask =
                hme_study_input_segmentation(pu1_inp, i4_inp_stride, u1_limit_active_partitions);

            if(e_me_quality_preset == ME_XTREME_SPEED)
            {
                i4_part_mask &= ~ENABLE_AMP;
            }

            if(e_me_quality_preset == ME_XTREME_SPEED_25)
            {
                i4_part_mask &= ~ENABLE_AMP;

                i4_part_mask &= ~ENABLE_SMP;
            }

            return i4_part_mask;
        }
        else
        {
            return ENABLE_2Nx2N;
        }
    }
}
