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
* \file ihevce_me_utils_instr_set_router.c
*
* \brief
*    This file contains function pointer initialization of me utility
*    functions
*
* \date
*    15/07/2013
*
* \author
*    Ittiam
*
* List of Functions
*  ihevce_me_utils_instr_set_router()
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_debug.h"
#include "ihevc_deblk.h"
#include "ihevc_defs.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_macros.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_resi_trans.h"
#include "ihevc_sao.h"
#include "ihevc_structs.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_platform_macros.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevce_api.h"
#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_function_selector.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_enc_structs.h"
#include "ihevce_had_satd.h"
#include "ihevce_cmn_utils_instr_set_router.h"

#include "hme_datatype.h"
#include "hme_common_defs.h"
#include "hme_common_utils.h"
#include "hme_interface.h"
#include "hme_defs.h"
#include "hme_err_compute.h"
#include "hme_globals.h"

#include "ihevce_me_instr_set_router.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
static FT_SAD_EVALUATOR *gapf_sad_pt_npu[NUM_BLK_SIZES];
static FT_PART_SADS_EVALUATOR_16X16CU *gpf_part_sads_evaluator_16x16CU;
static FT_PART_SADS_EVALUATOR *gpf_part_sads_evaluator_MxM;
static FT_SAD_EVALUATOR *gpf_sad_grid_mxn;
/* 9 => Number of function types */
/* 2 => Number of results to store */
static FT_CALC_SAD_AND_RESULT *gapf_calc_sad_and_result_fxn[9][2];

