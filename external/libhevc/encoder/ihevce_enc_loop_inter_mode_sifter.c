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
* \file ihevce_enc_loop_inter_mode_sifter.c
*
* \brief
*    This file contains functions for selecting best inter candidates for RDOPT evaluation
*
* \date
*    10/09/2014
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
#include "ihevc_macros.h"
#include "ihevc_debug.h"
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
#include "ihevce_hle_interface.h"
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
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_decomp_pre_intra_structs.h"
#include "ihevce_decomp_pre_intra_pass.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_nbr_avail.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_cabac_rdo.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_enc_loop_pass.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_common_utils.h"
#include "ihevce_stasino_helpers.h"

#include "hme_datatype.h"
#include "hme_common_defs.h"
#include "hme_common_utils.h"
#include "hme_interface.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_err_compute.h"
#include "hme_globals.h"
#include "ihevce_mv_pred.h"
#include "ihevce_mv_pred_merge.h"
#include "ihevce_inter_pred.h"
#include "ihevce_enc_loop_inter_mode_sifter.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
static WORD32 ihevce_get_num_part_types_in_me_cand_list(
    cu_inter_cand_t *ps_me_cand_list,
    UWORD8 *pu1_part_type_ref_cand,
    UWORD8 *pu1_idx_ref_cand,
    UWORD8 *pu1_diff_skip_cand_flag,
    WORD8 *pi1_skip_cand_from_merge_idx,
    WORD8 *pi1_final_skip_cand_merge_idx,
    UWORD8 u1_max_num_part_types_to_select,
    UWORD8 u1_num_me_cands)
{
    UWORD8 i, j;
    UWORD8 u1_num_unique_parts = 0;

    for(i = 0; i < u1_num_me_cands; i++)
    {
        UWORD8 u1_cur_part_type = ps_me_cand_list[i].b3_part_size;
        UWORD8 u1_is_unique = 1;

        if(u1_num_unique_parts >= u1_max_num_part_types_to_select)
        {
            return u1_num_unique_parts;
        }

        /* loop to check if the current cand is already present in the list */
        for(j = 0; j < u1_num_unique_parts; j++)
        {
            if(u1_cur_part_type == pu1_part_type_ref_cand[j])
            {
                u1_is_unique = 0;
                break;
            }
        }

        if(u1_is_unique)
        {
            if(SIZE_2Nx2N == u1_cur_part_type)
            {
                *pu1_diff_skip_cand_flag = 0;
                *pi1_skip_cand_from_merge_idx = u1_num_unique_parts;
                *pi1_final_skip_cand_merge_idx = u1_num_unique_parts;
            }

            pu1_part_type_ref_cand[u1_num_unique_parts] = u1_cur_part_type;
            pu1_idx_ref_cand[u1_num_unique_parts] = i;
            u1_num_unique_parts++;
        }
    }

    return u1_num_unique_parts;
}

static WORD32 ihevce_compute_inter_pred_and_cost(
    inter_pred_ctxt_t *ps_mc_ctxt,
    PF_LUMA_INTER_PRED_PU pf_luma_inter_pred_pu,
    PF_SAD_FXN_T pf_sad_func,
    pu_t *ps_pu,
    void *pv_src,
    void *pv_pred,
    WORD32 i4_src_stride,
    WORD32 i4_pred_stride,
    UWORD8 u1_compute_error,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list)
{
    IV_API_CALL_STATUS_T u1_is_valid_mv;
    WORD32 i4_error;

    u1_is_valid_mv = pf_luma_inter_pred_pu(ps_mc_ctxt, ps_pu, pv_pred, i4_pred_stride, 0);

    if(u1_compute_error)
    {
        if(IV_SUCCESS == u1_is_valid_mv)
        {
            err_prms_t s_err_prms;

            s_err_prms.i4_blk_ht = (ps_pu->b4_ht + 1) << 2;
            s_err_prms.i4_blk_wd = (ps_pu->b4_wd + 1) << 2;
            s_err_prms.pu1_inp = (UWORD8 *)pv_src;
            s_err_prms.pu2_inp = (UWORD16 *)pv_src;
            s_err_prms.pu1_ref = (UWORD8 *)pv_pred;
            s_err_prms.pu2_ref = (UWORD16 *)pv_pred;
            s_err_prms.i4_inp_stride = i4_src_stride;
            s_err_prms.i4_ref_stride = i4_pred_stride;
            s_err_prms.pi4_sad_grid = &i4_error;

            s_err_prms.ps_cmn_utils_optimised_function_list = ps_cmn_utils_optimised_function_list;

            pf_sad_func(&s_err_prms);
        }
        else
        {
            /* max 32 bit satd */
            i4_error = INT_MAX;
        }

        return i4_error;
    }

    return INT_MAX;
}

static WORD32 ihevce_determine_best_merge_pu(
    merge_prms_t *ps_prms,
    pu_t *ps_pu_merge,
    pu_t *ps_pu_me,
    void *pv_src,
    WORD32 i4_me_cand_cost,
    WORD32 i4_pred_buf_offset,
    UWORD8 u1_num_cands,
    UWORD8 u1_part_id,
    UWORD8 u1_force_pred_evaluation)
{
    pu_t *ps_pu;

    INTER_CANDIDATE_ID_T e_cand_id;

    UWORD8 i;
    UWORD8 u1_best_pred_mode;
    WORD32 i4_mean;
    UWORD32 u4_cur_variance, u4_best_variance;

    merge_cand_list_t *ps_list = ps_prms->ps_list;
    inter_pred_ctxt_t *ps_mc_ctxt = ps_prms->ps_mc_ctxt;
    PF_LUMA_INTER_PRED_PU pf_luma_inter_pred_pu = ps_prms->pf_luma_inter_pred_pu;
    PF_SAD_FXN_T pf_sad_fxn = ps_prms->pf_sad_fxn;

    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list =
        ps_prms->ps_cmn_utils_optimised_function_list;

    WORD32(*pai4_noise_term)[MAX_NUM_INTER_PARTS] = ps_prms->pai4_noise_term;
    UWORD32(*pau4_pred_variance)[MAX_NUM_INTER_PARTS] = ps_prms->pau4_pred_variance;
    WORD32 i4_alpha_stim_multiplier = ps_prms->i4_alpha_stim_multiplier;
    UWORD32 *pu4_src_variance = ps_prms->pu4_src_variance;
    UWORD8 u1_is_cu_noisy = ps_prms->u1_is_cu_noisy;
    UWORD8 u1_is_hbd = ps_prms->u1_is_hbd;
    UWORD8 *pu1_valid_merge_indices = ps_prms->au1_valid_merge_indices;
    void **ppv_pred_buf_list = ps_prms->ppv_pred_buf_list;
    UWORD8 *pu1_merge_pred_buf_array = ps_prms->pu1_merge_pred_buf_array;
    UWORD8(*pau1_best_pred_buf_id)[MAX_NUM_INTER_PARTS] = ps_prms->pau1_best_pred_buf_id;
    UWORD8 u1_merge_idx_cabac_model = ps_prms->u1_merge_idx_cabac_model;
    WORD32 i4_lambda = ps_prms->i4_lambda;
    WORD32 i4_src_stride = ps_prms->i4_src_stride;
    WORD32 i4_pred_stride = ps_prms->i4_pred_stride;
    UWORD8 u1_max_cands = ps_prms->u1_max_cands;
    UWORD8 u1_best_buf_id = pu1_merge_pred_buf_array[0];
    UWORD8 u1_cur_buf_id = pu1_merge_pred_buf_array[1];
    UWORD8 u1_best_cand_id = UCHAR_MAX;
    WORD32 i4_best_cost = INT_MAX;
    WORD32 i4_cur_noise_term = 0;
    WORD32 i4_best_noise_term = 0;

    ps_pu = ps_pu_merge;
    e_cand_id = MERGE_DERIVED;

    ASSERT(ps_pu->b1_merge_flag);

    for(i = 0; i < u1_num_cands; i++)
    {
        WORD32 i4_cur_cost;

        void *pv_pred = (UWORD8 *)ppv_pred_buf_list[u1_cur_buf_id] + i4_pred_buf_offset;
        UWORD8 u1_is_pred_available = 0;

        if(!ps_prms->u1_use_merge_cand_from_top_row && ps_prms->pu1_is_top_used[i])
        {
            continue;
        }

        ps_pu->mv = ps_list[i].mv;
        ps_pu->b3_merge_idx = pu1_valid_merge_indices[i];

        /* set the prediction mode */
        if(ps_list[i].u1_pred_flag_l0 && ps_list[i].u1_pred_flag_l1)
        {
            ps_pu->b2_pred_mode = PRED_BI;
        }
        else if(ps_list[i].u1_pred_flag_l0)
        {
            ps_pu->b2_pred_mode = PRED_L0;
        }
        else
        {
            ps_pu->b2_pred_mode = PRED_L1;
        }

        /* 8x8 SMPs should not have bipred mode as per std */
        {
            WORD32 i4_part_wd, i4_part_ht;

            i4_part_wd = (ps_pu->b4_wd + 1) << 2;
            i4_part_ht = (ps_pu->b4_ht + 1) << 2;

            if((PRED_BI == ps_pu->b2_pred_mode) && ((i4_part_wd + i4_part_ht) < 16))
            {
                continue;
            }
        }

        if((!u1_force_pred_evaluation) &&
           (ihevce_compare_pu_mv_t(
               &ps_pu->mv, &ps_pu_me->mv, ps_pu->b2_pred_mode, ps_pu_me->b2_pred_mode)))
        {
            i4_cur_cost = i4_me_cand_cost;
            u1_is_pred_available = 1;

            if((i4_cur_cost < INT_MAX) && u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
                i4_cur_noise_term = pai4_noise_term[ME_OR_SKIP_DERIVED][u1_part_id];
                u4_cur_variance = pau4_pred_variance[ME_OR_SKIP_DERIVED][u1_part_id];
            }
        }
        else
        {
            i4_cur_cost = ihevce_compute_inter_pred_and_cost(
                ps_mc_ctxt,
                pf_luma_inter_pred_pu,
                pf_sad_fxn,
                ps_pu,
                pv_src,
                pv_pred,
                i4_src_stride,
                i4_pred_stride,
                1,
                ps_cmn_utils_optimised_function_list);

            if((i4_cur_cost < INT_MAX) && u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
                ihevce_calc_variance(
                    pv_pred,
                    i4_pred_stride,
                    &i4_mean,
                    &u4_cur_variance,
                    (ps_pu->b4_ht + 1) << 2,
                    (ps_pu->b4_wd + 1) << 2,
                    u1_is_hbd,
                    0);

                i4_cur_noise_term = ihevce_compute_noise_term(
                    i4_alpha_stim_multiplier, pu4_src_variance[u1_part_id], u4_cur_variance);

                MULTIPLY_STIM_WITH_DISTORTION(
                    i4_cur_cost, i4_cur_noise_term, STIM_Q_FORMAT, ALPHA_Q_FORMAT);
            }
        }

        if(i4_cur_cost < INT_MAX)
        {
            WORD32 i4_merge_idx_cost = 0;
            COMPUTE_MERGE_IDX_COST(
                u1_merge_idx_cabac_model, i, u1_max_cands, i4_lambda, i4_merge_idx_cost);
            i4_cur_cost += i4_merge_idx_cost;
        }

        if(i4_cur_cost < i4_best_cost)
        {
            i4_best_cost = i4_cur_cost;

            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
                i4_best_noise_term = i4_cur_noise_term;
                u4_best_variance = u4_cur_variance;
            }

            u1_best_cand_id = i;
            u1_best_pred_mode = ps_pu->b2_pred_mode;

            if(u1_is_pred_available)
            {
                pau1_best_pred_buf_id[e_cand_id][u1_part_id] =
                    pau1_best_pred_buf_id[ME_OR_SKIP_DERIVED][u1_part_id];
            }
            else
            {
                SWAP(u1_best_buf_id, u1_cur_buf_id);
                pau1_best_pred_buf_id[e_cand_id][u1_part_id] = u1_best_buf_id;
            }
        }
    }

    if(u1_best_cand_id != UCHAR_MAX)
    {
        ps_pu->mv = ps_list[u1_best_cand_id].mv;
        ps_pu->b2_pred_mode = u1_best_pred_mode;
        ps_pu->b3_merge_idx = pu1_valid_merge_indices[u1_best_cand_id];

        if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
        {
            pai4_noise_term[MERGE_DERIVED][u1_part_id] = i4_best_noise_term;
            pau4_pred_variance[MERGE_DERIVED][u1_part_id] = u4_best_variance;
        }
    }

    return i4_best_cost;
}

