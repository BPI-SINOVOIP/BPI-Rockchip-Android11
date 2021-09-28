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
*******************************************************************************
* @file
*  hme_function_selector.c
*
* @brief
*  Contains functions to initialize function pointers used in hevc me
*
* @author
*  ittiam
*
* @par List of Functions:
*
*
* @remarks
*  None
*
*******************************************************************************
*/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <stdlib.h>
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
#include "ihevce_global_tables.h"

#include "hme_datatype.h"
#include "hme_common_defs.h"
#include "hme_common_utils.h"
#include "hme_interface.h"
#include "hme_defs.h"
#include "hme_err_compute.h"
#include "hme_globals.h"

#include "ihevce_me_instr_set_router.h"

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

// clang-format off

#ifdef ENABLE_NEON
FT_CALC_SATD_AND_RESULT hme_evalsatd_update_1_best_result_pt_pu_16x16_neon;
#endif

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/


#ifdef ENABLE_NEON
void hme_init_function_ptr_neon(void *pv_me_context)
{
    me_master_ctxt_t *pv_me_ctxt = (me_master_ctxt_t *)pv_me_context;

    // clang-format off
    pv_me_ctxt->s_func_selector.pf_had_8x8_using_4_4x4_r = &ihevce_had_8x8_using_4_4x4_r_neon;
    pv_me_ctxt->s_func_selector.pf_had_16x16_r = &ihevce_had_16x16_r_neon;
    pv_me_ctxt->s_func_selector.pf_compute_32x32HAD_using_16x16 = &ihevce_compute_32x32HAD_using_16x16_neon;
    pv_me_ctxt->s_func_selector.pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_eq_1 = hme_evalsatd_update_1_best_result_pt_pu_16x16_neon;
    pv_me_ctxt->s_func_selector.pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_9 = hme_evalsatd_update_1_best_result_pt_pu_16x16_neon;
    pv_me_ctxt->s_func_selector.pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_17 = hme_evalsatd_update_1_best_result_pt_pu_16x16_neon;
    // clang-format on
}
#endif

void hme_init_function_ptr_generic(void *pv_me_context)
{
    me_master_ctxt_t *pv_me_ctxt = (me_master_ctxt_t *)pv_me_context;

    // clang-format off
    pv_me_ctxt->s_func_selector.pf_had_8x8_using_4_4x4_r = &ihevce_had_8x8_using_4_4x4_r;
    pv_me_ctxt->s_func_selector.pf_had_16x16_r = &ihevce_had_16x16_r;
    pv_me_ctxt->s_func_selector.pf_compute_32x32HAD_using_16x16 = &ihevce_compute_32x32HAD_using_16x16;
    pv_me_ctxt->s_func_selector.pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_eq_1 = hme_evalsatd_update_1_best_result_pt_pu_16x16;
    pv_me_ctxt->s_func_selector.pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_9 = hme_evalsatd_update_1_best_result_pt_pu_16x16;
    pv_me_ctxt->s_func_selector.pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_17 = hme_evalsatd_update_1_best_result_pt_pu_16x16;
    // clang-format on
}

void hme_init_function_ptr(void *pv_me_context, IV_ARCH_T e_processor_arch)
{
    switch(e_processor_arch)
    {
#ifdef ENABLE_NEON
    case ARCH_ARM_A9Q:
    case ARCH_ARM_V8_NEON:
        hme_init_function_ptr_neon(pv_me_context);
        break;
#endif
    default:
        hme_init_function_ptr_generic(pv_me_context);
        break;
    }
}
