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
* \file ihevce_recur_bracketing.c
*
* \brief
*    This file contains interface functions of recursive bracketing
*    module
* \date
*    12/02/2012
*
* \author
*    Ittiam
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
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_ipe_structs.h"
#include "ihevce_ipe_pass.h"
#include "ihevce_recur_bracketing.h"
#include "ihevce_nbr_avail.h"
#include "ihevc_common_tables.h"
#include "ihevce_decomp_pre_intra_structs.h"
#include "ihevce_decomp_pre_intra_pass.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IP_DBG_L1_l2 0
#define CHILD_BIAS 12

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
extern pf_intra_pred g_apf_lum_ip[10];

extern WORD32 g_i4_ip_funcs[MAX_NUM_IP_MODES];

UWORD8 gau1_cu_pos_x[64] = { 0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3, 4, 5, 4, 5, 6, 7,
                             6, 7, 4, 5, 4, 5, 6, 7, 6, 7, 0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1,
                             2, 3, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 4, 5, 4, 5, 6, 7, 6, 7 };

UWORD8 gau1_cu_pos_y[64] = { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3, 0, 0, 1, 1, 0, 0,
                             1, 1, 2, 2, 3, 3, 2, 2, 3, 3, 4, 4, 5, 5, 4, 4, 5, 5, 6, 6, 7, 7,
                             6, 6, 7, 7, 4, 4, 5, 5, 4, 4, 5, 5, 6, 6, 7, 7, 6, 6, 7, 7 };

