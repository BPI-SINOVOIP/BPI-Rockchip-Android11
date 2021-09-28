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
*
* @file ihevce_global_tables.c
*
* @brief
*    This file contains definitions of global tables used by the encoder
*
* @author
*    Ittiam
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
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_sao.h"
#include "ihevc_resi_trans.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "hme_datatype.h"
#include "hme_common_defs.h"
#include "hme_common_utils.h"
#include "hme_interface.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_err_compute.h"
#include "hme_globals.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_enc_loop_pass.h"
#include "ihevce_global_tables.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
const level_data_t g_as_level_data[TOTAL_NUM_LEVELS] = {
    /* LEVEL1 */
    { LEVEL1, 552960, 36864, { 128, 0 }, { 350, 0 }, 2, 16, 1, 1 },

    /* LEVEL2 */
    { LEVEL2, 3686400, 122880, { 1500, 0 }, { 1500, 0 }, 2, 16, 1, 1 },

    /* LEVEL2_1 */
    { LEVEL2_1, 7372800, 245760, { 3000, 0 }, { 3000, 0 }, 2, 20, 1, 1 },

    /* LEVEL3 */
    { LEVEL3, 16588800, 552960, { 6000, 0 }, { 6000, 0 }, 2, 30, 2, 2 },

    /* LEVEL3_1 */
    { LEVEL3_1, 33177600, 983040, { 10000, 0 }, { 10000, 0 }, 2, 40, 3, 3 },

    /* LEVEL4 */
    { LEVEL4, 66846720, 2228224, { 12000, 30000 }, { 12000, 30000 }, 4, 75, 5, 5 },

    /* LEVEL4_1 */
    { LEVEL4_1, 133693440, 2228224, { 20000, 50000 }, { 20000, 50000 }, 4, 75, 5, 5 },

    /* LEVEL5 */
    { LEVEL5, 267386880, 8912896, { 25000, 100000 }, { 25000, 100000 }, 6, 200, 11, 10 },

    /* LEVEL5_1 */
    { LEVEL5_1, 534773760, 8912896, { 40000, 160000 }, { 40000, 160000 }, 8, 200, 11, 10 },

    /* LEVEL5_2 */
    { LEVEL5_2, 1069547520, 8912896, { 60000, 240000 }, { 60000, 240000 }, 8, 200, 11, 10 },

    /* LEVEL6 */
    { LEVEL6, 1069547520, 35651584, { 60000, 240000 }, { 60000, 240000 }, 8, 600, 22, 20 },

    /* LEVEL6_1 */
    { LEVEL6_1, 2139095040, 35651584, { 120000, 480000 }, { 120000, 480000 }, 8, 600, 22, 20 },

    /* LEVEL6_2 */
    { LEVEL6_2, 4278190080, 35651584, { 240000, 800000 }, { 240000, 800000 }, 6, 600, 22, 20 },

};

/** \brief Default flat Scaling matrix for 4x4 transform */
const WORD16 gi2_flat_scale_mat_4x4[] = { 16, 16, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16 };

/** \brief Default flat Scaling matrix for 8x8 transform */
const WORD16 gi2_flat_scale_mat_8x8[] = { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                                          16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 };