static WORD8 ihevce_merge_cand_pred_buffer_preparation(
    void **ppv_pred_buf_list,
    cu_inter_cand_t *ps_cand,
    UWORD8 (*pau1_final_pred_buf_id)[MAX_NUM_INTER_PARTS],
    WORD32 i4_pred_stride,
    UWORD8 u1_cu_size,
    UWORD8 u1_part_type,
    UWORD8 u1_num_bytes_per_pel,
    FT_COPY_2D *pf_copy_2d)
{
    WORD32 i4_part_wd;
    WORD32 i4_part_ht;
    WORD32 i4_part_wd_pu2;
    WORD32 i4_part_ht_pu2;
    WORD32 i4_buf_offset;
    UWORD8 *pu1_pred_src;
    UWORD8 *pu1_pred_dst;
    WORD8 i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][0];

    WORD32 i4_stride = i4_pred_stride * u1_num_bytes_per_pel;

    if((0 == u1_part_type) ||
       (pau1_final_pred_buf_id[MERGE_DERIVED][0] == pau1_final_pred_buf_id[MERGE_DERIVED][1]))
    {
        ps_cand->pu1_pred_data =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
        ps_cand->pu2_pred_data =
            (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
        ps_cand->i4_pred_data_stride = i4_pred_stride;

        i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][0];
    }
    else if(pau1_final_pred_buf_id[MERGE_DERIVED][0] == pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0])
    {
        i4_part_wd = (ps_cand->as_inter_pu[0].b4_wd + 1) << 2;
        i4_part_ht = (ps_cand->as_inter_pu[0].b4_ht + 1) << 2;

        i4_buf_offset = 0;

        pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]] +
                       i4_buf_offset;
        pu1_pred_dst =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]] + i4_buf_offset;

        pf_copy_2d(
            pu1_pred_dst,
            i4_stride,
            pu1_pred_src,
            i4_stride,
            i4_part_wd * u1_num_bytes_per_pel,
            i4_part_ht);

        ps_cand->pu1_pred_data =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];
        ps_cand->pu2_pred_data =
            (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];
        ps_cand->i4_pred_data_stride = i4_pred_stride;

        i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][1];
    }
    else if(pau1_final_pred_buf_id[MERGE_DERIVED][1] == pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1])
    {
        i4_part_wd = (ps_cand->as_inter_pu[0].b4_wd + 1) << 2;
        i4_part_ht = (ps_cand->as_inter_pu[0].b4_ht + 1) << 2;

        i4_buf_offset = (i4_part_ht < u1_cu_size) * i4_part_ht * i4_pred_stride +
                        (i4_part_wd < u1_cu_size) * i4_part_wd;

        i4_buf_offset *= u1_num_bytes_per_pel;

        i4_part_wd = (ps_cand->as_inter_pu[1].b4_wd + 1) << 2;
        i4_part_ht = (ps_cand->as_inter_pu[1].b4_ht + 1) << 2;

        pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]] +
                       i4_buf_offset;
        pu1_pred_dst =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]] + i4_buf_offset;

        pf_copy_2d(
            pu1_pred_dst,
            i4_stride,
            pu1_pred_src,
            i4_stride,
            i4_part_wd * u1_num_bytes_per_pel,
            i4_part_ht);

        ps_cand->pu1_pred_data =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
        ps_cand->pu2_pred_data =
            (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
        ps_cand->i4_pred_data_stride = i4_pred_stride;

        i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][0];
    }
    else
    {
        i4_part_wd = (ps_cand->as_inter_pu[0].b4_wd + 1) << 2;
        i4_part_ht = (ps_cand->as_inter_pu[0].b4_ht + 1) << 2;

        i4_part_wd_pu2 = (ps_cand->as_inter_pu[1].b4_wd + 1) << 2;
        i4_part_ht_pu2 = (ps_cand->as_inter_pu[1].b4_ht + 1) << 2;

        switch((PART_TYPE_T)u1_part_type)
        {
        case PRT_2NxN:
        case PRT_Nx2N:
        case PRT_2NxnU:
        case PRT_nLx2N:
        {
            pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
            pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];

            ps_cand->pu1_pred_data =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];
            ps_cand->pu2_pred_data =
                (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];

            i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][1];

            break;
        }
        case PRT_nRx2N:
        case PRT_2NxnD:
        {
            i4_buf_offset = (i4_part_ht < u1_cu_size) * i4_part_ht * i4_pred_stride +
                            (i4_part_wd < u1_cu_size) * i4_part_wd;

            i4_buf_offset *= u1_num_bytes_per_pel;

            pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]] +
                           i4_buf_offset;
            pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]] +
                           i4_buf_offset;

            i4_part_wd = i4_part_wd_pu2;
            i4_part_ht = i4_part_ht_pu2;

            ps_cand->pu1_pred_data =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
            ps_cand->pu2_pred_data =
                (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];

            i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][0];

            break;
        }
        }

        pf_copy_2d(
            pu1_pred_dst,
            i4_stride,
            pu1_pred_src,
            i4_stride,
            i4_part_wd * u1_num_bytes_per_pel,
            i4_part_ht);

        ps_cand->i4_pred_data_stride = i4_pred_stride;
    }

    return i1_retval;
}

static WORD8 ihevce_mixed_mode_cand_type1_pred_buffer_preparation(
    void **ppv_pred_buf_list,
    cu_inter_cand_t *ps_cand,
    UWORD8 (*pau1_final_pred_buf_id)[MAX_NUM_INTER_PARTS],
    UWORD8 *pu1_merge_pred_buf_idx_array,
    WORD32 i4_pred_stride,
    UWORD8 u1_me_pred_buf_id,
    UWORD8 u1_merge_pred_buf_id,
    UWORD8 u1_type0_cand_is_valid,
    UWORD8 u1_cu_size,
    UWORD8 u1_part_type,
    UWORD8 u1_num_bytes_per_pel,
    FT_COPY_2D *pf_copy_2d)
{
    WORD32 i4_part_wd;
    WORD32 i4_part_ht;
    WORD32 i4_part_wd_pu2;
    WORD32 i4_part_ht_pu2;
    UWORD8 *pu1_pred_src;
    UWORD8 *pu1_pred_dst = NULL;
    WORD8 i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];

    WORD32 i4_stride = i4_pred_stride * u1_num_bytes_per_pel;

    ASSERT(0 != u1_part_type);

    i4_part_wd = (ps_cand->as_inter_pu[0].b4_wd + 1) << 2;
    i4_part_ht = (ps_cand->as_inter_pu[0].b4_ht + 1) << 2;

    i4_part_wd_pu2 = (ps_cand->as_inter_pu[1].b4_wd + 1) << 2;
    i4_part_ht_pu2 = (ps_cand->as_inter_pu[1].b4_ht + 1) << 2;

    if(pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1] == pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1])
    {
        ps_cand->pu1_pred_data =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
        ps_cand->pu2_pred_data =
            (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
        ps_cand->i4_pred_data_stride = i4_pred_stride;

        i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];

        return i1_retval;
    }
    else
    {
        UWORD8 u1_bitfield = ((u1_merge_pred_buf_id == UCHAR_MAX) << 3) |
                             ((u1_me_pred_buf_id == UCHAR_MAX) << 2) |
                             ((!u1_type0_cand_is_valid) << 1) |
                             (pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1] ==
                              pau1_final_pred_buf_id[MERGE_DERIVED][1]);

        WORD32 i4_buf_offset = (i4_part_ht < u1_cu_size) * i4_part_ht * i4_pred_stride +
                               (i4_part_wd < u1_cu_size) * i4_part_wd;

        i4_buf_offset *= u1_num_bytes_per_pel;

        switch(u1_bitfield)
        {
        case 15:
        case 14:
        case 6:
        {
            switch((PART_TYPE_T)u1_part_type)
            {
            case PRT_2NxN:
            case PRT_Nx2N:
            case PRT_2NxnU:
            case PRT_nLx2N:
            {
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
                pu1_pred_dst =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1]];

                i1_retval = pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1];

                break;
            }
            case PRT_nRx2N:
            case PRT_2NxnD:
            {
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1]] +
                    i4_buf_offset;
                pu1_pred_dst =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]] +
                    i4_buf_offset;

                i4_part_wd = i4_part_wd_pu2;
                i4_part_ht = i4_part_ht_pu2;

                i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];

                break;
            }
            }

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd * u1_num_bytes_per_pel,
                i4_part_ht);

            break;
        }
        case 13:
        case 9:
        case 5:
        {
            UWORD8 i;

            for(i = 0; i < 3; i++)
            {
                if((pu1_merge_pred_buf_idx_array[i] != pau1_final_pred_buf_id[MERGE_DERIVED][1]) &&
                   (pu1_merge_pred_buf_idx_array[i] != pau1_final_pred_buf_id[MERGE_DERIVED][0]))
                {
                    pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[pu1_merge_pred_buf_idx_array[i]] +
                                   i4_buf_offset;

                    i1_retval = pu1_merge_pred_buf_idx_array[i];

                    break;
                }
            }

            pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]] +
                           i4_buf_offset;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd_pu2 * u1_num_bytes_per_pel,
                i4_part_ht_pu2);
            /* Copy PU1 */
            pu1_pred_src =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
            pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[i1_retval];

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd * u1_num_bytes_per_pel,
                i4_part_ht);

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            break;
        }
        case 12:
        case 10:
        case 8:
        case 4:
        case 2:
        case 0:
        {
            pu1_pred_src =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
            pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1]];

            i1_retval = pau1_final_pred_buf_id[MIXED_MODE_TYPE1][1];

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd * u1_num_bytes_per_pel,
                i4_part_ht);

            break;
        }
        case 11:
        {
            pu1_pred_src =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
            pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];

            i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][1];

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd * u1_num_bytes_per_pel,
                i4_part_ht);

            break;
        }
        case 7:
        {
            pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]] +
                           i4_buf_offset;
            pu1_pred_dst =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]] +
                i4_buf_offset;

            i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1];

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd_pu2 * u1_num_bytes_per_pel,
                i4_part_ht_pu2);

            break;
        }
        case 3:
        case 1:
        {
            if((u1_merge_pred_buf_id == pau1_final_pred_buf_id[MERGE_DERIVED][0]) &&
               (u1_merge_pred_buf_id != pau1_final_pred_buf_id[MERGE_DERIVED][1]))
            {
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
                pu1_pred_dst =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]];

                i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][1];

                ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
                ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
                ps_cand->i4_pred_data_stride = i4_pred_stride;

                pf_copy_2d(
                    pu1_pred_dst,
                    i4_stride,
                    pu1_pred_src,
                    i4_stride,
                    i4_part_wd * u1_num_bytes_per_pel,
                    i4_part_ht);
            }
            else
            {
                UWORD8 i;

                for(i = 0; i < 3; i++)
                {
                    if((pu1_merge_pred_buf_idx_array[i] !=
                        pau1_final_pred_buf_id[MERGE_DERIVED][1]) &&
                       (pu1_merge_pred_buf_idx_array[i] !=
                        pau1_final_pred_buf_id[MERGE_DERIVED][0]))
                    {
                        pu1_pred_dst =
                            (UWORD8 *)ppv_pred_buf_list[pu1_merge_pred_buf_idx_array[i]] +
                            i4_buf_offset;

                        i1_retval = pu1_merge_pred_buf_idx_array[i];

                        break;
                    }
                }

                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][1]] +
                    i4_buf_offset;

                pf_copy_2d(
                    pu1_pred_dst,
                    i4_stride,
                    pu1_pred_src,
                    i4_stride,
                    i4_part_wd_pu2 * u1_num_bytes_per_pel,
                    i4_part_ht_pu2);

                /* Copy PU1 */
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
                pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[i1_retval];

                pf_copy_2d(
                    pu1_pred_dst,
                    i4_stride,
                    pu1_pred_src,
                    i4_stride,
                    i4_part_wd * u1_num_bytes_per_pel,
                    i4_part_ht);

                ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
                ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
                ps_cand->i4_pred_data_stride = i4_pred_stride;

                break;
            }
        }
        }
    }

    return i1_retval;
}

