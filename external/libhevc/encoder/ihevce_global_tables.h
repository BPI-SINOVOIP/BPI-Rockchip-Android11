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
* @file ihevce_global_tables.h
*
* @brief
*  This file contains declarations of global tables used by the encoder
*
* @author
*  Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_GLOBAL_TABLES_H_
#define _IHEVCE_GLOBAL_TABLES_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/
typedef struct
{
    LEVEL_T e_level;

    UWORD32 u4_max_luma_sample_rate;

    WORD32 i4_max_luma_picture_size;

    WORD32 i4_max_bit_rate[TOTAL_NUM_TIERS];

    WORD32 i4_max_cpb[TOTAL_NUM_TIERS];

    WORD32 i4_min_compression_ratio;

    WORD32 i4_max_slices_per_picture;

    WORD32 i4_max_num_tile_rows;

    WORD32 i4_max_num_tile_columns;
} level_data_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/
extern const level_data_t g_as_level_data[TOTAL_NUM_LEVELS];

extern const WORD16 gi2_flat_scale_mat_4x4[];
extern const WORD16 gi2_flat_scale_mat_8x8[];

extern const WORD16 gi2_flat_scale_mat_16x16[];

extern const WORD16 gi2_flat_rescale_mat_4x4[];
extern const WORD16 gi2_flat_rescale_mat_8x8[];
extern const WORD16 gi2_flat_rescale_mat_16x16[];

extern const UWORD8 g_u1_scan_table_8x8[3][64];
extern const UWORD8 g_u1_scan_table_4x4[3][16];
extern const UWORD8 g_u1_scan_table_2x2[3][4];
extern const UWORD8 g_u1_scan_table_1x1[1];

extern qpel_input_buf_cfg_t gas_qpel_inp_buf_cfg[4][4];

extern const WORD8 gai1_is_part_vertical[TOT_NUM_PARTS];
extern const WORD8 gai1_part_wd_and_ht[TOT_NUM_PARTS][2];

extern UWORD8 gau1_ref_bits[16];

extern const UWORD8 gau1_ctb_raster_to_zscan[256];
extern WORD32 ga_trans_shift[5];

extern UWORD32 gau4_frame_qstep_multiplier[54];

extern UWORD8 gau1_inter_tu_posy_scl_amt_amp[4][10];

extern UWORD8 gau1_inter_tu_posy_scl_amt[4];

extern UWORD8 gau1_inter_tu_posx_scl_amt_amp[4][10];

extern UWORD8 gau1_inter_tu_posx_scl_amt[4];

extern UWORD8 gau1_inter_tu_shft_amt_amp[4][10];

extern UWORD8 gau1_inter_tu_shft_amt[4];

extern WORD32 g_i4_ip_funcs[MAX_NUM_IP_MODES];
extern const UWORD8 gau1_chroma422_intra_angle_mapping[36];
extern const UWORD32 gau4_nbr_flags_8x8_4x4blks[64];

extern WORD32 gai4_subBlock2csbfId_map4x4TU[1];
extern WORD32 gai4_subBlock2csbfId_map8x8TU[4];
extern WORD32 gai4_subBlock2csbfId_map16x16TU[16];
extern WORD32 gai4_subBlock2csbfId_map32x32TU[64];

extern const float gad_look_up_activity[TOT_QP_MOD_OFFSET];

#endif