static U08 gau1_calc_sad_and_result[2][2][4][TOT_NUM_PARTS] = {
    //grid flag = 0
    { //noise = 0
      { //NxN or NxN & SMP
        { 1, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4 },
        //SMP only
        { 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
        //AMP
        { 1, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
        //2Nx2N only, i.e. num_parts = 1
        { 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 } },
      //noise = 1
      { { 5, 7, 7, 7, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        { 5, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        { 5, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        { 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 } } },

    //grid flag = 1
    { //noise = 0
      { { 0, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4 },
        { 0, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
        { 0, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
        { 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 } },
      //noise = 1
      { { 0, 7, 7, 7, 6, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        { 0, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        { 0, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8 },
        { 0, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 } } }
};

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
/*!
******************************************************************************
* \if Function name : ihevce_me_instr_set_router \endif
*
* \brief
*    Function pointer initialization of me utils struct
*
*****************************************************************************
*/
void ihevce_me_instr_set_router(ihevce_me_optimised_function_list_t *ps_func_list, IV_ARCH_T e_arch)
{
    // clang-format off
#ifdef DISABLE_AVX2_INTR
    e_arch = (e_arch == ARCH_X86_AVX2) ? ARCH_X86_AVX : e_arch;
#endif

    switch(e_arch)
    {
#ifdef ENABLE_NEON
    case ARCH_ARM_A9Q:
    case ARCH_ARM_V8_NEON:
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8_4x4 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8_for_grid = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_generic = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8_4x4 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8_for_grid = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_generic = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_sad_and_1_best_result_generic = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_generic = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_eq_1 = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_square_parts = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_9 = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_17 = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_compute_variance_for_all_parts = hme_compute_variance_for_all_parts;
        ps_func_list->pf_compute_stim_injected_distortion_for_all_parts = hme_compute_stim_injected_distortion_for_all_parts;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_1_for_grid = hme_calc_sad_and_1_best_result_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_eq_1 = hme_calc_sad_and_1_best_result_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_lt_17 = hme_calc_sad_and_1_best_result_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_lt_9 = hme_calc_sad_and_1_best_result_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_num_square_parts = hme_calc_sad_and_1_best_result_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_generic = hme_calc_sad_and_1_best_result_subpel;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_num_part_eq_1 = hme_calc_sad_and_1_best_result_subpel_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_num_part_lt_17 = hme_calc_sad_and_1_best_result_subpel_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_num_part_lt_9 = hme_calc_sad_and_1_best_result_subpel_neon;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_square_parts = hme_calc_sad_and_1_best_result_subpel_neon;
        ps_func_list->pf_combine_4x4_sads_and_compute_cost_high_quality = hme_combine_4x4_sads_and_compute_cost_high_quality_neon;
        ps_func_list->pf_combine_4x4_sads_and_compute_cost_high_speed = hme_combine_4x4_sads_and_compute_cost_high_speed_neon;
        ps_func_list->pf_compute_4x4_sads_for_16x16_blk = compute_4x4_sads_for_16x16_blk_neon;
        ps_func_list->pf_evalsad_grid_npu_MxN = hme_evalsad_grid_npu_MxN_neon;
        ps_func_list->pf_evalsad_grid_pu_MxM = compute_part_sads_for_MxM_blk_neon;
        ps_func_list->pf_evalsad_pt_npu_12x16_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_16x12_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_16x4_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_24x32_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_8x4_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_mxn_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_width_multiple_4_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_evalsad_pt_npu_width_multiple_8_8bit = hme_evalsad_pt_npu_MxN_8bit_neon;
        ps_func_list->pf_get_wt_inp_8x8 = hme_get_wt_inp_8x8_neon;
        ps_func_list->pf_get_wt_inp_ctb = hme_get_wt_inp_ctb_neon;
        ps_func_list->pf_get_wt_inp_generic = hme_get_wt_inp;
        ps_func_list->pf_mv_clipper = hme_mv_clipper;
        ps_func_list->pf_qpel_interp_avg_1pt = hme_qpel_interp_avg_1pt_neon;
        ps_func_list->pf_qpel_interp_avg_2pt_horz_with_reuse = hme_qpel_interp_avg_2pt_horz_with_reuse_neon;
        ps_func_list->pf_qpel_interp_avg_2pt_vert_with_reuse = hme_qpel_interp_avg_2pt_vert_with_reuse_neon;
        ps_func_list->pf_qpel_interp_avg_generic = hme_qpel_interp_avg_neon;
        ps_func_list->pf_store_4x4_sads_high_quality = hme_store_4x4_sads_high_quality_neon;
        ps_func_list->pf_store_4x4_sads_high_speed = hme_store_4x4_sads_high_speed_neon;
        break;
#endif
    default:
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8_4x4 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_8x8_for_grid = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_1_best_result_explicit_generic = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8_4x4 = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_8x8_for_grid = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_pt_sad_and_2_best_results_explicit_generic = hme_calc_pt_sad_and_result_explicit;
        ps_func_list->pf_calc_sad_and_1_best_result_generic = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_generic = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_eq_1 = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_square_parts = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_9 = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_17 = hme_calc_stim_injected_sad_and_1_best_result;
        ps_func_list->pf_compute_variance_for_all_parts = hme_compute_variance_for_all_parts;
        ps_func_list->pf_compute_stim_injected_distortion_for_all_parts = hme_compute_stim_injected_distortion_for_all_parts;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_1_for_grid = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_eq_1 = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_lt_17 = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_sad_and_1_best_result_num_part_lt_9 = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_sad_and_1_best_result_num_square_parts = hme_calc_sad_and_1_best_result;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_generic = hme_calc_sad_and_1_best_result_subpel;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_num_part_eq_1 = hme_calc_sad_and_1_best_result_subpel;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_num_part_lt_17 = hme_calc_sad_and_1_best_result_subpel;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_num_part_lt_9 = hme_calc_sad_and_1_best_result_subpel;
        ps_func_list->pf_calc_sad_and_1_best_result_subpel_square_parts = hme_calc_sad_and_1_best_result_subpel;
        ps_func_list->pf_combine_4x4_sads_and_compute_cost_high_quality = hme_combine_4x4_sads_and_compute_cost_high_quality;
        ps_func_list->pf_combine_4x4_sads_and_compute_cost_high_speed = hme_combine_4x4_sads_and_compute_cost_high_speed;
        ps_func_list->pf_compute_4x4_sads_for_16x16_blk = compute_4x4_sads_for_16x16_blk;
        ps_func_list->pf_evalsad_grid_npu_MxN = hme_evalsad_grid_npu_MxN;
        ps_func_list->pf_evalsad_grid_pu_MxM = compute_part_sads_for_MxM_blk;
        ps_func_list->pf_evalsad_pt_npu_12x16_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_16x12_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_16x4_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_24x32_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_8x4_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_mxn_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_width_multiple_4_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_evalsad_pt_npu_width_multiple_8_8bit = hme_evalsad_pt_npu_MxN_8bit;
        ps_func_list->pf_get_wt_inp_8x8 = hme_get_wt_inp;
        ps_func_list->pf_get_wt_inp_ctb = hme_get_wt_inp;
        ps_func_list->pf_get_wt_inp_generic = hme_get_wt_inp;
        ps_func_list->pf_mv_clipper = hme_mv_clipper;
        ps_func_list->pf_qpel_interp_avg_1pt = hme_qpel_interp_avg_1pt;
        ps_func_list->pf_qpel_interp_avg_2pt_horz_with_reuse = hme_qpel_interp_avg_2pt_horz_with_reuse;
        ps_func_list->pf_qpel_interp_avg_2pt_vert_with_reuse = hme_qpel_interp_avg_2pt_vert_with_reuse;
        ps_func_list->pf_qpel_interp_avg_generic = hme_qpel_interp_avg;
        ps_func_list->pf_store_4x4_sads_high_quality = hme_store_4x4_sads_high_quality;
        ps_func_list->pf_store_4x4_sads_high_speed = hme_store_4x4_sads_high_speed;
        break;
    }

    gapf_sad_pt_npu[BLK_4x4] = ps_func_list->pf_evalsad_pt_npu_width_multiple_4_8bit;
    gapf_sad_pt_npu[BLK_4x8] = ps_func_list->pf_evalsad_pt_npu_width_multiple_4_8bit;
    gapf_sad_pt_npu[BLK_8x4] = ps_func_list->pf_evalsad_pt_npu_8x4_8bit;
    gapf_sad_pt_npu[BLK_8x8] = ps_func_list->pf_evalsad_pt_npu_width_multiple_8_8bit;
    gapf_sad_pt_npu[BLK_4x16] = ps_func_list->pf_evalsad_pt_npu_width_multiple_4_8bit;
    gapf_sad_pt_npu[BLK_8x16] = ps_func_list->pf_evalsad_pt_npu_width_multiple_8_8bit;
    gapf_sad_pt_npu[BLK_12x16] = ps_func_list->pf_evalsad_pt_npu_12x16_8bit;
    gapf_sad_pt_npu[BLK_16x4] = ps_func_list->pf_evalsad_pt_npu_16x4_8bit;
    gapf_sad_pt_npu[BLK_16x8] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_16x12] = ps_func_list->pf_evalsad_pt_npu_16x12_8bit;
    gapf_sad_pt_npu[BLK_16x16] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_8x32] = ps_func_list->pf_evalsad_pt_npu_width_multiple_8_8bit;
    gapf_sad_pt_npu[BLK_16x32] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_24x32] = ps_func_list->pf_evalsad_pt_npu_24x32_8bit;
    gapf_sad_pt_npu[BLK_32x8] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_32x16] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_32x8] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_32x16] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_32x24] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_32x32] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_16x64] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_32x64] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_48x64] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_64x16] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_64x32] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_64x48] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;
    gapf_sad_pt_npu[BLK_64x64] = ps_func_list->pf_evalsad_pt_npu_width_multiple_16_8bit;

    gpf_part_sads_evaluator_16x16CU = ps_func_list->pf_compute_4x4_sads_for_16x16_blk;
    gpf_part_sads_evaluator_MxM = ps_func_list->pf_evalsad_grid_pu_MxM;

    gpf_sad_grid_mxn = ps_func_list->pf_evalsad_grid_npu_MxN;

    gapf_calc_sad_and_result_fxn[0][0] = ps_func_list->pf_calc_sad_and_1_best_result_num_part_1_for_grid;
    gapf_calc_sad_and_result_fxn[1][0] = ps_func_list->pf_calc_sad_and_1_best_result_num_part_eq_1;
    gapf_calc_sad_and_result_fxn[2][0] = ps_func_list->pf_calc_sad_and_1_best_result_num_square_parts;
    gapf_calc_sad_and_result_fxn[3][0] = ps_func_list->pf_calc_sad_and_1_best_result_num_part_lt_9;
    gapf_calc_sad_and_result_fxn[4][0] = ps_func_list->pf_calc_sad_and_1_best_result_num_part_lt_17;
    gapf_calc_sad_and_result_fxn[5][0] = ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_eq_1;
    gapf_calc_sad_and_result_fxn[6][0] = ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_square_parts;
    gapf_calc_sad_and_result_fxn[7][0] = ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_9;
    gapf_calc_sad_and_result_fxn[8][0] = ps_func_list->pf_calc_stim_injected_sad_and_1_best_result_num_part_lt_17;
    gapf_calc_sad_and_result_fxn[0][1] = ps_func_list->pf_calc_sad_and_2_best_results_num_part_1_for_grid;
    gapf_calc_sad_and_result_fxn[1][1] = ps_func_list->pf_calc_sad_and_2_best_results_num_part_eq_1;
    gapf_calc_sad_and_result_fxn[2][1] = ps_func_list->pf_calc_sad_and_2_best_results_num_square_parts;
    gapf_calc_sad_and_result_fxn[3][1] = ps_func_list->pf_calc_sad_and_2_best_results_num_part_lt_9;
    gapf_calc_sad_and_result_fxn[4][1] = ps_func_list->pf_calc_sad_and_2_best_results_num_part_lt_17;
    gapf_calc_sad_and_result_fxn[5][1] = ps_func_list->pf_calc_stim_injected_sad_and_2_best_results_num_part_eq_1;
    gapf_calc_sad_and_result_fxn[6][1] = ps_func_list->pf_calc_stim_injected_sad_and_2_best_results_num_square_parts;
    gapf_calc_sad_and_result_fxn[7][1] = ps_func_list->pf_calc_stim_injected_sad_and_2_best_results_num_part_lt_9;
    gapf_calc_sad_and_result_fxn[8][1] = ps_func_list->pf_calc_stim_injected_sad_and_2_best_results_num_part_lt_17;
}
// clang-format on

