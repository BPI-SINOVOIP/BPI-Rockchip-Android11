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
* \file ihevce_decomp_pre_intra_pass.c
*
* \brief
*    This file contains definitions related to frame decomposition done during
*    pre intra processing
*
* \date
*    19/02/2013
*
* \author
*    Ittiam
*
* List of Functions
*    ihevce_intra_populate_mode_bits_cost()
*    ihevce_8x8_sad_computer()
*    ihevce_4x4_sad_computer()
*    ihevce_ed_4x4_find_best_modes()
*    ihevce_ed_calc_4x4_blk()
*    ihevce_ed_calc_8x8_blk()
*    ihevce_ed_calc_incomplete_ctb()
*    ihevce_cu_level_qp_mod()
*    ihevce_ed_calc_ctb()
*    ihevce_ed_frame_init()
*    ihevce_scale_by_2()
*    ihevce_decomp_pre_intra_process_row()
*    ihevce_decomp_pre_intra_process()
*    ihevce_decomp_pre_intra_get_num_mem_recs()
*    ihevce_decomp_pre_intra_get_mem_recs()
*    ihevce_decomp_pre_intra_init()
*    ihevce_decomp_pre_intra_frame_init()
*    ihevce_merge_sort()
*    ihevce_decomp_pre_intra_curr_frame_pre_intra_deinit()
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
#include <stdint.h>
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
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "ihevce_global_tables.h"

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/**
*****************************************************************************
* @brief subset of intra modes to be evaluated during pre enc intra process
*****************************************************************************
*/
static const UWORD8 gau1_modes_to_eval[11] = { 0, 1, 26, 2, 6, 10, 14, 18, 22, 30, 34 };

/**
*****************************************************************************
* @brief  list of pointers to luma intra pred functions
*****************************************************************************
*/
pf_intra_pred g_apf_lum_ip[NUM_IP_FUNCS];

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_intra_populate_mode_bits_cost \endif
*
* \brief: look-up table of cost of signalling an intra mode in the
*  bitstream
*
*****************************************************************************
*/
static void ihevce_intra_populate_mode_bits_cost(UWORD16 *mode_bits_cost, WORD32 lambda)
{
    WORD32 i;
    // 5.5 * lambda
    UWORD16 five_bits_cost = COMPUTE_RATE_COST_CLIP30(11, lambda, (LAMBDA_Q_SHIFT + 1));

    for(i = 0; i < NUM_MODES; i++)
    {
        mode_bits_cost[i] = five_bits_cost;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_8x8_sad_computer \endif
*
* \brief: compute sad between 2 8x8 blocks
*
*****************************************************************************
*/
UWORD16 ihevce_8x8_sad_computer(UWORD8 *src, UWORD8 *pred, WORD32 src_strd, WORD32 pred_strd)
{
    UWORD16 sad = 0;
    WORD32 i, j;

    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < 8; j++)
        {
            sad += ABS(src[j] - pred[j]);
        }
        src += src_strd;
        pred += pred_strd;
    }

    return sad;
}

/*!
******************************************************************************
* \if Function name : ihevce_4x4_sad_computer \endif
*
* \brief: compute sad between 2 4x4 blocks
*
*****************************************************************************
*/
UWORD16 ihevce_4x4_sad_computer(UWORD8 *src, UWORD8 *pred, WORD32 src_strd, WORD32 pred_strd)
{
    UWORD16 sad = 0;
    WORD32 i, j;

    for(i = 0; i < 4; i++)
    {
        for(j = 0; j < 4; j++)
        {
            sad += ABS(src[j] - pred[j]);
        }
        src += src_strd;
        pred += pred_strd;
    }

    return sad;
}

