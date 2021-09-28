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
* \file ihevce_me_instr_set_router.h
*
* \brief
*    This file contains declarations related to me utilities used in encoder
*
* \date
*    15/07/2013
*
* \author
*    Ittiam
*
* List of Functions
*
*
******************************************************************************
*/

#ifndef __IHEVCE_ME_INSTR_SET_ROUTER_H_
#define __IHEVCE_ME_INSTR_SET_ROUTER_H_

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef void FT_SAD_EVALUATOR(err_prms_t *);

typedef void FT_PART_SADS_EVALUATOR(
    grid_ctxt_t *, UWORD8 *, WORD32, WORD32 **, cand_t *, WORD32 *, CU_SIZE_T);

typedef void
    FT_PART_SADS_EVALUATOR_16X16CU(grid_ctxt_t *, UWORD8 *, WORD32, UWORD16 **, cand_t *, WORD32 *);

typedef void FT_CALC_SAD_AND_RESULT(
    hme_search_prms_t *, wgt_pred_ctxt_t *, err_prms_t *, result_upd_prms_t *, U08 **, S32);

typedef void FT_CALC_SAD_AND_RESULT_SUBPEL(err_prms_t *, result_upd_prms_t *);

typedef void FT_QPEL_INTERP_AVG(interp_prms_t *, S32, S32, S32);

typedef void FT_QPEL_INTERP_AVG_1PT(interp_prms_t *, S32, S32, S32, U08 **, S32 *);

typedef void FT_QPEL_INTERP_AVG_2PT(interp_prms_t *, S32, S32, U08 **, S32 *);

typedef void FT_GET_WT_INP(layer_ctxt_t *, wgt_pred_ctxt_t *, S32, S32, S32, S32, S32, U08);

typedef void
    FT_STORE_4X4_SADS(hme_search_prms_t *, layer_ctxt_t *, range_prms_t *, wgt_pred_ctxt_t *, S16 *);

typedef void FT_COMBINE_4X4_SADS_AND_COMPUTE_COST(
    S08,
    range_prms_t *,
    range_prms_t *,
    hme_mv_t *,
    hme_mv_t *,
    pred_ctxt_t *,
    PF_MV_COST_FXN,
    S16 *,
    S16 *,
    S16 *);

typedef void FT_MV_CLIPPER(hme_search_prms_t *, S32, S08, U08, U08, U08);

typedef void FT_COMPUTE_VARIANCE(U08 *, S32, S32 *, U32 *, S32, U08);

typedef void FT_COMPUTE_DISTORTION(
    U08 *, S32, S32 *, ULWORD64 *, ULWORD64 *, S32 *, S32, S32, S32, S32, S32, U08);

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

// clang-format off
typedef struct
{
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_mxn_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_grid_npu_MxN;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_8x4_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_16x4_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_16x12_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_24x32_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_12x16_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_width_multiple_4_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_width_multiple_8_8bit;
    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_width_multiple_16_8bit;
    FT_PART_SADS_EVALUATOR_16X16CU *pf_compute_4x4_sads_for_16x16_blk;
    FT_PART_SADS_EVALUATOR *pf_evalsad_grid_pu_MxM;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_1_best_result_generic;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_1_best_result_generic;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_1_best_result_num_part_eq_1;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_1_best_result_num_square_parts;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_9;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_17;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_1_best_result_num_part_eq_1;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_1_best_result_num_part_1_for_grid;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_1_best_result_num_square_parts;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_1_best_result_num_part_lt_9;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_1_best_result_num_part_lt_17;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_1_best_result_explicit_generic;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_1_best_result_explicit_8x8;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_1_best_result_explicit_8x8_for_grid;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_1_best_result_explicit_8x8_4x4;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_2_best_results_explicit_generic;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_2_best_results_explicit_8x8;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_2_best_results_explicit_8x8_for_grid;
    FT_CALC_SAD_AND_RESULT *pf_calc_pt_sad_and_2_best_results_explicit_8x8_4x4;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_1_best_result_subpel_generic;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_1_best_result_subpel_num_part_eq_1;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_1_best_result_subpel_square_parts;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_1_best_result_subpel_num_part_lt_9;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_1_best_result_subpel_num_part_lt_17;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_2_best_results_generic;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_2_best_results_generic;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_2_best_results_num_part_eq_1;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_2_best_results_num_square_parts;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_2_best_results_num_part_lt_9;
    FT_CALC_SAD_AND_RESULT *pf_calc_stim_injected_sad_and_2_best_results_num_part_lt_17;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_2_best_results_num_part_eq_1;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_2_best_results_num_part_1_for_grid;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_2_best_results_num_square_parts;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_2_best_results_num_part_lt_9;
    FT_CALC_SAD_AND_RESULT *pf_calc_sad_and_2_best_results_num_part_lt_17;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_2_best_results_subpel_generic;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_2_best_results_subpel_num_part_eq_1;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_2_best_results_subpel_square_parts;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_2_best_results_subpel_num_part_lt_9;
    FT_CALC_SAD_AND_RESULT_SUBPEL *pf_calc_sad_and_2_best_results_subpel_num_part_lt_17;
    FT_QPEL_INTERP_AVG *pf_qpel_interp_avg_generic;
    FT_QPEL_INTERP_AVG_1PT *pf_qpel_interp_avg_1pt;
    FT_QPEL_INTERP_AVG_2PT *pf_qpel_interp_avg_2pt_vert_with_reuse;
    FT_QPEL_INTERP_AVG_2PT *pf_qpel_interp_avg_2pt_horz_with_reuse;
    FT_GET_WT_INP *pf_get_wt_inp_generic;
    FT_GET_WT_INP *pf_get_wt_inp_8x8;
    FT_GET_WT_INP *pf_get_wt_inp_ctb;
    FT_STORE_4X4_SADS *pf_store_4x4_sads_high_speed;
    FT_STORE_4X4_SADS *pf_store_4x4_sads_high_quality;
    FT_COMBINE_4X4_SADS_AND_COMPUTE_COST *pf_combine_4x4_sads_and_compute_cost_high_speed;
    FT_COMBINE_4X4_SADS_AND_COMPUTE_COST *pf_combine_4x4_sads_and_compute_cost_high_quality;
    FT_MV_CLIPPER *pf_mv_clipper;
    FT_COMPUTE_VARIANCE *pf_compute_variance_for_all_parts;
    FT_COMPUTE_DISTORTION *pf_compute_stim_injected_distortion_for_all_parts;
} ihevce_me_optimised_function_list_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_me_instr_set_router(
    ihevce_me_optimised_function_list_t *ps_func_list, IV_ARCH_T e_arch);