#define RESET_BIT(x, bit) (x = x & ~((WORD32)1 << bit))

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_update_cand_list \endif
*
* \brief
*    Final Candidate list population, nbr flag andd nbr mode update function
*
* \param[in] ps_row_cu : pointer to cu analyse struct
* \param[in] ps_cu_node : pointer to cu node info buffer
* \param[in] ps_ed_blk_l1 : pointer to level 1 and 2 decision buffer
* \param[in] pu1_cand_mode_list  : pointer to candidate list buffer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_update_cand_list(
    ihevce_ipe_cu_tree_t *ps_cu_node, ihevce_ed_blk_t *ps_ed_blk_l1, ihevce_ipe_ctxt_t *ps_ctxt)
{
    WORD32 row, col, x, y, size;

    /* Candidate mode Update */
    (void)ps_ed_blk_l1;
    /* Update CTB mode map for the finalised CU */
    x = ((ps_cu_node->u2_x0 << 3) >> 2) + 1;
    y = ((ps_cu_node->u2_y0 << 3) >> 2) + 1;
    size = ps_cu_node->u1_cu_size >> 2;
    for(row = y; row < (y + size); row++)
    {
        for(col = x; col < (x + size); col++)
        {
            ps_ctxt->au1_ctb_mode_map[row][col] = ps_cu_node->best_mode;
        }
    }
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_intra_populate_mode_bits_cost_bracketing \endif
*
* \brief
*    Mpm indx calc function based on left and top available modes
*
* \param[in] top_intra_mode : Top available intra mode
* \param[in] left_intra_mode : Left available intra mode
* \param[in] available_top : Top availability flag
* \param[in] available_left : Left availability flag
* \param[in] cu_pos_y : cu position wrt to CTB
* \param[in] mode_bits_cost : pointer to mode bits buffer
* \param[in] lambda : Lambda value (SAD/SATD)
* \param[in] cand_mode_list  : pointer to candidate list buffer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_intra_populate_mode_bits_cost_bracketing(
    WORD32 top_intra_mode,
    WORD32 left_intra_mode,
    WORD32 available_top,
    WORD32 available_left,
    WORD32 cu_pos_y,
    UWORD16 *mode_bits_cost,
    UWORD16 *mode_bits,
    WORD32 lambda,
    WORD32 *cand_mode_list)
{
    /* local variables */
    WORD32 i;
    WORD32 cand_intra_pred_mode_left, cand_intra_pred_mode_top;

    UWORD16 one_bits_cost =
        COMPUTE_RATE_COST_CLIP30(4, lambda, (LAMBDA_Q_SHIFT + 1));  //1.5 * lambda
    UWORD16 two_bits_cost =
        COMPUTE_RATE_COST_CLIP30(6, lambda, (LAMBDA_Q_SHIFT + 1));  //2.5 * lambda
    UWORD16 five_bits_cost =
        COMPUTE_RATE_COST_CLIP30(12, lambda, (LAMBDA_Q_SHIFT + 1));  //5.5 * lambda

    for(i = 0; i < 35; i++)
    {
        mode_bits_cost[i] = five_bits_cost;
        mode_bits[i] = 5;
    }

    /* EIID: set availability flag to zero if modes are invalid.
       Required since some CU's might be skipped (though available)
       and their modes will be set to 255 (-1)*/
    if(35 < top_intra_mode || 0 > top_intra_mode)
        available_top = 0;
    if(35 < left_intra_mode || 0 > left_intra_mode)
        available_left = 0;

    /* Calculate cand_intra_pred_mode_N as per sec. 8.4.2 in JCTVC-J1003_d7 */
    /* N = top */
    if(0 == available_top)
    {
        cand_intra_pred_mode_top = INTRA_DC;
    }
    /* for neighbour != INTRA, setting DC is done outside */
    else if(0 == cu_pos_y) /* It's on the CTB boundary */
    {
        cand_intra_pred_mode_top = INTRA_DC;
    }
    else
    {
        cand_intra_pred_mode_top = top_intra_mode;
    }

    /* N = left */
    if(0 == available_left)
    {
        cand_intra_pred_mode_left = INTRA_DC;
        //cand_intra_pred_mode_left = cand_intra_pred_mode_top;
    }
    /* for neighbour != INTRA, setting DC is done outside */
    else
    {
        cand_intra_pred_mode_left = left_intra_mode;
    }

    /* Calculate cand_mode_list as per sec. 8.4.2 in JCTVC-J1003_d7 */
    if(cand_intra_pred_mode_left == cand_intra_pred_mode_top)
    {
        if(cand_intra_pred_mode_left < 2)
        {
            cand_mode_list[0] = INTRA_PLANAR;
            cand_mode_list[1] = INTRA_DC;
            cand_mode_list[2] = INTRA_ANGULAR(26); /* angular 26 = Vertical */
        }
        else
        {
            cand_mode_list[0] = cand_intra_pred_mode_left;
            cand_mode_list[1] = 2 + ((cand_intra_pred_mode_left + 29) % 32);
            cand_mode_list[2] = 2 + ((cand_intra_pred_mode_left - 2 + 1) % 32);
        }
    }
    else
    {
        if(0 == available_left)
        {
            cand_mode_list[0] = cand_intra_pred_mode_top;
            cand_mode_list[1] = cand_intra_pred_mode_left;
        }
        else
        {
            cand_mode_list[0] = cand_intra_pred_mode_left;
            cand_mode_list[1] = cand_intra_pred_mode_top;
        }
        if((cand_intra_pred_mode_left != INTRA_PLANAR) &&
           (cand_intra_pred_mode_top != INTRA_PLANAR))
        {
            cand_mode_list[2] = INTRA_PLANAR;
        }
        else if((cand_intra_pred_mode_left != INTRA_DC) && (cand_intra_pred_mode_top != INTRA_DC))
        {
            cand_mode_list[2] = INTRA_DC;
        }
        else
        {
            cand_mode_list[2] = INTRA_ANGULAR(26);
        }
    }
    mode_bits_cost[cand_mode_list[0]] = one_bits_cost;
    mode_bits_cost[cand_mode_list[1]] = two_bits_cost;
    mode_bits_cost[cand_mode_list[2]] = two_bits_cost;

    mode_bits[cand_mode_list[0]] = 2;
    mode_bits[cand_mode_list[1]] = 3;
    mode_bits[cand_mode_list[2]] = 3;
}

/*!
******************************************************************************
* \if Function name : ihevce_pu_calc_4x4_blk \endif
*
* \brief
*    4x4 pu (8x8 CU) mode decision using step 8421 method
*
* \param[in] ps_cu_node : pointer to cu node info buffer
* \param[in] pu1_src : pointer to src pixels
* \param[in] src_stride : frm source stride
* \param[in] ref : pointer to reference pixels for prediction
* \param[in] cand_mode_list  : pointer to candidate list buffer
* \param[in] best_costs_4x4  : pointer to 3 best cost buffer
* \param[in] best_modes_4x4  : pointer to 3 best mode buffer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_pu_calc_4x4_blk(
    ihevce_ipe_ctxt_t *ps_ctxt,
    ihevce_ipe_cu_tree_t *ps_cu_node,
    UWORD8 *pu1_src,
    WORD32 src_stride,
    UWORD8 *ref,
    UWORD16 *mode_bits_cost,
    WORD32 *best_costs_4x4,
    UWORD8 *best_modes_4x4,
    func_selector_t *ps_func_selector)
{
    WORD16 *pi2_trans_tmp = ps_ctxt->pi2_trans_tmp;
    WORD16 *pi2_trans_out = ps_ctxt->pi2_trans_out;
    UWORD8 u1_use_satd = ps_ctxt->u1_use_satd;
    UWORD8 u1_level_1_refine_on = ps_ctxt->u1_level_1_refine_on;

    WORD32 i, j = 0, i_end;
    UWORD8 mode, best_amode = 255;
    UWORD8 pred[16];

    UWORD16 sad;
    WORD32 sad_cost = 0;
    WORD32 best_asad_cost = 0xFFFFF;
    WORD32 temp;
    UWORD8 modes_to_eval[5];
    WORD32 costs_4x4[5];
    UWORD8 modes_4x4[5] = { 0, 1, 2, 3, 4 };

    /* LO resolution hence low resolution disable */
    WORD32 u1_low_resol = 0;
    UWORD8 au1_best_modes[1] = { 0 };
    WORD32 ai4_best_sad_costs[1] = { 0 };

    WORD16 *pi2_tmp = &pi2_trans_tmp[0];

    ihevce_ipe_optimised_function_list_t *ps_ipe_optimised_function_list =
        &ps_ctxt->s_ipe_optimised_function_list;

    //apf_resd_trns[0] = &ihevc_resi_trans_4x4_ttype1;
    //apf_resd_trns[0] = &ihevc_HAD_4x4_8bit;

    for(i = 0; i < 5; i++)
    {
        costs_4x4[i] = MAX_INTRA_COST_IPE;
    }

    ps_ipe_optimised_function_list->pf_ed_4x4_find_best_modes(
        pu1_src,
        src_stride,
        ref,
        mode_bits_cost,
        au1_best_modes,
        ai4_best_sad_costs,
        u1_low_resol,
        ps_ipe_optimised_function_list->pf_4x4_sad_computer);

    best_amode = au1_best_modes[0];
    best_asad_cost = ai4_best_sad_costs[0];

    ASSERT(best_amode != 255);
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

        sad = ps_ipe_optimised_function_list->pf_4x4_sad_computer(pu1_src, &pred[0], src_stride, 4);

        sad_cost = sad;
        sad_cost += mode_bits_cost[mode];

        if(sad_cost < best_asad_cost)
        {
            best_amode = mode;
            best_asad_cost = sad_cost;
        }
    }

    /* Around best level 2 angular mode, search for best level 1 mode */
    /* Also evaluate for non-angular mode */

    i = 0;
    /*Level 1 refinement is disabled for ES preset */
    if(1 == u1_level_1_refine_on)
    {
        if(best_amode != 2)
            modes_to_eval[i++] = best_amode - 1;
        modes_to_eval[i++] = best_amode;
    }

    modes_to_eval[i++] = 0;
    modes_to_eval[i++] = 1;

    if(1 == u1_level_1_refine_on)
    {
        if(best_amode != 34)
            modes_to_eval[i++] = best_amode + 1;
    }
    i_end = i;
    i = 0;

    for(; i < i_end; i++)
    {
        mode = modes_to_eval[i];

        g_apf_lum_ip[g_i4_ip_funcs[mode]](&ref[0], 0, &pred[0], 4, 4, mode);

        /* Hard coding to use SATD */
        if(u1_use_satd)
        {
            ps_func_selector->ihevc_resi_trans_4x4_ttype1_fptr(
                pu1_src, &pred[0], (WORD32 *)pi2_tmp, pi2_trans_out, src_stride, 4, 4, NULL_PLANE);

            sad = ihevce_ipe_pass_satd(pi2_trans_out, 4, 4);
        }
        else
        {
            sad = ps_ipe_optimised_function_list->pf_4x4_sad_computer(
                pu1_src, &pred[0], src_stride, 4);
        }
        sad_cost = sad;
        sad_cost += mode_bits_cost[mode];

        costs_4x4[i] = sad_cost;
    }

    /* Arrange the reference array in ascending order */
    for(i = 0; i < (i_end - 1); i++)
    {
        for(j = i + 1; j < i_end; j++)
        {
            if(costs_4x4[i] > costs_4x4[j])
            {
                temp = costs_4x4[i];
                costs_4x4[i] = costs_4x4[j];
                costs_4x4[j] = temp;

                temp = modes_4x4[i];
                modes_4x4[i] = modes_4x4[j];
                modes_4x4[j] = temp;
            }
        }
    }
    for(i = 0; i < 3; i++)
    {
        best_costs_4x4[i] = costs_4x4[i];
        best_modes_4x4[i] = modes_to_eval[modes_4x4[i]];
    }

    {
        ps_cu_node->best_mode = best_modes_4x4[0];
        ps_cu_node->best_cost = best_costs_4x4[0];
        ps_cu_node->best_satd = best_costs_4x4[0] - mode_bits_cost[ps_cu_node->best_mode];
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_pu_calc_8x8_blk \endif
*
* \brief
*    4x4 pu (8x8 CU) mode decision loop using step 8421 method
*
* \param[in] ps_curr_src : pointer to src pixels struct
* \param[in] ps_ctxt : pointer to IPE context struct
* \param[in] ps_cu_node : pointer to cu node info buffer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_pu_calc_8x8_blk(
    iv_enc_yuv_buf_t *ps_curr_src,
    ihevce_ipe_ctxt_t *ps_ctxt,
    ihevce_ipe_cu_tree_t *ps_cu_node,
    func_selector_t *ps_func_selector)
{
    WORD32 i, j;
    WORD32 nbr_flags;
    nbr_avail_flags_t s_nbr;
    WORD32 trans_size = ps_cu_node->ps_parent->u1_cu_size >> 1;

    UWORD8 *pu1_src_4x4;
    WORD32 xA, xB, yA, yB;
    //WORD32 x, y, size;
    WORD32 top_intra_mode;
    WORD32 left_intra_mode;
    //    WORD8 *top_intra_mode_ptr;
    //  WORD8 *left_intra_mode_ptr;
    UWORD8 *pu1_orig;
    WORD32 src_strd = ps_curr_src->i4_y_strd;

    WORD32 cu_pos_x = ps_cu_node->ps_parent->u2_x0 << 1;
    WORD32 cu_pos_y = ps_cu_node->ps_parent->u2_y0 << 1;
    ihevc_intra_pred_luma_ref_substitution_ft *ihevc_intra_pred_luma_ref_substitution_fptr;

    ihevc_intra_pred_luma_ref_substitution_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_ref_substitution_fptr;

    pu1_orig = (UWORD8 *)(ps_curr_src->pv_y_buf) +
               ((ps_cu_node->ps_parent->u2_y0 << 3) * src_strd) +
               (ps_cu_node->ps_parent->u2_x0 << 3);
    for(i = 0; i < 2; i++)
    {
        for(j = 0; j < 2; j++)
        {
            WORD32 cand_mode_list[3];
            pu1_src_4x4 = pu1_orig + (i * trans_size * src_strd) + (j * trans_size);
            /* get the neighbour availability flags */
            nbr_flags = ihevce_get_nbr_intra(
                &s_nbr,
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x + ((j) * (trans_size >> 2)),
                cu_pos_y + ((i) * (trans_size >> 2)),
                trans_size >> 2);

            /* call the function which populates sad cost for all the modes */
            xA = ((ps_cu_node->ps_parent->u2_x0 << 3) >> 2) + j;
            yA = ((ps_cu_node->ps_parent->u2_y0 << 3) >> 2) + 1 + i;
            xB = xA + 1;
            yB = yA - 1;
            left_intra_mode = ps_ctxt->au1_ctb_mode_map[yA][xA];
            top_intra_mode = ps_ctxt->au1_ctb_mode_map[yB][xB];

            ihevce_intra_populate_mode_bits_cost_bracketing(
                top_intra_mode,
                left_intra_mode,
                s_nbr.u1_top_avail,
                s_nbr.u1_left_avail,
                ps_cu_node->ps_parent->u2_y0,
                &ps_ctxt->au2_mode_bits_cost_8x8pu[i * 2 + j][0],
                &ps_ctxt->au2_mode_bits_8x8_pu[0],
                ps_ctxt->i4_ol_sad_lambda,
                cand_mode_list);

            /* call the function which populates ref data for intra predicion */
            ihevc_intra_pred_luma_ref_substitution_fptr(
                pu1_src_4x4 - src_strd - 1,
                pu1_src_4x4 - src_strd,
                pu1_src_4x4 - 1,
                src_strd,
                4,
                nbr_flags,
                &ps_ctxt->au1_ref_8x8pu[i * 2 + j][0],
                0);

            ihevce_pu_calc_4x4_blk(
                ps_ctxt,
                ps_cu_node->ps_sub_cu[(i * 2) + j],
                pu1_src_4x4,
                src_strd,
                &ps_ctxt->au1_ref_8x8pu[i * 2 + j][0],
                &ps_ctxt->au2_mode_bits_cost_8x8pu[i * 2 + j][0],
                &ps_cu_node->ps_sub_cu[(i * 2) + j]->au4_best_cost_1tu[0],
                &ps_cu_node->ps_sub_cu[(i * 2) + j]->au1_best_mode_1tu[0],
                ps_func_selector);

            /*&au4_cost_4x4[i*2 + j][0],
                &au1_modes_4x4[i*2 + j][0]);*/ //TTODO : mode will change for the four partition

            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x + ((j) * (trans_size >> 2)),
                cu_pos_y + ((i) * (trans_size >> 2)),
                (trans_size >> 2),
                1);

            xA = ((ps_cu_node->ps_parent->u2_x0 << 3) >> 2) + 1 + j;
            yA = ((ps_cu_node->ps_parent->u2_y0 << 3) >> 2) + 1 + i;
            ps_ctxt->au1_ctb_mode_map[yA][xA] = ps_cu_node->ps_sub_cu[i * 2 + j]->best_mode;
            ps_cu_node->ps_sub_cu[i * 2 + j]->u2_mode_bits_cost =
                ps_ctxt->au2_mode_bits_8x8_pu[ps_cu_node->ps_sub_cu[i * 2 + j]->best_mode];
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_bracketing_analysis \endif
*
* \brief
*    Interface function that evaluates MAX cu and MAX - 1 cu, with MAX cu size
*    info decided coarse resolution mode decision. Compares the SATD/SAD cost btwn
*    2 CUS and determines the actual CU size and best 3 modes to be given to rdopt
*
* \param[in] ps_ctxt : pointer to IPE context struct
* \param[in] ps_cu_node : pointer to cu node info buffer
* \param[in] ps_curr_src : pointer to src pixels struct
* \param[in] ps_ctb_out : pointer to ip ctb out struct
* \param[in] ps_row_cu : pointer to cu analyse struct
* \param[in] ps_ed_l1_ctb : pointer to level 1 early deci struct
* \param[in] ps_ed_l2_ctb : pointer to level 2 early deci struct
* \param[in] ps_l0_ipe_out_ctb : pointer to ipe_l0_ctb_analyse_for_me_t struct
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_bracketing_analysis(
    ihevce_ipe_ctxt_t *ps_ctxt,
    ihevce_ipe_cu_tree_t *ps_cu_node,
    iv_enc_yuv_buf_t *ps_curr_src,
    ctb_analyse_t *ps_ctb_out,
    //cu_analyse_t         *ps_row_cu,
    ihevce_ed_blk_t *ps_ed_l1_ctb,
    ihevce_ed_blk_t *ps_ed_l2_ctb,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    ipe_l0_ctb_analyse_for_me_t *ps_l0_ipe_out_ctb)
{
    WORD32 cu_pos_x = 0;
    WORD32 cu_pos_y = 0;

    UWORD8 u1_curr_ctb_wdt = ps_cu_node->u1_width;
    UWORD8 u1_curr_ctb_hgt = ps_cu_node->u1_height;
    WORD32 num_8x8_blks_x = (u1_curr_ctb_wdt >> 3);
    WORD32 num_8x8_blks_y = (u1_curr_ctb_hgt >> 3);

    ihevce_ed_blk_t *ps_ed_blk_l1 = ps_ed_l1_ctb;
    ihevce_ed_blk_t *ps_ed_blk_l2 = ps_ed_l2_ctb;

    WORD32 i;
    WORD32 cand_mode_list[3];
    //cu_analyse_t *ps_curr_cu = ps_row_cu;
    WORD32 blk_cnt = 0;
    WORD32 j = 0;
    WORD32 merge_32x32_l1, merge_32x32_l2;

    WORD32 i4_skip_intra_eval_32x32_l1;
    //EIID: flag indicating number of 16x16 blocks to be skipped for intra evaluation within 32x32 block

    WORD32 parent_cost = 0;
    WORD32 child_cost[4] = { 0 };
    WORD32 child_cost_least = 0;
    WORD32 child_satd[4] = { 0 };
    WORD32 x, y, size;
    WORD32 merge_64x64 = 1;
    UWORD8 au1_best_32x32_modes[4];
    WORD32 au4_best_32x32_cost[4];
    WORD32 parent_best_mode;
    UWORD8 best_mode;

    WORD32 i4_quality_preset = ps_ctxt->i4_quality_preset;
    /* flag to control 1CU-4TU modes based on quality preset                */
    /* if set 1CU-4TU are explicity evaluated else 1CU-1TU modes are copied */
    WORD32 i4_enable_1cu_4tu = (i4_quality_preset == IHEVCE_QUALITY_P2) ||
                               (i4_quality_preset == IHEVCE_QUALITY_P0);

    /* flag to control 4CU-16TU mode based on quality preset                */
    /* if set 4CU-16TU are explicity evaluated else 4CU-4TU modes are copied*/
    WORD32 i4_enable_4cu_16tu = (i4_quality_preset == IHEVCE_QUALITY_P2) ||
                                (i4_quality_preset == IHEVCE_QUALITY_P0);

    WORD32 i4_mod_factor_num, i4_mod_factor_den = QP_MOD_FACTOR_DEN;  //2;
    float f_strength;
    /* Accumalte satd */
    LWORD64 i8_frame_acc_satd_cost = 0, i8_frame_acc_satd_by_modqp_q10 = 0;
    WORD32 i4_ctb_acc_satd = 0;

    /* Accumalate Mode bits cost */
    LWORD64 i8_frame_acc_mode_bits_cost = 0;

    /* Step2 is bypassed for parent, uses children modes*/
    WORD32 step2_bypass = 1;

    if(1 == ps_ctxt->u1_disable_child_cu_decide)
        step2_bypass = 0;

    ps_cu_node->ps_parent = ps_ctxt->ps_ipe_cu_tree;
    for(i = 0; i < 4; i++)
    {
        ps_cu_node->ps_sub_cu[i] = ps_ctxt->ps_ipe_cu_tree + 1 + i;
    }

    /* Loop for all 8x8 block in a CTB */
    ps_ctb_out->u4_cu_split_flags = 0x1;

    /* Initialize intra 64x64, 32x32 and 16x16 costs to max value */
    for(i = 0; i < (MAX_CU_IN_CTB >> 4); i++)
    {
        ps_l0_ipe_out_ctb->ai4_best32x32_intra_cost[i] = MAX_INTRA_COST_IPE;
    }

    for(i = 0; i < (MAX_CU_IN_CTB >> 2); i++)
    {
        ps_l0_ipe_out_ctb->ai4_best16x16_intra_cost[i] = MAX_INTRA_COST_IPE;
    }

    for(i = 0; i < (MAX_CU_IN_CTB); i++)
    {
        ps_l0_ipe_out_ctb->ai4_best8x8_intra_cost[i] = MAX_INTRA_COST_IPE;
    }

    ps_l0_ipe_out_ctb->i4_best64x64_intra_cost = MAX_INTRA_COST_IPE;

    /* by default 64x64 modes are set to default values DC and Planar */
    ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[0] = 0;
    ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[1] = 1;
    ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[2] = 255;

    /* by default 64x4 split is set to 1 */
    ps_l0_ipe_out_ctb->u1_split_flag = 1;

    /* Modulation factor calculated based on spatial variance instead of hardcoded val*/
    i4_mod_factor_num = ps_ctxt->ai4_mod_factor_derived_by_variance[1];  //16;

    f_strength = ps_ctxt->f_strength;

    /* ------------------------------------------------ */
    /* populate the early decisions done by L1 analysis */
    /* ------------------------------------------------ */
    for(i = 0; i < (MAX_CU_IN_CTB >> 2); i++)
    {
        ps_l0_ipe_out_ctb->ai4_best_sad_8x8_l1_ipe[i] = ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i];
        ps_l0_ipe_out_ctb->ai4_best_sad_cost_8x8_l1_ipe[i] = ps_ed_ctb_l1->i4_best_sad_cost_8x8_l1_ipe[i];
        ps_l0_ipe_out_ctb->ai4_best_sad_8x8_l1_me[i] = ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i];
        ps_l0_ipe_out_ctb->ai4_best_sad_cost_8x8_l1_me[i] = ps_ed_ctb_l1->i4_best_sad_cost_8x8_l1_me[i];
    }

    /* Init CTB level accumalated SATD and MPM bits */
    ps_l0_ipe_out_ctb->i4_ctb_acc_satd = 0;
    ps_l0_ipe_out_ctb->i4_ctb_acc_mpm_bits = 0;

    /* ------------------------------------------------ */
    /* Loop over all the blocks in current CTB          */
    /* ------------------------------------------------ */
    {
        /* 64 8x8 blocks should be encountered for the do,while loop to exit */
        do
        {
            intra32_analyse_t *ps_intra32_analyse;
            intra16_analyse_t *ps_intra16_analyse;
            WORD32 *pi4_intra_32_cost;
            WORD32 *pi4_intra_16_cost;
            WORD32 *pi4_intra_8_cost;
            WORD32 merge_16x16_l1;

            /* Given the blk_cnt, get the CU's top-left 8x8 block's x and y positions within the CTB */
            cu_pos_x = gau1_cu_pos_x[blk_cnt];
            cu_pos_y = gau1_cu_pos_y[blk_cnt];

            /* default value for 32x32 best mode - blk_cnt increases by 16 for each 32x32 */
            au1_best_32x32_modes[blk_cnt >> 4] = 255;

            /* get the corresponding intra 32 analyse pointer  use (blk_cnt / 16) */
            /* blk cnt is in terms of 8x8 units so a 32x32 will have 16 8x8 units */
            ps_intra32_analyse = &ps_l0_ipe_out_ctb->as_intra32_analyse[blk_cnt >> 4];

            /* get the corresponding intra 16 analyse pointer use (blk_cnt & 0xF / 4)*/
            /* blk cnt is in terms of 8x8 units so a 16x16 will have 4 8x8 units */
            ps_intra16_analyse = &ps_intra32_analyse->as_intra16_analyse[(blk_cnt & 0xF) >> 2];

            /* Line below assumes min_cu_size of 8 - checks whether CU starts are within picture */
            if((cu_pos_x < num_8x8_blks_x) && (cu_pos_y < num_8x8_blks_y))
            {
                /* Reset to zero for every cu decision */
                merge_32x32_l1 = 0;

                child_cost_least = 0;

                /* At L2, each 4x4 corresponds to 16x16 at L0. Every 4 16x16 stores a merge_success flag */
                ps_ed_blk_l2 = ps_ed_l2_ctb + (blk_cnt >> 2);

                pi4_intra_32_cost = &ps_l0_ipe_out_ctb->ai4_best32x32_intra_cost[blk_cnt >> 4];

                /* by default 32x32 modes are set to default values DC and Planar */
                ps_intra32_analyse->au1_best_modes_32x32_tu[0] = 0;
                ps_intra32_analyse->au1_best_modes_32x32_tu[1] = 1;
                ps_intra32_analyse->au1_best_modes_32x32_tu[2] = 255;

                /* By default 32x32 split is set to 1 */
                ps_intra32_analyse->b1_split_flag = 1;

                ps_intra32_analyse->au1_best_modes_16x16_tu[0] = 0;
                ps_intra32_analyse->au1_best_modes_16x16_tu[1] = 1;
                ps_intra32_analyse->au1_best_modes_16x16_tu[2] = 255;

                /* 16x16 cost & 8x8 cost are stored in Raster scan order */
                /* stride of 16x16 buffer is MAX_CU_IN_CTB_ROW >> 1      */
                /* stride of 8x8 buffer is MAX_CU_IN_CTB_ROW             */
                {
                    WORD32 pos_x_8x8, pos_y_8x8;

                    pos_x_8x8 = gau1_cu_pos_x[blk_cnt];
                    pos_y_8x8 = gau1_cu_pos_y[blk_cnt];

                    pi4_intra_16_cost = &ps_l0_ipe_out_ctb->ai4_best16x16_intra_cost[0];

                    pi4_intra_16_cost +=
                        ((pos_x_8x8 >> 1) + ((pos_y_8x8 >> 1) * (MAX_CU_IN_CTB_ROW >> 1)));

                    pi4_intra_8_cost = &ps_l0_ipe_out_ctb->ai4_best8x8_intra_cost[0];

                    pi4_intra_8_cost += (pos_x_8x8 + (pos_y_8x8 * MAX_CU_IN_CTB_ROW));
                }

                merge_32x32_l1 = 0;
                merge_32x32_l2 = 0;
                i4_skip_intra_eval_32x32_l1 = 0;

                /* Enable 16x16 merge iff sufficient 8x8 blocks remain in the current CTB */
                merge_16x16_l1 = 0;
                if(((num_8x8_blks_x - cu_pos_x) >= 2) && ((num_8x8_blks_y - cu_pos_y) >= 2))
                {
#if !ENABLE_UNIFORM_CU_SIZE_8x8
                    merge_16x16_l1 = ps_ed_blk_l1->merge_success;
#else
                    merge_16x16_l1 = 0;
#endif
                }

                /* Enable 32x32 merge iff sufficient 8x8 blocks remain in the current CTB */
                if(((num_8x8_blks_x - cu_pos_x) >= 4) && ((num_8x8_blks_y - cu_pos_y) >= 4))
                {
                    /* Check 4 flags of L1(8x8) say merge */
                    for(i = 0; i < 4; i++)
                    {
                        merge_32x32_l1 += (ps_ed_blk_l1 + (i * 4))->merge_success;

                        //EIDD: num 16x16 blocks for which inter_intra flag says eval only inter, i.e. skip intra eval
                        i4_skip_intra_eval_32x32_l1 +=
                            ((ps_ed_blk_l1 + (i * 4))->intra_or_inter == 2) ? 1 : 0;
                    }

#if !ENABLE_UNIFORM_CU_SIZE_8x8
                    /* Check 1 flag from L2(16x16) say merge */
                    merge_32x32_l2 = ps_ed_blk_l2->merge_success;
#else
                    merge_32x32_l1 = 0;
                    merge_32x32_l2 = 0;
#endif
                }

#if DISABLE_L2_IPE_IN_PB_L1_IN_B
                if((i4_quality_preset == IHEVCE_QUALITY_P6) && (ps_ctxt->i4_slice_type != ISLICE))
                {
                    merge_32x32_l2 = 0;
                    ps_ed_blk_l2->merge_success = 0;
                }
#endif

                ps_intra32_analyse->b1_valid_cu = 1;

                /* If Merge success from all 4 L1 and L2, max CU size 32x32 is chosen */
                /* EIID: if all blocks to be skipped then skip entire 32x32 for intra eval,
                if no blocks to be skipped then eval entire 32x32,
                else break the merge and go to 16x16 level eval */
                if((merge_32x32_l1 == 4) && merge_32x32_l2 &&
                   ((i4_skip_intra_eval_32x32_l1 == 0) ||
                    (i4_skip_intra_eval_32x32_l1 == 4))  //comment this line to disable break-merge
                )
                {
#if IP_DBG_L1_l2
                    /* Populate params for 32x32 block analysis */
                    ps_cu_node->ps_parent->best_cost = MAX_INTRA_COST_IPE;

                    ps_cu_node->ps_parent->u1_cu_size = 32;
                    ps_cu_node->ps_parent->u2_x0 = gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                    ps_cu_node->ps_parent->u2_y0 = gau1_cu_pos_y[blk_cnt]; /* Populate properly */
                    ps_cu_node->ps_parent->best_mode = ps_ed_blk_l2->best_merge_mode;
                    /* CU size 32x32 and fill the final cu params */

                    ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                    /* Increment pointers */
                    ps_ed_blk_l1 += 16;
                    blk_cnt += 16;
                    ps_row_cu++;
                    merge_64x64 &= 1;
#else

                    /* EIID: dont evaluate if all 4 blocks at L1 said inter is winning*/
                    if(4 == i4_skip_intra_eval_32x32_l1 && (ps_ctxt->i4_slice_type != ISLICE))
                    {
                        WORD32 i4_local_ctr1, i4_local_ctr2;

                        ps_cu_node->ps_parent->best_cost = MAX_INTRA_COST_IPE;

                        ps_cu_node->ps_parent->u1_cu_size = 32;
                        ps_cu_node->ps_parent->u2_x0 =
                            gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->u2_y0 =
                            gau1_cu_pos_y[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->best_mode =
                            INTRA_DC;  //ps_ed_blk_l2->best_merge_mode;
                        /* CU size 32x32 and fill the final cu params */

                        /* fill in the first modes as invalid */
                        ps_cu_node->ps_parent->au1_best_mode_1tu[0] = INTRA_DC;
                        ps_cu_node->ps_parent->au1_best_mode_1tu[1] =
                            INTRA_DC;  //for safery. Since update_cand_list will set num_modes as 3
                        ps_cu_node->ps_parent->au1_best_mode_1tu[2] = INTRA_DC;

                        ps_cu_node->ps_parent->au1_best_mode_4tu[0] = INTRA_DC;
                        ps_cu_node->ps_parent->au1_best_mode_4tu[1] = INTRA_DC;
                        ps_cu_node->ps_parent->au1_best_mode_4tu[2] = INTRA_DC;

                        ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                        //ps_row_cu->s_cu_intra_cand.b6_num_intra_cands = 0;
                        //ps_row_cu->u1_num_intra_rdopt_cands = 0;

                        ps_intra32_analyse->b1_valid_cu = 0;
                        ps_intra32_analyse->b1_split_flag = 0;
                        ps_intra32_analyse->b1_merge_flag = 0;
                        /*memset (&ps_intra32_analyse->au1_best_modes_32x32_tu,
                        255,
                        NUM_BEST_MODES);
                        memset (&ps_intra32_analyse->au1_best_modes_16x16_tu,
                        255,
                        NUM_BEST_MODES);*/
                        //set only first mode since if it's 255. it wont go ahead
                        ps_intra32_analyse->au1_best_modes_32x32_tu[0] = 255;
                        ps_intra32_analyse->au1_best_modes_16x16_tu[0] = 255;

                        *pi4_intra_32_cost = MAX_INTRA_COST_IPE;

                        /*since ME will start evaluating from bottom up, set the lower
                        cu size data invalid */
                        for(i4_local_ctr1 = 0; i4_local_ctr1 < 4; i4_local_ctr1++)
                        {
                            WORD32 *pi4_intra_8_cost_curr16;

                            ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                .au1_best_modes_16x16_tu[0] = 255;
                            ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                .au1_best_modes_8x8_tu[0] = 255;
                            ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1].b1_merge_flag = 0;
                            ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1].b1_valid_cu = 0;
                            ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1].b1_split_flag = 0;

                            pi4_intra_16_cost
                                [(i4_local_ctr1 & 1) + ((MAX_CU_IN_CTB_ROW >> 1) *
                                                        (i4_local_ctr1 >> 1))] = MAX_INTRA_COST_IPE;

                            pi4_intra_8_cost_curr16 = pi4_intra_8_cost + ((i4_local_ctr1 & 1) << 1);
                            pi4_intra_8_cost_curr16 +=
                                ((i4_local_ctr1 >> 1) << 1) * MAX_CU_IN_CTB_ROW;

                            for(i4_local_ctr2 = 0; i4_local_ctr2 < 4; i4_local_ctr2++)
                            {
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .au1_4x4_best_modes[0][0] = 255;
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .au1_4x4_best_modes[1][0] = 255;
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .au1_4x4_best_modes[2][0] = 255;
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .au1_4x4_best_modes[3][0] = 255;
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .au1_best_modes_8x8_tu[0] = 255;
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .au1_best_modes_4x4_tu[0] = 255;
                                ps_intra32_analyse->as_intra16_analyse[i4_local_ctr1]
                                    .as_intra8_analyse[i4_local_ctr2]
                                    .b1_valid_cu = 0;

                                pi4_intra_8_cost_curr16
                                    [(i4_local_ctr2 & 1) +
                                     (MAX_CU_IN_CTB_ROW * (i4_local_ctr2 >> 1))] =
                                        MAX_INTRA_COST_IPE;
                            }
                        }

                        /* set neighbours even if intra is not evaluated, since source is always available. */
                        ihevce_set_nbr_map(
                            ps_ctxt->pu1_ctb_nbr_map,
                            ps_ctxt->i4_nbr_map_strd,
                            ps_cu_node->ps_parent->u2_x0 << 1,
                            ps_cu_node->ps_parent->u2_y0 << 1,
                            (ps_cu_node->ps_parent->u1_cu_size >> 2),
                            1);

                        /* cost accumalation of best cu size candiate */
                        /*i8_frame_acc_satd_cost += parent_cost;*/

                        /* Mode bits cost accumalation for best cu size and cu mode */
                        /*i8_frame_acc_mode_bits_cost += ps_cu_node->ps_parent->u2_mode_bits_cost;*/

                        /*satd/mod_qp accumulation of best cu */
                        /*i8_frame_acc_satd_by_modqp_q10 += ((LWORD64)ps_cu_node->ps_parent->best_satd << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3))/i4_q_scale_q3_mod;*/

                        /* Increment pointers */
                        ps_ed_blk_l1 += 16;
                        blk_cnt += 16;
                        //ps_row_cu++;
                        merge_64x64 = 0;

                        /* increment for stat purpose only. Increment is valid only on single thread */
                        ps_ctxt->u4_num_16x16_skips_at_L0_IPE += 4;
                    }
                    else
                    {
                        /* Revaluation of 4 16x16 blocks at 8x8 prediction level */
                        //memcpy(ps_ctxt->ai1_ctb_mode_map_temp, ps_ctxt->ai1_ctb_mode_map, sizeof(ps_ctxt->ai1_ctb_mode_map));

                        if((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6) &&
                           (ps_ctxt->i4_slice_type == PSLICE))
                        {
                            ps_ctxt->u1_disable_child_cu_decide = 1;
                            step2_bypass = 0;
                        }

                        /* Based on the flag, Child modes decision can be disabled*/
                        if(0 == ps_ctxt->u1_disable_child_cu_decide)
                        {
                            for(j = 0; j < 4; j++)
                            {
                                ps_cu_node->ps_sub_cu[j]->u2_x0 =
                                    gau1_cu_pos_x[blk_cnt + (j * 4)]; /* Populate properly */
                                ps_cu_node->ps_sub_cu[j]->u2_y0 =
                                    gau1_cu_pos_y[blk_cnt + (j * 4)]; /* Populate properly */
                                ps_cu_node->ps_sub_cu[j]->u1_cu_size = 16;

                                {
                                    WORD32 best_ang_mode =
                                        (ps_ed_blk_l1 + (j * 4))->best_merge_mode;

                                    if(best_ang_mode < 2)
                                        best_ang_mode = 26;

                                    ihevce_mode_eval_filtering(
                                        ps_cu_node->ps_sub_cu[j],
                                        ps_cu_node,
                                        ps_ctxt,
                                        ps_curr_src,
                                        best_ang_mode,
                                        &ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0],
                                        &ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0],
                                        !step2_bypass,
                                        1);

                                    if(i4_enable_4cu_16tu)
                                    {
                                        ihevce_mode_eval_filtering(
                                            ps_cu_node->ps_sub_cu[j],
                                            ps_cu_node,
                                            ps_ctxt,
                                            ps_curr_src,
                                            best_ang_mode,
                                            &ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0],
                                            &ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0],
                                            !step2_bypass,
                                            0);
                                    }
                                    else
                                    {
                                        /* 4TU not evaluated :  4tu modes set same as 1tu modes */
                                        memcpy(
                                            &ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0],
                                            &ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0],
                                            NUM_BEST_MODES);

                                        /* 4TU not evaluated : currently 4tu cost set same as 1tu cost */
                                        memcpy(
                                            &ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0],
                                            &ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0],
                                            NUM_BEST_MODES * sizeof(WORD32));
                                    }

                                    child_cost[j] =
                                        MIN(ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0],
                                            ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0]);

                                    /* Child cost is sum of costs at 16x16 level  */
                                    child_cost_least += child_cost[j];

                                    /* Select the best mode to be populated as top and left nbr depending on the
                                    4tu and 1tu cost */
                                    if(ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0] >
                                       ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0])
                                    {
                                        ps_cu_node->ps_sub_cu[j]->best_mode =
                                            ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0];
                                    }
                                    else
                                    {
                                        ps_cu_node->ps_sub_cu[j]->best_mode =
                                            ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0];
                                    }

                                    { /* Update the CTB nodes only for MAX - 1 CU nodes */
                                        WORD32 xA, yA, row, col;
                                        xA = ((ps_cu_node->ps_sub_cu[j]->u2_x0 << 3) >> 2) + 1;
                                        yA = ((ps_cu_node->ps_sub_cu[j]->u2_y0 << 3) >> 2) + 1;
                                        size = ps_cu_node->ps_sub_cu[j]->u1_cu_size >> 2;
                                        for(row = yA; row < (yA + size); row++)
                                        {
                                            for(col = xA; col < (xA + size); col++)
                                            {
                                                ps_ctxt->au1_ctb_mode_map[row][col] =
                                                    ps_cu_node->ps_sub_cu[j]->best_mode;
                                            }
                                        }
                                    }
                                }

                                /*Child SATD cost*/
                                child_satd[j] = ps_cu_node->ps_sub_cu[j]->best_satd;

                                /* store the child 16x16 costs */
                                pi4_intra_16_cost[(j & 1) + ((MAX_CU_IN_CTB_ROW >> 1) * (j >> 1))] =
                                    child_cost[j];

                                /* set the CU valid flag */
                                ps_intra16_analyse[j].b1_valid_cu = 1;

                                /* All 16x16 merge is valid, if Cu 32x32 is chosen */
                                /* To be reset, if CU 64x64 is chosen */
                                ps_intra16_analyse[j].b1_merge_flag = 1;

                                /* storing the modes to intra 16 analyse */
                                /* store the best 16x16 modes 8x8 tu */
                                memcpy(
                                    &ps_intra16_analyse[j].au1_best_modes_8x8_tu[0],
                                    &ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0],
                                    sizeof(UWORD8) * (NUM_BEST_MODES));
                                ps_intra16_analyse[j].au1_best_modes_8x8_tu[NUM_BEST_MODES] = 255;

                                /* store the best 16x16 modes 16x16 tu */
                                memcpy(
                                    &ps_intra16_analyse[j].au1_best_modes_16x16_tu[0],
                                    &ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0],
                                    sizeof(UWORD8) * (NUM_BEST_MODES));
                                ps_intra16_analyse[j].au1_best_modes_16x16_tu[NUM_BEST_MODES] = 255;

                                /* divide the 16x16 costs (pro rating) to 4 8x8 costs */
                                /* store the same 16x16 modes as 4 8x8 child modes    */
                                {
                                    WORD32 idx_8x8;
                                    WORD32 *pi4_intra_8_cost_curr16;
                                    intra8_analyse_t *ps_intra8_analyse;

                                    pi4_intra_8_cost_curr16 = pi4_intra_8_cost + ((j & 1) << 1);
                                    pi4_intra_8_cost_curr16 += ((j >> 1) << 1) * MAX_CU_IN_CTB_ROW;

                                    for(idx_8x8 = 0; idx_8x8 < 4; idx_8x8++)
                                    {
                                        pi4_intra_8_cost_curr16
                                            [(idx_8x8 & 1) + (MAX_CU_IN_CTB_ROW * (idx_8x8 >> 1))] =
                                                (child_cost[j] + 3) >> 2;

                                        ps_intra8_analyse =
                                            &ps_intra16_analyse[j].as_intra8_analyse[idx_8x8];

                                        ps_intra8_analyse->b1_enable_nxn = 0;
                                        ps_intra8_analyse->b1_valid_cu = 1;

                                        /* store the best 8x8 modes 8x8 tu */
                                        memcpy(
                                            &ps_intra8_analyse->au1_best_modes_8x8_tu[0],
                                            &ps_intra16_analyse[j].au1_best_modes_8x8_tu[0],
                                            sizeof(UWORD8) * (NUM_BEST_MODES + 1));

                                        /* store the best 8x8 modes 4x4 tu */
                                        memcpy(
                                            &ps_intra8_analyse->au1_best_modes_4x4_tu[0],
                                            &ps_intra16_analyse[j].au1_best_modes_8x8_tu[0],
                                            sizeof(UWORD8) * (NUM_BEST_MODES + 1));

                                        /* NXN modes not evaluated hence set to 0 */
                                        memset(
                                            &ps_intra8_analyse->au1_4x4_best_modes[0][0],
                                            255,
                                            sizeof(UWORD8) * 4 * (NUM_BEST_MODES + 1));
                                    }
                                }
                            }

                            ihevce_set_nbr_map(
                                ps_ctxt->pu1_ctb_nbr_map,
                                ps_ctxt->i4_nbr_map_strd,
                                ps_cu_node->ps_sub_cu[0]->u2_x0 << 1,
                                ps_cu_node->ps_sub_cu[0]->u2_y0 << 1,
                                (ps_cu_node->ps_sub_cu[0]->u1_cu_size >> 1),
                                0);
                        }
