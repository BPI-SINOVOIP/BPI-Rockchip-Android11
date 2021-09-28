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
* @file hme_search_algo.c
*
* @brief
*    Contains various search algorithms to be used by coarse/refinement layers
*
* @author
*    Ittiam
*
*
* List of Functions
* hme_compute_grid_results_step_gt_1()
* hme_compute_grid_results_step_1()
* hme_pred_search_square_stepn()
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
/* Function Definitions                                                      */
/*****************************************************************************/

/**
********************************************************************************
*  @fn     void hme_compute_grid_results_step_1(err_prms_t *ps_err_prms,
result_upd_prms_t *ps_result_prms,
BLK_SIZE_T e_blk_size)
*
*  @brief  Updates results for a grid of step = 1
*
*  @param[in] ps_err_prms: Various parameters to this function
*
*  @param[in] ps_result_prms : Parameters pertaining to result updation
*
*  @param[out] e_blk_size: Block size of the blk being searched for
*
*  @return none
********************************************************************************
*/
void hme_compute_grid_results(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms, BLK_SIZE_T e_blk_size)
{
    PF_RESULT_FXN_T pf_hme_result_fxn;
    PF_SAD_FXN_T pf_sad_fxn;
    S32 i4_num_results;
    S32 part_id;

    part_id = ps_result_prms->pi4_valid_part_ids[0];

    i4_num_results = (S32)ps_result_prms->ps_search_results->u1_num_results_per_part;

    pf_sad_fxn = hme_get_sad_fxn(e_blk_size, ps_err_prms->i4_grid_mask, ps_err_prms->i4_part_mask);

    pf_hme_result_fxn =
        hme_get_result_fxn(ps_err_prms->i4_grid_mask, ps_err_prms->i4_part_mask, i4_num_results);

    pf_sad_fxn(ps_err_prms);
    pf_hme_result_fxn(ps_result_prms);
}

