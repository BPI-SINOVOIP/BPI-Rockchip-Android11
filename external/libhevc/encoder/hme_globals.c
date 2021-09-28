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
* @file hme_globals.c
*
* @brief
*    Contains all the global definitions used by HME
*
* @author
*    Ittiam
*
*
* List of Functions
*
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
#include <limits.h>

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
#include "ihevce_entropy_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_global_tables.h"
#include "ihevce_dep_mngr_interface.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_globals.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_refine.h"
#include "hme_err_compute.h"
#include "hme_common_utils.h"
#include "hme_search_algo.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

/**
******************************************************************************
*  @brief Converts an encode order to raster order x coord. Meant for 16x16
*         CU within 64x64 or within 32x32
******************************************************************************
*/
U08 gau1_encode_to_raster_x[16] = { 0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3 };

/**
******************************************************************************
*  @brief Converts an encode order to raster order y coord. Meant for 16x16
*         CU within 64x64 or within 32x32
******************************************************************************
*/
U08 gau1_encode_to_raster_y[16] = { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3 };

/**
******************************************************************************
*  @brief Given a CU id within the bigger CU (0..3), and the partition type
*         currently within the small CU, we can figure out candidate
*         partition types for bigger CU. E.g. IF CU id is 0, and is AMP of
*         nLx2N, candidate partitions for bigger CU are nLx2N and 2Nx2N
******************************************************************************
*/
PART_TYPE_T ge_part_type_to_merge_part[4][MAX_PART_TYPES][3] = {
    /* CU 0: TL */
    {
        { PRT_2Nx2N, PRT_2NxN, PRT_Nx2N },  // small CU= 2Nx2N
        { PRT_2NxnU, PRT_INVALID, PRT_INVALID },  //small CU = 2NxN
        { PRT_nLx2N, PRT_INVALID, PRT_INVALID },  //small CU = Nx2N
        { PRT_2Nx2N, PRT_INVALID, PRT_INVALID },  //small CU = NxN
        { PRT_2Nx2N, PRT_2NxnU, PRT_INVALID },  //small CU = 2NxnU
        { PRT_2NxN, PRT_2NxnU, PRT_INVALID },  //small CU = 2NxnD
        { PRT_2Nx2N, PRT_nLx2N, PRT_INVALID },  //small CU = nLx2N
        { PRT_Nx2N, PRT_nLx2N, PRT_INVALID },  //small CU = nRx2N

    },
    /* CU 1: TR */
    {
        { PRT_2Nx2N, PRT_2NxN, PRT_Nx2N },  // small CU= 2Nx2N
        { PRT_2NxnU, PRT_INVALID, PRT_INVALID },  //small CU = 2NxN
        { PRT_nRx2N, PRT_INVALID, PRT_INVALID },  //small CU = Nx2N
        { PRT_2Nx2N, PRT_INVALID, PRT_INVALID },  //small CU = NxN
        { PRT_2Nx2N, PRT_2NxnU, PRT_INVALID },  //small CU = 2NxnU
        { PRT_2NxN, PRT_2NxnU, PRT_INVALID },  //small CU = 2NxnD
        { PRT_Nx2N, PRT_nRx2N, PRT_INVALID },  //small CU = nLx2N
        { PRT_2Nx2N, PRT_nRx2N, PRT_INVALID },  //small CU = nRx2N

    },
    /* CU 0: BL */
    {
        { PRT_2Nx2N, PRT_2NxN, PRT_Nx2N },  // small CU= 2Nx2N
        { PRT_2NxnD, PRT_INVALID, PRT_INVALID },  //small CU = 2NxN
        { PRT_nLx2N, PRT_INVALID, PRT_INVALID },  //small CU = Nx2N
        { PRT_2Nx2N, PRT_INVALID, PRT_INVALID },  //small CU = NxN
        { PRT_2NxN, PRT_2NxnD, PRT_INVALID },  //small CU = 2NxnU
        { PRT_2Nx2N, PRT_2NxnD, PRT_INVALID },  //small CU = 2NxnD
        { PRT_2Nx2N, PRT_nLx2N, PRT_INVALID },  //small CU = nLx2N
        { PRT_2NxN, PRT_nLx2N, PRT_INVALID },  //small CU = nRx2N

    },
    /* CU 0: BR */
    {
        { PRT_2Nx2N, PRT_2NxN, PRT_Nx2N },  // small CU= 2Nx2N
        { PRT_2NxnD, PRT_INVALID, PRT_INVALID },  //small CU = 2NxN
        { PRT_nRx2N, PRT_INVALID, PRT_INVALID },  //small CU = Nx2N
        { PRT_2Nx2N, PRT_INVALID, PRT_INVALID },  //small CU = NxN
        { PRT_2NxN, PRT_2NxnD, PRT_INVALID },  //small CU = 2NxnU
        { PRT_2Nx2N, PRT_2NxnD, PRT_INVALID },  //small CU = 2NxnD
        { PRT_Nx2N, PRT_nRx2N, PRT_INVALID },  //small CU = nLx2N
        { PRT_2Nx2N, PRT_nRx2N, PRT_INVALID },  //small CU = nRx2N

    }
};

/**
******************************************************************************
*  @brief A given partition type has 1,2 or 4 partitions, each corresponding
*  to a unique partition id PART_ID_T enum type. So, this global converts
*  partition type to a bitmask of corresponding partition ids.
******************************************************************************
*/
S32 gai4_part_type_to_part_mask[MAX_PART_TYPES] = {
    (ENABLE_2Nx2N), (ENABLE_2NxN),  (ENABLE_Nx2N),  (ENABLE_NxN),
    (ENABLE_2NxnU), (ENABLE_2NxnD), (ENABLE_nLx2N), (ENABLE_nRx2N),
};

/**
******************************************************************************
*  @brief Reads out the index of function pointer to a sad_compute function
*         of blk given a blk size enumeration
******************************************************************************
*/
U08 gau1_blk_size_to_fp[NUM_BLK_SIZES] = {
    0,  //BLK_4x4
    4,  //BLK_4x8
    28,  //BLK_8x4
    8,  //BLK_8x8
    4,  //BLK_4x16
    8,  //BLK_8x16
    12,  //BLK_12x16
    20,  //BLK_16x4
    16,  //BLK_16x8
    32,  //BLK_16x12
    16,  //BLK_16x16
    8,  //BLK_8x32
    16,  //BLK_16x32
    24,  //BLK_24x32
    16,  //BLK_32x8
    16,  //BLK_32x16
    16,  //BLK_32x24
    16,  //BLK_32x32
    16,  //BLK_16x64
    16,  //BLK_32x64
    16,  //BLK_48x64
    16,  //BLK_64x16
    16,  //BLK_64x32
    16,  //BLK_64x48
    16  //BLK_64x64
};

/**
******************************************************************************
*  @brief Reads out the width of blk given a blk size enumeration
******************************************************************************
*/
U08 gau1_blk_size_to_wd[NUM_BLK_SIZES] = {
    4,  //BLK_4x4
    4,  //BLK_4x8
    8,  //BLK_8x4
    8,  //BLK_8x8
    4,  //BLK_4x16
    8,  //BLK_8x16
    12,  //BLK_12x16
    16,  //BLK_16x4
    16,  //BLK_16x8
    16,  //BLK_16x12
    16,  //BLK_16x16
    8,  //BLK_8x32
    16,  //BLK_16x32
    24,  //BLK_24x32
    32,  //BLK_32x8
    32,  //BLK_32x16
    32,  //BLK_32x24
    32,  //BLK_32x32
    16,  //BLK_16x64
    32,  //BLK_32x64
    48,  //BLK_48x64
    64,  //BLK_64x16
    64,  //BLK_64x32
    64,  //BLK_64x48
    64,  //BLK_64x64
};

/**
******************************************************************************
*  @brief Reads out the shift to be done for blk given a blk size enumeration
******************************************************************************
*/
U08 gau1_blk_size_to_wd_shift[NUM_BLK_SIZES] = {
    3,  //BLK_4x4
    3,  //BLK_4x8
    4,  //BLK_8x4
    4,  //BLK_8x8
    3,  //BLK_4x16
    4,  //BLK_8x16
    12,  //BLK_12x16
    5,  //BLK_16x4
    5,  //BLK_16x8
    5,  //BLK_16x12
    5,  //BLK_16x16
    4,  //BLK_8x32
    5,  //BLK_16x32
    24,  //BLK_24x32
    6,  //BLK_32x8
    6,  //BLK_32x16
    6,  //BLK_32x24
    6,  //BLK_32x32
    5,  //BLK_16x64
    6,  //BLK_32x64
    48,  //BLK_48x64
    7,  //BLK_64x16
    7,  //BLK_64x32
    7,  //BLK_64x48
    7,  //BLK_64x64
};
/**
******************************************************************************
*  @brief Reads out the height of blk given a blk size enumeration
******************************************************************************
*/
U08 gau1_blk_size_to_ht[NUM_BLK_SIZES] = {
    4,  //BLK_4x4
    8,  //BLK_4x8
    4,  //BLK_8x4
    8,  //BLK_8x8
    16,  //BLK_4x16
    16,  //BLK_8x16
    16,  //BLK_12x16
    4,  //BLK_16x4
    8,  //BLK_16x8
    12,  //BLK_16x12
    16,  //BLK_16x16
    32,  //BLK_8x32
    32,  //BLK_16x32
    32,  //BLK_24x32
    8,  //BLK_32x8
    16,  //BLK_32x16
    24,  //BLK_32x24
    32,  //BLK_32x32
    64,  //BLK_16x64
    64,  //BLK_32x64
    64,  //BLK_48x64
    16,  //BLK_64x16
    32,  //BLK_64x32
    48,  //BLK_64x48
    64,  //BLK_64x64
};

/**
******************************************************************************
*  @brief Given a minimum pt enum in a 3x3 grid, reads out the list of active
*  search pts in next iteration as a bit-mask, eliminating need to search
*  pts that have already been searched in this iteration.
******************************************************************************
*/
S32 gai4_opt_grid_mask[NUM_GRID_PTS];

/**
******************************************************************************
*  @brief Given a minimum pt enum in a 3x3 grid, reads out the x offset of
*  the min pt relative to center assuming step size of 1
******************************************************************************
*/
S08 gai1_grid_id_to_x[NUM_GRID_PTS];

/**
******************************************************************************
*  @brief Given a minimum pt enum in a 3x3 grid, reads out the y offset of
*  the min pt relative to center assuming step size of 1
******************************************************************************
*/
S08 gai1_grid_id_to_y[NUM_GRID_PTS];

/**
******************************************************************************
*  @brief Lookup of the blk size enum, given a specific partition and cu size
******************************************************************************
*/
BLK_SIZE_T ge_part_id_to_blk_size[NUM_CU_SIZES][TOT_NUM_PARTS];

/**
******************************************************************************
*  @brief For a given partition split, find number of partitions
******************************************************************************
*/
U08 gau1_num_parts_in_part_type[MAX_PART_TYPES];

/**
******************************************************************************
*  @brief For a given partition split, returns the enumerations of specific
*  partitions in raster order. E.g. for PRT_2NxN, part id 0 is
*  PART_ID_2NxN_T and part id 1 is PART_ID_2NxN_B
******************************************************************************
*/
PART_ID_T ge_part_type_to_part_id[MAX_PART_TYPES][MAX_NUM_PARTS];

/**
******************************************************************************
*  @brief For a given partition id, returs the rectangular position and size
*  of partition within cu relative ot cu start.
******************************************************************************
*/
part_attr_t gas_part_attr_in_cu[TOT_NUM_PARTS];

/**
******************************************************************************
*  @brief Gives the CU type enumeration given a  blk size.
******************************************************************************
*/
CU_SIZE_T ge_blk_size_to_cu_size[NUM_BLK_SIZES];

/**
******************************************************************************
*  @brief Given a minimum pt enum in a diamond grid, reads out the list
*  of active search pts in next iteration as a bit-mask, eliminating need
*  to search pts that have already been searched in this iteration.
******************************************************************************
*/

S32 gai4_opt_grid_mask_diamond[5];

/**
******************************************************************************
*  @brief Given a minimum pt enum in a 9 point grid, reads out the list
*  of active search pts in next iteration as a bit-mask, eliminating need
*  to search pts that have already been searched in this iteration.
******************************************************************************
*/

S32 gai4_opt_grid_mask_conventional[9];

/**
******************************************************************************
*  @brief Returns 1 if there are qpel points to the top and bottom of the
*         current point
******************************************************************************
*/
S32 gai4_2pt_qpel_interpol_possible_vert[4][4] = {
    { 1, 0, 1, 0 }, { 1, 0, 1, 0 }, { 1, 0, 1, 0 }, { 1, 0, 1, 0 }
};

/**
******************************************************************************
*  @brief Returns 1 if there are qpel points to the left and right of the
*         current point
******************************************************************************
*/
S32 gai4_2pt_qpel_interpol_possible_horz[4][4] = {
    { 1, 1, 1, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 }, { 0, 0, 0, 0 }
};

S32 gai4_select_qpel_function_vert[4][16] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                                              { 3, 3, 3, 3, 1, 3, 1, 3, 3, 3, 3, 3, 1, 3, 1, 3 },
                                              { 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4, 2, 4, 2, 4 },
                                              { 5, 5, 5, 5, 7, 6, 7, 6, 5, 5, 5, 5, 7, 6, 7, 6 } };

S32 gai4_select_qpel_function_horz[4][16] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                                              { 3, 1, 3, 1, 3, 3, 3, 3, 3, 1, 3, 1, 3, 3, 3, 3 },
                                              { 4, 2, 4, 2, 4, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4 },
                                              { 5, 7, 5, 7, 5, 6, 5, 6, 5, 7, 5, 7, 5, 6, 5, 6 } };

U08 gau1_cu_id_raster_to_enc[4][4] = {
    { 0, 1, 4, 5 }, { 2, 3, 6, 7 }, { 8, 9, 12, 13 }, { 10, 11, 14, 15 }
};

/**
******************************************************************************
*  @brief Given a CU size, this array returns blk size enum
******************************************************************************
*/
BLK_SIZE_T ge_cu_size_to_blk_size[NUM_CU_SIZES];