/** \brief Default flat Scaling matrix for 16x16 transform */
const WORD16 gi2_flat_scale_mat_16x16[] = {
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

/** \brief Default flat ReScaling matrix for 4x4 transform */
const WORD16 gi2_flat_rescale_mat_4x4[] = { 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
                                            2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048 };

/** \brief Default flat ReScaling matrix for 8x8 transform */
const WORD16 gi2_flat_rescale_mat_8x8[] = {
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048
};

/** \brief Default flat ReScaling matrix for 16x16 transform */
const WORD16 gi2_flat_rescale_mat_16x16[] = {
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048,
    2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048
};

/**
* @brief Give the scanning order of csb in a 32x32 TU
* based on first idx. 0 - upright_diagonal, 1 - horizontal, 2 - vertical scan
*/
const UWORD8 g_u1_scan_table_8x8[3][64] = {
    /* diag up right scan */
    { 0,  8,  1,  16, 9,  2,  24, 17, 10, 3,  32, 25, 18, 11, 4,  40, 33, 26, 19, 12, 5,  48,
      41, 34, 27, 20, 13, 6,  56, 49, 42, 35, 28, 21, 14, 7,  57, 50, 43, 36, 29, 22, 15, 58,
      51, 44, 37, 30, 23, 59, 52, 45, 38, 31, 60, 53, 46, 39, 61, 54, 47, 62, 55, 63 },

    /* horizontal scan */
    { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
      44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63 },

    /* vertical scan */
    { 0,  8,  16, 24, 32, 40, 48, 56, 1,  9,  17, 25, 33, 41, 49, 57, 2,  10, 18, 26, 34, 42,
      50, 58, 3,  11, 19, 27, 35, 43, 51, 59, 4,  12, 20, 28, 36, 44, 52, 60, 5,  13, 21, 29,
      37, 45, 53, 61, 6,  14, 22, 30, 38, 46, 54, 62, 7,  15, 23, 31, 39, 47, 55, 63 }
};

/**
* @brief Give the scanning order of csb in a 16x16 TU  or 4x4 csb
* based on first idx. 0 - upright_diagonal, 1 - horizontal, 2 - vertical scan
*/
const UWORD8 g_u1_scan_table_4x4[3][16] = {
    /* diag up right scan */
    { 0, 4, 1, 8, 5, 2, 12, 9, 6, 3, 13, 10, 7, 14, 11, 15 },

    /* horizontal scan */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },

    /* vertical scan */
    { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 }
};

/**
* @brief Give the scanning order of csb in a 8x8 TU
* based on first idx. 0 - upright_diagonal, 1 - horizontal, 2 - vertical scan
*/
const UWORD8 g_u1_scan_table_2x2[3][4] = {
    /* diag up right scan */
    { 0, 2, 1, 3 },

    /* horizontal scan */
    { 0, 1, 2, 3 },

    /* vertical scan */
    { 0, 2, 1, 3 }
};

/**
* @brief Give the scanning order of csb in a 4x4 TU
* scan idx. doesn't matter as it's 0 for all cases
*/
const UWORD8 g_u1_scan_table_1x1[1] = { 0 };

/**
******************************************************************************
*  @brief For a given frac pt, fracx, fracy, this module figures out the
*  corresponding fpel/hpel buffers along with x and y offsets if any. The
*  grid used is shown as follows:
*    A j E k B
*    l m n o p
*    F q G r H
*    s t u v w
*    C x I y D
*
*  In this grid capital letters are fpel/hpel bufs.
******************************************************************************
*/
qpel_input_buf_cfg_t gas_qpel_inp_buf_cfg[4][4] = {
    {
        /* 0, 0 pt: both buf id would be fxfy = 0 */
        { 0, 0, 0, 0, 0, 0 },
        /* 1, 0 pt: pt j; avg of A and E */
        { 0, 0, 0, 1, 0, 0 },
        /* 2, 0 pt: pt E, buf id 0 and 1 would be hxfy = 1 */
        { 1, 0, 0, 1, 0, 0 },
        /* 3, 0 pt: pt k, avg of E and B */
        { 1, 0, 0, 0, 1, 0 },
    },
    {
        /* 0, 1 pt: pt l: avg of A and F */
        { 0, 0, 0, 2, 0, 0 },
        /* 1, 1 pt: pt m : avg of E and F */
        { 1, 0, 0, 2, 0, 0 },
        /* 2, 2 pt: pt n: avg of E and G */
        { 1, 0, 0, 3, 0, 0 },
        /* 3, 2 pt : pt o: avg of E and H */
        { 1, 0, 0, 2, 1, 0 },
    },
    {
        /* 0, 2 pt: pt F; both buf id would be fxhy = 2 */
        { 2, 0, 0, 2, 0, 0 },
        /* 1, 2 pt: pt q; avg of F and G */
        { 2, 0, 0, 3, 0, 0 },
        /* 2, 2 pt: pt G: both buf id would be hxhy = 3 */
        { 3, 0, 0, 3, 0, 0 },
        /* 2, 3 pt: pt r: avg of G and H */
        { 3, 0, 0, 2, 1, 0 },
    },
    {
        /* 0, 3 pt: pt s; avg of F and C */
        { 2, 0, 0, 0, 0, 1 },
        /* 1, 3 ot: pt t; avg of F and I */
        { 2, 0, 0, 1, 0, 1 },
        /* 2, 3 pt: pt u, avg of G and I */
        { 3, 0, 0, 1, 0, 1 },
        /* 3, 3 pt; pt v, avg of H and I */
        { 2, 1, 0, 1, 0, 1 },
    }
};

