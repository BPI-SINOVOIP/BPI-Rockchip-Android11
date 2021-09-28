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
* @file ihevce_common_utils.h
*
* @brief
*  Contains the declarations and definitions of common utils for encoder
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_COMMON_UTILS_H_
#define _IHEVCE_COMMON_UTILS_H_

#include <math.h>
#include <limits.h>

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
*  @macro  IHEVCE_WT_PRED
*  @brief Implements wt pred formula as per spec
******************************************************************************
*/
#define IHEVCE_WT_PRED(p0, p1, w0, w1, rnd, shift)                                                 \
    (((((WORD32)w0) * ((WORD32)p0) + ((WORD32)w1) * ((WORD32)p1)) >> shift) + rnd)

#define SORT_PRIMARY_INTTYPE_ARRAY_AND_REORDER_GENERIC_COMPANION_ARRAY(                            \
    primary_array, companion_array, array_length, type_companion)                                  \
    {                                                                                              \
        WORD32 i, j;                                                                               \
                                                                                                   \
        for(i = 0; i < (array_length - 1); i++)                                                    \
        {                                                                                          \
            for(j = (i + 1); j < array_length; j++)                                                \
            {                                                                                      \
                if(primary_array[i] > primary_array[j])                                            \
                {                                                                                  \
                    type_companion t;                                                              \
                                                                                                   \
                    SWAP(primary_array[i], primary_array[j]);                                      \
                                                                                                   \
                    t = companion_array[i];                                                        \
                    companion_array[i] = companion_array[j];                                       \
                    companion_array[j] = t;                                                        \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
    }

#define SORT_PRIMARY_INTTYPE_ARRAY_AND_REORDER_INTTYPE_COMPANION_ARRAY(                            \
    primary_array, companion_array, array_length)                                                  \
    {                                                                                              \
        WORD32 i, j;                                                                               \
                                                                                                   \
        for(i = 0; i < (array_length - 1); i++)                                                    \
        {                                                                                          \
            for(j = (i + 1); j < array_length; j++)                                                \
            {                                                                                      \
                if(primary_array[i] > primary_array[j])                                            \
                {                                                                                  \
                    type_companion t;                                                              \
                                                                                                   \
                    SWAP(primary_array[i], primary_array[j]);                                      \
                    SWAP(companion_array[i], companion_array[j]);                                  \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
    }

#define SORT_INTTYPE_ARRAY(primary_array, array_length)                                            \
    {                                                                                              \
        WORD32 i, j;                                                                               \
                                                                                                   \
        for(i = 0; i < (array_length - 1); i++)                                                    \
        {                                                                                          \
            for(j = (i + 1); j < array_length; j++)                                                \
            {                                                                                      \
                if(primary_array[i] > primary_array[j])                                            \
                {                                                                                  \
                    SWAP(primary_array[i], primary_array[j]);                                      \
                }                                                                                  \
            }                                                                                      \
        }                                                                                          \
    }

#define SET_BIT(x, bitpos) ((x) | (1 << (bitpos)))

#define CLEAR_BIT(x, bitpos) ((x) & (~(1 << (bitpos))))

#define CU_TREE_NODE_FILL(ps_node, valid_flag, posx, posy, size, inter_eval_enable)                \
    {                                                                                              \
        ps_node->is_node_valid = valid_flag;                                                       \
        ps_node->u1_cu_size = size;                                                                \
        ps_node->u1_intra_eval_enable = 0;                                                         \
        ps_node->b3_cu_pos_x = posx;                                                               \
        ps_node->b3_cu_pos_y = posy;                                                               \
        ps_node->u1_inter_eval_enable = inter_eval_enable;                                         \
    }

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_copy_2d(
    UWORD8 *pu1_dst,
    WORD32 dst_stride,
    UWORD8 *pu1_src,
    WORD32 src_stride,
    WORD32 blk_wd,
    WORD32 blk_ht);

void ihevce_2d_square_copy_luma(
    void *p_dst,
    WORD32 dst_strd,
    void *p_src,
    WORD32 src_strd,
    WORD32 num_cols_to_copy,
    WORD32 unit_size);

void ihevce_wt_avg_2d(
    UWORD8 *pu1_pred0,
    UWORD8 *pu1_pred1,
    WORD32 pred0_strd,
    WORD32 pred1_strd,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 w0,
    WORD32 w1,
    WORD32 o0,
    WORD32 o1,
    WORD32 log_wdc);

void ihevce_itrans_recon_dc(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD16 i2_deq_value,
    CHROMA_PLANE_ID_T e_chroma_plane);

WORD32 ihevce_find_num_clusters_of_identical_points_1D(
    UWORD8 *pu1_inp_array,
    UWORD8 *pu1_out_array,
    UWORD8 *pu1_freq_of_out_data_in_inp,
    WORD32 i4_num_inp_array_elements);

void ihevce_scale_mv(mv_t *ps_mv, WORD32 i4_poc_to, WORD32 i4_poc_from, WORD32 i4_curr_poc);

WORD32 ihevce_compare_pu_mv_t(
    pu_mv_t *ps_1, pu_mv_t *ps_2, WORD32 i4_pred_mode_1, WORD32 i4_pred_mode_2);

void ihevce_set_pred_buf_as_free(UWORD32 *pu4_idx_array, UWORD8 u1_buf_id);

UWORD8 ihevce_get_free_pred_buf_indices(
    UWORD8 *pu1_idx_array, UWORD32 *pu4_bitfield, UWORD8 u1_num_bufs_requested);

WORD32 ihevce_osal_init(void *pv_hle_ctxt);

WORD32 ihevce_osal_delete(void *pv_hle_ctxt);

static INLINE UWORD32 ihevce_num_ones_generic(UWORD32 bitfield)
{
    bitfield = bitfield - ((bitfield >> 1) & 0x55555555);
    bitfield = (bitfield & 0x33333333) + ((bitfield >> 2) & 0x33333333);
    return (((bitfield + (bitfield >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

static INLINE UWORD32 ihevce_num_ones_popcnt(UWORD32 bitfield)
{
    return __builtin_popcount(bitfield);
}

WORD32 ihevce_compute_area_of_valid_cus_in_ctb(cur_ctb_cu_tree_t *ps_cu_tree);

void ihevce_cu_tree_init(
    cur_ctb_cu_tree_t *ps_cu_tree,
    cur_ctb_cu_tree_t *ps_cu_tree_root,
    WORD32 *pi4_nodes_created_in_cu_tree,
    WORD32 tree_depth,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos);

#endif /* _IHEVCE_COMMON_UTILS_H_ */
