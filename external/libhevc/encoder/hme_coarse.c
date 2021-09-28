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
* @file hme_coarse.c
*
* @brief
*    Contains ME algorithm for the coarse layer.
*
* @author
*    Ittiam
*
*
* List of Functions
* hme_update_mv_bank_coarse()
* hme_coarse()
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

/*******************************************************************************
*                             MACROS
*******************************************************************************/
#define COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, ps_search_node, shift)                              \
    {                                                                                              \
        ps_mv->i2_mv_x = ps_search_node->s_mv.i2_mvx >> (shift);                                   \
        ps_mv->i2_mv_y = ps_search_node->s_mv.i2_mvy >> (shift);                                   \
        *pi1_ref_idx = ps_search_node->i1_ref_idx;                                                 \
    }

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
********************************************************************************
*  @fn     void hme_update_mv_bank_coarse(search_results_t *ps_search_results,
*                                   layer_mv_t *ps_layer_mv,
*                                   S32 i4_blk_x,
*                                   S32 i4_blk_y,
*                                   search_node_t *ps_search_node_4x8_l,
*                                   search_node_t *ps_search_node_8x4_t,
*                                   S08 i1_ref_idx,
*                                   mvbank_update_prms_t *ps_prms
*
*  @brief  Updates the coarse layer MV Bank for a given ref id and blk pos
*
*  @param[in]  ps_search_results: Search results data structure
*
*  @param[in, out]  ps_layer_mv : MV Bank for this layer
*
*  @param[in]  i4_search_blk_x: column number of the 4x4 blk searched
*
*  @param[in]  i4_search_blk_y: row number of the 4x4 blk searched
*
*  @param[in]  ps_search_node_4x8_t: Best MV of the 4x8T blk
*
*  @param[in]  ps_search_node_8x4_l: Best MV of the 8x4L blk
*
*  @param[in]  i1_ref_idx : Reference ID that has been searched
*
*  @param[in]  ps_prms : Parameters pertaining to the MV Bank update
*
*  @return None
********************************************************************************
*/
void hme_update_mv_bank_coarse(
    search_results_t *ps_search_results,
    layer_mv_t *ps_layer_mv,
    S32 i4_search_blk_x,
    S32 i4_search_blk_y,
    search_node_t *ps_search_node_4x8_t,
    search_node_t *ps_search_node_8x4_l,
    S08 i1_ref_idx,
    mvbank_update_prms_t *ps_prms)
{
    /* These point to the MV and ref idx posn to be udpated */
    hme_mv_t *ps_mv;
    S08 *pi1_ref_idx;

    /* Offset within the bank */
    S32 i4_offset;

    S32 i, j, i4_blk_x, i4_blk_y;

    /* Best results for 8x4R and 4x8B blocks */
    search_node_t *ps_search_node_8x4_r, *ps_search_node_4x8_b;

    /* Number of MVs in a block */
    S32 num_mvs = ps_layer_mv->i4_num_mvs_per_ref;

    search_node_t *aps_search_nodes[4];

    /* The search blk may be different in size from the blk used to hold MV */
    i4_blk_x = i4_search_blk_x << ps_prms->i4_shift;
    i4_blk_y = i4_search_blk_y << ps_prms->i4_shift;

    /* Compute the offset in the MV bank */
    i4_offset = i4_blk_x + i4_blk_y * ps_layer_mv->i4_num_blks_per_row;
    i4_offset *= ps_layer_mv->i4_num_mvs_per_blk;

    /* Identify the correct offset in the mvbank and the reference id buf */
    ps_mv = ps_layer_mv->ps_mv + (i4_offset + (num_mvs * i1_ref_idx));
    pi1_ref_idx = ps_layer_mv->pi1_ref_idx + (i4_offset + (num_mvs * i1_ref_idx));

    /*************************************************************************/
    /* We have atleast 4 distinct results: the 4x8 top (coming from top blk) */
    /* 8x4 left (coming from left blk), 8x4 and 4x8 right and bot resp.      */
    /* If number of results to be stored is 4, then we store all these 4     */
    /* results, else we pick best ones                                       */
    /*************************************************************************/
    ps_search_node_8x4_r = ps_search_results->aps_part_results[i1_ref_idx][PART_ID_2NxN_B];
    ps_search_node_4x8_b = ps_search_results->aps_part_results[i1_ref_idx][PART_ID_Nx2N_R];

    ASSERT(num_mvs <= 4);

    /* Doing this to sort best results */
    aps_search_nodes[0] = ps_search_node_8x4_r;
    aps_search_nodes[1] = ps_search_node_4x8_b;
    aps_search_nodes[2] = ps_search_node_8x4_l;
    aps_search_nodes[3] = ps_search_node_4x8_t;
    if(num_mvs == 4)
    {
        COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, aps_search_nodes[0], 0);
        ps_mv++;
        pi1_ref_idx++;
        COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, aps_search_nodes[1], 0);
        ps_mv++;
        pi1_ref_idx++;
        COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, aps_search_nodes[2], 0);
        ps_mv++;
        pi1_ref_idx++;
        COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, aps_search_nodes[3], 0);
        ps_mv++;
        pi1_ref_idx++;
        return;
    }

    /* Run through the results, store them in best to worst order */
    for(i = 0; i < num_mvs; i++)
    {
        for(j = i + 1; j < 4; j++)
        {
            if(aps_search_nodes[j]->i4_tot_cost < aps_search_nodes[i]->i4_tot_cost)
            {
                SWAP_HME(aps_search_nodes[j], aps_search_nodes[i], search_node_t *);
            }
        }
        COPY_SEARCH_RESULT(ps_mv, pi1_ref_idx, aps_search_nodes[i], 0);
        ps_mv++;
        pi1_ref_idx++;
    }
}

/**
********************************************************************************
*  @fn     void hme_coarse_frm_init(me_ctxt_t *ps_ctxt, coarse_prms_t *ps_coarse_prms)
*
*  @brief  Frame init entry point Coarse ME.
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_coarse_prms : Coarse layer config params
*
*  @return None
********************************************************************************
*/
void hme_coarse_frm_init(coarse_me_ctxt_t *ps_ctxt, coarse_prms_t *ps_coarse_prms)
{
    layer_ctxt_t *ps_curr_layer;

    S32 i4_pic_wd, i4_pic_ht;

    S32 num_blks_in_pic, num_blks_in_row;

    BLK_SIZE_T e_search_blk_size = BLK_4x4;

    S32 blk_size_shift = 2, blk_wd = 4, blk_ht = 4;

    /* Number of references to search */
    S32 i4_num_ref;

    ps_curr_layer = ps_ctxt->ps_curr_descr->aps_layers[ps_coarse_prms->i4_layer_id];
    i4_num_ref = ps_coarse_prms->i4_num_ref;

    i4_pic_wd = ps_curr_layer->i4_wd;
    i4_pic_ht = ps_curr_layer->i4_ht;
    /* Macro updates num_blks_in_pic and num_blks_in_row*/
    GET_NUM_BLKS_IN_PIC(i4_pic_wd, i4_pic_ht, blk_size_shift, num_blks_in_row, num_blks_in_pic);

    /************************************************************************/
    /* Initialize the mv bank that holds results of this layer.             */
    /************************************************************************/
    hme_init_mv_bank(
        ps_curr_layer,
        BLK_4x4,
        i4_num_ref,
        ps_coarse_prms->num_results,
        ps_ctxt->u1_encode[ps_coarse_prms->i4_layer_id]);

    return;
}

/**
********************************************************************************
*  @fn    void hme_derive_worst_case_search_range(range_prms_t *ps_range,
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
void hme_derive_worst_case_search_range(
    range_prms_t *ps_range,
    range_prms_t *ps_pic_limit,
    range_prms_t *ps_mv_limit,
    S32 i4_x,
    S32 i4_y,
    S32 blk_wd,
    S32 blk_ht)
{
    /* Taking max x of left block, min x of current block */
    ps_range->i2_max_x =
        MIN((ps_pic_limit->i2_max_x - (S16)blk_wd - (S16)(i4_x - 4)), ps_mv_limit->i2_max_x);
    ps_range->i2_min_x = MAX((ps_pic_limit->i2_min_x - (S16)i4_x), ps_mv_limit->i2_min_x);
    /* Taking max y of top block, min y of current block */
    ps_range->i2_max_y =
        MIN((ps_pic_limit->i2_max_y - (S16)blk_ht - (S16)(i4_y - 4)), ps_mv_limit->i2_max_y);
    ps_range->i2_min_y = MAX((ps_pic_limit->i2_min_y - (S16)i4_y), ps_mv_limit->i2_min_y);
}

