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
* \file ihevce_cmn_utils_instr_set_router.c
*
* \brief
*    This file contains function pointer initialization of common utility
*    functions
*
* \date
*    15/07/2013
*
* \author
*    Ittiam
*
* List of Functions
*  ihevce_cmn_utils_instr_set_router()
*
******************************************************************************
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
#include "ihevc_debug.h"

#include "ihevce_cmn_utils_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_cmn_utils_instr_set_router \endif
*
* \brief
*    Function pointer initialization of common utils struct
*
*****************************************************************************
*/
void ihevce_cmn_utils_instr_set_router(
    ihevce_cmn_opt_func_t *ps_func_list, UWORD8 u1_is_popcnt_available, IV_ARCH_T e_arch)
{
    // clang-format off
    switch(e_arch)
    {
    (void)u1_is_popcnt_available;
#ifdef ENABLE_NEON
    case ARCH_ARM_A9Q:
    case ARCH_ARM_V8_NEON:
        ps_func_list->pf_2d_square_copy = ihevce_2d_square_copy_luma_neon;
        ps_func_list->pf_AC_HAD_8x8_8bit = ihevce_compute_ac_had_8x8_8bit_neon;
        ps_func_list->pf_chroma_AC_HAD_4x4_8bit = ihevce_chroma_compute_AC_HAD_4x4_8bit_neon;
        ps_func_list->pf_chroma_HAD_16x16_8bit = ihevce_chroma_HAD_16x16_8bit_neon;
        ps_func_list->pf_chroma_HAD_4x4_8bit = ihevce_chroma_HAD_4x4_8bit_neon;
        ps_func_list->pf_chroma_HAD_8x8_8bit = ihevce_chroma_HAD_8x8_8bit_neon;
        ps_func_list->pf_chroma_interleave_2d_copy = ihevce_chroma_interleave_2d_copy_neon;
        ps_func_list->pf_chroma_interleave_ssd_calculator = ihevce_chroma_interleave_ssd_calculator_neon;
        ps_func_list->pf_copy_2d = ihevce_copy_2d_neon;
        ps_func_list->pf_get_chroma_eo_sao_params = ihevce_get_chroma_eo_sao_params_neon;
        ps_func_list->pf_get_luma_eo_sao_params = ihevce_get_luma_eo_sao_params_neon;
        ps_func_list->pf_HAD_16x16_8bit = ihevce_HAD_16x16_8bit_neon;
        ps_func_list->pf_HAD_32x32_8bit = ihevce_HAD_32x32_8bit_neon;
        ps_func_list->pf_HAD_4x4_8bit = ihevce_HAD_4x4_8bit_neon;
        ps_func_list->pf_HAD_8x8_8bit = ihevce_HAD_8x8_8bit_neon;
        ps_func_list->pf_itrans_recon_dc = ihevce_itrans_recon_dc_neon;
        ps_func_list->pf_scan_coeffs = ihevce_scan_coeffs_neon;
        ps_func_list->pf_ssd_and_sad_calculator = ihevce_ssd_and_sad_calculator_neon;
        ps_func_list->pf_ssd_calculator = ihevce_ssd_calculator_neon;
        ps_func_list->pf_wt_avg_2d = ihevce_wt_avg_2d_neon;
        break;
#endif
    default:
        ps_func_list->pf_2d_square_copy = ihevce_2d_square_copy_luma;
        ps_func_list->pf_AC_HAD_8x8_8bit = ihevce_compute_ac_had_8x8_8bit;
        ps_func_list->pf_chroma_AC_HAD_4x4_8bit = ihevce_chroma_compute_AC_HAD_4x4_8bit;
        ps_func_list->pf_chroma_HAD_16x16_8bit = ihevce_chroma_HAD_16x16_8bit;
        ps_func_list->pf_chroma_HAD_4x4_8bit = ihevce_chroma_HAD_4x4_8bit;
        ps_func_list->pf_chroma_HAD_8x8_8bit = ihevce_chroma_HAD_8x8_8bit;
        ps_func_list->pf_chroma_interleave_2d_copy = ihevce_chroma_interleave_2d_copy;
        ps_func_list->pf_chroma_interleave_ssd_calculator = ihevce_chroma_interleave_ssd_calculator;
        ps_func_list->pf_copy_2d = ihevce_copy_2d;
        ps_func_list->pf_get_chroma_eo_sao_params = ihevce_get_chroma_eo_sao_params;
        ps_func_list->pf_get_luma_eo_sao_params = ihevce_get_luma_eo_sao_params;
        ps_func_list->pf_HAD_16x16_8bit = ihevce_HAD_16x16_8bit;
        ps_func_list->pf_HAD_32x32_8bit = ihevce_HAD_32x32_8bit;
        ps_func_list->pf_HAD_4x4_8bit = ihevce_HAD_4x4_8bit;
        ps_func_list->pf_HAD_8x8_8bit = ihevce_HAD_8x8_8bit;
        ps_func_list->pf_itrans_recon_dc = ihevce_itrans_recon_dc;
        ps_func_list->pf_scan_coeffs = ihevce_scan_coeffs;
        ps_func_list->pf_ssd_and_sad_calculator = ihevce_ssd_and_sad_calculator;
        ps_func_list->pf_ssd_calculator = ihevce_ssd_calculator;
        ps_func_list->pf_wt_avg_2d = ihevce_wt_avg_2d;
        break;
    }
    // clang-format on
}