/**
******************************************************************************
*  @brief Given a part type, returns whether the part type is vertically
*          oriented.
******************************************************************************
*/
U08 gau1_is_vert_part[MAX_PART_TYPES];

/**
******************************************************************************
*  @brief Given a partition, returns the number of best results to consider
*          for full pell refinement.
******************************************************************************
*/
U08 gau1_num_best_results_PQ[TOT_NUM_PARTS];
U08 gau1_num_best_results_HQ[TOT_NUM_PARTS];
U08 gau1_num_best_results_MS[TOT_NUM_PARTS];
U08 gau1_num_best_results_HS[TOT_NUM_PARTS];
U08 gau1_num_best_results_XS[TOT_NUM_PARTS];
U08 gau1_num_best_results_XS25[TOT_NUM_PARTS];

/**
******************************************************************************
*  @brief gau1_cu_tr_valid[y][x] returns the validity of a top rt candt for
*         CU with raster id x, y within CTB. Valid for 16x16 CUs and above
******************************************************************************
*/
U08 gau1_cu_tr_valid[4][4] = { { 1, 1, 1, 1 }, { 1, 0, 1, 0 }, { 1, 1, 1, 0 }, { 1, 0, 1, 0 } };
/**
******************************************************************************
*  @brief gau1_cu_tr_valid[y][x] returns the validity of a bot lt candt for
*         CU with raster id x, y within CTB. Valid for 16x16 CUs and above
******************************************************************************
*/
U08 gau1_cu_bl_valid[4][4] = { { 1, 0, 1, 0 }, { 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 0, 0, 0, 0 } };

/**
******************************************************************************
*  @brief Returns the validity of top rt candt for a given part id, will not
*          be valid if tr of a part pts to a non causal neighbour like 16x8B
******************************************************************************
*/
U08 gau1_partid_tr_valid[TOT_NUM_PARTS];
/**
******************************************************************************
*  @brief Returns the validity of bottom left cant for given part id, will
*  not be valid, if bl of a part pts to a non causal neighbour like 8x16R
******************************************************************************
*/
U08 gau1_partid_bl_valid[TOT_NUM_PARTS];

/**
******************************************************************************
*  @brief The number of partition id in the CU, e.g. PART_ID_16x8_B is 2nd
******************************************************************************
*/
U08 gau1_part_id_to_part_num[TOT_NUM_PARTS];

/**
******************************************************************************
*  @brief Returns partition type for a given partition id, e.g.
*  PART_ID_16x8_B returns PRT_TYPE_16x8
******************************************************************************
*/
PART_TYPE_T ge_part_id_to_part_type[TOT_NUM_PARTS];

/**
******************************************************************************
*  @brief given raster id x, y of 8x8 blk in 64x64 CTB, return the enc order
******************************************************************************
*/
U08 gau1_8x8_cu_id_raster_to_enc[8][8] = {
    { 0, 1, 4, 5, 16, 17, 20, 21 },     { 2, 3, 6, 7, 18, 19, 22, 23 },
    { 8, 9, 12, 13, 24, 25, 28, 29 },   { 10, 11, 14, 15, 26, 27, 30, 31 },
    { 32, 33, 36, 37, 48, 49, 52, 53 }, { 34, 35, 38, 39, 50, 51, 54, 55 },
    { 40, 41, 44, 45, 56, 57, 60, 61 }, { 42, 43, 46, 47, 58, 59, 62, 63 }
};

/**
******************************************************************************
*  @brief Return the bits for a given partition id which gets added to the
*  cost. Although the bits are for a given partition type, we add off the
*  bits per partition while computing mv cost. For example, if the bits for
*  2NxN part type is 3, we add 1.5 bits for 2NxN_T and 1.5 for 2NxN_B.
*  Hence this is stored in Q1 format
******************************************************************************
*/
U08 gau1_bits_for_part_id_q1[TOT_NUM_PARTS];

/**
*******************************************************************************
@brief number of bits per bin
*******************************************************************************
*/
#define HME_CABAC_BITS_PER_BIN 0.5

/**
*******************************************************************************
@brief bin count to bit count conversion
*******************************************************************************
*/
#define HME_CAB_BITS_PER_BIN_Q8 128
#define HME_GET_CAB_BITS(x) (U08(((x)*HME_CABAC_BITS_PER_BIN + 0.5)))
#define HME_GET_BITS_FROM_BINS(x) ((x * HME_CAB_BITS_PER_BIN_Q8) >> 8)

/**
******************************************************************************
*  @brief For a given partition split, num bits to encode the partition type
*         merge flags, mvp flags, split tu bits;
*         assuming one bin equal to 0.5 bit for now
******************************************************************************
*/
U08 gau1_num_bits_for_part_type[MAX_PART_TYPES] = {
    /* bits for part type + merge/mvp flags + split cu/tu */
    0,  //HME_GET_CAB_BITS(0),  /* PRT_2Nx2N */
    0,  //HME_GET_CAB_BITS(0),  /* PRT_2NxN  */
    0,  //HME_GET_CAB_BITS(0),  /* PRT_Nx2N  */
    0,  //HME_GET_CAB_BITS(0),  /* PRT_NxN   */
    0,  //HME_GET_CAB_BITS(0), /* PRT_2NxnU */
    0,  //HME_GET_CAB_BITS(0), /* PRT_2NxnD */
    0,  //HME_GET_CAB_BITS(0), /* PRT_nLx2N */
    0,  //HME_GET_CAB_BITS(0)  /* PRT_nRx2N */
};

/**
*******************************************************************************
* @brief Used exclusively in the Intrinsics version of the function
*       'hme_combine_4x4_sads_and_compute_cost_high_speed' instead
*       of calling get_range()
*******************************************************************************
*/
S16 gi2_mvy_range[MAX_MVY_SUPPORTED_IN_COARSE_LAYER + 1][8] = {
    { 1, 1, 1, 1, 1, 1, 1, 1 },  //0
    { 2, 2, 2, 2, 2, 2, 2, 2 },  //1
    { 4, 4, 4, 4, 4, 4, 4, 4 },  //2
    { 4, 4, 4, 4, 4, 4, 4, 4 },         { 6, 6, 6, 6, 6, 6, 6, 6 },  //4
    { 6, 6, 6, 6, 6, 6, 6, 6 },         { 6, 6, 6, 6, 6, 6, 6, 6 },
    { 6, 6, 6, 6, 6, 6, 6, 6 },         { 8, 8, 8, 8, 8, 8, 8, 8 },  //8
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 10, 10, 10, 10, 10, 10, 10, 10 },  //16
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 12, 12, 12, 12, 12, 12, 12, 12 },  //32
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 14, 14, 14, 14, 14, 14, 14, 14 }  //64
};

/**
*******************************************************************************
* @brief Used exclusively in the Intrinsics version of the function
*       'hme_combine_4x4_sads_and_compute_cost_high_speed' instead
*       of calling get_range()
*******************************************************************************
*/
S16 gi2_mvx_range[MAX_MVX_SUPPORTED_IN_COARSE_LAYER * 2 + 1][8] = {
    { 16, 14, 14, 14, 14, 14, 14, 14 },  //-128
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-124
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-120
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-116
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-112
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-108
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-104
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-100
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-96
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //-92
    { 14, 14, 14, 14, 14, 14, 14, 12 }, { 14, 14, 14, 14, 14, 14, 14, 12 },
    { 14, 14, 14, 14, 14, 14, 14, 12 },

    { 14, 14, 14, 14, 14, 14, 14, 12 },  //-88
    { 14, 14, 14, 14, 14, 14, 12, 12 }, { 14, 14, 14, 14, 14, 14, 12, 12 },
    { 14, 14, 14, 14, 14, 14, 12, 12 },

    { 14, 14, 14, 14, 14, 14, 12, 12 },  //-84
    { 14, 14, 14, 14, 14, 12, 12, 12 }, { 14, 14, 14, 14, 14, 12, 12, 12 },
    { 14, 14, 14, 14, 14, 12, 12, 12 },

    { 14, 14, 14, 14, 14, 12, 12, 12 },  //-80
    { 14, 14, 14, 14, 12, 12, 12, 12 }, { 14, 14, 14, 14, 12, 12, 12, 12 },
    { 14, 14, 14, 14, 12, 12, 12, 12 },

    { 14, 14, 14, 14, 12, 12, 12, 12 },  //-76
    { 14, 14, 14, 12, 12, 12, 12, 12 }, { 14, 14, 14, 12, 12, 12, 12, 12 },
    { 14, 14, 14, 12, 12, 12, 12, 12 },

    { 14, 14, 14, 12, 12, 12, 12, 12 },  //-72
    { 14, 14, 12, 12, 12, 12, 12, 12 }, { 14, 14, 12, 12, 12, 12, 12, 12 },
    { 14, 14, 12, 12, 12, 12, 12, 12 },

    { 14, 14, 12, 12, 12, 12, 12, 12 },  //-68
    { 14, 12, 12, 12, 12, 12, 12, 12 }, { 14, 12, 12, 12, 12, 12, 12, 12 },
    { 14, 12, 12, 12, 12, 12, 12, 12 },

    { 14, 12, 12, 12, 12, 12, 12, 12 },  //-64
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },  //-60

    { 12, 12, 12, 12, 12, 12, 12, 10 },  //-59
    { 12, 12, 12, 12, 12, 12, 12, 10 }, { 12, 12, 12, 12, 12, 12, 12, 10 },
    { 12, 12, 12, 12, 12, 12, 12, 10 },

    { 12, 12, 12, 12, 12, 12, 10, 10 },  //-55
    { 12, 12, 12, 12, 12, 12, 10, 10 }, { 12, 12, 12, 12, 12, 12, 10, 10 },
    { 12, 12, 12, 12, 12, 12, 10, 10 },

    { 12, 12, 12, 12, 12, 10, 10, 10 },  //-51
    { 12, 12, 12, 12, 12, 10, 10, 10 }, { 12, 12, 12, 12, 12, 10, 10, 10 },
    { 12, 12, 12, 12, 12, 10, 10, 10 },

    { 12, 12, 12, 12, 10, 10, 10, 10 },  //-47
    { 12, 12, 12, 12, 10, 10, 10, 10 }, { 12, 12, 12, 12, 10, 10, 10, 10 },
    { 12, 12, 12, 12, 10, 10, 10, 10 },

    { 12, 12, 12, 10, 10, 10, 10, 8 },  //-43
    { 12, 12, 12, 10, 10, 10, 10, 8 },  { 12, 12, 12, 10, 10, 10, 10, 8 },
    { 12, 12, 12, 10, 10, 10, 10, 8 },

    { 12, 12, 10, 10, 10, 10, 8, 8 },  //-39
    { 12, 12, 10, 10, 10, 10, 8, 8 },   { 12, 12, 10, 10, 10, 10, 8, 8 },
    { 12, 12, 10, 10, 10, 10, 8, 8 },

    { 12, 10, 10, 10, 10, 8, 8, 6 },  //-35
    { 12, 10, 10, 10, 10, 8, 8, 6 },    { 12, 10, 10, 10, 10, 8, 8, 6 },
    { 12, 10, 10, 10, 10, 8, 8, 6 },

    { 10, 10, 10, 10, 8, 8, 6, 4 },  //-31
    { 10, 10, 10, 10, 8, 8, 6, 4 },     { 10, 10, 10, 10, 8, 8, 6, 2 },
    { 10, 10, 10, 10, 8, 8, 6, 1 },

    { 10, 10, 10, 8, 8, 6, 4, 2 },  //-27
    { 10, 10, 10, 8, 8, 6, 4, 4 },      { 10, 10, 10, 8, 8, 6, 2, 4 },
    { 10, 10, 10, 8, 8, 6, 1, 6 },

    { 10, 10, 8, 8, 6, 4, 2, 6 },  //-23
    { 10, 10, 8, 8, 6, 4, 4, 6 },       { 10, 10, 8, 8, 6, 2, 4, 6 },
    { 10, 10, 8, 8, 6, 1, 6, 8 },  //8@7

    { 10, 8, 8, 6, 4, 2, 6, 8 },  //-19
    { 10, 8, 8, 6, 4, 4, 6, 8 },        { 10, 8, 8, 6, 2, 4, 6, 8 },
    { 10, 8, 8, 6, 1, 6, 8, 8 },  //12@7

    { 8, 8, 6, 4, 2, 6, 8, 8 },  //-15
    { 8, 8, 6, 4, 4, 6, 8, 8 },         { 8, 8, 6, 2, 4, 6, 8, 8 },
    { 8, 8, 6, 1, 6, 8, 8, 10 },  //16@7

    { 8, 6, 4, 2, 6, 8, 8, 10 },  //-11
    { 8, 6, 4, 4, 6, 8, 8, 10 },        { 8, 6, 2, 4, 6, 8, 8, 10 },
    { 8, 6, 1, 6, 8, 8, 10, 10 },  //20@7

    { 6, 4, 2, 6, 8, 8, 10, 10 },  //-7
    { 6, 4, 4, 6, 8, 8, 10, 10 },       { 6, 2, 4, 6, 8, 8, 10, 10 },
    { 6, 1, 6, 8, 8, 10, 10, 10 },  //24@7

    { 4, 2, 6, 8, 8, 10, 10, 10 },  //-3
    { 4, 4, 6, 8, 8, 10, 10, 10 },      { 2, 4, 6, 8, 8, 10, 10, 10 },
    { 1, 6, 8, 8, 10, 10, 10, 10 },  //28@7

    { 2, 6, 8, 8, 10, 10, 10, 10 },  //1
    { 4, 6, 8, 8, 10, 10, 10, 10 },     { 4, 6, 8, 8, 10, 10, 10, 10 },
    { 6, 8, 8, 10, 10, 10, 10, 12 },  //32@7

    { 6, 8, 8, 10, 10, 10, 10, 12 },  //5
    { 6, 8, 8, 10, 10, 10, 10, 12 },    { 6, 8, 8, 10, 10, 10, 10, 12 },
    { 8, 8, 10, 10, 10, 10, 12, 12 },  //36@7

    { 8, 8, 10, 10, 10, 10, 12, 12 },  //9
    { 8, 8, 10, 10, 10, 10, 12, 12 },   { 8, 8, 10, 10, 10, 10, 12, 12 },
    { 8, 10, 10, 10, 10, 12, 12, 12 },  //40@7

    { 8, 10, 10, 10, 10, 12, 12, 12 },  //13
    { 8, 10, 10, 10, 10, 12, 12, 12 },  { 8, 10, 10, 10, 10, 12, 12, 12 },
    { 10, 10, 10, 10, 12, 12, 12, 12 },  //44@7

    { 10, 10, 10, 10, 12, 12, 12, 12 },  //17
    { 10, 10, 10, 10, 12, 12, 12, 12 }, { 10, 10, 10, 10, 12, 12, 12, 12 },
    { 10, 10, 10, 12, 12, 12, 12, 12 },  //48@7

    { 10, 10, 10, 12, 12, 12, 12, 12 },  //21
    { 10, 10, 10, 12, 12, 12, 12, 12 }, { 10, 10, 10, 12, 12, 12, 12, 12 },
    { 10, 10, 12, 12, 12, 12, 12, 12 },  //52@7

    { 10, 10, 12, 12, 12, 12, 12, 12 },  //25
    { 10, 10, 12, 12, 12, 12, 12, 12 }, { 10, 10, 12, 12, 12, 12, 12, 12 },
    { 10, 12, 12, 12, 12, 12, 12, 12 },  //56@7

    { 10, 12, 12, 12, 12, 12, 12, 12 },  //29
    { 10, 12, 12, 12, 12, 12, 12, 12 }, { 10, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 },  //60@7

    { 12, 12, 12, 12, 12, 12, 12, 12 },  //33
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 14 },  //64@7

    { 12, 12, 12, 12, 12, 12, 12, 14 },  //37
    { 12, 12, 12, 12, 12, 12, 12, 14 }, { 12, 12, 12, 12, 12, 12, 12, 14 },
    { 12, 12, 12, 12, 12, 12, 14, 14 },  //64@6

    { 12, 12, 12, 12, 12, 12, 14, 14 },  //41
    { 12, 12, 12, 12, 12, 12, 14, 14 }, { 12, 12, 12, 12, 12, 12, 14, 14 },
    { 12, 12, 12, 12, 12, 14, 14, 14 },  //64@5

    { 12, 12, 12, 12, 12, 14, 14, 14 },  //45
    { 12, 12, 12, 12, 12, 14, 14, 14 }, { 12, 12, 12, 12, 12, 14, 14, 14 },
    { 12, 12, 12, 12, 14, 14, 14, 14 },  //64@4

    { 12, 12, 12, 12, 14, 14, 14, 14 },  //49
    { 12, 12, 12, 12, 14, 14, 14, 14 }, { 12, 12, 12, 12, 14, 14, 14, 14 },
    { 12, 12, 12, 14, 14, 14, 14, 14 },  //64@3

    { 12, 12, 12, 14, 14, 14, 14, 14 },  //53
    { 12, 12, 12, 14, 14, 14, 14, 14 }, { 12, 12, 12, 14, 14, 14, 14, 14 },
    { 12, 12, 14, 14, 14, 14, 14, 14 },  //64@2

    { 12, 12, 14, 14, 14, 14, 14, 14 },  //57
    { 12, 12, 14, 14, 14, 14, 14, 14 }, { 12, 12, 14, 14, 14, 14, 14, 14 },
    { 12, 14, 14, 14, 14, 14, 14, 14 },  //64@1

    { 12, 14, 14, 14, 14, 14, 14, 14 },  //61
    { 12, 14, 14, 14, 14, 14, 14, 14 }, { 12, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //92@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //65
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //96@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //69
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //100@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //73
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //104@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //77
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //108@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //81
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //112@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //85
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //116@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //89
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //120@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //93
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 },  //124@7

    { 14, 14, 14, 14, 14, 14, 14, 14 },  //97
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 16 },  //128@7

    { 14, 14, 14, 14, 14, 14, 14, 16 },  //101
    { 14, 14, 14, 14, 14, 14, 14, 16 }, { 14, 14, 14, 14, 14, 14, 14, 16 },
    { 14, 14, 14, 14, 14, 14, 16, 16 },  //132@7

    { 14, 14, 14, 14, 14, 14, 16, 16 },  //105
    { 14, 14, 14, 14, 14, 14, 16, 16 }, { 14, 14, 14, 14, 14, 14, 16, 16 },
    { 14, 14, 14, 14, 14, 16, 16, 16 },  //136@7

    { 14, 14, 14, 14, 14, 16, 16, 16 },  //109
    { 14, 14, 14, 14, 14, 16, 16, 16 }, { 14, 14, 14, 14, 14, 16, 16, 16 },
    { 14, 14, 14, 14, 16, 16, 16, 16 },  //140@7

    { 14, 14, 14, 14, 16, 16, 16, 16 },  //113
    { 14, 14, 14, 14, 16, 16, 16, 16 }, { 14, 14, 14, 14, 16, 16, 16, 16 },
    { 14, 14, 14, 16, 16, 16, 16, 16 },  //144@7

    { 14, 14, 14, 16, 16, 16, 16, 16 },  //117
    { 14, 14, 14, 16, 16, 16, 16, 16 }, { 14, 14, 14, 16, 16, 16, 16, 16 },
    { 14, 14, 16, 16, 16, 16, 16, 16 },  //148@7

    { 14, 14, 16, 16, 16, 16, 16, 16 },  //121
    { 14, 14, 16, 16, 16, 16, 16, 16 }, { 14, 14, 16, 16, 16, 16, 16, 16 },
    { 14, 16, 16, 16, 16, 16, 16, 16 },  //152@7

    { 14, 16, 16, 16, 16, 16, 16, 16 },  //125
    { 14, 16, 16, 16, 16, 16, 16, 16 }, { 14, 16, 16, 16, 16, 16, 16, 16 },
    { 16, 16, 16, 16, 16, 16, 16, 16 },  //156@7
};