/*!
******************************************************************************
* \if Function name : ihevce_ed_4x4_find_best_modes \endif
*
* \brief: evaluate input 4x4 block for pre-selected list intra modes and
* return best sad, cost
*
*****************************************************************************
*/
void ihevce_ed_4x4_find_best_modes(
    UWORD8 *pu1_src,
    WORD32 src_stride,
    UWORD8 *ref,
    UWORD16 *mode_bits_cost,
    UWORD8 *pu1_best_modes,
    WORD32 *pu1_best_sad_costs,
    WORD32 u1_low_resol,
    FT_SAD_COMPUTER *pf_4x4_sad_computer)
{
    WORD32 i;
    UWORD8 mode = 0, best_amode = 0, best_nmode = 0;
    UWORD8 pred[16];
    WORD32 sad = 0;
    WORD32 sad_cost = 0;
    WORD32 best_asad_cost = 0xFFFFF;
    WORD32 best_nsad_cost = 0xFFFFF;

    /* If lower layers, l1 or l2, all the 11 modes are evaluated */
    /* If L0 layer, all modes excluding DC and Planar are evaluated */
    if(1 == u1_low_resol)
        i = 0;
    else
        i = 2;

    /* Find the best non-angular and angular mode till level 4 */
    for(; i < 11; i++)
    {
        mode = gau1_modes_to_eval[i];
        g_apf_lum_ip[g_i4_ip_funcs[mode]](&ref[0], 0, &pred[0], 4, 4, mode);
        sad = pf_4x4_sad_computer(pu1_src, pred, src_stride, 4);
        sad_cost = sad + mode_bits_cost[mode];
        if(mode < 2)
        {
            if(sad_cost < best_nsad_cost)
            {
                best_nmode = mode;
                best_nsad_cost = sad_cost;
            }
        }
        else
        {
            if(sad_cost < best_asad_cost)
            {
                best_amode = mode;
                best_asad_cost = sad_cost;
            }
        }
    }

    pu1_best_modes[0] = best_amode;
    pu1_best_sad_costs[0] = best_asad_cost;

    if(1 == u1_low_resol)
    {
        pu1_best_modes[1] = best_nmode;
        pu1_best_sad_costs[1] = best_nsad_cost;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_ed_calc_4x4_blk \endif
*
* \brief: evaluate input 4x4 block for all intra modes and return best sad &
*  cost
*
*****************************************************************************
*/
static void ihevce_ed_calc_4x4_blk(
    ihevce_ed_blk_t *ps_ed,
    UWORD8 *pu1_src,
    WORD32 src_stride,
    UWORD8 *ref,
    UWORD16 *mode_bits_cost,
    WORD32 *pi4_best_satd,
    WORD32 i4_quality_preset,
    WORD32 *pi4_best_sad_cost,
    ihevce_ipe_optimised_function_list_t *ps_ipe_optimised_function_list)
{
    WORD32 i, i_end;
    UWORD8 mode, best_amode, best_nmode;
    UWORD8 pred[16];
    UWORD16 sad;
    WORD32 sad_cost = 0;
    WORD32 best_asad_cost = 0xFFFFF;
    WORD32 best_nsad_cost = 0xFFFFF;
    UWORD8 au1_best_modes[2];
    WORD32 ai4_best_sad_costs[2];
    /* L1/L2 resolution hence low resolution enable */
    const WORD32 u1_low_resol = 1;
    UWORD8 modes_to_eval[2];

    ps_ipe_optimised_function_list->pf_ed_4x4_find_best_modes(
        pu1_src,
        src_stride,
        ref,
        mode_bits_cost,
        au1_best_modes,
        ai4_best_sad_costs,
        u1_low_resol,
        ps_ipe_optimised_function_list->pf_4x4_sad_computer);

    best_nmode = au1_best_modes[1];
    best_amode = au1_best_modes[0];
    best_nsad_cost = ai4_best_sad_costs[1];
    best_asad_cost = ai4_best_sad_costs[0];
    *pi4_best_satd = best_asad_cost - mode_bits_cost[best_amode];

    /* Around best level 4 angular mode, search for best level 2 mode */
    modes_to_eval[0] = best_amode - 2;
    modes_to_eval[1] = best_amode + 2;
    i = 0;
    i_end = 2;
    if(best_amode == 2)
        i = 1;
    else if(best_amode == 34)
        i_end = 1;
    for(; i < i_end; i++)
    {
        mode = modes_to_eval[i];
        g_apf_lum_ip[g_i4_ip_funcs[mode]](&ref[0], 0, &pred[0], 4, 4, mode);
        sad = ps_ipe_optimised_function_list->pf_4x4_sad_computer(pu1_src, pred, src_stride, 4);
        sad_cost = sad + mode_bits_cost[mode];
        if(sad_cost < best_asad_cost)
        {
            best_amode = mode;
            best_asad_cost = sad_cost;
            *pi4_best_satd = sad;
        }
    }

    if(i4_quality_preset < IHEVCE_QUALITY_P4)
    {
        /* Around best level 2 angular mode, search for best level 1 mode */
        modes_to_eval[0] = best_amode - 1;
        modes_to_eval[1] = best_amode + 1;
        i = 0;
        i_end = 2;
        if(best_amode == 2)
            i = 1;
        else if(best_amode == 34)
            i_end = 1;
        for(; i < i_end; i++)
        {
            mode = modes_to_eval[i];
            g_apf_lum_ip[g_i4_ip_funcs[mode]](&ref[0], 0, &pred[0], 4, 4, mode);
            sad = ps_ipe_optimised_function_list->pf_4x4_sad_computer(pu1_src, pred, src_stride, 4);
            sad_cost = sad + mode_bits_cost[mode];
            if(sad_cost < best_asad_cost)
            {
                best_amode = mode;
                best_asad_cost = sad_cost;
                *pi4_best_satd = sad;
            }
        }
    }

    if(best_asad_cost < best_nsad_cost)
    {
        ps_ed->best_mode = best_amode;
        *pi4_best_sad_cost = best_asad_cost;
    }
    else
    {
        ps_ed->best_mode = best_nmode;
        *pi4_best_sad_cost = best_nsad_cost;
    }
    ps_ed->intra_or_inter = 0;
    ps_ed->merge_success = 0;
}

/*!
******************************************************************************
* \if Function name : ihevce_ed_calc_8x8_blk \endif
*
* \brief: evaluate input 8x8 block for intra modes basing on the intra mode
*  decisions made at 4x4 level. This function also makes a decision whether
*  to split blk in to 4x4 partitions or not.
*
*****************************************************************************
*/
static void ihevce_ed_calc_8x8_blk(
    ihevce_ed_ctxt_t *ps_ed_ctxt,
    ihevce_ed_blk_t *ps_ed_8x8,
    UWORD8 *pu1_src,
    WORD32 src_stride,
    WORD32 *nbr_flags_ptr,
    WORD32 lambda,
    WORD32 *pi4_best_satd,
    WORD32 i4_layer_id,
    WORD32 i4_quality_preset,
    WORD32 *pi4_best_sad_cost_8x8_l1_ipe,
    WORD32 *pi4_best_sad_8x8_l1_ipe,
    ihevce_ipe_optimised_function_list_t *ps_ipe_optimised_function_list,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list)
{
    ihevce_ed_blk_t *ps_ed_4x4 = ps_ed_8x8;
    UWORD8 *pu1_src_arr[4];
    WORD32 ai4_4x4_best_sad_cost[4];
    WORD32 nbr_flags_c, nbr_flags_r;
    UWORD8 *pu1_src_4x4;
    WORD32 i, j;
    func_selector_t *ps_func_selector = ps_ed_ctxt->ps_func_selector;
    ihevc_intra_pred_luma_ref_substitution_ft *pf_intra_pred_luma_ref_substitution =
        ps_func_selector->ihevc_intra_pred_luma_ref_substitution_fptr;

    /* linearize ref samples for ipe of 8x8 block */
    nbr_flags_c = nbr_flags_ptr[0];
    nbr_flags_r = nbr_flags_ptr[1];
    if(CHECK_TR_AVAILABLE(nbr_flags_r))
    {
        SET_TR_AVAILABLE(nbr_flags_c);
    }
    else
    {
        SET_TR_UNAVAILABLE(nbr_flags_c);
    }

    pf_intra_pred_luma_ref_substitution(
        pu1_src - src_stride - 1,
        pu1_src - src_stride,
        pu1_src - 1,
        src_stride,
        8,
        nbr_flags_c,
        &ps_ed_ctxt->au1_ref_8x8[0][0],
        0);

    for(i = 0; i < 2; i++)
    {
        pu1_src_4x4 = pu1_src + i * 4 * src_stride;
        for(j = 0; j < 2; j++)
        {
            WORD32 i4_best_satd;

            pu1_src_arr[i * 2 + j] = pu1_src_4x4;
            nbr_flags_c = nbr_flags_ptr[i * 8 + j];

            /* linearize ref samples for ipe of 4x4 block */
            pf_intra_pred_luma_ref_substitution(
                pu1_src_4x4 - src_stride - 1,
                pu1_src_4x4 - src_stride,
                pu1_src_4x4 - 1,
                src_stride,
                4,
                nbr_flags_c,
                &ps_ed_ctxt->au1_ref_full_ctb[i * 2 + j][0],
                0);

            /* populates mode bits cost */
            ihevce_intra_populate_mode_bits_cost(
                &ps_ed_ctxt->au2_mode_bits_cost_full_ctb[i * 2 + j][0], lambda);

            ihevce_ed_calc_4x4_blk(
                ps_ed_4x4,
                pu1_src_4x4,
                src_stride,
                &ps_ed_ctxt->au1_ref_full_ctb[i * 2 + j][0],
                &ps_ed_ctxt->au2_mode_bits_cost_full_ctb[i * 2 + j][0],
                &i4_best_satd,
                i4_quality_preset,
                &ai4_4x4_best_sad_cost[i * 2 + j],
                ps_ipe_optimised_function_list);

            pu1_src_4x4 += 4;
            ps_ed_4x4 += 1;
        }
    }

    /* 8x8 merge */
    {
        UWORD8 pred[64];
        WORD32 merge_success;
        WORD32 sad, satd, cost;
        UWORD16 u2_sum_best_4x4_sad_cost = 0;
        UWORD16 u2_sum_best_4x4_satd_cost = 0;
        WORD32 i4_best_8x8_sad, i4_best_8x8_satd = 0;
        UWORD16 u2_best_8x8_cost = (UWORD16)(-1);
        UWORD8 u1_best_8x8_mode;
        UWORD8 modes_to_eval[6];
        UWORD8 u1_cond_4x4_satd;
        UWORD8 mode;

        /* init */
        ps_ed_4x4 = ps_ed_8x8;
        u1_best_8x8_mode = mode = ps_ed_4x4[0].best_mode;
        merge_success =
            (((ps_ed_4x4[0].best_mode == ps_ed_4x4[1].best_mode) +
              (ps_ed_4x4[0].best_mode == ps_ed_4x4[2].best_mode) +
              (ps_ed_4x4[0].best_mode == ps_ed_4x4[3].best_mode)) == 3);
        *pi4_best_satd = 0;

        for(i = 0; i < 4; i++)
        {
            u2_sum_best_4x4_sad_cost += ai4_4x4_best_sad_cost[i];
            modes_to_eval[i] = ps_ed_4x4[i].best_mode;
        }

        u1_cond_4x4_satd = ((1 == i4_layer_id) || (!merge_success && i4_quality_preset < IHEVCE_QUALITY_P4));
        if(u1_cond_4x4_satd)
        {
            /* Get SATD for 4x4 blocks */
            for(i = 0; i < 4; i++)
            {
                mode = modes_to_eval[i];
                g_apf_lum_ip[g_i4_ip_funcs[mode]](
                    &ps_ed_ctxt->au1_ref_full_ctb[i][0], 0, &pred[0], 4, 4, mode);

                satd = ps_cmn_utils_optimised_function_list->pf_HAD_4x4_8bit(
                    pu1_src_arr[i], src_stride, &pred[0], 4, NULL, 0);

                (ps_ed_4x4 + i)->i4_4x4_satd = satd;

                u2_sum_best_4x4_satd_cost +=
                    (satd + ps_ed_ctxt->au2_mode_bits_cost_full_ctb[i][mode]);
                *pi4_best_satd += satd;
            }
        }

        if(!merge_success)
        {
            UWORD8 i1_start; /* no of modes to evaluate */
            UWORD8 ai1_modes[6];
            WORD32 i4_merge_success_stage2 = 0;

            /* Prepare 6 candidates for 8x8 block. Two are DC and planar */
            ai1_modes[4] = 0;
            ai1_modes[5] = 1;
            i1_start = 4;

            /* Assign along with removing duplicates rest 4 candidates. */
            for(i = 3; i >= 0; i--)
            {
                WORD8 i1_fresh_mode_flag = 1;

                mode = modes_to_eval[i];
                /* Check if duplicate already exists in ai1_modes */
                for(j = i1_start; j < 6; j++)
                {
                    if(mode == ai1_modes[j])
                        i1_fresh_mode_flag = 0;
                }
                if(i1_fresh_mode_flag)
                {
                    i1_start--;
                    ai1_modes[i1_start] = mode;
                }
            }

            if(i4_quality_preset < IHEVCE_QUALITY_P4)
            {
                // 7.5 * lambda to incorporate transform flags
                u2_sum_best_4x4_satd_cost +=
                    (COMPUTE_RATE_COST_CLIP30(12, lambda, (LAMBDA_Q_SHIFT + 1)));

                /* loop over all modes for calculating SATD */
                for(i = i1_start; i < 6; i++)
                {
                    mode = ai1_modes[i];
                    g_apf_lum_ip[g_i4_ip_funcs[mode]](
                        &ps_ed_ctxt->au1_ref_8x8[0][0], 0, &pred[0], 8, 8, mode);

                    satd = ps_cmn_utils_optimised_function_list->pf_HAD_8x8_8bit(
                        pu1_src_arr[0], src_stride, &pred[0], 8, NULL, 0);

                    cost = satd + ps_ed_ctxt->au2_mode_bits_cost_full_ctb[0][mode];

                    /* Update data corresponding to least 8x8 cost */
                    if(cost <= u2_best_8x8_cost)
                    {
                        u2_best_8x8_cost = cost;
                        i4_best_8x8_satd = satd;
                        u1_best_8x8_mode = mode;
                    }
                }

                /* 8x8 vs 4x4 decision based on SATD values */
                if((u2_best_8x8_cost <= u2_sum_best_4x4_satd_cost) || (u2_best_8x8_cost <= 300))
                {
                    i4_merge_success_stage2 = 1;
                }

                /* Find the SAD based cost for 8x8 block for best mode */
                if(1 == i4_layer_id)
                {
                    UWORD8 i4_best_8x8_mode = u1_best_8x8_mode;
                    WORD32 i4_best_8x8_sad_curr;

                    g_apf_lum_ip[g_i4_ip_funcs[i4_best_8x8_mode]](
                        &ps_ed_ctxt->au1_ref_8x8[0][0], 0, &pred[0], 8, 8, i4_best_8x8_mode);

                    i4_best_8x8_sad_curr = ps_ipe_optimised_function_list->pf_8x8_sad_computer(
                        pu1_src_arr[0], &pred[0], src_stride, 8);

                    *pi4_best_sad_cost_8x8_l1_ipe =
                        i4_best_8x8_sad_curr +
                        ps_ed_ctxt->au2_mode_bits_cost_full_ctb[0][i4_best_8x8_mode];
                    *pi4_best_sad_8x8_l1_ipe = i4_best_8x8_sad_curr;
                }
            }
            else /*If high_speed or extreme speed*/
            {
                // 7.5 * lambda to incorporate transform flags
                u2_sum_best_4x4_sad_cost +=
                    (COMPUTE_RATE_COST_CLIP30(12, lambda, (LAMBDA_Q_SHIFT + 1)));

                /*Loop over all modes for calculating SAD*/
                for(i = i1_start; i < 6; i++)
                {
                    mode = ai1_modes[i];
                    g_apf_lum_ip[g_i4_ip_funcs[mode]](
                        &ps_ed_ctxt->au1_ref_8x8[0][0], 0, &pred[0], 8, 8, mode);

                    sad = ps_ipe_optimised_function_list->pf_8x8_sad_computer(
                        pu1_src_arr[0], &pred[0], src_stride, 8);

                    cost = sad + ps_ed_ctxt->au2_mode_bits_cost_full_ctb[0][mode];

                    /*Find the data correspoinding to least cost */
                    if(cost <= u2_best_8x8_cost)
                    {
                        u2_best_8x8_cost = cost;
                        i4_best_8x8_sad = sad;
                        u1_best_8x8_mode = mode;
                    }
                }

                /* 8x8 vs 4x4 decision based on SAD values */
                if((u2_best_8x8_cost <= u2_sum_best_4x4_sad_cost) || (u2_best_8x8_cost <= 300))
                {
                    i4_merge_success_stage2 = 1;
                    if(1 == i4_layer_id)
                    {
                        g_apf_lum_ip[g_i4_ip_funcs[u1_best_8x8_mode]](
                            &ps_ed_ctxt->au1_ref_8x8[0][0], 0, &pred[0], 8, 8, u1_best_8x8_mode);
                        i4_best_8x8_satd = ps_cmn_utils_optimised_function_list->pf_HAD_8x8_8bit(
                            pu1_src_arr[0], src_stride, &pred[0], 8, NULL, 0);
                    }
                }

                if(1 == i4_layer_id)
                {
                    *pi4_best_sad_cost_8x8_l1_ipe = u2_best_8x8_cost;
                    *pi4_best_sad_8x8_l1_ipe = i4_best_8x8_sad;
                }
            }
            if(i4_merge_success_stage2)
            {
                ps_ed_4x4->merge_success = 1;
                ps_ed_4x4->best_merge_mode = u1_best_8x8_mode;
                *pi4_best_satd = i4_best_8x8_satd;
            }
        }
        else
        {
            ps_ed_4x4->merge_success = 1;
            ps_ed_4x4->best_merge_mode = u1_best_8x8_mode;

            if(1 == i4_layer_id)
            {
                mode = u1_best_8x8_mode;
                g_apf_lum_ip[g_i4_ip_funcs[mode]](
                    &ps_ed_ctxt->au1_ref_8x8[0][0], 0, &pred[0], 8, 8, mode);

                i4_best_8x8_sad = ps_ipe_optimised_function_list->pf_8x8_sad_computer(
                    pu1_src_arr[0], &pred[0], src_stride, 8);

                *pi4_best_sad_cost_8x8_l1_ipe =
                    i4_best_8x8_sad + ps_ed_ctxt->au2_mode_bits_cost_full_ctb[0][mode];
                *pi4_best_sad_8x8_l1_ipe = i4_best_8x8_sad;

                i4_best_8x8_satd = ps_cmn_utils_optimised_function_list->pf_HAD_8x8_8bit(
                    pu1_src_arr[0], src_stride, &pred[0], 8, NULL, 0);
            }
            *pi4_best_satd = i4_best_8x8_satd;
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_ed_calc_ctb \endif
*
* \brief: performs L1/L2 8x8 and 4x4 intra mode analysis
*
*****************************************************************************
*/
void ihevce_ed_calc_ctb(
    ihevce_ed_ctxt_t *ps_ed_ctxt,
    ihevce_ed_blk_t *ps_ed_ctb,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    UWORD8 *pu1_src,
    WORD32 src_stride,
    WORD32 num_4x4_blks_x,
    WORD32 num_4x4_blks_y,
    WORD32 *nbr_flags,
    WORD32 i4_layer_id,
    ihevce_ipe_optimised_function_list_t *ps_ipe_optimised_function_list,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list)
{
    ihevce_ed_blk_t *ps_ed_8x8;
    UWORD8 *pu1_src_8x8;
    WORD32 *nbr_flags_ptr;
    WORD32 lambda = ps_ed_ctxt->lambda;
    WORD32 i, j;
    WORD32 z_scan_idx = 0;
    WORD32 z_scan_act_idx = 0;

    if(i4_layer_id == 1)
    {
        WORD32 i4_i;

        for(i4_i = 0; i4_i < 64; i4_i++)
        {
            (ps_ed_ctb + i4_i)->i4_4x4_satd = -1;
        }

        for(i4_i = 0; i4_i < 16; i4_i++)
        {
            ps_ed_ctb_l1->i4_sum_4x4_satd[i4_i] = -2;
            ps_ed_ctb_l1->i4_min_4x4_satd[i4_i] = 0x7FFFFFFF;
            ps_ed_ctb_l1->i4_8x8_satd[i4_i][0] = -2;
            ps_ed_ctb_l1->i4_8x8_satd[i4_i][1] = -2;
        }

        for(i4_i = 0; i4_i < 4; i4_i++)
        {
            ps_ed_ctb_l1->i4_16x16_satd[i4_i][0] = -2;
            ps_ed_ctb_l1->i4_16x16_satd[i4_i][1] = -2;
            ps_ed_ctb_l1->i4_16x16_satd[i4_i][2] = -2;
        }
        ps_ed_ctb_l1->i4_32x32_satd[0][0] = -2;
        ps_ed_ctb_l1->i4_32x32_satd[0][1] = -2;
        ps_ed_ctb_l1->i4_32x32_satd[0][2] = -2;
        ps_ed_ctb_l1->i4_32x32_satd[0][3] = -2;

        for(i4_i = 0; i4_i < 16; i4_i++)
        {
            ps_ed_ctb_l1->i4_best_sad_cost_8x8_l1_me[i4_i] = -1;
            ps_ed_ctb_l1->i4_sad_cost_me_for_ref[i4_i] = -1;
            ps_ed_ctb_l1->i4_sad_me_for_ref[i4_i] = -1;
            ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i4_i] = -1;

            ps_ed_ctb_l1->i4_best_sad_8x8_l1_me_for_decide[i4_i] = -1;

            ps_ed_ctb_l1->i4_best_satd_8x8[i4_i] = -1;
            ps_ed_ctb_l1->i4_best_sad_cost_8x8_l1_ipe[i4_i] = -1;
            ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_i] = -1;
        }
    }

    ASSERT((num_4x4_blks_x & 1) == 0);
    ASSERT((num_4x4_blks_y & 1) == 0);
    for(i = 0; i < num_4x4_blks_y / 2; i++)
    {
        pu1_src_8x8 = pu1_src + i * 2 * 4 * src_stride;
        nbr_flags_ptr = &nbr_flags[0] + 2 * 8 * i;

        for(j = 0; j < num_4x4_blks_x / 2; j++)
        {
            WORD32 i4_best_satd;
            WORD32 i4_best_sad_cost_8x8_l1_ipe;
            WORD32 i4_best_sad_8x8_l1_ipe;

            z_scan_idx = gau1_ctb_raster_to_zscan[i * 2 * 16 + j * 2];
            z_scan_act_idx = gau1_ctb_raster_to_zscan[i * 16 + j];
            ASSERT(z_scan_act_idx <= 15);

            ps_ed_8x8 = ps_ed_ctb + z_scan_idx;
            ihevce_ed_calc_8x8_blk(
                ps_ed_ctxt,
                ps_ed_8x8,
                pu1_src_8x8,
                src_stride,
                nbr_flags_ptr,
                lambda,
                &i4_best_satd,
                i4_layer_id,
                ps_ed_ctxt->i4_quality_preset,
                &i4_best_sad_cost_8x8_l1_ipe,
                &i4_best_sad_8x8_l1_ipe,
                ps_ipe_optimised_function_list,
                ps_cmn_utils_optimised_function_list);
            ASSERT(i4_best_satd >= 0);

            if(i4_layer_id == 1)
            {
                ps_ed_ctb_l1->i4_best_sad_cost_8x8_l1_ipe[z_scan_act_idx] =
                    i4_best_sad_cost_8x8_l1_ipe;
                ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[z_scan_act_idx] = i4_best_sad_8x8_l1_ipe;
                ps_ed_ctb_l1->i4_best_satd_8x8[z_scan_act_idx] = i4_best_satd;
                ps_ed_ctxt->i8_sum_best_satd += i4_best_satd;
                ps_ed_ctxt->i8_sum_sq_best_satd += (i4_best_satd * i4_best_satd);
            }
            pu1_src_8x8 += 8;
            nbr_flags_ptr += 2;
        }
    }
}

float fast_log2(float val)
{
    union { float val; int32_t x; } u = { val };
    float log_2 = (float)(((u.x >> 23) & 255) - 128);

    u.x &= ~(255 << 23);
    u.x += 127 << 23;
    log_2 += ((-1.0f / 3) * u.val + 2) * u.val - 2.0f / 3;
    return log_2;
}

/*!
******************************************************************************
* \if Function name : ihevce_cu_level_qp_mod \endif
*
* \brief: Performs CU level QP modulation
*
*****************************************************************************
*/
WORD32 ihevce_cu_level_qp_mod(
    WORD32 frm_qscale,
    WORD32 cu_satd,
    long double frm_avg_activity,
    float f_mod_strength,
    WORD32 *pi4_act_factor,
    WORD32 *pi4_q_scale_mod,
    rc_quant_t *rc_quant_ctxt)
{
    WORD32 cu_qscale;
    WORD32 cu_qp;

    *pi4_act_factor = (1 << QP_LEVEL_MOD_ACT_FACTOR);
    if(cu_satd != -1 && (WORD32)frm_avg_activity != 0)
    {
        ULWORD64 sq_cur_satd = (cu_satd * cu_satd);
        float log2_sq_cur_satd = fast_log2(1 + sq_cur_satd);
        WORD32 qp_offset = f_mod_strength * (log2_sq_cur_satd - frm_avg_activity);

        ASSERT(USE_SQRT_AVG_OF_SATD_SQR);
        qp_offset = CLIP3(qp_offset, MIN_QP_MOD_OFFSET, MAX_QP_MOD_OFFSET);
        *pi4_act_factor *= gad_look_up_activity[qp_offset + ABS(MIN_QP_MOD_OFFSET)];
        ASSERT(*pi4_act_factor > 0);
        cu_qscale = ((frm_qscale * (*pi4_act_factor)) + (1 << (QP_LEVEL_MOD_ACT_FACTOR - 1)));
        cu_qscale >>= QP_LEVEL_MOD_ACT_FACTOR;
    }
    else
    {
        cu_qscale = frm_qscale;
    }
    cu_qscale = CLIP3(cu_qscale, rc_quant_ctxt->i2_min_qscale, rc_quant_ctxt->i2_max_qscale);
    cu_qp = rc_quant_ctxt->pi4_qscale_to_qp[cu_qscale];
    cu_qp = CLIP3(cu_qp, rc_quant_ctxt->i2_min_qp, rc_quant_ctxt->i2_max_qp);
    *pi4_q_scale_mod = cu_qscale;

    return (cu_qp);
}

/*!
******************************************************************************
* \if Function name : ihevce_ed_frame_init \endif
*
* \brief: Initialize frame context for early decision
*
*****************************************************************************
*/
void ihevce_ed_frame_init(void *pv_ed_ctxt, WORD32 i4_layer_no)
{
    ihevce_ed_ctxt_t *ps_ed_ctxt = (ihevce_ed_ctxt_t *)pv_ed_ctxt;

    g_apf_lum_ip[IP_FUNC_MODE_0] = ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_planar_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_1] = ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_dc_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_2] = ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode2_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_3TO9] =
        ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_3_to_9_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_10] = ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_horz_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_11TO17] =
        ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_11_to_17_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_18_34] =
        ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_18_34_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_19TO25] =
        ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_19_to_25_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_26] = ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_ver_fptr;
    g_apf_lum_ip[IP_FUNC_MODE_27TO33] =
        ps_ed_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_27_to_33_fptr;

    if(i4_layer_no == 1)
    {
        ps_ed_ctxt->i8_sum_best_satd = 0;
        ps_ed_ctxt->i8_sum_sq_best_satd = 0;
    }
}