/**
********************************************************************************
* @fn void hme_combine_4x4_sads_and_compute_cost(S08 i1_ref_idx,
*                                           range_prms_t *ps_mv_range,
*                                           range_prms_t *ps_mv_limit,
*                                           hme_mv_t *ps_best_mv_4x8,
*                                           hme_mv_t *ps_best_mv_8x4,
*                                           pred_ctxt_t *ps_pred_ctxt,
*                                           PF_MV_COST_FXN pf_mv_cost_compute,
*                                           ME_QUALITY_PRESETS_T e_me_quality_preset,
*                                           S16 *pi2_sads_4x4_current,
*                                           S16 *pi2_sads_4x4_east,
*                                           S16 *pi2_sads_4x4_south,
*                                           FILE *fp_dump_sad)
*
*  @brief  Does a full search on entire srch window with a given step size in coarse layer
*
*  @param[in] i1_ref_idx : Cur ref idx
*
*  @param[in] ps_layer_ctxt: All info about this layer
*
*  @param[out] ps_best_mv  : type hme_mv_t contains best mv x and y
*
*  @param[in] ps_pred_ctxt : Prediction ctxt for cost computation
*
*  @param[in] pf_mv_cost_compute : mv cost computation function
*
*  @return void
********************************************************************************
*/
void hme_combine_4x4_sads_and_compute_cost_high_quality(
    S08 i1_ref_idx,
    range_prms_t *ps_mv_range,
    range_prms_t *ps_mv_limit,
    hme_mv_t *ps_best_mv_4x8,
    hme_mv_t *ps_best_mv_8x4,
    pred_ctxt_t *ps_pred_ctxt,
    PF_MV_COST_FXN pf_mv_cost_compute,
    S16 *pi2_sads_4x4_current,
    S16 *pi2_sads_4x4_east,
    S16 *pi2_sads_4x4_south)
{
    /* These control number of parts and number of pts in grid to search */
    S32 stepy, stepx, best_mv_y_4x8, best_mv_x_4x8, best_mv_y_8x4, best_mv_x_8x4;
    S32 step_shift_x, step_shift_y;
    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    S32 min_cost_4x8 = MAX_32BIT_VAL;
    S32 min_cost_8x4 = MAX_32BIT_VAL;

    search_node_t s_search_node;
    s_search_node.i1_ref_idx = i1_ref_idx;

    stepx = stepy = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_QUALITY */
    step_shift_x = step_shift_y = 1;

    mv_x_offset = (-ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = (-ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_mv_range->i2_min_y; mvy < ps_mv_range->i2_max_y; mvy += stepy)
    {
        for(mvx = ps_mv_range->i2_min_x; mvx < ps_mv_range->i2_max_x; mvx += stepx)
        {
            S32 sad_4x8, cost_4x8, sad_8x4, cost_8x4;
            S32 sad_pos = ((mvx >> step_shift_x) + mv_x_offset) +
                          ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range;

            /* Get SAD by adding SAD for current and neighbour S  */
            sad_4x8 = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_south[sad_pos];
            sad_8x4 = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_east[sad_pos];

            //          fprintf(fp_dump_sad,"%d\t",sad);
            s_search_node.s_mv.i2_mvx = mvx;
            s_search_node.s_mv.i2_mvy = mvy;

            cost_4x8 = cost_8x4 =
                pf_mv_cost_compute(&s_search_node, ps_pred_ctxt, PART_ID_2Nx2N, MV_RES_FPEL);

            cost_4x8 += sad_4x8;
            cost_8x4 += sad_8x4;

            if(cost_4x8 < min_cost_4x8)
            {
                best_mv_x_4x8 = mvx;
                best_mv_y_4x8 = mvy;
                min_cost_4x8 = cost_4x8;
            }
            if(cost_8x4 < min_cost_8x4)
            {
                best_mv_x_8x4 = mvx;
                best_mv_y_8x4 = mvy;
                min_cost_8x4 = cost_8x4;
            }
        }
    }

    ps_best_mv_4x8->i2_mv_x = best_mv_x_4x8;
    ps_best_mv_4x8->i2_mv_y = best_mv_y_4x8;

    ps_best_mv_8x4->i2_mv_x = best_mv_x_8x4;
    ps_best_mv_8x4->i2_mv_y = best_mv_y_8x4;
}

void hme_combine_4x4_sads_and_compute_cost_high_speed(
    S08 i1_ref_idx,
    range_prms_t *ps_mv_range,
    range_prms_t *ps_mv_limit,
    hme_mv_t *ps_best_mv_4x8,
    hme_mv_t *ps_best_mv_8x4,
    pred_ctxt_t *ps_pred_ctxt,
    PF_MV_COST_FXN pf_mv_cost_compute,
    S16 *pi2_sads_4x4_current,
    S16 *pi2_sads_4x4_east,
    S16 *pi2_sads_4x4_south)
{
    /* These control number of parts and number of pts in grid to search */
    S32 stepy, stepx, best_mv_y_4x8, best_mv_x_4x8, best_mv_y_8x4, best_mv_x_8x4;
    S32 step_shift_x, step_shift_y;
    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    S32 rnd, lambda, lambda_q_shift;

    S32 min_cost_4x8 = MAX_32BIT_VAL;
    S32 min_cost_8x4 = MAX_32BIT_VAL;

    (void)pf_mv_cost_compute;
    stepx = stepy = HME_COARSE_STEP_SIZE_HIGH_SPEED;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_SPEED */
    step_shift_x = step_shift_y = 2;

    mv_x_offset = (-ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = (-ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    lambda = ps_pred_ctxt->lambda;
    lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
    rnd = 1 << (lambda_q_shift - 1);

    ASSERT(MAX_MVX_SUPPORTED_IN_COARSE_LAYER >= ABS(ps_mv_range->i2_max_x));
    ASSERT(MAX_MVY_SUPPORTED_IN_COARSE_LAYER >= ABS(ps_mv_range->i2_max_y));

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_mv_range->i2_min_y; mvy < ps_mv_range->i2_max_y; mvy += stepy)
    {
        for(mvx = ps_mv_range->i2_min_x; mvx < ps_mv_range->i2_max_x; mvx += stepx)
        {
            S32 sad_4x8, cost_4x8, sad_8x4, cost_8x4;

            S32 sad_pos = ((mvx >> step_shift_x) + mv_x_offset) +
                          ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range;

            /* Get SAD by adding SAD for current and neighbour S  */
            sad_4x8 = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_south[sad_pos];
            sad_8x4 = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_east[sad_pos];

            //          fprintf(fp_dump_sad,"%d\t",sad);

            cost_4x8 = cost_8x4 =
                (2 * hme_get_range(ABS(mvx)) - 1) + (2 * hme_get_range(ABS(mvy)) - 1) + i1_ref_idx;

            cost_4x8 += (mvx != 0) ? 1 : 0;
            cost_4x8 += (mvy != 0) ? 1 : 0;
            cost_4x8 = (cost_4x8 * lambda + rnd) >> lambda_q_shift;

            cost_8x4 += (mvx != 0) ? 1 : 0;
            cost_8x4 += (mvy != 0) ? 1 : 0;
            cost_8x4 = (cost_8x4 * lambda + rnd) >> lambda_q_shift;

            cost_4x8 += sad_4x8;
            cost_8x4 += sad_8x4;

            if(cost_4x8 < min_cost_4x8)
            {
                best_mv_x_4x8 = mvx;
                best_mv_y_4x8 = mvy;
                min_cost_4x8 = cost_4x8;
            }
            if(cost_8x4 < min_cost_8x4)
            {
                best_mv_x_8x4 = mvx;
                best_mv_y_8x4 = mvy;
                min_cost_8x4 = cost_8x4;
            }
        }
    }

    ps_best_mv_4x8->i2_mv_x = best_mv_x_4x8;
    ps_best_mv_4x8->i2_mv_y = best_mv_y_4x8;

    ps_best_mv_8x4->i2_mv_x = best_mv_x_8x4;
    ps_best_mv_8x4->i2_mv_y = best_mv_y_8x4;
}

/**
********************************************************************************
*  @fn     hme_store_4x4_sads(hme_search_prms_t *ps_search_prms,
*                               layer_ctxt_t *ps_layer_ctxt)
*
*  @brief  Does a 4x4 sad computation on a given range and stores it in memory
*
*  @param[in] ps_search_prms : Search prms structure containing info like
*               blk dimensions, search range etc
*
*  @param[in] ps_layer_ctxt: All info about this layer
*
*  @param[in] ps_wt_inp_prms: All info about weighted input
*
*  @param[in] e_me_quality_preset: motion estimation quality preset
*
*  @param[in] pi2_sads_4x4: Memory to store all 4x4 SADs for given range
*
*  @return void
********************************************************************************
*/

void hme_store_4x4_sads_high_quality(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    range_prms_t *ps_mv_limit,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S16 *pi2_sads_4x4)
{
    S32 sad, i, j;

    /* Input and reference attributes */
    U08 *pu1_inp, *pu1_inp_orig, *pu1_ref;
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref, *pu1_ref_coloc;

    S32 stepy, stepx, step_shift_x, step_shift_y;
    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    /* Points to the range limits for mv */
    range_prms_t *ps_range_prms;

    /* Reference index to be searched */
    S32 i4_search_idx = ps_search_prms->i1_ref_idx;
    /* Using the member 0 to store for all ref. idx. */
    ps_range_prms = ps_search_prms->aps_mv_range[0];
    pu1_inp_orig = ps_wt_inp_prms->apu1_wt_inp[i4_search_idx];
    i4_inp_stride = ps_search_prms->i4_inp_stride;

    /* Move to the location of the search blk in inp buffer */
    pu1_inp_orig += ps_search_prms->i4_cu_x_off;
    pu1_inp_orig += ps_search_prms->i4_cu_y_off * i4_inp_stride;

    /*************************************************************************/
    /* we use either input of previously encoded pictures as reference       */
    /* in coarse layer                                                       */
    /*************************************************************************/
    i4_ref_stride = ps_layer_ctxt->i4_inp_stride;
    ppu1_ref = ps_layer_ctxt->ppu1_list_inp;

    /* colocated position in reference picture */
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    pu1_ref_coloc = ppu1_ref[i4_search_idx] + i4_ref_offset;

    stepx = stepy = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_QUALITY */
    step_shift_x = step_shift_y = 1;

    mv_x_offset = -(ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = -(ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_range_prms->i2_min_y; mvy < ps_range_prms->i2_max_y; mvy += stepy)
    {
        for(mvx = ps_range_prms->i2_min_x; mvx < ps_range_prms->i2_max_x; mvx += stepx)
        {
            /* Set up the reference and inp ptr */
            pu1_ref = pu1_ref_coloc + mvx + (mvy * i4_ref_stride);
            pu1_inp = pu1_inp_orig;
            /* SAD computation */
            {
                sad = 0;
                for(i = 0; i < 4; i++)
                {
                    for(j = 0; j < 4; j++)
                    {
                        sad += (ABS(((S32)pu1_inp[j] - (S32)pu1_ref[j])));
                    }
                    pu1_inp += i4_inp_stride;
                    pu1_ref += i4_ref_stride;
                }
            }

            pi2_sads_4x4
                [((mvx >> step_shift_x) + mv_x_offset) +
                 ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range] = sad;
        }
    }
}

void hme_store_4x4_sads_high_speed(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    range_prms_t *ps_mv_limit,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S16 *pi2_sads_4x4)
{
    S32 sad, i, j;

    /* Input and reference attributes */
    U08 *pu1_inp, *pu1_inp_orig, *pu1_ref;
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref, *pu1_ref_coloc;

    S32 stepy, stepx, step_shift_x, step_shift_y;
    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    /* Points to the range limits for mv */
    range_prms_t *ps_range_prms;

    /* Reference index to be searched */
    S32 i4_search_idx = ps_search_prms->i1_ref_idx;

    /* Using the member 0 for all ref. idx */
    ps_range_prms = ps_search_prms->aps_mv_range[0];
    pu1_inp_orig = ps_wt_inp_prms->apu1_wt_inp[i4_search_idx];
    i4_inp_stride = ps_search_prms->i4_inp_stride;

    /* Move to the location of the search blk in inp buffer */
    pu1_inp_orig += ps_search_prms->i4_cu_x_off;
    pu1_inp_orig += ps_search_prms->i4_cu_y_off * i4_inp_stride;

    /*************************************************************************/
    /* we use either input of previously encoded pictures as reference       */
    /* in coarse layer                                                       */
    /*************************************************************************/
    i4_ref_stride = ps_layer_ctxt->i4_inp_stride;
    ppu1_ref = ps_layer_ctxt->ppu1_list_inp;

    /* colocated position in reference picture */
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    pu1_ref_coloc = ppu1_ref[i4_search_idx] + i4_ref_offset;

    stepx = stepy = HME_COARSE_STEP_SIZE_HIGH_SPEED;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_SPEED */
    step_shift_x = step_shift_y = 2;

    mv_x_offset = -(ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = -(ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_range_prms->i2_min_y; mvy < ps_range_prms->i2_max_y; mvy += stepy)
    {
        for(mvx = ps_range_prms->i2_min_x; mvx < ps_range_prms->i2_max_x; mvx += stepx)
        {
            /* Set up the reference and inp ptr */
            pu1_ref = pu1_ref_coloc + mvx + (mvy * i4_ref_stride);
            pu1_inp = pu1_inp_orig;
            /* SAD computation */
            {
                sad = 0;
                for(i = 0; i < 4; i++)
                {
                    for(j = 0; j < 4; j++)
                    {
                        sad += (ABS(((S32)pu1_inp[j] - (S32)pu1_ref[j])));
                    }
                    pu1_inp += i4_inp_stride;
                    pu1_ref += i4_ref_stride;
                }
            }

            pi2_sads_4x4
                [((mvx >> step_shift_x) + mv_x_offset) +
                 ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range] = sad;
        }
    }
}
/**
********************************************************************************
*  @fn     void hme_coarsest(me_ctxt_t *ps_ctxt, coarse_prms_t *ps_coarse_prms)
*
*  @brief  Top level entry point for Coarse ME. Runs across blks and searches
*          at a 4x4 blk granularity by using 4x8 and 8x4 patterns.
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_coarse_prms : Coarse layer config params
*
*  @param[in]  ps_multi_thrd_ctxt : Multi thread context
*
*  @return None
********************************************************************************
*/
void hme_coarsest(
    coarse_me_ctxt_t *ps_ctxt,
    coarse_prms_t *ps_coarse_prms,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 i4_ping_pong,
    void **ppv_dep_mngr_hme_sync)
{
    S16 *pi2_cur_ref_sads_4x4;
    S32 ai4_sad_4x4_block_size[MAX_NUM_REF], ai4_sad_4x4_block_stride[MAX_NUM_REF];
    S32 num_rows_coarse;
    S32 sad_top_offset, sad_current_offset;
    S32 search_node_top_offset, search_node_left_offset;

    ME_QUALITY_PRESETS_T e_me_quality_preset =
        ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets;

    search_results_t *ps_search_results;
    mvbank_update_prms_t s_mv_update_prms;
    BLK_SIZE_T e_search_blk_size = BLK_4x4;
    hme_search_prms_t s_search_prms_4x8, s_search_prms_8x4, s_search_prms_4x4;

    S32 global_id_8x4, global_id_4x8;

    /*************************************************************************/
    /* These directly point to the best search result nodes that will be     */
    /* updated by the search algorithm, rather than have to go through an    */
    /* elaborate structure                                                   */
    /*************************************************************************/
    search_node_t *aps_best_search_node_8x4[MAX_NUM_REF];
    search_node_t *aps_best_search_node_4x8[MAX_NUM_REF];

    /* These point to various spatial candts */
    search_node_t *ps_candt_8x4_l, *ps_candt_8x4_t, *ps_candt_8x4_tl;
    search_node_t *ps_candt_4x8_l, *ps_candt_4x8_t, *ps_candt_4x8_tl;
    search_node_t *ps_candt_zeromv_8x4, *ps_candt_zeromv_4x8;
    search_node_t *ps_candt_fs_8x4, *ps_candt_fs_4x8;
    search_node_t as_top_neighbours[4], as_left_neighbours[3];

    /* Holds the global mv for a given ref index */
    search_node_t s_candt_global[MAX_NUM_REF];

    /* All the search candidates */
    search_candt_t as_search_candts_8x4[MAX_INIT_CANDTS];
    search_candt_t as_search_candts_4x8[MAX_INIT_CANDTS];
    search_candt_t *ps_search_candts_8x4, *ps_search_candts_4x8;

    /* Actual range per blk and the pic level boundaries */
    range_prms_t s_range_prms, s_pic_limit, as_mv_limit[MAX_NUM_REF];

    /* Current and prev pic layer ctxt at the coarsest layer */
    layer_ctxt_t *ps_curr_layer, *ps_prev_layer;

    /* best mv of full search */
    hme_mv_t best_mv_4x8, best_mv_8x4;

    /* Book keeping at blk level */
    S32 blk_x, num_blks_in_pic, num_blks_in_row, num_4x4_blks_in_row;

    S32 blk_y;

    /* Block dimensions */
    S32 blk_size_shift = 2, blk_wd = 4, blk_ht = 4;

    S32 lambda = ps_coarse_prms->lambda;

    /* Number of references to search */
    S32 i4_num_ref;

    S32 i4_i, id, i;
    S08 i1_ref_idx;

    S32 i4_pic_wd, i4_pic_ht;
    S32 i4_layer_id;

    S32 end_of_frame;

    pf_get_wt_inp fp_get_wt_inp;

    /* Maximum search iterations around any candidate */
    S32 i4_max_iters = ps_coarse_prms->i4_max_iters;

    ps_curr_layer = ps_ctxt->ps_curr_descr->aps_layers[ps_coarse_prms->i4_layer_id];
    ps_prev_layer = hme_coarse_get_past_layer_ctxt(ps_ctxt, ps_coarse_prms->i4_layer_id);

    /* We need only one instance of search results structure */
    ps_search_results = &ps_ctxt->s_search_results_8x8;

    ps_search_candts_8x4 = &as_search_candts_8x4[0];
    ps_search_candts_4x8 = &as_search_candts_4x8[0];

    end_of_frame = 0;

    i4_pic_wd = ps_curr_layer->i4_wd;
    i4_pic_ht = ps_curr_layer->i4_ht;

    fp_get_wt_inp = ((ihevce_me_optimised_function_list_t *)ps_ctxt->pv_me_optimised_function_list)
                        ->pf_get_wt_inp_8x8;

    num_rows_coarse = ps_ctxt->i4_num_row_bufs;

    /*************************************************************************/
    /* Coarse Layer always does explicit search. Number of reference frames  */
    /* to search is a configurable parameter supplied by the application     */
    /*************************************************************************/
    i4_num_ref = ps_coarse_prms->i4_num_ref;
    i4_layer_id = ps_coarse_prms->i4_layer_id;

    /*************************************************************************/
    /*  The search algorithm goes as follows:                                */
    /*                                                                       */
    /*          ___                                                          */
    /*         | e |                                                         */
    /*      ___|___|___                                                      */
    /*     | c | a | b |                                                     */
    /*     |___|___|___|                                                     */
    /*         | d |                                                         */
    /*         |___|                                                         */
    /*                                                                       */
    /* For the target block a, we collect best results from 2 8x4 blks       */
    /* These are c-a and a-b. The 4x8 blks are e-a and a-d                   */
    /* c-a result is already available from results of blk c. a-b is         */
    /* evaluated in this blk. Likewise e-a result is stored in a row buffer  */
    /* a-d is evaluated this blk                                             */
    /* So we store a row buffer which stores best 4x8 results of all top blk */
    /*************************************************************************/

    /************************************************************************/
    /* Initialize the pointers to the best node.                            */
    /************************************************************************/
    for(i4_i = 0; i4_i < i4_num_ref; i4_i++)
    {
        aps_best_search_node_8x4[i4_i] = ps_search_results->aps_part_results[i4_i][PART_ID_2NxN_B];
        aps_best_search_node_4x8[i4_i] = ps_search_results->aps_part_results[i4_i][PART_ID_Nx2N_R];
    }

    /************************************************************************/
    /* Initialize the "searchresults" structure. This will set up the number*/
    /* of search types, result updates etc                                  */
    /************************************************************************/
    {
        S32 num_results_per_part;
        /* We evaluate 4 types of results per 4x4 blk. 8x4L and 8x4R and     */
        /* 4x8 T and 4x8B. So if we are to give 4 results, then we need to   */
        /* only evaluate 1 result per part. In the coarse layer, we are      */
        /* limited to 2 results max per part, and max of 8 results.          */
        num_results_per_part = (ps_coarse_prms->num_results + 3) >> 2;
        hme_init_search_results(
            ps_search_results,
            i4_num_ref,
            ps_coarse_prms->num_results,
            num_results_per_part,
            BLK_8x8,
            0,
            0,
            ps_ctxt->au1_is_past);
    }

    /* Macro updates num_blks_in_pic and num_blks_in_row*/
    GET_NUM_BLKS_IN_PIC(i4_pic_wd, i4_pic_ht, blk_size_shift, num_blks_in_row, num_blks_in_pic);

    num_4x4_blks_in_row = num_blks_in_row + 1;

    s_mv_update_prms.e_search_blk_size = e_search_blk_size;
    s_mv_update_prms.i4_num_ref = i4_num_ref;
    s_mv_update_prms.i4_shift = 0;

    /* For full search, support 2 or 4 step size */
    if(ps_coarse_prms->do_full_search)
    {
        ASSERT((ps_coarse_prms->full_search_step == 2) || (ps_coarse_prms->full_search_step == 4));
    }

    for(i4_i = 0; i4_i < i4_num_ref; i4_i++)
    {
        S32 blk, delta_poc;
        S32 mv_x_clip, mv_y_clip;
        /* Initialize only the first row */
        for(blk = 0; blk < num_blks_in_row; blk++)
        {
            INIT_SEARCH_NODE(&ps_ctxt->aps_best_search_nodes_4x8_n_rows[i4_i][blk], i4_i);
        }

        delta_poc = ABS(ps_curr_layer->i4_poc - ps_curr_layer->ai4_ref_id_to_poc_lc[i4_i]);

        /* Setting search range for different references based on the delta poc */
        /*************************************************************************/
        /* set the MV limit per ref. pic.                                        */
        /*    - P pic. : Based on the config params.                             */
        /*    - B/b pic: Based on the Max/Min MV from prev. P and config. param. */
        /*************************************************************************/
        {
            /* TO DO : Remove hard coding of P-P dist. of 4 */
            mv_x_clip = (ps_curr_layer->i2_max_mv_x * delta_poc) / 4;

            /* Only for B/b pic. */
            if(1 == ps_ctxt->s_frm_prms.bidir_enabled)
            {
                WORD16 i2_mv_y_per_poc;

                /* Get abs MAX for symmetric search */
                i2_mv_y_per_poc =
                    MAX(ps_ctxt->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[i4_layer_id],
                        (ABS(ps_ctxt->s_coarse_dyn_range_prms.i2_dyn_min_y_per_poc[i4_layer_id])));

                mv_y_clip = i2_mv_y_per_poc * delta_poc;
            }
            /* Set the Config. File Params for P pic. */
            else
            {
                /* TO DO : Remove hard coding of P-P dist. of 4 */
                mv_y_clip = (ps_curr_layer->i2_max_mv_y * delta_poc) / 4;
            }

            /* Making mv_x and mv_y range multiple of 4 */
            mv_x_clip = (((mv_x_clip + 3) >> 2) << 2);
            mv_y_clip = (((mv_y_clip + 3) >> 2) << 2);
            /* Clipping the range of mv_x and mv_y */
            mv_x_clip = CLIP3(mv_x_clip, 4, MAX_MVX_SUPPORTED_IN_COARSE_LAYER);
            mv_y_clip = CLIP3(mv_y_clip, 4, MAX_MVY_SUPPORTED_IN_COARSE_LAYER);

            as_mv_limit[i4_i].i2_min_x = -mv_x_clip;
            as_mv_limit[i4_i].i2_min_y = -mv_y_clip;
            as_mv_limit[i4_i].i2_max_x = mv_x_clip;
            as_mv_limit[i4_i].i2_max_y = mv_y_clip;
        }
        /*Populating SAD block size based on search range */
        ai4_sad_4x4_block_size[i4_i] = ((2 * mv_x_clip) / ps_coarse_prms->full_search_step) *
                                       ((2 * mv_y_clip) / ps_coarse_prms->full_search_step);
        ai4_sad_4x4_block_stride[i4_i] = (num_blks_in_row + 1) * ai4_sad_4x4_block_size[i4_i];
    }

    for(i = 0; i < 2 * MAX_INIT_CANDTS; i++)
    {
        search_node_t *ps_search_node;
        ps_search_node = &ps_ctxt->s_init_search_node[i];
        INIT_SEARCH_NODE(ps_search_node, 0);
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
    /* Set up place holders to hold the search nodes of each initial candt */
    for(i = 0; i < MAX_INIT_CANDTS; i++)
    {
        ps_search_candts_8x4[i].ps_search_node = &ps_ctxt->s_init_search_node[i];

        ps_search_candts_4x8[i].ps_search_node = &ps_ctxt->s_init_search_node[MAX_INIT_CANDTS + i];

        ps_search_candts_8x4[i].u1_num_steps_refine = (U08)i4_max_iters;
        ps_search_candts_4x8[i].u1_num_steps_refine = (U08)i4_max_iters;
    }

    /* For Top,TopLeft and Left cand., no need for refinement */
    id = 0;
    if((ps_coarse_prms->do_full_search) && (ME_XTREME_SPEED_25 == e_me_quality_preset))
    {
        /* This search candt has the full search result */
        ps_candt_fs_8x4 = ps_search_candts_8x4[id].ps_search_node;
        id++;
    }

    ps_candt_8x4_l = ps_search_candts_8x4[id].ps_search_node;
    ps_search_candts_8x4[id].u1_num_steps_refine = 0;
    id++;
    ps_candt_8x4_t = ps_search_candts_8x4[id].ps_search_node;
    ps_search_candts_8x4[id].u1_num_steps_refine = 0;
    id++;
    ps_candt_8x4_tl = ps_search_candts_8x4[id].ps_search_node;
    ps_search_candts_8x4[id].u1_num_steps_refine = 0;
    id++;
    /* This search candt stores the global candt */
    global_id_8x4 = id;
    id++;

    if((ps_coarse_prms->do_full_search) && (ME_XTREME_SPEED_25 != e_me_quality_preset))
    {
        /* This search candt has the full search result */
        ps_candt_fs_8x4 = ps_search_candts_8x4[id].ps_search_node;
        id++;
    }
    /* Don't increment id as (0,0) is removed from cand. list. Initializing */
    /* the pointer for hme_init_pred_ctxt_no_encode()                       */
    ps_candt_zeromv_8x4 = ps_search_candts_8x4[id].ps_search_node;

    /* For Top,TopLeft and Left cand., no need for refinement */
    id = 0;
    if((ps_coarse_prms->do_full_search) && (ME_XTREME_SPEED_25 == e_me_quality_preset))
    {
        /* This search candt has the full search result */
        ps_candt_fs_4x8 = ps_search_candts_4x8[id].ps_search_node;
        id++;
    }

    ps_candt_4x8_l = ps_search_candts_4x8[id].ps_search_node;
    ps_search_candts_4x8[id].u1_num_steps_refine = 0;
    id++;
    ps_candt_4x8_t = ps_search_candts_4x8[id].ps_search_node;
    ps_search_candts_4x8[id].u1_num_steps_refine = 0;
    id++;
    ps_candt_4x8_tl = ps_search_candts_4x8[id].ps_search_node;
    ps_search_candts_4x8[id].u1_num_steps_refine = 0;
    id++;
    /* This search candt stores the global candt */
    global_id_4x8 = id;
    id++;
    if((ps_coarse_prms->do_full_search) && (ME_XTREME_SPEED_25 != e_me_quality_preset))
    {
        /* This search candt has the full search result */
        ps_candt_fs_4x8 = ps_search_candts_4x8[id].ps_search_node;
        id++;
    }
    /* Don't increment id4as (0,0) is removed from cand. list. Initializing */
    /* the pointer for hme_init_pred_ctxt_no_encode()                       */
    ps_candt_zeromv_4x8 = ps_search_candts_4x8[id].ps_search_node;

    /* Zero mv always has 0 mvx and y componnent, ref idx initialized inside */
    ps_candt_zeromv_8x4->s_mv.i2_mvx = 0;
    ps_candt_zeromv_8x4->s_mv.i2_mvy = 0;
    ps_candt_zeromv_4x8->s_mv.i2_mvx = 0;
    ps_candt_zeromv_4x8->s_mv.i2_mvy = 0;

    /* SET UP THE PRED CTXT FOR L0 AND L1 */
    {
        S32 pred_lx;

        /* Bottom left always not available */
        as_left_neighbours[2].u1_is_avail = 0;

        for(pred_lx = 0; pred_lx < 2; pred_lx++)
        {
            pred_ctxt_t *ps_pred_ctxt;

            ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
            hme_init_pred_ctxt_no_encode(
                ps_pred_ctxt,
                ps_search_results,
                as_top_neighbours,
                as_left_neighbours,
                NULL,
                ps_candt_zeromv_8x4,
                ps_candt_zeromv_8x4,
                pred_lx,
                lambda,
                ps_coarse_prms->lambda_q_shift,
                ps_ctxt->apu1_ref_bits_tlu_lc,
                ps_ctxt->ai2_ref_scf);
        }
    }

    /*************************************************************************/
    /* Initialize the search parameters for search algo with the following   */
    /* parameters: No SATD, calculated number of initial candidates,         */
    /* No post refinement, initial step size and number of iterations as     */
    /* passed by the calling function.                                       */
    /* Also, we use input for this layer search, and not recon.              */
    /*************************************************************************/
    if(e_me_quality_preset == ME_XTREME_SPEED_25)
        s_search_prms_8x4.i4_num_init_candts = 1;
    else
        s_search_prms_8x4.i4_num_init_candts = id;
    s_search_prms_8x4.i4_use_satd = 0;
    s_search_prms_8x4.i4_start_step = ps_coarse_prms->i4_start_step;
    s_search_prms_8x4.i4_num_steps_post_refine = 0;
    s_search_prms_8x4.i4_use_rec = 0;
    s_search_prms_8x4.ps_search_candts = ps_search_candts_8x4;
    s_search_prms_8x4.e_blk_size = BLK_8x4;
    s_search_prms_8x4.i4_max_iters = ps_coarse_prms->i4_max_iters;
    /* Coarse layer is always explicit */
    if(ME_MEDIUM_SPEED > e_me_quality_preset)
    {
        s_search_prms_8x4.pf_mv_cost_compute = compute_mv_cost_coarse;
    }
    else
    {
        s_search_prms_8x4.pf_mv_cost_compute = compute_mv_cost_coarse_high_speed;
    }

    s_search_prms_8x4.i4_inp_stride = 8;
    s_search_prms_8x4.i4_cu_x_off = s_search_prms_8x4.i4_cu_y_off = 0;
    if(ps_coarse_prms->do_full_search)
        s_search_prms_8x4.i4_max_iters = 1;
    s_search_prms_8x4.i4_part_mask = (1 << PART_ID_2NxN_B);
    /* Using the member 0 to store for all ref. idx. */
    s_search_prms_8x4.aps_mv_range[0] = &s_range_prms;
    s_search_prms_8x4.ps_search_results = ps_search_results;
    s_search_prms_8x4.full_search_step = ps_coarse_prms->full_search_step;

    s_search_prms_4x8 = s_search_prms_8x4;
    s_search_prms_4x8.ps_search_candts = ps_search_candts_4x8;
    s_search_prms_4x8.e_blk_size = BLK_4x8;
    s_search_prms_4x8.i4_part_mask = (1 << PART_ID_Nx2N_R);

    s_search_prms_4x4 = s_search_prms_8x4;
    /* Since s_search_prms_4x4 is used only to computer sad at 4x4 level, search candidate is not used */
    s_search_prms_4x4.ps_search_candts = ps_search_candts_4x8;
    s_search_prms_4x4.e_blk_size = BLK_4x4;
    s_search_prms_4x4.i4_part_mask = (1 << PART_ID_2Nx2N);
    /*************************************************************************/
    /* Picture limit on all 4 sides. This will be used to set mv limits for  */
    /* every block given its coordinate.                                     */
    /*************************************************************************/
    SET_PIC_LIMIT(
        s_pic_limit,
        ps_curr_layer->i4_pad_x_inp,
        ps_curr_layer->i4_pad_y_inp,
        ps_curr_layer->i4_wd,
        ps_curr_layer->i4_ht,
        s_search_prms_4x4.i4_num_steps_post_refine);

    /* Pick the global mv from previous reference */
    for(i1_ref_idx = 0; i1_ref_idx < i4_num_ref; i1_ref_idx++)
    {
        if(ME_XTREME_SPEED_25 != e_me_quality_preset)
        {
            /* Distance of current pic from reference */
            S32 i4_delta_poc;

            hme_mv_t s_mv;
            i4_delta_poc = ps_curr_layer->i4_poc - ps_curr_layer->ai4_ref_id_to_poc_lc[i1_ref_idx];

            hme_get_global_mv(ps_prev_layer, &s_mv, i4_delta_poc);

            s_candt_global[i1_ref_idx].s_mv.i2_mvx = s_mv.i2_mv_x;
            s_candt_global[i1_ref_idx].s_mv.i2_mvy = s_mv.i2_mv_y;
            s_candt_global[i1_ref_idx].i1_ref_idx = i1_ref_idx;

            /*********************************************************************/
            /* Initialize the histogram for each reference index in current      */
            /* layer ctxt                                                        */
            /*********************************************************************/
            hme_init_histogram(
                ps_ctxt->aps_mv_hist[i1_ref_idx],
                (S32)as_mv_limit[i1_ref_idx].i2_max_x,
                (S32)as_mv_limit[i1_ref_idx].i2_max_y);
        }

        /*********************************************************************/
        /* Initialize the dyn. search range params. for each reference index */
        /* in current layer ctxt                                             */
        /*********************************************************************/
        /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
        if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
        {
            INIT_DYN_SEARCH_PRMS(
                &ps_ctxt->s_coarse_dyn_range_prms.as_dyn_range_prms[i4_layer_id][i1_ref_idx],
                ps_curr_layer->ai4_ref_id_to_poc_lc[i1_ref_idx]);
        }
    }

    /*************************************************************************/
    /* if exhaustive algorithmm then we use only 1 candt 0, 0                */
    /* else we use a lot of causal and non causal candts                     */
    /* finally set number to the configured number of candts                 */
    /*************************************************************************/

    /* Loop in raster order over each 4x4 blk in a given row till end of frame */
    while(0 == end_of_frame)
    {
        job_queue_t *ps_job;
        void *pv_hme_dep_mngr;
        WORD32 offset_val, check_dep_pos, set_dep_pos;

        /* Get the current layer HME Dep Mngr       */
        /* Note : Use layer_id - 1 in HME layers    */
        pv_hme_dep_mngr = ppv_dep_mngr_hme_sync[ps_coarse_prms->i4_layer_id - 1];

        /* Get the current row from the job queue */
        ps_job = (job_queue_t *)ihevce_pre_enc_grp_get_next_job(
            ps_multi_thrd_ctxt, ps_multi_thrd_ctxt->i4_me_coarsest_lyr_type, 1, i4_ping_pong);

        /* If all rows are done, set the end of process flag to 1, */
        /* and the current row to -1 */
        if(NULL == ps_job)
        {
            blk_y = -1;
            end_of_frame = 1;
        }
        else
        {
            ASSERT(ps_multi_thrd_ctxt->i4_me_coarsest_lyr_type == ps_job->i4_pre_enc_task_type);

            /* Obtain the current row's details from the job */
            blk_y = ps_job->s_job_info.s_me_job_info.i4_vert_unit_row_no;

            if(1 == ps_ctxt->s_frm_prms.is_i_pic)
            {
                /* set the output dependency of current row */
                ihevce_pre_enc_grp_job_set_out_dep(ps_multi_thrd_ctxt, ps_job, i4_ping_pong);
                continue;
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

            /* Loop over all the blocks in current row */
            /* One block extra, since the last block in a row needs East block */
            for(blk_x = 0; blk_x < (num_blks_in_row + 1); blk_x++)
            {
                /* Wait till top row block is processed   */
                /* Currently checking till top right block*/
                if(blk_x < (num_blks_in_row))
                {
                    ihevce_dmgr_chk_row_row_sync(
                        pv_hme_dep_mngr,
                        blk_x,
                        offset_val,
                        check_dep_pos,
                        0, /* Col Tile No. : Not supported in PreEnc*/
                        ps_ctxt->thrd_id);
                }

                /***************************************************************/
                /* Get Weighted input for all references                       */
                /***************************************************************/
                fp_get_wt_inp(
                    ps_curr_layer,
                    &ps_ctxt->s_wt_pred,
                    1 << (blk_size_shift + 1),
                    blk_x << blk_size_shift,
                    (blk_y - 1) << blk_size_shift,
                    1 << (blk_size_shift + 1),
                    i4_num_ref,
                    ps_ctxt->i4_wt_pred_enable_flag);

                /* RESET ALL SEARCH RESULTS FOR THE NEW BLK */
                hme_reset_search_results(
                    ps_search_results,
                    s_search_prms_8x4.i4_part_mask | s_search_prms_4x8.i4_part_mask,
                    MV_RES_FPEL);

                /* Compute the search node offsets */
                /* MAX is used to clip when left and top neighbours are not availbale at coarse boundaries  */
                search_node_top_offset =
                    blk_x + ps_ctxt->ai4_row_index[MAX((blk_y - 2), 0)] * num_blks_in_row;
                search_node_left_offset =
                    MAX((blk_x - 1), 0) +
                    ps_ctxt->ai4_row_index[MAX((blk_y - 1), 0)] * num_blks_in_row;

                /* Input offset: wrt CU start. Offset for South block */
                s_search_prms_4x4.i4_cu_x_off = 0;
                s_search_prms_4x4.i4_cu_y_off = 4;
                s_search_prms_4x4.i4_inp_stride = 8;
                s_search_prms_4x4.i4_x_off = blk_x << blk_size_shift;
                s_search_prms_4x4.i4_y_off = blk_y << blk_size_shift;

                s_search_prms_4x8.i4_x_off = s_search_prms_8x4.i4_x_off = blk_x << blk_size_shift;
                s_search_prms_4x8.i4_y_off = s_search_prms_8x4.i4_y_off = (blk_y - 1)
                                                                          << blk_size_shift;

                /* This layer will always use explicit ME */
                /* Loop across different Ref IDx */
                for(i1_ref_idx = 0; i1_ref_idx < i4_num_ref; i1_ref_idx++)
                {
                    sad_top_offset = (blk_x * ai4_sad_4x4_block_size[i1_ref_idx]) +
                                     ps_ctxt->ai4_row_index[MAX((blk_y - 1), 0)] *
                                         ai4_sad_4x4_block_stride[i1_ref_idx];
                    sad_current_offset =
                        (blk_x * ai4_sad_4x4_block_size[i1_ref_idx]) +
                        ps_ctxt->ai4_row_index[blk_y] * ai4_sad_4x4_block_stride[i1_ref_idx];

                    /* Initialize search node if blk_x == 0, as it doesn't have left neighbours */
                    if(0 == blk_x)
                        INIT_SEARCH_NODE(
                            &ps_ctxt->aps_best_search_nodes_8x4_n_rows[i1_ref_idx][blk_x],
                            i1_ref_idx);

                    pi2_cur_ref_sads_4x4 = ps_ctxt->api2_sads_4x4_n_rows[i1_ref_idx];

                    /* Initialize changing params here */
                    s_search_prms_8x4.i1_ref_idx = i1_ref_idx;
                    s_search_prms_4x8.i1_ref_idx = i1_ref_idx;
                    s_search_prms_4x4.i1_ref_idx = i1_ref_idx;

                    if(num_blks_in_row == blk_x)
                    {
                        S16 *pi2_sads_4x4_current;
                        /* Since the current 4x4 block will be a padded region, which may not match with any of the reference  */
                        pi2_sads_4x4_current = pi2_cur_ref_sads_4x4 + sad_current_offset;

                        memset(pi2_sads_4x4_current, 0, ai4_sad_4x4_block_size[i1_ref_idx]);
                    }

                    /* SAD to be computed and stored for the 4x4 block in 1st row and the last block of all rows*/
                    if((0 == blk_y) || (num_blks_in_row == blk_x))
                    {
                        S16 *pi2_sads_4x4_current;
                        /* Computer 4x4 SADs for current block */
                        /* Pointer to store SADs */
                        pi2_sads_4x4_current = pi2_cur_ref_sads_4x4 + sad_current_offset;

                        hme_derive_worst_case_search_range(
                            &s_range_prms,
                            &s_pic_limit,
                            &as_mv_limit[i1_ref_idx],
                            blk_x << blk_size_shift,
                            blk_y << blk_size_shift,
                            blk_wd,
                            blk_ht);

                        if(ME_PRISTINE_QUALITY >= e_me_quality_preset)
                        {
                            ((ihevce_me_optimised_function_list_t *)
                                 ps_ctxt->pv_me_optimised_function_list)
                                ->pf_store_4x4_sads_high_quality(
                                    &s_search_prms_4x4,
                                    ps_curr_layer,
                                    &as_mv_limit[i1_ref_idx],
                                    &ps_ctxt->s_wt_pred,
                                    pi2_sads_4x4_current);
                        }
                        else
                        {
                            ((ihevce_me_optimised_function_list_t *)
                                 ps_ctxt->pv_me_optimised_function_list)
                                ->pf_store_4x4_sads_high_speed(
                                    &s_search_prms_4x4,
                                    ps_curr_layer,
                                    &as_mv_limit[i1_ref_idx],
                                    &ps_ctxt->s_wt_pred,
                                    pi2_sads_4x4_current);
                        }
                    }
                    else
                    {
                        /* For the zero mv candt, the ref idx to be modified */
                        ps_candt_zeromv_8x4->i1_ref_idx = i1_ref_idx;
                        ps_candt_zeromv_4x8->i1_ref_idx = i1_ref_idx;

                        if(ME_XTREME_SPEED_25 != e_me_quality_preset)
                        {
                            /* For the global mvs alone, the search node points to a local variable */
                            ps_search_candts_8x4[global_id_8x4].ps_search_node =
                                &s_candt_global[i1_ref_idx];
                            ps_search_candts_4x8[global_id_4x8].ps_search_node =
                                &s_candt_global[i1_ref_idx];
                        }

                        hme_get_spatial_candt(
                            ps_curr_layer,
                            BLK_4x4,
                            blk_x,
                            blk_y - 1,
                            i1_ref_idx,
                            as_top_neighbours,
                            as_left_neighbours,
                            0,
                            1,
                            0,
                            0);
                        /* set up the various candts */
                        *ps_candt_4x8_l = as_left_neighbours[0];
                        *ps_candt_4x8_t = as_top_neighbours[1];
                        *ps_candt_4x8_tl = as_top_neighbours[0];
                        *ps_candt_8x4_l = *ps_candt_4x8_l;
                        *ps_candt_8x4_tl = *ps_candt_4x8_tl;
                        *ps_candt_8x4_t = *ps_candt_4x8_t;

                        {
                            S32 pred_lx;
                            S16 *pi2_sads_4x4_current, *pi2_sads_4x4_top;
                            pred_ctxt_t *ps_pred_ctxt;
                            PF_MV_COST_FXN pf_mv_cost_compute;

                            /* Computer 4x4 SADs for current block */
                            /* Pointer to store SADs */
                            pi2_sads_4x4_current = pi2_cur_ref_sads_4x4 + sad_current_offset;

                            hme_derive_worst_case_search_range(
                                &s_range_prms,
                                &s_pic_limit,
                                &as_mv_limit[i1_ref_idx],
                                blk_x << blk_size_shift,
                                blk_y << blk_size_shift,
                                blk_wd,
                                blk_ht);
                            if(i4_pic_ht == blk_y)
                            {
                                memset(pi2_sads_4x4_current, 0, ai4_sad_4x4_block_size[i1_ref_idx]);
                            }
                            else
                            {
                                if(ME_PRISTINE_QUALITY >= e_me_quality_preset)
                                {
                                    ((ihevce_me_optimised_function_list_t *)
                                         ps_ctxt->pv_me_optimised_function_list)
                                        ->pf_store_4x4_sads_high_quality(
                                            &s_search_prms_4x4,
                                            ps_curr_layer,
                                            &as_mv_limit[i1_ref_idx],
                                            &ps_ctxt->s_wt_pred,
                                            pi2_sads_4x4_current);
                                }
                                else
                                {
                                    ((ihevce_me_optimised_function_list_t *)
                                         ps_ctxt->pv_me_optimised_function_list)
                                        ->pf_store_4x4_sads_high_speed(
                                            &s_search_prms_4x4,
                                            ps_curr_layer,
                                            &as_mv_limit[i1_ref_idx],
                                            &ps_ctxt->s_wt_pred,
                                            pi2_sads_4x4_current);
                                }
                            }
                            /* Set pred direction to L0 or L1 */
                            pred_lx = 1 - ps_search_results->pu1_is_past[i1_ref_idx];

                            /* Suitable context (L0 or L1) */
                            ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];

                            /* Coarse layer is always explicit */
                            if(ME_PRISTINE_QUALITY > e_me_quality_preset)
                            {
                                pf_mv_cost_compute = compute_mv_cost_coarse;
                            }
                            else
                            {
                                /* Cost function is not called in high speed case. Below one is just a dummy function */
                                pf_mv_cost_compute = compute_mv_cost_coarse_high_speed;
                            }

                            /*********************************************************************/
                            /* Now, compute the mv for the top block                             */
                            /*********************************************************************/
                            pi2_sads_4x4_top = pi2_cur_ref_sads_4x4 + sad_top_offset;

                            /*********************************************************************/
                            /* For every blk in the picture, the search range needs to be derived*/
                            /* Any blk can have any mv, but practical search constraints are     */
                            /* imposed by the picture boundary and amt of padding.               */
                            /*********************************************************************/
                            hme_derive_search_range(
                                &s_range_prms,
                                &s_pic_limit,
                                &as_mv_limit[i1_ref_idx],
                                blk_x << blk_size_shift,
                                (blk_y - 1) << blk_size_shift,
                                blk_wd,
                                blk_ht);

                            /* Computer the mv for the top block */
                            if(ME_PRISTINE_QUALITY >= e_me_quality_preset)
                            {
                                ((ihevce_me_optimised_function_list_t *)
                                     ps_ctxt->pv_me_optimised_function_list)
                                    ->pf_combine_4x4_sads_and_compute_cost_high_quality(
                                        i1_ref_idx,
                                        &s_range_prms, /* Both 4x8 and 8x4 has same search range */
                                        &as_mv_limit[i1_ref_idx],
                                        &best_mv_4x8,
                                        &best_mv_8x4,
                                        ps_pred_ctxt,
                                        pf_mv_cost_compute,
                                        pi2_sads_4x4_top, /* Current SAD block */
                                        (pi2_sads_4x4_top +
                                         ai4_sad_4x4_block_size[i1_ref_idx]), /* East SAD block */
                                        pi2_sads_4x4_current); /* South SAD block */
                            }
                            else
                            {
                                ((ihevce_me_optimised_function_list_t *)
                                     ps_ctxt->pv_me_optimised_function_list)
                                    ->pf_combine_4x4_sads_and_compute_cost_high_speed(
                                        i1_ref_idx,
                                        &s_range_prms, /* Both 4x8 and 8x4 has same search range */
                                        &as_mv_limit[i1_ref_idx],
                                        &best_mv_4x8,
                                        &best_mv_8x4,
                                        ps_pred_ctxt,
                                        pf_mv_cost_compute,
                                        pi2_sads_4x4_top, /* Current SAD block */
                                        (pi2_sads_4x4_top +
                                         ai4_sad_4x4_block_size[i1_ref_idx]), /* East SAD block */
                                        pi2_sads_4x4_current); /* South SAD block */
                            }

                            ps_candt_fs_4x8->s_mv.i2_mvx = best_mv_4x8.i2_mv_x;
                            ps_candt_fs_4x8->s_mv.i2_mvy = best_mv_4x8.i2_mv_y;
                            ps_candt_fs_4x8->i1_ref_idx = i1_ref_idx;

                            ps_candt_fs_8x4->s_mv.i2_mvx = best_mv_8x4.i2_mv_x;
                            ps_candt_fs_8x4->s_mv.i2_mvy = best_mv_8x4.i2_mv_y;
                            ps_candt_fs_8x4->i1_ref_idx = i1_ref_idx;
                        }

                        /* call the appropriate Search Algo for 4x8S. The 4x8N would  */
                        /* have already been called by top block */
                        hme_pred_search_square_stepn(
                            &s_search_prms_8x4,
                            ps_curr_layer,
                            &ps_ctxt->s_wt_pred,
                            e_me_quality_preset,
                            (ihevce_me_optimised_function_list_t *)
                                ps_ctxt->pv_me_optimised_function_list

                        );

                        /* Call the appropriate search algo for 8x4E */
                        hme_pred_search_square_stepn(
                            &s_search_prms_4x8,
                            ps_curr_layer,
                            &ps_ctxt->s_wt_pred,
                            e_me_quality_preset,
                            (ihevce_me_optimised_function_list_t *)
                                ps_ctxt->pv_me_optimised_function_list);

                        if(ME_XTREME_SPEED_25 != e_me_quality_preset)
                        {
                            /* Histogram updates across different Ref ID for global MV */
                            hme_update_histogram(
                                ps_ctxt->aps_mv_hist[i1_ref_idx],
                                aps_best_search_node_8x4[i1_ref_idx]->s_mv.i2_mvx,
                                aps_best_search_node_8x4[i1_ref_idx]->s_mv.i2_mvy);
                            hme_update_histogram(
                                ps_ctxt->aps_mv_hist[i1_ref_idx],
                                aps_best_search_node_4x8[i1_ref_idx]->s_mv.i2_mvx,
                                aps_best_search_node_4x8[i1_ref_idx]->s_mv.i2_mvy);
                        }

                        /* update the best results to the mv bank */
                        hme_update_mv_bank_coarse(
                            ps_search_results,
                            ps_curr_layer->ps_layer_mvbank,
                            blk_x,
                            (blk_y - 1),
                            ps_ctxt->aps_best_search_nodes_4x8_n_rows[i1_ref_idx] +
                                search_node_top_offset, /* Top Candidate */
                            ps_ctxt->aps_best_search_nodes_8x4_n_rows[i1_ref_idx] +
                                search_node_left_offset, /* Left candidate */
                            i1_ref_idx,
                            &s_mv_update_prms);

                        /* Copy the best search result to 5 row array for future use */
                        *(ps_ctxt->aps_best_search_nodes_4x8_n_rows[i1_ref_idx] + blk_x +
                          ps_ctxt->ai4_row_index[blk_y - 1] * num_blks_in_row) =
                            *(aps_best_search_node_4x8[i1_ref_idx]);

                        *(ps_ctxt->aps_best_search_nodes_8x4_n_rows[i1_ref_idx] + blk_x +
                          ps_ctxt->ai4_row_index[blk_y - 1] * num_blks_in_row) =
                            *(aps_best_search_node_8x4[i1_ref_idx]);

                        /* UPDATE the MIN and MAX MVs for Dynamical Search Range for each ref. pic. */
                        /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
                        if(ps_ctxt->s_frm_prms.is_i_pic == ps_ctxt->s_frm_prms.bidir_enabled)
                        {
                            WORD32 num_mvs, i, j;
                            search_node_t *aps_search_nodes[4];
                            /* Best results for 8x4R and 4x8B blocks */
                            search_node_t *ps_search_node_8x4_r, *ps_search_node_4x8_b;

                            num_mvs = ps_curr_layer->ps_layer_mvbank->i4_num_mvs_per_ref;

                            /*************************************************************************/
                            /* We have atleast 4 distinct results: the 4x8 top (coming from top blk) */
                            /* 8x4 left (coming from left blk), 8x4 and 4x8 right and bot resp.      */
                            /* If number of results to be stored is 4, then we store all these 4     */
                            /* results, else we pick best ones                                       */
                            /*************************************************************************/
                            ps_search_node_8x4_r =
                                ps_search_results->aps_part_results[i1_ref_idx][PART_ID_2NxN_B];
                            ps_search_node_4x8_b =
                                ps_search_results->aps_part_results[i1_ref_idx][PART_ID_Nx2N_R];

                            ASSERT(num_mvs <= 4);

                            /* Doing this to sort best results */
                            aps_search_nodes[0] = ps_search_node_8x4_r;
                            aps_search_nodes[1] = ps_search_node_4x8_b;
                            aps_search_nodes[2] =
                                ps_ctxt->aps_best_search_nodes_8x4_n_rows[i1_ref_idx] +
                                search_node_left_offset; /* Left candidate */
                            aps_search_nodes[3] =
                                ps_ctxt->aps_best_search_nodes_4x8_n_rows[i1_ref_idx] +
                                search_node_top_offset; /* Top Candidate */

                            /* Note : Need to be resolved!!! */
                            /* Added this to match with "hme_update_mv_bank_coarse" */
                            if(num_mvs != 4)
                            {
                                /* Run through the results, store them in best to worst order */
                                for(i = 0; i < num_mvs; i++)
                                {
                                    for(j = i + 1; j < 4; j++)
                                    {
                                        if(aps_search_nodes[j]->i4_tot_cost <
                                           aps_search_nodes[i]->i4_tot_cost)
                                        {
                                            SWAP_HME(
                                                aps_search_nodes[j],
                                                aps_search_nodes[i],
                                                search_node_t *);
                                        }
                                    }
                                }
                            }

                            /* UPDATE the MIN and MAX MVs for Dynamical Search Range for each ref. pic. */
                            for(i = 0; i < num_mvs; i++)
                            {
                                hme_update_dynamic_search_params(
                                    &ps_ctxt->s_coarse_dyn_range_prms
                                         .as_dyn_range_prms[i4_layer_id][i1_ref_idx],
                                    aps_search_nodes[i]->s_mv.i2_mvy);
                            }
                        }
                    }
                }

                /* Update the number of blocks processed in the current row */
                ihevce_dmgr_set_row_row_sync(
                    pv_hme_dep_mngr,
                    (blk_x + 1),
                    blk_y,
                    0 /* Col Tile No. : Not supported in PreEnc*/);
            }

            /* set the output dependency after completion of row */
            ihevce_pre_enc_grp_job_set_out_dep(ps_multi_thrd_ctxt, ps_job, i4_ping_pong);
        }
    }

    return;
}