/**
******************************************************************************
*  @brief Returns area of a partition in terms of number of pixels
*         assuming block size is 16x16
******************************************************************************
*/
S32 gai4_partition_area[TOT_NUM_PARTS] = {
    256,  //PART_ID_2Nx2N
    128,  //PART_ID_2NxN_T
    128,  //PART_ID_2NxN_B
    128,  //PART_ID_Nx2N_L
    128,  //PART_ID_Nx2N_R
    64,  //PART_ID_NxN_TL
    64,  //PART_ID_NxN_TR
    64,  //PART_ID_NxN_BL
    64,  //PART_ID_NxN_BR
    64,  //PART_ID_2NxnU_T
    192,  //PART_ID_2NxnU_B
    192,  //PART_ID_2NxnD_T
    64,  //PART_ID_2NxnD_B
    64,  //PART_ID_nLx2N_L
    192,  //PART_ID_nLx2N_R
    192,  //PART_ID_nRx2N_L
    64  //PART_ID_nRx2N_R
};

/* 2 - 1 list for PQ and HQ. The other list for all other presets */
const U08 gau1_search_cand_priority_in_l1_and_l2_me[2][NUM_SEARCH_CAND_TYPES] = {
    {
        17,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        1,  //SPATIAL_TOP0
        2,  //SPATIAL_TOP_RIGHT0
        3,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        UCHAR_MAX,  //PROJECTED_COLOC2
        UCHAR_MAX,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        UCHAR_MAX,  //PROJECTED_TOP0
        14,  //PROJECTED_TOP1
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT0
        15,  //PROJECTED_TOP_RIGHT1
        UCHAR_MAX,  //PROJECTED_TOP_LEFT0
        16,  //PROJECTED_TOP_LEFT1
        6,  //PROJECTED_RIGHT0
        10,  //PROJECTED_RIGHT1
        7,  //PROJECTED_BOTTOM0
        11,  //PROJECTED_BOTTOM1
        8,  //PROJECTED_BOTTOM_RIGHT0
        12,  //PROJECTED_BOTTOM_RIGHT1
        9,  //PROJECTED_BOTTOM_LEFT0
        13,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        10,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        1,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        UCHAR_MAX,  //PROJECTED_COLOC2
        UCHAR_MAX,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        11,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        12,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        13,  //PROJECTED_TOP_LEFT1
        6,  //PROJECTED_RIGHT0
        14,  //PROJECTED_RIGHT1
        7,  //PROJECTED_BOTTOM0
        15,  //PROJECTED_BOTTOM1
        8,  //PROJECTED_BOTTOM_RIGHT0
        16,  //PROJECTED_BOTTOM_RIGHT1
        9,  //PROJECTED_BOTTOM_LEFT0
        17,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    }
};