/**
********************************************************************************
*
*  @brief  downscales by 2 in horz and vertical direction, creates output of
*          size wd/2 * ht/2
*
*  @param[in]  pu1_src : source pointer
*  @param[in]  src_stride : source stride
*  @param[out] pu1_dst : destination pointer. Starting of a row.
*  @param[in]  dst_stride : destination stride
*  @param[in]  wd : width
*  @param[in]  ht : height
*  @param[in]  pu1_wkg_mem : working memory (atleast of size CEIL16(wd) * ht))
*  @param[in]  ht_offset : height offset of the block to be scaled
*  @param[in]  block_ht : height of the block to be scaled
*  @param[in]  wd_offset : width offset of the block to be scaled
*  @param[in]  block_wd : width of the block to be scaled
*
*  @return void
*
*  @remarks Assumption made block_ht should me multiple of 2. LANCZOS_SCALER
*
********************************************************************************
*/
void ihevce_scaling_filter_mxn(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_scrtch,
    WORD32 scrtch_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 ht,
    WORD32 wd)
{
#define FILT_TAP_Q 8
#define N_TAPS 7
    const WORD16 i4_ftaps[N_TAPS] = { -18, 0, 80, 132, 80, 0, -18 };
    WORD32 i, j;
    WORD32 tmp;
    UWORD8 *pu1_src_tmp = pu1_src - 3 * src_strd;
    UWORD8 *pu1_scrtch_tmp = pu1_scrtch;

    /* horizontal filtering */
    for(i = -3; i < ht + 2; i++)
    {
        for(j = 0; j < wd; j += 2)
        {
            tmp = (i4_ftaps[3] * pu1_src_tmp[j] +
                   i4_ftaps[2] * (pu1_src_tmp[j - 1] + pu1_src_tmp[j + 1]) +
                   i4_ftaps[1] * (pu1_src_tmp[j + 2] + pu1_src_tmp[j - 2]) +
                   i4_ftaps[0] * (pu1_src_tmp[j + 3] + pu1_src_tmp[j - 3]) +
                   (1 << (FILT_TAP_Q - 1))) >>
                  FILT_TAP_Q;
            pu1_scrtch_tmp[j >> 1] = CLIP_U8(tmp);
        }
        pu1_scrtch_tmp += scrtch_strd;
        pu1_src_tmp += src_strd;
    }
    /* vertical filtering */
    pu1_scrtch_tmp = pu1_scrtch + 3 * scrtch_strd;
    for(i = 0; i < ht; i += 2)
    {
        for(j = 0; j < (wd >> 1); j++)
        {
            tmp =
                (i4_ftaps[3] * pu1_scrtch_tmp[j] +
                 i4_ftaps[2] * (pu1_scrtch_tmp[j + scrtch_strd] + pu1_scrtch_tmp[j - scrtch_strd]) +
                 i4_ftaps[1] *
                     (pu1_scrtch_tmp[j + 2 * scrtch_strd] + pu1_scrtch_tmp[j - 2 * scrtch_strd]) +
                 i4_ftaps[0] *
                     (pu1_scrtch_tmp[j + 3 * scrtch_strd] + pu1_scrtch_tmp[j - 3 * scrtch_strd]) +
                 (1 << (FILT_TAP_Q - 1))) >>
                FILT_TAP_Q;
            pu1_dst[j] = CLIP_U8(tmp);
        }
        pu1_dst += dst_strd;
        pu1_scrtch_tmp += (scrtch_strd << 1);
    }
}