static WORD8 ihevce_mixed_mode_cand_type0_pred_buffer_preparation(
    void **ppv_pred_buf_list,
    cu_inter_cand_t *ps_cand,
    UWORD8 (*pau1_final_pred_buf_id)[MAX_NUM_INTER_PARTS],
    UWORD8 *pu1_merge_pred_buf_idx_array,
    UWORD8 u1_me_pred_buf_id,
    UWORD8 u1_merge_pred_buf_id,
    UWORD8 u1_mixed_tyep1_pred_buf_id,
    WORD32 i4_pred_stride,
    UWORD8 u1_cu_size,
    UWORD8 u1_part_type,
    UWORD8 u1_num_bytes_per_pel,
    FT_COPY_2D *pf_copy_2d)
{
    WORD32 i4_part_wd;
    WORD32 i4_part_ht;
    WORD32 i4_part_wd_pu2;
    WORD32 i4_part_ht_pu2;
    WORD32 i4_buf_offset;
    UWORD8 *pu1_pred_src;
    UWORD8 *pu1_pred_dst = NULL;
    WORD8 i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];

    WORD32 i4_stride = i4_pred_stride * u1_num_bytes_per_pel;

    ASSERT(0 != u1_part_type);

    i4_part_wd = (ps_cand->as_inter_pu[0].b4_wd + 1) << 2;
    i4_part_ht = (ps_cand->as_inter_pu[0].b4_ht + 1) << 2;
    i4_part_wd_pu2 = (ps_cand->as_inter_pu[1].b4_wd + 1) << 2;
    i4_part_ht_pu2 = (ps_cand->as_inter_pu[1].b4_ht + 1) << 2;

    i4_buf_offset = (i4_part_ht < u1_cu_size) * i4_part_ht * i4_pred_stride +
                    (i4_part_wd < u1_cu_size) * i4_part_wd;

    i4_buf_offset *= u1_num_bytes_per_pel;

    if(pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0] == pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0])
    {
        ps_cand->pu1_pred_data =
            (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
        ps_cand->pu2_pred_data =
            (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
        ps_cand->i4_pred_data_stride = i4_pred_stride;

        i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];
    }
    else
    {
        UWORD8 u1_bitfield =
            ((u1_merge_pred_buf_id == UCHAR_MAX) << 2) | ((u1_me_pred_buf_id == UCHAR_MAX) << 1) |
            (u1_mixed_tyep1_pred_buf_id != pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]);

        switch(u1_bitfield)
        {
        case 7:
        {
            switch((PART_TYPE_T)u1_part_type)
            {
            case PRT_2NxN:
            case PRT_Nx2N:
            case PRT_2NxnU:
            case PRT_nLx2N:
            {
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0]];
                pu1_pred_dst =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]];

                i1_retval = pau1_final_pred_buf_id[MIXED_MODE_TYPE0][1];

                break;
            }
            case PRT_nRx2N:
            case PRT_2NxnD:
            {
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]] +
                    i4_buf_offset;
                pu1_pred_dst =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0]] +
                    i4_buf_offset;

                i4_part_wd = i4_part_wd_pu2;
                i4_part_ht = i4_part_ht_pu2;

                i1_retval = pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0];

                break;
            }
            }

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd * u1_num_bytes_per_pel,
                i4_part_ht);

            break;
        }
        case 6:
        case 5:
        case 4:
        {
            pu1_pred_src =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]] +
                i4_buf_offset;
            pu1_pred_dst =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0]] +
                i4_buf_offset;

            i1_retval = pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0];

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd_pu2 * u1_num_bytes_per_pel,
                i4_part_ht_pu2);
            break;
        }
        case 3:
        {
            pu1_pred_src = (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MIXED_MODE_TYPE0][0]];
            pu1_pred_dst =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]];

            i1_retval = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1];

            ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
            ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
            ps_cand->i4_pred_data_stride = i4_pred_stride;

            pf_copy_2d(
                pu1_pred_dst,
                i4_stride,
                pu1_pred_src,
                i4_stride,
                i4_part_wd * u1_num_bytes_per_pel,
                i4_part_ht);

            break;
        }
        case 2:
        case 1:
        case 0:
        {
            if((u1_merge_pred_buf_id == pau1_final_pred_buf_id[MERGE_DERIVED][1]) &&
               (u1_merge_pred_buf_id != pau1_final_pred_buf_id[MERGE_DERIVED][0]))
            {
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]] +
                    i4_buf_offset;
                pu1_pred_dst =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]] +
                    i4_buf_offset;

                i1_retval = pau1_final_pred_buf_id[MERGE_DERIVED][0];

                ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
                ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
                ps_cand->i4_pred_data_stride = i4_pred_stride;

                pf_copy_2d(
                    pu1_pred_dst,
                    i4_stride,
                    pu1_pred_src,
                    i4_stride,
                    i4_part_wd_pu2 * u1_num_bytes_per_pel,
                    i4_part_ht_pu2);
            }
            else
            {
                UWORD8 i;

                for(i = 0; i < 3; i++)
                {
                    if((pu1_merge_pred_buf_idx_array[i] != u1_merge_pred_buf_id) &&
                       (pu1_merge_pred_buf_idx_array[i] != u1_mixed_tyep1_pred_buf_id))
                    {
                        pu1_pred_dst =
                            (UWORD8 *)ppv_pred_buf_list[pu1_merge_pred_buf_idx_array[i]] +
                            i4_buf_offset;

                        i1_retval = pu1_merge_pred_buf_idx_array[i];

                        break;
                    }
                }

                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][1]] +
                    i4_buf_offset;

                pf_copy_2d(
                    pu1_pred_dst,
                    i4_stride,
                    pu1_pred_src,
                    i4_stride,
                    i4_part_wd_pu2 * u1_num_bytes_per_pel,
                    i4_part_ht_pu2);

                /* Copy PU1 */
                pu1_pred_src =
                    (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[MERGE_DERIVED][0]];
                pu1_pred_dst = (UWORD8 *)ppv_pred_buf_list[i1_retval];

                pf_copy_2d(
                    pu1_pred_dst,
                    i4_stride,
                    pu1_pred_src,
                    i4_stride,
                    i4_part_wd * u1_num_bytes_per_pel,
                    i4_part_ht);

                ps_cand->pu1_pred_data = (UWORD8 *)ppv_pred_buf_list[i1_retval];
                ps_cand->pu2_pred_data = (UWORD16 *)ppv_pred_buf_list[i1_retval];
                ps_cand->i4_pred_data_stride = i4_pred_stride;

                break;
            }
        }
        }
    }

    return i1_retval;
}

static UWORD8 ihevce_find_idx_of_worst_cost(UWORD32 *pu4_cost_array, UWORD8 u1_array_size)
{
    WORD32 i;

    UWORD8 u1_worst_cost_idx = 0;

    for(i = 1; i < u1_array_size; i++)
    {
        if(pu4_cost_array[i] > pu4_cost_array[u1_worst_cost_idx])
        {
            u1_worst_cost_idx = i;
        }
    }

    return u1_worst_cost_idx;
}

static void ihevce_free_unused_buf_indices(
    UWORD32 *pu4_pred_buf_usage_indicator,
    UWORD8 *pu1_merge_pred_buf_idx_array,
    UWORD8 *pu1_buf_id_in_use,
    UWORD8 *pu1_buf_id_to_free,
    UWORD8 u1_me_buf_id,
    UWORD8 u1_num_available_cands,
    UWORD8 u1_num_bufs_to_free,
    UWORD8 u1_eval_merge,
    UWORD8 u1_eval_skip,
    UWORD8 u1_part_type)
{
    UWORD8 i;

    if(u1_eval_skip)
    {
        if(pu1_buf_id_in_use[ME_OR_SKIP_DERIVED] == pu1_merge_pred_buf_idx_array[0])
        {
            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_merge_pred_buf_idx_array[1]);
        }
        else if(pu1_buf_id_in_use[ME_OR_SKIP_DERIVED] == pu1_merge_pred_buf_idx_array[1])
        {
            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_merge_pred_buf_idx_array[0]);
        }
        else
        {
            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_merge_pred_buf_idx_array[0]);

            ihevce_set_pred_buf_as_free(
                pu4_pred_buf_usage_indicator, pu1_merge_pred_buf_idx_array[1]);
        }

        for(i = 0; i < u1_num_bufs_to_free; i++)
        {
            if(pu1_buf_id_to_free[i] != u1_me_buf_id)
            {
                ihevce_set_pred_buf_as_free(pu4_pred_buf_usage_indicator, pu1_buf_id_to_free[i]);
            }
        }
    }
    else if((!u1_eval_merge) && (!u1_eval_skip) && (pu1_buf_id_in_use[ME_OR_SKIP_DERIVED] == UCHAR_MAX))
    {
        ihevce_set_pred_buf_as_free(pu4_pred_buf_usage_indicator, u1_me_buf_id);

        for(i = 0; i < u1_num_bufs_to_free; i++)
        {
            if(pu1_buf_id_to_free[i] != u1_me_buf_id)
            {
                ihevce_set_pred_buf_as_free(pu4_pred_buf_usage_indicator, pu1_buf_id_to_free[i]);
            }
        }
    }
    else if((!u1_eval_merge) && (!u1_eval_skip) && (pu1_buf_id_in_use[ME_OR_SKIP_DERIVED] != UCHAR_MAX))
    {
        for(i = 0; i < u1_num_bufs_to_free; i++)
        {
            if(pu1_buf_id_to_free[i] != u1_me_buf_id)
            {
                ihevce_set_pred_buf_as_free(pu4_pred_buf_usage_indicator, pu1_buf_id_to_free[i]);
            }
        }
    }
    else if((u1_eval_merge) && (0 == u1_part_type))
    {
        /* ME pred buf */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            u1_me_buf_id,
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        /* Merge pred buf 0 */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            pu1_merge_pred_buf_idx_array[0],
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        /* Merge pred buf 1 */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            pu1_merge_pred_buf_idx_array[1],
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        for(i = 0; i < u1_num_bufs_to_free; i++)
        {
            if((pu1_buf_id_to_free[i] != u1_me_buf_id) &&
               (pu1_merge_pred_buf_idx_array[0] != pu1_buf_id_to_free[i]) &&
               (pu1_merge_pred_buf_idx_array[1] != pu1_buf_id_to_free[i]))
            {
                ihevce_set_pred_buf_as_free(pu4_pred_buf_usage_indicator, pu1_buf_id_to_free[i]);
            }
        }
    }
    else if((u1_eval_merge) || (u1_eval_skip))
    {
        /* ME pred buf */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            u1_me_buf_id,
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        /* Merge pred buf 0 */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            pu1_merge_pred_buf_idx_array[0],
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        /* Merge pred buf 1 */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            pu1_merge_pred_buf_idx_array[1],
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        /* Merge pred buf 2 */
        COMPUTE_NUM_POSITIVE_REFERENCES_AND_FREE_IF_ZERO(
            pu1_merge_pred_buf_idx_array[2],
            pu1_buf_id_in_use,
            pu1_buf_id_to_free,
            4,
            u1_num_bufs_to_free,
            pu4_pred_buf_usage_indicator);

        for(i = 0; i < u1_num_bufs_to_free; i++)
        {
            if((pu1_buf_id_to_free[i] != u1_me_buf_id) &&
               (pu1_merge_pred_buf_idx_array[0] != pu1_buf_id_to_free[i]) &&
               (pu1_merge_pred_buf_idx_array[1] != pu1_buf_id_to_free[i]))
            {
                ihevce_set_pred_buf_as_free(pu4_pred_buf_usage_indicator, pu1_buf_id_to_free[i]);
            }
        }
    }
}

static UWORD8 ihevce_check_if_buf_can_be_freed(
    UWORD8 *pu1_pred_id_of_winners,
    UWORD8 u1_idx_of_worst_cost_in_pred_buf_array,
    UWORD8 u1_num_cands_previously_added)
{
    UWORD8 i;

    UWORD8 u1_num_trysts = 0;

    for(i = 0; i < u1_num_cands_previously_added; i++)
    {
        if(u1_idx_of_worst_cost_in_pred_buf_array == pu1_pred_id_of_winners[i])
        {
            u1_num_trysts++;

            if(u1_num_trysts > 1)
            {
                return 0;
            }
        }
    }

    ASSERT(u1_num_trysts > 0);

    return 1;
}