/* 12 cases are -  */
/* case  0 - P picture, num_refs=1, 4x4 in L1ME = 0 */
/* case  1 - P picture, num_refs=1, 4x4 in L1ME = 1 */
/* case  2 - P picture, num_refs=2, 4x4 in L1ME = 0 */
/* case  3 - P picture, num_refs=2, 4x4 in L1ME = 1 */
/* case  4 - P picture, num_refs=3, 4x4 in L1ME = 0 */
/* case  5 - P picture, num_refs=3, 4x4 in L1ME = 1 */
/* case  6 - P picture, num_refs=3, 4x4 in L1ME = 0 */
/* case  7 - P picture, num_refs=3, 4x4 in L1ME = 1 */
/* case  8 - B picture, num_refs=1, 4x4 in L1ME = 0 */
/* case  9 - B picture, num_refs=1, 4x4 in L1ME = 1 */
/* case 10 - B picture, num_refs=2, 4x4 in L1ME = 0 */
/* case 11 - B picture, num_refs=2, 4x4 in L1ME = 1 */
const U08 gau1_search_cand_priority_in_l0_me[12][NUM_SEARCH_CAND_TYPES] = {
    {
        10,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        2,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        UCHAR_MAX,  //PROJECTED_COLOC2
        UCHAR_MAX,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        1,  //PROJECTED_TOP0
        UCHAR_MAX,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        UCHAR_MAX,  //PROJECTED_TOP_LEFT1
        6,  //PROJECTED_RIGHT0
        UCHAR_MAX,  //PROJECTED_RIGHT1
        7,  //PROJECTED_BOTTOM0
        UCHAR_MAX,  //PROJECTED_BOTTOM1
        8,  //PROJECTED_BOTTOM_RIGHT0
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT1
        9,  //PROJECTED_BOTTOM_LEFT0
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        13,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        2,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        UCHAR_MAX,  //PROJECTED_COLOC2
        UCHAR_MAX,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        6,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        7,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        8,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        1,  //PROJECTED_TOP0
        UCHAR_MAX,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        UCHAR_MAX,  //PROJECTED_TOP_LEFT1
        9,  //PROJECTED_RIGHT0
        UCHAR_MAX,  //PROJECTED_RIGHT1
        10,  //PROJECTED_BOTTOM0
        UCHAR_MAX,  //PROJECTED_BOTTOM1
        11,  //PROJECTED_BOTTOM_RIGHT0
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT1
        12,  //PROJECTED_BOTTOM_LEFT0
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        20,  //ZERO_MV
        21,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        1,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        6,  //PROJECTED_COLOC2
        7,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        3,  //PROJECTED_TOP1
        8,  //PROJECTED_TOP_RIGHT0
        9,  //PROJECTED_TOP_RIGHT1
        10,  //PROJECTED_TOP_LEFT0
        11,  //PROJECTED_TOP_LEFT1
        12,  //PROJECTED_RIGHT0
        13,  //PROJECTED_RIGHT1
        14,  //PROJECTED_BOTTOM0
        15,  //PROJECTED_BOTTOM1
        16,  //PROJECTED_BOTTOM_RIGHT0
        17,  //PROJECTED_BOTTOM_RIGHT1
        18,  //PROJECTED_BOTTOM_LEFT0
        19,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        26,  //ZERO_MV
        27,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        1,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        6,  //PROJECTED_COLOC2
        7,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        12,  //PROJECTED_COLOC_TR0
        15,  //PROJECTED_COLOC_TR1
        13,  //PROJECTED_COLOC_BL0
        16,  //PROJECTED_COLOC_BL1
        14,  //PROJECTED_COLOC_BR0
        17,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        3,  //PROJECTED_TOP1
        8,  //PROJECTED_TOP_RIGHT0
        9,  //PROJECTED_TOP_RIGHT1
        10,  //PROJECTED_TOP_LEFT0
        11,  //PROJECTED_TOP_LEFT1
        18,  //PROJECTED_RIGHT0
        19,  //PROJECTED_RIGHT1
        20,  //PROJECTED_BOTTOM0
        21,  //PROJECTED_BOTTOM1
        22,  //PROJECTED_BOTTOM_RIGHT0
        23,  //PROJECTED_BOTTOM_RIGHT1
        24,  //PROJECTED_BOTTOM_LEFT0
        25,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        22,  //ZERO_MV
        23,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        1,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        6,  //PROJECTED_COLOC2
        7,  //PROJECTED_COLOC3
        8,  //PROJECTED_COLOC4
        9,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        3,  //PROJECTED_TOP1
        10,  //PROJECTED_TOP_RIGHT0
        11,  //PROJECTED_TOP_RIGHT1
        12,  //PROJECTED_TOP_LEFT0
        13,  //PROJECTED_TOP_LEFT1
        14,  //PROJECTED_RIGHT0
        15,  //PROJECTED_RIGHT1
        16,  //PROJECTED_BOTTOM0
        17,  //PROJECTED_BOTTOM1
        18,  //PROJECTED_BOTTOM_RIGHT0
        19,  //PROJECTED_BOTTOM_RIGHT1
        20,  //PROJECTED_BOTTOM_LEFT0
        21,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        24,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        25,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        26,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        27,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        28,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        29,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        30,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        28,  //ZERO_MV
        29,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        1,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        6,  //PROJECTED_COLOC2
        7,  //PROJECTED_COLOC3
        8,  //PROJECTED_COLOC4
        9,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        14,  //PROJECTED_COLOC_TR0
        17,  //PROJECTED_COLOC_TR1
        15,  //PROJECTED_COLOC_BL0
        18,  //PROJECTED_COLOC_BL1
        16,  //PROJECTED_COLOC_BR0
        19,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        3,  //PROJECTED_TOP1
        10,  //PROJECTED_TOP_RIGHT0
        11,  //PROJECTED_TOP_RIGHT1
        12,  //PROJECTED_TOP_LEFT0
        13,  //PROJECTED_TOP_LEFT1
        20,  //PROJECTED_RIGHT0
        21,  //PROJECTED_RIGHT1
        22,  //PROJECTED_BOTTOM0
        23,  //PROJECTED_BOTTOM1
        24,  //PROJECTED_BOTTOM_RIGHT0
        25,  //PROJECTED_BOTTOM_RIGHT1
        26,  //PROJECTED_BOTTOM_LEFT0
        27,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        30,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        31,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        32,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        33,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        34,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        35,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        36,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        24,  //ZERO_MV
        25,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        1,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        6,  //PROJECTED_COLOC2
        7,  //PROJECTED_COLOC3
        8,  //PROJECTED_COLOC4
        9,  //PROJECTED_COLOC5
        10,  //PROJECTED_COLOC6
        11,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        3,  //PROJECTED_TOP1
        12,  //PROJECTED_TOP_RIGHT0
        13,  //PROJECTED_TOP_RIGHT1
        14,  //PROJECTED_TOP_LEFT0
        15,  //PROJECTED_TOP_LEFT1
        16,  //PROJECTED_RIGHT0
        17,  //PROJECTED_RIGHT1
        18,  //PROJECTED_BOTTOM0
        19,  //PROJECTED_BOTTOM1
        20,  //PROJECTED_BOTTOM_RIGHT0
        21,  //PROJECTED_BOTTOM_RIGHT1
        22,  //PROJECTED_BOTTOM_LEFT0
        23,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        26,  //PROJECTED_TOP2
        33,  //PROJECTED_TOP3
        27,  //PROJECTED_TOP_RIGHT2
        34,  //PROJECTED_TOP_RIGHT3
        28,  //PROJECTED_TOP_LEFT2
        35,  //PROJECTED_TOP_LEFT3
        29,  //PROJECTED_RIGHT2
        36,  //PROJECTED_RIGHT3
        30,  //PROJECTED_BOTTOM2
        37,  //PROJECTED_BOTTOM3
        31,  //PROJECTED_BOTTOM_RIGHT2
        38,  //PROJECTED_BOTTOM_RIGHT3
        32,  //PROJECTED_BOTTOM_LEFT2
        39  //PROJECTED_BOTTOM_LEFT3
    },
    {
        30,  //ZERO_MV
        31,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        1,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        4,  //PROJECTED_COLOC0
        5,  //PROJECTED_COLOC1
        6,  //PROJECTED_COLOC2
        7,  //PROJECTED_COLOC3
        8,  //PROJECTED_COLOC4
        9,  //PROJECTED_COLOC5
        10,  //PROJECTED_COLOC6
        11,  //PROJECTED_COLOC7
        16,  //PROJECTED_COLOC_TR0
        19,  //PROJECTED_COLOC_TR1
        17,  //PROJECTED_COLOC_BL0
        20,  //PROJECTED_COLOC_BL1
        18,  //PROJECTED_COLOC_BR0
        21,  //PROJECTED_COLOC_BR1
        2,  //PROJECTED_TOP0
        3,  //PROJECTED_TOP1
        12,  //PROJECTED_TOP_RIGHT0
        13,  //PROJECTED_TOP_RIGHT1
        14,  //PROJECTED_TOP_LEFT0
        15,  //PROJECTED_TOP_LEFT1
        22,  //PROJECTED_RIGHT0
        23,  //PROJECTED_RIGHT1
        24,  //PROJECTED_BOTTOM0
        25,  //PROJECTED_BOTTOM1
        26,  //PROJECTED_BOTTOM_RIGHT0
        27,  //PROJECTED_BOTTOM_RIGHT1
        28,  //PROJECTED_BOTTOM_LEFT0
        29,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        32,  //PROJECTED_TOP2
        39,  //PROJECTED_TOP3
        33,  //PROJECTED_TOP_RIGHT2
        40,  //PROJECTED_TOP_RIGHT3
        34,  //PROJECTED_TOP_LEFT2
        41,  //PROJECTED_TOP_LEFT3
        35,  //PROJECTED_RIGHT2
        42,  //PROJECTED_RIGHT3
        36,  //PROJECTED_BOTTOM2
        43,  //PROJECTED_BOTTOM3
        37,  //PROJECTED_BOTTOM_RIGHT2
        44,  //PROJECTED_BOTTOM_RIGHT3
        38,  //PROJECTED_BOTTOM_LEFT2
        45  //PROJECTED_BOTTOM_LEFT3
    },
    {
        10,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        2,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        UCHAR_MAX,  //PROJECTED_COLOC2
        UCHAR_MAX,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        1,  //PROJECTED_TOP0
        11,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        12,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        13,  //PROJECTED_TOP_LEFT1
        6,  //PROJECTED_RIGHT0
        14,  //PROJECTED_RIGHT1
        7,  //PROJECTED_BOTTOM0
        15,  //PROJECTED_BOTTOM1
        8,  //PROJECTED_BOTTOM_RIGHT0
        16,  //PROJECTED_BOTTOM_RIGHT1
        9,  //PROJECTED_BOTTOM_LEFT0
        17,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        13,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        2,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        UCHAR_MAX,  //PROJECTED_COLOC2
        UCHAR_MAX,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        6,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        7,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        8,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        1,  //PROJECTED_TOP0
        14,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        15,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        16,  //PROJECTED_TOP_LEFT1
        9,  //PROJECTED_RIGHT0
        17,  //PROJECTED_RIGHT1
        10,  //PROJECTED_BOTTOM0
        18,  //PROJECTED_BOTTOM1
        11,  //PROJECTED_BOTTOM_RIGHT0
        19,  //PROJECTED_BOTTOM_RIGHT1
        12,  //PROJECTED_BOTTOM_LEFT0
        20,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        10,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        2,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        18,  //PROJECTED_COLOC2
        19,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        UCHAR_MAX,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        UCHAR_MAX,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        UCHAR_MAX,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        1,  //PROJECTED_TOP0
        11,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        12,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        13,  //PROJECTED_TOP_LEFT1
        6,  //PROJECTED_RIGHT0
        14,  //PROJECTED_RIGHT1
        7,  //PROJECTED_BOTTOM0
        15,  //PROJECTED_BOTTOM1
        8,  //PROJECTED_BOTTOM_RIGHT0
        16,  //PROJECTED_BOTTOM_RIGHT1
        9,  //PROJECTED_BOTTOM_LEFT0
        17,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    },
    {
        13,  //ZERO_MV
        UCHAR_MAX,  //ZERO_MV_ALTREF
        0,  //SPATIAL_LEFT0
        UCHAR_MAX,  //SPATIAL_TOP0
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT0
        UCHAR_MAX,  //SPATIAL_TOP_LEFT0
        UCHAR_MAX,  //SPATIAL_LEFT1
        UCHAR_MAX,  //SPATIAL_TOP1
        UCHAR_MAX,  //SPATIAL_TOP_RIGHT1
        UCHAR_MAX,  //SPATIAL_TOP_LEFT1
        2,  //PROJECTED_COLOC0
        3,  //PROJECTED_COLOC1
        21,  //PROJECTED_COLOC2
        22,  //PROJECTED_COLOC3
        UCHAR_MAX,  //PROJECTED_COLOC4
        UCHAR_MAX,  //PROJECTED_COLOC5
        UCHAR_MAX,  //PROJECTED_COLOC6
        UCHAR_MAX,  //PROJECTED_COLOC7
        6,  //PROJECTED_COLOC_TR0
        UCHAR_MAX,  //PROJECTED_COLOC_TR1
        7,  //PROJECTED_COLOC_BL0
        UCHAR_MAX,  //PROJECTED_COLOC_BL1
        8,  //PROJECTED_COLOC_BR0
        UCHAR_MAX,  //PROJECTED_COLOC_BR1
        1,  //PROJECTED_TOP0
        14,  //PROJECTED_TOP1
        4,  //PROJECTED_TOP_RIGHT0
        15,  //PROJECTED_TOP_RIGHT1
        5,  //PROJECTED_TOP_LEFT0
        16,  //PROJECTED_TOP_LEFT1
        9,  //PROJECTED_RIGHT0
        17,  //PROJECTED_RIGHT1
        10,  //PROJECTED_BOTTOM0
        18,  //PROJECTED_BOTTOM1
        11,  //PROJECTED_BOTTOM_RIGHT0
        19,  //PROJECTED_BOTTOM_RIGHT1
        12,  //PROJECTED_BOTTOM_LEFT0
        20,  //PROJECTED_BOTTOM_LEFT1
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV0
        UCHAR_MAX,  //COLOCATED_GLOBAL_MV1
        UCHAR_MAX,  //PROJECTED_TOP2
        UCHAR_MAX,  //PROJECTED_TOP3
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT2
        UCHAR_MAX,  //PROJECTED_TOP_RIGHT3
        UCHAR_MAX,  //PROJECTED_TOP_LEFT2
        UCHAR_MAX,  //PROJECTED_TOP_LEFT3
        UCHAR_MAX,  //PROJECTED_RIGHT2
        UCHAR_MAX,  //PROJECTED_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM2
        UCHAR_MAX,  //PROJECTED_BOTTOM3
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT2
        UCHAR_MAX,  //PROJECTED_BOTTOM_RIGHT3
        UCHAR_MAX,  //PROJECTED_BOTTOM_LEFT2
        UCHAR_MAX  //PROJECTED_BOTTOM_LEFT3
    }
};