void ihevce_scale_by_2(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_wkg_mem,
    WORD32 ht_offset,
    WORD32 block_ht,
    WORD32 wd_offset,
    WORD32 block_wd,
    FT_COPY_2D *pf_copy_2d,
    FT_SCALING_FILTER_BY_2 *pf_scaling_filter_mxn)
{
#define N_TAPS 7
#define MAX_BLK_SZ (MAX_CTB_SIZE + ((N_TAPS >> 1) << 1))
    UWORD8 au1_cpy[MAX_BLK_SZ * MAX_BLK_SZ];
    UWORD32 cpy_strd = MAX_BLK_SZ;
    UWORD8 *pu1_cpy = au1_cpy + cpy_strd * (N_TAPS >> 1) + (N_TAPS >> 1);

    UWORD8 *pu1_in, *pu1_out;
    WORD32 in_strd, wkg_mem_strd;

    WORD32 row_start, row_end;
    WORD32 col_start, col_end;
    WORD32 i, fun_select;
    WORD32 ht_tmp, wd_tmp;
    FT_SCALING_FILTER_BY_2 *ihevce_scaling_filters[2];

    assert((wd & 1) == 0);
    assert((ht & 1) == 0);
    assert(block_wd <= MAX_CTB_SIZE);
    assert(block_ht <= MAX_CTB_SIZE);

    /* function pointers for filtering different dimensions */
    ihevce_scaling_filters[0] = ihevce_scaling_filter_mxn;
    ihevce_scaling_filters[1] = pf_scaling_filter_mxn;

    /* handle boundary blks */
    col_start = (wd_offset < (N_TAPS >> 1)) ? 1 : 0;
    row_start = (ht_offset < (N_TAPS >> 1)) ? 1 : 0;
    col_end = ((wd_offset + block_wd) > (wd - (N_TAPS >> 1))) ? 1 : 0;
    row_end = ((ht_offset + block_ht) > (ht - (N_TAPS >> 1))) ? 1 : 0;
    if(col_end && (wd % block_wd != 0))
    {
        block_wd = (wd % block_wd);
    }
    if(row_end && (ht % block_ht != 0))
    {
        block_ht = (ht % block_ht);
    }

    /* boundary blks needs to be padded, copy src to tmp buffer */
    if(col_start || col_end || row_end || row_start)
    {
        UWORD8 *pu1_src_tmp = pu1_src + wd_offset + ht_offset * src_strd;

        pu1_cpy -= (3 * (1 - col_start) + cpy_strd * 3 * (1 - row_start));
        pu1_src_tmp -= (3 * (1 - col_start) + src_strd * 3 * (1 - row_start));
        ht_tmp = block_ht + 3 * (1 - row_start) + 3 * (1 - row_end);
        wd_tmp = block_wd + 3 * (1 - col_start) + 3 * (1 - col_end);
        pf_copy_2d(pu1_cpy, cpy_strd, pu1_src_tmp, src_strd, wd_tmp, ht_tmp);
        pu1_in = au1_cpy + cpy_strd * 3 + 3;
        in_strd = cpy_strd;
    }
    else
    {
        pu1_in = pu1_src + wd_offset + ht_offset * src_strd;
        in_strd = src_strd;
    }

    /*top padding*/
    if(row_start)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + cpy_strd * 3;

        pu1_cpy = au1_cpy + cpy_strd * (3 - 1);
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy -= cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy -= cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
    }

    /*bottom padding*/
    if(row_end)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + cpy_strd * 3 + (block_ht - 1) * cpy_strd;

        pu1_cpy = pu1_cpy_tmp + cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy += cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy += cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
    }

    /*left padding*/
    if(col_start)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + 3;

        pu1_cpy = au1_cpy;
        for(i = 0; i < block_ht + 6; i++)
        {
            pu1_cpy[0] = pu1_cpy[1] = pu1_cpy[2] = pu1_cpy_tmp[0];
            pu1_cpy += cpy_strd;
            pu1_cpy_tmp += cpy_strd;
        }
    }

    /*right padding*/
    if(col_end)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + 3 + block_wd - 1;

        pu1_cpy = au1_cpy + 3 + block_wd;
        for(i = 0; i < block_ht + 6; i++)
        {
            pu1_cpy[0] = pu1_cpy[1] = pu1_cpy[2] = pu1_cpy_tmp[0];
            pu1_cpy += cpy_strd;
            pu1_cpy_tmp += cpy_strd;
        }
    }

    wkg_mem_strd = block_wd >> 1;
    pu1_out = pu1_dst + (wd_offset >> 1);
    fun_select = (block_wd % 16 == 0);
    ihevce_scaling_filters[fun_select](
        pu1_in, in_strd, pu1_wkg_mem, wkg_mem_strd, pu1_out, dst_strd, block_ht, block_wd);

    /* Left padding of 16 for 1st block of every row */
    if(wd_offset == 0)
    {
        UWORD8 u1_val;
        WORD32 pad_wd = 16;
        WORD32 pad_ht = block_ht >> 1;
        UWORD8 *dst = pu1_dst;

        for(i = 0; i < pad_ht; i++)
        {
            u1_val = dst[0];
            memset(&dst[-pad_wd], u1_val, pad_wd);
            dst += dst_strd;
        }
    }

    if(wd == wd_offset + block_wd)
    {
        /* Right padding of (16 + (CEIL16(wd/2))-wd/2) for last block of every row */
        /* Right padding is done only after processing of last block of that row is done*/
        UWORD8 u1_val;
        WORD32 pad_wd = 16 + CEIL16((wd >> 1)) - (wd >> 1) + 4;
        WORD32 pad_ht = block_ht >> 1;
        UWORD8 *dst = pu1_dst + (wd >> 1) - 1;

        for(i = 0; i < pad_ht; i++)
        {
            u1_val = dst[0];
            memset(&dst[1], u1_val, pad_wd);
            dst += dst_strd;
        }

        if(ht_offset == 0)
        {
            /* Top padding of 16 is done for 1st row only after we reach end of that row */
            pad_wd = dst_strd;
            pad_ht = 16;
            dst = pu1_dst - 16;
            for(i = 1; i <= pad_ht; i++)
            {
                memcpy(dst - (i * dst_strd), dst, pad_wd);
            }
        }

        /* Bottom padding of (16 + (CEIL16(ht/2)) - ht/2) is done only if we have
         reached end of frame */
        if(ht - ht_offset - block_ht == 0)
        {
            pad_wd = dst_strd;
            pad_ht = 16 + CEIL16((ht >> 1)) - (ht >> 1) + 4;
            dst = pu1_dst + (((block_ht >> 1) - 1) * dst_strd) - 16;
            for(i = 1; i <= pad_ht; i++)
                memcpy(dst + (i * dst_strd), dst, pad_wd);
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_decomp_pre_intra_process_row \endif
*
* \brief
*  Row level function which down scales a given row by 2 in horz and vertical
*  direction creates output of size wd/2 * ht/2. When decomposition is done
*  from L1 to L2 pre intra analysis is done on L1
*
*****************************************************************************
*/
void ihevce_decomp_pre_intra_process_row(
    UWORD8 *pu1_src,
    WORD32 src_stride,
    UWORD8 *pu1_dst_decomp,
    WORD32 dst_stride,
    WORD32 layer_wd,
    WORD32 layer_ht,
    UWORD8 *pu1_wkg_mem,
    WORD32 ht_offset,
    WORD32 block_ht,
    WORD32 block_wd,
    WORD32 num_col_blks,
    WORD32 layer_no,
    ihevce_ed_ctxt_t *ps_ed_ctxt,
    ihevce_ed_blk_t *ps_ed_row,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_row,
    WORD32 num_4x4_blks_ctb_y,
    WORD32 num_4x4_blks_last_ctb_x,
    WORD32 skip_decomp,
    WORD32 skip_pre_intra,
    WORD32 row_block_no,
    ctb_analyse_t *ps_ctb_analyse,
    ihevce_ipe_optimised_function_list_t *ps_ipe_optimised_function_list,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list)
{
    WORD32 do_pre_intra_analysis = ((layer_no == 1) || (layer_no == 2)) && (!skip_pre_intra);
    WORD32 col_block_no;
    WORD32 i, j;

    if(!skip_decomp)
    {
        ctb_analyse_t *ps_ctb_analyse_curr = ps_ctb_analyse + row_block_no * num_col_blks;

        for(col_block_no = 0; col_block_no < num_col_blks; col_block_no++)
        {
            ihevce_scale_by_2(
                pu1_src,
                src_stride,
                pu1_dst_decomp,
                dst_stride,
                layer_wd,
                layer_ht,
                pu1_wkg_mem,
                ht_offset,
                block_ht,
                block_wd * col_block_no,
                block_wd,
                ps_cmn_utils_optimised_function_list->pf_copy_2d,
                ps_ipe_optimised_function_list->pf_scaling_filter_mxn);

            /* Disable noise detection */
            memset(
                ps_ctb_analyse_curr->s_ctb_noise_params.au1_is_8x8Blk_noisy,
                0,
                sizeof(ps_ctb_analyse_curr->s_ctb_noise_params.au1_is_8x8Blk_noisy));

            ps_ctb_analyse_curr->s_ctb_noise_params.i4_noise_present = 0;

            ps_ctb_analyse_curr++;
        }
    }

    if(do_pre_intra_analysis)
    {
        ihevce_ed_blk_t *ps_ed_ctb = ps_ed_row;
        ihevce_ed_ctb_l1_t *ps_ed_ctb_l1 = ps_ed_ctb_l1_row;
        WORD32 *nbr_flags_ptr = &ps_ed_ctxt->ai4_nbr_flags[0];
        UWORD8 *pu1_src_pre_intra = pu1_src + (ht_offset * src_stride);
        WORD32 num_4x4_blks_in_ctb = block_wd >> 2;
        WORD32 src_inc_pre_intra = num_4x4_blks_in_ctb * 4;
        WORD32 inc_ctb = num_4x4_blks_in_ctb * num_4x4_blks_in_ctb;

        /* To analyse any given CTB we need to set the availability flags of the
         * following neighbouring CTB: BL,L,TL,T,TR */
        /* copy the neighbor flags for a general ctb (ctb inside the frame); not any corners */
        memcpy(
            ps_ed_ctxt->ai4_nbr_flags,
            gau4_nbr_flags_8x8_4x4blks,
            sizeof(gau4_nbr_flags_8x8_4x4blks));

        /* set top flags unavailable for first ctb row */
        if(ht_offset == 0)
        {
            for(j = 0; j < num_4x4_blks_in_ctb; j++)
            {
                SET_T_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j]);
                SET_TR_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j]);
                SET_TL_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j]);
            }
        }

        /* set bottom left flags as not available for last row */
        if(ht_offset + block_ht >= layer_ht)
        {
            for(j = 0; j < num_4x4_blks_in_ctb; j++)
            {
                SET_BL_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[(num_4x4_blks_ctb_y - 1) * 8 + j]);
            }
        }

        /* set left flags unavailable for 1st ctb col */
        for(j = 0; j < num_4x4_blks_ctb_y; j++)
        {
            SET_L_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j * 8]);
            SET_BL_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j * 8]);
            SET_TL_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j * 8]);
        }

        for(col_block_no = 0; col_block_no < num_col_blks; col_block_no++)
        {
            if(col_block_no == 1)
            {
                /* For the rest of the ctbs, set left flags available */
                for(j = 0; j < num_4x4_blks_ctb_y; j++)
                {
                    SET_L_AVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j * 8]);
                }
                for(j = 0; j < num_4x4_blks_ctb_y - 1; j++)
                {
                    SET_BL_AVAILABLE(ps_ed_ctxt->ai4_nbr_flags[j * 8]);
                    SET_TL_AVAILABLE(ps_ed_ctxt->ai4_nbr_flags[(j + 1) * 8]);
                }
                if(ht_offset != 0)
                {
                    SET_TL_AVAILABLE(ps_ed_ctxt->ai4_nbr_flags[0]);
                }
            }

            if(col_block_no == num_col_blks - 1)
            {
                /* set top right flags unavailable for last ctb col */
                for(i = 0; i < num_4x4_blks_ctb_y; i++)
                {
                    SET_TR_UNAVAILABLE(ps_ed_ctxt->ai4_nbr_flags[i * 8 + num_4x4_blks_last_ctb_x - 1]);
                }
            }

            /* Call intra analysis for the ctb */
            ihevce_ed_calc_ctb(
                ps_ed_ctxt,
                ps_ed_ctb,
                ps_ed_ctb_l1,
                pu1_src_pre_intra,
                src_stride,
                (col_block_no == num_col_blks - 1) ? num_4x4_blks_last_ctb_x : num_4x4_blks_in_ctb,
                num_4x4_blks_ctb_y,
                nbr_flags_ptr,
                layer_no,
                ps_ipe_optimised_function_list,
                ps_cmn_utils_optimised_function_list);
            pu1_src_pre_intra += src_inc_pre_intra;
            ps_ed_ctb += inc_ctb;
            ps_ed_ctb_l1 += 1;
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_decomp_pre_intra_process \endif
*
* \brief
*  Frame level function to decompose given layer L0 into coarser layers and
*  perform intra analysis on layers below L0
*
*****************************************************************************
*/
void ihevce_decomp_pre_intra_process(
    void *pv_ctxt,
    ihevce_lap_output_params_t *ps_lap_out_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    void *pv_multi_thrd_ctxt,
    WORD32 thrd_id,
    WORD32 i4_ping_pong)
{
    ihevce_decomp_pre_intra_master_ctxt_t *ps_master_ctxt = pv_ctxt;
    ihevce_decomp_pre_intra_ctxt_t *ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[thrd_id];
    multi_thrd_ctxt_t *ps_multi_thrd = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;
    WORD32 i4_num_layers = ps_ctxt->i4_num_layers;
    UWORD8 *pu1_wkg_mem = ps_ctxt->au1_wkg_mem;
    ihevce_ed_ctxt_t *ps_ed_ctxt = ps_ctxt->ps_ed_ctxt;
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1 = ps_ed_ctxt->ps_ed_ctb_l1;
    ihevce_ed_blk_t *ps_ed;
    WORD32 i4_layer_no;
    WORD32 end_of_layer;
    UWORD8 *pu1_src, *pu1_dst;
    WORD32 src_stride, dst_stride;
    WORD32 i4_layer_wd, i4_layer_ht;
    WORD32 ht_offset, block_ht, row_block_no, num_row_blocks;
    WORD32 block_wd, num_col_blks;
    WORD32 skip_decomp, skip_pre_intra;
    WORD32 inc_ctb;

    ASSERT(i4_num_layers >= 3);
    ps_ctxt->as_layers[0].pu1_inp = (UWORD8 *)ps_lap_out_prms->s_input_buf.pv_y_buf;
    ps_ctxt->as_layers[0].i4_inp_stride = ps_lap_out_prms->s_input_buf.i4_y_strd;
    ps_ctxt->as_layers[0].i4_actual_wd = ps_lap_out_prms->s_input_buf.i4_y_wd;
    ps_ctxt->as_layers[0].i4_actual_ht = ps_lap_out_prms->s_input_buf.i4_y_ht;

    /* This loop does decomp & intra by picking jobs from job queue */
    for(i4_layer_no = 0; i4_layer_no < i4_num_layers; i4_layer_no++)
    {
        WORD32 idx = 0;

        src_stride = ps_ctxt->as_layers[i4_layer_no].i4_inp_stride;
        pu1_src = ps_ctxt->as_layers[i4_layer_no].pu1_inp;
        i4_layer_wd = ps_ctxt->as_layers[i4_layer_no].i4_actual_wd;
        i4_layer_ht = ps_ctxt->as_layers[i4_layer_no].i4_actual_ht;
        pu1_dst = ps_ctxt->as_layers[i4_layer_no + 1].pu1_inp;
        dst_stride = ps_ctxt->as_layers[i4_layer_no + 1].i4_inp_stride;
        block_wd = ps_ctxt->as_layers[i4_layer_no].i4_decomp_blk_wd;
        block_ht = ps_ctxt->as_layers[i4_layer_no].i4_decomp_blk_ht;
        num_col_blks = ps_ctxt->as_layers[i4_layer_no].i4_num_col_blks;
        num_row_blocks = ps_ctxt->as_layers[i4_layer_no].i4_num_row_blks;
        inc_ctb = (block_wd >> 2) * (block_wd >> 2);
        end_of_layer = 0;
        skip_pre_intra = 1;
        skip_decomp = 0;
        if(i4_layer_no >= (ps_ctxt->i4_num_layers - 1))
        {
            skip_decomp = 1;
        }

        /* ------------ Loop over all the CTB rows & perform Decomp --------------- */
        while(0 == end_of_layer)
        {
            job_queue_t *ps_pre_enc_job;
            WORD32 num_4x4_blks_ctb_y = 0, num_4x4_blks_last_ctb_x = 0;

            /* Get the current row from the job queue */
            ps_pre_enc_job = (job_queue_t *)ihevce_pre_enc_grp_get_next_job(
                pv_multi_thrd_ctxt, (DECOMP_JOB_LYR0 + i4_layer_no), 1, i4_ping_pong);

            /* If all rows are done, set the end of layer flag to 1, */
            if(NULL == ps_pre_enc_job)
            {
                end_of_layer = 1;
            }
            else
            {
                /* Obtain the current row's details from the job */
                row_block_no = ps_pre_enc_job->s_job_info.s_decomp_job_info.i4_vert_unit_row_no;
                ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx] = row_block_no;
                ht_offset = row_block_no * block_ht;

                if(row_block_no < (num_row_blocks))
                {
                    pu1_dst = ps_ctxt->as_layers[i4_layer_no + 1].pu1_inp +
                              ((block_ht >> 1) * dst_stride * row_block_no);

                    /* call the row level processing function */
                    ihevce_decomp_pre_intra_process_row(
                        pu1_src,
                        src_stride,
                        pu1_dst,
                        dst_stride,
                        i4_layer_wd,
                        i4_layer_ht,
                        pu1_wkg_mem,
                        ht_offset,
                        block_ht,
                        block_wd,
                        num_col_blks,
                        i4_layer_no,
                        ps_ed_ctxt,
                        ps_ed,
                        ps_ed_ctb_l1,
                        num_4x4_blks_ctb_y,
                        num_4x4_blks_last_ctb_x,
                        skip_decomp,
                        skip_pre_intra,
                        row_block_no,
                        ps_ctxt->ps_ctb_analyse,
                        &ps_ctxt->s_ipe_optimised_function_list,
                        &ps_ctxt->s_cmn_opt_func);
                }
                idx++;
                /* set the output dependency */
                ihevce_pre_enc_grp_job_set_out_dep(
                    pv_multi_thrd_ctxt, ps_pre_enc_job, i4_ping_pong);
            }
        }
        ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed = idx;

        /* ------------ For the same rows perform preintra if required --------------- */
        ihevce_ed_frame_init(ps_ed_ctxt, i4_layer_no);

        if((1 == i4_layer_no) && (IHEVCE_QUALITY_P6 == ps_ctxt->i4_quality_preset))
        {
            WORD32 vert_ctr, ctb_ctr, i;
            WORD32 ctb_ctr_blks = ps_ctxt->as_layers[1].i4_num_col_blks;
            WORD32 vert_ctr_blks = ps_ctxt->as_layers[1].i4_num_row_blks;

            if((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6) &&
               (ps_lap_out_prms->i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
            {
                for(vert_ctr = 0; vert_ctr < vert_ctr_blks; vert_ctr++)
                {
                    ihevce_ed_ctb_l1_t *ps_ed_ctb_row_l1 =
                        ps_ctxt->ps_ed_ctb_l1 + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;

                    for(ctb_ctr = 0; ctb_ctr < ctb_ctr_blks; ctb_ctr++)
                    {
                        ihevce_ed_ctb_l1_t *ps_ed_ctb_curr_l1 = ps_ed_ctb_row_l1 + ctb_ctr;

                        for(i = 0; i < 16; i++)
                        {
                            ps_ed_ctb_curr_l1->i4_best_sad_cost_8x8_l1_ipe[i] = 0x7fffffff;
                            ps_ed_ctb_curr_l1->i4_best_sad_8x8_l1_ipe[i] = 0x7fffffff;
                        }
                    }
                }
            }
        }

#if DISABLE_L2_IPE_IN_PB_L1_IN_B
        if(((2 == i4_layer_no) && (ps_lap_out_prms->i4_pic_type == IV_I_FRAME ||
                                   ps_lap_out_prms->i4_pic_type == IV_IDR_FRAME)) ||
           ((1 == i4_layer_no) &&
            (ps_lap_out_prms->i4_temporal_lyr_id <= TEMPORAL_LAYER_DISABLE)) ||
           ((IHEVCE_QUALITY_P6 != ps_ctxt->i4_quality_preset) && (0 != i4_layer_no)))
#else
        if((0 != i4_layer_no) &&
           (1 != ((IHEVCE_QUALITY_P6 == ps_ctxt->i4_quality_preset) &&
                  (ps_lap_out_prms->i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))))
#endif
        {
            WORD32 i4_num_rows = ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed;

            ps_ed_ctxt->lambda = ps_ctxt->ai4_lambda[i4_layer_no];
            if(0 == i4_layer_no)
            {
                ps_ed_ctxt->ps_ed_pic = NULL;
                ps_ed_ctxt->ps_ed = NULL;
                ps_ed_ctxt->ps_ed_ctb_l1_pic = NULL;
                ps_ed_ctxt->ps_ed_ctb_l1 = NULL;
            }
            else if(1 == i4_layer_no)
            {
                ps_ed_ctxt->ps_ed_pic = ps_ctxt->ps_layer1_buf;
                ps_ed_ctxt->ps_ed = ps_ctxt->ps_layer1_buf;
                ps_ed_ctxt->ps_ed_ctb_l1_pic = ps_ctxt->ps_ed_ctb_l1;
                ps_ed_ctxt->ps_ed_ctb_l1 = ps_ctxt->ps_ed_ctb_l1;
            }
            else if(2 == i4_layer_no)
            {
                ps_ed_ctxt->ps_ed_pic = ps_ctxt->ps_layer2_buf;
                ps_ed_ctxt->ps_ed = ps_ctxt->ps_layer2_buf;
                ps_ed_ctxt->ps_ed_ctb_l1_pic = NULL;
                ps_ed_ctxt->ps_ed_ctb_l1 = NULL;
            }

            skip_decomp = 1;
            skip_pre_intra = 0;

            for(idx = 0; idx < i4_num_rows; idx++)
            {
                WORD32 num_4x4_blks_ctb_y = 0, num_4x4_blks_last_ctb_x = 0;

                /* Obtain the current row's details from the job */
                row_block_no = ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx];
                ht_offset = row_block_no * block_ht;

                if(row_block_no < (num_row_blocks))
                {
                    pu1_dst = ps_ctxt->as_layers[i4_layer_no + 1].pu1_inp +
                              ((block_ht >> 1) * dst_stride * row_block_no);

                    if(i4_layer_no == 1 || i4_layer_no == 2)
                    {
                        ps_ed = ps_ed_ctxt->ps_ed + (row_block_no * inc_ctb * (num_col_blks));
                        ps_ed_ctb_l1 = ps_ed_ctxt->ps_ed_ctb_l1 + (row_block_no * num_col_blks);
                        ps_ed_ctxt->i4_quality_preset = ps_ctxt->i4_quality_preset;
                        num_4x4_blks_last_ctb_x = block_wd >> 2;
                        num_4x4_blks_ctb_y = block_ht >> 2;
                        if(row_block_no == num_row_blocks - 1)
                        {
                            if(i4_layer_ht % block_ht)
                            {
                                num_4x4_blks_ctb_y = ((i4_layer_ht % block_ht) + 3) >> 2;
                            }
                        }
                        if(i4_layer_wd % block_wd)
                        {
                            num_4x4_blks_last_ctb_x = ((i4_layer_wd % block_wd) + 3) >> 2;
                        }
                    }

                    /* call the row level processing function */
                    ihevce_decomp_pre_intra_process_row(
                        pu1_src,
                        src_stride,
                        pu1_dst,
                        dst_stride,
                        i4_layer_wd,
                        i4_layer_ht,
                        pu1_wkg_mem,
                        ht_offset,
                        block_ht,
                        block_wd,
                        num_col_blks,
                        i4_layer_no,
                        ps_ed_ctxt,
                        ps_ed,
                        ps_ed_ctb_l1,
                        num_4x4_blks_ctb_y,
                        num_4x4_blks_last_ctb_x,
                        skip_decomp,
                        skip_pre_intra,
                        row_block_no,
                        NULL,
                        &ps_ctxt->s_ipe_optimised_function_list,
                        &ps_ctxt->s_cmn_opt_func);
                }

                if(1 == i4_layer_no)
                {
                    ps_multi_thrd->aai4_l1_pre_intra_done[i4_ping_pong][row_block_no] = 1;
                }
            }
            for(idx = 0; idx < MAX_NUM_CTB_ROWS_FRM; idx++)
            {
                ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx] = -1;
            }
            ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed = 0;
        }

#if DISABLE_L2_IPE_IN_PB_L1_IN_B
        if((IHEVCE_QUALITY_P6 == ps_ctxt->i4_quality_preset) &&
           (((i4_layer_no == 2) && (ps_lap_out_prms->i4_pic_type == ISLICE)) ||
            ((i4_layer_no == 1) && (ps_lap_out_prms->i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))))
        {
            WORD32 i4_num_rows = ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed;
            if(1 == i4_layer_no)
            {
                for(idx = 0; idx < i4_num_rows; idx++)
                {
                    row_block_no = ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx];

                    {
                        ps_multi_thrd->aai4_l1_pre_intra_done[i4_ping_pong][row_block_no] = 1;
                    }
                }
            }
            for(idx = 0; idx < MAX_NUM_CTB_ROWS_FRM; idx++)
            {
                ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx] = -1;
            }
            ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed = 0;
        }