/**
********************************************************************************
*  @fn     void hme_pred_search_square_stepn(hme_search_prms_t *ps_search_prms,
*                                   layer_ctxt_t *ps_layer_ctxt)
*
*  @brief  Implements predictive search, with square grid refinement. In this
*          case, we start with a bigger step size, like 4, refining upto a
*          variable number of pts, till we hit end of search range or hit a
*          minima. Then we refine using smaller steps. The bigger step size
*          like 4 or 2, do not use optimized SAD functions, they evaluate
*          SAD for each individual pt.
*
*  @param[in,out]  ps_search_prms: All the params to this function
*
*  @param[in] ps_layer_ctxt: Context for the layer
*
*  @return None
********************************************************************************
*/
void hme_pred_search_square_stepn(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    ME_QUALITY_PRESETS_T e_me_quality_preset,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list

)
{
    /* Stores the SAD for all parts at each pt in the grid */
    S32 ai4_sad_grid[9][TOT_NUM_PARTS];

    S32 ai4_valid_part_ids[TOT_NUM_PARTS + 1];

    /* Atributes of input candidates */
    search_candt_t *ps_search_candts;
    search_node_t s_search_node;

    /* Number of candidates to search */
    S32 i4_num_candts, max_num_iters, i4_num_results;

    /* Input and reference attributes */
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref;

    /* Holds the search results at the end of this fxn */
    search_results_t *ps_search_results;

    /* These control number of parts and number of pts in grid to search */
    S32 i4_part_mask, i4_grid_mask;

    /* Blk width, blk height and blk size are derived from input params */
    BLK_SIZE_T e_blk_size;
    CU_SIZE_T e_cu_size;
    S32 i4_blk_wd, i4_blk_ht, i4_step, i4_candt, i4_iter;
    S32 i4_inp_off;
    S32 i4_min_id;
    /* Points to the range limits for mv */
    range_prms_t *ps_range_prms;

    /*************************************************************************/
    /* These functions pointers for calculating Err and the result update    */
    /* Each carries its own parameters structure, which is generated on the  */
    /* fly in this function                                                  */
    /*************************************************************************/
    err_prms_t s_err_prms;
    result_upd_prms_t s_result_prms;

    max_num_iters = ps_search_prms->i4_max_iters;
    /* Using the member 0 to store for all ref. idx., see in coarsest */
    ps_range_prms = ps_search_prms->aps_mv_range[0];
    i4_inp_stride = ps_search_prms->i4_inp_stride;
    /* Move to the location of the search blk in inp buffer */
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += (ps_search_prms->i4_cu_y_off * i4_inp_stride);

    ps_search_results = ps_search_prms->ps_search_results;

    /*************************************************************************/
    /* Depending on flag i4_use_rec, we use either input of previously       */
    /* encoded pictures or we use recon of previously encoded pictures.      */
    /*************************************************************************/
    if(ps_search_prms->i4_use_rec == 1)
    {
        i4_ref_stride = ps_layer_ctxt->i4_rec_stride;
        ppu1_ref = ps_layer_ctxt->ppu1_list_rec_fxfy;
    }
    else
    {
        i4_ref_stride = ps_layer_ctxt->i4_inp_stride;
        ppu1_ref = ps_layer_ctxt->ppu1_list_inp;
    }
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;

    /*************************************************************************/
    /* Obtain the blk size of the search blk. Assumed here that the search   */
    /* is done on a CU size, rather than any arbitrary blk size.             */
    /*************************************************************************/
    ps_search_results = ps_search_prms->ps_search_results;
    e_blk_size = ps_search_prms->e_blk_size;
    i4_blk_wd = (S32)gau1_blk_size_to_wd[e_blk_size];
    i4_blk_ht = (S32)gau1_blk_size_to_ht[e_blk_size];
    e_cu_size = ps_search_results->e_cu_size;
    i4_num_results = (S32)ps_search_results->u1_num_results_per_part;

    ps_search_candts = ps_search_prms->ps_search_candts;
    i4_num_candts = ps_search_prms->i4_num_init_candts;
    i4_part_mask = ps_search_prms->i4_part_mask;

    /*************************************************************************/
    /* This array stores the ids of the partitions whose                     */
    /* SADs are updated. Since the partitions whose SADs are updated may not */
    /* be in contiguous order, we supply another level of indirection.       */
    /*************************************************************************/
    hme_create_valid_part_ids(i4_part_mask, ai4_valid_part_ids);

    /* Update the parameters used to pass to SAD */
    /* input ptr, strides, SAD Grid, part mask, blk width and ht */
    /* The above are fixed ptrs, only pu1_ref and grid mask are  */
    /* varying params which are updated just before calling fxn  */
    s_err_prms.i4_inp_stride = i4_inp_stride;
    s_err_prms.i4_ref_stride = i4_ref_stride;
    s_err_prms.i4_part_mask = i4_part_mask;
    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0][0];
    s_err_prms.i4_blk_wd = i4_blk_wd;
    s_err_prms.i4_blk_ht = i4_blk_ht;
    s_err_prms.pi4_valid_part_ids = ai4_valid_part_ids;

    s_result_prms.pf_mv_cost_compute = ps_search_prms->pf_mv_cost_compute;
    s_result_prms.ps_search_results = ps_search_results;
    s_result_prms.pi4_valid_part_ids = ai4_valid_part_ids;
    s_result_prms.i1_ref_idx = ps_search_prms->i1_ref_idx;
    s_result_prms.i4_part_mask = ps_search_prms->i4_part_mask;
    s_result_prms.ps_search_node_base = &s_search_node;
    s_result_prms.pi4_sad_grid = &ai4_sad_grid[0][0];

    /* Run through each of the candts in a loop */
    for(i4_candt = 0; i4_candt < i4_num_candts; i4_candt++)
    {
        S32 i4_num_refine;

        i4_step = ps_search_prms->i4_start_step;

        s_search_node = *(ps_search_candts->ps_search_node);

        /* initialize minimum cost for this candidate. As we search around */
        /* this candidate, this is used to check early exit, when in any   */
        /* given iteration, the center pt of the grid is lowest value      */
        s_result_prms.i4_min_cost = MAX_32BIT_VAL;

        /* If we need to do refinements, then we need to evaluate */
        /* neighbouring pts. Before doing so, we have to do       */
        /* basic range checks against max allowed mvs             */
        i4_num_refine = ps_search_candts->u1_num_steps_refine;

        CLIP_MV_WITHIN_RANGE(
            s_search_node.s_mv.i2_mvx, s_search_node.s_mv.i2_mvy, ps_range_prms, 0, 0, 0);

        /* The first time, we search all 8 pts around init candt plus the init candt */
        i4_grid_mask = 0x1ff;
        s_err_prms.pu1_inp = ps_wt_inp_prms->apu1_wt_inp[s_search_node.i1_ref_idx] + i4_inp_off;

        for(i4_iter = 0; i4_iter < max_num_iters; i4_iter++)
        {
            i4_grid_mask &= hme_clamp_grid_by_mvrange(&s_search_node, i4_step, ps_range_prms);

            s_err_prms.i4_grid_mask = i4_grid_mask;
            s_err_prms.pu1_ref = ppu1_ref[s_search_node.i1_ref_idx] + i4_ref_offset;
            s_err_prms.pu1_ref +=
                (s_search_node.s_mv.i2_mvx +
                 (s_search_node.s_mv.i2_mvy * s_err_prms.i4_ref_stride));

            s_result_prms.i4_step = i4_step;
            s_err_prms.i4_step = i4_step;
            s_result_prms.i4_grid_mask = i4_grid_mask;

            /* For Top,TopLeft and Left cand., get only center point SAD    */
            /* and do early exit                                            */
            if(0 == i4_num_refine)
            {
                s_err_prms.i4_grid_mask = 0x1;
                s_result_prms.i4_grid_mask = 0x1;

                /* sad pt fun. populates sad to 0th location, whereas update */
                /* fun. takes it based on part. id                           */
                s_err_prms.pi4_sad_grid =
                    s_result_prms.pi4_sad_grid + (1 * s_result_prms.pi4_valid_part_ids[0]);

                ps_me_optimised_function_list->pf_evalsad_pt_npu_mxn_8bit(&s_err_prms);

                s_err_prms.pi4_sad_grid = s_result_prms.pi4_sad_grid;

                if(ME_XTREME_SPEED_25 == e_me_quality_preset)
                    hme_update_results_grid_pu_bestn_xtreme_speed(&s_result_prms);
                else
                    hme_update_results_grid_pu_bestn(&s_result_prms);

                i4_min_id = (S32)PT_C; /* Center Point         */
                i4_step = 0; /* No further refinment */
                s_result_prms.i4_step = i4_step;
                s_err_prms.i4_step = i4_step;
            }
            else
            {
                if(ME_XTREME_SPEED_25 == e_me_quality_preset)
                {
                    err_prms_t *ps_err_prms = &s_err_prms;
                    ASSERT(ps_err_prms->i4_grid_mask != 1);
                    ASSERT((ps_err_prms->i4_part_mask == 4) || (ps_err_prms->i4_part_mask == 16));

                    /*****************************************************************/
                    /* In this case, there are no partial updates. The blk can be    */
                    /* of any type and need not be a CU. The only thing that matters */
                    /* here is the width of the blk, 4/8/(>=16)                      */
                    /*****************************************************************/
                    ps_me_optimised_function_list->pf_evalsad_grid_npu_MxN(&s_err_prms);

                    hme_update_results_grid_pu_bestn_xtreme_speed(&s_result_prms);
                }
                else
                {
                    /* Obtain SAD for all 9 pts in grid*/
                    hme_compute_grid_results(&s_err_prms, &s_result_prms, e_blk_size);
                }

                /* Early exit in case of centre being local minima */
                i4_min_id = s_result_prms.i4_min_id;
            }

            i4_grid_mask = gai4_opt_grid_mask[i4_min_id];

            s_search_node.s_mv.i2_mvx += (i4_step * gai1_grid_id_to_x[i4_min_id]);
            s_search_node.s_mv.i2_mvy += (i4_step * gai1_grid_id_to_y[i4_min_id]);
            if(i4_min_id == (S32)PT_C)
                break;
        }

        /* Next keep reducing stepsize by factor of 2 */
        i4_step >>= 1;
        while(i4_step)
        {
            i4_grid_mask = 0x1fe &
                           hme_clamp_grid_by_mvrange(&s_search_node, i4_step, ps_range_prms);
            //i4_grid_mask &= 0x1fe;

            s_err_prms.i4_grid_mask = i4_grid_mask;
            s_result_prms.i4_grid_mask = i4_grid_mask;
            s_err_prms.i4_step = i4_step;
            s_result_prms.i4_step = i4_step;
            s_err_prms.pu1_ref = ppu1_ref[s_search_node.i1_ref_idx] + i4_ref_offset;
            s_err_prms.pu1_ref +=
                (s_search_node.s_mv.i2_mvx +
                 (s_search_node.s_mv.i2_mvy * s_err_prms.i4_ref_stride));
            if(ME_XTREME_SPEED_25 == e_me_quality_preset)
            {
                err_prms_t *ps_err_prms = &s_err_prms;
                ASSERT(ps_err_prms->i4_grid_mask != 1);
                ASSERT((ps_err_prms->i4_part_mask == 4) || (ps_err_prms->i4_part_mask == 16));

                /*****************************************************************/
                /* In this case, there are no partial updates. The blk can be    */
                /* of any type and need not be a CU. The only thing that matters */
                /* here is the width of the blk, 4/8/(>=16)                      */
                /*****************************************************************/
                ps_me_optimised_function_list->pf_evalsad_grid_npu_MxN(&s_err_prms);

                hme_update_results_grid_pu_bestn_xtreme_speed(&s_result_prms);
            }
            else
            {
                hme_compute_grid_results(&s_err_prms, &s_result_prms, e_blk_size);
            }

            i4_min_id = s_result_prms.i4_min_id;

            s_search_node.s_mv.i2_mvx += (i4_step * gai1_grid_id_to_x[i4_min_id]);
            s_search_node.s_mv.i2_mvy += (i4_step * gai1_grid_id_to_y[i4_min_id]);

            i4_step >>= 1;
        }

        ps_search_candts++;
    }
}

