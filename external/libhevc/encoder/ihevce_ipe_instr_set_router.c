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
* \file ihevce_ipe_instr_set_router.c
*
* \brief
*    This file contains function pointer initialization of functions used during
*    pre-enc intra pred estimation
*
* \date
*    15/07/2013
*
* \author
*    Ittiam
*
* List of Functions
*  ihevce_ipe_instr_set_router()
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
#include "ihevce_ipe_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_ipe_instr_set_router \endif
*
* \brief
*    Function pointer initialization of pre enc ipe struct
*
*****************************************************************************
*/
void ihevce_ipe_instr_set_router(
    ihevce_ipe_optimised_function_list_t *ps_func_list, IV_ARCH_T e_arch)
{
    // clang-format off
    switch(e_arch)
    {

#ifdef ENABLE_NEON
    case ARCH_ARM_A9Q:
    case ARCH_ARM_V8_NEON:
        ps_func_list->pf_4x4_sad_computer = ihevce_4x4_sad_computer_neon;
        ps_func_list->pf_8x8_sad_computer = ihevce_8x8_sad_computer_neon;
        ps_func_list->pf_ed_4x4_find_best_modes = ihevce_ed_4x4_find_best_modes;
        ps_func_list->pf_nxn_sad_computer = ihevce_nxn_sad_computer_neon;
        ps_func_list->pf_scaling_filter_mxn = ihevce_scaling_filter_mxn_neon;
        break;
#endif

    default:
        ps_func_list->pf_4x4_sad_computer = ihevce_4x4_sad_computer;
        ps_func_list->pf_8x8_sad_computer = ihevce_8x8_sad_computer;
        ps_func_list->pf_ed_4x4_find_best_modes = ihevce_ed_4x4_find_best_modes;
        ps_func_list->pf_nxn_sad_computer = ihevce_nxn_sad_computer;
        ps_func_list->pf_scaling_filter_mxn = ihevce_scaling_filter_mxn;
        break;
    }
    // clang-format on
}
