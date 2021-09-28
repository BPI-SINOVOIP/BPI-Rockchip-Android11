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
* \file ihevce_cmn_utils_instr_set_router.h
*
* \brief
*    This file contains declarations related to common utilities used in encoder
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

#ifndef __IHEVCE_CMN_UTILS_INSTR_SET_ROUTER_H_
#define __IHEVCE_CMN_UTILS_INSTR_SET_ROUTER_H_

#include "ihevc_typedefs.h"
#include "ihevc_defs.h"
#include "ihevce_defs.h"

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef UWORD32 FT_CALC_HAD_SATD_8BIT(UWORD8 *, WORD32, UWORD8 *, WORD32, WORD16 *, WORD32);

typedef LWORD64 FT_SSD_CALCULATOR(
    UWORD8 *, UWORD8 *, UWORD32, UWORD32, UWORD32, UWORD32, CHROMA_PLANE_ID_T);

typedef LWORD64 FT_SSD_AND_SAD_CALCULATOR(UWORD8 *, WORD32, UWORD8 *, WORD32, WORD32, UWORD32 *);

typedef void FT_CHROMA_INTERLEAVE_2D_COPY(
    UWORD8 *, WORD32, UWORD8 *, WORD32, WORD32, WORD32, CHROMA_PLANE_ID_T);

typedef void FT_COPY_2D(UWORD8 *, WORD32, UWORD8 *, WORD32, WORD32, WORD32);

typedef void FT_2D_SQUARE_COPY(void *, WORD32, void *, WORD32, WORD32, WORD32);

typedef void FT_WT_AVG_2D(
    UWORD8 *,
    UWORD8 *,
    WORD32,
    WORD32,
    WORD32,
    WORD32,
    UWORD8 *,
    WORD32,
    WORD32,
    WORD32,
    WORD32,
    WORD32,
    WORD32);

typedef void
    FT_ITRANS_RECON_DC(UWORD8 *, WORD32, UWORD8 *, WORD32, WORD32, WORD16, CHROMA_PLANE_ID_T);

typedef WORD32 FT_SCAN_COEFFS(WORD16 *, WORD32 *, WORD32, WORD32, UWORD8 *, UWORD8 *, WORD32);

typedef void FT_GET_EO_SAO_PARAMS(void *, WORD32, WORD32 *, WORD32 *);

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    FT_CALC_HAD_SATD_8BIT *pf_HAD_4x4_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_HAD_8x8_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_HAD_16x16_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_HAD_32x32_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_AC_HAD_8x8_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_chroma_HAD_4x4_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_chroma_AC_HAD_4x4_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_chroma_HAD_8x8_8bit;
    FT_CALC_HAD_SATD_8BIT *pf_chroma_HAD_16x16_8bit;
    FT_SSD_CALCULATOR *pf_ssd_calculator;
    FT_SSD_CALCULATOR *pf_chroma_interleave_ssd_calculator;
    FT_SSD_AND_SAD_CALCULATOR *pf_ssd_and_sad_calculator;
    FT_CHROMA_INTERLEAVE_2D_COPY *pf_chroma_interleave_2d_copy;
    FT_COPY_2D *pf_copy_2d;
    FT_2D_SQUARE_COPY *pf_2d_square_copy;
    FT_WT_AVG_2D *pf_wt_avg_2d;
    FT_ITRANS_RECON_DC *pf_itrans_recon_dc;
    FT_SCAN_COEFFS *pf_scan_coeffs;
    FT_GET_EO_SAO_PARAMS *pf_get_luma_eo_sao_params;
    FT_GET_EO_SAO_PARAMS *pf_get_chroma_eo_sao_params;
} ihevce_cmn_opt_func_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void ihevce_cmn_utils_instr_set_router(
    ihevce_cmn_opt_func_t *ps_func_list, UWORD8 u1_is_popcnt_available, IV_ARCH_T e_arch);

/* Function List - C */
FT_CALC_HAD_SATD_8BIT ihevce_HAD_4x4_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_HAD_8x8_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_HAD_16x16_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_HAD_32x32_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_compute_ac_had_8x8_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_HAD_4x4_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_compute_AC_HAD_4x4_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_HAD_8x8_8bit;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_HAD_16x16_8bit;
FT_SSD_CALCULATOR ihevce_ssd_calculator;
FT_SSD_CALCULATOR ihevce_chroma_interleave_ssd_calculator;
FT_SSD_AND_SAD_CALCULATOR ihevce_ssd_and_sad_calculator;
FT_CHROMA_INTERLEAVE_2D_COPY ihevce_chroma_interleave_2d_copy;
FT_COPY_2D ihevce_copy_2d;
FT_2D_SQUARE_COPY ihevce_2d_square_copy_luma;
FT_WT_AVG_2D ihevce_wt_avg_2d;
FT_ITRANS_RECON_DC ihevce_itrans_recon_dc;
FT_SCAN_COEFFS ihevce_scan_coeffs;
FT_GET_EO_SAO_PARAMS ihevce_get_luma_eo_sao_params;
FT_GET_EO_SAO_PARAMS ihevce_get_chroma_eo_sao_params;

#ifdef ENABLE_NEON
FT_CALC_HAD_SATD_8BIT ihevce_HAD_4x4_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_HAD_8x8_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_compute_AC_HAD_4x4_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_compute_ac_had_8x8_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_HAD_16x16_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_HAD_4x4_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_HAD_8x8_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_chroma_HAD_16x16_8bit_neon;
FT_CALC_HAD_SATD_8BIT ihevce_HAD_32x32_8bit_neon;
FT_SSD_CALCULATOR ihevce_ssd_calculator_neon;
FT_SSD_CALCULATOR ihevce_chroma_interleave_ssd_calculator_neon;
FT_SSD_AND_SAD_CALCULATOR ihevce_ssd_and_sad_calculator_neon;
FT_2D_SQUARE_COPY ihevce_2d_square_copy_luma_neon;
FT_CHROMA_INTERLEAVE_2D_COPY ihevce_chroma_interleave_2d_copy_neon;
FT_COPY_2D ihevce_copy_2d_neon;
FT_GET_EO_SAO_PARAMS ihevce_get_luma_eo_sao_params_neon;
FT_ITRANS_RECON_DC ihevce_itrans_recon_dc_neon;
FT_GET_EO_SAO_PARAMS ihevce_get_chroma_eo_sao_params_neon;
FT_SCAN_COEFFS ihevce_scan_coeffs_neon;
FT_WT_AVG_2D ihevce_wt_avg_2d_neon;
#endif

#endif
