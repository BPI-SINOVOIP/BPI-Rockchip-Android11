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
* @file ihevce_trace.h
*
* @brief
*  This file contains entropy and cabac trace related structures and macros
*
* @author
*    Ittiam
******************************************************************************
*/

#ifndef _IHEVCE_TRACE_H_
#define _IHEVCE_TRACE_H_

#define ENABLE_TRACE 0

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/* strings assigned for prefix names */
// clang-format off
#define SEQ_LEVEL             "SEQ"    /*!< prefix string for sequence params */
#define HRD_LEVEL             "HRD"    /*!< prefix string for hrd params */
#define PIC_LEVEL             "PIC_INFO"    /*!< prefix string for picture params */
#define SLICE_LEVEL           "SLICE"  /*!< prefix string for slice params */
#define MB_LEVEL              "MB"     /*!< prefix string for MB params */
#define ECD_DATA              "ECD"
#define LYR_COEFF_LEVEL       "LYR"    /*!< prefix string for current layer tx levels */
#define ACC_COEFF_LEVEL       "LYR"    /*!< prefix string for accumulated tx levels/coeffs */
#define ACC_COEFFS            "LYR"    /*!< prefix string for accumulated coeffs */
#define LYR_DIFF_SIG          "LYR"    /*!< prefix string for MB params */
#define LYR_IP_SIG            "LYR"    /*!< prefix string for MB params */
#define RES_CHANGE_SIG        "RES CGE"
#define REF_BASE_DEBLK        "REF BASE"    /*!< refix string for ref base parameters */
#define TARGET_DEBLK          "TGT"    /*!< prefix string for target layer parameters */
#define TARGET_MC             "TGT"    /*!< prefix string for target layer parameters */
#define DUMMY                 "NOT VALID"
// clang-format on

#define TRACE(a) ihevce_trace((a))

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    IHEVCE_FALSE = 0,
    IHEVCE_TRUE = 1
} IHEVCE_BOOL_T;

// clang-format off
typedef enum
{
    TRACE_SEQ             = 0x00000001,   /*!< sequence params dump enable */
    TRACE_PIC             = 0x00000002,   /*!< picparams dump enable */
    TRACE_SLICE           = 0x00000004,   /*!< slice params dump enable */
    TRACE_MB_PARAMS       = 0x00000008,   /*!< mb level decoded dump enable */
    TRACE_MB_INF_PARAMS   = 0x00000010,   /*!< mb level inferred dumping enable */
    TRACE_ECD_DATA        = 0x00000020,   /*!< ECD data dump */
    TRACE_LYR_COEFF_LEVEL = 0x00000040,   /*!< Current layer coeff levels */
    TRACE_ACC_COEFF_LEVEL = 0x00000080,   /*!< Accumulated coffs/level */
    TRACE_ACC_COEFFS      = 0x00000100,   /*!< Accumulated coeffs */
    TRACE_LYR_DIFF_SIG    = 0x00000200,   /*!< layer level differential signal */
    TRACE_LYR_IP_SIG      = 0x00000400,   /*!< layer level Intra pred signal */
    TRACE_INTRA_UPSMPL_SIG= 0x00000800,   /*!< Intra upsampled data */
    TRACE_RES_UPSMPL_SIG  = 0x00001000,   /*!< Residual upsampled data */
    TRACE_BS_INFO         = 0x00002000,   /*!< BS information */
    TRACE_RES_CGE_MV      = 0x00100000,   /*!< Res change Motion vectors */
    TRACE_RES_CGE_MODE    = 0x00200000,   /*!< Res change MB modes */
    TRACE_RES_CGE_DATA    = 0x00400000,   /*!< Res change data */
    TRACE_TGT_MC_PRED     = 0x00800000,   /*!< moiton comp pred sugnal dump enable */
    TRACE_TGT_LYR_DEBLK   = 0x08000000,   /*!< Input to target layer deblocking */
    TRACE_REF_BASE_DEBLK  = 0x10000000,   /*!< deblocked data dumping enable */
    TRACE_ALL             = 0xFFFFFFFF    /*!< all params dumping enable */
}TRACE_PREFIX_T;
// clang-format on

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
extern const char *g_api1_prefix_name[32];

/* Dummy macros when trace is disabled */
#define ENTROPY_TRACE(syntax_string, value)

#define AEV_TRACE(string, value, range)

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
WORD32 ihevce_trace(UWORD32 u4_prefix);

#endif  //_IHEVCE_TRACE_H_
