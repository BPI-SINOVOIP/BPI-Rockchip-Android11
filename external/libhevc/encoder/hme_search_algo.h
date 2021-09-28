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
* \file hme_search_algo.h
*
* \brief
*    contains prototypes for search algorithms called by coarse/refinement
*    layers.
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_SEARCH_ALGO_H_
#define _HME_SEARCH_ALGO_H_

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/
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
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

/**
********************************************************************************
*  @fn     hme_do_fullsearch(hme_search_prms_t *ps_search_prms,
*                               layer_ctxt_t *ps_layer_ctxt,
*                               hme_mv_t *ps_best_mv,
*                               pred_ctxt_t *ps_pred_ctxt,
*                               PF_MV_COST_FXN pf_mv_cost_compute)
*
*  @brief  Does a full search on entire srch window with a given step size
*
*  @param[in] ps_search_prms : Search prms structure containing info like
*               blk dimensions, search range etc
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
void hme_do_fullsearch(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    hme_mv_t *ps_best_mv,
    pred_ctxt_t *ps_pred_ctxt,
    PF_MV_COST_FXN pf_mv_cost_compute,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    ME_QUALITY_PRESETS_T e_me_quality_preset,
    range_prms_t *ps_range_prms);

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
*  @param[out] pi4_valid_part_ids: Array to hold valid partitions
*
*  @param[in] i4_disable_refine flag to disable refinement
*
*  @return None
********************************************************************************
*/
void hme_pred_search(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S08 i1_grid_flag,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

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
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

#endif /* #ifndef _HME_SEARCH_ALGO_H_*/