#if 1  //DISBLE_CHILD_CU_EVAL_L0_IPE //1
                        else
                        {
                            for(j = 0; j < 4; j++)
                            {
                                WORD32 idx_8x8;
                                intra8_analyse_t *ps_intra8_analyse;
                                ps_intra16_analyse[j].au1_best_modes_8x8_tu[0] = 255;
                                ps_intra16_analyse[j].au1_best_modes_16x16_tu[0] = 255;

                                ps_intra16_analyse[j].b1_valid_cu = 0;

                                for(idx_8x8 = 0; idx_8x8 < 4; idx_8x8++)
                                {
                                    ps_intra8_analyse =
                                        &ps_intra16_analyse[j].as_intra8_analyse[idx_8x8];

                                    ps_intra8_analyse->au1_best_modes_8x8_tu[0] = 255;
                                    ps_intra8_analyse->au1_best_modes_4x4_tu[0] = 255;

                                    ps_intra8_analyse->b1_enable_nxn = 0;
                                    ps_intra8_analyse->b1_valid_cu = 0;

                                    /* NXN modes not evaluated hence set to 0 */
                                    memset(
                                        &ps_intra8_analyse->au1_4x4_best_modes[0][0],
                                        255,
                                        sizeof(UWORD8) * 4 * (NUM_BEST_MODES + 1));
                                }
                            }

                            child_cost_least = MAX_INTRA_COST_IPE;
                        }
