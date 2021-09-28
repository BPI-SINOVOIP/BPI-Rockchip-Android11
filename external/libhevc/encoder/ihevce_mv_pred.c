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
 *  ihevcd_mv_pred.c
 *
 * @brief
 *  Contains functions for motion vector prediction
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 * - ihevcd_mvp_spatial_cand()
 *
 * @remarks
 *  None
 *
 *******************************************************************************
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
#include "ihevc_common_tables.h"

#include "ihevce_defs.h"
#include "ihevce_hle_interface.h"
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
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_mv_pred.h"
#include "ihevce_mv_pred_merge.h"
#include "ihevce_common_utils.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
 *******************************************************************************
 *
 * @brief
 * This function performs Motion Vector prediction and return a list of mv
 *
 * @par Description:
 *  MV predictor list is computed using neighbor mvs and colocated mv
 *
 * @param[in] ps_ctxt
 * pointer to mv predictor context
 *
 * @param[in] ps_top_nbr_4x4
 * pointer to top 4x4 nbr structure
 *
 * @param[in] ps_left_nbr_4x4
 * pointer to left 4x4 nbr structure
 *
 * @param[in] ps_top_left_nbr_4x4
 * pointer to top left 4x4 nbr structure
 *
 * @param[in] left_nbr_4x4_strd
 * left nbr buffer stride in terms of 4x4 units
 *
 * @param[in] ps_avail_flags
 * Neighbor availability flags container
 *
 * @param[in] ps_col_mv
 * Colocated MV pointer
 *
 * @param[in] ps_pu
 * Current Partition PU strucrture pointer
 *
 * @param[inout] ps_pred_mv
 * pointer to store predicted MV list
 *
 * @returns
 * None
 * @remarks
 *
 *
 *******************************************************************************
 */