const SEARCH_CANDIDATE_TYPE_T
    gae_search_cand_priority_to_search_cand_type_map_in_l0_me[12][NUM_SEARCH_CAND_TYPES] = {
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ PROJECTED_TOP0,
          /* 2*/ PROJECTED_COLOC0,
          /* 3*/ PROJECTED_COLOC1,
          /* 4*/ PROJECTED_TOP_RIGHT0,
          /* 5*/ PROJECTED_TOP_LEFT0,
          /* 6*/ PROJECTED_RIGHT0,
          /* 7*/ PROJECTED_BOTTOM0,
          /* 8*/ PROJECTED_BOTTOM_RIGHT0,
          /* 9*/ PROJECTED_BOTTOM_LEFT0,
          /*10*/ ZERO_MV,
          /*11*/ ILLUSORY_CANDIDATE,
          /*12*/ ILLUSORY_CANDIDATE,
          /*13*/ ILLUSORY_CANDIDATE,
          /*14*/ ILLUSORY_CANDIDATE,
          /*15*/ ILLUSORY_CANDIDATE,
          /*16*/ ILLUSORY_CANDIDATE,
          /*17*/ ILLUSORY_CANDIDATE,
          /*18*/ ILLUSORY_CANDIDATE,
          /*19*/ ILLUSORY_CANDIDATE,
          /*20*/ ILLUSORY_CANDIDATE,
          /*21*/ ILLUSORY_CANDIDATE,
          /*22*/ ILLUSORY_CANDIDATE,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ PROJECTED_TOP0,
          /* 2*/ PROJECTED_COLOC0,
          /* 3*/ PROJECTED_COLOC1,
          /* 4*/ PROJECTED_TOP_RIGHT0,
          /* 5*/ PROJECTED_TOP_LEFT0,
          /* 6*/ PROJECTED_COLOC_TR0,
          /* 7*/ PROJECTED_COLOC_BL0,
          /* 8*/ PROJECTED_COLOC_BR0,
          /* 9*/ PROJECTED_RIGHT0,
          /*10*/ PROJECTED_BOTTOM0,
          /*11*/ PROJECTED_BOTTOM_RIGHT0,
          /*12*/ PROJECTED_BOTTOM_LEFT0,
          /*13*/ ZERO_MV,
          /*14*/ ILLUSORY_CANDIDATE,
          /*15*/ ILLUSORY_CANDIDATE,
          /*16*/ ILLUSORY_CANDIDATE,
          /*17*/ ILLUSORY_CANDIDATE,
          /*18*/ ILLUSORY_CANDIDATE,
          /*19*/ ILLUSORY_CANDIDATE,
          /*20*/ ILLUSORY_CANDIDATE,
          /*21*/ ILLUSORY_CANDIDATE,
          /*22*/ ILLUSORY_CANDIDATE,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ SPATIAL_LEFT1,
          /* 2*/ PROJECTED_TOP0,
          /* 3*/ PROJECTED_TOP1,
          /* 4*/ PROJECTED_COLOC0,
          /* 5*/ PROJECTED_COLOC1,
          /* 6*/ PROJECTED_COLOC2,
          /* 7*/ PROJECTED_COLOC3,
          /* 8*/ PROJECTED_TOP_RIGHT0,
          /* 9*/ PROJECTED_TOP_RIGHT1,
          /*10*/ PROJECTED_TOP_LEFT0,
          /*11*/ PROJECTED_TOP_LEFT1,
          /*12*/ PROJECTED_RIGHT0,
          /*13*/ PROJECTED_RIGHT1,
          /*14*/ PROJECTED_BOTTOM0,
          /*15*/ PROJECTED_BOTTOM1,
          /*16*/ PROJECTED_BOTTOM_RIGHT0,
          /*17*/ PROJECTED_BOTTOM_RIGHT1,
          /*18*/ PROJECTED_BOTTOM_LEFT0,
          /*19*/ PROJECTED_BOTTOM_LEFT1,
          /*20*/ ZERO_MV,
          /*21*/ ZERO_MV_ALTREF,
          /*22*/ ILLUSORY_CANDIDATE,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ SPATIAL_LEFT1,
          /* 2*/ PROJECTED_TOP0,
          /* 3*/ PROJECTED_TOP1,
          /* 4*/ PROJECTED_COLOC0,
          /* 5*/ PROJECTED_COLOC1,
          /* 6*/ PROJECTED_COLOC2,
          /* 7*/ PROJECTED_COLOC3,
          /* 8*/ PROJECTED_TOP_RIGHT0,
          /* 9*/ PROJECTED_TOP_RIGHT1,
          /*10*/ PROJECTED_TOP_LEFT0,
          /*11*/ PROJECTED_TOP_LEFT1,
          /*12*/ PROJECTED_COLOC_TR0,
          /*13*/ PROJECTED_COLOC_BL0,
          /*14*/ PROJECTED_COLOC_BR0,
          /*15*/ PROJECTED_COLOC_TR1,
          /*16*/ PROJECTED_COLOC_BL1,
          /*17*/ PROJECTED_COLOC_BR1,
          /*18*/ PROJECTED_RIGHT0,
          /*19*/ PROJECTED_RIGHT1,
          /*20*/ PROJECTED_BOTTOM0,
          /*21*/ PROJECTED_BOTTOM1,
          /*22*/ PROJECTED_BOTTOM_RIGHT0,
          /*23*/ PROJECTED_BOTTOM_RIGHT1,
          /*24*/ PROJECTED_BOTTOM_LEFT0,
          /*25*/ PROJECTED_BOTTOM_LEFT1,
          /*26*/ ZERO_MV,
          /*27*/ ZERO_MV_ALTREF,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ SPATIAL_LEFT1,
          /* 2*/ PROJECTED_TOP0,
          /* 3*/ PROJECTED_TOP1,
          /* 4*/ PROJECTED_COLOC0,
          /* 5*/ PROJECTED_COLOC1,
          /* 6*/ PROJECTED_COLOC2,
          /* 7*/ PROJECTED_COLOC3,
          /* 8*/ PROJECTED_COLOC4,
          /* 9*/ PROJECTED_COLOC5,
          /*10*/ PROJECTED_TOP_RIGHT0,
          /*11*/ PROJECTED_TOP_RIGHT1,
          /*12*/ PROJECTED_TOP_LEFT0,
          /*13*/ PROJECTED_TOP_LEFT1,
          /*14*/ PROJECTED_RIGHT0,
          /*15*/ PROJECTED_RIGHT1,
          /*16*/ PROJECTED_BOTTOM0,
          /*17*/ PROJECTED_BOTTOM1,
          /*18*/ PROJECTED_BOTTOM_RIGHT0,
          /*19*/ PROJECTED_BOTTOM_RIGHT1,
          /*20*/ PROJECTED_BOTTOM_LEFT0,
          /*21*/ PROJECTED_BOTTOM_LEFT1,
          /*22*/ ZERO_MV,
          /*23*/ ZERO_MV_ALTREF,
          /*24*/ PROJECTED_TOP2,
          /*25*/ PROJECTED_TOP_RIGHT2,
          /*26*/ PROJECTED_TOP_LEFT2,
          /*27*/ PROJECTED_RIGHT2,
          /*28*/ PROJECTED_BOTTOM2,
          /*29*/ PROJECTED_BOTTOM_RIGHT2,
          /*30*/ PROJECTED_BOTTOM_LEFT2,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ SPATIAL_LEFT1,
          /* 2*/ PROJECTED_TOP0,
          /* 3*/ PROJECTED_TOP1,
          /* 4*/ PROJECTED_COLOC0,
          /* 5*/ PROJECTED_COLOC1,
          /* 6*/ PROJECTED_COLOC2,
          /* 7*/ PROJECTED_COLOC3,
          /* 8*/ PROJECTED_COLOC4,
          /* 9*/ PROJECTED_COLOC5,
          /*10*/ PROJECTED_TOP_RIGHT0,
          /*11*/ PROJECTED_TOP_RIGHT1,
          /*12*/ PROJECTED_TOP_LEFT0,
          /*13*/ PROJECTED_TOP_LEFT1,
          /*14*/ PROJECTED_COLOC_TR0,
          /*15*/ PROJECTED_COLOC_BL0,
          /*16*/ PROJECTED_COLOC_BR0,
          /*17*/ PROJECTED_COLOC_TR1,
          /*18*/ PROJECTED_COLOC_BL1,
          /*19*/ PROJECTED_COLOC_BR1,
          /*20*/ PROJECTED_RIGHT0,
          /*21*/ PROJECTED_RIGHT1,
          /*22*/ PROJECTED_BOTTOM0,
          /*23*/ PROJECTED_BOTTOM1,
          /*24*/ PROJECTED_BOTTOM_RIGHT0,
          /*25*/ PROJECTED_BOTTOM_RIGHT1,
          /*26*/ PROJECTED_BOTTOM_LEFT0,
          /*27*/ PROJECTED_BOTTOM_LEFT1,
          /*28*/ ZERO_MV,
          /*29*/ ZERO_MV_ALTREF,
          /*30*/ PROJECTED_TOP2,
          /*31*/ PROJECTED_TOP_RIGHT2,
          /*32*/ PROJECTED_TOP_LEFT2,
          /*33*/ PROJECTED_RIGHT2,
          /*34*/ PROJECTED_BOTTOM2,
          /*35*/ PROJECTED_BOTTOM_RIGHT2,
          /*36*/ PROJECTED_BOTTOM_LEFT2,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ SPATIAL_LEFT1,
          /* 2*/ PROJECTED_TOP0,
          /* 3*/ PROJECTED_TOP1,
          /* 4*/ PROJECTED_COLOC0,
          /* 5*/ PROJECTED_COLOC1,
          /* 6*/ PROJECTED_COLOC2,
          /* 7*/ PROJECTED_COLOC3,
          /* 8*/ PROJECTED_COLOC4,
          /* 9*/ PROJECTED_COLOC5,
          /*10*/ PROJECTED_COLOC6,
          /*11*/ PROJECTED_COLOC7,
          /*12*/ PROJECTED_TOP_RIGHT0,
          /*13*/ PROJECTED_TOP_RIGHT1,
          /*14*/ PROJECTED_TOP_LEFT0,
          /*15*/ PROJECTED_TOP_LEFT1,
          /*16*/ PROJECTED_RIGHT0,
          /*17*/ PROJECTED_RIGHT1,
          /*18*/ PROJECTED_BOTTOM0,
          /*19*/ PROJECTED_BOTTOM1,
          /*20*/ PROJECTED_BOTTOM_RIGHT0,
          /*21*/ PROJECTED_BOTTOM_RIGHT1,
          /*22*/ PROJECTED_BOTTOM_LEFT0,
          /*23*/ PROJECTED_BOTTOM_LEFT1,
          /*24*/ ZERO_MV,
          /*25*/ ZERO_MV_ALTREF,
          /*26*/ PROJECTED_TOP2,
          /*27*/ PROJECTED_TOP_RIGHT2,
          /*28*/ PROJECTED_TOP_LEFT2,
          /*29*/ PROJECTED_RIGHT2,
          /*30*/ PROJECTED_BOTTOM2,
          /*31*/ PROJECTED_BOTTOM_RIGHT2,
          /*32*/ PROJECTED_BOTTOM_LEFT2,
          /*33*/ PROJECTED_TOP3,
          /*34*/ PROJECTED_TOP_RIGHT3,
          /*35*/ PROJECTED_TOP_LEFT3,
          /*36*/ PROJECTED_RIGHT3,
          /*37*/ PROJECTED_BOTTOM3,
          /*38*/ PROJECTED_BOTTOM_RIGHT3,
          /*39*/ PROJECTED_BOTTOM_LEFT3,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ SPATIAL_LEFT1,
          /* 2*/ PROJECTED_TOP0,
          /* 3*/ PROJECTED_TOP1,
          /* 4*/ PROJECTED_COLOC0,
          /* 5*/ PROJECTED_COLOC1,
          /* 6*/ PROJECTED_COLOC2,
          /* 7*/ PROJECTED_COLOC3,
          /* 8*/ PROJECTED_COLOC4,
          /* 9*/ PROJECTED_COLOC5,
          /*10*/ PROJECTED_COLOC6,
          /*11*/ PROJECTED_COLOC7,
          /*12*/ PROJECTED_TOP_RIGHT0,
          /*13*/ PROJECTED_TOP_RIGHT1,
          /*14*/ PROJECTED_TOP_LEFT0,
          /*15*/ PROJECTED_TOP_LEFT1,
          /*16*/ PROJECTED_COLOC_TR0,
          /*17*/ PROJECTED_COLOC_TR1,
          /*18*/ PROJECTED_COLOC_BL0,
          /*19*/ PROJECTED_COLOC_BL1,
          /*20*/ PROJECTED_COLOC_BR0,
          /*21*/ PROJECTED_COLOC_BR1,
          /*22*/ PROJECTED_RIGHT0,
          /*23*/ PROJECTED_RIGHT1,
          /*24*/ PROJECTED_BOTTOM0,
          /*25*/ PROJECTED_BOTTOM1,
          /*26*/ PROJECTED_BOTTOM_RIGHT0,
          /*27*/ PROJECTED_BOTTOM_RIGHT1,
          /*28*/ PROJECTED_BOTTOM_LEFT0,
          /*29*/ PROJECTED_BOTTOM_LEFT1,
          /*30*/ ZERO_MV,
          /*31*/ ZERO_MV_ALTREF,
          /*32*/ PROJECTED_TOP2,
          /*33*/ PROJECTED_TOP_RIGHT2,
          /*34*/ PROJECTED_TOP_LEFT2,
          /*35*/ PROJECTED_RIGHT2,
          /*36*/ PROJECTED_BOTTOM2,
          /*37*/ PROJECTED_BOTTOM_RIGHT2,
          /*38*/ PROJECTED_BOTTOM_LEFT2,
          /*39*/ PROJECTED_TOP3,
          /*40*/ PROJECTED_TOP_RIGHT3,
          /*41*/ PROJECTED_TOP_LEFT3,
          /*42*/ PROJECTED_RIGHT3,
          /*43*/ PROJECTED_BOTTOM3,
          /*44*/ PROJECTED_BOTTOM_RIGHT3,
          /*45*/ PROJECTED_BOTTOM_LEFT3,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ PROJECTED_TOP0,
          /* 2*/ PROJECTED_COLOC0,
          /* 3*/ PROJECTED_COLOC1,
          /* 4*/ PROJECTED_TOP_RIGHT0,
          /* 5*/ PROJECTED_TOP_LEFT0,
          /* 6*/ PROJECTED_RIGHT0,
          /* 7*/ PROJECTED_BOTTOM0,
          /* 8*/ PROJECTED_BOTTOM_RIGHT0,
          /* 9*/ PROJECTED_BOTTOM_LEFT0,
          /*10*/ ZERO_MV,
          /*11*/ PROJECTED_TOP1,
          /*12*/ PROJECTED_TOP_RIGHT1,
          /*13*/ PROJECTED_TOP_LEFT1,
          /*14*/ PROJECTED_RIGHT1,
          /*15*/ PROJECTED_BOTTOM1,
          /*16*/ PROJECTED_BOTTOM_RIGHT1,
          /*17*/ PROJECTED_BOTTOM_LEFT1,
          /*18*/ ILLUSORY_CANDIDATE,
          /*19*/ ILLUSORY_CANDIDATE,
          /*20*/ ILLUSORY_CANDIDATE,
          /*21*/ ILLUSORY_CANDIDATE,
          /*22*/ ILLUSORY_CANDIDATE,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ PROJECTED_TOP0,
          /* 2*/ PROJECTED_COLOC0,
          /* 3*/ PROJECTED_COLOC1,
          /* 4*/ PROJECTED_TOP_RIGHT0,
          /* 5*/ PROJECTED_TOP_LEFT0,
          /* 6*/ PROJECTED_COLOC_TR0,
          /* 7*/ PROJECTED_COLOC_BL0,
          /* 8*/ PROJECTED_COLOC_BR0,
          /* 9*/ PROJECTED_RIGHT0,
          /*10*/ PROJECTED_BOTTOM0,
          /*11*/ PROJECTED_BOTTOM_RIGHT0,
          /*12*/ PROJECTED_BOTTOM_LEFT0,
          /*13*/ ZERO_MV,
          /*14*/ PROJECTED_TOP1,
          /*15*/ PROJECTED_TOP_RIGHT1,
          /*16*/ PROJECTED_TOP_LEFT1,
          /*17*/ PROJECTED_RIGHT1,
          /*18*/ PROJECTED_BOTTOM1,
          /*19*/ PROJECTED_BOTTOM_RIGHT1,
          /*20*/ PROJECTED_BOTTOM_LEFT1,
          /*21*/ ILLUSORY_CANDIDATE,
          /*22*/ ILLUSORY_CANDIDATE,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ PROJECTED_TOP0,
          /* 2*/ PROJECTED_COLOC0,
          /* 3*/ PROJECTED_COLOC1,
          /* 4*/ PROJECTED_TOP_RIGHT0,
          /* 5*/ PROJECTED_TOP_LEFT0,
          /* 6*/ PROJECTED_RIGHT0,
          /* 7*/ PROJECTED_BOTTOM0,
          /* 8*/ PROJECTED_BOTTOM_RIGHT0,
          /* 9*/ PROJECTED_BOTTOM_LEFT0,
          /*10*/ ZERO_MV,
          /*11*/ PROJECTED_TOP1,
          /*12*/ PROJECTED_TOP_RIGHT1,
          /*13*/ PROJECTED_TOP_LEFT1,
          /*14*/ PROJECTED_RIGHT1,
          /*15*/ PROJECTED_BOTTOM1,
          /*16*/ PROJECTED_BOTTOM_RIGHT1,
          /*17*/ PROJECTED_BOTTOM_LEFT1,
          /*18*/ PROJECTED_COLOC2,
          /*19*/ PROJECTED_COLOC3,
          /*20*/ ILLUSORY_CANDIDATE,
          /*21*/ ILLUSORY_CANDIDATE,
          /*22*/ ILLUSORY_CANDIDATE,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE },
        { /* 0*/ SPATIAL_LEFT0,
          /* 1*/ PROJECTED_TOP0,
          /* 2*/ PROJECTED_COLOC0,
          /* 3*/ PROJECTED_COLOC1,
          /* 4*/ PROJECTED_TOP_RIGHT0,
          /* 5*/ PROJECTED_TOP_LEFT0,
          /* 6*/ PROJECTED_COLOC_TR0,
          /* 7*/ PROJECTED_COLOC_BL0,
          /* 8*/ PROJECTED_COLOC_BR0,
          /* 9*/ PROJECTED_RIGHT0,
          /*10*/ PROJECTED_BOTTOM0,
          /*11*/ PROJECTED_BOTTOM_RIGHT0,
          /*12*/ PROJECTED_BOTTOM_LEFT0,
          /*13*/ ZERO_MV,
          /*14*/ PROJECTED_TOP1,
          /*15*/ PROJECTED_TOP_RIGHT1,
          /*16*/ PROJECTED_TOP_LEFT1,
          /*17*/ PROJECTED_RIGHT1,
          /*18*/ PROJECTED_BOTTOM1,
          /*19*/ PROJECTED_BOTTOM_RIGHT1,
          /*20*/ PROJECTED_BOTTOM_LEFT1,
          /*21*/ PROJECTED_COLOC2,
          /*22*/ PROJECTED_COLOC3,
          /*23*/ ILLUSORY_CANDIDATE,
          /*24*/ ILLUSORY_CANDIDATE,
          /*25*/ ILLUSORY_CANDIDATE,
          /*26*/ ILLUSORY_CANDIDATE,
          /*27*/ ILLUSORY_CANDIDATE,
          /*28*/ ILLUSORY_CANDIDATE,
          /*29*/ ILLUSORY_CANDIDATE,
          /*30*/ ILLUSORY_CANDIDATE,
          /*31*/ ILLUSORY_CANDIDATE,
          /*32*/ ILLUSORY_CANDIDATE,
          /*33*/ ILLUSORY_CANDIDATE,
          /*34*/ ILLUSORY_CANDIDATE,
          /*35*/ ILLUSORY_CANDIDATE,
          /*36*/ ILLUSORY_CANDIDATE,
          /*37*/ ILLUSORY_CANDIDATE,
          /*38*/ ILLUSORY_CANDIDATE,
          /*39*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*41*/ ILLUSORY_CANDIDATE,
          /*42*/ ILLUSORY_CANDIDATE,
          /*43*/ ILLUSORY_CANDIDATE,
          /*44*/ ILLUSORY_CANDIDATE,
          /*45*/ ILLUSORY_CANDIDATE,
          /*46*/ ILLUSORY_CANDIDATE,
          /*47*/ ILLUSORY_CANDIDATE,
          /*48*/ ILLUSORY_CANDIDATE,
          /*49*/ ILLUSORY_CANDIDATE,
          /*40*/ ILLUSORY_CANDIDATE,
          /*51*/ ILLUSORY_CANDIDATE,
          /*52*/ ILLUSORY_CANDIDATE,
          /*53*/ ILLUSORY_CANDIDATE }
    };