#else
        if((i4_layer_no != 0) && ((IHEVCE_QUALITY_P6 == ps_ctxt->i4_quality_preset) &&
                                  (ps_lap_out_prms->i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE)))
        {
            WORD32 i4_num_rows = ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed;
            for(idx = 0; idx < i4_num_rows; idx++)
            {
                row_block_no = ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx];
                if(1 == i4_layer_no)
                {
                    ps_multi_thrd->aai4_l1_pre_intra_done[i4_ping_pong][row_block_no] = 1;
                }
            }
            for(idx = 0; idx < MAX_NUM_CTB_ROWS_FRM; idx++)
            {
                ps_ctxt->as_layers[i4_layer_no].ai4_curr_row_no[idx] = -1;
            }
            ps_ctxt->as_layers[i4_layer_no].i4_num_rows_processed = 0;
        }
#endif
    }
}

/*!
************************************************************************
* \brief
*    return number of records used by decomp pre intra
*
************************************************************************
*/
WORD32 ihevce_decomp_pre_intra_get_num_mem_recs(void)
{
    return (NUM_DECOMP_PRE_INTRA_MEM_RECS);
}

/*!
************************************************************************
* @brief
*    return each record attributes of  decomp pre intra
************************************************************************
*/
WORD32 ihevce_decomp_pre_intra_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab, WORD32 i4_num_proc_thrds, WORD32 i4_mem_space)
{
    /* memories should be requested assuming worst case requirememnts */

    /* Module context structure */
    ps_mem_tab[DECOMP_PRE_INTRA_CTXT].i4_mem_size = sizeof(ihevce_decomp_pre_intra_master_ctxt_t);
    ps_mem_tab[DECOMP_PRE_INTRA_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DECOMP_PRE_INTRA_CTXT].i4_mem_alignment = 8;

    /* Thread context structure */
    ps_mem_tab[DECOMP_PRE_INTRA_THRDS_CTXT].i4_mem_size =
        i4_num_proc_thrds * sizeof(ihevce_decomp_pre_intra_ctxt_t);
    ps_mem_tab[DECOMP_PRE_INTRA_THRDS_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DECOMP_PRE_INTRA_THRDS_CTXT].i4_mem_alignment = 8;

    /* early decision context structure */
    ps_mem_tab[DECOMP_PRE_INTRA_ED_CTXT].i4_mem_size = i4_num_proc_thrds * sizeof(ihevce_ed_ctxt_t);
    ps_mem_tab[DECOMP_PRE_INTRA_ED_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DECOMP_PRE_INTRA_ED_CTXT].i4_mem_alignment = 8;

    return (NUM_DECOMP_PRE_INTRA_MEM_RECS);
}

