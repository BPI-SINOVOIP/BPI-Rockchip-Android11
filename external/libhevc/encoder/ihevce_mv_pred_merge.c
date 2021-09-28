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
 *  ihevcd_mv_pred_merge.c
 *
 * @brief
 *  Contains functions for motion vector merge candidates derivation
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 * - ihevce_compare_pu_mv_t()
 * - ihevce_mv_pred_merge()
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
 * @brief Function scaling temporal motion vector
 *
 *
 * @par Description:
 *   Scales mv based on difference between current POC and current
 *   reference POC and neighbour reference poc
 *
 * @param[inout] mv
 *   motion vector to be scaled
 *
 * @param[in] cur_ref_poc
 *   Current PU refernce pic poc
 *
 * @param[in] nbr_ref_poc
 *   Neighbor PU reference pic poc
 *
 * @param[in] cur_poc
 *   Picture order count of current pic
 *
 * @returns
 *  None
 *
 * @remarks
 *
 *******************************************************************************
 */
void ihevce_scale_collocated_mv(
    mv_t *ps_mv, WORD32 cur_ref_poc, WORD32 col_ref_poc, WORD32 col_poc, WORD32 cur_poc)
{
    WORD32 td, tb, tx;
    WORD32 dist_scale_factor;
    WORD32 mvx, mvy;

    td = CLIP_S8(col_poc - col_ref_poc);
    tb = CLIP_S8(cur_poc - cur_ref_poc);

    tx = (16384 + (abs(td) >> 1)) / td;

    dist_scale_factor = (tb * tx + 32) >> 6;
    dist_scale_factor = CLIP3(dist_scale_factor, -4096, 4095);

    mvx = ps_mv->i2_mvx;
    mvy = ps_mv->i2_mvy;

    mvx = SIGN(dist_scale_factor * mvx) * ((abs(dist_scale_factor * mvx) + 127) >> 8);
    mvy = SIGN(dist_scale_factor * mvy) * ((abs(dist_scale_factor * mvy) + 127) >> 8);

    ps_mv->i2_mvx = CLIP_S16(mvx);
    ps_mv->i2_mvy = CLIP_S16(mvy);

} /* End of ihevce_scale_collocated_mv */