static void ihevce_get_worst_costs_and_indices(
    UWORD32 *pu4_cost_src,
    UWORD32 *pu4_cost_dst,
    UWORD8 *pu1_worst_dst_cand_idx,
    UWORD8 u1_src_array_length,
    UWORD8 u1_num_cands_to_pick,
    UWORD8 u1_worst_cost_idx_in_dst_array)
{
    WORD32 i;

    pu4_cost_dst[0] = pu4_cost_src[u1_worst_cost_idx_in_dst_array];
    pu4_cost_src[u1_worst_cost_idx_in_dst_array] = 0;
    pu1_worst_dst_cand_idx[0] = u1_worst_cost_idx_in_dst_array;

    for(i = 1; i < u1_num_cands_to_pick; i++)
    {
        pu1_worst_dst_cand_idx[i] =
            ihevce_find_idx_of_worst_cost(pu4_cost_src, u1_src_array_length);

        pu4_cost_dst[i] = pu4_cost_src[pu1_worst_dst_cand_idx[i]];
        pu4_cost_src[pu1_worst_dst_cand_idx[i]] = 0;
    }

    for(i = 0; i < u1_num_cands_to_pick; i++)
    {
        pu4_cost_src[pu1_worst_dst_cand_idx[i]] = pu4_cost_dst[i];
    }
}

static UWORD8 ihevce_select_cands_to_replace_previous_worst(
    UWORD32 *pu4_cost_src,
    UWORD32 *pu4_cost_dst,
    INTER_CANDIDATE_ID_T *pe_cand_id,
    UWORD8 *pu1_cand_idx_in_dst_array,
    UWORD8 *pu1_buf_id_to_free,
    UWORD8 *pu1_pred_id_of_winners,
    UWORD8 *pu1_num_bufs_to_free,
    WORD32 i4_max_num_inter_rdopt_cands,
    UWORD8 u1_num_cands_previously_added,
    UWORD8 u1_num_available_cands,
    UWORD8 u1_worst_cost_idx_in_dst_array)
{
    WORD32 i, j, k;
    UWORD32 au4_worst_dst_costs[4];
    UWORD8 au1_worst_dst_cand_idx[4];

    INTER_CANDIDATE_ID_T ae_default_cand_id[4] = {
        ME_OR_SKIP_DERIVED, MERGE_DERIVED, MIXED_MODE_TYPE1, MIXED_MODE_TYPE0
    };

    UWORD8 u1_num_cands_to_add_wo_comparisons =
        i4_max_num_inter_rdopt_cands - u1_num_cands_previously_added;
    UWORD8 u1_num_cands_to_add_after_comparisons =
        u1_num_available_cands - u1_num_cands_to_add_wo_comparisons;
    UWORD8 u1_num_cands_to_add = 0;
    UWORD8 au1_valid_src_cands[4] = { 0, 0, 0, 0 };

    ASSERT(u1_num_cands_to_add_after_comparisons >= 0);

    /* Sorting src costs */
    SORT_PRIMARY_INTTYPE_ARRAY_AND_REORDER_GENERIC_COMPANION_ARRAY(
        pu4_cost_src, pe_cand_id, u1_num_available_cands, INTER_CANDIDATE_ID_T);

    for(i = 0; i < u1_num_cands_to_add_wo_comparisons; i++)
    {
        pu1_cand_idx_in_dst_array[u1_num_cands_to_add++] = u1_num_cands_previously_added + i;
        au1_valid_src_cands[pe_cand_id[i]] = 1;
    }

    if(u1_num_cands_previously_added)
    {
        WORD8 i1_last_index = 0;

        ihevce_get_worst_costs_and_indices(
            pu4_cost_dst,
            au4_worst_dst_costs,
            au1_worst_dst_cand_idx,
            u1_num_cands_previously_added,
            u1_num_cands_to_add_after_comparisons,
            u1_worst_cost_idx_in_dst_array);

        for(i = u1_num_available_cands - 1; i >= u1_num_cands_to_add_wo_comparisons; i--)
        {
            for(j = u1_num_cands_to_add_after_comparisons - 1; j >= i1_last_index; j--)
            {
                if((pu4_cost_src[i] < au4_worst_dst_costs[j]))
                {
                    if((i - u1_num_cands_to_add_wo_comparisons) <= j)
                    {
                        for(k = 0; k <= (i - u1_num_cands_to_add_wo_comparisons); k++)
                        {
                            pu1_cand_idx_in_dst_array[u1_num_cands_to_add++] =
                                au1_worst_dst_cand_idx[k];
                            au1_valid_src_cands[pe_cand_id[u1_num_cands_to_add_wo_comparisons + k]] =
                                1;

                            if(1 == ihevce_check_if_buf_can_be_freed(
                                        pu1_pred_id_of_winners,
                                        pu1_pred_id_of_winners[au1_worst_dst_cand_idx[k]],
                                        u1_num_cands_previously_added))
                            {
                                pu1_buf_id_to_free[(*pu1_num_bufs_to_free)++] =
                                    pu1_pred_id_of_winners[au1_worst_dst_cand_idx[k]];
                            }
                            else
                            {
                                pu1_pred_id_of_winners[au1_worst_dst_cand_idx[k]] = UCHAR_MAX;
                            }
                        }

                        i1_last_index = -1;
                    }
                    else
                    {
                        i1_last_index = j;
                    }

                    break;
                }
            }

            if(-1 == i1_last_index)
            {
                break;
            }
        }
    }

    for(i = 0, j = 0; i < u1_num_available_cands; i++)
    {
        if(au1_valid_src_cands[ae_default_cand_id[i]])
        {
            pe_cand_id[j++] = ae_default_cand_id[i];
        }
    }

    return u1_num_cands_to_add;
}

static UWORD8 ihevce_merge_cands_with_existing_best(
    inter_cu_mode_info_t *ps_mode_info,
    cu_inter_cand_t **pps_cand_src,
    pu_mv_t (*pas_mvp_winner)[NUM_INTER_PU_PARTS],
    UWORD32 (*pau4_cost)[MAX_NUM_INTER_PARTS],
    void **ppv_pred_buf_list,
    UWORD8 (*pau1_final_pred_buf_id)[MAX_NUM_INTER_PARTS],
    UWORD32 *pu4_pred_buf_usage_indicator,
    UWORD8 *pu1_num_merge_cands,
    UWORD8 *pu1_num_skip_cands,
    UWORD8 *pu1_num_mixed_mode_type0_cands,
    UWORD8 *pu1_num_mixed_mode_type1_cands,
    UWORD8 *pu1_merge_pred_buf_idx_array,

    FT_COPY_2D *pf_copy_2d,

    WORD32 i4_pred_stride,
    WORD32 i4_max_num_inter_rdopt_cands,
    UWORD8 u1_cu_size,
    UWORD8 u1_part_type,
    UWORD8 u1_eval_merge,
    UWORD8 u1_eval_skip,
    UWORD8 u1_num_bytes_per_pel)
{
    UWORD32 au4_cost_src[4];
    WORD32 i;
    WORD32 u1_num_available_cands;
    UWORD8 au1_buf_id_in_use[4];
    UWORD8 au1_buf_id_to_free[4];
    UWORD8 au1_cand_idx_in_dst_array[4];

    INTER_CANDIDATE_ID_T ae_cand_id[4] = {
        ME_OR_SKIP_DERIVED, MERGE_DERIVED, MIXED_MODE_TYPE1, MIXED_MODE_TYPE0
    };

    cu_inter_cand_t **pps_cand_dst = ps_mode_info->aps_cu_data;

    UWORD8 u1_num_cands_previously_added = ps_mode_info->u1_num_inter_cands;
    UWORD8 u1_worst_cost_idx = ps_mode_info->u1_idx_of_worst_cost_in_cost_array;
    UWORD8 u1_idx_of_worst_cost_in_pred_buf_array =
        ps_mode_info->u1_idx_of_worst_cost_in_pred_buf_array;
    UWORD32 *pu4_cost_dst = ps_mode_info->au4_cost;
    UWORD8 *pu1_pred_id_of_winners = ps_mode_info->au1_pred_buf_idx;
    UWORD8 u1_num_bufs_to_free = 0;
    UWORD8 u1_skip_or_merge_cand_is_valid = 0;
    UWORD8 u1_num_invalid_cands = 0;

    memset(au1_buf_id_in_use, UCHAR_MAX, sizeof(au1_buf_id_in_use));

    u1_num_available_cands = (u1_eval_merge) ? 2 + ((u1_part_type != 0) + 1) : 1;

    for(i = 0; i < u1_num_available_cands; i++)
    {
        WORD32 i4_idx = i - u1_num_invalid_cands;

        if(u1_part_type == 0)
        {
            au4_cost_src[i4_idx] = pau4_cost[ae_cand_id[i4_idx]][0];
        }
        else
        {
            au4_cost_src[i4_idx] =
                pau4_cost[ae_cand_id[i4_idx]][0] + pau4_cost[ae_cand_id[i4_idx]][1];
        }

        if(au4_cost_src[i4_idx] >= INT_MAX)
        {
            memmove(
                &ae_cand_id[i4_idx],
                &ae_cand_id[i4_idx + 1],
                sizeof(INTER_CANDIDATE_ID_T) * (u1_num_available_cands - i - 1));

            u1_num_invalid_cands++;
        }
    }

    u1_num_available_cands -= u1_num_invalid_cands;

    if((u1_num_cands_previously_added + u1_num_available_cands) > i4_max_num_inter_rdopt_cands)
    {
        u1_num_available_cands = ihevce_select_cands_to_replace_previous_worst(
            au4_cost_src,
            pu4_cost_dst,
            ae_cand_id,
            au1_cand_idx_in_dst_array,
            au1_buf_id_to_free,
            pu1_pred_id_of_winners,
            &u1_num_bufs_to_free,
            i4_max_num_inter_rdopt_cands,
            u1_num_cands_previously_added,
            u1_num_available_cands,
            u1_worst_cost_idx);
    }
    else
    {
        for(i = 0; i < u1_num_available_cands; i++)
        {
            au1_cand_idx_in_dst_array[i] = u1_num_cands_previously_added + i;
        }
    }

    for(i = 0; i < u1_num_available_cands; i++)
    {
        UWORD8 u1_dst_array_idx = au1_cand_idx_in_dst_array[i];

        if(u1_part_type == 0)
        {
            au4_cost_src[i] = pau4_cost[ae_cand_id[i]][0];
        }
        else
        {
            au4_cost_src[i] = pau4_cost[ae_cand_id[i]][0] + pau4_cost[ae_cand_id[i]][1];
        }

        pps_cand_dst[u1_dst_array_idx] = pps_cand_src[ae_cand_id[i]];

        /* Adding a skip candidate identical to the merge winner */
        if((u1_eval_merge) && (0 == u1_part_type) && (MIXED_MODE_TYPE1 == ae_cand_id[i]))
        {
            (*pu1_num_skip_cands)++;

            pu4_cost_dst[u1_dst_array_idx] = au4_cost_src[i];

            if(u1_num_cands_previously_added >= i4_max_num_inter_rdopt_cands)
            {
                u1_worst_cost_idx =
                    ihevce_find_idx_of_worst_cost(pu4_cost_dst, u1_num_cands_previously_added);

                u1_idx_of_worst_cost_in_pred_buf_array = pu1_pred_id_of_winners[u1_worst_cost_idx];
            }
            else
            {
                u1_num_cands_previously_added++;
            }

            if(u1_skip_or_merge_cand_is_valid)
            {
                pps_cand_dst[u1_dst_array_idx]->pu1_pred_data =
                    (UWORD8 *)ppv_pred_buf_list[au1_buf_id_in_use[MERGE_DERIVED]];
                pps_cand_dst[u1_dst_array_idx]->pu2_pred_data =
                    (UWORD16 *)ppv_pred_buf_list[au1_buf_id_in_use[MERGE_DERIVED]];
                pps_cand_dst[u1_dst_array_idx]->i4_pred_data_stride = i4_pred_stride;

                au1_buf_id_in_use[MIXED_MODE_TYPE1] = au1_buf_id_in_use[MERGE_DERIVED];
                pu1_pred_id_of_winners[u1_dst_array_idx] = au1_buf_id_in_use[MERGE_DERIVED];
            }
            else
            {
                u1_skip_or_merge_cand_is_valid = 1;

                au1_buf_id_in_use[MIXED_MODE_TYPE1] = ihevce_merge_cand_pred_buffer_preparation(
                    ppv_pred_buf_list,
                    pps_cand_dst[u1_dst_array_idx],
                    pau1_final_pred_buf_id,
                    i4_pred_stride,
                    u1_cu_size,
                    u1_part_type,
                    u1_num_bytes_per_pel,
                    pf_copy_2d);

                pu1_pred_id_of_winners[u1_dst_array_idx] = au1_buf_id_in_use[MIXED_MODE_TYPE1];
            }

            continue;
        }

        if(u1_num_cands_previously_added < i4_max_num_inter_rdopt_cands)
        {
            if(u1_num_cands_previously_added)
            {
                if(au4_cost_src[i] > pu4_cost_dst[u1_worst_cost_idx])
                {
                    u1_worst_cost_idx = u1_num_cands_previously_added;
                }
            }

            pu4_cost_dst[u1_dst_array_idx] = au4_cost_src[i];

            u1_num_cands_previously_added++;
        }
        else
        {
            pu4_cost_dst[u1_dst_array_idx] = au4_cost_src[i];

            u1_worst_cost_idx = ihevce_find_idx_of_worst_cost(
                ps_mode_info->au4_cost, u1_num_cands_previously_added);

            u1_idx_of_worst_cost_in_pred_buf_array = pu1_pred_id_of_winners[u1_worst_cost_idx];
        }

        switch(ae_cand_id[i])
        {
        case ME_OR_SKIP_DERIVED:
        {
            (*pu1_num_skip_cands) += u1_eval_skip;

            pps_cand_dst[u1_dst_array_idx]->pu1_pred_data =
                (UWORD8 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
            pps_cand_dst[u1_dst_array_idx]->pu2_pred_data =
                (UWORD16 *)ppv_pred_buf_list[pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0]];
            pps_cand_dst[u1_dst_array_idx]->i4_pred_data_stride = i4_pred_stride;

            if(u1_worst_cost_idx == u1_dst_array_idx)
            {
                u1_idx_of_worst_cost_in_pred_buf_array =
                    pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];
            }

            u1_skip_or_merge_cand_is_valid = u1_eval_skip;

            au1_buf_id_in_use[ME_OR_SKIP_DERIVED] = pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];
            pu1_pred_id_of_winners[u1_dst_array_idx] =
                pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0];

            break;
        }
        case MERGE_DERIVED:
        {
            (*pu1_num_merge_cands)++;

            au1_buf_id_in_use[MERGE_DERIVED] = ihevce_merge_cand_pred_buffer_preparation(
                ppv_pred_buf_list,
                pps_cand_dst[u1_dst_array_idx],
                pau1_final_pred_buf_id,
                i4_pred_stride,
                u1_cu_size,
                u1_part_type,
                u1_num_bytes_per_pel,
                pf_copy_2d

            );

            pu1_pred_id_of_winners[u1_dst_array_idx] = au1_buf_id_in_use[MERGE_DERIVED];

            if(u1_worst_cost_idx == u1_dst_array_idx)
            {
                u1_idx_of_worst_cost_in_pred_buf_array = au1_buf_id_in_use[MERGE_DERIVED];
            }

            u1_skip_or_merge_cand_is_valid = 1;

            break;
        }
        case MIXED_MODE_TYPE1:
        {
            (*pu1_num_mixed_mode_type1_cands)++;

            au1_buf_id_in_use[MIXED_MODE_TYPE1] =
                ihevce_mixed_mode_cand_type1_pred_buffer_preparation(
                    ppv_pred_buf_list,
                    pps_cand_dst[u1_dst_array_idx],
                    pau1_final_pred_buf_id,
                    pu1_merge_pred_buf_idx_array,
                    i4_pred_stride,
                    au1_buf_id_in_use[ME_OR_SKIP_DERIVED],
                    au1_buf_id_in_use[MERGE_DERIVED],
                    (u1_num_available_cands - i) > 1,
                    u1_cu_size,
                    u1_part_type,
                    u1_num_bytes_per_pel,
                    pf_copy_2d

                );

            pu1_pred_id_of_winners[u1_dst_array_idx] = au1_buf_id_in_use[MIXED_MODE_TYPE1];

            if(u1_worst_cost_idx == u1_dst_array_idx)
            {
                u1_idx_of_worst_cost_in_pred_buf_array = au1_buf_id_in_use[MIXED_MODE_TYPE1];
            }

            break;
        }
        case MIXED_MODE_TYPE0:
        {
            (*pu1_num_mixed_mode_type0_cands)++;

            au1_buf_id_in_use[MIXED_MODE_TYPE0] =
                ihevce_mixed_mode_cand_type0_pred_buffer_preparation(
                    ppv_pred_buf_list,
                    pps_cand_dst[u1_dst_array_idx],
                    pau1_final_pred_buf_id,
                    pu1_merge_pred_buf_idx_array,
                    au1_buf_id_in_use[ME_OR_SKIP_DERIVED],
                    au1_buf_id_in_use[MERGE_DERIVED],
                    au1_buf_id_in_use[MIXED_MODE_TYPE1],
                    i4_pred_stride,
                    u1_cu_size,
                    u1_part_type,
                    u1_num_bytes_per_pel,
                    pf_copy_2d);

            pu1_pred_id_of_winners[u1_dst_array_idx] = au1_buf_id_in_use[MIXED_MODE_TYPE0];

            if(u1_worst_cost_idx == u1_dst_array_idx)
            {
                u1_idx_of_worst_cost_in_pred_buf_array = au1_buf_id_in_use[MIXED_MODE_TYPE0];
            }

            break;
        }
        }
    }

    ihevce_free_unused_buf_indices(
        pu4_pred_buf_usage_indicator,
        pu1_merge_pred_buf_idx_array,
        au1_buf_id_in_use,
        au1_buf_id_to_free,
        pau1_final_pred_buf_id[ME_OR_SKIP_DERIVED][0],
        u1_num_available_cands,
        u1_num_bufs_to_free,
        u1_eval_merge,
        u1_eval_skip,
        u1_part_type);

    ps_mode_info->u1_idx_of_worst_cost_in_cost_array = u1_worst_cost_idx;
    ps_mode_info->u1_num_inter_cands = u1_num_cands_previously_added;
    ps_mode_info->u1_idx_of_worst_cost_in_pred_buf_array = u1_idx_of_worst_cost_in_pred_buf_array;

    return u1_skip_or_merge_cand_is_valid;
}