void ihevce_mv_pred(
    mv_pred_ctxt_t *ps_ctxt,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_left_nbr_4x4,
    WORD32 left_nbr_4x4_strd,
    nbr_avail_flags_t *ps_avail_flags,
    pu_mv_t *ps_col_mv,
    pu_t *ps_pu,
    pu_mv_t *ps_pred_mv,
    UWORD8 (*pau1_is_top_used)[MAX_MVP_LIST_CAND])
{
    WORD32 is_scaled_flag_list[2] /* Indicates whether A0 or A1 is available */;
    WORD32 lb_avail, l_avail, t_avail, tr_avail, tl_avail;
    WORD32 avail_a_flag[2];
    WORD32 avail_b_flag[2];
    mv_t as_mv_a[2];
    mv_t as_mv_b[2];
    UWORD8 i1_cur_ref_idx_list[2];
    WORD32 part_pos_x;
    WORD32 part_pos_y;
    WORD32 part_wd;
    WORD32 part_ht;

    /*******************************************/
    /* Neighbor location: Graphical indication */
    /*                                         */
    /*          B2 _____________B1 B0          */
    /*            |               |            */
    /*            |               |            */
    /*            |               |            */
    /*            |      PU     ht|            */
    /*            |               |            */
    /*            |               |            */
    /*          A1|______wd_______|            */
    /*          A0                             */
    /*                                         */
    /*******************************************/

    /* Initialization */
    avail_a_flag[0] = 0;
    avail_a_flag[1] = 0;
    avail_b_flag[0] = 0;
    avail_b_flag[1] = 0;

    as_mv_a[0].i2_mvx = 0;
    as_mv_a[0].i2_mvy = 0;
    as_mv_a[1].i2_mvx = 0;
    as_mv_a[1].i2_mvy = 0;
    as_mv_b[0].i2_mvx = 0;
    as_mv_b[0].i2_mvy = 0;
    as_mv_b[1].i2_mvx = 0;
    as_mv_b[1].i2_mvy = 0;

    lb_avail = ps_avail_flags->u1_bot_lt_avail;
    l_avail = ps_avail_flags->u1_left_avail;
    tr_avail = ps_avail_flags->u1_top_rt_avail;
    t_avail = ps_avail_flags->u1_top_avail;
    tl_avail = ps_avail_flags->u1_top_lt_avail;

    is_scaled_flag_list[0] = 0;
    is_scaled_flag_list[1] = 0;

    part_pos_x = ps_pu->b4_pos_x << 2;
    part_pos_y = ps_pu->b4_pos_y << 2;
    part_wd = (ps_pu->b4_wd + 1) << 2;
    part_ht = (ps_pu->b4_ht + 1) << 2;

    /* Initializing current PU reference index     */
    /* if -1 is set then that direction is invalid */
    i1_cur_ref_idx_list[0] = (-1 == ps_pu->mv.i1_l0_ref_idx) ? 0 : ps_pu->mv.i1_l0_ref_idx;
    i1_cur_ref_idx_list[1] = (-1 == ps_pu->mv.i1_l1_ref_idx) ? 0 : ps_pu->mv.i1_l1_ref_idx;

    /************************************************************/
    /* Calculating of motion vector A from neighbors A0 and A1  */
    /************************************************************/
    {
        WORD32 l_x, a;
        WORD32 *pi4_avail_flag;
        WORD32 nbr_avail[2]; /*[A0/A1] */
        WORD8 i1_nbr_ref_idx_list[2][2]; /* [A0/A1][L0/L1] */
        UWORD8 u1_nbr_intra_flag[2]; /*[A0/A1] */
        UWORD8 u1_nbr_pred_flag[2][2]; /* [A0/A1][L0/L1] */
        mv_t *ps_mv;
        nbr_4x4_t *ps_a0, *ps_a1;
        mv_t *ps_nbr_mv[2][2]; /* [A0/A1][L0/L1] */

        /* A0 and A1 initializations */
        ps_mv = &as_mv_a[0];
        pi4_avail_flag = avail_a_flag;

        /* Pointers to A0 and A1 */
        {
            WORD32 y_a0, y_a1;
            /* TODO: y_a0, y_a1 is coded assuming left nbr pointer starts at PU */
            y_a0 = (part_ht >> 2);
            y_a1 = ((part_ht - 1) >> 2);

            ps_a0 = ps_left_nbr_4x4 + (y_a0 * left_nbr_4x4_strd);
            ps_a1 = ps_left_nbr_4x4 + (y_a1 * left_nbr_4x4_strd);
        }

        nbr_avail[0] = lb_avail && (!ps_a0->b1_intra_flag);
        nbr_avail[1] = l_avail && (!ps_a1->b1_intra_flag);

        /* Setting is scaled flag based on availability of A0 and A1 */
        if((nbr_avail[0] == 1) || (nbr_avail[1]))
        {
            is_scaled_flag_list[0] = 1;
            is_scaled_flag_list[1] = 1;
        }

        /* Initializing A0 variables */
        ps_nbr_mv[0][0] = &ps_a0->mv.s_l0_mv;
        ps_nbr_mv[0][1] = &ps_a0->mv.s_l1_mv;

        i1_nbr_ref_idx_list[0][0] = ps_a0->mv.i1_l0_ref_idx;
        i1_nbr_ref_idx_list[0][1] = ps_a0->mv.i1_l1_ref_idx;

        u1_nbr_pred_flag[0][0] = (UWORD8)ps_a0->b1_pred_l0_flag;
        u1_nbr_pred_flag[0][1] = (UWORD8)ps_a0->b1_pred_l1_flag;

        u1_nbr_intra_flag[0] = (UWORD8)ps_a0->b1_intra_flag;

        /* Initializing A1 variables */
        ps_nbr_mv[1][0] = &ps_a1->mv.s_l0_mv;
        ps_nbr_mv[1][1] = &ps_a1->mv.s_l1_mv;

        i1_nbr_ref_idx_list[1][0] = ps_a1->mv.i1_l0_ref_idx;
        i1_nbr_ref_idx_list[1][1] = ps_a1->mv.i1_l1_ref_idx;

        u1_nbr_pred_flag[1][0] = (UWORD8)ps_a1->b1_pred_l0_flag;
        u1_nbr_pred_flag[1][1] = (UWORD8)ps_a1->b1_pred_l1_flag;

        u1_nbr_intra_flag[1] = (UWORD8)ps_a1->b1_intra_flag;

        /* Derivation of mvL0A and mvL1A from A0 and A1 */
        for(l_x = 0; l_x < 2; l_x++) /* list 0 and list 1 */
        {
            WORD32 l_y;

            l_y = !l_x; /* if i=0, y = L1 else y = L0 */

            for(a = 0; a < 2; a++)
            {
                /* MODE_INTRA check has been taken care in availability check */
                if((nbr_avail[a] == 1) && (pi4_avail_flag[l_x] == 0))
                {
                    if(u1_nbr_pred_flag[a][l_x] == 1)
                    {
                        WORD32 nbr_ref_poc, cur_ref_poc;
                        WORD8 i1_cur_ref_idx, i1_nbr_ref_idx;

                        i1_cur_ref_idx = i1_cur_ref_idx_list[l_x];
                        cur_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_cur_ref_idx]->i4_poc;
                        i1_nbr_ref_idx = i1_nbr_ref_idx_list[a][l_x];
                        nbr_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_nbr_ref_idx]->i4_poc;

                        if(nbr_ref_poc == cur_ref_poc)
                        {
                            pi4_avail_flag[l_x] = 1;
                            ps_mv[l_x] = *ps_nbr_mv[a][l_x];
                            break;
                        }
                    }
                    if(u1_nbr_pred_flag[a][l_y] == 1)
                    {
                        WORD32 nbr_ref_poc, cur_ref_poc;
                        WORD8 i1_nbr_ref_idx, i1_cur_ref_idx;

                        i1_cur_ref_idx = i1_cur_ref_idx_list[l_x];
                        cur_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_cur_ref_idx]->i4_poc;

                        i1_nbr_ref_idx = i1_nbr_ref_idx_list[a][l_y];
                        nbr_ref_poc = ps_ctxt->ps_ref_list[l_y][i1_nbr_ref_idx]->i4_poc;
                        if(nbr_ref_poc == cur_ref_poc)
                        {
                            pi4_avail_flag[l_x] = 1;
                            ps_mv[l_x] = *ps_nbr_mv[a][l_y];
                            break;
                        }
                    }
                }
            }
        }

        for(l_x = 0; l_x < 2; l_x++) /* list 0 and list 1 */
        {
            if(pi4_avail_flag[l_x] == 0)
            {
                WORD8 i1_nbr_ref_list_idx, i1_nbr_ref_idx;
                WORD32 l_y;

                l_y = !l_x; /* if i=0, y = L1 else y = L0 */

                for(a = 0; a < 2; a++)
                {
                    /* MODE_INTRA check has been taken care in availability check */
                    if((nbr_avail[a] == 1) && (pi4_avail_flag[l_x] == 0))
                    {
                        /* Long term reference check Removed */
                        if(u1_nbr_pred_flag[a][l_x] == 1)
                        {
                            pi4_avail_flag[l_x] = 1;
                            ps_mv[l_x] = *ps_nbr_mv[a][l_x];
                            i1_nbr_ref_idx = i1_nbr_ref_idx_list[a][l_x];
                            i1_nbr_ref_list_idx = l_x;
                            break;
                        }
                        /* Long term reference check Removed */
                        else if(u1_nbr_pred_flag[a][l_y] == 1)
                        {
                            pi4_avail_flag[l_x] = 1;
                            ps_mv[l_x] = *ps_nbr_mv[a][l_y];
                            i1_nbr_ref_idx = i1_nbr_ref_idx_list[a][l_y];
                            i1_nbr_ref_list_idx = l_y;
                            break;
                        }
                    }
                }

                /* Long term reference check Removed */
                if(pi4_avail_flag[l_x] == 1)
                {
                    WORD8 i1_cur_ref_idx;
                    WORD32 cur_ref_poc, nbr_ref_poc;
                    WORD32 cur_poc;

                    i1_cur_ref_idx = i1_cur_ref_idx_list[l_x];
                    cur_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_cur_ref_idx]->i4_poc;

                    nbr_ref_poc = ps_ctxt->ps_ref_list[i1_nbr_ref_list_idx][i1_nbr_ref_idx]->i4_poc;

                    cur_poc = ps_ctxt->ps_slice_hdr->i4_abs_pic_order_cnt;

                    ihevce_scale_mv(&ps_mv[l_x], cur_ref_poc, nbr_ref_poc, cur_poc);
                }
            }
        }
    }

    /************************************************************/
    /* Calculating of motion vector B from neighbors B0 and B1  */
    /************************************************************/
    {
        WORD32 l_x, b;
        WORD32 *pi4_avail_flag;
        WORD32 nbr_avail[3]; /* [B0/B1/B2] */
        WORD8 i1_nbr_ref_idx_list[3][2]; /* [B0/B1/B2][L0/L1] */
        UWORD8 u1_nbr_intra_flag[3]; /*[B0/B1/B2] */
        UWORD8 u1_nbr_pred_flag[3][2]; /* [B0/B1/B2][L0/L1] */
        mv_t *ps_mv;
        nbr_4x4_t *ps_b0, *ps_b1, *ps_b2;
        mv_t *ps_nbr_mv[3][2]; /* [B0/B1/B2][L0/L1] */

        /* B0, B1 and B2 initializations */
        ps_mv = &as_mv_b[0];
        pi4_avail_flag = avail_b_flag;

        /* Pointers to B0, B1 and B2 */
        {
            WORD32 x_b0, x_b1, x_b2;

            /* Relative co-ordiante of Xp,Yp w.r.t CTB start will work */
            /* as long as minCTB = 16                                  */
            x_b0 = (part_pos_x + part_wd);
            x_b1 = (part_pos_x + part_wd - 1);
            x_b2 = (part_pos_x - 1);

            /* Getting offset back to given pointer */
            x_b0 = x_b0 - part_pos_x;
            x_b1 = x_b1 - part_pos_x;
            x_b2 = x_b2 - part_pos_x;

            /* Below derivation are based on top pointer */
            /* is pointing first pixel of PU             */
            ps_b0 = ps_top_nbr_4x4 + (x_b0 >> 2);
            ps_b1 = ps_top_nbr_4x4 + (x_b1 >> 2);

            /* At CTB boundary, use top-left passed in */
            if(part_pos_y)
            {
                ps_b2 = ps_top_left_nbr_4x4;
            }
            else
            {
                /* Not at CTB boundary, use top and  */
                /* add correction to go to top-left */
                ps_b2 = (ps_top_nbr_4x4) + (x_b2 >> 2);
            }
        }
        nbr_avail[0] = tr_avail && (!ps_b0->b1_intra_flag);
        nbr_avail[1] = t_avail && (!ps_b1->b1_intra_flag);
        nbr_avail[2] = tl_avail && (!ps_b2->b1_intra_flag);

        /* Initializing B0 related variables */
        ps_nbr_mv[0][0] = &ps_b0->mv.s_l0_mv;
        ps_nbr_mv[0][1] = &ps_b0->mv.s_l1_mv;

        i1_nbr_ref_idx_list[0][0] = ps_b0->mv.i1_l0_ref_idx;
        i1_nbr_ref_idx_list[0][1] = ps_b0->mv.i1_l1_ref_idx;

        u1_nbr_pred_flag[0][0] = (UWORD8)ps_b0->b1_pred_l0_flag;
        u1_nbr_pred_flag[0][1] = (UWORD8)ps_b0->b1_pred_l1_flag;

        u1_nbr_intra_flag[0] = (UWORD8)ps_b0->b1_intra_flag;

        /* Initializing B1 related variables */
        ps_nbr_mv[1][0] = &ps_b1->mv.s_l0_mv;
        ps_nbr_mv[1][1] = &ps_b1->mv.s_l1_mv;

        i1_nbr_ref_idx_list[1][0] = ps_b1->mv.i1_l0_ref_idx;
        i1_nbr_ref_idx_list[1][1] = ps_b1->mv.i1_l1_ref_idx;

        u1_nbr_pred_flag[1][0] = (UWORD8)ps_b1->b1_pred_l0_flag;
        u1_nbr_pred_flag[1][1] = (UWORD8)ps_b1->b1_pred_l1_flag;

        u1_nbr_intra_flag[1] = (UWORD8)ps_b1->b1_intra_flag;

        /* Initializing B2 related variables */
        ps_nbr_mv[2][0] = &ps_b2->mv.s_l0_mv;
        ps_nbr_mv[2][1] = &ps_b2->mv.s_l1_mv;

        i1_nbr_ref_idx_list[2][0] = ps_b2->mv.i1_l0_ref_idx;
        i1_nbr_ref_idx_list[2][1] = ps_b2->mv.i1_l1_ref_idx;

        u1_nbr_pred_flag[2][0] = (UWORD8)ps_b2->b1_pred_l0_flag;
        u1_nbr_pred_flag[2][1] = (UWORD8)ps_b2->b1_pred_l1_flag;

        u1_nbr_intra_flag[2] = (UWORD8)ps_b2->b1_intra_flag;

        /* Derivation of mvL0B and mvL1B from B0,B1 and B2 */
        for(l_x = 0; l_x < 2; l_x++) /* list 0 and list 1 */
        {
            WORD32 l_y;

            l_y = !l_x; /* if i=0, y = L1 else y = L0 */

            for(b = 0; b < 3; b++)
            {
                if((nbr_avail[b] == 1) && (pi4_avail_flag[l_x] == 0))
                {
                    if(u1_nbr_pred_flag[b][l_x] == 1)
                    {
                        WORD32 nbr_ref_poc, cur_ref_poc;
                        WORD8 i1_cur_ref_idx, i1_nbr_ref_idx;

                        i1_cur_ref_idx = i1_cur_ref_idx_list[l_x];
                        cur_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_cur_ref_idx]->i4_poc;
                        i1_nbr_ref_idx = i1_nbr_ref_idx_list[b][l_x];
                        nbr_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_nbr_ref_idx]->i4_poc;

                        if(nbr_ref_poc == cur_ref_poc)
                        {
                            pi4_avail_flag[l_x] = 1;
                            ps_mv[l_x] = *ps_nbr_mv[b][l_x];
                            break;
                        }
                    }
                    if(u1_nbr_pred_flag[b][l_y] == 1)
                    {
                        WORD32 nbr_ref_poc, cur_ref_poc;
                        WORD8 i1_nbr_ref_idx, i1_cur_ref_idx;

                        i1_cur_ref_idx = i1_cur_ref_idx_list[l_x];
                        cur_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_cur_ref_idx]->i4_poc;

                        i1_nbr_ref_idx = i1_nbr_ref_idx_list[b][l_y];
                        nbr_ref_poc = ps_ctxt->ps_ref_list[l_y][i1_nbr_ref_idx]->i4_poc;

                        if(nbr_ref_poc == cur_ref_poc)
                        {
                            pi4_avail_flag[l_x] = 1;
                            ps_mv[l_x] = *ps_nbr_mv[b][l_y];
                            break;
                        }
                    }
                }
            }
        }

        if((is_scaled_flag_list[0] == 0) && (avail_b_flag[0] == 1))
        {
            avail_a_flag[0] = 1;
            as_mv_a[0] = as_mv_b[0];
        }
        if((is_scaled_flag_list[1] == 0) && (avail_b_flag[1] == 1))
        {
            avail_a_flag[1] = 1;
            as_mv_a[1] = as_mv_b[1];
        }

        for(l_x = 0; l_x < 2; l_x++) /* list 0 and list 1 */
        {
            if(is_scaled_flag_list[l_x] == 0)
            {
                /* If isScaledFlagLX == 0, availFlagLXB flag is set to 0 */
                pi4_avail_flag[l_x] = 0;
                {
                    WORD8 i1_nbr_ref_list_idx, i1_nbr_ref_idx;
                    WORD32 l_y;

                    l_y = !l_x; /* if i=0, y = L1 else y = L0 */

                    for(b = 0; b < 3; b++)
                    {
                        if((nbr_avail[b] == 1) && (pi4_avail_flag[l_x] == 0))
                        {
                            /* Long term reference check Removed */
                            if(u1_nbr_pred_flag[b][l_x] == 1)
                            {
                                pi4_avail_flag[l_x] = 1;
                                ps_mv[l_x] = *ps_nbr_mv[b][l_x];
                                i1_nbr_ref_idx = i1_nbr_ref_idx_list[b][l_x];
                                i1_nbr_ref_list_idx = l_x;
                                break;
                            }
                            /* Long term reference check Removed */
                            else if(u1_nbr_pred_flag[b][l_y] == 1)
                            {
                                pi4_avail_flag[l_x] = 1;
                                ps_mv[l_x] = *ps_nbr_mv[b][l_y];
                                i1_nbr_ref_idx = i1_nbr_ref_idx_list[b][l_y];
                                i1_nbr_ref_list_idx = l_y;
                                break;
                            }
                        }
                    }
                    /* Long term reference check Removed */
                    if(pi4_avail_flag[l_x] == 1)
                    {
                        WORD8 i1_cur_ref_idx;
                        WORD32 cur_ref_poc, nbr_ref_poc;
                        WORD32 cur_poc;

                        i1_cur_ref_idx = i1_cur_ref_idx_list[l_x];
                        cur_ref_poc = ps_ctxt->ps_ref_list[l_x][i1_cur_ref_idx]->i4_poc;

                        nbr_ref_poc =
                            ps_ctxt->ps_ref_list[i1_nbr_ref_list_idx][i1_nbr_ref_idx]->i4_poc;

                        cur_poc = ps_ctxt->ps_slice_hdr->i4_abs_pic_order_cnt;

                        if(cur_ref_poc != nbr_ref_poc)
                            ihevce_scale_mv(&ps_mv[l_x], cur_ref_poc, nbr_ref_poc, cur_poc);
                    }
                }
            }
        }
    }

    /* Candidate list */
    {
        mv_t as_mvp_list_l0[MAX_MVP_LIST_CAND_MEM]; /*[Cand0/Cand1/Cand2] */
        mv_t as_mvp_list_l1[MAX_MVP_LIST_CAND_MEM]; /*[Cand0/Cand1/Cand2] */
        UWORD8 au1_is_top_used_l0[MAX_MVP_LIST_CAND_MEM];
        UWORD8 au1_is_top_used_l1[MAX_MVP_LIST_CAND_MEM];
        WORD32 num_mvp_cand_l0;
        WORD32 num_mvp_cand_l1;

        /* L0 candidate list*/
        num_mvp_cand_l0 = 0;

        if(avail_a_flag[0] == 1)
        {
            as_mvp_list_l0[num_mvp_cand_l0] = as_mv_a[0];
            au1_is_top_used_l0[num_mvp_cand_l0] = (is_scaled_flag_list[0] == 0);
            num_mvp_cand_l0++;
        }
        if(avail_b_flag[0] == 1)
        {
            if(((as_mv_a[0].i2_mvx != as_mv_b[0].i2_mvx) ||
                (as_mv_a[0].i2_mvy != as_mv_b[0].i2_mvy)) ||
               (0 == num_mvp_cand_l0))
            {
                as_mvp_list_l0[num_mvp_cand_l0] = as_mv_b[0];
                au1_is_top_used_l0[num_mvp_cand_l0] = 1;
                num_mvp_cand_l0++;
            }
        }

        /* L1 candidate list*/
        num_mvp_cand_l1 = 0;

        if(avail_a_flag[1] == 1)
        {
            as_mvp_list_l1[num_mvp_cand_l1] = as_mv_a[1];
            au1_is_top_used_l1[num_mvp_cand_l1] = (is_scaled_flag_list[1] == 0);
            num_mvp_cand_l1++;
        }
        if(avail_b_flag[1] == 1)
        {
            if(((as_mv_a[1].i2_mvx != as_mv_b[1].i2_mvx) ||
                (as_mv_a[1].i2_mvy != as_mv_b[1].i2_mvy)) ||
               (0 == num_mvp_cand_l1))
            {
                as_mvp_list_l1[num_mvp_cand_l1] = as_mv_b[1];
                au1_is_top_used_l1[num_mvp_cand_l1] = 1;
                num_mvp_cand_l1++;
            }
        }

        /***********************************************************/
        /*          Collocated MV prediction                       */
        /***********************************************************/
        if((MAX_MVP_LIST_CAND > num_mvp_cand_l0) || (MAX_MVP_LIST_CAND > num_mvp_cand_l1))
        {
            mv_t as_mv_col[2], s_mv_col_l0, s_mv_col_l1;
            WORD32 avail_col_flag[2] = { 0 };
            WORD32 x_col, y_col, avail_col_l0, avail_col_l1;

            x_col = part_pos_x + part_wd;
            y_col = part_pos_y + part_ht;
            ihevce_collocated_mvp(ps_ctxt, ps_pu, as_mv_col, avail_col_flag, 1, x_col, y_col);

            avail_col_l0 = avail_col_flag[0];
            avail_col_l1 = avail_col_flag[1];
            if(avail_col_l0 || avail_col_l1)
            {
                s_mv_col_l0 = as_mv_col[0];
                s_mv_col_l1 = as_mv_col[1];
            }

            if(avail_col_l0 == 0 || avail_col_l1 == 0)
            {
                /* Checking Collocated MV availability at Center of PU */
                x_col = part_pos_x + (part_wd >> 1);
                y_col = part_pos_y + (part_ht >> 1);
                ihevce_collocated_mvp(ps_ctxt, ps_pu, as_mv_col, avail_col_flag, 1, x_col, y_col);

                if(avail_col_l0 == 0)
                {
                    s_mv_col_l0 = as_mv_col[0];
                }
                if(avail_col_l1 == 0)
                {
                    s_mv_col_l1 = as_mv_col[1];
                }

                avail_col_l0 |= avail_col_flag[0];
                avail_col_l1 |= avail_col_flag[1];
            }

            /* Checking if mvp index matches collocated mv */
            if(avail_col_l0)
            {
                if(MAX_MVP_LIST_CAND > num_mvp_cand_l0)
                {
                    as_mvp_list_l0[num_mvp_cand_l0] = s_mv_col_l0;
                    au1_is_top_used_l0[num_mvp_cand_l0] = 0;
                    num_mvp_cand_l0++;
                }
            }
            if(avail_col_l1)
            {
                if(MAX_MVP_LIST_CAND > num_mvp_cand_l1)
                {
                    as_mvp_list_l1[num_mvp_cand_l1] = s_mv_col_l1;
                    au1_is_top_used_l1[num_mvp_cand_l1] = 0;
                    num_mvp_cand_l1++;
                }
            }
        }

        /* Adding zero if mv candidates are less than 2 */
        while(num_mvp_cand_l0 < MAX_MVP_LIST_CAND)
        {
            as_mvp_list_l0[num_mvp_cand_l0].i2_mvx = 0;
            as_mvp_list_l0[num_mvp_cand_l0].i2_mvy = 0;
            au1_is_top_used_l0[num_mvp_cand_l0] = 0;
            num_mvp_cand_l0++;
        };
        while(num_mvp_cand_l1 < MAX_MVP_LIST_CAND)
        {
            as_mvp_list_l1[num_mvp_cand_l1].i2_mvx = 0;
            as_mvp_list_l1[num_mvp_cand_l1].i2_mvy = 0;
            au1_is_top_used_l1[num_mvp_cand_l1] = 0;
            num_mvp_cand_l1++;
        };
        /* Removing mvs if candidates are greater than 2 */
        if(num_mvp_cand_l0 > MAX_MVP_LIST_CAND)
        {
            num_mvp_cand_l0 = MAX_MVP_LIST_CAND;
        };
        if(num_mvp_cand_l1 > MAX_MVP_LIST_CAND)
        {
            num_mvp_cand_l1 = MAX_MVP_LIST_CAND;
        };

        /* Copying list to output */
        {
            WORD32 i;
            for(i = 0; i < num_mvp_cand_l0; i++)
            {
                ps_pred_mv[i].s_l0_mv = as_mvp_list_l0[i];
                pau1_is_top_used[0][i] = au1_is_top_used_l0[i];
            }

            for(i = 0; i < num_mvp_cand_l1; i++)
            {
                ps_pred_mv[i].s_l1_mv = as_mvp_list_l1[i];
                pau1_is_top_used[1][i] = au1_is_top_used_l1[i];
            }
        }
    }
}
