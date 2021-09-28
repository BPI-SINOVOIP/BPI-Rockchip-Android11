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
* \file ihevce_ipe_instr_set_router.h
*
* \brief
*    This file contains declarations related to pre enc intra pred estimation
*    functions used in encoder
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

#ifndef __IHEVCE_IPE_INSTR_SET_ROUTER_H_
#define __IHEVCE_IPE_INSTR_SET_ROUTER_H_

#include "ihevc_typedefs.h"
#include "ihevce_defs.h"
#include "ihevce_cmn_utils_instr_set_router.h"

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef UWORD16 FT_SAD_COMPUTER(UWORD8 *, UWORD8 *, WORD32, WORD32);

typedef UWORD32 FT_BLK_SAD_COMPUTER(UWORD8 *, UWORD8 *, WORD32, WORD32, WORD32, WORD32);

typedef WORD32 FT_SAD_COMPUTER_GENERIC(UWORD8 *, WORD32, UWORD8 *, WORD32, WORD32);

typedef void
    FT_SCALING_FILTER_BY_2(UWORD8 *, WORD32, UWORD8 *, WORD32, UWORD8 *, WORD32, WORD32, WORD32);

typedef void FT_SCALE_BY_2(
    UWORD8 *,
    WORD32,
    UWORD8 *,
    WORD32,
    WORD32,
    WORD32,
    UWORD8 *,
    WORD32,
    WORD32,
    WORD32,
    WORD32,
    FT_COPY_2D *,
    FT_SCALING_FILTER_BY_2 *);

typedef void FT_ED_4X4_FIND_BEST_MODES(
    UWORD8 *, WORD32, UWORD8 *, UWORD16 *, UWORD8 *, WORD32 *, WORD32, FT_SAD_COMPUTER *);

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    FT_SAD_COMPUTER *pf_4x4_sad_computer;
    FT_SAD_COMPUTER *pf_8x8_sad_computer;
    FT_SAD_COMPUTER_GENERIC *pf_nxn_sad_computer;
    FT_SCALING_FILTER_BY_2 *pf_scaling_filter_mxn;
    FT_ED_4X4_FIND_BEST_MODES *pf_ed_4x4_find_best_modes;
} ihevce_ipe_optimised_function_list_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void ihevce_ipe_instr_set_router(
    ihevce_ipe_optimised_function_list_t *ps_func_list, IV_ARCH_T e_arch);

/* Function List - C */
FT_SAD_COMPUTER ihevce_4x4_sad_computer;
FT_SAD_COMPUTER ihevce_8x8_sad_computer;
FT_SAD_COMPUTER_GENERIC ihevce_nxn_sad_computer;
FT_SCALE_BY_2 ihevce_scale_by_2;
FT_SCALING_FILTER_BY_2 ihevce_scaling_filter_mxn;
FT_ED_4X4_FIND_BEST_MODES ihevce_ed_4x4_find_best_modes;

#ifdef ENABLE_NEON
/* Function List - ARM Neon */
FT_SAD_COMPUTER ihevce_4x4_sad_computer_neon;
FT_SAD_COMPUTER ihevce_8x8_sad_computer_neon;
FT_SAD_COMPUTER_GENERIC ihevce_nxn_sad_computer_neon;
FT_BLK_SAD_COMPUTER ihevce_4mx4n_sad_computer_neon;
FT_SCALING_FILTER_BY_2 ihevce_scaling_filter_mxn_neon;
#endif

#endif