PF_SAD_FXN_T hme_get_sad_fxn(
    BLK_SIZE_T e_blk_size, S32 i4_grid_mask, S32 i4_part_mask);

void ihevce_sifter_sad_fxn_assigner(
    FT_SAD_EVALUATOR **ppf_evalsad_pt_npu_mxn, IV_ARCH_T e_arch);

void hme_evalsad_grid_pu_MxM(err_prms_t *ps_prms);

FT_CALC_SAD_AND_RESULT *hme_get_calc_sad_and_result_fxn(S08 i1_grid_flag,
    U08 u1_is_cu_noisy, S32 i4_part_mask, S32 num_parts, S32 num_results);

/* Function List - C */
FT_SAD_EVALUATOR hme_evalsad_pt_npu_MxN_8bit;
FT_SAD_EVALUATOR hme_evalsad_grid_npu_MxN;
FT_PART_SADS_EVALUATOR compute_part_sads_for_MxM_blk;
FT_PART_SADS_EVALUATOR_16X16CU compute_4x4_sads_for_16x16_blk;
FT_CALC_SAD_AND_RESULT hme_calc_sad_and_1_best_result;
FT_CALC_SAD_AND_RESULT hme_calc_stim_injected_sad_and_1_best_result;
FT_CALC_SAD_AND_RESULT hme_calc_pt_sad_and_result_explicit;
FT_CALC_SAD_AND_RESULT_SUBPEL hme_calc_sad_and_1_best_result_subpel;
FT_CALC_SAD_AND_RESULT hme_calc_sad_and_2_best_results;
FT_CALC_SAD_AND_RESULT hme_calc_stim_injected_sad_and_2_best_results;
FT_CALC_SAD_AND_RESULT_SUBPEL hme_calc_sad_and_2_best_results_subpel;
FT_QPEL_INTERP_AVG hme_qpel_interp_avg;
FT_QPEL_INTERP_AVG_1PT hme_qpel_interp_avg_1pt;
FT_QPEL_INTERP_AVG_2PT hme_qpel_interp_avg_2pt_vert_with_reuse;
FT_QPEL_INTERP_AVG_2PT hme_qpel_interp_avg_2pt_horz_with_reuse;
FT_GET_WT_INP hme_get_wt_inp;
FT_STORE_4X4_SADS hme_store_4x4_sads_high_speed;
FT_STORE_4X4_SADS hme_store_4x4_sads_high_quality;
FT_COMBINE_4X4_SADS_AND_COMPUTE_COST hme_combine_4x4_sads_and_compute_cost_high_speed;
FT_COMBINE_4X4_SADS_AND_COMPUTE_COST hme_combine_4x4_sads_and_compute_cost_high_quality;
FT_MV_CLIPPER hme_mv_clipper;
FT_COMPUTE_VARIANCE hme_compute_variance_for_all_parts;
FT_COMPUTE_DISTORTION hme_compute_stim_injected_distortion_for_all_parts;


/* Function List - Neon */
#ifdef ENABLE_NEON
FT_SAD_EVALUATOR hme_evalsad_pt_npu_MxN_8bit_neon;
FT_SAD_EVALUATOR hme_evalsad_grid_npu_MxN_neon;
FT_PART_SADS_EVALUATOR compute_part_sads_for_MxM_blk_neon;
FT_PART_SADS_EVALUATOR_16X16CU compute_4x4_sads_for_16x16_blk_neon;
FT_CALC_SAD_AND_RESULT hme_calc_sad_and_1_best_result_neon;
FT_CALC_SAD_AND_RESULT_SUBPEL hme_calc_sad_and_1_best_result_subpel_neon;
FT_QPEL_INTERP_AVG hme_qpel_interp_avg_neon;
FT_QPEL_INTERP_AVG_1PT hme_qpel_interp_avg_1pt_neon;
FT_QPEL_INTERP_AVG_2PT hme_qpel_interp_avg_2pt_vert_with_reuse_neon;
FT_QPEL_INTERP_AVG_2PT hme_qpel_interp_avg_2pt_horz_with_reuse_neon;
FT_GET_WT_INP hme_get_wt_inp_8x8_neon;
FT_GET_WT_INP hme_get_wt_inp_ctb_neon;
FT_STORE_4X4_SADS hme_store_4x4_sads_high_speed_neon;
FT_STORE_4X4_SADS hme_store_4x4_sads_high_quality_neon;
FT_COMBINE_4X4_SADS_AND_COMPUTE_COST hme_combine_4x4_sads_and_compute_cost_high_speed_neon;
FT_COMBINE_4X4_SADS_AND_COMPUTE_COST hme_combine_4x4_sads_and_compute_cost_high_quality_neon;
#endif

// clang-format on

#endif
