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
* \file ihevce_decomp_pre_intra_structs.h
*
* \brief
*    This file contains strcutures of pre_enc_loop pass
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_DECOMP_PRE_INTRA_STRUCTS_H_
#define _IHEVCE_DECOMP_PRE_INTRA_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*For decomposition of every row we need some extra rows above n below that row*/
#define NUM_EXTRA_ROWS_REQ 3

/*Macros for pre intra early decisions*/
#define NUM_MODES 35
#define SAD_NOT_VALID 0xFFFFF

#define SET_T_AVAILABLE(x) (x = x | (1 << 8))
#define SET_L_AVAILABLE(x) (x = x | (1 << 7))
#define SET_TL_AVAILABLE(x) (x = x | (1 << 16))
#define SET_TR_AVAILABLE(x) (x = x | (1 << 12))
#define SET_BL_AVAILABLE(x) (x = x | (1 << 3))
#define SET_ALL_AVAILABLE(x) (x = (1 << 8) + (1 << 7) + (1 << 16) + (1 << 12) + (1 << 3))

#define SET_T_UNAVAILABLE(x) (x = x & ~((WORD32)1 << 8))
#define SET_L_UNAVAILABLE(x) (x = x & ~((WORD32)1 << 7))
#define SET_TL_UNAVAILABLE(x) (x = x & ~((WORD32)1 << 16))
#define SET_TR_UNAVAILABLE(x) (x = x & ~((WORD32)1 << 12))
#define SET_BL_UNAVAILABLE(x) (x = x & ~((WORD32)1 << 3))
#define SET_ALL_UNAVAILABLE(x) (x = 0)

#define CHECK_T_AVAILABLE(x) ((x & (1 << 8)) >> 8)
#define CHECK_L_AVAILABLE(x) ((x & (1 << 7)) >> 7)
#define CHECK_TL_AVAILABLE(x) ((x & (1 << 16)) >> 16)
#define CHECK_TR_AVAILABLE(x) ((x & (1 << 12)) >> 12)
#define CHECK_BL_AVAILABLE(x) ((x & (1 << 3)) >> 3)

/*  q format for lamba used in the encoder                                   */
#define LAMBDA_Q_SHIFT 8

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    DECOMP_PRE_INTRA_CTXT = 0,
    DECOMP_PRE_INTRA_THRDS_CTXT,
    DECOMP_PRE_INTRA_ED_CTXT,

    /* should always be the last entry */
    NUM_DECOMP_PRE_INTRA_MEM_RECS
} DECOMP_PRE_INTRA_MEM_TABS_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief  Context for  early intra or inter decision
******************************************************************************
 */
typedef struct
{
    /** lambda for cost calculation */
    WORD32 lambda;

    /*Pointer to 4x4 blocks of entire frame */
    ihevce_ed_blk_t *ps_ed_pic;

    /*Pointer to present 4x4 block */
    ihevce_ed_blk_t *ps_ed;

    /*Pointer to ctb level data of entire frame */
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_pic;

    /*Pointer to ctb level data of current ctb */
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;

    /*Sum of best SATDs at L1*/
    LWORD64 i8_sum_best_satd;

    /*Sum of best SATDs at L1*/
    LWORD64 i8_sum_sq_best_satd;

    /** Encoder quality preset : See IHEVCE_QUALITY_CONFIG_T for presets */
    WORD32 i4_quality_preset;

    /*following are the changes for reducing the stack memory used by this module. Local variables are copied to context memory */

    /** Neighbour flags. Used as local variable in pre_intra_process_row function. Shouldnt be used by other functions */
    WORD32 ai4_nbr_flags[64];

    /** reference data for four 4x4 blocks. This is used as local variable in ed_calc_8x8_blk */
    UWORD8 au1_ref_full_ctb[4][18];

    /** reference data for 8x8 block. This is used as local variable in ed_calc_8x8_blk */
    UWORD8 au1_ref_8x8[1][33];

    /** mode bits costs array. This is used as local variable in ed_calc_8x8_blk */
    UWORD16 au2_mode_bits_cost_full_ctb[4][NUM_MODES];

    /** Pointer to structure containing function pointers of common*/
    func_selector_t *ps_func_selector;

} ihevce_ed_ctxt_t;  //early decision

typedef struct
{
    /** Actual Width of this layer */
    WORD32 i4_actual_wd;

    /** Actual height of this layer */
    WORD32 i4_actual_ht;

    /** Padded width of this layer */
    WORD32 i4_padded_wd;

    /** Padded height of this layer */
    WORD32 i4_padded_ht;

    /** input pointer. */
    UWORD8 *pu1_inp;

    /** stride of input buffer */
    WORD32 i4_inp_stride;

    /** Decomposition block height size */
    WORD32 i4_decomp_blk_ht;

    /** Decomposition block width size */
    WORD32 i4_decomp_blk_wd;

    /** Number of blocks in a row */
    WORD32 i4_num_col_blks;

    /** Number of rows in a layer */
    WORD32 i4_num_row_blks;
    WORD32 ai4_curr_row_no[MAX_NUM_CTB_ROWS_FRM];

    WORD32 i4_num_rows_processed;
} decomp_layer_ctxt_t;

typedef struct
{
    /* Number of layers */
    WORD32 i4_num_layers;

    /** Handles for all layers. Entry 0 refers to L0 , 3 refers to L3 */
    decomp_layer_ctxt_t as_layers[MAX_NUM_HME_LAYERS];

    /** Array for working memory of the thread */
    UWORD8 au1_wkg_mem[((MAX_CTB_SIZE >> 1) * (MAX_CTB_SIZE + 2 * NUM_EXTRA_ROWS_REQ))];

    /** Encoder quality preset : See IHEVCE_QUALITY_CONFIG_T for presets */
    WORD32 i4_quality_preset;

    /** ed_ctxt pointer. This memory is re-used across layers now */
    ihevce_ed_ctxt_t *ps_ed_ctxt;

    ihevce_ed_blk_t *ps_layer1_buf;
    ihevce_ed_blk_t *ps_layer2_buf;
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;

    WORD32 ai4_lambda[MAX_NUM_HME_LAYERS];

    /* pointer to the structure ps_ctb_analyse in pre_enc_me_ctxt_t */
    ctb_analyse_t *ps_ctb_analyse;

    WORD32 i4_enable_noise_detection;

    ihevce_ipe_optimised_function_list_t s_ipe_optimised_function_list;

    ihevce_cmn_opt_func_t s_cmn_opt_func;

} ihevce_decomp_pre_intra_ctxt_t;

/**
******************************************************************************
 *  @brief  Encode loop master context structure
******************************************************************************
*/
typedef struct
{
    /** Array of encode loop structure */
    ihevce_decomp_pre_intra_ctxt_t *aps_decomp_pre_intra_thrd_ctxt[MAX_NUM_FRM_PROC_THRDS_PRE_ENC];

    /** Number of processing threads created run time */
    WORD32 i4_num_proc_thrds;

} ihevce_decomp_pre_intra_master_ctxt_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

#endif /* _IHEVCE_DECOMP_PRE_INTRA_STRUCTS_H_ */