/*!
************************************************************************
* @brief
*    Init decomp pre intra context
************************************************************************
*/
void *ihevce_decomp_pre_intra_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    func_selector_t *ps_func_selector,
    WORD32 i4_resolution_id,
    UWORD8 u1_is_popcnt_available)
{
    ihevce_decomp_pre_intra_master_ctxt_t *ps_mstr_ctxt = ps_mem_tab[DECOMP_PRE_INTRA_CTXT].pv_base;
    ihevce_decomp_pre_intra_ctxt_t *ps_ctxt = ps_mem_tab[DECOMP_PRE_INTRA_THRDS_CTXT].pv_base;
    ihevce_ed_ctxt_t *ps_ed_ctxt = ps_mem_tab[DECOMP_PRE_INTRA_ED_CTXT].pv_base;
    ihevce_tgt_params_t *ps_tgt_prms = &ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id];
    WORD32 min_cu_size = 1 << ps_init_prms->s_config_prms.i4_min_log2_cu_size;
    WORD32 a_wd[MAX_NUM_HME_LAYERS], a_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_wd[MAX_NUM_LAYERS], a_disp_ht[MAX_NUM_LAYERS];
    WORD32 n_tot_layers;
    WORD32 i, j, k;

    /* Get the height and width of each layer */
    *a_wd = ps_tgt_prms->i4_width + SET_CTB_ALIGN(ps_tgt_prms->i4_width, min_cu_size);
    *a_ht = ps_tgt_prms->i4_height + SET_CTB_ALIGN(ps_tgt_prms->i4_height, min_cu_size);
    n_tot_layers = hme_derive_num_layers(1, a_wd, a_ht, a_disp_wd, a_disp_ht);
    ps_mstr_ctxt->i4_num_proc_thrds = i4_num_proc_thrds;
    for(i = 0; i < ps_mstr_ctxt->i4_num_proc_thrds; i++)
    {
        ps_mstr_ctxt->aps_decomp_pre_intra_thrd_ctxt[i] = ps_ctxt;
        ps_ctxt->i4_num_layers = n_tot_layers;
        ps_ctxt->ps_ed_ctxt = ps_ed_ctxt;
        for(j = 0; j < n_tot_layers; j++)
        {
            /** If CTB size= 64, decomp_blk_wd = 64 for L0, 32 for L1 , 16 for L2, 8 for L3 */
            WORD32 max_ctb_size = 1 << ps_init_prms->s_config_prms.i4_max_log2_cu_size;
            WORD32 decomp_blk_wd = max_ctb_size >> j;
            WORD32 decomp_blk_ht = max_ctb_size >> j;

            ps_ctxt->as_layers[j].i4_actual_wd = a_wd[j];
            ps_ctxt->as_layers[j].i4_actual_ht = a_ht[j];
            if(0 == j)
            {
                ps_ctxt->as_layers[j].i4_padded_ht = a_ht[j];
                ps_ctxt->as_layers[j].i4_padded_wd = a_wd[j];
            }
            else
            {
                ps_ctxt->as_layers[j].i4_padded_ht = a_ht[j] + 32 + 4;
                ps_ctxt->as_layers[j].i4_padded_wd = a_wd[j] + 32 + 4;
            }
            ps_ctxt->as_layers[j].pu1_inp = NULL;
            ps_ctxt->as_layers[j].i4_inp_stride = 0;
            ps_ctxt->as_layers[j].i4_decomp_blk_ht = decomp_blk_ht;
            ps_ctxt->as_layers[j].i4_decomp_blk_wd = decomp_blk_wd;
            ps_ctxt->as_layers[j].i4_num_row_blks = ((a_ht[j] + (decomp_blk_ht - 1)) / decomp_blk_ht);
            ps_ctxt->as_layers[j].i4_num_col_blks = ((a_wd[j] + (decomp_blk_wd - 1)) / decomp_blk_wd);
            for(k = 0; k < MAX_NUM_CTB_ROWS_FRM; k++)
            {
                ps_ctxt->as_layers[j].ai4_curr_row_no[k] = -1;
            }
            ps_ctxt->as_layers[j].i4_num_rows_processed = 0;
        }
        ps_ctxt->i4_quality_preset = ps_tgt_prms->i4_quality_preset;
        if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P7)
        {
            ps_ctxt->i4_quality_preset = IHEVCE_QUALITY_P6;
        }
        if(ps_init_prms->s_coding_tools_prms.i4_vqet &
           (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER))
        {
            if(ps_init_prms->s_coding_tools_prms.i4_vqet &
               (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION))
            {
                ps_ctxt->i4_enable_noise_detection = 1;
            }
            else
            {
                ps_ctxt->i4_enable_noise_detection = 0;
            }
        }
        else
        {
            ps_ctxt->i4_enable_noise_detection = 0;
        }
        ihevce_cmn_utils_instr_set_router(
            &ps_ctxt->s_cmn_opt_func, u1_is_popcnt_available, ps_init_prms->e_arch_type);
        ihevce_ipe_instr_set_router(
            &ps_ctxt->s_ipe_optimised_function_list, ps_init_prms->e_arch_type);

        ps_ed_ctxt->ps_func_selector = ps_func_selector;

        ps_ctxt++;
        ps_ed_ctxt++;
    }
    /* return the handle to caller */
    return ((void *)ps_mstr_ctxt);
}