/**
********************************************************************************
*  @fn     hme_pred_search_square_step1(hme_search_prms_t *ps_search_prms,
*                               layer_ctxt_t *ps_layer_ctxt)
*
*  @brief  Implements predictive search with square grid refinement. In this
*           case, the square grid is of step 1 always. since this is considered
*           to be more of a refinement search
*
*  @param[in,out]  ps_search_prms: All the params to this function
*
*  @param[in] ps_layer_ctxt: All info about this layer
*
*  @return None
********************************************************************************
*/
/**
********************************************************************************
*  @fn     hme_pred_search(hme_search_prms_t *ps_search_prms,
*                               layer_ctxt_t *ps_layer_ctxt)
*
*  @brief  Implements predictive search after removing duplicate candidates
*          from initial list. Each square grid (of step 1) is expanded
*          to nine search pts before the dedeuplication process. one point
*          cost is then evaluated for each unique node after the deduplication
*          process
*
*  @param[in,out]  ps_search_prms: All the params to this function
*
*  @param[in] ps_layer_ctxt: All info about this layer
*
*  @return None
********************************************************************************
*/
void hme_pred_search(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S08 i1_grid_flag,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list

)
{
    /* Stores the SAD for all parts at each pt in the grid */
    S32 ai4_sad_grid[9 * TOT_NUM_PARTS];

    /* Atributes of input candidates */
    search_node_t *ps_search_node;

    search_results_t *ps_search_results;
    S32 i4_num_nodes, i4_candt;

    /* Input and reference attributes */
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref;

    /* These control number of parts and number of pts in grid to search */
    S32 i4_part_mask, i4_grid_mask;

    S32 shift_for_cu_size;

    /* Blk width, blk height and blk size are derived from input params */
    BLK_SIZE_T e_blk_size;
    CU_SIZE_T e_cu_size;
    S32 i4_blk_wd, i4_blk_ht;

    /*************************************************************************/
    /* These functions pointers for calculating Err and the result update    */
    /* Each carries its own parameters structure, which is generated on the  */
    /* fly in this function                                                  */
    /*************************************************************************/
    PF_RESULT_FXN_T pf_hme_result_fxn;
    PF_SAD_FXN_T pf_sad_fxn;
    PF_CALC_SAD_AND_RESULT pf_calc_sad_and_result;
    err_prms_t s_err_prms;
    result_upd_prms_t s_result_prms;
    S32 i4_num_results;
    S32 i4_inp_off;
    fullpel_refine_ctxt_t *ps_fullpel_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;

    i4_inp_stride = ps_search_prms->i4_inp_stride;

    /* Move to the location of the search blk in inp buffer */
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * i4_inp_stride;

    /*************************************************************************/
    /* Depending on flag i4_use_rec, we use either input of previously       */
    /* encoded pictures or we use recon of previously encoded pictures.      */
    /*************************************************************************/
    if(ps_search_prms->i4_use_rec == 1)
    {
        i4_ref_stride = ps_layer_ctxt->i4_rec_stride;
        ppu1_ref = ps_layer_ctxt->ppu1_list_rec_fxfy;
    }
    else
    {
        i4_ref_stride = ps_layer_ctxt->i4_rec_stride;
        ppu1_ref = ps_layer_ctxt->ppu1_list_inp;
    }
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    /* Obtain the blk size of the search blk. Assumed here that the search   */
    /* is done on a CU size, rather than any arbitrary blk size.             */
    ps_search_results = ps_search_prms->ps_search_results;
    e_blk_size = ps_search_prms->e_blk_size;
    i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
    i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];
    e_cu_size = ps_search_results->e_cu_size;

    /* Assuming cu size of 8x8 as enum 0, the other will be 1, 2, 3 */
    /* This will also set the shift w.r.t. the base cu size of 8x8 */
    shift_for_cu_size = e_cu_size;

    ps_search_node = ps_search_prms->ps_search_nodes;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    i4_part_mask = ps_search_prms->i4_part_mask;

    /* Update the parameters used to pass to SAD */
    /* input ptr, strides, SAD Grid, part mask, blk width and ht */
    /* The above are fixed ptrs, only pu1_ref and grid mask are  */
    /* varying params which are updated just before calling fxn  */
    s_err_prms.i4_inp_stride = i4_inp_stride;
    s_err_prms.i4_ref_stride = i4_ref_stride;
    s_err_prms.i4_part_mask = i4_part_mask;
    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0];
    s_err_prms.i4_blk_wd = i4_blk_wd;
    s_err_prms.i4_blk_ht = i4_blk_ht;
    s_err_prms.i4_step = 1;
    s_err_prms.i4_num_partitions = ps_fullpel_refine_ctxt->i4_num_valid_parts;

    s_result_prms.pf_mv_cost_compute = ps_search_prms->pf_mv_cost_compute;
    s_result_prms.ps_search_results = ps_search_results;
    s_result_prms.i1_ref_idx = (S08)ps_search_prms->i1_ref_idx;
    s_result_prms.pi4_sad_grid = ai4_sad_grid;
    s_result_prms.i4_part_mask = i4_part_mask;
    s_result_prms.i4_step = 1;
    pf_calc_sad_and_result = hme_get_calc_sad_and_result_fxn(
        i1_grid_flag,
        ps_search_prms->u1_is_cu_noisy,
        i4_part_mask,
        ps_fullpel_refine_ctxt->i4_num_valid_parts,
        ps_search_results->u1_num_results_per_part);

    pf_calc_sad_and_result(
        ps_search_prms, ps_wt_inp_prms, &s_err_prms, &s_result_prms, ppu1_ref, i4_ref_stride);
}