static UWORD8 ihevce_prepare_cand_containers(
    ihevce_inter_cand_sifter_prms_t *ps_ctxt,
    cu_inter_cand_t **pps_cands,
    UWORD8 *pu1_merge_pred_buf_idx_array,
    UWORD8 *pu1_me_pred_buf_idx,
    UWORD8 u1_part_type,
    UWORD8 u1_me_cand_list_idx,
    UWORD8 u1_eval_merge,
    UWORD8 u1_eval_skip)
{
    UWORD8 u1_num_bufs_currently_allocated;

    WORD32 i4_pred_stride = ps_ctxt->ps_pred_buf_data->i4_pred_stride;
    UWORD8 u1_cu_size = ps_ctxt->u1_cu_size;
    UWORD8 u1_cu_pos_x = ps_ctxt->u1_cu_pos_x;
    UWORD8 u1_cu_pos_y = ps_ctxt->u1_cu_pos_y;
    void **ppv_pred_buf_list = ps_ctxt->ps_pred_buf_data->apv_inter_pred_data;

    if(!u1_eval_merge)
    {
        if(u1_eval_skip)
        {
            u1_num_bufs_currently_allocated = ihevce_get_free_pred_buf_indices(
                pu1_merge_pred_buf_idx_array, &ps_ctxt->ps_pred_buf_data->u4_is_buf_in_use, 2);

            if(u1_num_bufs_currently_allocated < 2)
            {
                return 0;
            }

            pps_cands[ME_OR_SKIP_DERIVED] =
                &ps_ctxt->ps_cu_inter_merge_skip->as_cu_inter_merge_skip_cand
                     [MAX_NUM_CU_MERGE_SKIP_CAND - 1 -
                      ps_ctxt->ps_cu_inter_merge_skip->u1_num_skip_cands];

            pps_cands[ME_OR_SKIP_DERIVED]->b1_skip_flag = 1;
            pps_cands[ME_OR_SKIP_DERIVED]->b1_eval_mark = 1;
            pps_cands[ME_OR_SKIP_DERIVED]->b1_eval_tx_cusize = 1;
            pps_cands[ME_OR_SKIP_DERIVED]->b1_eval_tx_cusize_by2 = 1;
            pps_cands[ME_OR_SKIP_DERIVED]->b1_intra_has_won = 0;
            pps_cands[ME_OR_SKIP_DERIVED]->b3_part_size = 0;
            pps_cands[ME_OR_SKIP_DERIVED]->i4_pred_data_stride = i4_pred_stride;
            pps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu->b1_intra_flag = 0;
            pps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu->b1_merge_flag = 1;
            pps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu->b4_pos_x = u1_cu_pos_x >> 2;
            pps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu->b4_pos_y = u1_cu_pos_y >> 2;
            pps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu->b4_wd = (u1_cu_size >> 2) - 1;
            pps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu->b4_ht = (u1_cu_size >> 2) - 1;

            pps_cands[MERGE_DERIVED] = pps_cands[ME_OR_SKIP_DERIVED];
        }
        else
        {
            u1_num_bufs_currently_allocated = ihevce_get_free_pred_buf_indices(
                pu1_me_pred_buf_idx, &ps_ctxt->ps_pred_buf_data->u4_is_buf_in_use, 1);

            if(u1_num_bufs_currently_allocated < 1)
            {
                return 0;
            }

            pps_cands[ME_OR_SKIP_DERIVED] = &ps_ctxt->ps_me_cands[u1_me_cand_list_idx];
            pps_cands[ME_OR_SKIP_DERIVED]->i4_pred_data_stride = i4_pred_stride;
            pps_cands[ME_OR_SKIP_DERIVED]->pu1_pred_data =
                (UWORD8 *)ppv_pred_buf_list[*pu1_me_pred_buf_idx];
            pps_cands[ME_OR_SKIP_DERIVED]->pu2_pred_data =
                (UWORD16 *)ppv_pred_buf_list[*pu1_me_pred_buf_idx];
        }
    }
    else
    {
        u1_num_bufs_currently_allocated = ihevce_get_free_pred_buf_indices(
            pu1_me_pred_buf_idx, &ps_ctxt->ps_pred_buf_data->u4_is_buf_in_use, 1);

        if(u1_num_bufs_currently_allocated < 1)
        {
            return 0;
        }

        pps_cands[ME_OR_SKIP_DERIVED] = &ps_ctxt->ps_me_cands[u1_me_cand_list_idx];

        if(u1_part_type > 0)
        {
            u1_num_bufs_currently_allocated = ihevce_get_free_pred_buf_indices(
                pu1_merge_pred_buf_idx_array, &ps_ctxt->ps_pred_buf_data->u4_is_buf_in_use, 3);

            if(u1_num_bufs_currently_allocated < 3)
            {
                return 0;
            }

            pps_cands[MERGE_DERIVED] = &ps_ctxt->ps_cu_inter_merge_skip->as_cu_inter_merge_skip_cand
                                            [ps_ctxt->ps_cu_inter_merge_skip->u1_num_merge_cands];

            pps_cands[MIXED_MODE_TYPE0] =
                &ps_ctxt->ps_mixed_modes_datastore
                     ->as_cu_data[ps_ctxt->ps_mixed_modes_datastore->u1_num_mixed_mode_type0_cands];

            pps_cands[MIXED_MODE_TYPE1] =
                &ps_ctxt->ps_mixed_modes_datastore->as_cu_data
                     [MAX_NUM_MIXED_MODE_INTER_RDO_CANDS - 1 -
                      ps_ctxt->ps_mixed_modes_datastore->u1_num_mixed_mode_type1_cands];

            *pps_cands[MERGE_DERIVED] = *pps_cands[ME_OR_SKIP_DERIVED];
            *pps_cands[MIXED_MODE_TYPE0] = *pps_cands[ME_OR_SKIP_DERIVED];
            *pps_cands[MIXED_MODE_TYPE1] = *pps_cands[ME_OR_SKIP_DERIVED];
        }
        else
        {
            u1_num_bufs_currently_allocated = ihevce_get_free_pred_buf_indices(
                pu1_merge_pred_buf_idx_array, &ps_ctxt->ps_pred_buf_data->u4_is_buf_in_use, 2);

            if(u1_num_bufs_currently_allocated < 2)
            {
                return 0;
            }

            pps_cands[MERGE_DERIVED] = &ps_ctxt->ps_cu_inter_merge_skip->as_cu_inter_merge_skip_cand
                                            [ps_ctxt->ps_cu_inter_merge_skip->u1_num_merge_cands];

            *pps_cands[MERGE_DERIVED] = *pps_cands[ME_OR_SKIP_DERIVED];
        }

        pps_cands[MERGE_DERIVED]->as_inter_pu[0].b1_merge_flag = 1;
        pps_cands[MERGE_DERIVED]->as_inter_pu[1].b1_merge_flag = 1;
    }

    return u1_num_bufs_currently_allocated;
}