const U08 gau1_max_num_search_cands_in_l0_me[12] = {
    11, 14, 22, 28, 31, 37, 40, 46, 18, 21, 20, 23
};

const SEARCH_CAND_LOCATIONS_T gae_search_cand_type_to_location_map[NUM_SEARCH_CAND_TYPES] = {
    ILLUSORY_LOCATION,  //ZERO_MV
    ILLUSORY_LOCATION,  //ZERO_MV_ALTREF
    LEFT,  //SPATIAL_LEFT0
    TOP,  //SPATIAL_TOP0
    TOPRIGHT,  //SPATIAL_TOP_RIGHT0
    TOPLEFT,  //SPATIAL_TOP_LEFT0
    LEFT,  //SPATIAL_LEFT1
    TOP,  //SPATIAL_TOP1
    TOPRIGHT,  //SPATIAL_TOP_RIGHT1
    TOPLEFT,  //SPATIAL_TOP_LEFT1
    COLOCATED,  //PROJECTED_COLOC0
    COLOCATED,  //PROJECTED_COLOC1
    COLOCATED,  //PROJECTED_COLOC2
    COLOCATED,  //PROJECTED_COLOC3
    COLOCATED,  //PROJECTED_COLOC4
    COLOCATED,  //PROJECTED_COLOC5
    COLOCATED,  //PROJECTED_COLOC6
    COLOCATED,  //PROJECTED_COLOC7
    COLOCATED_4x4_TR,  //PROJECTED_COLOC_TR0
    COLOCATED_4x4_TR,  //PROJECTED_COLOC_TR1
    COLOCATED_4x4_BL,  //PROJECTED_COLOC_BL0
    COLOCATED_4x4_BL,  //PROJECTED_COLOC_BL1
    COLOCATED_4x4_BR,  //PROJECTED_COLOC_BR0
    COLOCATED_4x4_BR,  //PROJECTED_COLOC_BR1
    TOP,  //PROJECTED_TOP0
    TOP,  //PROJECTED_TOP1
    TOPRIGHT,  //PROJECTED_TOP_RIGHT0
    TOPRIGHT,  //PROJECTED_TOP_RIGHT1
    TOPLEFT,  //PROJECTED_TOP_LEFT0
    TOPLEFT,  //PROJECTED_TOP_LEFT1
    RIGHT,  //PROJECTED_RIGHT0
    RIGHT,  //PROJECTED_RIGHT1
    BOTTOM,  //PROJECTED_BOTTOM0
    BOTTOM,  //PROJECTED_BOTTOM1
    BOTTOMRIGHT,  //PROJECTED_BOTTOM_RIGHT0
    BOTTOMRIGHT,  //PROJECTED_BOTTOM_RIGHT1
    BOTTOMLEFT,  //PROJECTED_BOTTOM_LEFT0
    BOTTOMLEFT,  //PROJECTED_BOTTOM_LEFT1
    ILLUSORY_LOCATION,  //COLOCATED_GLOBAL_MV0
    ILLUSORY_LOCATION,  //COLOCATED_GLOBAL_MV1
    TOP,  //PROJECTED_TOP2
    TOP,  //PROJECTED_TOP3
    TOPRIGHT,  //PROJECTED_TOP_RIGHT2
    TOPRIGHT,  //PROJECTED_TOP_RIGHT3
    TOPLEFT,  //PROJECTED_TOP_LEFT2
    TOPLEFT,  //PROJECTED_TOP_LEFT3
    RIGHT,  //PROJECTED_RIGHT2
    RIGHT,  //PROJECTED_RIGHT3
    BOTTOM,  //PROJECTED_BOTTOM2
    BOTTOM,  //PROJECTED_BOTTOM3
    BOTTOMRIGHT,  //PROJECTED_BOTTOM_RIGHT2
    BOTTOMRIGHT,  //PROJECTED_BOTTOM_RIGHT3
    BOTTOMLEFT,  //PROJECTED_BOTTOM_LEFT2
    BOTTOMLEFT  //PROJECTED_BOTTOM_LEFT3
};

/*  0 => projected; 1 => spatial; 2 => others */
const U08 gau1_search_cand_type_to_spatiality_map[NUM_SEARCH_CAND_TYPES] = {
    2,  //ZERO_MV
    2,  //ZERO_MV_ALTREF
    1,  //SPATIAL_LEFT0
    1,  //SPATIAL_TOP0
    1,  //SPATIAL_TOP_RIGHT0
    1,  //SPATIAL_TOP_LEFT0
    1,  //SPATIAL_LEFT1
    1,  //SPATIAL_TOP1
    1,  //SPATIAL_TOP_RIGHT1
    1,  //SPATIAL_TOP_LEFT1
    0,  //PROJECTED_COLOC0
    0,  //PROJECTED_COLOC1
    0,  //PROJECTED_COLOC2
    0,  //PROJECTED_COLOC3
    0,  //PROJECTED_COLOC4
    0,  //PROJECTED_COLOC5
    0,  //PROJECTED_COLOC6
    0,  //PROJECTED_COLOC7
    0,  //PROJECTED_COLOC_TR0
    0,  //PROJECTED_COLOC_TR1
    0,  //PROJECTED_COLOC_BL0
    0,  //PROJECTED_COLOC_BL1
    0,  //PROJECTED_COLOC_BR0
    0,  //PROJECTED_COLOC_BR1
    0,  //PROJECTED_TOP0
    0,  //PROJECTED_TOP1
    0,  //PROJECTED_TOP_RIGHT0
    0,  //PROJECTED_TOP_RIGHT1
    0,  //PROJECTED_TOP_LEFT0
    0,  //PROJECTED_TOP_LEFT1
    0,  //PROJECTED_RIGHT0
    0,  //PROJECTED_RIGHT1
    0,  //PROJECTED_BOTTOM0
    0,  //PROJECTED_BOTTOM1
    0,  //PROJECTED_BOTTOM_RIGHT0
    0,  //PROJECTED_BOTTOM_RIGHT1
    0,  //PROJECTED_BOTTOM_LEFT0
    0,  //PROJECTED_BOTTOM_LEFT1
    0,  //COLOCATED_GLOBAL_MV0
    0,  //COLOCATED_GLOBAL_MV1
    0,  //PROJECTED_TOP2
    0,  //PROJECTED_TOP3
    0,  //PROJECTED_TOP_RIGHT2
    0,  //PROJECTED_TOP_RIGHT3
    0,  //PROJECTED_TOP_LEFT2
    0,  //PROJECTED_TOP_LEFT3
    0,  //PROJECTED_RIGHT2
    0,  //PROJECTED_RIGHT3
    0,  //PROJECTED_BOTTOM2
    0,  //PROJECTED_BOTTOM3
    0,  //PROJECTED_BOTTOM_RIGHT2
    0,  //PROJECTED_BOTTOM_RIGHT3
    0,  //PROJECTED_BOTTOM_LEFT2
    0,  //PROJECTED_BOTTOM_LEFT3
};

const S08 gai1_search_cand_type_to_result_id_map[NUM_SEARCH_CAND_TYPES] = {
    0,  //ZERO_MV
    1,  //ZERO_MV_ALTREF
    0,  //SPATIAL_LEFT0
    0,  //SPATIAL_TOP0
    0,  //SPATIAL_TOP_RIGHT0
    0,  //SPATIAL_TOP_LEFT0
    1,  //SPATIAL_LEFT1
    1,  //SPATIAL_TOP1
    1,  //SPATIAL_TOP_RIGHT1
    1,  //SPATIAL_TOP_LEFT1
    0,  //PROJECTED_COLOC0
    1,  //PROJECTED_COLOC1
    2,  //PROJECTED_COLOC2
    3,  //PROJECTED_COLOC3
    4,  //PROJECTED_COLOC4
    5,  //PROJECTED_COLOC5
    6,  //PROJECTED_COLOC6
    7,  //PROJECTED_COLOC7
    0,  //PROJECTED_COLOC_TR0
    1,  //PROJECTED_COLOC_TR1
    0,  //PROJECTED_COLOC_BL0
    1,  //PROJECTED_COLOC_BL1
    0,  //PROJECTED_COLOC_BR0
    1,  //PROJECTED_COLOC_BR1
    0,  //PROJECTED_TOP0
    1,  //PROJECTED_TOP1
    0,  //PROJECTED_TOP_RIGHT0
    1,  //PROJECTED_TOP_RIGHT1
    0,  //PROJECTED_TOP_LEFT0
    1,  //PROJECTED_TOP_LEFT1
    0,  //PROJECTED_RIGHT0
    1,  //PROJECTED_RIGHT1
    0,  //PROJECTED_BOTTOM0
    1,  //PROJECTED_BOTTOM1
    0,  //PROJECTED_BOTTOM_RIGHT0
    1,  //PROJECTED_BOTTOM_RIGHT1
    0,  //PROJECTED_BOTTOM_LEFT0
    1,  //PROJECTED_BOTTOM_LEFT1
    0,  //COLOCATED_GLOBAL_MV0
    1,  //COLOCATED_GLOBAL_MV1
    2,  //PROJECTED_TOP2
    3,  //PROJECTED_TOP3
    2,  //PROJECTED_TOP_RIGHT2
    3,  //PROJECTED_TOP_RIGHT3
    2,  //PROJECTED_TOP_LEFT2
    3,  //PROJECTED_TOP_LEFT3
    2,  //PROJECTED_RIGHT2
    3,  //PROJECTED_RIGHT3
    2,  //PROJECTED_BOTTOM2
    3,  //PROJECTED_BOTTOM3
    2,  //PROJECTED_BOTTOM_RIGHT2
    3,  //PROJECTED_BOTTOM_RIGHT3
    2,  //PROJECTED_BOTTOM_LEFT2
    3  //PROJECTED_BOTTOM_LEFT3
};

const S32 gai4_search_cand_location_to_x_offset_map[NUM_SEARCH_CAND_LOCATIONS] = {
    COLOCATED_BLOCK_OFFSET,
    COLOCATED_4X4_NEXT_BLOCK_OFFSET,
    COLOCATED_BLOCK_OFFSET,
    COLOCATED_4X4_NEXT_BLOCK_OFFSET,
    -PREV_BLOCK_OFFSET_IN_L0_ME,
    -PREV_BLOCK_OFFSET_IN_L0_ME,
    0,
    NEXT_BLOCK_OFFSET_IN_L0_ME,
    NEXT_BLOCK_OFFSET_IN_L0_ME,
    NEXT_BLOCK_OFFSET_IN_L0_ME,
    0,
    -PREV_BLOCK_OFFSET_IN_L0_ME
};

const S32 gai4_search_cand_location_to_y_offset_map[NUM_SEARCH_CAND_LOCATIONS] = {
    COLOCATED_BLOCK_OFFSET,
    COLOCATED_BLOCK_OFFSET,
    COLOCATED_4X4_NEXT_BLOCK_OFFSET,
    COLOCATED_4X4_NEXT_BLOCK_OFFSET,
    0,
    -PREV_BLOCK_OFFSET_IN_L0_ME,
    -PREV_BLOCK_OFFSET_IN_L0_ME,
    -PREV_BLOCK_OFFSET_IN_L0_ME,
    0,
    NEXT_BLOCK_OFFSET_IN_L0_ME,
    NEXT_BLOCK_OFFSET_IN_L0_ME,
    NEXT_BLOCK_OFFSET_IN_L0_ME
};