static __inline FT_CALC_SAD_AND_RESULT *hme_get_calc_sad_and_result_explicit_fxn(
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list,
    S32 i4_part_mask,
    S32 i4_num_partitions,
    S08 i1_grid_enable,
    U08 u1_num_results_per_part)
{
    FT_CALC_SAD_AND_RESULT *pf_func = NULL;

    if(2 == u1_num_results_per_part)
    {
        if(i4_part_mask == 1)
        {
            ASSERT(i4_num_partitions == 1);

            if(i1_grid_enable == 0)
            {
                pf_func =
                    ps_me_optimised_function_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8;
            }
            else
            {
                pf_func = ps_me_optimised_function_list
                              ->pf_calc_pt_sad_and_2_best_results_explicit_8x8_for_grid;
            }
        }
        else
        {
            ASSERT(i4_num_partitions == 5);

            pf_func =
                ps_me_optimised_function_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8_4x4;
        }
    }
    else if(1 == u1_num_results_per_part)
    {
        if(i4_part_mask == 1)
        {
            ASSERT(i4_num_partitions == 1);

            if(i1_grid_enable == 0)
            {
                pf_func =
                    ps_me_optimised_function_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8;
            }
            else
            {
                pf_func = ps_me_optimised_function_list
                              ->pf_calc_pt_sad_and_1_best_result_explicit_8x8_for_grid;
            }
        }
        else
        {
            ASSERT(i4_num_partitions == 5);

            pf_func =
                ps_me_optimised_function_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8_4x4;
        }
    }

    return pf_func;
}

