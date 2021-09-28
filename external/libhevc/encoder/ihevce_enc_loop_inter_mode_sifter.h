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
* \file ihevce_enc_loop_inter_mode_sifter.h
*
* \brief
*
* \date
*    11/09/2014
*
* \author
*    Ittiam
*
******************************************************************************
*/
#ifndef _IHEVCE_ENC_LOOP_INTER_MODE_SIFTER
#define _IHEVCE_ENC_LOOP_INTER_MODE_SIFTER

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

#define SKIP_MODE_COST ((DISABLE_SKIP) ? INT_MAX : 1)

#define COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(                                          \
    value_referred, pos_array, neg_array, length_pos_array, length_neg_array, usage_indicator)     \
    {                                                                                              \
        UWORD8 i;                                                                                  \
                                                                                                   \
        UWORD8 num_references = 0;                                                                 \
                                                                                                   \
        for(i = 0; i < length_pos_array; i++)                                                      \
        {                                                                                          \
            num_references += (value_referred == pos_array[i]);                                    \
        }                                                                                          \
                                                                                                   \
        for(i = 0; i < length_neg_array; i++)                                                      \
        {                                                                                          \
            num_references -= (value_referred == neg_array[i]);                                    \
        }                                                                                          \
                                                                                                   \
        if(num_references <= 0)                                                                    \
        {                                                                                          \
            ihevce_set_pred_buf_as_free(usage_indicator, value_referred);                          \
        }                                                                                          \
    }

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

typedef enum
{
    ME_OR_SKIP_DERIVED = 0,
    MERGE_DERIVED = 1,
    MIXED_MODE_TYPE0 = 2,
    MIXED_MODE_TYPE1 = 3

} INTER_CANDIDATE_ID_T;

typedef enum
{
    CLASS1,
    CLASS2,
    CLASS3

} UNIVERSE_CLASS_ID_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    cu_inter_merge_skip_t *ps_cu_inter_merge_skip;

    cu_mixed_mode_inter_t *ps_mixed_modes_datastore;

    cu_inter_cand_t *ps_me_cands;

    inter_cu_mode_info_t *ps_inter_cu_mode_info;

    mv_pred_ctxt_t *ps_mv_pred_ctxt;

    inter_pred_ctxt_t *ps_mc_ctxt;

    WORD32 (*pai4_mv_cost)[NUM_INTER_PU_PARTS];

    WORD32 (*pai4_me_err_metric)[NUM_INTER_PU_PARTS];

    void *pv_src;

    ihevce_inter_pred_buf_data_t *ps_pred_buf_data;

    UWORD8 *pu1_ctb_nbr_map;

    nbr_4x4_t *aps_cu_nbr_buf[2];

    nbr_4x4_t *ps_left_nbr_4x4;

    nbr_4x4_t *ps_top_nbr_4x4;

    nbr_4x4_t *ps_topleft_nbr_4x4;

    cu_me_intra_pred_prms_t *ps_cu_me_intra_pred_prms;

    PF_LUMA_INTER_PRED_PU pf_luma_inter_pred_pu;

    WORD32 i4_ctb_nbr_map_stride;

    WORD32 i4_src_strd;

    WORD32 i4_nbr_4x4_left_strd;

    WORD32 i4_max_num_inter_rdopt_cands;

    WORD32 i4_lambda_qf;

    UWORD8 u1_cu_size;

    UWORD8 u1_cu_pos_x;

    UWORD8 u1_cu_pos_y;

    UWORD8 u1_num_me_cands;

    UWORD8 u1_max_merge_candidates;

    UWORD8 u1_use_satd_for_merge_eval;

    UWORD8 u1_quality_preset;

    WORD8 i1_slice_type;

    UWORD8 u1_is_hbd;

    UWORD8 u1_reuse_me_sad;

    UWORD8 u1_merge_idx_cabac_model;

    UWORD8 u1_use_merge_cand_from_top_row;

    UWORD8 u1_is_cu_noisy;

    WORD32 i4_alpha_stim_multiplier;

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list;

    FT_SAD_EVALUATOR *pf_evalsad_pt_npu_mxn_8bit;

} ihevce_inter_cand_sifter_prms_t;

typedef struct
{
    UWORD8 au1_valid_merge_indices[MAX_NUM_MERGE_CAND];

    merge_cand_list_t *ps_list;

    inter_pred_ctxt_t *ps_mc_ctxt;

    mv_pred_ctxt_t *ps_mv_pred_ctxt;

    PF_LUMA_INTER_PRED_PU pf_luma_inter_pred_pu;

    PF_SAD_FXN_T pf_sad_fxn;

    void **ppv_pred_buf_list;

    UWORD8 *pu1_merge_pred_buf_array;

    UWORD8 (*pau1_best_pred_buf_id)[MAX_NUM_INTER_PARTS];

    UWORD8 *pu1_is_top_used;

    WORD32 (*pai4_noise_term)[MAX_NUM_INTER_PARTS];

    UWORD32 (*pau4_pred_variance)[MAX_NUM_INTER_PARTS];

    UWORD32 *pu4_src_variance;

    WORD32 i4_alpha_stim_multiplier;

    UWORD8 u1_merge_idx_cabac_model;
    WORD32 i4_src_stride;

    WORD32 i4_pred_stride;

    WORD32 i4_lambda;

    UWORD8 u1_max_cands;

    UWORD8 u1_use_merge_cand_from_top_row;

    UWORD8 u1_is_cu_noisy;

    UWORD8 u1_is_hbd;

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list;

} merge_prms_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_inter_cand_sifter(ihevce_inter_cand_sifter_prms_t *ps_ctxt);

#endif /* _IHEVCE_ENC_LOOP_INTER_MODE_SIFTER */