/*!
************************************************************************
* @brief
*    Init decomp pre intra layer buffers
************************************************************************
*/
void ihevce_decomp_pre_intra_frame_init(
    void *pv_ctxt,
    UWORD8 **ppu1_decomp_lyr_bufs,
    WORD32 *pi4_lyr_buf_stride,
    ihevce_ed_blk_t *ps_layer1_buf,
    ihevce_ed_blk_t *ps_layer2_buf,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    WORD32 i4_ol_sad_lambda_qf,
    ctb_analyse_t *ps_ctb_analyse)
{
    ihevce_decomp_pre_intra_master_ctxt_t *ps_master_ctxt = pv_ctxt;
    ihevce_decomp_pre_intra_ctxt_t *ps_ctxt;
    WORD32 i, j;

    for(i = 0; i < ps_master_ctxt->i4_num_proc_thrds; i++)
    {
        ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[i];

        /* L0 layer (actual input) is registered in process call */
        for(j = 1; j < ps_ctxt->i4_num_layers; j++)
        {
            ps_ctxt->as_layers[j].i4_inp_stride = pi4_lyr_buf_stride[j - 1];
            ps_ctxt->as_layers[j].pu1_inp = ppu1_decomp_lyr_bufs[j - 1];

            /* Populating the buffer pointers for layer1 and layer2 buffers to store the
            structure for each 4x4 block after pre intra analysis on their respective layers */
            if(j == 1)
            {
                WORD32 sad_lambda_l1 = (3 * i4_ol_sad_lambda_qf >> 2);
                WORD32 temp = 1 << LAMBDA_Q_SHIFT;
                WORD32 lambda = ((temp) > sad_lambda_l1) ? temp : sad_lambda_l1;

                ps_ctxt->ps_layer1_buf = ps_layer1_buf;
                ps_ctxt->ps_ed_ctb_l1 = ps_ed_ctb_l1;
                ps_ctxt->ai4_lambda[j] = lambda;
            }
            else if(j == 2)
            {
                WORD32 sad_lambda_l2 = i4_ol_sad_lambda_qf >> 1;
                WORD32 temp = 1 << LAMBDA_Q_SHIFT;
                WORD32 lambda = ((temp) > sad_lambda_l2) ? temp : sad_lambda_l2;

                ps_ctxt->ps_layer2_buf = ps_layer2_buf;
                ps_ctxt->ai4_lambda[j] = lambda;
            }
            else
            {
                ps_ctxt->ai4_lambda[j] = -1;
            }
        }

        /* make the ps_ctb_analyse refernce as a part of the private context */
        ps_ctxt->ps_ctb_analyse = ps_ctb_analyse;
    }
}

/**
*******************************************************************************
*
* @brief Merge Sort function.
*
* @par Description:
*     This function sorts the data in the input array in ascending
*     order using merge sort algorithm. Intermediate data obtained in
*     merge sort are stored in output 2-D array.
*
* @param[in]
*   pi4_input_val  :   Input 1-D array
*   aai4_output_val:   Output 2-D array containing elements sorted in sets of
*                      4,16,64 etc.
*   i4_length      : length of the array
*   i4_ip_sort_level: Input sort level. Specifies the level upto which array is sorted.
*                     It should be 1 if the array is unsorted. Should be 4 if array is sorted
*                     in sets of 4.
*   i4_op_sort_level: Output sort level. Specify the level upto which sorting is required.
*                     If it is given as length of array it sorts for whole array.
*
*******************************************************************************
*/
void ihevce_merge_sort(
    WORD32 *pi4_input_val,
    WORD32 aai4_output_val[][64],
    WORD32 i4_length,
    WORD32 i4_ip_sort_level,
    WORD32 i4_op_sort_level)
{
    WORD32 i, j, k;
    WORD32 count, level;
    WORD32 temp[64];
    WORD32 *pi4_temp_buf_cpy;
    WORD32 *pi4_temp = &temp[0];
    WORD32 calc_level;

    pi4_temp_buf_cpy = pi4_temp;

    GETRANGE(calc_level, i4_op_sort_level / i4_ip_sort_level);

    calc_level = calc_level - 1;

    /*** This function is written under the assumption that we need only intermediate values of
    sort in the range of 4,16,64 etc. ***/
    ASSERT((calc_level % 2) == 0);

    /** One iteration of this for loop does 1 sets of sort and produces one intermediate value in 2 iterations **/
    for(level = 0; level < calc_level; level++)
    {
        /** Merges adjacent sets of elements based on current sort level **/
        for(count = 0; count < i4_length; (count = count + (i4_ip_sort_level * 2)))
        {
            i = 0;
            j = 0;
            if(pi4_input_val[i4_ip_sort_level - 1] < pi4_input_val[i4_ip_sort_level])
            {
                /*** Condition for early exit ***/
                memcpy(&pi4_temp[0], pi4_input_val, sizeof(WORD32) * i4_ip_sort_level * 2);
            }
            else
            {
                for(k = 0; k < (i4_ip_sort_level * 2); k++)
                {
                    if((i < i4_ip_sort_level) && (j < i4_ip_sort_level))
                    {
                        if(pi4_input_val[i] > pi4_input_val[j + i4_ip_sort_level])
                        {
                            /** copy to output array **/
                            pi4_temp[k] = pi4_input_val[j + i4_ip_sort_level];
                            j++;
                        }
                        else
                        {
                            /** copy to output array **/
                            pi4_temp[k] = pi4_input_val[i];
                            i++;
                        }
                    }
                    else if(i == i4_ip_sort_level)
                    {
                        /** copy the remaining data to output array **/
                        pi4_temp[k] = pi4_input_val[j + i4_ip_sort_level];
                        j++;
                    }
                    else
                    {
                        /** copy the remaining data to output array **/
                        pi4_temp[k] = pi4_input_val[i];
                        i++;
                    }
                }
            }
            pi4_input_val += (i4_ip_sort_level * 2);
            pi4_temp += (i4_ip_sort_level * 2);
        }
        pi4_input_val = pi4_temp - i4_length;

        if(level % 2)
        {
            /** Assign a temp address for storing next sort level output as we will not need this data as output **/
            pi4_temp = pi4_temp_buf_cpy;
        }
        else
        {
            /** Assign address for storing the intermediate data into output 2-D array **/
            pi4_temp = aai4_output_val[level / 2];
        }
        i4_ip_sort_level *= 2;
    }
}

