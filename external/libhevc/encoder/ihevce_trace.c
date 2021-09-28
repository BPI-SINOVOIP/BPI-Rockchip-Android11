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
* @file ihevce_trace.c
*
* @brief
*    This file contains function definitions for implementing trace
*
* @author
*    Ittiam
*
* List of Functions
*   ihevce_trace_deinit()
*   ihevce_trace_init()
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

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevc_debug.h"
#include "ihevc_defs.h"
#include "ihevc_macros.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_cabac_tables.h"
#include "itt_video_api.h"
#include "ihevce_api.h"
#include "ihevce_defs.h"
#include "ihevce_buffer_que_interface.h"
#include "ihevce_hle_interface.h"

#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_trace.h"

/*****************************************************************************/
/* Global Variable Definitions                                               */
/*****************************************************************************/

/* Table of Prefix names, one for each bit of the debug_id mask */
/* bits 12 - 16 are used inly to indicate the layer id */
/* add null strings to those locations */
// clang-format off
const char *g_api1_prefix_name[32] =
{
    SEQ_LEVEL,              /* TRACE_SEQ             = 0x00000001,   !< sequence params dump enable */
    PIC_LEVEL,              /* TRACE_PIC             = 0x00000002,   !< picparams dump enable */
    SLICE_LEVEL,            /* TRACE_SLICE           = 0x00000004,   !< slice params dump enable */
    MB_LEVEL,               /* TRACE_MB_PARAMS       = 0x00000008,   !< mb level decoded dump enable */
    MB_LEVEL,               /* TRACE_MB_INF_PARAMS   = 0x00000010,   !< mb level inferred dumping enable */
    ECD_DATA,               /* TRACE_ECD_DATA        = 0x00000020,   !< ECD data dump */
    LYR_COEFF_LEVEL,        /* TRACE_LYR_COEFF_LEVEL = 0x00000040,   !< Current layer coeff levels */
    ACC_COEFF_LEVEL,        /* TRACE_ACC_COEFF_LEVEL = 0x00000080,   !< Accumulated coffs/level */
    ACC_COEFFS,             /* TRACE_ACC_COEFFS      = 0x00000100,   !< Accumulated coeffs */
    LYR_DIFF_SIG,           /* TRACE_LYR_DIFF_SIG    = 0x00000200,   !< layer level differential signal */
    LYR_IP_SIG,             /* TRACE_LYR_IP_SIG      = 0x00000400,   !< layer level Intra pred signal */
    MB_LEVEL,               /* TRACE_INTRA_UPSMPL_SIG= 0x00000800,   !< Intra upsampled data */
    MB_LEVEL,               /* TRACE_RES_UPSMPL_SIG  = 0x00001000,   !< Residual upsampled data */
    MB_LEVEL,               /* TRACE_BS_INFO         = 0x00002000,   !< BS information */
    DUMMY,                  /* 0x00004000 */
    DUMMY,                  /* 0x00008000 */
    DUMMY,                  /* 0x00010000 */
    DUMMY,                  /* 0x00020000 */
    DUMMY,                  /* 0x00040000 */
    DUMMY,                  /* 0x00080000 */
    RES_CHANGE_SIG,         /* TRACE_RES_CGE_MV      = 0x00100000,   !< Res change Motion vectors */
    RES_CHANGE_SIG,         /* TRACE_RES_CGE_MODE    = 0x00200000,   !< Res change MB modes */
    RES_CHANGE_SIG,         /* TRACE_RES_CGE_DATA    = 0x00400000,   !< Res change data */
    TARGET_MC,              /* TRACE_TGT_MC_PRED     = 0x00800000,   !< moiton comp pred sugnal dump enable */
    DUMMY,                  /* 0x01000000 */
    DUMMY,                  /* 0x02000000 */
    DUMMY,                  /* 0x04000000 */
    TARGET_DEBLK,           /* TRACE_TGT_LYR_DEBLK   = 0x08000000,   !< Input to target layer deblocking */
    REF_BASE_DEBLK,         /* TRACE_REF_BASE_DEBLK  = 0x10000000,   !< deblocked data dumping enable */
    DUMMY,                  /* 0x20000000 */
    DUMMY,                  /* 0x40000000 */
    DUMMY                   /* 0x80000000 */
};
// clang-format on

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Dummar trace init when trace is disabled in encoder
*
*  @par   Description
*  This routine needs to be called at start of trace
*
*  @param[in]   pu1_file_name
*  Name of file where trace outputs need to be stores (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_trace_init(UWORD8 *pu1_file_name)
{
    (void)pu1_file_name;
    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Dummar trace de-init function when trace is disabled
*
*  @par   Description
*  This routine needs to be called at end of trace
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_trace_deinit(void)
{
    return IHEVCE_SUCCESS;
}

/** \brief This function return the bit position set of the input */
WORD32 svcd_trace_get_bit_pos(UWORD32 u4_input)
{
    /* local variables */
    WORD32 i4_bit_pos;

    i4_bit_pos = -1;

    /* only a single bit of 32 bits should to be set */
    assert(0 == (u4_input & (u4_input - 1)));

    /* loop to get the bit position of the prefix */
    while(0 != u4_input)
    {
        u4_input >>= 1;
        i4_bit_pos++;
    } /* end of while loop */

    /* check on validity of the bit position */
    assert((31 >= i4_bit_pos) && (0 <= i4_bit_pos));

    return (i4_bit_pos);
}

/** \brief This function does the parameter dumping for trace info */

WORD32 ihevce_trace(UWORD32 u4_prefix)
{
    WORD32 i4_array_indx;

    /* get the bit position of the prefix */
    i4_array_indx = svcd_trace_get_bit_pos(u4_prefix);
    return i4_array_indx;
}