static __inline void ihevce_merge_prms_init(
    merge_prms_t *ps_prms,
    merge_cand_list_t *ps_list,
    inter_pred_ctxt_t *ps_mc_ctxt,
    mv_pred_ctxt_t *ps_mv_pred_ctxt,
    PF_LUMA_INTER_PRED_PU pf_luma_inter_pred_pu,
    PF_SAD_FXN_T pf_sad_fxn,
    void **ppv_pred_buf_list,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    UWORD8 *pu1_merge_pred_buf_array,
    UWORD8 (*pau1_best_pred_buf_id)[MAX_NUM_INTER_PARTS],
    UWORD8 *pu1_is_top_used,
    WORD32 (*pai4_noise_term)[MAX_NUM_INTER_PARTS],
    UWORD32 (*pau4_pred_variance)[MAX_NUM_INTER_PARTS],
    UWORD32 *pu4_src_variance,
    WORD32 i4_alpha_stim_multiplier,
    WORD32 i4_src_stride,
    WORD32 i4_pred_stride,
    WORD32 i4_lambda,
    UWORD8 u1_is_cu_noisy,
    UWORD8 u1_is_hbd,
    UWORD8 u1_max_cands,
    UWORD8 u1_merge_idx_cabac_model,
    UWORD8 u1_use_merge_cand_from_top_row)
{
    ps_prms->ps_list = ps_list;
    ps_prms->ps_mc_ctxt = ps_mc_ctxt;
    ps_prms->ps_mv_pred_ctxt = ps_mv_pred_ctxt;
    ps_prms->pf_luma_inter_pred_pu = pf_luma_inter_pred_pu;
    ps_prms->pf_sad_fxn = pf_sad_fxn;
    ps_prms->ppv_pred_buf_list = ppv_pred_buf_list;
    ps_prms->ps_cmn_utils_optimised_function_list = ps_cmn_utils_optimised_function_list;

    ps_prms->pu1_merge_pred_buf_array = pu1_merge_pred_buf_array;
    ps_prms->pau1_best_pred_buf_id = pau1_best_pred_buf_id;
    ps_prms->pu1_is_top_used = pu1_is_top_used;
    ps_prms->pai4_noise_term = pai4_noise_term;
    ps_prms->pau4_pred_variance = pau4_pred_variance;
    ps_prms->pu4_src_variance = pu4_src_variance;
    ps_prms->i4_alpha_stim_multiplier = i4_alpha_stim_multiplier;
    ps_prms->i4_src_stride = i4_src_stride;
    ps_prms->i4_pred_stride = i4_pred_stride;
    ps_prms->i4_lambda = i4_lambda;
    ps_prms->u1_is_cu_noisy = u1_is_cu_noisy;
    ps_prms->u1_is_hbd = u1_is_hbd;
    ps_prms->u1_max_cands = u1_max_cands;
    ps_prms->u1_merge_idx_cabac_model = u1_merge_idx_cabac_model;
    ps_prms->u1_use_merge_cand_from_top_row = u1_use_merge_cand_from_top_row;
}