FT_CALC_SAD_AND_RESULT *hme_get_calc_sad_and_result_fxn(
    S08 i1_grid_flag, U08 u1_is_cu_noisy, S32 i4_part_mask, S32 num_parts, S32 num_results)
{
    U08 u1_index;

    ASSERT((1 == num_results) || (2 == num_results));

    u1_index =
        gau1_calc_sad_and_result[i1_grid_flag][u1_is_cu_noisy]
                                [(!!(i4_part_mask & (ENABLE_SMP | ENABLE_NxN)) &&
                                  !(i4_part_mask & ENABLE_AMP))
                                     ? (!!(i4_part_mask & ENABLE_NxN) ? 0 : 1)
                                     : (!!(i4_part_mask & ENABLE_AMP) ? 2 : 3)][num_parts - 1];

    return gapf_calc_sad_and_result_fxn[u1_index][2 == num_results];
}

void hme_evalsad_grid_pu_MxM(err_prms_t *ps_prms)
{
    grid_ctxt_t s_grid;
    cand_t as_candt[9];

    S32 *api4_sad_grid[TOT_NUM_PARTS];

    hme_mv_t s_mv = { 0, 0 };

    CU_SIZE_T e_cu_size = (CU_SIZE_T)(hme_get_range(ps_prms->i4_blk_wd) - 4);

    S32 i4_ref_idx = 0, i;
    S32 num_candts = 0;

    s_grid.num_grids = 1;
    s_grid.ref_buf_stride = ps_prms->i4_ref_stride;
    s_grid.grd_sz_y_x = ((ps_prms->i4_step << 16) | ps_prms->i4_step);
    s_grid.ppu1_ref_ptr = &ps_prms->pu1_ref;
    s_grid.pi4_grd_mask = &ps_prms->i4_grid_mask;
    s_grid.p_mv = &s_mv;
    s_grid.p_ref_idx = &i4_ref_idx;

    for(i = 0; i < 9; i++)
    {
        if(s_grid.pi4_grd_mask[0] & (1 << i))
        {
            num_candts++;
        }
    }

    for(i = 0; i < TOT_NUM_PARTS; i++)
    {
        api4_sad_grid[i] = &ps_prms->pi4_sad_grid[i * num_candts];
    }

    gpf_part_sads_evaluator_MxM(
        &s_grid,
        ps_prms->pu1_inp,
        ps_prms->i4_inp_stride,
        (WORD32 **)api4_sad_grid,
        as_candt,
        &num_candts,
        e_cu_size);
}