/**
* @brief is partition vertical
*/
const WORD8 gai1_is_part_vertical[TOT_NUM_PARTS] = { 0, 1, 1, 0, 0, 0, 0, 0, 0,
                                                     1, 1, 1, 1, 0, 0, 0, 0 };

/**
* @brief partition dimensions
*/
const WORD8 gai1_part_wd_and_ht[TOT_NUM_PARTS][2] = { { 16, 16 }, { 16, 8 }, { 16, 8 },  { 8, 16 },
                                                      { 8, 16 },  { 8, 8 },  { 8, 8 },   { 8, 8 },
                                                      { 8, 8 },   { 16, 4 }, { 16, 12 }, { 16, 12 },
                                                      { 16, 4 },  { 4, 16 }, { 12, 16 }, { 12, 16 },
                                                      { 4, 16 } };

/**
******************************************************************************
*  @brief bits to code given ref id assuming more than 2 ref ids active
******************************************************************************
*/
UWORD8 gau1_ref_bits[16] = { 1, 3, 3, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7, 9 };

/**
* @brief raster to zscan lookup table
*/
const UWORD8 gau1_ctb_raster_to_zscan[256] = {
    0,   1,   4,   5,   16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,  85,  2,   3,   6,
    7,   18,  19,  22,  23,  66,  67,  70,  71,  82,  83,  86,  87,  8,   9,   12,  13,  24,  25,
    28,  29,  72,  73,  76,  77,  88,  89,  92,  93,  10,  11,  14,  15,  26,  27,  30,  31,  74,
    75,  78,  79,  90,  91,  94,  95,  32,  33,  36,  37,  48,  49,  52,  53,  96,  97,  100, 101,
    112, 113, 116, 117, 34,  35,  38,  39,  50,  51,  54,  55,  98,  99,  102, 103, 114, 115, 118,
    119, 40,  41,  44,  45,  56,  57,  60,  61,  104, 105, 108, 109, 120, 121, 124, 125, 42,  43,
    46,  47,  58,  59,  62,  63,  106, 107, 110, 111, 122, 123, 126, 127, 128, 129, 132, 133, 144,
    145, 148, 149, 192, 193, 196, 197, 208, 209, 212, 213, 130, 131, 134, 135, 146, 147, 150, 151,
    194, 195, 198, 199, 210, 211, 214, 215, 136, 137, 140, 141, 152, 153, 156, 157, 200, 201, 204,
    205, 216, 217, 220, 221, 138, 139, 142, 143, 154, 155, 158, 159, 202, 203, 206, 207, 218, 219,
    222, 223, 160, 161, 164, 165, 176, 177, 180, 181, 224, 225, 228, 229, 240, 241, 244, 245, 162,
    163, 166, 167, 178, 179, 182, 183, 226, 227, 230, 231, 242, 243, 246, 247, 168, 169, 172, 173,
    184, 185, 188, 189, 232, 233, 236, 237, 248, 249, 252, 253, 170, 171, 174, 175, 186, 187, 190,
    191, 234, 235, 238, 239, 250, 251, 254, 255
};

/**
* @brief <Fill me>
*/
UWORD32 gau4_frame_qstep_multiplier[54] = { 16, 16, 16, 15, 15, 15, 15, 15, 15, 13, 13, 13, 13, 12,
                                            12, 11, 11, 10, 10, 9,  9,  8,  8,  8,  7,  7,  7,  6,
                                            6,  5,  5,  5,  4,  4,  3,  3,  3,  2,  2,  2,  1,  1,
                                            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 };