#endif

                        /* Populate params for 32x32 block analysis */

                        ps_cu_node->ps_parent->u1_cu_size = 32;
                        ps_cu_node->ps_parent->u2_x0 =
                            gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->u2_y0 =
                            gau1_cu_pos_y[blk_cnt]; /* Populate properly */

                        /* Revaluation for 32x32 parent block at 16x16 prediction level */
                        //memcpy(ps_ctxt->ai1_ctb_mode_map_temp, ps_ctxt->ai1_ctb_mode_map, sizeof(ps_ctxt->ai1_ctb_mode_map));

                        {
                            /* Eval for TUSize = CuSize */
                            ihevce_mode_eval_filtering(
                                ps_cu_node->ps_parent,
                                ps_cu_node,
                                ps_ctxt,
                                ps_curr_src,
                                26,
                                &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                step2_bypass,
                                1);

                            if(i4_enable_1cu_4tu)
                            {
                                /* Eval for TUSize = CuSize/2 */
                                ihevce_mode_eval_filtering(
                                    ps_cu_node->ps_parent,
                                    ps_cu_node,
                                    ps_ctxt,
                                    ps_curr_src,
                                    26,
                                    &ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                    &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                    step2_bypass,
                                    0);
                            }
                            else
                            {
                                /* 4TU not evaluated :  4tu modes set same as 1tu modes */
                                memcpy(
                                    &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                    &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                    NUM_BEST_MODES);

                                /* 4TU not evaluated : currently 4tu cost set same as 1tu cost */
                                memcpy(
                                    &ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                    &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                                    NUM_BEST_MODES * sizeof(WORD32));
                            }
                        }

                        ps_ctxt->u1_disable_child_cu_decide = 0;
                        step2_bypass = 1;

                        /* Update parent cost */
                        parent_cost =
                            MIN(ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                ps_cu_node->ps_parent->au4_best_cost_1tu[0]);

                        /* Select the best mode to be populated as top and left nbr depending on the
                        4tu and 1tu cost */
                        if(ps_cu_node->ps_parent->au4_best_cost_4tu[0] >
                           ps_cu_node->ps_parent->au4_best_cost_1tu[0])
                        {
                            ps_cu_node->ps_parent->best_mode =
                                ps_cu_node->ps_parent->au1_best_mode_1tu[0];
                        }
                        else
                        {
                            ps_cu_node->ps_parent->best_mode =
                                ps_cu_node->ps_parent->au1_best_mode_4tu[0];
                        }

                        /* store the 32x32 cost */
                        *pi4_intra_32_cost = parent_cost;

                        /* set the CU valid flag */
                        ps_intra32_analyse->b1_valid_cu = 1;

                        ps_intra32_analyse->b1_merge_flag = 1;

                        /* storing the modes to intra 32 analyse */
                        {
                            /* store the best 32x32 modes 16x16 tu */
                            memcpy(
                                &ps_intra32_analyse->au1_best_modes_16x16_tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                sizeof(UWORD8) * (NUM_BEST_MODES));
                            ps_intra32_analyse->au1_best_modes_16x16_tu[NUM_BEST_MODES] = 255;

                            /* store the best 32x32 modes 32x32 tu */
                            memcpy(
                                &ps_intra32_analyse->au1_best_modes_32x32_tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                sizeof(UWORD8) * (NUM_BEST_MODES));
                            ps_intra32_analyse->au1_best_modes_32x32_tu[NUM_BEST_MODES] = 255;
                        }
                        parent_best_mode = ps_cu_node->ps_parent->best_mode;
                        if((parent_cost <=
                            child_cost_least + (ps_ctxt->i4_ol_satd_lambda * CHILD_BIAS >>
                                                LAMBDA_Q_SHIFT)))  //|| identical_modes)
                        {
                            WORD32 i4_q_scale_q3_mod;
                            UWORD8 u1_cu_possible_qp;
                            WORD32 i4_act_factor;

                            /* CU size 32x32 and fill the final cu params */

                            ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                            if((IHEVCE_QUALITY_P3 > i4_quality_preset))
                            {
                                for(i = 0; i < 4; i++)
                                {
                                    intra8_analyse_t *ps_intra8_analyse;
                                    ps_intra8_analyse = &ps_intra16_analyse->as_intra8_analyse[i];
                                    for(j = 0; j < 4; j++)
                                    {
                                        /* Populate best 3 nxn modes */
                                        ps_intra8_analyse->au1_4x4_best_modes[j][0] =
                                            ps_cu_node->ps_sub_cu[i]->au1_best_mode_4tu[0];
                                        ps_intra8_analyse->au1_4x4_best_modes[j][1] =
                                            ps_cu_node->ps_sub_cu[i]
                                                ->au1_best_mode_4tu[1];  //(ps_ed + 1)->best_mode;
                                        ps_intra8_analyse->au1_4x4_best_modes[j][2] =
                                            ps_cu_node->ps_sub_cu[i]
                                                ->au1_best_mode_4tu[2];  //(ps_ed + 2)->best_mode;
                                        ps_intra8_analyse->au1_4x4_best_modes[j][3] = 255;
                                    }
                                }
                            }
                            /* store the 32x32 non split flag */
                            ps_intra32_analyse->b1_split_flag = 0;
                            ps_intra32_analyse->as_intra16_analyse[0].b1_split_flag = 0;
                            ps_intra32_analyse->as_intra16_analyse[1].b1_split_flag = 0;
                            ps_intra32_analyse->as_intra16_analyse[2].b1_split_flag = 0;
                            ps_intra32_analyse->as_intra16_analyse[3].b1_split_flag = 0;

                            au1_best_32x32_modes[blk_cnt >> 4] =
                                ps_cu_node->ps_parent->au1_best_mode_1tu[0];

                            au4_best_32x32_cost[blk_cnt >> 4] =
                                ps_cu_node->ps_parent->au4_best_cost_1tu[0];
                            /*As 32*32 has won, pick L2 8x8 qp which maps
                            to L0 32x32 Qp*/
                            ASSERT(((blk_cnt >> 4) & 3) == (blk_cnt >> 4));
                            ASSERT(ps_ed_ctb_l1->i4_16x16_satd[blk_cnt >> 4][0] != -2);
                            u1_cu_possible_qp = ihevce_cu_level_qp_mod(
                                ps_ctxt->i4_qscale,
                                ps_ed_ctb_l1->i4_16x16_satd[blk_cnt >> 4][0],
                                ps_ctxt->ld_curr_frame_16x16_log_avg[0],
                                f_strength,
                                &i4_act_factor,
                                &i4_q_scale_q3_mod,
                                ps_ctxt->ps_rc_quant_ctxt);
                            /* cost accumalation of best cu size candiate */
                            i8_frame_acc_satd_cost += parent_cost;

                            /* satd and mpm bits accumalation of best cu size candiate */
                            i4_ctb_acc_satd += ps_cu_node->ps_parent->best_satd;

                            /* Mode bits cost accumalation for best cu size and cu mode */
                            i8_frame_acc_mode_bits_cost += ps_cu_node->ps_parent->u2_mode_bits_cost;

                            /*satd/mod_qp accumulation of best cu */
                            i8_frame_acc_satd_by_modqp_q10 +=
                                ((LWORD64)ps_cu_node->ps_parent->best_satd
                                 << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                                i4_q_scale_q3_mod;

                            /* Increment pointers */
                            ps_ed_blk_l1 += 16;
                            blk_cnt += 16;
                            //ps_row_cu++;
                            merge_64x64 &= 1;
                        }
                        else
                        {
                            /* store the 32x32 split flag */
                            ps_intra32_analyse->b1_split_flag = 1;

                            /* CU size 16x16 and fill the final cu params for all 4 blocks */
                            for(j = 0; j < 4; j++)
                            {
                                WORD32 i4_q_scale_q3_mod;
                                UWORD8 u1_cu_possible_qp;
                                WORD32 i4_act_factor;

                                /* Set CU split flag */
                                ASSERT(blk_cnt % 4 == 0);

                                ihevce_update_cand_list(
                                    ps_cu_node->ps_sub_cu[j], ps_ed_blk_l1, ps_ctxt);

                                /* store the 16x16 non split flag  */
                                ps_intra16_analyse[j].b1_split_flag = 0;

                                ASSERT(((blk_cnt >> 2) & 0xF) == (blk_cnt >> 2));
                                ASSERT(ps_ed_ctb_l1->i4_8x8_satd[blk_cnt >> 2][0] != -2);
                                /*As 16*16 has won, pick L1 8x8 qp which maps
                                to L0 16x16 Qp*/
                                u1_cu_possible_qp = ihevce_cu_level_qp_mod(
                                    ps_ctxt->i4_qscale,
                                    ps_ed_ctb_l1->i4_8x8_satd[blk_cnt >> 2][0],
                                    ps_ctxt->ld_curr_frame_8x8_log_avg[0],
                                    f_strength,
                                    &i4_act_factor,
                                    &i4_q_scale_q3_mod,
                                    ps_ctxt->ps_rc_quant_ctxt);

                                /*accum satd/qp for all child block*/
                                i8_frame_acc_satd_by_modqp_q10 +=
                                    ((LWORD64)child_satd[j]
                                     << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                                    i4_q_scale_q3_mod;

                                /* Accumalate mode bits for all child blocks */
                                i8_frame_acc_mode_bits_cost +=
                                    ps_cu_node->ps_sub_cu[j]->u2_mode_bits_cost;

                                /* satd and mpm bits accumalation of best cu size candiate */
                                i4_ctb_acc_satd += child_satd[j];

                                /* Increment pointers */
                                //ps_row_cu++;
                                ps_ed_blk_l1 += 4;
                                blk_cnt += 4;
                            }

                            /* cost accumalation of best cu size candiate */
                            i8_frame_acc_satd_cost += child_cost_least;

                            /* 64x64 merge is not possible */
                            merge_64x64 = 0;
                        }

                        //ps_ed_blk_l2 += 4;

                    }  //end of EIID's else
#endif
                }
                /* If Merge success for L1 max CU size 16x16 is chosen */
                else if(merge_16x16_l1)
                {
#if IP_DBG_L1_l2
                    ps_cu_node->ps_parent->u1_cu_size = 16;
                    ps_cu_node->ps_parent->u2_x0 = gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                    ps_cu_node->ps_parent->u2_y0 = gau1_cu_pos_y[blk_cnt]; /* Populate properly */
                    ps_cu_node->ps_parent->best_mode = ps_ed_blk_l1->best_merge_mode;
                    ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                    blk_cnt += 4;
                    ps_ed_blk_l1 += 4;
                    ps_row_cu++;
                    merge_64x64 = 0;
#else

                    /*EIID: evaluate only if L1 early-inter-intra decision is not favouring inter*/
                    /* enable this only in B pictures */
                    if(ps_ed_blk_l1->intra_or_inter == 2 && (ps_ctxt->i4_slice_type != ISLICE))
                    {
                        WORD32 i4_q_scale_q3_mod, i4_local_ctr;
                        WORD8 i1_cu_possible_qp;
                        WORD32 i4_act_factor;
                        /* make cost infinity. */
                        /* make modes invalid */
                        /* update loop variables */
                        /* set other output variales */
                        /* dont set neighbour flag so that next blocks wont access this cu */
                        /* what happens to ctb_mode_map?? */

                        ps_cu_node->ps_parent->u1_cu_size = 16;
                        ps_cu_node->ps_parent->u2_x0 =
                            gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->u2_y0 =
                            gau1_cu_pos_y[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->best_mode =
                            INTRA_DC;  //ps_ed_blk_l1->best_merge_mode;

                        /* fill in the first modes as invalid */

                        ps_cu_node->ps_parent->au1_best_mode_1tu[0] = INTRA_DC;
                        ps_cu_node->ps_parent->au1_best_mode_1tu[1] =
                            INTRA_DC;  //for safery. Since update_cand_list will set num_modes as 3
                        ps_cu_node->ps_parent->au1_best_mode_1tu[2] = INTRA_DC;

                        ps_cu_node->ps_parent->au1_best_mode_4tu[0] = INTRA_DC;
                        ps_cu_node->ps_parent->au1_best_mode_4tu[1] = INTRA_DC;
                        ps_cu_node->ps_parent->au1_best_mode_4tu[2] = INTRA_DC;

                        ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                        //ps_row_cu->s_cu_intra_cand.b6_num_intra_cands = 0;
                        //ps_row_cu->u1_num_intra_rdopt_cands = 0;

                        ps_intra32_analyse->b1_split_flag = 1;
                        ps_intra32_analyse->b1_merge_flag = 0;

                        ps_intra16_analyse->b1_valid_cu = 0;
                        ps_intra16_analyse->b1_split_flag = 0;
                        ps_intra16_analyse->b1_merge_flag = 1;
                        //memset (&ps_intra16_analyse->au1_best_modes_16x16_tu,
                        //  255,
                        //  NUM_BEST_MODES);
                        //memset (&ps_intra16_analyse->au1_best_modes_8x8_tu,
                        //  255,
                        //  NUM_BEST_MODES);
                        //set only first mode since if it's 255. it wont go ahead
                        ps_intra16_analyse->au1_best_modes_16x16_tu[0] = 255;
                        ps_intra16_analyse->au1_best_modes_8x8_tu[0] = 255;
                        *pi4_intra_16_cost = MAX_INTRA_COST_IPE;

                        /*since ME will start evaluating from bottom up, set the lower
                        cu size data invalid */
                        for(i4_local_ctr = 0; i4_local_ctr < 4; i4_local_ctr++)
                        {
                            ps_intra16_analyse->as_intra8_analyse[i4_local_ctr]
                                .au1_4x4_best_modes[0][0] = 255;
                            ps_intra16_analyse->as_intra8_analyse[i4_local_ctr]
                                .au1_4x4_best_modes[1][0] = 255;
                            ps_intra16_analyse->as_intra8_analyse[i4_local_ctr]
                                .au1_4x4_best_modes[2][0] = 255;
                            ps_intra16_analyse->as_intra8_analyse[i4_local_ctr]
                                .au1_4x4_best_modes[3][0] = 255;
                            ps_intra16_analyse->as_intra8_analyse[i4_local_ctr]
                                .au1_best_modes_8x8_tu[0] = 255;
                            ps_intra16_analyse->as_intra8_analyse[i4_local_ctr]
                                .au1_best_modes_4x4_tu[0] = 255;

                            pi4_intra_8_cost
                                [(i4_local_ctr & 1) + (MAX_CU_IN_CTB_ROW * (i4_local_ctr >> 1))] =
                                    MAX_INTRA_COST_IPE;
                        }

                        /* set neighbours even if intra is not evaluated, since source is always available. */
                        ihevce_set_nbr_map(
                            ps_ctxt->pu1_ctb_nbr_map,
                            ps_ctxt->i4_nbr_map_strd,
                            ps_cu_node->ps_parent->u2_x0 << 1,
                            ps_cu_node->ps_parent->u2_y0 << 1,
                            (ps_cu_node->ps_parent->u1_cu_size >> 2),
                            1);

                        //what happends to RC variables??
                        /* run only constant Qp */
                        ASSERT(((blk_cnt >> 2) & 0xF) == (blk_cnt >> 2));
                        ASSERT(ps_ed_ctb_l1->i4_8x8_satd[blk_cnt >> 2][0] != -2);
                        i1_cu_possible_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_8x8_satd[blk_cnt >> 2][0],
                            ps_ctxt->ld_curr_frame_8x8_log_avg[0],
                            f_strength,
                            &i4_act_factor,
                            &i4_q_scale_q3_mod,
                            ps_ctxt->ps_rc_quant_ctxt);

                        /* cost accumalation of best cu size candiate */
                        i8_frame_acc_satd_cost += 0;  //parent_cost;  //incorrect accumulation

                        /*satd/mod_qp accumulation of best cu */
                        i8_frame_acc_satd_by_modqp_q10 += 0;  //incorrect accumulation
                        //((LWORD64)ps_cu_node->ps_parent->best_satd << SATD_BY_ACT_Q_FAC)/i4_q_scale_q3_mod;

                        /* Accumalate mode bits for all child blocks */
                        i8_frame_acc_mode_bits_cost +=
                            0;  //ps_cu_node->ps_parent->u2_mode_bits_cost;
                        //incoorect accumulation

                        blk_cnt += 4;
                        ps_ed_blk_l1 += 4;
                        //ps_row_cu++;
                        merge_64x64 = 0;

                        /* increment for stat purpose only. Increment is valid only on single thread */
                        ps_ctxt->u4_num_16x16_skips_at_L0_IPE += 1;
                    }
                    else
                    {
                        /* 64x64 merge is not possible */
                        merge_64x64 = 0;

                        /* set the 32x32 split flag to 1 */
                        ps_intra32_analyse->b1_split_flag = 1;

                        ps_intra32_analyse->b1_merge_flag = 0;

                        ps_intra16_analyse->b1_merge_flag = 1;

                        if((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6) &&
                           (ps_ctxt->i4_slice_type == PSLICE))
                        {
                            ps_ctxt->u1_disable_child_cu_decide = 1;
                            step2_bypass = 0;
                        }
                        //memcpy(ps_ctxt->ai1_ctb_mode_map_temp, ps_ctxt->ai1_ctb_mode_map, sizeof(ps_ctxt->ai1_ctb_mode_map));
                        /* Based on the flag, Child modes decision can be disabled*/
                        if(0 == ps_ctxt->u1_disable_child_cu_decide)
                        {
                            for(j = 0; j < 4; j++)
                            {
                                intra8_analyse_t *ps_intra8_analyse;
                                WORD32 best_ang_mode = (ps_ed_blk_l1 + j)->best_mode;

                                if(best_ang_mode < 2)
                                    best_ang_mode = 26;

                                //ps_cu_node->ps_sub_cu[j]->best_cost = MAX_INTRA_COST_IPE;
                                //ps_cu_node->ps_sub_cu[j]->best_mode = (ps_ed_blk_l1 + j)->best_mode;

                                ps_cu_node->ps_sub_cu[j]->u2_x0 =
                                    gau1_cu_pos_x[blk_cnt + j]; /* Populate properly */
                                ps_cu_node->ps_sub_cu[j]->u2_y0 =
                                    gau1_cu_pos_y[blk_cnt + j]; /* Populate properly */
                                ps_cu_node->ps_sub_cu[j]->u1_cu_size = 8;

                                ihevce_mode_eval_filtering(
                                    ps_cu_node->ps_sub_cu[j],
                                    ps_cu_node,
                                    ps_ctxt,
                                    ps_curr_src,
                                    best_ang_mode,
                                    &ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0],
                                    &ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0],
                                    !step2_bypass,
                                    1);

                                if(i4_enable_4cu_16tu)
                                {
                                    ihevce_mode_eval_filtering(
                                        ps_cu_node->ps_sub_cu[j],
                                        ps_cu_node,
                                        ps_ctxt,
                                        ps_curr_src,
                                        best_ang_mode,
                                        &ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0],
                                        &ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0],
                                        !step2_bypass,
                                        0);
                                }
                                else
                                {
                                    /* 4TU not evaluated :  4tu modes set same as 1tu modes */
                                    memcpy(
                                        &ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0],
                                        &ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0],
                                        NUM_BEST_MODES);

                                    /* 4TU not evaluated : currently 4tu cost set same as 1tu cost */
                                    memcpy(
                                        &ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0],
                                        &ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0],
                                        NUM_BEST_MODES * sizeof(WORD32));
                                }

                                child_cost[j] =
                                    MIN(ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0],
                                        ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0]);

                                child_cost_least += child_cost[j];

                                /* Select the best mode to be populated as top and left nbr depending on the
                                4tu and 1tu cost */
                                if(ps_cu_node->ps_sub_cu[j]->au4_best_cost_4tu[0] >
                                   ps_cu_node->ps_sub_cu[j]->au4_best_cost_1tu[0])
                                {
                                    ps_cu_node->ps_sub_cu[j]->best_mode =
                                        ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0];
                                }
                                else
                                {
                                    ps_cu_node->ps_sub_cu[j]->best_mode =
                                        ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0];
                                }
                                { /* Update the CTB nodes only for MAX - 1 CU nodes */
                                    WORD32 xA, yA, row, col;
                                    xA = ((ps_cu_node->ps_sub_cu[j]->u2_x0 << 3) >> 2) + 1;
                                    yA = ((ps_cu_node->ps_sub_cu[j]->u2_y0 << 3) >> 2) + 1;
                                    size = ps_cu_node->ps_sub_cu[j]->u1_cu_size >> 2;
                                    for(row = yA; row < (yA + size); row++)
                                    {
                                        for(col = xA; col < (xA + size); col++)
                                        {
                                            ps_ctxt->au1_ctb_mode_map[row][col] =
                                                ps_cu_node->ps_sub_cu[j]->best_mode;
                                        }
                                    }
                                }

                                /*collect individual child satd for final SATD/qp accum*/
                                child_satd[j] = ps_cu_node->ps_sub_cu[j]->best_satd;

                                ps_intra8_analyse = &ps_intra16_analyse->as_intra8_analyse[j];

                                /* store the child 8x8 costs */
                                pi4_intra_8_cost[(j & 1) + (MAX_CU_IN_CTB_ROW * (j >> 1))] =
                                    child_cost[j];

                                /* set the CU valid flag */
                                ps_intra8_analyse->b1_valid_cu = 1;
                                ps_intra8_analyse->b1_enable_nxn = 0;

                                /* storing the modes to intra8  analyse */

                                /* store the best 8x8 modes 8x8 tu */
                                memcpy(
                                    &ps_intra8_analyse->au1_best_modes_8x8_tu[0],
                                    &ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0],
                                    sizeof(UWORD8) * (NUM_BEST_MODES));
                                ps_intra8_analyse->au1_best_modes_8x8_tu[NUM_BEST_MODES] = 255;

                                /* store the best 8x8 modes 4x4 tu */
                                memcpy(
                                    &ps_intra8_analyse->au1_best_modes_4x4_tu[0],
                                    &ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0],
                                    sizeof(UWORD8) * (NUM_BEST_MODES));
                                ps_intra8_analyse->au1_best_modes_4x4_tu[NUM_BEST_MODES] = 255;

                                /* NXN modes not evaluated hence set to 255 */
                                memset(
                                    &ps_intra8_analyse->au1_4x4_best_modes[0][0],
                                    255,
                                    sizeof(UWORD8) * 4 * (NUM_BEST_MODES + 1));
                            }

                            ihevce_set_nbr_map(
                                ps_ctxt->pu1_ctb_nbr_map,
                                ps_ctxt->i4_nbr_map_strd,
                                ps_cu_node->ps_sub_cu[0]->u2_x0 << 1,
                                ps_cu_node->ps_sub_cu[0]->u2_y0 << 1,
                                (ps_cu_node->ps_sub_cu[0]->u1_cu_size >> 1),
                                0);
                        }
