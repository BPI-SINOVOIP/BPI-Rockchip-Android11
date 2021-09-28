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
* \file ihevce_tu_tree_selector.h
*
* \brief
*    This file contains definitions and declarations used for TU tree selection
*
* \date
*    20/04/2016
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _TU_TREE_SELECTOR_
#define _TU_TREE_SELECTOR_

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/
typedef struct
{
    void *pv_src;

    void *pv_pred;

    void *pv_recon;

    WORD32 i4_src_stride;

    WORD32 i4_pred_stride;

    WORD32 i4_recon_stride;

} src_pred_rec_buf_t;

typedef struct
{
    src_pred_rec_buf_t s_src_pred_rec_buf_luma;

    src_pred_rec_buf_t s_src_pred_rec_buf_chroma;

    nbr_4x4_t *ps_nbr_data_buf;

    WORD16 *pi2_deq_data;

    WORD16 *pi2_deq_data_chroma;

    UWORD8 **ppu1_ecd;

    WORD32 i4_nbr_data_buf_stride;

    WORD32 i4_deq_data_stride;

    WORD32 i4_deq_data_stride_chroma;

} buffer_data_for_tu_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
extern WORD32 ihevce_tu_tree_coverage_in_cu(tu_tree_node_t *ps_node);

extern UWORD16 ihevce_tu_tree_init(
    tu_tree_node_t *ps_root,
    UWORD8 u1_cu_size,
    UWORD8 u1_min_tree_depth,
    UWORD8 u1_max_tree_depth,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422);
#if !ENABLE_TOP_DOWN_TU_RECURSION
extern LWORD64 ihevce_tu_tree_selector(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    tu_tree_node_t *ps_node,
    buffer_data_for_tu_t *ps_buffer_data,
    UWORD8 *pu1_cabac_ctxt,
    WORD32 i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_cur_depth,
    UWORD8 u1_max_depth,
    UWORD8 u1_part_type,
    UWORD8 u1_compute_spatial_ssd);
#endif
extern LWORD64 ihevce_topDown_tu_tree_selector(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    tu_tree_node_t *ps_node,
    buffer_data_for_tu_t *ps_buffer_data,
    UWORD8 *pu1_cabac_ctxt,
    WORD32 i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_cur_depth,
    UWORD8 u1_max_depth,
    UWORD8 u1_part_type,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_compute_spatial_ssd);

extern void ihevce_tu_selector_debriefer(
    tu_tree_node_t *ps_node,
    enc_loop_cu_final_prms_t *ps_final_prms,
    LWORD64 *pi8_total_cost,
    LWORD64 *pi8_total_non_coded_cost,
    WORD32 *pi4_num_bytes_used_for_ecd,
    WORD32 *pi4_num_bits_used_for_encoding,
    UWORD16 *pu2_tu_ctr,
    WORD32 i4_cu_qp,
    UWORD8 u1_cu_posx,
    UWORD8 u1_cu_posy,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422,
    TU_POS_T e_tu_pos);

extern void ihevce_tuSplitArray_to_tuTree_mapper(
    tu_tree_node_t *ps_root,
    WORD32 ai4_tuSplitArray[4],
    UWORD8 u1_cu_size,
    UWORD8 u1_tu_size,
    UWORD8 u1_min_tu_size,
    UWORD8 u1_max_tu_size,
    UWORD8 u1_is_skip);

#endif