/**
******************************************************************************
* @brief  Look up table for choosing the appropriate function for
*         Intra prediction
*
* @remarks Same look up table enums are used for luma & chroma but each
*          have seperate functions implemented
******************************************************************************
*/
WORD32 g_i4_ip_funcs[MAX_NUM_IP_MODES] = {
    IP_FUNC_MODE_0, /* Mode 0 */
    IP_FUNC_MODE_1, /* Mode 1 */
    IP_FUNC_MODE_2, /* Mode 2 */
    IP_FUNC_MODE_3TO9, /* Mode 3 */
    IP_FUNC_MODE_3TO9, /* Mode 4 */
    IP_FUNC_MODE_3TO9, /* Mode 5 */
    IP_FUNC_MODE_3TO9, /* Mode 6 */
    IP_FUNC_MODE_3TO9, /* Mode 7 */
    IP_FUNC_MODE_3TO9, /* Mode 8 */
    IP_FUNC_MODE_3TO9, /* Mode 9 */
    IP_FUNC_MODE_10, /* Mode 10 */
    IP_FUNC_MODE_11TO17, /* Mode 11 */
    IP_FUNC_MODE_11TO17, /* Mode 12 */
    IP_FUNC_MODE_11TO17, /* Mode 13 */
    IP_FUNC_MODE_11TO17, /* Mode 14 */
    IP_FUNC_MODE_11TO17, /* Mode 15 */
    IP_FUNC_MODE_11TO17, /* Mode 16 */
    IP_FUNC_MODE_11TO17, /* Mode 17 */
    IP_FUNC_MODE_18_34, /* Mode 18 */
    IP_FUNC_MODE_19TO25, /* Mode 19 */
    IP_FUNC_MODE_19TO25, /* Mode 20 */
    IP_FUNC_MODE_19TO25, /* Mode 21 */
    IP_FUNC_MODE_19TO25, /* Mode 22 */
    IP_FUNC_MODE_19TO25, /* Mode 23 */
    IP_FUNC_MODE_19TO25, /* Mode 24 */
    IP_FUNC_MODE_19TO25, /* Mode 25 */
    IP_FUNC_MODE_26, /* Mode 26 */
    IP_FUNC_MODE_27TO33, /* Mode 27 */
    IP_FUNC_MODE_27TO33, /* Mode 26 */
    IP_FUNC_MODE_27TO33, /* Mode 29 */
    IP_FUNC_MODE_27TO33, /* Mode 30 */
    IP_FUNC_MODE_27TO33, /* Mode 31 */
    IP_FUNC_MODE_27TO33, /* Mode 32 */
    IP_FUNC_MODE_27TO33, /* Mode 33 */
    IP_FUNC_MODE_18_34, /* Mode 34 */
};

/**
******************************************************************************
* @brief  Look up table for calculating the TU size for all the TUs in a CU
*         if CU part mode is one of SIZE_2Nx2N, SIZE_2NxN, SIZE_Nx2N
*
*         i ranging (0 to 3)
*         tu_size[i] = cu_size >> gau1_inter_tu_shft_amt[i];
*
* @remarks For non AMP cases only TU size = CU/2 is used
*          and number of TU partitions in these CU will be 4
******************************************************************************
*/
UWORD8 gau1_inter_tu_shft_amt[4] = {
    /* SIZE_2Nx2N, SIZE_2NxN, SIZE_Nx2N cases */
    1,
    1,
    1,
    1
};

/**
******************************************************************************
* @brief  Look up table for calculating the TU size for all the TUs in a CU
*         if CU part mode is one of SIZE_2NxnU, SIZE_2NxnD, SIZE_nLx2N
*         SIZE_nRx2N (AMP motion partition cases)
*
*         part_mode = {SIZE_2NxnU,SIZE_2NxnD,SIZE_nLx2N,SIZE_nRx2N}
*         i ranging (0 to 9)
*         tu_size[i] = cu_size >> gau1_inter_tu_shft_amt_amp[part_mode-4][i];
*
* @remarks For AMP cases a mixture of TU size = CU/2 & CU/4 is used
*          based on motion partition orientation, number of TU partitions
*          in these CU will be 10
******************************************************************************
*/
UWORD8 gau1_inter_tu_shft_amt_amp[4][10] = {
    /* SIZE_2NxnU case */
    { 2, 2, 2, 2, 2, 2, 2, 2, 1, 1 },

    /* SIZE_2NxnD case */
    { 1, 1, 2, 2, 2, 2, 2, 2, 2, 2 },

    /* SIZE_nLx2N case */
    { 2, 2, 2, 2, 1, 2, 2, 2, 2, 1 },

    /* SIZE_nRx2N case */
    { 1, 2, 2, 2, 2, 1, 2, 2, 2, 2 }
};