PF_SAD_FXN_T hme_get_sad_fxn(BLK_SIZE_T e_blk_size, S32 i4_grid_mask, S32 i4_part_mask)
{
    S32 i4_grid_en = ((i4_grid_mask & 0x1fe) != 0);

    if(i4_grid_en)
    {
        if(i4_part_mask & (i4_part_mask - 1))
        {
            if(BLK_16x16 == e_blk_size)
            {
                return hme_evalsad_grid_pu_16x16;
            }
            else
            {
                return hme_evalsad_grid_pu_MxM;
            }
        }
        else
        {
            return gpf_sad_grid_mxn;
        }
    }
    else
    {
        if(i4_part_mask & (i4_part_mask - 1))
        {
            if(BLK_16x16 == e_blk_size)
            {
                return hme_evalsad_grid_pu_16x16;
            }
            else
            {
                return hme_evalsad_grid_pu_MxM;
            }
        }
        else
        {
            return gapf_sad_pt_npu[e_blk_size];
        }
    }
}

void ihevce_sifter_sad_fxn_assigner(FT_SAD_EVALUATOR **ppf_evalsad_pt_npu_mxn, IV_ARCH_T e_arch)
{
    switch(e_arch)
    {
#ifdef ENABLE_NEON
    case ARCH_ARM_A9Q:
    case ARCH_ARM_V8_NEON:
        ppf_evalsad_pt_npu_mxn[0] = hme_evalsad_pt_npu_MxN_8bit_neon;
        break;
#endif

    default:
        ppf_evalsad_pt_npu_mxn[0] = hme_evalsad_pt_npu_MxN_8bit;
        break;
    }
}