void ihevce_collocated_mvp(
    mv_pred_ctxt_t *ps_mv_ctxt,
    pu_t *ps_pu,
    mv_t *ps_mv_col,
    WORD32 *pu4_avail_col_flag,
    WORD32 use_pu_ref_idx,
    WORD32 x_col,
    WORD32 y_col)
{
    sps_t *ps_sps = ps_mv_ctxt->ps_sps;
    slice_header_t *ps_slice_hdr = ps_mv_ctxt->ps_slice_hdr;
    recon_pic_buf_t *ps_col_ref_buf;
    WORD32 xp_col, yp_col;  //In pixel unit
    WORD32 col_ctb_x, col_ctb_y;  //In CTB unit
    mv_t as_mv_col[2];
    WORD32 log2_ctb_size;
    WORD32 ctb_size;
    WORD32 avail_col;
    WORD32 col_ctb_idx, pu_cnt;
    WORD32 au4_list_col[2];
    WORD32 num_minpu_in_ctb;
    UWORD8 *pu1_pic_pu_map_ctb;
    pu_col_mv_t *ps_col_mv;
    WORD32 part_pos_y;

    part_pos_y = ps_pu->b4_pos_y << 2;

    log2_ctb_size = ps_sps->i1_log2_ctb_size;
    ctb_size = (1 << log2_ctb_size);

    avail_col = 1;

    /* Initializing reference list */
    if((ps_slice_hdr->i1_slice_type == BSLICE) && (ps_slice_hdr->i1_collocated_from_l0_flag == 0))
    {
        /* L1 */
        ps_col_ref_buf = ps_mv_ctxt->ps_ref_list[1][ps_slice_hdr->i1_collocated_ref_idx];
    }
    else
    {
        /* L0 */
        ps_col_ref_buf = ps_mv_ctxt->ps_ref_list[0][ps_slice_hdr->i1_collocated_ref_idx];
    }
    num_minpu_in_ctb = (ctb_size / MIN_PU_SIZE) * (ctb_size / MIN_PU_SIZE);

    if(((part_pos_y >> log2_ctb_size) == (y_col >> log2_ctb_size)) &&
       (((x_col + (ps_mv_ctxt->i4_ctb_x << log2_ctb_size)) < ps_sps->i2_pic_width_in_luma_samples) ||
        ps_mv_ctxt->ai4_tile_xtra_ctb[2]) &&
       ((((y_col + (ps_mv_ctxt->i4_ctb_y << log2_ctb_size)) <
          ps_sps->i2_pic_height_in_luma_samples) ||
         ps_mv_ctxt->ai4_tile_xtra_ctb[3])))
    {
        xp_col = ((x_col >> 4) << 4);
        yp_col = ((y_col >> 4) << 4);
        col_ctb_x = ps_mv_ctxt->i4_ctb_x + (xp_col >> log2_ctb_size);
        col_ctb_y = ps_mv_ctxt->i4_ctb_y + (yp_col >> log2_ctb_size);

        /* pu1_frm_pu_map has (i2_pic_wd_in_ctb + 1) CTBs for stride */
        col_ctb_idx = col_ctb_x + (col_ctb_y) * (ps_sps->i2_pic_wd_in_ctb + 1);

        if(xp_col == ctb_size)
            xp_col = 0;

        pu1_pic_pu_map_ctb = ps_col_ref_buf->pu1_frm_pu_map + col_ctb_idx * num_minpu_in_ctb;

        pu_cnt = pu1_pic_pu_map_ctb[(yp_col >> 2) * (ctb_size / MIN_PU_SIZE) + (xp_col >> 2)];

        /* ps_frm_col_mv has (i2_pic_wd_in_ctb + 1) CTBs for stride */
        ps_col_mv = ps_col_ref_buf->ps_frm_col_mv +
                    (col_ctb_y * (ps_sps->i2_pic_wd_in_ctb + 1) + col_ctb_x) * num_minpu_in_ctb +
                    pu_cnt;
    }
    else
        avail_col = 0;

    if((avail_col == 0) || (ps_col_mv->b1_intra_flag == 1) ||
       (ps_slice_hdr->i1_slice_temporal_mvp_enable_flag == 0))
    {
        pu4_avail_col_flag[0] = 0;
        pu4_avail_col_flag[1] = 0;
        ps_mv_col[0].i2_mvx = 0;
        ps_mv_col[0].i2_mvy = 0;
        ps_mv_col[1].i2_mvx = 0;
        ps_mv_col[1].i2_mvy = 0;
    }
    else
    {
        WORD32 au4_ref_idx_col[2];
        WORD32 pred_flag_l0, pred_flag_l1;
        pred_flag_l0 = (ps_col_mv->b2_pred_mode != PRED_L1);
        pred_flag_l1 = (ps_col_mv->b2_pred_mode != PRED_L0);

        if(pred_flag_l0 == 0)
        {
            as_mv_col[0] = ps_col_mv->s_l1_mv;
            au4_ref_idx_col[0] = ps_col_mv->i1_l1_ref_idx;
            au4_list_col[0] = 1; /* L1 */

            as_mv_col[1] = ps_col_mv->s_l1_mv;
            au4_ref_idx_col[1] = ps_col_mv->i1_l1_ref_idx;
            au4_list_col[1] = 1; /* L1 */
        }
        else
        {
            if(pred_flag_l1 == 0)
            {
                as_mv_col[0] = ps_col_mv->s_l0_mv;
                au4_ref_idx_col[0] = ps_col_mv->i1_l0_ref_idx;
                au4_list_col[0] = 0; /* L1 */

                as_mv_col[1] = ps_col_mv->s_l0_mv;
                au4_ref_idx_col[1] = ps_col_mv->i1_l0_ref_idx;
                au4_list_col[1] = 0; /* L1 */
            }
            else
            {
                if(1 == ps_slice_hdr->i1_low_delay_flag)
                {
                    as_mv_col[0] = ps_col_mv->s_l0_mv;
                    au4_ref_idx_col[0] = ps_col_mv->i1_l0_ref_idx;
                    au4_list_col[0] = 0; /* L0 */

                    as_mv_col[1] = ps_col_mv->s_l1_mv;
                    au4_ref_idx_col[1] = ps_col_mv->i1_l1_ref_idx;
                    au4_list_col[1] = 1; /* L1 */
                }
                else
                {
                    if(0 == ps_slice_hdr->i1_collocated_from_l0_flag)
                    {
                        as_mv_col[0] = ps_col_mv->s_l0_mv;
                        au4_ref_idx_col[0] = ps_col_mv->i1_l0_ref_idx;

                        as_mv_col[1] = ps_col_mv->s_l0_mv;
                        au4_ref_idx_col[1] = ps_col_mv->i1_l0_ref_idx;
                    }
                    else
                    {
                        as_mv_col[0] = ps_col_mv->s_l1_mv;
                        au4_ref_idx_col[0] = ps_col_mv->i1_l1_ref_idx;

                        as_mv_col[1] = ps_col_mv->s_l1_mv;
                        au4_ref_idx_col[1] = ps_col_mv->i1_l1_ref_idx;
                    }

                    au4_list_col[0] =
                        ps_slice_hdr->i1_collocated_from_l0_flag; /* L"collocated_from_l0_flag" */
                    au4_list_col[1] =
                        ps_slice_hdr->i1_collocated_from_l0_flag; /* L"collocated_from_l0_flag" */
                }
            }
        }
        avail_col = 1;
        {
            WORD32 cur_poc, col_poc, col_ref_poc_l0, cur_ref_poc;
            WORD32 col_ref_poc_l0_lt, cur_ref_poc_lt;
            WORD32 ref_idx_l0, ref_idx_l1;

            if(use_pu_ref_idx)
            {
                ref_idx_l0 = ps_pu->mv.i1_l0_ref_idx;
                ref_idx_l1 = ps_pu->mv.i1_l1_ref_idx;
            }
            else
            {
                ref_idx_l0 = 0;
                ref_idx_l1 = 0;
            }

            col_poc = ps_col_ref_buf->i4_poc;
            cur_poc = ps_slice_hdr->i4_abs_pic_order_cnt;

            if(-1 != ref_idx_l0)
            {
                if(au4_list_col[0] == 0)
                {
                    col_ref_poc_l0 = ps_col_ref_buf->ai4_col_l0_poc[au4_ref_idx_col[0]];
                    col_ref_poc_l0_lt = 0; /* Encoder has only short term references */
                }
                else
                {
                    col_ref_poc_l0 = ps_col_ref_buf->ai4_col_l1_poc[au4_ref_idx_col[0]];
                    col_ref_poc_l0_lt = 0;
                }
                /* L0 collocated mv */
                cur_ref_poc = ps_mv_ctxt->ps_ref_list[0][ref_idx_l0]->i4_poc;
                cur_ref_poc_lt = 0;

                {
                    pu4_avail_col_flag[0] = 1;

                    /*if(cur_ref_poc_lt || ((col_poc - col_ref_poc_l0) == (cur_poc - cur_ref_poc)))*/
                    if((col_poc - col_ref_poc_l0) == (cur_poc - cur_ref_poc))
                    {
                        ps_mv_col[0] = as_mv_col[0];
                    }
                    else
                    {
                        ps_mv_col[0] = as_mv_col[0];
                        if(col_ref_poc_l0 != col_poc)
                        {
                            ihevce_scale_collocated_mv(
                                (mv_t *)(&ps_mv_col[0]),
                                cur_ref_poc,
                                col_ref_poc_l0,
                                col_poc,
                                cur_poc);
                        }
                    }
                }
            }
            else
            {
                pu4_avail_col_flag[0] = 0;
                ps_mv_col[0].i2_mvx = 0;
                ps_mv_col[0].i2_mvy = 0;
            }
            if((BSLICE == ps_slice_hdr->i1_slice_type) && (-1 != ref_idx_l1))
            {
                WORD32 col_ref_poc_l1_lt, col_ref_poc_l1;

                if(au4_list_col[1] == 0)
                {
                    col_ref_poc_l1 = ps_col_ref_buf->ai4_col_l0_poc[au4_ref_idx_col[0]];
                    col_ref_poc_l1_lt = 0;
                }
                else
                {
                    col_ref_poc_l1 = ps_col_ref_buf->ai4_col_l1_poc[au4_ref_idx_col[0]];
                    col_ref_poc_l1_lt = 0;
                }

                /* L1 collocated mv */
                cur_ref_poc = ps_mv_ctxt->ps_ref_list[1][ref_idx_l1]->i4_poc;
                cur_ref_poc_lt = 0;

                {
                    pu4_avail_col_flag[1] = 1;

                    /*if(cur_ref_poc_lt || ((col_poc - col_ref_poc_l1) == (cur_poc - cur_ref_poc)))*/
                    if((col_poc - col_ref_poc_l1) == (cur_poc - cur_ref_poc))
                    {
                        ps_mv_col[1] = as_mv_col[1];
                    }
                    else
                    {
                        ps_mv_col[1] = as_mv_col[1];
                        if(col_ref_poc_l1 != col_poc)
                        {
                            ihevce_scale_collocated_mv(
                                (mv_t *)&ps_mv_col[1],
                                cur_ref_poc,
                                col_ref_poc_l1,
                                col_poc,
                                cur_poc);
                        }
                    }
                }
            } /* End of if BSLICE */
            else
            {
                pu4_avail_col_flag[1] = 0;
            }
        }

    } /* End of collocated MV calculation */

} /* End of ihevce_collocated_mvp */