/**
******************************************************************************
* @brief  Look up table for calculating the TU position in horizontal
*         for all the TUs in a CU, if CU part mode is one of
*         SIZE_2Nx2N, SIZE_2NxN, SIZE_Nx2N
*
*         i ranging (0 to 3)
*         tu_posx[i](in pixels in cu) =
*                 ((cusize >> 2) * gau1_inter_tu_posx_scl_amt[i]);
******************************************************************************
*/
UWORD8 gau1_inter_tu_posx_scl_amt[4] = {
    /* SIZE_2Nx2N, SIZE_2NxN, SIZE_Nx2N cases */
    0,
    2,
    0,
    2
};

/**
******************************************************************************
* @brief  Look up table for calculating the TU position in horizontal
*         for all the TUs in a CU, if CU part mode is one of
*         SIZE_2NxnU, SIZE_2NxnD, SIZE_nLx2N,SIZE_nRx2N (AMP motion partition cases)
*
*         part_mode = {SIZE_2NxnU,SIZE_2NxnD,SIZE_nLx2N,SIZE_nRx2N}
*         i ranging (0 to 9)
*         tu_posx[i](in pixels in cu) =
*              ((cusize >> 2) * gau1_inter_tu_posx_scl_amt_amp[part_mode-4][i]);
******************************************************************************
*/
UWORD8 gau1_inter_tu_posx_scl_amt_amp[4][10] = {
    /* SIZE_2NxnU case */
    { 0, 1, 0, 1, 2, 3, 2, 3, 0, 2 },

    /* SIZE_2NxnD case */
    { 0, 2, 0, 1, 0, 1, 2, 3, 2, 3 },

    /* SIZE_nLx2N case */
    { 0, 1, 0, 1, 2, 0, 1, 0, 1, 2 },

    /* SIZE_nRx2N case */
    { 0, 2, 3, 2, 3, 0, 2, 3, 2, 3 }
};

/**
******************************************************************************
* @brief  Look up table for calculating the TU position in vertical
*         for all the TUs in a CU, if CU part mode is one of
*         SIZE_2Nx2N, SIZE_2NxN, SIZE_Nx2N
*
*         i ranging (0 to 3)
*         tu_posy[i](in pixels in cu) =
*                 ((cusize >> 2) * gau1_inter_tu_posy_scl_amt[i]);
******************************************************************************
*/
UWORD8 gau1_inter_tu_posy_scl_amt[4] = {
    /* SIZE_2Nx2N, SIZE_2NxN, SIZE_Nx2N cases */
    0,
    0,
    2,
    2
};

/**
******************************************************************************
* @brief  Look up table for calculating the TU position in vertical
*         for all the TUs in a CU, if CU part mode is one of
*         SIZE_2NxnU, SIZE_2NxnD, SIZE_nLx2N,SIZE_nRx2N (AMP motion partition cases)
*
*         part_mode = {SIZE_2NxnU,SIZE_2NxnD,SIZE_nLx2N,SIZE_nRx2N}
*         i ranging (0 to 9)
*         tu_posy[i](in pixels in cu) =
*              ((cusize >> 2) * gau1_inter_tu_posy_scl_amt_amp[part_mode-4][i]);
******************************************************************************
*/
UWORD8 gau1_inter_tu_posy_scl_amt_amp[4][10] = {
    /* SIZE_2NxnU case */
    { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2 },

    /* SIZE_2NxnD case */
    { 0, 0, 2, 2, 3, 3, 2, 2, 3, 3 },

    /* SIZE_nLx2N case */
    { 0, 0, 1, 1, 0, 2, 2, 3, 3, 2 },

    /* SIZE_nRx2N case */
    { 0, 0, 0, 1, 1, 2, 2, 2, 3, 3 }
};

/**
* @brief transform shift. Initialized in ihevce_enc_loop_init()
*/
WORD32 ga_trans_shift[5];

/**
* @brief chroma 422 intra angle mapping
*/
const UWORD8 gau1_chroma422_intra_angle_mapping[36] = {
    0,  1,  2,  2,  2,  2,  3,  5,  7,  8,  10, 12, 13, 15, 17, 18, 19, 20,
    21, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 29, 30, 31, DM_CHROMA_IDX
};