/**
********************************************************************************
*  @fn     void hme_pred_search_no_encode(hme_search_prms_t *ps_search_prms,
*                                         layer_ctxt_t *ps_layer_ctxt,
*                                         wgt_pred_ctxt_t *ps_wt_inp_prms,
*                                         S32 *pi4_valid_part_ids,
*                                         S32 disable_refine,
*                                         ME_QUALITY_PRESETS_T e_me_quality_preset)
*
*  @brief  Implements predictive search after removing duplicate candidates
*          from initial list. Each square grid (of step 1) is expanded
*          to nine search pts before the dedeuplication process. one point
*          cost is then evaluated for each unique node after the deduplication
*          process
*
*  @param[in,out]  ps_search_prms: All the params to this function
*
*  @param[in] ps_layer_ctxt: All info about this layer
*
*  @return None
********************************************************************************
*/
void hme_pred_search_no_encode(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 *pi4_valid_part_ids,
    S32 disable_refine,
    ME_QUALITY_PRESETS_T e_me_quality_preset,
    S08 i1_grid_enable,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    /* Stores the SAD for all parts at each pt in the grid */
    S32 ai4_sad_grid[9 * TOT_NUM_PARTS];

    /* Atributes of input candidates */
    search_node_t *ps_search_node;
    search_results_t *ps_search_results;
    S32 i4_num_nodes;

    /* Input and reference attributes */
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref;

    /* These control number of parts and number of pts in grid to search */
    S32 i4_part_mask;  // i4_grid_mask;

    S32 shift_for_cu_size;
    /* Blk width, blk height and blk size are derived from input params */
    BLK_SIZE_T e_blk_size;
    CU_SIZE_T e_cu_size;
    S32 i4_blk_wd, i4_blk_ht;

    /*************************************************************************/
    /* These functions pointers for calculating Err and the result update    */
    /* Each carries its own parameters structure, which is generated on the  */
    /* fly in this function                                                  */
    /*************************************************************************/
    PF_CALC_SAD_AND_RESULT pf_calc_sad_and_result;
    err_prms_t s_err_prms;
    result_upd_prms_t s_result_prms;
    S32 i4_num_results;
    S32 i4_search_idx = ps_search_prms->i1_ref_idx;
    S32 i4_inp_off;
    S32 i4_num_partitions;

    i4_inp_stride = ps_search_prms->i4_inp_stride;

    /* Move to the location of the search blk in inp buffer */
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * i4_inp_stride;

    /*************************************************************************/
    /* Depending on flag i4_use_rec, we use either input of previously       */
    /* encoded pictures or we use recon of previously encoded pictures.      */
    /*************************************************************************/
    if(ps_search_prms->i4_use_rec == 1)
    {
        i4_ref_stride = ps_layer_ctxt->i4_rec_stride;
        ppu1_ref = ps_layer_ctxt->ppu1_list_rec_fxfy;
    }
    else
    {
        i4_ref_stride = ps_layer_ctxt->i4_inp_stride;
        ppu1_ref = ps_layer_ctxt->ppu1_list_inp;
    }
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    /* Obtain the blk size of the search blk. Assumed here that the search   */
    /* is done on a CU size, rather than any arbitrary blk size.             */
    ps_search_results = ps_search_prms->ps_search_results;
    e_blk_size = ps_search_prms->e_blk_size;
    i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
    i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];
    e_cu_size = ps_search_results->e_cu_size;

    /* Assuming cu size of 8x8 as enum 0, the other will be 1, 2, 3 */
    /* This will also set the shift w.r.t. the base cu size of 8x8 */
    shift_for_cu_size = e_cu_size;

    ps_search_node = ps_search_prms->ps_search_nodes;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    i4_part_mask = ps_search_prms->i4_part_mask;

    /*************************************************************************/
    /* This array stores the ids of the partitions whose                     */
    /* SADs are updated. Since the partitions whose SADs are updated may not */
    /* be in contiguous order, we supply another level of indirection.       */
    /*************************************************************************/
    i4_num_partitions = hme_create_valid_part_ids(i4_part_mask, pi4_valid_part_ids);

    /* Update the parameters used to pass to SAD */
    /* input ptr, strides, SAD Grid, part mask, blk width and ht */
    /* The above are fixed ptrs, only pu1_ref and grid mask are  */
    /* varying params which are updated just before calling fxn  */
    s_err_prms.i4_inp_stride = i4_inp_stride;
    s_err_prms.i4_ref_stride = i4_ref_stride;
    s_err_prms.i4_part_mask = i4_part_mask;
    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0];
    s_err_prms.i4_blk_wd = i4_blk_wd;
    s_err_prms.i4_blk_ht = i4_blk_ht;
    s_err_prms.i4_step = 1;
    s_err_prms.pi4_valid_part_ids = pi4_valid_part_ids;
    s_err_prms.i4_num_partitions = i4_num_partitions;

    s_result_prms.pf_mv_cost_compute = ps_search_prms->pf_mv_cost_compute;
    s_result_prms.ps_search_results = ps_search_results;
    s_result_prms.pi4_valid_part_ids = pi4_valid_part_ids;
    s_result_prms.i1_ref_idx = (S08)ps_search_prms->i1_ref_idx;
    s_result_prms.pi4_sad_grid = ai4_sad_grid;
    s_result_prms.i4_part_mask = i4_part_mask;
    s_result_prms.i4_step = 1;

    pf_calc_sad_and_result = hme_get_calc_sad_and_result_explicit_fxn(
        ps_me_optimised_function_list,
        i4_part_mask,
        i4_num_partitions,
        i1_grid_enable,
        ps_search_results->u1_num_results_per_part);

    pf_calc_sad_and_result(
        ps_search_prms, ps_wt_inp_prms, &s_err_prms, &s_result_prms, ppu1_ref, i4_ref_stride);
}