/**
 *******************************************************************************
 *
 * @brief Compare Motion vectors function
 *
 *
 * @par Description:
 *   Checks if MVs and Reference idx are excatly matching.
 *
 * @param[inout] ps_1
 *   motion vector 1 to be compared
 *
 * @param[in] ps_2
 *   motion vector 2 to be compared
 *
 * @returns
 *  0 : if not matching 1 : if matching
 *
 * @remarks
 *
 *******************************************************************************
 */

/**
 *******************************************************************************
 *
 * @brief
 * This function performs Motion Vector Merge candidates derivation
 *
 * @par Description:
 *  MV merge list is computed using neighbor mvs and colocated mv
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
 * @param[in] part_mode
 * Partition mode @sa PART_SIZE_E
 *
 * @param[in] part_idx
 * Partition idx of current partition inside CU
 *
 * @param[in] single_mcl_flag
 * Single MCL flag based on 8x8 CU and Parallel merge value
 *
 * @param[out] ps_merge_cand_list
 * pointer to store MV merge candidates list
 *
 * @returns
 * Number of merge candidates
 * @remarks
 *
 *
 *******************************************************************************
 */
WORD32 ihevce_mv_pred_merge(
    mv_pred_ctxt_t *ps_ctxt,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_left_nbr_4x4,
    WORD32 left_nbr_4x4_strd,
    nbr_avail_flags_t *ps_avail_flags,
    pu_mv_t *ps_col_mv,
    pu_t *ps_pu,
    PART_SIZE_E part_mode,
    WORD32 part_idx,
    WORD32 single_mcl_flag,
    merge_cand_list_t *ps_merge_cand_list,
    UWORD8 *pu1_is_top_used)
{
    /******************************************************/
    /*      Spatial Merge Candidates                      */
    /******************************************************/
    WORD32 part_pos_x;
    WORD32 part_pos_y;
    WORD32 part_wd;
    WORD32 part_ht;
    WORD32 slice_type;
    WORD32 num_ref_idx_l0_active;
    WORD32 num_ref_idx_l1_active;
    WORD32 num_merge_cand;
    WORD32 log2_parallel_merge_level_minus2;
    WORD32 n;
    WORD8 i1_spatial_avail_flag_n[MAX_NUM_MV_NBR]; /*[A0/A1/B0/B1/B2]*/
    WORD32 nbr_x[MAX_NUM_MV_NBR], nbr_y[MAX_NUM_MV_NBR];
    UWORD8 u1_nbr_avail[MAX_NUM_MV_NBR];
    WORD32 merge_shift;
    nbr_4x4_t *ps_nbr_mv[MAX_NUM_MV_NBR];

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

    part_pos_x = ps_pu->b4_pos_x << 2;
    part_pos_y = ps_pu->b4_pos_y << 2;
    part_ht = (ps_pu->b4_ht + 1) << 2;
    part_wd = (ps_pu->b4_wd + 1) << 2;

    slice_type = ps_ctxt->ps_slice_hdr->i1_slice_type;
    num_ref_idx_l0_active = ps_ctxt->ps_slice_hdr->i1_num_ref_idx_l0_active;
    num_ref_idx_l1_active = ps_ctxt->ps_slice_hdr->i1_num_ref_idx_l1_active;
    log2_parallel_merge_level_minus2 = ps_ctxt->i4_log2_parallel_merge_level_minus2;

    /* Assigning co-ordinates to neighbors */
    nbr_x[NBR_A0] = part_pos_x - 1;
    nbr_y[NBR_A0] = part_pos_y + part_ht; /* A0 */

    nbr_x[NBR_A1] = part_pos_x - 1;
    nbr_y[NBR_A1] = part_pos_y + part_ht - 1; /* A1 */

    nbr_x[NBR_B0] = part_pos_x + part_wd;
    nbr_y[NBR_B0] = part_pos_y - 1; /* B0 */

    nbr_x[NBR_B1] = part_pos_x + part_wd - 1;
    nbr_y[NBR_B1] = part_pos_y - 1; /* B1 */

    nbr_x[NBR_B2] = part_pos_x - 1;
    nbr_y[NBR_B2] = part_pos_y - 1; /* B2 */

    /* Assigning mv's */
    ps_nbr_mv[NBR_A0] = ps_left_nbr_4x4 + ((nbr_y[NBR_A0] - part_pos_y) >> 2) * left_nbr_4x4_strd;
    ps_nbr_mv[NBR_A1] = ps_left_nbr_4x4 + ((nbr_y[NBR_A1] - part_pos_y) >> 2) * left_nbr_4x4_strd;
    ps_nbr_mv[NBR_B0] = ps_top_nbr_4x4 + ((nbr_x[NBR_B0] - part_pos_x) >> 2);
    ps_nbr_mv[NBR_B1] = ps_top_nbr_4x4 + ((nbr_x[NBR_B1] - part_pos_x) >> 2);

    if(part_pos_y == 0) /* AT vertical CTB boundary */
        ps_nbr_mv[NBR_B2] = ps_top_nbr_4x4 + ((nbr_x[NBR_B2] - part_pos_x) >> 2);
    else
        ps_nbr_mv[NBR_B2] = ps_top_left_nbr_4x4;

    /* Assigning nbr availability */
    u1_nbr_avail[NBR_A0] = ps_avail_flags->u1_bot_lt_avail &&
                           (!ps_nbr_mv[NBR_A0]->b1_intra_flag); /* A0 */
    u1_nbr_avail[NBR_A1] = ps_avail_flags->u1_left_avail &&
                           (!ps_nbr_mv[NBR_A1]->b1_intra_flag); /* A1 */
    u1_nbr_avail[NBR_B0] = ps_avail_flags->u1_top_rt_avail &&
                           (!ps_nbr_mv[NBR_B0]->b1_intra_flag); /* B0 */
    u1_nbr_avail[NBR_B1] = ps_avail_flags->u1_top_avail &&
                           (!ps_nbr_mv[NBR_B1]->b1_intra_flag); /* B1 */
    u1_nbr_avail[NBR_B2] = ps_avail_flags->u1_top_lt_avail &&
                           (!ps_nbr_mv[NBR_B2]->b1_intra_flag); /* B2 */

    merge_shift = log2_parallel_merge_level_minus2 + 2;

    /* Availability check */
    /* A1 */
    {
        WORD32 avail_flag;
        avail_flag = 1;
        n = NBR_A1;

        /* if at same merge level */
        if((part_pos_x >> merge_shift) == (nbr_x[n] >> merge_shift) &&
           ((part_pos_y >> merge_shift) == (nbr_y[n] >> merge_shift)))
        {
            u1_nbr_avail[n] = 0;
        }

        /* SPEC JCTVC-K1003_v9 version has a different way using not available       */
        /* candidates compared to software. for non square part and seconf part case */
        /* ideally nothing from the 1st partition should be used as per spec but     */
        /* HM 8.2 dev verison does not adhere to this. currenlty code fllows HM      */

        /* if single MCL is 0 , second part of 2 part in CU */
        if((single_mcl_flag == 0) && (part_idx == 1) &&
           ((part_mode == PART_Nx2N) || (part_mode == PART_nLx2N) || (part_mode == PART_nRx2N)))
        {
            u1_nbr_avail[n] = 0;
        }

        if(u1_nbr_avail[n] == 0)
        {
            avail_flag = 0;
        }
        i1_spatial_avail_flag_n[n] = avail_flag;
    }
    /* B1 */
    {
        WORD32 avail_flag;
        avail_flag = 1;
        n = NBR_B1;

        /* if at same merge level */
        if((part_pos_x >> merge_shift) == (nbr_x[n] >> merge_shift) &&
           ((part_pos_y >> merge_shift) == (nbr_y[n] >> merge_shift)))
        {
            u1_nbr_avail[n] = 0;
        }

        /* if single MCL is 0 , second part of 2 part in CU */
        if((single_mcl_flag == 0) && (part_idx == 1) &&
           ((part_mode == PART_2NxN) || (part_mode == PART_2NxnU) || (part_mode == PART_2NxnD)))
        {
            u1_nbr_avail[n] = 0;
        }

        if(u1_nbr_avail[n] == 0)
        {
            avail_flag = 0;
        }

        if((avail_flag == 1) && (u1_nbr_avail[NBR_A1] == 1))
        {
            /* TODO: Assumption: mvs and ref indicies in both l0 and l1*/
            /* should match for non availability                       */
            WORD32 i4_pred_1, i4_pred_2;
            i4_pred_1 =
                (ps_nbr_mv[NBR_A1]->b1_pred_l0_flag | (ps_nbr_mv[NBR_A1]->b1_pred_l1_flag << 1)) -
                1;
            i4_pred_2 = (ps_nbr_mv[n]->b1_pred_l0_flag | (ps_nbr_mv[n]->b1_pred_l1_flag << 1)) - 1;
            if(ihevce_compare_pu_mv_t(
                   &ps_nbr_mv[NBR_A1]->mv, &ps_nbr_mv[n]->mv, i4_pred_1, i4_pred_2))
            {
                avail_flag = 0;
            }
        }
        i1_spatial_avail_flag_n[n] = avail_flag;
    }

    /* B0 */
    {
        WORD32 avail_flag;
        avail_flag = 1;
        n = NBR_B0;

        /* if at same merge level */
        if((part_pos_x >> merge_shift) == (nbr_x[n] >> merge_shift) &&
           ((part_pos_y >> merge_shift) == (nbr_y[n] >> merge_shift)))
        {
            u1_nbr_avail[n] = 0;
        }

        if(u1_nbr_avail[n] == 0)
        {
            avail_flag = 0;
        }

        if((avail_flag == 1) && (u1_nbr_avail[NBR_B1] == 1))
        {
            WORD32 i4_pred_1, i4_pred_2;
            i4_pred_1 =
                (ps_nbr_mv[NBR_B1]->b1_pred_l0_flag | (ps_nbr_mv[NBR_B1]->b1_pred_l1_flag << 1)) -
                1;
            i4_pred_2 = (ps_nbr_mv[n]->b1_pred_l0_flag | (ps_nbr_mv[n]->b1_pred_l1_flag << 1)) - 1;
            if(ihevce_compare_pu_mv_t(
                   &ps_nbr_mv[NBR_B1]->mv, &ps_nbr_mv[n]->mv, i4_pred_1, i4_pred_2))
            {
                avail_flag = 0;
            }
        }
        i1_spatial_avail_flag_n[n] = avail_flag;
    }

    /* A0 */
    {
        WORD32 avail_flag;
        avail_flag = 1;
        n = NBR_A0;

        /* if at same merge level */
        if((part_pos_x >> merge_shift) == (nbr_x[n] >> merge_shift) &&
           ((part_pos_y >> merge_shift) == (nbr_y[n] >> merge_shift)))
        {
            u1_nbr_avail[n] = 0;
        }

        if(u1_nbr_avail[n] == 0)
        {
            avail_flag = 0;
        }

        if((avail_flag == 1) && (u1_nbr_avail[NBR_A1] == 1))
        {
            WORD32 i4_pred_1, i4_pred_2;
            i4_pred_1 =
                (ps_nbr_mv[NBR_A1]->b1_pred_l0_flag | (ps_nbr_mv[NBR_A1]->b1_pred_l1_flag << 1)) -
                1;
            i4_pred_2 = (ps_nbr_mv[n]->b1_pred_l0_flag | (ps_nbr_mv[n]->b1_pred_l1_flag << 1)) - 1;
            if(ihevce_compare_pu_mv_t(
                   &ps_nbr_mv[NBR_A1]->mv, &ps_nbr_mv[n]->mv, i4_pred_1, i4_pred_2))
            {
                avail_flag = 0;
            }
        }
        i1_spatial_avail_flag_n[n] = avail_flag;
    }
    /* B2 */
    {
        WORD32 avail_flag;
        avail_flag = 1;
        n = NBR_B2;

        /* if at same merge level */
        if((part_pos_x >> merge_shift) == (nbr_x[n] >> merge_shift) &&
           ((part_pos_y >> merge_shift) == (nbr_y[n] >> merge_shift)))
        {
            u1_nbr_avail[n] = 0;
        }

        if(u1_nbr_avail[n] == 0)
        {
            avail_flag = 0;
        }

        if((i1_spatial_avail_flag_n[NBR_A0] + i1_spatial_avail_flag_n[NBR_A1] +
            i1_spatial_avail_flag_n[NBR_B0] + i1_spatial_avail_flag_n[NBR_B1]) == 4)
        {
            avail_flag = 0;
        }

        if(avail_flag == 1)
        {
            if(u1_nbr_avail[NBR_A1] == 1)
            {
                WORD32 i4_pred_1, i4_pred_2;
                i4_pred_1 = (ps_nbr_mv[NBR_A1]->b1_pred_l0_flag |
                             (ps_nbr_mv[NBR_A1]->b1_pred_l1_flag << 1)) -
                            1;
                i4_pred_2 =
                    (ps_nbr_mv[n]->b1_pred_l0_flag | (ps_nbr_mv[n]->b1_pred_l1_flag << 1)) - 1;
                if(ihevce_compare_pu_mv_t(
                       &ps_nbr_mv[NBR_A1]->mv, &ps_nbr_mv[n]->mv, i4_pred_1, i4_pred_2))
                {
                    avail_flag = 0;
                }
            }
            if(u1_nbr_avail[NBR_B1] == 1)
            {
                WORD32 i4_pred_1, i4_pred_2;
                i4_pred_1 = (ps_nbr_mv[NBR_B1]->b1_pred_l0_flag |
                             (ps_nbr_mv[NBR_B1]->b1_pred_l1_flag << 1)) -
                            1;
                i4_pred_2 =
                    (ps_nbr_mv[n]->b1_pred_l0_flag | (ps_nbr_mv[n]->b1_pred_l1_flag << 1)) - 1;
                if(ihevce_compare_pu_mv_t(
                       &ps_nbr_mv[NBR_B1]->mv, &ps_nbr_mv[n]->mv, i4_pred_1, i4_pred_2))
                {
                    avail_flag = 0;
                }
            }
        }
        i1_spatial_avail_flag_n[n] = avail_flag;
    }

    /******************************************************/
    /*          Merge Candidates List                     */
    /******************************************************/
    /* Preparing MV merge candidate list */
    {
        WORD32 merge_list_priority[MAX_NUM_MERGE_CAND] = { NBR_A1, NBR_B1, NBR_B0, NBR_A0, NBR_B2 };

        num_merge_cand = 0;
        for(n = 0; n < MAX_NUM_MERGE_CAND; n++)
        {
            WORD32 merge_idx;
            merge_idx = merge_list_priority[n];
            if(i1_spatial_avail_flag_n[merge_idx] == 1)
            {
                ps_merge_cand_list[num_merge_cand].mv = ps_nbr_mv[merge_idx]->mv;
                ps_merge_cand_list[num_merge_cand].u1_pred_flag_l0 =
                    (UWORD8)ps_nbr_mv[merge_idx]->b1_pred_l0_flag;
                ps_merge_cand_list[num_merge_cand].u1_pred_flag_l1 =
                    (UWORD8)ps_nbr_mv[merge_idx]->b1_pred_l1_flag;

                switch(merge_list_priority[n])
                {
                case NBR_A1:
                case NBR_A0:
                {
                    pu1_is_top_used[num_merge_cand] = 0;

                    break;
                }
                default:
                {
                    pu1_is_top_used[num_merge_cand] = 1;

                    break;
                }
                }

                num_merge_cand++;
            }
        }

        /******************************************************/
        /*           Temporal Merge Candidates                */
        /******************************************************/
        if(num_merge_cand < MAX_NUM_MERGE_CAND)
        {
            mv_t as_mv_col[2];
            WORD32 avail_col_flag[2] = { 0 }, x_col, y_col;
            WORD32 avail_col_l0, avail_col_l1;

            /* Checking Collocated MV availability at Bottom right of PU*/
            x_col = part_pos_x + part_wd;
            y_col = part_pos_y + part_ht;
            ihevce_collocated_mvp(ps_ctxt, ps_pu, as_mv_col, avail_col_flag, 0, x_col, y_col);

            avail_col_l0 = avail_col_flag[0];
            avail_col_l1 = avail_col_flag[1];

            if(avail_col_l0 || avail_col_l1)
            {
                ps_merge_cand_list[num_merge_cand].mv.s_l0_mv = as_mv_col[0];
                ps_merge_cand_list[num_merge_cand].mv.s_l1_mv = as_mv_col[1];
            }

            if(avail_col_l0 == 0 || avail_col_l1 == 0)
            {
                /* Checking Collocated MV availability at Center of PU */
                x_col = part_pos_x + (part_wd >> 1);
                y_col = part_pos_y + (part_ht >> 1);
                ihevce_collocated_mvp(ps_ctxt, ps_pu, as_mv_col, avail_col_flag, 0, x_col, y_col);

                if(avail_col_l0 == 0)
                {
                    ps_merge_cand_list[num_merge_cand].mv.s_l0_mv = as_mv_col[0];
                }
                if(avail_col_l1 == 0)
                {
                    ps_merge_cand_list[num_merge_cand].mv.s_l1_mv = as_mv_col[1];
                }

                avail_col_l0 |= avail_col_flag[0];
                avail_col_l1 |= avail_col_flag[1];
            }

            ps_merge_cand_list[num_merge_cand].mv.i1_l0_ref_idx = 0;
            ps_merge_cand_list[num_merge_cand].mv.i1_l1_ref_idx = 0;
            ps_merge_cand_list[num_merge_cand].u1_pred_flag_l0 = avail_col_l0 ? 1 : 0;
            ps_merge_cand_list[num_merge_cand].u1_pred_flag_l1 = avail_col_l1 ? 1 : 0;

            if(avail_col_l0 || avail_col_l1)
            {
                pu1_is_top_used[num_merge_cand] = 0;
                num_merge_cand++;
            }
        }

        /******************************************************/
        /*      Bi pred merge candidates                      */
        /******************************************************/
        if(slice_type == BSLICE)
        {
            if((num_merge_cand > 1) && (num_merge_cand < MAX_NUM_MERGE_CAND))
            {
                WORD32 priority_list0[12] = { 0, 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3 };
                WORD32 priority_list1[12] = { 1, 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2 };
                WORD32 l0_cand, l1_cand;
                WORD32 bi_pred_idx = 0;
                WORD32 total_bi_pred_cand = num_merge_cand * (num_merge_cand - 1);

                while(bi_pred_idx < total_bi_pred_cand)
                {
                    l0_cand = priority_list0[bi_pred_idx];
                    l1_cand = priority_list1[bi_pred_idx];

                    if((ps_merge_cand_list[l0_cand].u1_pred_flag_l0 == 1) &&
                       (ps_merge_cand_list[l1_cand].u1_pred_flag_l1 == 1))
                    {
                        WORD8 i1_l0_ref_idx, i1_l1_ref_idx;
                        WORD32 l0_poc, l1_poc;
                        mv_t s_l0_mv, s_l1_mv;

                        i1_l0_ref_idx = ps_merge_cand_list[l0_cand].mv.i1_l0_ref_idx;
                        i1_l1_ref_idx = ps_merge_cand_list[l1_cand].mv.i1_l1_ref_idx;
                        l0_poc = ps_ctxt->ps_ref_list[0][i1_l0_ref_idx]->i4_poc;
                        l1_poc = ps_ctxt->ps_ref_list[1][i1_l1_ref_idx]->i4_poc;
                        s_l0_mv = ps_merge_cand_list[l0_cand].mv.s_l0_mv;
                        s_l1_mv = ps_merge_cand_list[l1_cand].mv.s_l1_mv;

                        if((l0_poc != l1_poc) || (s_l0_mv.i2_mvx != s_l1_mv.i2_mvx) ||
                           (s_l0_mv.i2_mvy != s_l1_mv.i2_mvy))
                        {
                            ps_merge_cand_list[num_merge_cand].mv.s_l0_mv = s_l0_mv;
                            ps_merge_cand_list[num_merge_cand].mv.s_l1_mv = s_l1_mv;
                            ps_merge_cand_list[num_merge_cand].mv.i1_l0_ref_idx = i1_l0_ref_idx;
                            ps_merge_cand_list[num_merge_cand].mv.i1_l1_ref_idx = i1_l1_ref_idx;
                            ps_merge_cand_list[num_merge_cand].u1_pred_flag_l0 = 1;
                            ps_merge_cand_list[num_merge_cand].u1_pred_flag_l1 = 1;

                            if(pu1_is_top_used[l0_cand] || pu1_is_top_used[l1_cand])
                            {
                                pu1_is_top_used[num_merge_cand] = 1;
                            }
                            else
                            {
                                pu1_is_top_used[num_merge_cand] = 0;
                            }

                            num_merge_cand++;
                        }
                    }

                    bi_pred_idx++;

                    if((bi_pred_idx == total_bi_pred_cand) ||
                       (num_merge_cand == MAX_NUM_MERGE_CAND))
                    {
                        break;
                    }
                }
            }
        } /* End of Bipred merge candidates */

        /******************************************************/
        /*      Zero merge candidates                         */
        /******************************************************/
        if(num_merge_cand < MAX_NUM_MERGE_CAND)
        {
            WORD32 num_ref_idx;
            WORD32 zero_idx;

            zero_idx = 0;

            if(slice_type == PSLICE)
                num_ref_idx = num_ref_idx_l0_active;
            else
                /* Slice type B */
                num_ref_idx = MIN(num_ref_idx_l0_active, num_ref_idx_l1_active);

            while(num_merge_cand < MAX_NUM_MERGE_CAND)
            {
                if(slice_type == PSLICE)
                {
                    ps_merge_cand_list[num_merge_cand].mv.i1_l0_ref_idx = zero_idx;
                    ps_merge_cand_list[num_merge_cand].mv.i1_l1_ref_idx = -1;
                    ps_merge_cand_list[num_merge_cand].u1_pred_flag_l0 = 1;
                    ps_merge_cand_list[num_merge_cand].u1_pred_flag_l1 = 0;
                }
                else /* Slice type B */
                {
                    ps_merge_cand_list[num_merge_cand].mv.i1_l0_ref_idx = zero_idx;
                    ps_merge_cand_list[num_merge_cand].mv.i1_l1_ref_idx = zero_idx;
                    ps_merge_cand_list[num_merge_cand].u1_pred_flag_l0 = 1;
                    ps_merge_cand_list[num_merge_cand].u1_pred_flag_l1 = 1;
                }

                ps_merge_cand_list[num_merge_cand].mv.s_l0_mv.i2_mvx = 0;
                ps_merge_cand_list[num_merge_cand].mv.s_l0_mv.i2_mvy = 0;
                ps_merge_cand_list[num_merge_cand].mv.s_l1_mv.i2_mvx = 0;
                ps_merge_cand_list[num_merge_cand].mv.s_l1_mv.i2_mvy = 0;

                pu1_is_top_used[num_merge_cand] = 0;

                num_merge_cand++;
                zero_idx++;

                /* if all the reference pics have been added as candidates      */
                /* the the loop shoudl break since it would add same cand again */
                if(zero_idx == num_ref_idx)
                {
                    break;
                }
            }
        } /* End of zero merge candidates */

    } /* End of merge candidate list population */

    return (num_merge_cand);
}