// clang-format off
/**
******************************************************************************
* @breif  LUT for returning the fractional bits(Q12) to encode a bin based on
*         probability state and the encoded bin (MPS / LPS). The fractional
*         bits are computed as -log2(probabililty of symbol)
*
*         Probabilites of the cabac states (0-63) are explained in section C
*         of ieee paper by Detlev Marpe et al (VOL. 13, NO. 7, JULY 2003)
*          alpha = (0.01875/0.5) ^ (1/63), p0 = 0.5 and p63 = 0.01875
*
*         Note that HEVC and AVC use the same cabac tables
*
* input   : curpState[bits7-1]  | (curMPS ^ encoded bin)[bit0]
*
* output  : fractionnal bits to encode the bin
*
******************************************************************************
*/
UWORD16 gau2_ihevce_cabac_bin_to_bits[64 * 2]   =
{
    /* bits for mps */          /* bits for lps */
    ROUND_Q12(1.000000000),     ROUND_Q12(1.000000000),
    ROUND_Q12(0.928535439),     ROUND_Q12(1.075189930),
    ROUND_Q12(0.863825936),     ROUND_Q12(1.150379860),
    ROUND_Q12(0.804976479),     ROUND_Q12(1.225569790),
    ROUND_Q12(0.751252392),     ROUND_Q12(1.300759720),
    ROUND_Q12(0.702043265),     ROUND_Q12(1.375949650),
    ROUND_Q12(0.656836490),     ROUND_Q12(1.451139580),
    ROUND_Q12(0.615197499),     ROUND_Q12(1.526329510),
    ROUND_Q12(0.576754745),     ROUND_Q12(1.601519441),
    ROUND_Q12(0.541188141),     ROUND_Q12(1.676709371),
    ROUND_Q12(0.508220033),     ROUND_Q12(1.751899301),
    ROUND_Q12(0.477608072),     ROUND_Q12(1.827089231),
    ROUND_Q12(0.449139524),     ROUND_Q12(1.902279161),
    ROUND_Q12(0.422626680),     ROUND_Q12(1.977469091),
    ROUND_Q12(0.397903130),     ROUND_Q12(2.052659021),
    ROUND_Q12(0.374820697),     ROUND_Q12(2.127848951),
    ROUND_Q12(0.353246914),     ROUND_Q12(2.203038881),
    ROUND_Q12(0.333062915),     ROUND_Q12(2.278228811),
    ROUND_Q12(0.314161674),     ROUND_Q12(2.353418741),
    ROUND_Q12(0.296446520),     ROUND_Q12(2.428608671),
    ROUND_Q12(0.279829872),     ROUND_Q12(2.503798601),
    ROUND_Q12(0.264232174),     ROUND_Q12(2.578988531),
    ROUND_Q12(0.249580966),     ROUND_Q12(2.654178461),
    ROUND_Q12(0.235810099),     ROUND_Q12(2.729368392),
    ROUND_Q12(0.222859049),     ROUND_Q12(2.804558322),
    ROUND_Q12(0.210672321),     ROUND_Q12(2.879748252),
    ROUND_Q12(0.199198934),     ROUND_Q12(2.954938182),
    ROUND_Q12(0.188391967),     ROUND_Q12(3.030128112),
    ROUND_Q12(0.178208162),     ROUND_Q12(3.105318042),
    ROUND_Q12(0.168607572),     ROUND_Q12(3.180507972),
    ROUND_Q12(0.159553254),     ROUND_Q12(3.255697902),
    ROUND_Q12(0.151010993),     ROUND_Q12(3.330887832),
    ROUND_Q12(0.142949058),     ROUND_Q12(3.406077762),
    ROUND_Q12(0.135337985),     ROUND_Q12(3.481267692),
    ROUND_Q12(0.128150381),     ROUND_Q12(3.556457622),
    ROUND_Q12(0.121360753),     ROUND_Q12(3.631647552),
    ROUND_Q12(0.114945349),     ROUND_Q12(3.706837482),
    ROUND_Q12(0.108882016),     ROUND_Q12(3.782027412),
    ROUND_Q12(0.103150076),     ROUND_Q12(3.857217343),
    ROUND_Q12(0.097730208),     ROUND_Q12(3.932407273),
    ROUND_Q12(0.092604344),     ROUND_Q12(4.007597203),
    ROUND_Q12(0.087755577),     ROUND_Q12(4.082787133),
    ROUND_Q12(0.083168071),     ROUND_Q12(4.157977063),
    ROUND_Q12(0.078826986),     ROUND_Q12(4.233166993),
    ROUND_Q12(0.074718402),     ROUND_Q12(4.308356923),
    ROUND_Q12(0.070829259),     ROUND_Q12(4.383546853),
    ROUND_Q12(0.067147292),     ROUND_Q12(4.458736783),
    ROUND_Q12(0.063660977),     ROUND_Q12(4.533926713),
    ROUND_Q12(0.060359483),     ROUND_Q12(4.609116643),
    ROUND_Q12(0.057232622),     ROUND_Q12(4.684306573),
    ROUND_Q12(0.054270808),     ROUND_Q12(4.759496503),
    ROUND_Q12(0.051465018),     ROUND_Q12(4.834686433),
    ROUND_Q12(0.048806753),     ROUND_Q12(4.909876363),
    ROUND_Q12(0.046288005),     ROUND_Q12(4.985066294),
    ROUND_Q12(0.043901228),     ROUND_Q12(5.060256224),
    ROUND_Q12(0.041639305),     ROUND_Q12(5.135446154),
    ROUND_Q12(0.039495525),     ROUND_Q12(5.210636084),
    ROUND_Q12(0.037463555),     ROUND_Q12(5.285826014),
    ROUND_Q12(0.035537418),     ROUND_Q12(5.361015944),
    ROUND_Q12(0.033711472),     ROUND_Q12(5.436205874),
    ROUND_Q12(0.031980387),     ROUND_Q12(5.511395804),
    ROUND_Q12(0.030339132),     ROUND_Q12(5.586585734),
    ROUND_Q12(0.028782950),     ROUND_Q12(5.661775664),
    ROUND_Q12(0.027307346),     ROUND_Q12(5.736965594)
};
// clang-format on