/**
*******************************************************************************
* @brief Used exclusively in the Intrinsics version of the function
*       'hme_combine_4x4_sads_and_compute_cost_high_quality' instead
*       of calling get_range()
*******************************************************************************
*/
S16 gi2_mvx_range_high_quality[MAX_MVX_SUPPORTED_IN_COARSE_LAYER * 2 + 1][8] = {
    { 16, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 12 },
    { 14, 14, 14, 14, 14, 14, 14, 12 }, { 14, 14, 14, 14, 14, 14, 12, 12 },
    { 14, 14, 14, 14, 14, 14, 12, 12 }, { 14, 14, 14, 14, 14, 12, 12, 12 },
    { 14, 14, 14, 14, 14, 12, 12, 12 }, { 14, 14, 14, 14, 12, 12, 12, 12 },
    { 14, 14, 14, 14, 12, 12, 12, 12 }, { 14, 14, 14, 12, 12, 12, 12, 12 },
    { 14, 14, 14, 12, 12, 12, 12, 12 }, { 14, 14, 12, 12, 12, 12, 12, 12 },
    { 14, 14, 12, 12, 12, 12, 12, 12 }, { 14, 12, 12, 12, 12, 12, 12, 12 },
    { 14, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 10 },
    { 12, 12, 12, 12, 12, 12, 12, 10 }, { 12, 12, 12, 12, 12, 12, 10, 10 },
    { 12, 12, 12, 12, 12, 12, 10, 10 }, { 12, 12, 12, 12, 12, 10, 10, 10 },
    { 12, 12, 12, 12, 12, 10, 10, 10 }, { 12, 12, 12, 12, 10, 10, 10, 10 },
    { 12, 12, 12, 12, 10, 10, 10, 10 }, { 12, 12, 12, 10, 10, 10, 10, 10 },
    { 12, 12, 12, 10, 10, 10, 10, 10 }, { 12, 12, 10, 10, 10, 10, 10, 10 },
    { 12, 12, 10, 10, 10, 10, 10, 10 }, { 12, 10, 10, 10, 10, 10, 10, 10 },
    { 12, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 8 },
    { 10, 10, 10, 10, 10, 10, 10, 8 },  { 10, 10, 10, 10, 10, 10, 8, 8 },
    { 10, 10, 10, 10, 10, 10, 8, 8 },   { 10, 10, 10, 10, 10, 8, 8, 8 },
    { 10, 10, 10, 10, 10, 8, 8, 8 },    { 10, 10, 10, 10, 8, 8, 8, 8 },
    { 10, 10, 10, 10, 8, 8, 8, 8 },     { 10, 10, 10, 8, 8, 8, 8, 6 },
    { 10, 10, 10, 8, 8, 8, 8, 6 },      { 10, 10, 8, 8, 8, 8, 6, 6 },
    { 10, 10, 8, 8, 8, 8, 6, 6 },       { 10, 8, 8, 8, 8, 6, 6, 4 },
    { 10, 8, 8, 8, 8, 6, 6, 4 },        { 8, 8, 8, 8, 6, 6, 4, 2 },
    { 8, 8, 8, 8, 6, 6, 4, 1 },         { 8, 8, 8, 6, 6, 4, 2, 2 },
    { 8, 8, 8, 6, 6, 4, 1, 4 },         { 8, 8, 6, 6, 4, 2, 2, 4 },
    { 8, 8, 6, 6, 4, 1, 4, 6 },         { 8, 6, 6, 4, 2, 2, 4, 6 },
    { 8, 6, 6, 4, 1, 4, 6, 6 },         { 6, 6, 4, 2, 2, 4, 6, 6 },
    { 6, 6, 4, 1, 4, 6, 6, 8 },         { 6, 4, 2, 2, 4, 6, 6, 8 },
    { 6, 4, 1, 4, 6, 6, 8, 8 },         { 4, 2, 2, 4, 6, 6, 8, 8 },
    { 4, 1, 4, 6, 6, 8, 8, 8 },         { 2, 2, 4, 6, 6, 8, 8, 8 },
    { 1, 4, 6, 6, 8, 8, 8, 8 },         { 2, 4, 6, 6, 8, 8, 8, 8 },
    { 4, 6, 6, 8, 8, 8, 8, 10 },        { 4, 6, 6, 8, 8, 8, 8, 10 },
    { 6, 6, 8, 8, 8, 8, 10, 10 },       { 6, 6, 8, 8, 8, 8, 10, 10 },
    { 6, 8, 8, 8, 8, 10, 10, 10 },      { 6, 8, 8, 8, 8, 10, 10, 10 },
    { 8, 8, 8, 8, 10, 10, 10, 10 },     { 8, 8, 8, 8, 10, 10, 10, 10 },
    { 8, 8, 8, 10, 10, 10, 10, 10 },    { 8, 8, 8, 10, 10, 10, 10, 10 },
    { 8, 8, 10, 10, 10, 10, 10, 10 },   { 8, 8, 10, 10, 10, 10, 10, 10 },
    { 8, 10, 10, 10, 10, 10, 10, 10 },  { 8, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 12 }, { 10, 10, 10, 10, 10, 10, 10, 12 },
    { 10, 10, 10, 10, 10, 10, 12, 12 }, { 10, 10, 10, 10, 10, 10, 12, 12 },
    { 10, 10, 10, 10, 10, 12, 12, 12 }, { 10, 10, 10, 10, 10, 12, 12, 12 },
    { 10, 10, 10, 10, 12, 12, 12, 12 }, { 10, 10, 10, 10, 12, 12, 12, 12 },
    { 10, 10, 10, 12, 12, 12, 12, 12 }, { 10, 10, 10, 12, 12, 12, 12, 12 },
    { 10, 10, 12, 12, 12, 12, 12, 12 }, { 10, 10, 12, 12, 12, 12, 12, 12 },
    { 10, 12, 12, 12, 12, 12, 12, 12 }, { 10, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 14 }, { 12, 12, 12, 12, 12, 12, 12, 14 },
    { 12, 12, 12, 12, 12, 12, 14, 14 }, { 12, 12, 12, 12, 12, 12, 14, 14 },
    { 12, 12, 12, 12, 12, 14, 14, 14 }, { 12, 12, 12, 12, 12, 14, 14, 14 },
    { 12, 12, 12, 12, 14, 14, 14, 14 }, { 12, 12, 12, 12, 14, 14, 14, 14 },
    { 12, 12, 12, 14, 14, 14, 14, 14 }, { 12, 12, 12, 14, 14, 14, 14, 14 },
    { 12, 12, 14, 14, 14, 14, 14, 14 }, { 12, 12, 14, 14, 14, 14, 14, 14 },
    { 12, 14, 14, 14, 14, 14, 14, 14 }, { 12, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 16 }, { 14, 14, 14, 14, 14, 14, 14, 16 },
    { 14, 14, 14, 14, 14, 14, 16, 16 }, { 14, 14, 14, 14, 14, 14, 16, 16 },
    { 14, 14, 14, 14, 14, 16, 16, 16 }, { 14, 14, 14, 14, 14, 16, 16, 16 },
    { 14, 14, 14, 14, 16, 16, 16, 16 }, { 14, 14, 14, 14, 16, 16, 16, 16 },
    { 14, 14, 14, 16, 16, 16, 16, 16 }, { 14, 14, 14, 16, 16, 16, 16, 16 },
    { 14, 14, 16, 16, 16, 16, 16, 16 }, { 14, 14, 16, 16, 16, 16, 16, 16 },
    { 14, 16, 16, 16, 16, 16, 16, 16 }, { 14, 16, 16, 16, 16, 16, 16, 16 },
    { 16, 16, 16, 16, 16, 16, 16, 16 }
};

const S16 gai2_mvx_range_mapping[8193][8] = {
    { 15, 14, 14, 15, 14, 15, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 13, 14, 13, 14, 14, 13 },
    { 14, 14, 13, 14, 13, 14, 14, 13 }, { 14, 14, 13, 14, 13, 14, 14, 13 },
    { 14, 14, 13, 14, 13, 14, 14, 13 }, { 14, 13, 13, 14, 13, 14, 13, 13 },
    { 14, 13, 13, 14, 13, 14, 13, 13 }, { 14, 13, 13, 14, 13, 14, 13, 13 },
    { 14, 13, 13, 14, 13, 14, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 12, 13, 12, 13, 13, 12 },
    { 13, 13, 12, 13, 12, 13, 13, 12 }, { 13, 13, 12, 13, 12, 13, 13, 12 },
    { 13, 13, 12, 13, 12, 13, 13, 12 }, { 13, 12, 12, 13, 12, 13, 12, 12 },
    { 13, 12, 12, 13, 12, 13, 12, 12 }, { 13, 12, 12, 13, 12, 13, 12, 12 },
    { 13, 12, 12, 13, 12, 13, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 11, 12, 11, 12, 12, 11 },
    { 12, 12, 11, 12, 11, 12, 12, 11 }, { 12, 12, 11, 12, 11, 12, 12, 11 },
    { 12, 12, 11, 12, 11, 12, 12, 11 }, { 12, 11, 11, 12, 11, 12, 11, 11 },
    { 12, 11, 11, 12, 11, 12, 11, 11 }, { 12, 11, 11, 12, 11, 12, 11, 11 },
    { 12, 11, 11, 12, 11, 12, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 10, 11, 10, 11, 11, 10 },
    { 11, 11, 10, 11, 10, 11, 11, 10 }, { 11, 11, 10, 11, 10, 11, 11, 10 },
    { 11, 11, 10, 11, 10, 11, 11, 10 }, { 11, 10, 10, 11, 10, 11, 10, 10 },
    { 11, 10, 10, 11, 10, 11, 10, 10 }, { 11, 10, 10, 11, 10, 11, 10, 10 },
    { 11, 10, 10, 11, 10, 11, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 9, 10, 9, 10, 10, 9 },
    { 10, 10, 9, 10, 9, 10, 10, 9 },    { 10, 10, 9, 10, 9, 10, 10, 9 },
    { 10, 10, 9, 10, 9, 10, 10, 9 },    { 10, 9, 9, 10, 9, 10, 9, 9 },
    { 10, 9, 9, 10, 9, 10, 9, 9 },      { 10, 9, 9, 10, 9, 10, 9, 9 },
    { 10, 9, 9, 10, 9, 10, 9, 9 },      { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 8, 9, 8, 9, 9, 8 },
    { 9, 9, 8, 9, 8, 9, 9, 8 },         { 9, 9, 8, 9, 8, 9, 9, 8 },
    { 9, 9, 8, 9, 8, 9, 9, 8 },         { 9, 8, 8, 9, 8, 9, 8, 8 },
    { 9, 8, 8, 9, 8, 9, 8, 8 },         { 9, 8, 8, 9, 8, 9, 8, 8 },
    { 9, 8, 8, 9, 8, 9, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 7, 8, 7, 8, 8, 7 },
    { 8, 8, 7, 8, 7, 8, 8, 7 },         { 8, 8, 7, 8, 7, 8, 8, 7 },
    { 8, 8, 7, 8, 7, 8, 8, 7 },         { 8, 7, 7, 8, 7, 8, 7, 7 },
    { 8, 7, 7, 8, 7, 8, 7, 7 },         { 8, 7, 7, 8, 7, 8, 7, 7 },
    { 8, 7, 7, 8, 7, 8, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 6, 7, 6, 7, 7, 6 },
    { 7, 7, 6, 7, 6, 7, 7, 6 },         { 7, 7, 6, 7, 6, 7, 7, 6 },
    { 7, 7, 6, 7, 6, 7, 7, 6 },         { 7, 6, 6, 7, 6, 7, 6, 6 },
    { 7, 6, 6, 7, 6, 7, 6, 6 },         { 7, 6, 6, 7, 6, 7, 6, 6 },
    { 7, 6, 6, 7, 6, 7, 6, 6 },         { 6, 6, 5, 6, 5, 6, 6, 5 },
    { 6, 6, 5, 6, 5, 6, 6, 5 },         { 6, 6, 5, 6, 5, 6, 6, 5 },
    { 6, 6, 5, 6, 5, 6, 6, 5 },         { 6, 5, 4, 6, 4, 6, 5, 4 },
    { 6, 5, 4, 6, 4, 6, 5, 4 },         { 6, 5, 3, 6, 3, 6, 5, 3 },
    { 6, 5, 2, 6, 2, 6, 5, 2 },         { 5, 4, 3, 5, 3, 5, 4, 3 },
    { 5, 4, 4, 5, 4, 5, 4, 4 },         { 5, 3, 4, 5, 4, 5, 3, 4 },
    { 5, 2, 5, 5, 5, 5, 2, 5 },         { 4, 3, 5, 4, 5, 4, 3, 5 },
    { 4, 4, 5, 4, 5, 4, 4, 5 },         { 3, 4, 5, 3, 5, 3, 4, 5 },
    { 2, 5, 6, 2, 6, 2, 5, 6 },         { 3, 5, 6, 3, 6, 3, 5, 6 },
    { 4, 5, 6, 4, 6, 4, 5, 6 },         { 4, 5, 6, 4, 6, 4, 5, 6 },
    { 5, 6, 6, 5, 6, 5, 6, 6 },         { 5, 6, 6, 5, 6, 5, 6, 6 },
    { 5, 6, 6, 5, 6, 5, 6, 6 },         { 5, 6, 6, 5, 6, 5, 6, 6 },
    { 6, 6, 7, 6, 7, 6, 6, 7 },         { 6, 6, 7, 6, 7, 6, 6, 7 },
    { 6, 6, 7, 6, 7, 6, 6, 7 },         { 6, 6, 7, 6, 7, 6, 6, 7 },
    { 6, 7, 7, 6, 7, 6, 7, 7 },         { 6, 7, 7, 6, 7, 6, 7, 7 },
    { 6, 7, 7, 6, 7, 6, 7, 7 },         { 6, 7, 7, 6, 7, 6, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 8, 7, 8, 7, 7, 8 },         { 7, 7, 8, 7, 8, 7, 7, 8 },
    { 7, 7, 8, 7, 8, 7, 7, 8 },         { 7, 7, 8, 7, 8, 7, 7, 8 },
    { 7, 8, 8, 7, 8, 7, 8, 8 },         { 7, 8, 8, 7, 8, 7, 8, 8 },
    { 7, 8, 8, 7, 8, 7, 8, 8 },         { 7, 8, 8, 7, 8, 7, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 9, 8, 9, 8, 8, 9 },         { 8, 8, 9, 8, 9, 8, 8, 9 },
    { 8, 8, 9, 8, 9, 8, 8, 9 },         { 8, 8, 9, 8, 9, 8, 8, 9 },
    { 8, 9, 9, 8, 9, 8, 9, 9 },         { 8, 9, 9, 8, 9, 8, 9, 9 },
    { 8, 9, 9, 8, 9, 8, 9, 9 },         { 8, 9, 9, 8, 9, 8, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 10, 9, 10, 9, 9, 10 },      { 9, 9, 10, 9, 10, 9, 9, 10 },
    { 9, 9, 10, 9, 10, 9, 9, 10 },      { 9, 9, 10, 9, 10, 9, 9, 10 },
    { 9, 10, 10, 9, 10, 9, 10, 10 },    { 9, 10, 10, 9, 10, 9, 10, 10 },
    { 9, 10, 10, 9, 10, 9, 10, 10 },    { 9, 10, 10, 9, 10, 9, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 11, 10, 11, 10, 10, 11 }, { 10, 10, 11, 10, 11, 10, 10, 11 },
    { 10, 10, 11, 10, 11, 10, 10, 11 }, { 10, 10, 11, 10, 11, 10, 10, 11 },
    { 10, 11, 11, 10, 11, 10, 11, 11 }, { 10, 11, 11, 10, 11, 10, 11, 11 },
    { 10, 11, 11, 10, 11, 10, 11, 11 }, { 10, 11, 11, 10, 11, 10, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 12, 11, 12, 11, 11, 12 }, { 11, 11, 12, 11, 12, 11, 11, 12 },
    { 11, 11, 12, 11, 12, 11, 11, 12 }, { 11, 11, 12, 11, 12, 11, 11, 12 },
    { 11, 12, 12, 11, 12, 11, 12, 12 }, { 11, 12, 12, 11, 12, 11, 12, 12 },
    { 11, 12, 12, 11, 12, 11, 12, 12 }, { 11, 12, 12, 11, 12, 11, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 13, 12, 13, 12, 12, 13 }, { 12, 12, 13, 12, 13, 12, 12, 13 },
    { 12, 12, 13, 12, 13, 12, 12, 13 }, { 12, 12, 13, 12, 13, 12, 12, 13 },
    { 12, 13, 13, 12, 13, 12, 13, 13 }, { 12, 13, 13, 12, 13, 12, 13, 13 },
    { 12, 13, 13, 12, 13, 12, 13, 13 }, { 12, 13, 13, 12, 13, 12, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 14, 13, 14, 13, 13, 14 }, { 13, 13, 14, 13, 14, 13, 13, 14 },
    { 13, 13, 14, 13, 14, 13, 13, 14 }, { 13, 13, 14, 13, 14, 13, 13, 14 },
    { 13, 14, 14, 13, 14, 13, 14, 14 }, { 13, 14, 14, 13, 14, 13, 14, 14 },
    { 13, 14, 14, 13, 14, 13, 14, 14 }, { 13, 14, 14, 13, 14, 13, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }, { 14, 14, 14, 14, 14, 14, 14, 14 },
    { 14, 14, 15, 14, 15, 14, 14, 15 }, { 14, 14, 15, 14, 15, 14, 14, 15 },
    { 14, 14, 15, 14, 15, 14, 14, 15 }, { 14, 14, 15, 14, 15, 14, 14, 15 },
    { 14, 15, 15, 14, 15, 14, 15, 15 }, { 14, 15, 15, 14, 15, 14, 15, 15 },
    { 14, 15, 15, 14, 15, 14, 15, 15 }, { 14, 15, 15, 14, 15, 14, 15, 15 },
    { 15, 15, 15, 15, 15, 15, 15, 15 }
};