static UWORD8 ihevce_merge_candidate_seive(
    nbr_avail_flags_t *ps_nbr,
    merge_cand_list_t *ps_merge_cand,
    UWORD8 *pu1_is_top_used,
    UWORD8 u1_num_merge_cands,
    UWORD8 u1_use_merge_cand_from_top_row)
{
    if(!u1_use_merge_cand_from_top_row)
    {
        if(ps_nbr->u1_bot_lt_avail || ps_nbr->u1_left_avail)
        {
            return !pu1_is_top_used[0];
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return u1_num_merge_cands;
    }
}

static UWORD8 ihevce_compute_pred_and_populate_modes(
    ihevce_inter_cand_sifter_prms_t *ps_ctxt,
    PF_SAD_FXN_T pf_sad_func,
    UWORD32 *pu4_src_variance,
    UWORD8 u1_part_type,
    UWORD8 u1_me_cand_list_idx,
    UWORD8 u1_eval_merge,
    UWORD8 u1_eval_skip)
{
    cu_inter_cand_t *aps_cands[4];
    pu_mv_t as_mvp_winner[4][NUM_INTER_PU_PARTS];
    merge_prms_t s_merge_prms;
    merge_cand_list_t as_merge_cand[MAX_NUM_MERGE_CAND];

    UWORD8 i, j;
    UWORD32 au4_cost[4][NUM_INTER_PU_PARTS];
    UWORD8 au1_final_pred_buf_id[4][NUM_INTER_PU_PARTS];
    UWORD8 au1_merge_pred_buf_idx_array[3];
    UWORD8 au1_is_top_used[MAX_NUM_MERGE_CAND];
    UWORD8 u1_me_pred_buf_idx;
    UWORD8 u1_num_bufs_currently_allocated;
    WORD32 i4_mean;
    UWORD32 au4_pred_variance[4][NUM_INTER_PU_PARTS];
    WORD32 ai4_noise_term[4][NUM_INTER_PU_PARTS];

    UWORD8 u1_cu_pos_x = ps_ctxt->u1_cu_pos_x;
    UWORD8 u1_cu_pos_y = ps_ctxt->u1_cu_pos_y;

    inter_cu_mode_info_t *ps_cu_mode_info = ps_ctxt->ps_inter_cu_mode_info;
    inter_pred_ctxt_t *ps_mc_ctxt = ps_ctxt->ps_mc_ctxt;
    nbr_4x4_t *ps_cu_nbr_buf = ps_ctxt->aps_cu_nbr_buf[0];
    nbr_4x4_t *ps_pu_left_nbr = ps_ctxt->ps_left_nbr_4x4;
    nbr_4x4_t *ps_pu_top_nbr = ps_ctxt->ps_top_nbr_4x4;
    nbr_4x4_t *ps_pu_topleft_nbr = ps_ctxt->ps_topleft_nbr_4x4;

    ihevce_inter_pred_buf_data_t *ps_pred_buf_info = ps_ctxt->ps_pred_buf_data;
    mv_pred_ctxt_t *ps_mv_pred_ctxt = ps_ctxt->ps_mv_pred_ctxt;

    PF_LUMA_INTER_PRED_PU pf_luma_inter_pred_pu = ps_ctxt->pf_luma_inter_pred_pu;

    void *pv_src = ps_ctxt->pv_src;
    WORD32 i4_src_stride = ps_ctxt->i4_src_strd;
    WORD32 i4_pred_stride = ps_ctxt->ps_pred_buf_data->i4_pred_stride;
    UWORD8 u1_num_parts = (u1_part_type != PRT_2Nx2N) + 1;
    UWORD8 u1_num_bytes_per_pel = ps_ctxt->u1_is_hbd + 1;
    void **ppv_pred_buf_list = ps_ctxt->ps_pred_buf_data->apv_inter_pred_data;
    UWORD8 u1_cu_size = ps_ctxt->u1_cu_size;
    WORD32 i4_nbr_4x4_left_stride = ps_ctxt->i4_nbr_4x4_left_strd;
    UWORD8 *pu1_ctb_nbr_map = ps_ctxt->pu1_ctb_nbr_map;
    WORD32 i4_nbr_map_stride = ps_ctxt->i4_ctb_nbr_map_stride;
    UWORD8 u1_max_merge_candidates = ps_ctxt->u1_max_merge_candidates;
    WORD32 i4_max_num_inter_rdopt_cands = ps_ctxt->i4_max_num_inter_rdopt_cands;
    WORD32 i4_pred_buf_offset = 0;
    WORD32 i4_src_buf_offset = 0;
    UWORD8 u1_single_mcl_flag =
        ((8 == u1_cu_size) && (ps_mv_pred_ctxt->i4_log2_parallel_merge_level_minus2 > 0));
    UWORD8 u1_skip_or_merge_cand_is_valid = 0;
    WORD32 i4_lambda_qf = ps_ctxt->i4_lambda_qf;
    UWORD8 u1_is_cu_noisy = ps_ctxt->u1_is_cu_noisy;

    ASSERT(0 == (u1_eval_skip && u1_eval_merge));
    ASSERT(u1_me_cand_list_idx < ps_ctxt->u1_num_me_cands);

    /*
    Algorithm -
    1. Determine pred and satd for ME cand.
    2. Determine merge winner for PU1.
    3. Determine pred and satd for mixed_type0 cand.
    4. Determine merge winner for PU2 and hence derive pred and satd for merge cand.
    5. Determine merge winner for PU2 assuming ME cand as PU1 winner and hence derive
    pred and satd for mixed_type1 cand.
    6. Sort the 4 preceding costs and hence, the cand list.
    7. Merge the sorted lists with the final cand list.

    PS : 2 - 7 will be relevant only if u1_eval_merge = 1 and u1_eval_skip = 0
    PPS : 1 will not be relevant if u1_eval_skip = 1
    */

    /*
    Explanatory notes -
    1. Motion Vector Merge candidates and nbr's in all merge mode (RealD)
    2. Motion Vector Merge candidates and nbr's in mixed mode (AltD)
    */

    u1_num_bufs_currently_allocated = ihevce_prepare_cand_containers(
        ps_ctxt,
        aps_cands,
        au1_merge_pred_buf_idx_array,
        &u1_me_pred_buf_idx,
        u1_part_type,
        u1_me_cand_list_idx,
        u1_eval_merge,
        u1_eval_skip);

    if(0 == u1_num_bufs_currently_allocated)
    {
        return 0;
    }

    if((u1_eval_merge) || (u1_eval_skip))
    {
        ihevce_merge_prms_init(
            &s_merge_prms,
            as_merge_cand,
            ps_mc_ctxt,
            ps_mv_pred_ctxt,
            pf_luma_inter_pred_pu,
            pf_sad_func,
            ppv_pred_buf_list,
            ps_ctxt->ps_cmn_utils_optimised_function_list,
            au1_merge_pred_buf_idx_array,
            au1_final_pred_buf_id,
            au1_is_top_used,
            ai4_noise_term,
            au4_pred_variance,
            pu4_src_variance,
            ps_ctxt->i4_alpha_stim_multiplier,
            i4_src_stride,
            i4_pred_stride,
            i4_lambda_qf,
            u1_is_cu_noisy,
            ps_ctxt->u1_is_hbd,
            u1_max_merge_candidates,
            ps_ctxt->u1_merge_idx_cabac_model,
            ps_ctxt->u1_use_merge_cand_from_top_row);
    }

    for(i = 0; i < u1_num_parts; i++)
    {
        nbr_avail_flags_t s_nbr;

        UWORD8 u1_part_wd;
        UWORD8 u1_part_ht;
        UWORD8 u1_pu_pos_x_4x4;
        UWORD8 u1_pu_pos_y_4x4;

        pu_t *ps_pu = &aps_cands[MERGE_DERIVED]->as_inter_pu[i];

        PART_SIZE_E e_part_size = (PART_SIZE_E)aps_cands[ME_OR_SKIP_DERIVED]->b3_part_size;

        void *pv_pu_src = (UWORD8 *)pv_src + i4_src_buf_offset;
        UWORD8 u1_num_merge_cands = 0;

        u1_part_wd = (aps_cands[0]->as_inter_pu[i].b4_wd + 1) << 2;
        u1_part_ht = (aps_cands[0]->as_inter_pu[i].b4_ht + 1) << 2;
        u1_pu_pos_x_4x4 = aps_cands[0]->as_inter_pu[i].b4_pos_x;
        u1_pu_pos_y_4x4 = aps_cands[0]->as_inter_pu[i].b4_pos_y;

        /* Inter cand pred and satd */
        if(!u1_eval_skip)
        {
            void *pv_pu_pred = (UWORD8 *)ppv_pred_buf_list[u1_me_pred_buf_idx] + i4_pred_buf_offset;

            if(ps_ctxt->u1_reuse_me_sad)
            {
                ihevce_compute_inter_pred_and_cost(
                    ps_mc_ctxt,
                    pf_luma_inter_pred_pu,
                    pf_sad_func,
                    &aps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu[i],
                    pv_pu_src,
                    pv_pu_pred,
                    i4_src_stride,
                    i4_pred_stride,
                    0,
                    ps_ctxt->ps_cmn_utils_optimised_function_list);

                au4_cost[ME_OR_SKIP_DERIVED][i] =
                    ps_ctxt->pai4_me_err_metric[u1_me_cand_list_idx][i];
            }
            else
            {
                au4_cost[ME_OR_SKIP_DERIVED][i] = ihevce_compute_inter_pred_and_cost(
                    ps_mc_ctxt,
                    pf_luma_inter_pred_pu,
                    pf_sad_func,
                    &aps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu[i],
                    pv_pu_src,
                    pv_pu_pred,
                    i4_src_stride,
                    i4_pred_stride,
                    1,
                    ps_ctxt->ps_cmn_utils_optimised_function_list);
            }

            au1_final_pred_buf_id[ME_OR_SKIP_DERIVED][i] = u1_me_pred_buf_idx;

            if(u1_is_cu_noisy && ps_ctxt->i4_alpha_stim_multiplier)
            {
                ihevce_calc_variance(
                    pv_pu_pred,
                    i4_pred_stride,
                    &i4_mean,
                    &au4_pred_variance[ME_OR_SKIP_DERIVED][i],
                    u1_part_ht,
                    u1_part_wd,
                    ps_ctxt->u1_is_hbd,
                    0);

                ai4_noise_term[ME_OR_SKIP_DERIVED][i] = ihevce_compute_noise_term(
                    ps_ctxt->i4_alpha_stim_multiplier,
                    pu4_src_variance[i],
                    au4_pred_variance[ME_OR_SKIP_DERIVED][i]);

                MULTIPLY_STIM_WITH_DISTORTION(
                    au4_cost[ME_OR_SKIP_DERIVED][i],
                    ai4_noise_term[ME_OR_SKIP_DERIVED][i],
                    STIM_Q_FORMAT,
                    ALPHA_Q_FORMAT);
            }
        }

        if(u1_eval_skip || u1_eval_merge)
        {
            pu_t s_pu, *ps_pu_merge;

            UWORD8 u1_is_any_top_available = 1;
            UWORD8 u1_are_valid_merge_cands_available = 1;

            /* get the neighbour availability flags */
            if((u1_num_parts > 1) && u1_single_mcl_flag)
            { /* 8x8 SMPs take the 2Nx2N neighbours */
                ihevce_get_only_nbr_flag(
                    &s_nbr,
                    pu1_ctb_nbr_map,
                    i4_nbr_map_stride,
                    aps_cands[0]->as_inter_pu[0].b4_pos_x,
                    aps_cands[0]->as_inter_pu[0].b4_pos_y,
                    u1_cu_size >> 2,
                    u1_cu_size >> 2);

                /* Make the PU width and height as 8 */
                memcpy(&s_pu, ps_pu, sizeof(pu_t));
                s_pu.b4_pos_x = u1_cu_pos_x >> 2;
                s_pu.b4_pos_y = u1_cu_pos_y >> 2;
                s_pu.b4_wd = (u1_cu_size >> 2) - 1;
                s_pu.b4_ht = (u1_cu_size >> 2) - 1;

                /* Give the local PU structure to MV merge */
                ps_pu_merge = &s_pu;
            }
            else
            {
                ihevce_get_only_nbr_flag(
                    &s_nbr,
                    pu1_ctb_nbr_map,
                    i4_nbr_map_stride,
                    u1_pu_pos_x_4x4,
                    u1_pu_pos_y_4x4,
                    u1_part_wd >> 2,
                    u1_part_ht >> 2);

                u1_is_any_top_available = s_nbr.u1_top_avail || s_nbr.u1_top_rt_avail ||
                                          s_nbr.u1_top_lt_avail;

                if(!ps_ctxt->u1_use_merge_cand_from_top_row)
                {
                    if(u1_is_any_top_available)
                    {
                        if(s_nbr.u1_left_avail || s_nbr.u1_bot_lt_avail)
                        {
                            s_nbr.u1_top_avail = 0;
                            s_nbr.u1_top_rt_avail = 0;
                            s_nbr.u1_top_lt_avail = 0;
                        }
                        else
                        {
                            u1_are_valid_merge_cands_available = 0;
                        }
                    }
                }

                /* Actual PU passed to MV merge */
                ps_pu_merge = ps_pu;
            }
            if(u1_are_valid_merge_cands_available)
            {
                u1_num_merge_cands = ihevce_mv_pred_merge(
                    ps_mv_pred_ctxt,
                    ps_pu_top_nbr,
                    ps_pu_left_nbr,
                    ps_pu_topleft_nbr,
                    i4_nbr_4x4_left_stride,
                    &s_nbr,
                    NULL,
                    ps_pu_merge,
                    e_part_size,
                    i,
                    u1_single_mcl_flag,
                    as_merge_cand,
                    au1_is_top_used);

                if(u1_num_merge_cands > u1_max_merge_candidates)
                {
                    u1_num_merge_cands = u1_max_merge_candidates;
                }

                u1_num_merge_cands = ihevce_merge_candidate_seive(
                    &s_nbr,
                    as_merge_cand,
                    au1_is_top_used,
                    u1_num_merge_cands,
                    ps_ctxt->u1_use_merge_cand_from_top_row || !u1_is_any_top_available);

                for(j = 0; j < u1_num_merge_cands; j++)
                {
                    s_merge_prms.au1_valid_merge_indices[j] = j;
                }

                au4_cost[MERGE_DERIVED][i] = ihevce_determine_best_merge_pu(
                    &s_merge_prms,
                    &aps_cands[MERGE_DERIVED]->as_inter_pu[i],
                    &aps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu[i],
                    pv_pu_src,
                    au4_cost[ME_OR_SKIP_DERIVED][i],
                    i4_pred_buf_offset,
                    u1_num_merge_cands,
                    i,
                    u1_eval_skip);
            }
            else
            {
                au4_cost[MERGE_DERIVED][i] = INT_MAX;
            }

            au4_cost[(i) ? MIXED_MODE_TYPE1 : MIXED_MODE_TYPE0][i] = au4_cost[MERGE_DERIVED][i];

            if(u1_eval_skip)
            {
                /* This statement ensures that the skip candidate is always added */
                au4_cost[ME_OR_SKIP_DERIVED][i] =
                    (au4_cost[MERGE_DERIVED][0] < INT_MAX) ? SKIP_MODE_COST : INT_MAX;
                au1_final_pred_buf_id[ME_OR_SKIP_DERIVED][i] =
                    au1_final_pred_buf_id[MERGE_DERIVED][i];
            }
            else
            {
                au4_cost[ME_OR_SKIP_DERIVED][i] += ps_ctxt->pai4_mv_cost[u1_me_cand_list_idx][i];
                au4_cost[(i) ? MIXED_MODE_TYPE0 : MIXED_MODE_TYPE1][i] =
                    au4_cost[ME_OR_SKIP_DERIVED][i];
            }

            au1_final_pred_buf_id[(i) ? MIXED_MODE_TYPE1 : MIXED_MODE_TYPE0][i] =
                au1_final_pred_buf_id[MERGE_DERIVED][i];
            au1_final_pred_buf_id[(i) ? MIXED_MODE_TYPE0 : MIXED_MODE_TYPE1][i] =
                au1_final_pred_buf_id[ME_OR_SKIP_DERIVED][i];
        }
        else
        {
            au4_cost[ME_OR_SKIP_DERIVED][i] += ps_ctxt->pai4_mv_cost[u1_me_cand_list_idx][i];
        }

        if(!i && (u1_num_parts > 1) && u1_eval_merge)
        {
            ihevce_set_inter_nbr_map(
                pu1_ctb_nbr_map,
                i4_nbr_map_stride,
                u1_pu_pos_x_4x4,
                u1_pu_pos_y_4x4,
                (u1_part_wd >> 2),
                (u1_part_ht >> 2),
                1);
            ihevce_populate_nbr_4x4_with_pu_data(
                ps_cu_nbr_buf, &aps_cands[ME_OR_SKIP_DERIVED]->as_inter_pu[i], u1_cu_size >> 2);

            if(u1_part_wd < u1_cu_size)
            {
                i4_pred_buf_offset = i4_src_buf_offset = u1_part_wd;

                if(!u1_single_mcl_flag) /* 8x8 SMPs take the 2Nx2N neighbours */
                {
                    ps_cu_nbr_buf += (u1_part_wd >> 2);
                    ps_pu_left_nbr = ps_cu_nbr_buf - 1;
                    ps_pu_top_nbr += (u1_part_wd >> 2);
                    ps_pu_topleft_nbr = ps_pu_top_nbr - 1;

                    i4_nbr_4x4_left_stride = (u1_cu_size >> 2);
                }
            }
            else if(u1_part_ht < u1_cu_size)
            {
                i4_pred_buf_offset = u1_part_ht * i4_pred_stride;
                i4_src_buf_offset = u1_part_ht * i4_src_stride;

                if(!u1_single_mcl_flag) /* 8x8 SMPs take the 2Nx2N neighbours */
                {
                    ps_cu_nbr_buf += (u1_part_ht >> 2) * (u1_cu_size >> 2);
                    ps_pu_left_nbr += (u1_part_ht >> 2) * i4_nbr_4x4_left_stride;
                    ps_pu_top_nbr = ps_cu_nbr_buf - (u1_cu_size >> 2);
                    ps_pu_topleft_nbr = ps_pu_left_nbr - i4_nbr_4x4_left_stride;
                }
            }

            i4_pred_buf_offset *= u1_num_bytes_per_pel;
            i4_src_buf_offset *= u1_num_bytes_per_pel;

            aps_cands[MIXED_MODE_TYPE0]->as_inter_pu[0] = aps_cands[MERGE_DERIVED]->as_inter_pu[0];
        }
        else if(!i && (u1_num_parts > 1) && (!u1_eval_merge))
        {
            if(u1_part_wd < u1_cu_size)
            {
                i4_pred_buf_offset = i4_src_buf_offset = u1_part_wd;
            }
            else if(u1_part_ht < u1_cu_size)
            {
                i4_pred_buf_offset = u1_part_ht * i4_pred_stride;
                i4_src_buf_offset = u1_part_ht * i4_src_stride;
            }

            i4_pred_buf_offset *= u1_num_bytes_per_pel;
            i4_src_buf_offset *= u1_num_bytes_per_pel;
        }
        else if(i && (u1_num_parts > 1) && u1_eval_merge)
        {
            aps_cands[MIXED_MODE_TYPE1]->as_inter_pu[1] = aps_cands[MERGE_DERIVED]->as_inter_pu[1];
        }
    }

    /* Adding a skip candidate */
    if((u1_eval_merge) && (0 == u1_part_type))
    {
        cu_inter_cand_t *ps_cand = &ps_ctxt->ps_cu_inter_merge_skip->as_cu_inter_merge_skip_cand
                                        [MAX_NUM_CU_MERGE_SKIP_CAND - 1 -
                                         ps_ctxt->ps_cu_inter_merge_skip->u1_num_skip_cands];

        (*ps_cand) = (*aps_cands[MERGE_DERIVED]);

        ps_cand->b1_skip_flag = 1;

        aps_cands[MIXED_MODE_TYPE1] = ps_cand;
        au4_cost[MIXED_MODE_TYPE1][0] = (au4_cost[MERGE_DERIVED][0] < INT_MAX) ? SKIP_MODE_COST
                                                                               : INT_MAX;
    }

    /* Sort and populate */
    u1_skip_or_merge_cand_is_valid = ihevce_merge_cands_with_existing_best(
        ps_cu_mode_info,
        aps_cands,
        as_mvp_winner,
        au4_cost,
        ppv_pred_buf_list,
        au1_final_pred_buf_id,
        &ps_pred_buf_info->u4_is_buf_in_use,
        &ps_ctxt->ps_cu_inter_merge_skip->u1_num_merge_cands,
        &ps_ctxt->ps_cu_inter_merge_skip->u1_num_skip_cands,
        &ps_ctxt->ps_mixed_modes_datastore->u1_num_mixed_mode_type0_cands,
        &ps_ctxt->ps_mixed_modes_datastore->u1_num_mixed_mode_type1_cands,
        au1_merge_pred_buf_idx_array,
        ps_ctxt->ps_cmn_utils_optimised_function_list->pf_copy_2d,

        i4_pred_stride,
        i4_max_num_inter_rdopt_cands,
        u1_cu_size,
        u1_part_type,
        u1_eval_merge,
        u1_eval_skip,
        u1_num_bytes_per_pel);

    return u1_skip_or_merge_cand_is_valid;
}

static __inline void ihevce_redundant_candidate_pruner(inter_cu_mode_info_t *ps_inter_cu_mode_info)
{
    WORD8 i, j;
    WORD8 i1_num_merge_vs_mvds;

    UWORD8 au1_redundant_cand_indices[MAX_NUM_INTER_RDO_CANDS] = { 0 };

    for(i = 0; i < (ps_inter_cu_mode_info->u1_num_inter_cands - 1); i++)
    {
        if(au1_redundant_cand_indices[i] || ps_inter_cu_mode_info->aps_cu_data[i]->b1_skip_flag)
        {
            continue;
        }

        for(j = i + 1; j < ps_inter_cu_mode_info->u1_num_inter_cands; j++)
        {
            if(au1_redundant_cand_indices[j] || ps_inter_cu_mode_info->aps_cu_data[j]->b1_skip_flag)
            {
                continue;
            }

            i1_num_merge_vs_mvds = 0;

            if(ps_inter_cu_mode_info->aps_cu_data[j]->b3_part_size ==
               ps_inter_cu_mode_info->aps_cu_data[i]->b3_part_size)
            {
                if(ihevce_compare_pu_mv_t(
                       &ps_inter_cu_mode_info->aps_cu_data[i]->as_inter_pu->mv,
                       &ps_inter_cu_mode_info->aps_cu_data[j]->as_inter_pu->mv,
                       ps_inter_cu_mode_info->aps_cu_data[i]->as_inter_pu->b2_pred_mode,
                       ps_inter_cu_mode_info->aps_cu_data[j]->as_inter_pu->b2_pred_mode))
                {
                    i1_num_merge_vs_mvds +=
                        ps_inter_cu_mode_info->aps_cu_data[i]->as_inter_pu->b1_merge_flag -
                        ps_inter_cu_mode_info->aps_cu_data[j]->as_inter_pu->b1_merge_flag;

                    if(ps_inter_cu_mode_info->aps_cu_data[i]->b3_part_size)
                    {
                        if(ihevce_compare_pu_mv_t(
                               &ps_inter_cu_mode_info->aps_cu_data[i]->as_inter_pu[1].mv,
                               &ps_inter_cu_mode_info->aps_cu_data[j]->as_inter_pu[1].mv,
                               ps_inter_cu_mode_info->aps_cu_data[i]->as_inter_pu[1].b2_pred_mode,
                               ps_inter_cu_mode_info->aps_cu_data[j]->as_inter_pu[1].b2_pred_mode))
                        {
                            i1_num_merge_vs_mvds +=
                                ps_inter_cu_mode_info->aps_cu_data[i]->as_inter_pu[1].b1_merge_flag -
                                ps_inter_cu_mode_info->aps_cu_data[j]->as_inter_pu[1].b1_merge_flag;
                        }
                    }
                }
            }

            if(i1_num_merge_vs_mvds != 0)
            {
                au1_redundant_cand_indices[(i1_num_merge_vs_mvds > 0) ? j : i] = 1;
            }
        }
    }

    for(i = 0; i < ps_inter_cu_mode_info->u1_num_inter_cands; i++)
    {
        if(au1_redundant_cand_indices[i])
        {
            memmove(
                &ps_inter_cu_mode_info->aps_cu_data[i],
                &ps_inter_cu_mode_info->aps_cu_data[i + 1],
                (ps_inter_cu_mode_info->u1_num_inter_cands - i - 1) *
                    sizeof(ps_inter_cu_mode_info->aps_cu_data[i]));

            memmove(
                &ps_inter_cu_mode_info->au4_cost[i],
                &ps_inter_cu_mode_info->au4_cost[i + 1],
                (ps_inter_cu_mode_info->u1_num_inter_cands - i - 1) *
                    sizeof(ps_inter_cu_mode_info->au4_cost[i]));

            memmove(
                &ps_inter_cu_mode_info->au1_pred_buf_idx[i],
                &ps_inter_cu_mode_info->au1_pred_buf_idx[i + 1],
                (ps_inter_cu_mode_info->u1_num_inter_cands - i - 1) *
                    sizeof(ps_inter_cu_mode_info->au1_pred_buf_idx[i]));

            memmove(
                &au1_redundant_cand_indices[i],
                &au1_redundant_cand_indices[i + 1],
                (ps_inter_cu_mode_info->u1_num_inter_cands - i - 1) *
                    sizeof(au1_redundant_cand_indices[i]));

            ps_inter_cu_mode_info->u1_num_inter_cands--;
            i--;
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_inter_cand_sifter \endif
*
* \brief
*    Selects the best inter candidate modes amongst ME, merge,
*    skip and mixed modes. Also computes corresponding preds
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_inter_cand_sifter(ihevce_inter_cand_sifter_prms_t *ps_ctxt)
{
    PF_SAD_FXN_T pf_sad_func;

    UWORD8 au1_final_cand_idx[MAX_INTER_CU_CANDIDATES];
    UWORD8 au1_part_types_evaluated[MAX_INTER_CU_CANDIDATES];
    UWORD8 u1_num_unique_parts;
    UWORD8 i, j;
    UWORD32 au4_src_variance[NUM_INTER_PU_PARTS];
    WORD32 i4_mean;

    cu_inter_cand_t *ps_me_cands = ps_ctxt->ps_me_cands;
    inter_cu_mode_info_t *ps_cu_mode_info = ps_ctxt->ps_inter_cu_mode_info;

    UWORD8 u1_diff_skip_cand_flag = 1;
    WORD8 i1_skip_cand_from_merge_idx = -1;
    WORD8 i1_final_skip_cand_merge_idx = -1;
    UWORD8 u1_max_num_part_types_to_select = MAX_INTER_CU_CANDIDATES;
    UWORD8 u1_num_me_cands = ps_ctxt->u1_num_me_cands;
    UWORD8 u1_num_parts_evaluated_for_merge = 0;
    UWORD8 u1_is_cu_noisy = ps_ctxt->u1_is_cu_noisy;

    if((ps_ctxt->u1_quality_preset >= IHEVCE_QUALITY_P3) && (ps_ctxt->i1_slice_type == BSLICE))
    {
        u1_max_num_part_types_to_select = 1;
    }

    {
        pf_sad_func = (ps_ctxt->u1_use_satd_for_merge_eval) ? compute_satd_8bit
                                                            : ps_ctxt->pf_evalsad_pt_npu_mxn_8bit;
    }

    u1_num_unique_parts = ihevce_get_num_part_types_in_me_cand_list(
        ps_me_cands,
        au1_part_types_evaluated,
        au1_final_cand_idx,
        &u1_diff_skip_cand_flag,
        &i1_skip_cand_from_merge_idx,
        &i1_final_skip_cand_merge_idx,
        u1_max_num_part_types_to_select,
        u1_num_me_cands);

    if((u1_num_me_cands + u1_diff_skip_cand_flag) && u1_is_cu_noisy &&
       ps_ctxt->i4_alpha_stim_multiplier)
    {
        ihevce_calc_variance(
            ps_ctxt->pv_src,
            ps_ctxt->i4_src_strd,
            &i4_mean,
            &ps_cu_mode_info->u4_src_variance,
            ps_ctxt->u1_cu_size,
            ps_ctxt->u1_cu_size,
            ps_ctxt->u1_is_hbd,
            0);
    }

    if(DISABLE_SKIP_AND_MERGE_WHEN_NOISY && u1_is_cu_noisy)
    {
        u1_diff_skip_cand_flag = 0;
    }
    else if(!DISABLE_SKIP_AND_MERGE_WHEN_NOISY && u1_is_cu_noisy)
    {
        if(ps_ctxt->u1_cu_size > MAX_CU_SIZE_WHERE_MERGE_AND_SKIPS_ENABLED_AND_WHEN_NOISY)
        {
            u1_diff_skip_cand_flag = 0;
        }
    }

    for(i = 0; i < u1_num_me_cands + u1_diff_skip_cand_flag; i++)
    {
        UWORD8 u1_part_type;
        UWORD8 u1_eval_skip;
        UWORD8 u1_eval_merge;
        UWORD8 u1_valid_cand;

        if(i == u1_num_me_cands)
        {
            u1_eval_skip = 1;
            u1_eval_merge = 0;
            u1_part_type = 0;
        }
        else
        {
            u1_eval_skip = 0;
            u1_part_type = ps_me_cands[i].b3_part_size;

            if(u1_num_parts_evaluated_for_merge >= u1_num_unique_parts)
            {
                u1_eval_merge = 0;
                u1_num_parts_evaluated_for_merge = u1_num_unique_parts;
            }
            else
            {
                u1_eval_merge = (i == au1_final_cand_idx[u1_num_parts_evaluated_for_merge]);
            }

            for(j = 0; (j < u1_num_parts_evaluated_for_merge) && (u1_eval_merge); j++)
            {
                if(u1_part_type == au1_part_types_evaluated[j])
                {
                    u1_eval_merge = 0;
                    break;
                }
            }
        }

        if(u1_is_cu_noisy && u1_part_type && ps_ctxt->i4_alpha_stim_multiplier)
        {
            void *pv_src = ps_ctxt->pv_src;
            UWORD8 u1_pu_wd = (ps_me_cands[i].as_inter_pu[0].b4_wd + 1) << 2;
            UWORD8 u1_pu_ht = (ps_me_cands[i].as_inter_pu[0].b4_ht + 1) << 2;

            ihevce_calc_variance(
                pv_src,
                ps_ctxt->i4_src_strd,
                &i4_mean,
                &au4_src_variance[0],
                u1_pu_ht,
                u1_pu_wd,
                ps_ctxt->u1_is_hbd,
                0);

            pv_src = (void *) (((UWORD8 *) pv_src) +
                ((ps_ctxt->u1_cu_size == u1_pu_wd) ? ps_ctxt->i4_src_strd * u1_pu_ht : u1_pu_wd)
                * (ps_ctxt->u1_is_hbd + 1));
            u1_pu_wd = (ps_me_cands[i].as_inter_pu[1].b4_wd + 1) << 2;
            u1_pu_ht = (ps_me_cands[i].as_inter_pu[1].b4_ht + 1) << 2;

            ihevce_calc_variance(
                pv_src,
                ps_ctxt->i4_src_strd,
                &i4_mean,
                &au4_src_variance[1],
                u1_pu_ht,
                u1_pu_wd,
                ps_ctxt->u1_is_hbd,
                0);
        }
        else if(u1_is_cu_noisy && !u1_part_type && ps_ctxt->i4_alpha_stim_multiplier)
        {
            au4_src_variance[0] = ps_cu_mode_info->u4_src_variance;
        }

        if(DISABLE_SKIP_AND_MERGE_WHEN_NOISY && u1_is_cu_noisy)
        {
            u1_eval_merge = 0;
        }
        else if(!DISABLE_SKIP_AND_MERGE_WHEN_NOISY && u1_is_cu_noisy)
        {
            if(ps_ctxt->u1_cu_size > MAX_CU_SIZE_WHERE_MERGE_AND_SKIPS_ENABLED_AND_WHEN_NOISY)
            {
                u1_eval_merge = 0;
            }
        }

        u1_valid_cand = ihevce_compute_pred_and_populate_modes(
            ps_ctxt,
            pf_sad_func,
            au4_src_variance,
            u1_part_type,
            MIN(i, (u1_num_me_cands - 1)),
            u1_eval_merge,
            u1_eval_skip);

        u1_num_parts_evaluated_for_merge += u1_eval_merge;

        /* set the neighbour map to 0 */
        if(u1_part_type)
        {
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_ctb_nbr_map_stride,
                (ps_ctxt->u1_cu_pos_x >> 2),
                (ps_ctxt->u1_cu_pos_y >> 2),
                (ps_ctxt->u1_cu_size >> 2),
                0);
        }
    }

    ihevce_redundant_candidate_pruner(ps_ctxt->ps_inter_cu_mode_info);
}