/*!
************************************************************************
* @brief
*   Calculate the average activities at 16*16 (8*8 in L1) and 32*32
*   (8*8 in L2) block sizes. As this function accumulates activities
*   across blocks of a frame, this needs to be called by only one thread
*   and only after ensuring the processing of entire frame is done
************************************************************************
*/
void ihevce_decomp_pre_intra_curr_frame_pre_intra_deinit(
    void *pv_pre_intra_ctxt,
    pre_enc_me_ctxt_t *ps_curr_out,
    frm_ctb_ctxt_t *ps_frm_ctb_prms)
{
    ihevce_decomp_pre_intra_master_ctxt_t *ps_master_ctxt = pv_pre_intra_ctxt;
    ihevce_decomp_pre_intra_ctxt_t *ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[0];

    ULWORD64 u8_frame_8x8_sum_act_sqr = 0;
    LWORD64 ai8_frame_8x8_sum_act_sqr[2] = { 0, 0 };
    WORD32 ai4_frame_8x8_sum_act[2] = { 0, 0 };
    WORD32 ai4_frame_8x8_sum_blks[2] = { 0, 0 };

    LWORD64 ai8_frame_16x16_sum_act_sqr[3] = { 0, 0, 0 };
    WORD32 ai4_frame_16x16_sum_act[3] = { 0, 0, 0 };
    WORD32 ai4_frame_16x16_sum_blks[3] = { 0, 0, 0 };

    LWORD64 ai8_frame_32x32_sum_act_sqr[3] = { 0, 0, 0 };
    WORD32 ai4_frame_32x32_sum_act[3] = { 0, 0, 0 };
    WORD32 ai4_frame_32x32_sum_blks[3] = { 0, 0, 0 };

    ihevce_ed_ctb_l1_t *ps_ed_ctb_pic_l1 = ps_curr_out->ps_ed_ctb_l1;
    ihevce_ed_blk_t *ps_ed_blk_l1 = ps_curr_out->ps_layer1_buf;
    WORD32 ctb_wd = ps_ctxt->as_layers[1].i4_decomp_blk_wd;
    WORD32 h_ctb_cnt = ps_ctxt->as_layers[1].i4_num_col_blks;
    WORD32 v_ctb_cnt = ps_ctxt->as_layers[1].i4_num_row_blks;
    WORD32 sub_blk_cnt = ((ctb_wd >> 2) * (ctb_wd >> 2));
    WORD32 i4_avg_noise_satd;
    WORD32 ctb_ctr, vert_ctr;
    WORD32 i, j, k;

    {
        /* Calculate min noise threshold */
        /* Min noise threshold is calculated by taking average of lowest 1% satd val in
         * the complete 4x4 frame satds */
#define MAX_SATD 64
#define SATD_NOISE_FLOOR_THRESHOLD 16
#define MIN_BLKS 2
        WORD32 i4_layer_wd = ps_ctxt->as_layers[1].i4_actual_wd;
        WORD32 i4_layer_ht = ps_ctxt->as_layers[1].i4_actual_ht;
        WORD32 i4_min_blk = ((MIN_BLKS * (i4_layer_wd >> 1) * (i4_layer_ht >> 1)) / 100);
        WORD32 i4_total_blks = 0;
        WORD32 satd_hist[MAX_SATD];
        LWORD64 i8_acc_satd = 0;

        memset(satd_hist, 0, sizeof(satd_hist));
        for(i = 0; i < sub_blk_cnt * h_ctb_cnt * v_ctb_cnt; i++)
        {
            if(ps_ed_blk_l1[i].i4_4x4_satd >= 0 && ps_ed_blk_l1[i].i4_4x4_satd < MAX_SATD)
            {
                satd_hist[ps_ed_blk_l1[i].i4_4x4_satd]++;
            }
        }
        for(i = 0; i < MAX_SATD && i4_total_blks <= i4_min_blk; i++)
        {
            i4_total_blks += satd_hist[i];
            i8_acc_satd += (i * satd_hist[i]);
        }
        if(i4_total_blks < i4_min_blk)
        {
            i4_avg_noise_satd = SATD_NOISE_FLOOR_THRESHOLD;
        }
        else
        {
            i4_avg_noise_satd = (WORD32)(i8_acc_satd + (i4_total_blks >> 1)) / i4_total_blks;
        }
        ps_curr_out->i4_avg_noise_thrshld_4x4 = i4_avg_noise_satd;
    }

    for(vert_ctr = 0; vert_ctr < v_ctb_cnt; vert_ctr++)
    {
        ihevce_ed_ctb_l1_t *ps_ed_ctb_row_l1 =
            ps_ed_ctb_pic_l1 + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;
        ihevce_ed_blk_t *ps_ed = ps_ed_blk_l1 + (vert_ctr * sub_blk_cnt * h_ctb_cnt);

        for(ctb_ctr = 0; ctb_ctr < h_ctb_cnt; ctb_ctr++, ps_ed += sub_blk_cnt)
        {
            ihevce_ed_ctb_l1_t *ps_ed_ctb_curr_l1 = ps_ed_ctb_row_l1 + ctb_ctr;
            WORD8 b8_satd_eval[4];
            WORD32 ai4_satd_4x4[64];
            WORD32 ai4_satd_8x8[16];  // derived from accumulating 4x4 satds
            WORD32 ai4_satd_16x16[4] = { 0 };  // derived from accumulating 8x8 satds
            WORD32 i4_satd_32x32 = 0;  // derived from accumulating 8x8 satds
            /* This 2-D array will contain 4x4 satds sorted in ascending order in sets
             * of 4, 16, 64  For example : '5 10 2 7 6 12 3 1' array input will return
             * '2 5 7 10 1 3 6 12' if sorted in sets of 4 */
            WORD32 aai4_sort_4_16_64_satd[3][64];
            /* This 2-D array will contain 8x8 satds sorted in ascending order in sets of
             * 4, 16***/
            WORD32 aai4_sort_4_16_satd[2][64];

            memset(b8_satd_eval, 1, sizeof(b8_satd_eval));
            for(i = 0; i < 4; i++)
            {
                ihevce_ed_blk_t *ps_ed_b32 = &ps_ed[i * 16];

                for(j = 0; j < 4; j++)
                {
                    ihevce_ed_blk_t *ps_ed_b16 = &ps_ed_b32[j * 4];
                    WORD32 satd_sum = 0;
                    WORD32 blk_cnt = 0;

                    for(k = 0; k < 4; k++)
                    {
                        ihevce_ed_blk_t *ps_ed_b4 = &ps_ed_b16[k];

                        if(-1 != ps_ed_b4->i4_4x4_satd)
                        {
#define SUB_NOISE_THRSHLD 0
#if SUB_NOISE_THRSHLD
                            ps_ed_b4->i4_4x4_satd = ps_ed_b4->i4_4x4_satd - i4_avg_noise_satd;
                            if(ps_ed_b4->i4_4x4_satd < 0)
                            {
                                ps_ed_b4->i4_4x4_satd = 0;
                            }
#else
                            if(ps_ed_b4->i4_4x4_satd < i4_avg_noise_satd)
                            {
                                ps_ed_b4->i4_4x4_satd = i4_avg_noise_satd;
                            }
#endif
                            blk_cnt++;
                            satd_sum += ps_ed_b4->i4_4x4_satd;
                        }
                        ai4_satd_4x4[i * 16 + j * 4 + k] = ps_ed_b4->i4_4x4_satd;
                    }
                    ASSERT(blk_cnt == 0 || blk_cnt == 4);
                    if(blk_cnt == 0)
                    {
                        satd_sum = -1;
                    }
                    ai4_satd_8x8[i * 4 + j] = satd_sum;
                    ai4_satd_16x16[i] += satd_sum;
                    i4_satd_32x32 += satd_sum;
                    ps_ed_ctb_curr_l1->i4_sum_4x4_satd[i * 4 + j] = satd_sum;
                }
            }

            {
                /* This function will sort 64 elements in array ai4_satd_4x4 in ascending order
                 *  to 3 arrays in sets of 4, 16, 64 into the 2-D array aai4_min_4_16_64_satd */
                WORD32 array_length = sizeof(ai4_satd_4x4) / sizeof(WORD32);
                ihevce_merge_sort(
                    &ai4_satd_4x4[0], aai4_sort_4_16_64_satd, array_length, 1, 64);

                /* This function will sort 64 elements in array ai4_satd_8x8 in ascending order
                 *  to 2 arrays in sets of 4, 16 into the 2-D array aai4_sum_4_16_satd_ctb */
                array_length = sizeof(ai4_satd_8x8) / sizeof(WORD32);
                ihevce_merge_sort(
                    &ai4_satd_8x8[0], aai4_sort_4_16_satd, array_length, 1, 16);
            }

            /* Populate avg satd to calculate modulation index and activity factors */
            /* 16x16 */
            for(i = 0; i < 4; i++)
            {
                for(j = 0; j < 4; j++)
                {
                    WORD32 satd_sum = ps_ed_ctb_curr_l1->i4_sum_4x4_satd[i * 4 + j];
                    WORD32 satd_min = aai4_sort_4_16_64_satd[0][i * 16 + j * 4 + MEDIAN_CU_TU];

                    ASSERT(-2 != satd_sum);
                    ps_ed_ctb_curr_l1->i4_min_4x4_satd[i * 4 + j] = satd_min;

                    if(-1 != satd_sum)
                    {
                        ps_ed_ctb_curr_l1->i4_8x8_satd[i * 4 + j][0] = satd_sum;
                        ps_ed_ctb_curr_l1->i4_8x8_satd[i * 4 + j][1] = satd_min;

                        u8_frame_8x8_sum_act_sqr += (satd_sum * satd_sum);
                        ai4_frame_8x8_sum_act[0] += satd_sum;
                        ai8_frame_8x8_sum_act_sqr[0] += (satd_sum * satd_sum);
                        ai4_frame_8x8_sum_blks[0] += 1;
                        ai4_frame_8x8_sum_act[1] += satd_min;
                        ai8_frame_8x8_sum_act_sqr[1] += (satd_min * satd_min);
                        ai4_frame_8x8_sum_blks[1] += 1;
                    }
                    else
                    {
                        ps_ed_ctb_curr_l1->i4_8x8_satd[i * 4 + j][0] = -1;
                        ps_ed_ctb_curr_l1->i4_8x8_satd[i * 4 + j][1] = -1;
                        b8_satd_eval[i] = 0;
                    }
                }

                if(b8_satd_eval[i])
                {
                    ps_ed_ctb_curr_l1->i4_16x16_satd[i][0] = ai4_satd_16x16[i];
                    ps_ed_ctb_curr_l1->i4_16x16_satd[i][1] = aai4_sort_4_16_satd[0][i * 4 + MEDIAN_CU_TU];
                    ps_ed_ctb_curr_l1->i4_16x16_satd[i][2] = aai4_sort_4_16_64_satd[1][i * 16 + MEDIAN_CU_TU_BY_2];

                    for(k = 0; k < 3; k++)
                    {
                        WORD32 satd = ps_ed_ctb_curr_l1->i4_16x16_satd[i][k];

                        ai4_frame_16x16_sum_act[k] += satd;
                        ai8_frame_16x16_sum_act_sqr[k] += (satd * satd);
                        ai4_frame_16x16_sum_blks[k] += 1;
                    }
                }
                else
                {
                    ps_ed_ctb_curr_l1->i4_16x16_satd[i][0] = -1;
                    ps_ed_ctb_curr_l1->i4_16x16_satd[i][1] = -1;
                    ps_ed_ctb_curr_l1->i4_16x16_satd[i][2] = -1;
                }
            }

            /*32x32*/
            if(b8_satd_eval[0] && b8_satd_eval[1] && b8_satd_eval[2] && b8_satd_eval[3])
            {
                WORD32 aai4_sort_4_satd[1][64];
                WORD32 array_length = sizeof(ai4_satd_16x16) / sizeof(WORD32);
                WORD32 satd;

                /* Sort 4 elements in ascending order */
                ihevce_merge_sort(ai4_satd_16x16, aai4_sort_4_satd, array_length, 1, 4);

                ps_ed_ctb_curr_l1->i4_32x32_satd[0][0] = aai4_sort_4_satd[0][MEDIAN_CU_TU];
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][1] = aai4_sort_4_16_satd[1][MEDIAN_CU_TU_BY_2];
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][2] = aai4_sort_4_16_64_satd[2][MEDIAN_CU_TU_BY_4];
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][3] = i4_satd_32x32;

                for(k = 0; k < 3; k++)
                {
                    WORD32 satd = ps_ed_ctb_curr_l1->i4_32x32_satd[0][k];

                    ai4_frame_32x32_sum_act[k] += satd;
                    ai8_frame_32x32_sum_act_sqr[k] += (satd * satd);
                    ai4_frame_32x32_sum_blks[k] += 1;
                }
            }
            else
            {
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][0] = -1;
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][1] = -1;
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][2] = -1;
                ps_ed_ctb_curr_l1->i4_32x32_satd[0][3] = -1;
            }
        }
    }

    for(i = 0; i < 2; i++)
    {
        /*8x8*/
#if USE_SQRT_AVG_OF_SATD_SQR
        ps_curr_out->i8_curr_frame_8x8_sum_act[i] = ai8_frame_8x8_sum_act_sqr[i];
#else
        ps_curr_out->i8_curr_frame_8x8_sum_act[i] = ai4_frame_8x8_sum_act[i];
#endif
        ps_curr_out->i4_curr_frame_8x8_sum_act_for_strength[i] = ai4_frame_8x8_sum_act[i];
        ps_curr_out->i4_curr_frame_8x8_num_blks[i] = ai4_frame_8x8_sum_blks[i];
        ps_curr_out->u8_curr_frame_8x8_sum_act_sqr = u8_frame_8x8_sum_act_sqr;

        /*16x16*/
#if USE_SQRT_AVG_OF_SATD_SQR
        ps_curr_out->i8_curr_frame_16x16_sum_act[i] = ai8_frame_16x16_sum_act_sqr[i];
#else
        ps_curr_out->i8_curr_frame_16x16_sum_act[i] = ai4_frame_16x16_sum_act[i];
#endif
        ps_curr_out->i4_curr_frame_16x16_num_blks[i] = ai4_frame_16x16_sum_blks[i];

        /*32x32*/
#if USE_SQRT_AVG_OF_SATD_SQR
        ps_curr_out->i8_curr_frame_32x32_sum_act[i] = ai8_frame_32x32_sum_act_sqr[i];
#else
        ps_curr_out->i8_curr_frame_32x32_sum_act[i] = ai4_frame_32x32_sum_act[i];
#endif
        ps_curr_out->i4_curr_frame_32x32_num_blks[i] = ai4_frame_32x32_sum_blks[i];
    }

    /*16x16*/
#if USE_SQRT_AVG_OF_SATD_SQR
    ps_curr_out->i8_curr_frame_16x16_sum_act[2] = ai8_frame_16x16_sum_act_sqr[2];
#else
    ps_curr_out->i8_curr_frame_16x16_sum_act[2] = ai4_frame_16x16_sum_act[2];
#endif
    ps_curr_out->i4_curr_frame_16x16_num_blks[2] = ai4_frame_16x16_sum_blks[2];

    /*32x32*/
#if USE_SQRT_AVG_OF_SATD_SQR
    ps_curr_out->i8_curr_frame_32x32_sum_act[2] = ai8_frame_32x32_sum_act_sqr[2];
#else
    ps_curr_out->i8_curr_frame_32x32_sum_act[2] = ai4_frame_32x32_sum_act[2];
#endif
    ps_curr_out->i4_curr_frame_32x32_num_blks[2] = ai4_frame_32x32_sum_blks[2];
}

/*!
************************************************************************
* @brief
*  accumulate L1 intra satd across all threads.
*  Note: call to this function has to be made after all threads have
*  finished preintra processing
*
************************************************************************
*/
LWORD64 ihevce_decomp_pre_intra_get_frame_satd(void *pv_ctxt, WORD32 *wd, WORD32 *ht)
{
    ihevce_decomp_pre_intra_master_ctxt_t *ps_master_ctxt = pv_ctxt;
    ihevce_decomp_pre_intra_ctxt_t *ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[0];
    LWORD64 satd_sum = ps_ctxt->ps_ed_ctxt->i8_sum_best_satd;
    WORD32 i;

    *wd = ps_ctxt->as_layers[1].i4_actual_wd;
    *ht = ps_ctxt->as_layers[1].i4_actual_ht;
    for(i = 1; i < ps_master_ctxt->i4_num_proc_thrds; i++)
    {
        ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[i];
        satd_sum += ps_ctxt->ps_ed_ctxt->i8_sum_best_satd;
    }

    return satd_sum;
}

LWORD64 ihevce_decomp_pre_intra_get_frame_satd_squared(void *pv_ctxt, WORD32 *wd, WORD32 *ht)
{
    ihevce_decomp_pre_intra_master_ctxt_t *ps_master_ctxt = pv_ctxt;
    ihevce_decomp_pre_intra_ctxt_t *ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[0];
    LWORD64 satd_sum = ps_ctxt->ps_ed_ctxt->i8_sum_sq_best_satd;
    WORD32 i;

    *wd = ps_ctxt->as_layers[1].i4_actual_wd;
    *ht = ps_ctxt->as_layers[1].i4_actual_ht;
    for(i = 1; i < ps_master_ctxt->i4_num_proc_thrds; i++)
    {
        ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[i];
        satd_sum += ps_ctxt->ps_ed_ctxt->i8_sum_sq_best_satd;
    }

    return satd_sum;
}