const S16 gai2_mvy_range_mapping[4097][8] = {
    { 14, 14, 14, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 12, 12, 12 },
    { 13, 13, 13, 13, 13, 12, 12, 12 }, { 13, 13, 13, 13, 13, 12, 12, 12 },
    { 13, 13, 13, 13, 13, 12, 12, 12 }, { 13, 13, 13, 12, 12, 12, 12, 12 },
    { 13, 13, 13, 12, 12, 12, 12, 12 }, { 13, 13, 13, 12, 12, 12, 12, 12 },
    { 13, 13, 13, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 11, 11, 11 },
    { 12, 12, 12, 12, 12, 11, 11, 11 }, { 12, 12, 12, 12, 12, 11, 11, 11 },
    { 12, 12, 12, 12, 12, 11, 11, 11 }, { 12, 12, 12, 11, 11, 11, 11, 11 },
    { 12, 12, 12, 11, 11, 11, 11, 11 }, { 12, 12, 12, 11, 11, 11, 11, 11 },
    { 12, 12, 12, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 10, 10, 10 },
    { 11, 11, 11, 11, 11, 10, 10, 10 }, { 11, 11, 11, 11, 11, 10, 10, 10 },
    { 11, 11, 11, 11, 11, 10, 10, 10 }, { 11, 11, 11, 10, 10, 10, 10, 10 },
    { 11, 11, 11, 10, 10, 10, 10, 10 }, { 11, 11, 11, 10, 10, 10, 10, 10 },
    { 11, 11, 11, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 9, 9, 9 },
    { 10, 10, 10, 10, 10, 9, 9, 9 },    { 10, 10, 10, 10, 10, 9, 9, 9 },
    { 10, 10, 10, 10, 10, 9, 9, 9 },    { 10, 10, 10, 9, 9, 9, 9, 9 },
    { 10, 10, 10, 9, 9, 9, 9, 9 },      { 10, 10, 10, 9, 9, 9, 9, 9 },
    { 10, 10, 10, 9, 9, 9, 9, 9 },      { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 8, 8, 8 },
    { 9, 9, 9, 9, 9, 8, 8, 8 },         { 9, 9, 9, 9, 9, 8, 8, 8 },
    { 9, 9, 9, 9, 9, 8, 8, 8 },         { 9, 9, 9, 8, 8, 8, 8, 8 },
    { 9, 9, 9, 8, 8, 8, 8, 8 },         { 9, 9, 9, 8, 8, 8, 8, 8 },
    { 9, 9, 9, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 7, 7, 7 },
    { 8, 8, 8, 8, 8, 7, 7, 7 },         { 8, 8, 8, 8, 8, 7, 7, 7 },
    { 8, 8, 8, 8, 8, 7, 7, 7 },         { 8, 8, 8, 7, 7, 7, 7, 7 },
    { 8, 8, 8, 7, 7, 7, 7, 7 },         { 8, 8, 8, 7, 7, 7, 7, 7 },
    { 8, 8, 8, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 6, 6, 6 },
    { 7, 7, 7, 7, 7, 6, 6, 6 },         { 7, 7, 7, 7, 7, 6, 6, 6 },
    { 7, 7, 7, 7, 7, 6, 6, 6 },         { 7, 7, 7, 6, 6, 6, 6, 6 },
    { 7, 7, 7, 6, 6, 6, 6, 6 },         { 7, 7, 7, 6, 6, 6, 6, 6 },
    { 7, 7, 7, 6, 6, 6, 6, 6 },         { 6, 6, 6, 6, 6, 5, 5, 5 },
    { 6, 6, 6, 6, 6, 5, 5, 5 },         { 6, 6, 6, 6, 6, 5, 5, 5 },
    { 6, 6, 6, 6, 6, 5, 5, 5 },         { 6, 6, 6, 5, 5, 4, 4, 4 },
    { 6, 6, 6, 5, 5, 4, 4, 4 },         { 6, 6, 6, 5, 5, 3, 3, 3 },
    { 6, 6, 6, 5, 5, 2, 2, 2 },         { 5, 5, 5, 4, 4, 3, 3, 3 },
    { 5, 5, 5, 4, 4, 4, 4, 4 },         { 5, 5, 5, 3, 3, 4, 4, 4 },
    { 5, 5, 5, 2, 2, 5, 5, 5 },         { 4, 4, 4, 3, 3, 5, 5, 5 },
    { 4, 4, 4, 4, 4, 5, 5, 5 },         { 3, 3, 3, 4, 4, 5, 5, 5 },
    { 2, 2, 2, 5, 5, 6, 6, 6 },         { 3, 3, 3, 5, 5, 6, 6, 6 },
    { 4, 4, 4, 5, 5, 6, 6, 6 },         { 4, 4, 4, 5, 5, 6, 6, 6 },
    { 5, 5, 5, 6, 6, 6, 6, 6 },         { 5, 5, 5, 6, 6, 6, 6, 6 },
    { 5, 5, 5, 6, 6, 6, 6, 6 },         { 5, 5, 5, 6, 6, 6, 6, 6 },
    { 6, 6, 6, 6, 6, 7, 7, 7 },         { 6, 6, 6, 6, 6, 7, 7, 7 },
    { 6, 6, 6, 6, 6, 7, 7, 7 },         { 6, 6, 6, 6, 6, 7, 7, 7 },
    { 6, 6, 6, 7, 7, 7, 7, 7 },         { 6, 6, 6, 7, 7, 7, 7, 7 },
    { 6, 6, 6, 7, 7, 7, 7, 7 },         { 6, 6, 6, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },         { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 8, 8, 8 },         { 7, 7, 7, 7, 7, 8, 8, 8 },
    { 7, 7, 7, 7, 7, 8, 8, 8 },         { 7, 7, 7, 7, 7, 8, 8, 8 },
    { 7, 7, 7, 8, 8, 8, 8, 8 },         { 7, 7, 7, 8, 8, 8, 8, 8 },
    { 7, 7, 7, 8, 8, 8, 8, 8 },         { 7, 7, 7, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 8, 8, 8 },         { 8, 8, 8, 8, 8, 8, 8, 8 },
    { 8, 8, 8, 8, 8, 9, 9, 9 },         { 8, 8, 8, 8, 8, 9, 9, 9 },
    { 8, 8, 8, 8, 8, 9, 9, 9 },         { 8, 8, 8, 8, 8, 9, 9, 9 },
    { 8, 8, 8, 9, 9, 9, 9, 9 },         { 8, 8, 8, 9, 9, 9, 9, 9 },
    { 8, 8, 8, 9, 9, 9, 9, 9 },         { 8, 8, 8, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 9, 9, 9 },         { 9, 9, 9, 9, 9, 9, 9, 9 },
    { 9, 9, 9, 9, 9, 10, 10, 10 },      { 9, 9, 9, 9, 9, 10, 10, 10 },
    { 9, 9, 9, 9, 9, 10, 10, 10 },      { 9, 9, 9, 9, 9, 10, 10, 10 },
    { 9, 9, 9, 10, 10, 10, 10, 10 },    { 9, 9, 9, 10, 10, 10, 10, 10 },
    { 9, 9, 9, 10, 10, 10, 10, 10 },    { 9, 9, 9, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 10, 10, 10 }, { 10, 10, 10, 10, 10, 10, 10, 10 },
    { 10, 10, 10, 10, 10, 11, 11, 11 }, { 10, 10, 10, 10, 10, 11, 11, 11 },
    { 10, 10, 10, 10, 10, 11, 11, 11 }, { 10, 10, 10, 10, 10, 11, 11, 11 },
    { 10, 10, 10, 11, 11, 11, 11, 11 }, { 10, 10, 10, 11, 11, 11, 11, 11 },
    { 10, 10, 10, 11, 11, 11, 11, 11 }, { 10, 10, 10, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 11, 11, 11 }, { 11, 11, 11, 11, 11, 11, 11, 11 },
    { 11, 11, 11, 11, 11, 12, 12, 12 }, { 11, 11, 11, 11, 11, 12, 12, 12 },
    { 11, 11, 11, 11, 11, 12, 12, 12 }, { 11, 11, 11, 11, 11, 12, 12, 12 },
    { 11, 11, 11, 12, 12, 12, 12, 12 }, { 11, 11, 11, 12, 12, 12, 12, 12 },
    { 11, 11, 11, 12, 12, 12, 12, 12 }, { 11, 11, 11, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 12, 12, 12 }, { 12, 12, 12, 12, 12, 12, 12, 12 },
    { 12, 12, 12, 12, 12, 13, 13, 13 }, { 12, 12, 12, 12, 12, 13, 13, 13 },
    { 12, 12, 12, 12, 12, 13, 13, 13 }, { 12, 12, 12, 12, 12, 13, 13, 13 },
    { 12, 12, 12, 13, 13, 13, 13, 13 }, { 12, 12, 12, 13, 13, 13, 13, 13 },
    { 12, 12, 12, 13, 13, 13, 13, 13 }, { 12, 12, 12, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 13, 13, 13 }, { 13, 13, 13, 13, 13, 13, 13, 13 },
    { 13, 13, 13, 13, 13, 14, 14, 14 }, { 13, 13, 13, 13, 13, 14, 14, 14 },
    { 13, 13, 13, 13, 13, 14, 14, 14 }, { 13, 13, 13, 13, 13, 14, 14, 14 },
    { 13, 13, 13, 14, 14, 14, 14, 14 }, { 13, 13, 13, 14, 14, 14, 14, 14 },
    { 13, 13, 13, 14, 14, 14, 14, 14 }, { 13, 13, 13, 14, 14, 14, 14, 14 },
    { 14, 14, 14, 14, 14, 14, 14, 14 }
};

const S16 gai2_set_best_cost_max[8][8] = {
    { 0xFFFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0xFFFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0xFFFF, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0xFFFF, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0xFFFF, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFFFF, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFFFF, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFFFF },
};

const S08 gai1_mv_adjust[8][2] = { { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 1 },
                                   { 2, 1 }, { 0, 2 }, { 1, 2 }, { 2, 2 }

};

const S08 gai1_mv_offsets_from_center_in_rect_grid[NUM_POINTS_IN_RECTANGULAR_GRID][2] = {
    { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { 0, 0 }
};