#if 1  //DISBLE_CHILD_CU_EVAL_L0_IPE //1
                        else
                        {
                            for(j = 0; j < 4; j++)
                            {
                                intra8_analyse_t *ps_intra8_analyse;
                                ps_intra8_analyse = &ps_intra16_analyse->as_intra8_analyse[j];
                                ps_intra8_analyse->au1_best_modes_8x8_tu[0] = 255;
                                ps_intra8_analyse->au1_best_modes_4x4_tu[0] = 255;
                                /* NXN modes not evaluated hence set to 255 */
                                memset(
                                    &ps_intra8_analyse->au1_4x4_best_modes[0][0],
                                    255,
                                    sizeof(UWORD8) * 4 * (NUM_BEST_MODES + 1));

                                ps_intra8_analyse->b1_valid_cu = 0;
                                ps_intra8_analyse->b1_enable_nxn = 0;
                            }
                            child_cost_least = MAX_INTRA_COST_IPE;
                        }
#endif
                        //ps_cu_node->ps_parent->best_mode = ps_ed_blk_l1->best_mode;
                        //ps_cu_node->ps_parent->best_cost = MAX_INTRA_COST_IPE;

                        ps_cu_node->ps_parent->u1_cu_size = 16;
                        ps_cu_node->ps_parent->u2_x0 =
                            gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->u2_y0 =
                            gau1_cu_pos_y[blk_cnt]; /* Populate properly */

                        //memcpy(ps_ctxt->ai1_ctb_mode_map_temp, ps_ctxt->ai1_ctb_mode_map, sizeof(ps_ctxt->ai1_ctb_mode_map));

                        /* Eval for TUSize = CuSize */
                        ihevce_mode_eval_filtering(
                            ps_cu_node->ps_parent,
                            ps_cu_node,
                            ps_ctxt,
                            ps_curr_src,
                            26,
                            &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                            &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                            step2_bypass,
                            1);

                        if(i4_enable_1cu_4tu)
                        {
                            /* Eval for TUSize = CuSize/2 */
                            ihevce_mode_eval_filtering(
                                ps_cu_node->ps_parent,
                                ps_cu_node,
                                ps_ctxt,
                                ps_curr_src,
                                26,
                                &ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                step2_bypass,
                                0);
                        }
                        else
                        {
                            /* 4TU not evaluated :  4tu modes set same as 1tu modes */
                            memcpy(
                                &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                NUM_BEST_MODES);

                            /* 4TU not evaluated : currently 4tu cost set same as 1tu cost */
                            memcpy(
                                &ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                                NUM_BEST_MODES * sizeof(WORD32));
                        }

                        ps_ctxt->u1_disable_child_cu_decide = 0;
                        step2_bypass = 1;

                        /* Update parent cost */
                        parent_cost =
                            MIN(ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                ps_cu_node->ps_parent->au4_best_cost_1tu[0]);

                        /* Select the best mode to be populated as top and left nbr depending on the
                        4tu and 1tu cost */
                        if(ps_cu_node->ps_parent->au4_best_cost_4tu[0] >
                           ps_cu_node->ps_parent->au4_best_cost_1tu[0])
                        {
                            ps_cu_node->ps_parent->best_mode =
                                ps_cu_node->ps_parent->au1_best_mode_1tu[0];
                        }
                        else
                        {
                            ps_cu_node->ps_parent->best_mode =
                                ps_cu_node->ps_parent->au1_best_mode_4tu[0];
                        }

                        /* store the 16x16 cost */
                        *pi4_intra_16_cost = parent_cost;

                        /* accumulate the 32x32 cost */
                        if(MAX_INTRA_COST_IPE == *pi4_intra_32_cost)
                        {
                            *pi4_intra_32_cost = parent_cost;
                        }
                        else
                        {
                            *pi4_intra_32_cost += parent_cost;
                        }

                        /* set the CU valid flag */
                        ps_intra16_analyse->b1_valid_cu = 1;

                        /* storing the modes to intra 16 analyse */
                        {
                            /* store the best 16x16 modes 16x16 tu */
                            memcpy(
                                &ps_intra16_analyse->au1_best_modes_16x16_tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                sizeof(UWORD8) * NUM_BEST_MODES);
                            ps_intra16_analyse->au1_best_modes_16x16_tu[NUM_BEST_MODES] = 255;

                            /* store the best 16x16 modes 8x8 tu */
                            memcpy(
                                &ps_intra16_analyse->au1_best_modes_8x8_tu[0],
                                &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                sizeof(UWORD8) * NUM_BEST_MODES);
                            ps_intra16_analyse->au1_best_modes_8x8_tu[NUM_BEST_MODES] = 255;
                        }

                        parent_best_mode = ps_cu_node->ps_parent->best_mode;
                        if(parent_cost <=
                           child_cost_least + (ps_ctxt->i4_ol_satd_lambda * CHILD_BIAS >>
                                               LAMBDA_Q_SHIFT))  //|| identical_modes)
                        {
                            WORD32 i4_q_scale_q3_mod;
                            WORD8 i1_cu_possible_qp;
                            WORD32 i4_act_factor;
                            //choose parent CU

                            ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                            /* set the 16x16 non split flag */
                            ps_intra16_analyse->b1_split_flag = 0;

                            /*As 16*16 has won, pick L1 8x8 qp which maps
                            to L0 16x16 Qp*/
                            ASSERT(((blk_cnt >> 4) & 3) == (blk_cnt >> 4));
                            ASSERT(ps_ed_ctb_l1->i4_16x16_satd[blk_cnt >> 4][0] != -2);
                            i1_cu_possible_qp = ihevce_cu_level_qp_mod(
                                ps_ctxt->i4_qscale,
                                ps_ed_ctb_l1->i4_16x16_satd[blk_cnt >> 4][0],
                                ps_ctxt->ld_curr_frame_8x8_log_avg[0],
                                f_strength,
                                &i4_act_factor,
                                &i4_q_scale_q3_mod,
                                ps_ctxt->ps_rc_quant_ctxt);

                            /* cost accumalation of best cu size candiate */
                            i8_frame_acc_satd_cost += parent_cost;

                            /* satd and mpm bits accumalation of best cu size candiate */
                            i4_ctb_acc_satd += ps_cu_node->ps_parent->best_satd;

                            /*satd/mod_qp accumulation of best cu */
                            i8_frame_acc_satd_by_modqp_q10 +=
                                ((LWORD64)ps_cu_node->ps_parent->best_satd
                                 << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                                i4_q_scale_q3_mod;

                            /* Accumalate mode bits for all child blocks */
                            i8_frame_acc_mode_bits_cost += ps_cu_node->ps_parent->u2_mode_bits_cost;

                            blk_cnt += 4;
                            ps_ed_blk_l1 += 4;
                            //ps_row_cu++;
                        }
                        else
                        {
                            //choose child CU
                            WORD8 i1_cu_possible_qp;
                            WORD32 i4_act_factor;
                            WORD32 i4_q_scale_q3_mod;

                            ASSERT(((blk_cnt >> 2) & 0xF) == (blk_cnt >> 2));
                            ASSERT(ps_ed_ctb_l1->i4_8x8_satd[blk_cnt >> 2][1] != -2);
                            i1_cu_possible_qp = ihevce_cu_level_qp_mod(
                                ps_ctxt->i4_qscale,
                                ps_ed_ctb_l1->i4_8x8_satd[blk_cnt >> 2][1],
                                ps_ctxt->ld_curr_frame_8x8_log_avg[1],
                                f_strength,
                                &i4_act_factor,
                                &i4_q_scale_q3_mod,
                                ps_ctxt->ps_rc_quant_ctxt);

                            /* set the 16x16 split flag */
                            ps_intra16_analyse->b1_split_flag = 1;

                            for(j = 0; j < 4; j++)
                            {
                                ihevce_update_cand_list(
                                    ps_cu_node->ps_sub_cu[j], ps_ed_blk_l1, ps_ctxt);

                                if((IHEVCE_QUALITY_P3 > i4_quality_preset))
                                {
                                    WORD32 k;
                                    intra8_analyse_t *ps_intra8_analyse;
                                    ps_intra8_analyse = &ps_intra16_analyse->as_intra8_analyse[j];

                                    for(k = 0; k < 4; k++)
                                    {
                                        /* Populate best 3 nxn modes */
                                        ps_intra8_analyse->au1_4x4_best_modes[k][0] =
                                            ps_cu_node->ps_sub_cu[j]->au1_best_mode_4tu[0];
                                        ps_intra8_analyse->au1_4x4_best_modes[k][1] =
                                            ps_cu_node->ps_sub_cu[j]
                                                ->au1_best_mode_4tu[1];  //(ps_ed + 1)->best_mode;
                                        ps_intra8_analyse->au1_4x4_best_modes[k][2] =
                                            ps_cu_node->ps_sub_cu[j]
                                                ->au1_best_mode_4tu[2];  //(ps_ed + 2)->best_mode;
                                        ps_intra8_analyse->au1_4x4_best_modes[k][3] = 255;
                                    }
                                }
                                /*accum satd/qp for all child block*/
                                i8_frame_acc_satd_by_modqp_q10 +=
                                    ((LWORD64)child_satd[j]
                                     << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                                    i4_q_scale_q3_mod;

                                /* Accumalate mode bits for all child blocks */
                                i8_frame_acc_mode_bits_cost +=
                                    ps_cu_node->ps_sub_cu[j]->u2_mode_bits_cost;

                                /* satd and mpm bits accumalation of best cu size candiate */
                                i4_ctb_acc_satd += child_satd[j];

                                blk_cnt += 1;
                                ps_ed_blk_l1 += 1;
                                //ps_row_cu++;
                            }

                            /* cost accumalation of best cu size candiate */
                            i8_frame_acc_satd_cost += child_cost_least;
                        }

                    }  //else of EIID
#endif
                }  // if(merge_16x16_l1)
                /* MAX CU SIZE 8x8 */
                else
                {
#if IP_DBG_L1_l2
                    for(i = 0; i < 4; i++)
                    {
                        ps_cu_node->ps_parent->u1_cu_size = 8;
                        ps_cu_node->ps_parent->u2_x0 =
                            gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->u2_y0 =
                            gau1_cu_pos_y[blk_cnt]; /* Populate properly */
                        ps_cu_node->ps_parent->best_mode = ps_ed_blk_l1->best_mode;

                        ihevce_update_cand_list(ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);
                        blk_cnt++;
                        ps_ed_blk_l1++;
                        ps_row_cu++;
                        merge_64x64 = 0;
                    }
#else

                    /* EIID: Skip all 4 8x8 block if L1 decisions says skip intra */
                    if(ps_ed_blk_l1->intra_or_inter == 2 && (ps_ctxt->i4_slice_type != ISLICE))
                    {
                        WORD32 i4_q_scale_q3_mod;
                        WORD8 i1_cu_possible_qp;
                        WORD32 i4_act_factor;

                        merge_64x64 = 0;

                        ps_intra32_analyse->b1_merge_flag = 0;

                        ps_intra16_analyse->au1_best_modes_8x8_tu[0] = 255;
                        ps_intra16_analyse->au1_best_modes_8x8_tu[1] = 255;
                        ps_intra16_analyse->au1_best_modes_8x8_tu[2] = 255;

                        ps_intra16_analyse->au1_best_modes_16x16_tu[0] = 255;
                        ps_intra16_analyse->au1_best_modes_16x16_tu[1] = 255;
                        ps_intra16_analyse->au1_best_modes_16x16_tu[2] = 255;
                        ps_intra16_analyse->b1_split_flag = 1;
                        ps_intra16_analyse->b1_valid_cu = 0;
                        ps_intra16_analyse->b1_merge_flag = 0;

                        for(i = 0; i < 4; i++)
                        {
                            intra8_analyse_t *ps_intra8_analyse;
                            WORD32 ctr_sub_cu;

                            cu_pos_x = gau1_cu_pos_x[blk_cnt];
                            cu_pos_y = gau1_cu_pos_y[blk_cnt];

                            if((cu_pos_x < num_8x8_blks_x) && (cu_pos_y < num_8x8_blks_y))
                            {
                                ps_intra8_analyse = &ps_intra16_analyse->as_intra8_analyse[i];

                                ps_intra8_analyse->b1_valid_cu = 0;
                                ps_intra8_analyse->b1_enable_nxn = 0;
                                ps_intra8_analyse->au1_4x4_best_modes[0][0] = 255;
                                ps_intra8_analyse->au1_4x4_best_modes[1][0] = 255;
                                ps_intra8_analyse->au1_4x4_best_modes[2][0] = 255;
                                ps_intra8_analyse->au1_4x4_best_modes[3][0] = 255;
                                ps_intra8_analyse->au1_best_modes_4x4_tu[0] = 255;
                                ps_intra8_analyse->au1_best_modes_8x8_tu[0] = 255;

                                ps_cu_node->ps_parent->u1_cu_size = 8;
                                ps_cu_node->ps_parent->u2_x0 =
                                    gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                                ps_cu_node->ps_parent->u2_y0 =
                                    gau1_cu_pos_y[blk_cnt]; /* Populate properly */
                                ps_cu_node->ps_parent->best_mode =
                                    INTRA_DC;  //ps_ed_blk_l1->best_mode;

                                /* fill in the first modes as invalid */

                                ps_cu_node->ps_parent->au1_best_mode_1tu[0] = INTRA_DC;
                                ps_cu_node->ps_parent->au1_best_mode_1tu[1] =
                                    INTRA_DC;  //for safery. Since update_cand_list will set num_modes as 3
                                ps_cu_node->ps_parent->au1_best_mode_1tu[2] = INTRA_DC;

                                ps_cu_node->ps_parent->au1_best_mode_4tu[0] = INTRA_DC;
                                ps_cu_node->ps_parent->au1_best_mode_4tu[1] = INTRA_DC;
                                ps_cu_node->ps_parent->au1_best_mode_4tu[2] = INTRA_DC;

                                ihevce_update_cand_list(
                                    ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                                //ps_row_cu->s_cu_intra_cand.b6_num_intra_cands = 0;
                                //ps_row_cu->u1_num_intra_rdopt_cands = 0;

                                for(ctr_sub_cu = 0; ctr_sub_cu < 4; ctr_sub_cu++)
                                {
                                    ps_cu_node->ps_sub_cu[ctr_sub_cu]->au1_best_mode_1tu[0] =
                                        INTRA_DC;
                                    ps_cu_node->ps_sub_cu[ctr_sub_cu]->au1_best_mode_4tu[0] =
                                        INTRA_DC;
                                    ps_cu_node->ps_sub_cu[ctr_sub_cu]->au4_best_cost_1tu[0] =
                                        MAX_INTRA_COST_IPE;

                                    ps_cu_node->ps_sub_cu[ctr_sub_cu]->au4_best_cost_4tu[0] =
                                        MAX_INTRA_COST_IPE;
                                    ps_cu_node->ps_sub_cu[ctr_sub_cu]->best_cost =
                                        MAX_INTRA_COST_IPE;
                                }

                                pi4_intra_8_cost[(i & 1) + (MAX_CU_IN_CTB_ROW * (i >> 1))] =
                                    MAX_INTRA_COST_IPE;

                                ASSERT(((blk_cnt >> 2) & 0xF) == (blk_cnt >> 2));
                                ASSERT(ps_ed_ctb_l1->i4_8x8_satd[(blk_cnt >> 2)][1] != -2);
                                i1_cu_possible_qp = ihevce_cu_level_qp_mod(
                                    ps_ctxt->i4_qscale,
                                    ps_ed_ctb_l1->i4_8x8_satd[(blk_cnt >> 2)][1],
                                    ps_ctxt->ld_curr_frame_8x8_log_avg[1],
                                    f_strength,
                                    &i4_act_factor,
                                    &i4_q_scale_q3_mod,
                                    ps_ctxt->ps_rc_quant_ctxt);

                                /* set neighbours even if intra is not evaluated, since source is always available. */
                                ihevce_set_nbr_map(
                                    ps_ctxt->pu1_ctb_nbr_map,
                                    ps_ctxt->i4_nbr_map_strd,
                                    ps_cu_node->ps_parent->u2_x0 << 1,
                                    ps_cu_node->ps_parent->u2_y0 << 1,
                                    (ps_cu_node->ps_parent->u1_cu_size >> 2),
                                    1);

                                //ps_row_cu++;
                            }
                            blk_cnt++;
                            ps_ed_blk_l1++;
                        }
                    }
                    else
                    {
                        //cu_intra_cand_t *ps_cu_intra_cand;
                        WORD8 i1_cu_possible_qp;
                        WORD32 i4_act_factor;
                        WORD32 i4_q_scale_q3_mod;

                        ASSERT(((blk_cnt >> 2) & 0xF) == (blk_cnt >> 2));
                        ASSERT(ps_ed_ctb_l1->i4_8x8_satd[(blk_cnt >> 2)][1] != -2);
                        i1_cu_possible_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_8x8_satd[(blk_cnt >> 2)][1],
                            ps_ctxt->ld_curr_frame_8x8_log_avg[1],
                            f_strength,
                            &i4_act_factor,
                            &i4_q_scale_q3_mod,
                            ps_ctxt->ps_rc_quant_ctxt);

                        /* 64x64 merge is not possible */
                        merge_64x64 = 0;

                        ps_intra32_analyse->b1_merge_flag = 0;

                        ps_intra16_analyse->b1_merge_flag = 0;

                        /* by default 16x16 modes are set to default values DC and Planar */
                        ps_intra16_analyse->au1_best_modes_8x8_tu[0] = 0;
                        ps_intra16_analyse->au1_best_modes_8x8_tu[1] = 1;
                        ps_intra16_analyse->au1_best_modes_8x8_tu[2] = 255;

                        ps_intra16_analyse->au1_best_modes_16x16_tu[0] = 0;
                        ps_intra16_analyse->au1_best_modes_16x16_tu[1] = 1;
                        ps_intra16_analyse->au1_best_modes_16x16_tu[2] = 255;
                        ps_intra16_analyse->b1_split_flag = 1;
                        ps_intra16_analyse->b1_valid_cu = 1;

                        for(i = 0; i < 4; i++)
                        {
                            intra8_analyse_t *ps_intra8_analyse;
                            cu_pos_x = gau1_cu_pos_x[blk_cnt];
                            cu_pos_y = gau1_cu_pos_y[blk_cnt];
                            if((cu_pos_x < num_8x8_blks_x) && (cu_pos_y < num_8x8_blks_y))
                            {
                                //ps_cu_intra_cand = &ps_row_cu->s_cu_intra_cand;
                                //ps_cu_node->ps_parent->best_cost = MAX_INTRA_COST_IPE;

                                //ps_cu_node->ps_parent->best_mode = ps_ed_blk_l1->best_mode;

                                child_cost_least = 0;

                                ps_intra8_analyse = &ps_intra16_analyse->as_intra8_analyse[i];
                                ps_cu_node->ps_parent->u1_cu_size = 8;
                                ps_cu_node->ps_parent->u2_x0 =
                                    gau1_cu_pos_x[blk_cnt]; /* Populate properly */
                                ps_cu_node->ps_parent->u2_y0 =
                                    gau1_cu_pos_y[blk_cnt]; /* Populate properly */

                                //memcpy(ps_ctxt->ai1_ctb_mode_map_temp, ps_ctxt->ai1_ctb_mode_map, sizeof(ps_ctxt->ai1_ctb_mode_map));

                                /*EARLY DECISION 8x8 block */
                                ihevce_pu_calc_8x8_blk(
                                    ps_curr_src, ps_ctxt, ps_cu_node, ps_ctxt->ps_func_selector);
                                for(j = 0; j < 4; j++)
                                {
                                    child_cost_least += ps_cu_node->ps_sub_cu[j]->best_cost;
                                    child_satd[j] = ps_cu_node->ps_sub_cu[j]->best_satd;
                                }

                                /* Based on the flag, CU = 4TU modes decision can be disabled, CU = 4PU is retained */
                                if(0 == ps_ctxt->u1_disable_child_cu_decide)
                                {
                                    ihevce_set_nbr_map(
                                        ps_ctxt->pu1_ctb_nbr_map,
                                        ps_ctxt->i4_nbr_map_strd,
                                        ps_cu_node->ps_parent->u2_x0 << 1,
                                        ps_cu_node->ps_parent->u2_y0 << 1,
                                        (ps_cu_node->ps_parent->u1_cu_size >> 2),
                                        0);

                                    //memcpy(ps_ctxt->ai1_ctb_mode_map_temp, ps_ctxt->ai1_ctb_mode_map, sizeof(ps_ctxt->ai1_ctb_mode_map));

                                    /* Eval for TUSize = CuSize */
                                    ihevce_mode_eval_filtering(
                                        ps_cu_node->ps_parent,
                                        ps_cu_node,
                                        ps_ctxt,
                                        ps_curr_src,
                                        26,
                                        &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                                        &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                        step2_bypass,
                                        1);

                                    if(i4_enable_1cu_4tu)
                                    {
                                        /* Eval for TUSize = CuSize/2 */
                                        ihevce_mode_eval_filtering(
                                            ps_cu_node->ps_parent,
                                            ps_cu_node,
                                            ps_ctxt,
                                            ps_curr_src,
                                            26,
                                            &ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                            &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                            step2_bypass,
                                            0);
                                    }
                                    else
                                    {
                                        /* 4TU not evaluated :  4tu modes set same as 1tu modes */
                                        memcpy(
                                            &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                            &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                            NUM_BEST_MODES);

                                        /* 4TU not evaluated : currently 4tu cost set same as 1tu cost */
                                        memcpy(
                                            &ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                            &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                                            NUM_BEST_MODES * sizeof(WORD32));
                                    }

                                    /* Update parent cost */
                                    parent_cost =
                                        MIN(ps_cu_node->ps_parent->au4_best_cost_4tu[0],
                                            ps_cu_node->ps_parent->au4_best_cost_1tu[0]);

                                    /* Select the best mode to be populated as top and left nbr depending on the
                            4tu and 1tu cost */
                                    if(ps_cu_node->ps_parent->au4_best_cost_4tu[0] >
                                       ps_cu_node->ps_parent->au4_best_cost_1tu[0])
                                    {
                                        ps_cu_node->ps_parent->best_mode =
                                            ps_cu_node->ps_parent->au1_best_mode_1tu[0];
                                    }
                                    else
                                    {
                                        ps_cu_node->ps_parent->best_mode =
                                            ps_cu_node->ps_parent->au1_best_mode_4tu[0];
                                    }
                                }

                                /* set the CU valid flag */
                                ps_intra8_analyse->b1_valid_cu = 1;
                                ps_intra8_analyse->b1_enable_nxn = 0;

                                /* storing the modes to intra 8 analyse */

                                /* store the best 8x8 modes 8x8 tu */
                                memcpy(
                                    &ps_intra8_analyse->au1_best_modes_8x8_tu[0],
                                    &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                                    sizeof(UWORD8) * (NUM_BEST_MODES));
                                ps_intra8_analyse->au1_best_modes_8x8_tu[NUM_BEST_MODES] = 255;

                                /* store the best 8x8 modes 4x4 tu */
                                memcpy(
                                    &ps_intra8_analyse->au1_best_modes_4x4_tu[0],
                                    &ps_cu_node->ps_parent->au1_best_mode_4tu[0],
                                    sizeof(UWORD8) * (NUM_BEST_MODES));
                                ps_intra8_analyse->au1_best_modes_4x4_tu[NUM_BEST_MODES] = 255;

                                /*As 8*8 has won, pick L1 4x4 qp which is equal to
                                L1 8x8 Qp*/
                                //ps_row_cu->u1_cu_possible_qp[0] = u1_cu_possible_qp;
                                //ps_row_cu->i4_act_factor[0][1] = i4_act_factor;

                                parent_best_mode = ps_cu_node->ps_parent->best_mode;
                                if(parent_cost <=
                                   child_cost_least +
                                       (ps_ctxt->i4_ol_satd_lambda * CHILD_BIAS >> LAMBDA_Q_SHIFT))
                                {
                                    /*CU = 4TU */
                                    ihevce_update_cand_list(
                                        ps_cu_node->ps_parent, ps_ed_blk_l1, ps_ctxt);

                                    /* store the child 8x8 costs */
                                    pi4_intra_8_cost[(i & 1) + (MAX_CU_IN_CTB_ROW * (i >> 1))] =
                                        parent_cost;

                                    /* cost accumalation of best cu size candiate */
                                    i8_frame_acc_satd_cost += parent_cost;

                                    /*satd/mod_qp accumulation of best cu */
                                    i8_frame_acc_satd_by_modqp_q10 +=
                                        ((LWORD64)ps_cu_node->ps_parent->best_satd
                                         << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                                        i4_q_scale_q3_mod;

                                    /* Accumalate mode bits for all child blocks */
                                    i8_frame_acc_mode_bits_cost +=
                                        ps_cu_node->ps_parent->u2_mode_bits_cost;

                                    /* satd and mpm bits accumalation of best cu size candiate */
                                    i4_ctb_acc_satd += ps_cu_node->ps_parent->best_satd;

                                    /* accumulate the 16x16 cost*/
                                    if(MAX_INTRA_COST_IPE == *pi4_intra_16_cost)
                                    {
                                        *pi4_intra_16_cost = parent_cost;
                                    }
                                    else
                                    {
                                        *pi4_intra_16_cost += parent_cost;
                                    }

                                    /* accumulate the 32x32 cost*/
                                    if(MAX_INTRA_COST_IPE == *pi4_intra_32_cost)
                                    {
                                        *pi4_intra_32_cost = parent_cost;
                                    }
                                    else
                                    {
                                        *pi4_intra_32_cost += parent_cost;
                                    }
                                }
                                else
                                {
                                    /*CU = 4PU*/
                                    //ps_row_cu->b3_cu_pos_x = (UWORD8) ps_cu_node->ps_parent->u2_x0;
                                    //ps_row_cu->b3_cu_pos_y = (UWORD8) ps_cu_node->ps_parent->u2_y0;
                                    //ps_row_cu->u1_cu_size  = ps_cu_node->ps_parent->u1_cu_size;

                                    /* store the child 8x8 costs woth 4x4 pu summed cost */
                                    pi4_intra_8_cost[(i & 1) + (MAX_CU_IN_CTB_ROW * (i >> 1))] =
                                        (child_cost_least);

                                    /* accumulate the 16x16 cost*/
                                    if(MAX_INTRA_COST_IPE == *pi4_intra_16_cost)
                                    {
                                        *pi4_intra_16_cost = child_cost_least;
                                    }
                                    else
                                    {
                                        *pi4_intra_16_cost += child_cost_least;
                                    }

                                    /* cost accumalation of best cu size candiate */
                                    i8_frame_acc_satd_cost += child_cost_least;

                                    for(j = 0; j < 4; j++)
                                    {
                                        /*satd/qp accumualtion*/
                                        i8_frame_acc_satd_by_modqp_q10 +=
                                            ((LWORD64)child_satd[j]
                                             << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                                            i4_q_scale_q3_mod;

                                        /* Accumalate mode bits for all child blocks */
                                        i8_frame_acc_mode_bits_cost +=
                                            ps_cu_node->ps_sub_cu[j]->u2_mode_bits_cost;

                                        /* satd and mpm bits accumalation of best cu size candiate */
                                        i4_ctb_acc_satd += child_satd[j];
                                    }

                                    /* accumulate the 32x32 cost*/
                                    if(MAX_INTRA_COST_IPE == *pi4_intra_32_cost)
                                    {
                                        *pi4_intra_32_cost = child_cost_least;
                                    }
                                    else
                                    {
                                        *pi4_intra_32_cost += child_cost_least;
                                    }

                                    ps_intra8_analyse->b1_enable_nxn = 1;

                                    /* Insert the best 8x8 modes unconditionally */

                                    x = ((ps_cu_node->u2_x0 << 3) >> 2) + 1;
                                    y = ((ps_cu_node->u2_y0 << 3) >> 2) + 1;
                                    size = ps_cu_node->u1_cu_size >> 2;

                                    ps_ctxt->au1_ctb_mode_map[y][x] =
                                        ps_cu_node->ps_sub_cu[0]->best_mode;
                                    ps_ctxt->au1_ctb_mode_map[y][x + 1] =
                                        ps_cu_node->ps_sub_cu[1]->best_mode;
                                    ps_ctxt->au1_ctb_mode_map[y + 1][x] =
                                        ps_cu_node->ps_sub_cu[2]->best_mode;
                                    ps_ctxt->au1_ctb_mode_map[y + 1][x + 1] =
                                        ps_cu_node->ps_sub_cu[3]->best_mode;
                                }
                                /* NXN mode population */
                                for(j = 0; j < 4; j++)
                                {
                                    cand_mode_list[0] =
                                        ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[0];
                                    cand_mode_list[1] =
                                        ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[1];
                                    cand_mode_list[2] =
                                        ps_cu_node->ps_sub_cu[j]->au1_best_mode_1tu[2];

                                    if(1)
                                    {
                                        /* Populate best 3 nxn modes */
                                        ps_intra8_analyse->au1_4x4_best_modes[j][0] =
                                            cand_mode_list[0];
                                        ps_intra8_analyse->au1_4x4_best_modes[j][1] =
                                            cand_mode_list[1];  //(ps_ed + 1)->best_mode;
                                        ps_intra8_analyse->au1_4x4_best_modes[j][2] =
                                            cand_mode_list[2];  //(ps_ed + 2)->best_mode;
                                        ps_intra8_analyse->au1_4x4_best_modes[j][3] = 255;

                                        //memcpy(ps_intra8_analyse->au1_4x4_best_modes[j], ps_row_cu->s_cu_intra_cand.au1_intra_luma_modes_nxn[j], 4);
                                    }
                                    /* For HQ, all 35 modes to be used for RDOPT, removed from here for memory clean-up */

                                    else /* IHEVCE_QUALITY_P0 == i4_quality_preset */
                                    {
                                        /* To indicate to enc loop that NXN is enabled in HIGH QUALITY fior CU 8x8*/
                                        ps_intra8_analyse->au1_4x4_best_modes[j][0] = 0;
                                    }

                                    ps_intra8_analyse
                                        ->au1_4x4_best_modes[j][MAX_INTRA_CU_CANDIDATES] = 255;
                                }

                                //ps_row_cu++;
                            }
                            else
                            {
                                /* For Incomplete CTB, 16x16 is not valid */
                                ps_intra16_analyse->b1_valid_cu = 0;
                            }
                            blk_cnt++;
                            ps_ed_blk_l1++;
                        }
                        //ps_ed_blk_l2 ++;
                    }  //else of EIID
#endif
                }
            }
            else
            {
                /* For incomplete CTB, init valid CU to 0 */
                ps_ed_blk_l1++;
                ps_intra32_analyse->b1_valid_cu = 0;
                ps_intra16_analyse[0].b1_valid_cu = 0;
                blk_cnt++;
                merge_64x64 = 0;
            }
        } while(blk_cnt != MAX_CTB_SIZE);
        /* if 64x64 merge is possible then check for 32x32 having same best modes */
        if(1 == merge_64x64)
        {
            WORD32 act_mode = au1_best_32x32_modes[0];

            ps_ed_blk_l2 = ps_ed_l2_ctb;
            best_mode = ps_ed_blk_l2->best_mode;
            merge_64x64 =
                ((act_mode == au1_best_32x32_modes[0]) + (act_mode == au1_best_32x32_modes[1]) +
                     (act_mode == au1_best_32x32_modes[2]) +
                     (act_mode == au1_best_32x32_modes[3]) ==
                 4);
            if(merge_64x64 == 1)
                best_mode = au1_best_32x32_modes[0];
            else
                best_mode = ps_ed_blk_l2->best_mode;
            /* All 32x32 costs are accumalated to 64x64 cost */
            ps_l0_ipe_out_ctb->i4_best64x64_intra_cost = 0;
            for(i = 0; i < 4; i++)
            {
                ps_l0_ipe_out_ctb->i4_best64x64_intra_cost +=
                    ps_l0_ipe_out_ctb->ai4_best32x32_intra_cost[i];
            }

            /* If all modes of 32x32 block is not same */
            if(0 == merge_64x64)
            {
                /*Compute CHILD cost for 32x32 */
                WORD32 child_cost_64x64 = au4_best_32x32_cost[0] + au4_best_32x32_cost[1] +
                                          au4_best_32x32_cost[2] + au4_best_32x32_cost[3];
                WORD32 cost = MAX_INTRA_COST_IPE;

                WORD32 best_mode_temp = 0;
                /*Compute 64x64 cost for each mode of 32x32*/
                for(i = 0; i < 4; i++)
                {
                    WORD32 mode = au1_best_32x32_modes[i];
                    if(mode < 2)
                        mode = 26;
                    ps_cu_node->ps_parent->u1_cu_size = 64;
                    ps_cu_node->ps_parent->u2_x0 = gau1_cu_pos_x[0]; /* Populate properly */
                    ps_cu_node->ps_parent->u2_y0 = gau1_cu_pos_y[0]; /* Populate properly */

                    ihevce_set_nbr_map(
                        ps_ctxt->pu1_ctb_nbr_map,
                        ps_ctxt->i4_nbr_map_strd,
                        (ps_cu_node->ps_parent->u2_x0 << 1),
                        (ps_cu_node->ps_parent->u2_y0 << 1),
                        (ps_cu_node->ps_parent->u1_cu_size >> 2),
                        0);

                    ihevce_mode_eval_filtering(
                        ps_cu_node->ps_parent,
                        ps_cu_node,
                        ps_ctxt,
                        ps_curr_src,
                        mode,
                        &ps_cu_node->ps_parent->au4_best_cost_1tu[0],
                        &ps_cu_node->ps_parent->au1_best_mode_1tu[0],
                        !step2_bypass,
                        0);

                    parent_cost = ps_cu_node->ps_parent->best_cost;
                    if(cost > parent_cost)
                    {
                        cost = parent_cost;
                        best_mode_temp = ps_cu_node->ps_parent->best_mode;
                    }
                }
                if(cost < child_cost_64x64)
                {
                    merge_64x64 = 1;
                    best_mode = best_mode_temp;

                    /* Update 64x64 cost if CU 64x64 is chosen  */
                    ps_l0_ipe_out_ctb->i4_best64x64_intra_cost = cost;

                    /* Accumalate the least cost for CU 64x64 */
                    i8_frame_acc_satd_cost = cost;
                    i8_frame_acc_mode_bits_cost = ps_cu_node->ps_parent->u2_mode_bits_cost;

                    /* satd and mpm bits accumalation of best cu size candiate */
                    i4_ctb_acc_satd = ps_cu_node->ps_parent->best_satd;
                }
            }
        }

        if(merge_64x64)
        {
            WORD32 i, j;
            intra32_analyse_t *ps_intra32_analyse;
            intra16_analyse_t *ps_intra16_analyse;
            WORD32 row, col;
            WORD32 i4_q_scale_q3_mod;
            WORD8 i1_cu_possible_qp;
            WORD32 i4_act_factor;
            //ps_row_cu = ps_curr_cu;
            ps_ctb_out->u4_cu_split_flags = 0x0;
            ps_ed_blk_l1 = ps_ed_l1_ctb;
            ps_ed_blk_l2 = ps_ed_l2_ctb;

            ps_l0_ipe_out_ctb->u1_split_flag = 0;

            /* If CU size of 64x64 is chosen, disbale all the 16x16 flag*/
            for(i = 0; i < 4; i++)
            {
                /* get the corresponding intra 32 analyse pointer  use (blk_cnt / 16) */
                /* blk cnt is in terms of 8x8 units so a 32x32 will have 16 8x8 units */
                ps_intra32_analyse = &ps_l0_ipe_out_ctb->as_intra32_analyse[i];

                for(j = 0; j < 4; j++)
                {
                    /* get the corresponding intra 16 analyse pointer use (blk_cnt & 0xF / 4)*/
                    /* blk cnt is in terms of 8x8 units so a 16x16 will have 4 8x8 units */
                    ps_intra16_analyse = &ps_intra32_analyse->as_intra16_analyse[j];
                    ps_intra16_analyse->b1_merge_flag = 0;
                }
            }

            /* CU size 64x64 and fill the final cu params */
            //ps_row_cu->b3_cu_pos_x = gau1_cu_pos_x[0];
            //ps_row_cu->b3_cu_pos_y = gau1_cu_pos_y[0];
            //ps_row_cu->u1_cu_size  = 64;

            /* Candidate mode Update */
            cand_mode_list[0] = best_mode;
            if(cand_mode_list[0] > 1)
            {
                if(cand_mode_list[0] == 2)
                {
                    cand_mode_list[1] = 34;
                    cand_mode_list[2] = 3;
                }
                else if(cand_mode_list[0] == 34)
                {
                    cand_mode_list[1] = 2;
                    cand_mode_list[2] = 33;
                }
                else
                {
                    cand_mode_list[1] = cand_mode_list[0] - 1;
                    cand_mode_list[2] = cand_mode_list[0] + 1;
                }
                //cand_mode_list[1] = ps_ed_blk_l1->nang_attr.best_mode;
                //cand_mode_list[2] = ps_ed_blk_l1->ang_attr.best_mode;
            }
            else
            {
                cand_mode_list[0] = 0;
                cand_mode_list[1] = 1;
                cand_mode_list[2] = 26;
                //cand_mode_list[2] = ps_ed_blk_l1->nang_attr.best_mode;
            }

            /* All 32x32 costs are accumalated to 64x64 cost */
            ps_l0_ipe_out_ctb->i4_best64x64_intra_cost = 0;
            for(i = 0; i < 4; i++)
            {
                ps_l0_ipe_out_ctb->i4_best64x64_intra_cost +=
                    ps_l0_ipe_out_ctb->ai4_best32x32_intra_cost[i];
            }
            /* by default 64x64 modes are set to default values DC and Planar */
            ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[0] = cand_mode_list[0];
            ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[1] = cand_mode_list[1];
            ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[2] = cand_mode_list[2];
            ps_l0_ipe_out_ctb->au1_best_modes_32x32_tu[3] = 255;

            /* Update CTB mode map for the finalised CU */
            x = ((ps_cu_node->u2_x0 << 3) >> 2) + 1;
            y = ((ps_cu_node->u2_y0 << 3) >> 2) + 1;
            size = ps_cu_node->u1_cu_size >> 2;

            for(row = y; row < (y + size); row++)
            {
                for(col = x; col < (x + size); col++)
                {
                    ps_ctxt->au1_ctb_mode_map[row][col] = best_mode;
                }
            }

            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ps_cu_node->u2_x0 << 1),
                (ps_cu_node->u2_y0 << 1),
                (ps_cu_node->u1_cu_size >> 2),
                1);

            /*As 64*64 has won, pick L1 32x32 qp*/
            //ASSERT(((blk_cnt>>6) & 0xF) == (blk_cnt>>6));
            //ASSERT((blk_cnt>>6) == 0);
            ASSERT(ps_ed_ctb_l1->i4_32x32_satd[0][0] != -2);
            i1_cu_possible_qp = ihevce_cu_level_qp_mod(
                ps_ctxt->i4_qscale,
                ps_ed_ctb_l1->i4_32x32_satd[0][0],
                ps_ctxt->ld_curr_frame_32x32_log_avg[0],
                f_strength,
                &i4_act_factor,
                &i4_q_scale_q3_mod,
                ps_ctxt->ps_rc_quant_ctxt);

            i8_frame_acc_satd_by_modqp_q10 =
                (i8_frame_acc_satd_cost << (SATD_BY_ACT_Q_FAC + QSCALE_Q_FAC_3)) /
                i4_q_scale_q3_mod;
            /* Increment pointers */
            ps_ed_blk_l1 += 64;
            ps_ed_blk_l2 += 16;
            //ps_row_cu++;
        }
    }

    //ps_ctb_out->u1_num_cus_in_ctb = (UWORD8)(ps_row_cu - ps_curr_cu);

    {
        WORD32 i4_i, i4_j;
        WORD32 dummy;
        WORD8 i1_cu_qp;
        (void)i1_cu_qp;
        /*MAM_VAR_L1*/
        for(i4_j = 0; i4_j < 2; i4_j++)
        {
            i4_mod_factor_num = ps_ctxt->ai4_mod_factor_derived_by_variance[i4_j];
            f_strength = ps_ctxt->f_strength;

            //i4_mod_factor_num = 4;

            ps_ed_blk_l1 = ps_ed_l1_ctb;
            ps_ed_blk_l2 = ps_ed_l2_ctb;
            //ps_row_cu = ps_curr_cu;

            /*Valid only for complete CTB */
            if((64 == u1_curr_ctb_wdt) && (64 == u1_curr_ctb_hgt))
            {
                ASSERT(ps_ed_ctb_l1->i4_32x32_satd[0][0] != -2);
                ASSERT(ps_ed_ctb_l1->i4_32x32_satd[0][1] != -2);
                ASSERT(ps_ed_ctb_l1->i4_32x32_satd[0][2] != -2);
                ASSERT(ps_ed_ctb_l1->i4_32x32_satd[0][3] != -2);

                i1_cu_qp = ihevce_cu_level_qp_mod(
                    ps_ctxt->i4_qscale,
                    ps_ed_ctb_l1->i4_32x32_satd[0][0],
                    ps_ctxt->ld_curr_frame_32x32_log_avg[0],
                    f_strength,
                    &ps_l0_ipe_out_ctb->i4_64x64_act_factor[0][i4_j],
                    &dummy,
                    ps_ctxt->ps_rc_quant_ctxt);

                i1_cu_qp = ihevce_cu_level_qp_mod(
                    ps_ctxt->i4_qscale,
                    ps_ed_ctb_l1->i4_32x32_satd[0][1],
                    ps_ctxt->ld_curr_frame_32x32_log_avg[1],
                    f_strength,
                    &ps_l0_ipe_out_ctb->i4_64x64_act_factor[1][i4_j],
                    &dummy,
                    ps_ctxt->ps_rc_quant_ctxt);
                i1_cu_qp = ihevce_cu_level_qp_mod(
                    ps_ctxt->i4_qscale,
                    ps_ed_ctb_l1->i4_32x32_satd[0][2],
                    ps_ctxt->ld_curr_frame_32x32_log_avg[2],
                    f_strength,
                    &ps_l0_ipe_out_ctb->i4_64x64_act_factor[2][i4_j],
                    &dummy,
                    ps_ctxt->ps_rc_quant_ctxt);

                i1_cu_qp = ihevce_cu_level_qp_mod(
                    ps_ctxt->i4_qscale,
                    ps_ed_ctb_l1->i4_32x32_satd[0][3],
                    2.0 + ps_ctxt->ld_curr_frame_16x16_log_avg[0],
                    f_strength,
                    &ps_l0_ipe_out_ctb->i4_64x64_act_factor[3][i4_j],
                    &dummy,
                    ps_ctxt->ps_rc_quant_ctxt);

                ASSERT(ps_l0_ipe_out_ctb->i4_64x64_act_factor[3][i4_j] > 0);
            }
            else
            {
                ps_l0_ipe_out_ctb->i4_64x64_act_factor[0][i4_j] = 1024;
                ps_l0_ipe_out_ctb->i4_64x64_act_factor[1][i4_j] = 1024;
                ps_l0_ipe_out_ctb->i4_64x64_act_factor[2][i4_j] = 1024;
                ps_l0_ipe_out_ctb->i4_64x64_act_factor[3][i4_j] = 1024;
            }

            /*Store the 8x8 Qps from L2 (in raster order) as output of intra prediction
            for the usage by ME*/

            {
                WORD32 pos_x_32, pos_y_32, pos;
                //WORD32 i4_incomplete_ctb_val_8;
                pos_x_32 = u1_curr_ctb_wdt / 16;
                pos_y_32 = u1_curr_ctb_hgt / 16;

                pos = (pos_x_32 < pos_y_32) ? pos_x_32 : pos_y_32;

                for(i4_i = 0; i4_i < 4; i4_i++)
                {
                    if(i4_i < pos)
                    {
                        ASSERT(ps_ed_ctb_l1->i4_16x16_satd[i4_i][0] != -2);
                        ASSERT(ps_ed_ctb_l1->i4_16x16_satd[i4_i][1] != -2);
                        ASSERT(ps_ed_ctb_l1->i4_16x16_satd[i4_i][2] != -2);
                        i1_cu_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_16x16_satd[i4_i][0],
                            ps_ctxt->ld_curr_frame_16x16_log_avg[0],
                            f_strength,
                            &ps_l0_ipe_out_ctb->i4_32x32_act_factor[i4_i][0][i4_j],
                            &dummy,
                            ps_ctxt->ps_rc_quant_ctxt);
                        i1_cu_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_16x16_satd[i4_i][1],
                            ps_ctxt->ld_curr_frame_16x16_log_avg[1],
                            f_strength,
                            &ps_l0_ipe_out_ctb->i4_32x32_act_factor[i4_i][1][i4_j],
                            &dummy,
                            ps_ctxt->ps_rc_quant_ctxt);
                        i1_cu_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_16x16_satd[i4_i][2],
                            ps_ctxt->ld_curr_frame_16x16_log_avg[2],
                            f_strength,
                            &ps_l0_ipe_out_ctb->i4_32x32_act_factor[i4_i][2][i4_j],
                            &dummy,
                            ps_ctxt->ps_rc_quant_ctxt);
                    }
                    else
                    {
                        /*For incomplete CTB */
                        ps_l0_ipe_out_ctb->i4_32x32_act_factor[i4_i][0][i4_j] = 1024;
                        ps_l0_ipe_out_ctb->i4_32x32_act_factor[i4_i][1][i4_j] = 1024;
                        ps_l0_ipe_out_ctb->i4_32x32_act_factor[i4_i][2][i4_j] = 1024;
                    }
                }
            }

            /*Store the 8x8 Qps from L1 (in raster order) as output of intra prediction
            for the usage by ME*/
            {
                WORD32 pos_x_16, pos_y_16, pos;
                //WORD32 i4_incomplete_ctb_val_8;
                pos_x_16 = u1_curr_ctb_wdt / 4;
                pos_y_16 = u1_curr_ctb_hgt / 4;

                pos = (pos_x_16 < pos_y_16) ? pos_x_16 : pos_y_16;
                for(i4_i = 0; i4_i < 16; i4_i++)
                {
                    if(i4_i < pos)
                    {
                        ASSERT(ps_ed_ctb_l1->i4_8x8_satd[i4_i][0] != -2);
                        ASSERT(ps_ed_ctb_l1->i4_8x8_satd[i4_i][1] != -2);
                        i1_cu_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_8x8_satd[i4_i][0],
                            ps_ctxt->ld_curr_frame_8x8_log_avg[0],
                            f_strength,
                            &ps_l0_ipe_out_ctb->i4_16x16_act_factor[i4_i][0][i4_j],
                            &dummy,
                            ps_ctxt->ps_rc_quant_ctxt);
                        i1_cu_qp = ihevce_cu_level_qp_mod(
                            ps_ctxt->i4_qscale,
                            ps_ed_ctb_l1->i4_8x8_satd[i4_i][1],
                            ps_ctxt->ld_curr_frame_8x8_log_avg[1],
                            f_strength,
                            &ps_l0_ipe_out_ctb->i4_16x16_act_factor[i4_i][1][i4_j],
                            &dummy,
                            ps_ctxt->ps_rc_quant_ctxt);
                    }
                    else
                    {
                        /*For incomplete CTB */
                        ps_l0_ipe_out_ctb->i4_16x16_act_factor[i4_i][0][i4_j] = 1024;
                        ps_l0_ipe_out_ctb->i4_16x16_act_factor[i4_i][1][i4_j] = 1024;
                    }
                }
            }
        }  //for loop

        /* Accumalate the cost of ctb to the total cost */
        ps_ctxt->i8_frame_acc_satd_cost += i8_frame_acc_satd_cost;
        ps_ctxt->i8_frame_acc_satd_by_modqp_q10 += i8_frame_acc_satd_by_modqp_q10;

        ps_ctxt->i8_frame_acc_mode_bits_cost += i8_frame_acc_mode_bits_cost;

        /* satd and mpm bits accumalation of best cu size candiate for the ctb */
        ps_l0_ipe_out_ctb->i4_ctb_acc_satd = i4_ctb_acc_satd;
        ps_l0_ipe_out_ctb->i4_ctb_acc_mpm_bits = i8_frame_acc_mode_bits_cost;

        ps_ctxt->i8_frame_acc_satd += i4_ctb_acc_satd;
    }

    {
        WORD32 ctr_8x8;
        for(ctr_8x8 = 0; ctr_8x8 < (MAX_CU_IN_CTB >> 2); ctr_8x8++)
        {
            /*Accumalate activity factor for Intra and Inter*/
            if(ps_l0_ipe_out_ctb->ai4_best_sad_cost_8x8_l1_ipe[ctr_8x8] <
               ps_ed_ctb_l1->i4_sad_me_for_ref[ctr_8x8])
            {
                ps_l0_ipe_out_ctb->ai4_8x8_act_factor[ctr_8x8] =
                    ps_l0_ipe_out_ctb->i4_16x16_act_factor[ctr_8x8][1][0];
            }
            else
            {
                ps_l0_ipe_out_ctb->ai4_8x8_act_factor[ctr_8x8] =
                    ps_l0_ipe_out_ctb->i4_16x16_act_factor[ctr_8x8][1][0];
            }

            /*Accumalate activity factor at frame level*/
            ps_ctxt->i8_frame_acc_act_factor += ps_l0_ipe_out_ctb->ai4_8x8_act_factor[ctr_8x8];
        }
    }
    return;
}

