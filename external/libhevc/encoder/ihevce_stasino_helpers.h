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
*******************************************************************************
* @file
*  ihevce_stasino_helpers.h
*
* @brief
*
*
* @author
*  Ittiam
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef _IHEVCE_STASINO_HELPERS_H_
#define _IHEVCE_STASINO_HELPERS_H_

#include <math.h>
/****************************************************************************/
/* Constant Macros                                                          */
/****************************************************************************/

/****************************************************************************/
/* Function Macros                                                          */
/****************************************************************************/
#define MULTIPLY_STIM_WITH_DISTORTION(dist, stimXalpha, stim_q_level, alpha_q_level)               \
    {                                                                                              \
        ULWORD64 u8_pure_dist = (dist);                                                            \
        WORD32 i4_q_level = stim_q_level + alpha_q_level;                                          \
                                                                                                   \
        u8_pure_dist *= ((1 << (i4_q_level)) - (stimXalpha));                                      \
        u8_pure_dist += (1 << ((i4_q_level)-1));                                                   \
        (dist) = u8_pure_dist >> (i4_q_level);                                                     \
    }

/****************************************************************************/
/* Typedefs                                                                 */
/****************************************************************************/

/****************************************************************************/
/* Enums                                                                    */
/****************************************************************************/

/****************************************************************************/
/* Structure                                                                */
/****************************************************************************/

/****************************************************************************/
/* Function Prototypes                                                      */
/****************************************************************************/

void ihevce_calc_variance(
    void *pv_input,
    WORD32 i4_stride,
    WORD32 *pi4_mean,
    UWORD32 *pu4_variance,
    UWORD8 u1_block_height,
    UWORD8 u1_block_width,
    UWORD8 u1_is_hbd,
    UWORD8 u1_disable_normalization);

void ihevce_calc_variance_signed(
    WORD16 *pv_input,
    WORD32 i4_stride,
    WORD32 *pi4_mean,
    UWORD32 *pu4_variance,
    UWORD8 u1_block_height,
    UWORD8 u1_block_width);

void ihevce_calc_chroma_variance(
    void *pv_input,
    WORD32 i4_stride,
    WORD32 *pi4_mean,
    UWORD32 *pu4_variance,
    UWORD8 u1_block_height,
    UWORD8 u1_block_width,
    UWORD8 u1_is_hbd,
    CHROMA_PLANE_ID_T e_chroma_plane);

static INLINE UWORD32 ihevce_compute_stim(UWORD32 u4_variance1, UWORD32 u4_variance2)
{
    return (u4_variance1 == u4_variance2)
               ? (1 << STIM_Q_FORMAT)
               : ((UWORD32)(
                     ((2 * (double)u4_variance1 * (double)u4_variance2) /
                      (pow((double)u4_variance1, 2) + pow((double)u4_variance2, 2))) *
                     pow((double)2, STIM_Q_FORMAT)));
}

LWORD64 ihevce_inject_stim_into_distortion(
    void *pv_src,
    WORD32 i4_src_stride,
    void *pv_pred,
    WORD32 i4_pred_stride,
    LWORD64 i8_distortion,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_blk_size,
    UWORD8 u1_is_hbd,
    UWORD8 u1_enable_psyRDOPT,
    CHROMA_PLANE_ID_T e_chroma_plane);

static INLINE WORD32 ihevce_derive_noise_weighted_alpha_stim_multiplier(
    WORD32 i4_alpha, UWORD32 u4SrcVar, UWORD32 u4PredVar, WORD32 i4_stim)
{
    (void)u4SrcVar;
    (void)u4PredVar;
    (void)i4_stim;
    return i4_alpha;
}

static INLINE WORD32 ihevce_compute_noise_term(WORD32 i4_alpha, UWORD32 u4SrcVar, UWORD32 u4PredVar)
{
    if(i4_alpha)
    {
        WORD32 i4_stim = ihevce_compute_stim(u4SrcVar, u4PredVar);

        ASSERT(i4_stim >= 0);

        i4_alpha = ihevce_derive_noise_weighted_alpha_stim_multiplier(
            i4_alpha, u4SrcVar, u4PredVar, i4_stim);

        return i4_stim * i4_alpha;
    }
    else
    {
        return 0;
    }
}

UWORD8 ihevce_determine_cu_noise_based_on_8x8Blk_data(
    UWORD8 *pu1_is_8x8Blk_noisy, UWORD8 u1_cu_x_pos, UWORD8 u1_cu_y_pos, UWORD8 u1_cu_size);

LWORD64 ihevce_psy_rd_cost_croma(
    LWORD64 *pui4_source_satd,
    void *p_recon,
    WORD32 recon_stride_vert,
    WORD32 recond_stride_horz,
    WORD32 cu_size_luma,
    WORD32 pic_type,
    WORD32 layer_id,
    WORD32 lambda,
    WORD32 start_index,
    WORD32 is_hbd,
    WORD32 sub_sampling_type,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list

);

LWORD64 ihevce_psy_rd_cost(
    LWORD64 *pui4_source_satd,
    void *pv_recon,
    WORD32 recon_stride_vert,
    WORD32 recond_stride_horz,
    WORD32 cu_size,
    WORD32 pic_type,
    WORD32 layer_id,
    WORD32 lambda,
    WORD32 start_index,
    WORD32 is_hbd,
    UWORD32 u4_psy_strength,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list);

WORD32 ihevce_ctb_noise_detect(
    UWORD8 *pu1_l0_ctb,
    WORD32 l0_stride,
    UWORD8 *pu1_l1_ctb,
    WORD32 l1_stride,
    WORD32 had_block_size,
    WORD32 ctb_width,
    WORD32 ctb_height,
    ihevce_ctb_noise_params *ps_ctb_noise_params,
    WORD32 ctb_height_offset,
    WORD32 ctb_width_offset,
    WORD32 frame_height,
    WORD32 frame_width);

void ihevce_had4_4x4_noise_detect(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst4x4,
    WORD16 *pi2_residue,
    WORD32 dst_strd,
    WORD32 scaling_for_pred);

WORD32 ihevce_had_16x16_r_noise_detect(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 scaling_for_pred);

UWORD32 ihevce_compute_8x8HAD_using_4x4_noise_detect(
    WORD16 *pi2_4x4_had,
    WORD32 had4_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf);

void ihevce_had_8x8_using_4_4x4_noise_detect(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 scaling_for_pred);

unsigned long ihevce_calc_stim_injected_variance(
    ULWORD64 *pu8_sigmaX,
    ULWORD64 *pu8_sigmaXSquared,
    ULWORD64 *u8_var,
    WORD32 i4_inv_wpred_wt,
    WORD32 i4_inv_wt_shift_val,
    WORD32 i4_wpred_log_wdc,
    WORD32 i4_part_id);

unsigned long ihevce_calc_variance_for_diff_weights(
    ULWORD64 *pu8_sigmaX,
    ULWORD64 *pu8_sigmaXSquared,
    ULWORD64 *u8_var,
    WORD32 *pi4_inv_wt,
    WORD32 *pi4_inv_wt_shift_val,
    pu_result_t *ps_result,
    WORD32 i4_wpred_log_wdc,
    PART_ID_T *pe_part_id,
    UWORD8 u1_cu_size,
    UWORD8 u1_num_parts,
    UWORD8 u1_is_for_src);

#endif /* _IHEVCE_STASINO_HELPERS_H_ */
