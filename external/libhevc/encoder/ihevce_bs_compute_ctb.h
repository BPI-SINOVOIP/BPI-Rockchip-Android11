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
* @file ihevce_bs_compute_ctb.h
*
* @brief
*  This file contains encoder boundary strength related macros and
*  interface prototypes
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_BS_COMPUTE_CTB_H_
#define _IHEVCE_BS_COMPUTE_CTB_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      defines the BS for a 32x32 TU in INTRA mode
******************************************************************************
 */
#define BS_INTRA_32 0xAAAA

/**
******************************************************************************
 *  @brief      defines the BS for a 16x16 TU in INTRA mode
******************************************************************************
 */
#define BS_INTRA_16 0xAA

/**
******************************************************************************
 *  @brief      defines the BS for a 8x8 TU in INTRA mode
******************************************************************************
 */
#define BS_INTRA_8 0xA

/**
******************************************************************************
 *  @brief      defines the BS for a 4x4 TU in INTRA mode
******************************************************************************
 */
#define BS_INTRA_4 0x2

/**
******************************************************************************
 *  @brief      defines the invalid BS in global array
******************************************************************************
 */
#define BS_INVALID 0xDEAF

/**
******************************************************************************
 *  @brief      defines the BS for a coded inter 32x32 TU
******************************************************************************
 */
#define BS_CBF_32 0x5555

/**
******************************************************************************
 *  @brief      defines the BS for a coded inter 16x16 TU
******************************************************************************
 */
#define BS_CBF_16 0x55

/**
******************************************************************************
 *  @brief      defines the BS for a coded inter 8x8 TU
******************************************************************************
 */
#define BS_CBF_8 0x5

/**
******************************************************************************
 *  @brief      defines the BS for a coded inter 4x4 TU
******************************************************************************
 */
#define BS_CBF_4 0x01

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief   Macro to set the value in input pointer with given value starting
 *  from ( 32 - (ip_pos<<1) - (edge_size>>1) ). This is for storing in BigEndian
 *  with 2 bits per 4x4. edge_size in pixels & ip_pos in terms of 4x4
 * (ip_pos<<1) : since 2bits per ip_pos (which is in 4x4)
 * (edge_size>>1) : since no. of bits of value is (edge_size>>1), edge_size in pix
******************************************************************************
 */
#define SET_VALUE_BIG(pu4_bs, value, ip_pos, edge_size)                                            \
    {                                                                                              \
        *(pu4_bs) = *(pu4_bs) | (value << (32 - (ip_pos << 1) - (edge_size >> 1)));                \
    }

/**
******************************************************************************
 *  @brief   extracts 2 bits starting from (30-2*ip_pos) from the value pointed
 *  by pu4_bs. This is for extracting from a BigEndian stored ip.
******************************************************************************
 */
#define EXTRACT_VALUE_BIG(pu4_bs, ip_pos) (((*(pu4_bs)) >> (30 - 2 * ip_pos)) & 0x3)

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_bs_init_ctb(
    deblk_bs_ctb_ctxt_t *ps_deblk_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 ctb_ctr,
    WORD32 vert_ctr);

void ihevce_bs_compute_cu(
    cu_enc_loop_out_t *ps_cu_final,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_curr_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 num_4x4_in_ctb,
    deblk_bs_ctb_ctxt_t *ps_deblk_prms);

void ihevce_bs_clear_invalid(
    deblk_bs_ctb_ctxt_t *ps_deblk_prms,
    WORD32 last_ctb_row_flag,
    WORD32 last_ctb_in_row_flag,
    WORD32 last_hz_ctb_wd,
    WORD32 last_vt_ctb_ht);

#endif /* _IHEVCE_BS_COMPUTE_CTB_H_ */