WORD32 ihevce_nxn_sad_computer(
    UWORD8 *pu1_inp, WORD32 i4_inp_stride, UWORD8 *pu1_ref, WORD32 i4_ref_stride, WORD32 trans_size)
{
    WORD32 wd, ht, i, j;
    WORD32 sad = 0;

    wd = trans_size;
    ht = trans_size;

    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++)
        {
            sad += (ABS(((WORD32)pu1_inp[j] - (WORD32)pu1_ref[j])));
        }
        pu1_inp += i4_inp_stride;
        pu1_ref += i4_ref_stride;
    }

    return sad;
}

/*!
******************************************************************************
* \if Function name : ihevce_mode_eval_filtering \endif
*
* \brief
*    Evaluates best 3 modes for the given CU size with probable modes from,
*    early decision structure, mpm candidates and dc, planar mode
*
* \param[in] ps_cu_node : pointer to MAX cu node info buffer
* \param[in] ps_child_cu_node : pointer to (MAX - 1) cu node info buffer
* \param[in] ps_ctxt : pointer to IPE context struct
* \param[in] ps_curr_src : pointer to src pixels struct
* \param[in] best_amode : best angular mode from l1 layer or
                            from (MAX - 1) CU mode
* \param[in] best_costs_4x4  : pointer to 3 best cost buffer
* \param[in] best_modes_4x4  : pointer to 3 best mode buffer
* \param[in] step2_bypass : if 0, (MAX - 1) CU is evaluated
*                           if 1, (MAX CU) sugested is evaluated
* \param[in] tu_eq_cu     : indicates if tu size is same as cu or cu/2
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_mode_eval_filtering(
    ihevce_ipe_cu_tree_t *ps_cu_node,
    ihevce_ipe_cu_tree_t *ps_child_cu_node,
    ihevce_ipe_ctxt_t *ps_ctxt,
    iv_enc_yuv_buf_t *ps_curr_src,
    WORD32 best_amode,
    WORD32 *best_costs_4x4,
    UWORD8 *best_modes_4x4,
    WORD32 step2_bypass,
    WORD32 tu_eq_cu)
{
    UWORD8 *pu1_origin, *pu1_orig;
    WORD32 src_strd = ps_curr_src->i4_y_strd;
    WORD32 nbr_flags;
    nbr_avail_flags_t s_nbr;
    WORD32 trans_size = tu_eq_cu ? ps_cu_node->u1_cu_size : ps_cu_node->u1_cu_size >> 1;
    WORD32 num_tu_in_x = tu_eq_cu ? 1 : 2;
    WORD32 num_tu_in_y = tu_eq_cu ? 1 : 2;
    UWORD8 mode;

    WORD32 cost_ang_mode = MAX_INTRA_COST_IPE;
    WORD32 filter_flag;
    WORD32 cost_amode_step2[7] = { 0 };
    /*WORD32 best_sad[5];  // NOTE_A01: Not getting consumed at present */
    WORD32 sad = 0;
    WORD32 cu_pos_x, cu_pos_y;
    WORD32 temp;
    WORD32 i = 0, j, k, i_end, z;
    //WORD32 row, col, size;
    UWORD8 *pu1_ref;
    WORD32 xA, yA, xB, yB;
    WORD32 top_intra_mode;
    WORD32 left_intra_mode;
    UWORD8 *pu1_ref_orig = &ps_ctxt->au1_ref_samples[0];
    UWORD8 *pu1_ref_filt = &ps_ctxt->au1_filt_ref_samples[0];

    UWORD8 modes_4x4[5] = { 0, 1, 2, 3, 4 };
    WORD32 count;

    pf_ipe_res_trans_had apf_resd_trns_had[4];

    WORD32 cand_mode_satd_list[3];
    ihevc_intra_pred_luma_ref_substitution_ft *ihevc_intra_pred_luma_ref_substitution_fptr;

    ihevc_intra_pred_luma_ref_substitution_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_ref_substitution_fptr;

    apf_resd_trns_had[0] = ps_ctxt->s_cmn_opt_func.pf_HAD_4x4_8bit;
    apf_resd_trns_had[1] = ps_ctxt->s_cmn_opt_func.pf_HAD_8x8_8bit;
    apf_resd_trns_had[2] = ps_ctxt->s_cmn_opt_func.pf_HAD_16x16_8bit;
    apf_resd_trns_had[3] = ps_ctxt->s_cmn_opt_func.pf_HAD_32x32_8bit;

    /* initialize modes_to_eval as zero */
    memset(&ps_ctxt->au1_modes_to_eval, 0, MAX_NUM_IP_MODES);

    /* Compute the Parent Cost */

    /* Pointer to top-left of the CU - y0,x0 in 8x8 granularity */
    pu1_orig = (UWORD8 *)(ps_curr_src->pv_y_buf) + ((ps_cu_node->u2_y0 << 3) * src_strd) +
               (ps_cu_node->u2_x0 << 3);

    /* Get position of CU within CTB at 4x4 granularity */
    cu_pos_x = ps_cu_node->u2_x0 << 1;
    cu_pos_y = ps_cu_node->u2_y0 << 1;

    /* get the neighbour availability flags */
    ihevce_get_only_nbr_flag(
        &s_nbr,
        ps_ctxt->pu1_ctb_nbr_map,
        ps_ctxt->i4_nbr_map_strd,
        cu_pos_x,
        cu_pos_y,
        trans_size >> 2,
        trans_size >> 2);

    /* Traverse for all 4 child blocks in the parent block */
    xA = (ps_cu_node->u2_x0 << 3) >> 2;
    yA = ((ps_cu_node->u2_y0 << 3) >> 2) + 1;
    xB = xA + 1;
    yB = yA - 1;
    left_intra_mode = ps_ctxt->au1_ctb_mode_map[yA][xA];
    top_intra_mode = ps_ctxt->au1_ctb_mode_map[yB][xB];
    /* call the function which populates sad cost for all the modes */

    ihevce_intra_populate_mode_bits_cost_bracketing(
        top_intra_mode,
        left_intra_mode,
        s_nbr.u1_top_avail,
        s_nbr.u1_left_avail,
        ps_cu_node->u2_y0,
        &ps_ctxt->au2_mode_bits_satd_cost[0],
        &ps_ctxt->au2_mode_bits_satd[0],
        ps_ctxt->i4_ol_satd_lambda,
        cand_mode_satd_list);

    for(k = 0; k < num_tu_in_y; k++)
    {
        for(j = 0; j < num_tu_in_x; j++)
        {
            /* get the neighbour availability flags */
            nbr_flags = ihevce_get_nbr_intra(
                &s_nbr,
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x + ((j) * (trans_size >> 2)),
                cu_pos_y + ((k) * (trans_size >> 2)),
                trans_size >> 2);

            pu1_origin = pu1_orig + (k * trans_size * src_strd) + (j * trans_size);

            /* Create reference samples array */
            ihevc_intra_pred_luma_ref_substitution_fptr(
                pu1_origin - src_strd - 1,
                pu1_origin - src_strd,
                pu1_origin - 1,
                src_strd,
                trans_size,
                nbr_flags,
                pu1_ref_orig,
                0);

            /* Perform reference samples filtering */
            ihevce_intra_pred_ref_filtering(pu1_ref_orig, trans_size, pu1_ref_filt);

            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x + ((j) * (trans_size >> 2)),
                cu_pos_y + ((k) * (trans_size >> 2)),
                (trans_size >> 2),
                1);

            pu1_ref_orig += (4 * MAX_CTB_SIZE + 1);
            pu1_ref_filt += (4 * MAX_CTB_SIZE + 1);
        }
    }

    /* Revaluation for angular mode */
    //if(ps_ed_blk->ang_attr.mode_present == 1)
    //if(((best_amode & 0x1) != 1))

    {
        WORD32 u1_trans_idx = trans_size >> 3;
        if(trans_size == 32)
            u1_trans_idx = 3;
        //best_amode = ps_ed_blk->ang_attr.best_mode;

        i = 0;
        if(!step2_bypass)
        {
            /* Around best level 4 angular mode, search for best level 2 mode */
            ASSERT((best_amode >= 2) && (best_amode <= 34));

            if(ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P3)
            {
                if(best_amode >= 4)
                    ps_ctxt->au1_modes_to_eval_temp[i++] = best_amode - 2;
            }

            ps_ctxt->au1_modes_to_eval_temp[i++] = best_amode;

            if(ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P3)
            {
                if(best_amode <= 32)
                    ps_ctxt->au1_modes_to_eval_temp[i++] = best_amode + 2;
            }
        }
        else
        {
            ps_ctxt->au1_modes_to_eval_temp[i++] = ps_child_cu_node->ps_sub_cu[0]->best_mode;
            ps_ctxt->au1_modes_to_eval_temp[i++] = ps_child_cu_node->ps_sub_cu[1]->best_mode;
            ps_ctxt->au1_modes_to_eval_temp[i++] = ps_child_cu_node->ps_sub_cu[2]->best_mode;
            ps_ctxt->au1_modes_to_eval_temp[i++] = ps_child_cu_node->ps_sub_cu[3]->best_mode;
        }

        /* Add the left and top MPM modes for computation*/

        ps_ctxt->au1_modes_to_eval_temp[i++] = cand_mode_satd_list[0];
        ps_ctxt->au1_modes_to_eval_temp[i++] = cand_mode_satd_list[1];

        i_end = i;
        count = 0;

        /*Remove duplicate modes from modes_to_eval_temp[] */
        for(j = 0; j < i_end; j++)
        {
            for(k = 0; k < count; k++)
            {
                if(ps_ctxt->au1_modes_to_eval_temp[j] == ps_ctxt->au1_modes_to_eval[k])
                    break;
            }
            if((k == count) && (ps_ctxt->au1_modes_to_eval_temp[j] > 1))
            {
                ps_ctxt->au1_modes_to_eval[count] = ps_ctxt->au1_modes_to_eval_temp[j];
                count++;
            }
        }
        i_end = count;
        if(count == 0)
        {
            ps_ctxt->au1_modes_to_eval[0] = 26;
            i_end = 1;
        }

        for(i = 0; i < i_end; i++)
        {
            pu1_ref_orig = &ps_ctxt->au1_ref_samples[0];
            pu1_ref_filt = &ps_ctxt->au1_filt_ref_samples[0];

            mode = ps_ctxt->au1_modes_to_eval[i];
            ASSERT((mode >= 2) && (mode <= 34));
            cost_amode_step2[i] = ps_ctxt->au2_mode_bits_satd_cost[mode];
            filter_flag = gau1_intra_pred_ref_filter[mode] & (1 << (CTZ(trans_size) - 2));

            for(k = 0; k < num_tu_in_y; k++)
            {
                for(j = 0; j < num_tu_in_x; j++)
                {
                    pu1_origin = pu1_orig + (k * trans_size * src_strd) + (j * trans_size);

                    if(0 == filter_flag)
                        pu1_ref = pu1_ref_orig;
                    else
                        pu1_ref = pu1_ref_filt;

                    g_apf_lum_ip[g_i4_ip_funcs[mode]](
                        pu1_ref, 0, &ps_ctxt->au1_pred_samples[0], trans_size, trans_size, mode);

                    if(ps_ctxt->u1_use_satd)
                    {
                        sad = apf_resd_trns_had[u1_trans_idx](
                            pu1_origin,
                            ps_curr_src->i4_y_strd,
                            &ps_ctxt->au1_pred_samples[0],
                            trans_size,
                            NULL,
                            0

                        );
                    }
                    else
                    {
                        sad = ps_ctxt->s_ipe_optimised_function_list.pf_nxn_sad_computer(
                            pu1_origin,
                            ps_curr_src->i4_y_strd,
                            &ps_ctxt->au1_pred_samples[0],
                            trans_size,
                            trans_size);
                    }

                    cost_amode_step2[i] += sad;

                    pu1_ref_orig += (4 * MAX_CTB_SIZE + 1);
                    pu1_ref_filt += (4 * MAX_CTB_SIZE + 1);
                }
            }
        }
        best_amode = ps_ctxt->au1_modes_to_eval[0];
        /*Init cost indx */
        cost_ang_mode = MAX_INTRA_COST_IPE;  //cost_amode_step2[0];
        for(z = 0; z < i_end; z++)
        {
            /* Least cost of all 3 angles are stored in cost_amode_step2[0] and corr. mode*/
            if(cost_ang_mode >= cost_amode_step2[z])
            {
                if(cost_ang_mode == cost_amode_step2[z])
                {
                    if(best_amode > ps_ctxt->au1_modes_to_eval[z])
                        best_amode = ps_ctxt->au1_modes_to_eval[z];
                }
                else
                {
                    best_amode = ps_ctxt->au1_modes_to_eval[z];
                }
                cost_ang_mode = cost_amode_step2[z];
            }
        }

        /*Modify mode bits for the angular modes */
    }

    {
        /* Step - I modification */
        ASSERT((best_amode >= 2) && (best_amode <= 34));
        i_end = 0;
        z = 0;

        /* Around best level 3 angular mode, search for best level 1 mode */
        ps_ctxt->au1_modes_to_eval[i_end++] = 0;
        ps_ctxt->au1_modes_to_eval[i_end++] = 1;

        if(best_amode != 2)
            ps_ctxt->au1_modes_to_eval[i_end++] = best_amode - 1;

        ps_ctxt->au1_modes_to_eval[i_end++] = best_amode;

        if(best_amode != 34)
            ps_ctxt->au1_modes_to_eval[i_end++] = best_amode + 1;

        /* Inserting step_2's best mode at last to avoid
        recalculation of it's SATD cost */

        //ps_ctxt->au1_modes_to_eval[i_end] = best_amode; //Bugfix: HSAD compared with SAD
        //cost_amode_step2[i_end] = cost_ang_mode;

        /*best_sad[i_end] = cost_ang_mode
                - mode_bits_satd_cost[best_amode]; //See NOTE_A01 above */

        cost_ang_mode = MAX_INTRA_COST_IPE; /* Init cost */

        for(i = 0; i < i_end; i++)
        {
            WORD32 u1_trans_idx = trans_size >> 3;
            if(trans_size == 32)
                u1_trans_idx = 3;
            pu1_ref_orig = &ps_ctxt->au1_ref_samples[0];
            pu1_ref_filt = &ps_ctxt->au1_filt_ref_samples[0];

            /*best_sad[i] = 0; //See NOTE_A01 above */
            mode = ps_ctxt->au1_modes_to_eval[i];
            cost_amode_step2[i] = ps_ctxt->au2_mode_bits_satd_cost[mode];
            filter_flag = gau1_intra_pred_ref_filter[mode] & (1 << (CTZ(trans_size) - 2));

            for(k = 0; k < num_tu_in_y; k++)
            {
                for(j = 0; j < num_tu_in_x; j++)
                {
                    pu1_origin = pu1_orig + (k * trans_size * src_strd) + (j * trans_size);

                    if(0 == filter_flag)
                        pu1_ref = pu1_ref_orig;
                    else
                        pu1_ref = pu1_ref_filt;

                    g_apf_lum_ip[g_i4_ip_funcs[mode]](
                        pu1_ref, 0, &ps_ctxt->au1_pred_samples[0], trans_size, trans_size, mode);

                    //if(trans_size != 4)
                    {
                        sad = apf_resd_trns_had[u1_trans_idx](
                            pu1_origin,
                            ps_curr_src->i4_y_strd,
                            &ps_ctxt->au1_pred_samples[0],
                            trans_size,
                            NULL,
                            0);
                    }

                    /*accumualting SATD though name says it is sad*/
                    cost_amode_step2[i] += sad;
                    /*best_sad[i] +=sad; //See NOTE_A01 above */
                    pu1_ref_orig += (4 * MAX_CTB_SIZE + 1);
                    pu1_ref_filt += (4 * MAX_CTB_SIZE + 1);
                }
            }
        }
        /* Updating i_end for the step_2's inserted mode*/
        //        i_end++;

        /* Arrange the reference array in ascending order */

        for(i = 0; i < (i_end - 1); i++)
        {
            for(j = i + 1; j < i_end; j++)
            {
                if(cost_amode_step2[i] > cost_amode_step2[j])
                {
                    temp = cost_amode_step2[i];
                    cost_amode_step2[i] = cost_amode_step2[j];
                    cost_amode_step2[j] = temp;

                    temp = modes_4x4[i];
                    modes_4x4[i] = modes_4x4[j];
                    modes_4x4[j] = temp;
                }
            }
        }

        /* Least cost of all 3 angles are stored in cost_amode_step2[0] and corr. mode*/
        best_amode = ps_ctxt->au1_modes_to_eval[modes_4x4[0]];
        cost_ang_mode = cost_amode_step2[0];
        ps_cu_node->best_satd = cost_ang_mode - ps_ctxt->au2_mode_bits_satd_cost[best_amode];
        ps_cu_node->best_cost = cost_amode_step2[0];
        ps_cu_node->best_mode = ps_ctxt->au1_modes_to_eval[modes_4x4[0]];
        ps_cu_node->best_satd =
            ps_cu_node->best_cost - ps_ctxt->au2_mode_bits_satd_cost[ps_cu_node->best_mode];

        /*Accumalate best mode bits cost for RC*/
        ps_cu_node->u2_mode_bits_cost = ps_ctxt->au2_mode_bits_satd[ps_cu_node->best_mode];

        /* Store the best three candidates */
        for(i = 0; i < 3; i++)
        {
            best_costs_4x4[i] = cost_amode_step2[i];
            best_modes_4x4[i] = ps_ctxt->au1_modes_to_eval[modes_4x4[i]];
        }
    }

    return;
}