/**
* @brief <Fill Me>
*/
WORD32 gai4_subBlock2csbfId_map4x4TU[1];
WORD32 gai4_subBlock2csbfId_map8x8TU[4];
WORD32 gai4_subBlock2csbfId_map16x16TU[16];
WORD32 gai4_subBlock2csbfId_map32x32TU[64];

/**
* @brief the neighbor flags for a general ctb (ctb inside the frame; not any corners).
* The table gau4_nbr_flags_8x8_4x4blks generated for 16x16 4x4 blocks(ctb_size = 64).
* But the same table holds good for other 4x4 blocks 2d arrays(eg 8x8 4x4 blks,4x4 4x4blks).
* But the flags must be accessed with stride of 16 since the table has been generated for
* ctb_size = 64. For odd 4x4 2d arrays(eg 3x3 4x4 blks) the flags needs modification.
* The flags also need modification for corner ctbs.
*/
const UWORD32 gau4_nbr_flags_8x8_4x4blks[64] = {
    0x11188, 0x11180, 0x11188, 0x11180, 0x11188, 0x11180, 0x11188, 0x11180, 0x11188, 0x10180,
    0x11180, 0x10180, 0x11188, 0x10180, 0x11180, 0x10180, 0x11188, 0x11180, 0x11188, 0x10180,
    0x11188, 0x11180, 0x11188, 0x10180, 0x11188, 0x10180, 0x11180, 0x10180, 0x11180, 0x10180,
    0x11180, 0x10180, 0x11188, 0x11180, 0x11188, 0x11180, 0x11188, 0x11180, 0x11188, 0x10180,
    0x11188, 0x10180, 0x11180, 0x10180, 0x11188, 0x10180, 0x11180, 0x10180, 0x11188, 0x11180,
    0x11188, 0x10180, 0x11188, 0x11180, 0x11188, 0x10180, 0x11180, 0x10180, 0x11180, 0x10180,
    0x11180, 0x10180, 0x11180, 0x10180
};

const float gad_look_up_activity[TOT_QP_MOD_OFFSET] = { 0.314980262f, 0.353553391f, 0.396850263f,
                                                        0.445449359f, 0.5f,         0.561231024f,
                                                        0.629960525f, 0.707106781f, 0.793700526f,
                                                        0.890898718f, 1.0f,         1.122462048f,
                                                        1.25992105f,  1.414213562f };
