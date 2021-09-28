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
* @file ihevce_bs_compute_ctb.c
*
* @brief
*  This file contains functions needed for boundary strength calculation
*
* @author
*  ittiam
*
* @List of Functions
*  ihevce_bs_init_ctb()
*  ihevce_bs_compute_ctb()
*  ihevce_bs_clear_invalid()
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
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_global_tables.h"

/*****************************************************************************/
/* Global Tables                                                             */
/*****************************************************************************/
// clang-format off
UWORD16 gau2_bs_table[2][8] =
{
    { BS_INTRA_4, BS_INTRA_8, BS_INVALID, BS_INTRA_16, BS_INVALID, BS_INVALID, BS_INVALID, BS_INTRA_32 },
    { BS_CBF_4, BS_CBF_8, BS_INVALID, BS_CBF_16, BS_INVALID, BS_INVALID, BS_INVALID, BS_CBF_32 }
};
// clang-format on

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief Initialize the Boundary Strength at a CTB level
*
* @par   Description
* Initialize the Boundary Strength at a CTB level to zeros*
*
* @param[out] ps_deblk_prms
* Pointer to structure s_deblk_prms, which contains
* s_deblk_prms.au4_horz_bs : max of 8 such conti. bs to be comp. for 64x64 ctb
* s_deblk_prms.au4_vert_bs : max of 8 such conti. bs to be comp. for 64x64 ctb
*
* @param[in] ctb_size
* Size in pels (can be 16, 32 or 64)
*
* @returns none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_bs_init_ctb(
    deblk_bs_ctb_ctxt_t *ps_deblk_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 ctb_ctr,
    WORD32 vert_ctr)
{
    WORD32 ctb_size = ps_frm_ctb_prms->i4_ctb_size;

    /* Pointer to the array to store the packed BS values in horizontal dir. */
    UWORD32 *pu4_horz_bs = &ps_deblk_prms->au4_horz_bs[0];
    /* Pointer to the array to store the packed BS values in vertical dir. */
    UWORD32 *pu4_vert_bs = &ps_deblk_prms->au4_vert_bs[0];

    WORD32 i4_top_ctb_tile_id, i4_left_ctb_tile_id;
    WORD32 *pi4_tile_id_map_temp;

    pi4_tile_id_map_temp = ps_frm_ctb_prms->pi4_tile_id_map +
                           vert_ctr * ps_frm_ctb_prms->i4_tile_id_ctb_map_stride + ctb_ctr;

    i4_left_ctb_tile_id = *(pi4_tile_id_map_temp - 1);
    i4_top_ctb_tile_id = *(pi4_tile_id_map_temp - ps_frm_ctb_prms->i4_tile_id_ctb_map_stride);

    ps_deblk_prms->u1_not_first_ctb_row_of_frame = (i4_top_ctb_tile_id != -1);
    ps_deblk_prms->u1_not_first_ctb_col_of_frame = (i4_left_ctb_tile_id != -1);

    /* BS should be set to NULL in the following cases
       Frame boundaries
       Edges if deblocking is disabled by disable_deblocking_filter_flag
       Slice boundaries if deblocking across slices is disabled
       Tile boundaries if deblocking across slices is disabled
       These are not considered now, except the frame boundary
    */

    /* Initializing the bs array to 0. array size = (ctb_size/8 + 1)*4 bytes */
    memset(pu4_horz_bs, 0, ((ctb_size >> 3) + 1) * sizeof(UWORD32));
    memset(pu4_vert_bs, 0, ((ctb_size >> 3) + 1) * sizeof(UWORD32));
}

/**
*******************************************************************************
*
* @brief Calculate the Boundary Strength at CU level
*
* @par   Description
* Calculate the Boundary Strength at CU level
*
* @param[in] ps_cu_final
* Pointer to the final CU structure, of which we use the following values
* u2_num_tus_in_cu : Total TUs in this CU
* ps_enc_tu : Pointer to first tu of this cu. Each TU need to be
* populated in TU order.
* u4_pred_mode_flag : The prediction mode flag for the CU
* cu_size : CU size in terms of min CU (8x8) units
* cu_pos_x : X Position of CU in current ctb
* cu_pos_y : Y Position of CU in current ctb
* u4_part_mode : Partition information for CU. For inter 0 : @sa PART_SIZE_E
* ps_pu : Pointer to first pu of this cu
*
* @param[in] ps_top_nbr_4x4
* Pointer to top 4x4 CU nbr structure
*
* @param[in] ps_left_nbr_4x4
* Pointer to left 4x4 CU nbr structure
*
* @param[in] ps_curr_nbr_4x4
* Pointer to current 4x4 ctb structure
*
* @param[in] nbr_4x4_left_strd
* Left nbr buffer stride in terms of 4x4 units
*
* @param[in] num_4x4_in_ctb
* Current buffer stride in terms of 4x4 units
*
* @param[out] ps_deblk_prms
* Pointer to structure s_deblk_prms, which contains
* s_deblk_prms.au4_horz_bs : max of 8 such conti. bs to be comp. for 64x64 ctb
* s_deblk_prms.au4_vert_bs : max of 8 such conti. bs to be comp. for 64x64 ctb
*
* @returns none
*
* @remarks
* 1 : Setting all 4 edges for a TU or PU block. Which is inefficient in
*   a) may set the BS twice b) set the frame/slice boundaries
* 2 : always update BS using bit-wise OR, which may set BS to 3 also.
*   ( Deblocking should take care of it as 2 itself )
*
*******************************************************************************
*/
void ihevce_bs_compute_cu(
    cu_enc_loop_out_t *ps_cu_final,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_curr_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 num_4x4_in_ctb,
    deblk_bs_ctb_ctxt_t *ps_deblk_prms)
{
    WORD32 i;
    WORD32 j;
    /* copy required arguments from pointer to CU structure */
    /* Total TUs in this CU */
    UWORD16 u2_num_tus_in_cu = ps_cu_final->u2_num_tus_in_cu;
    /* Pointer to first tu of this cu */
    tu_enc_loop_out_t *ps_enc_tu = ps_cu_final->ps_enc_tu;
    /* The prediction mode flag for the CU */
    UWORD32 u4_pred_mode_flag = ps_cu_final->b1_pred_mode_flag;
    /* X Position of CU in current ctb in (8x8) units */
    WORD32 cu_pos_x = ps_cu_final->b3_cu_pos_x;
    /* Y Position of CU in current ctb in (8x8) units */
    WORD32 cu_pos_y = ps_cu_final->b3_cu_pos_y;

    /* Indicates partition information for CU */
    UWORD32 u4_part_mode = ps_cu_final->b3_part_mode;

    /* Pointer to first pu of this cu */
    pu_t *ps_pu = ps_cu_final->ps_pu;

    /* Number of pus in current cu */
    WORD32 num_pus_in_cu;
    /* Pointer to the array to store the packed BS values in horizontal dir. */
    UWORD32 *pu4_horz_bs = &ps_deblk_prms->au4_horz_bs[0];
    /* Pointer to the array to store the packed BS values in vertical dir. */
    UWORD32 *pu4_vert_bs = &ps_deblk_prms->au4_vert_bs[0];

    (void)ps_curr_nbr_4x4;
    (void)num_4x4_in_ctb;

    /* CTB boundary case setting the BS for intra and cbf non zero case for CU top edge */
    if((ps_deblk_prms->u1_not_first_ctb_row_of_frame) && (0 == ps_cu_final->b3_cu_pos_y))
    {
        nbr_4x4_t *ps_nbr_4x4;
        UWORD32 u4_temp_bs = *pu4_horz_bs;
        WORD32 horz_bit_offset;
        WORD32 ctr;

        /* every 4x4 takes 2 bits in the register this is taken care in the loop */
        /* deriving 4x4 position */
        horz_bit_offset = (ps_cu_final->b3_cu_pos_x << 3) >> 2;

        /* scanning through each 4x4 csb along horizontal direction */
        for(ctr = 0; ctr < ((ps_cu_final->b4_cu_size << 3) >> 2); ctr++)
        {
            ps_nbr_4x4 = ps_top_nbr_4x4 + ctr;
            if(ps_nbr_4x4->b1_intra_flag)
            {
                /* To store in BigEnd. format. BS[0]|BS[1]| .. |BS[15] */
                u4_temp_bs = (u4_temp_bs | (2U << (30 - 2 * (ctr + horz_bit_offset))));
            }
            else if(ps_nbr_4x4->b1_y_cbf)
            {
                /* To store in BigEnd. format. BS[0]|BS[1]| .. |BS[15] */
                u4_temp_bs = (u4_temp_bs | (1 << (30 - 2 * (ctr + horz_bit_offset))));
            }
        }

        /* storing the BS computed for first row based on top ctb CUs  */
        *(pu4_horz_bs) = u4_temp_bs;
    }

    /* CTB boundary case setting the BS for intra and cbf non zero case for CU left edge */
    if((ps_deblk_prms->u1_not_first_ctb_col_of_frame) && (0 == ps_cu_final->b3_cu_pos_x))
    {
        nbr_4x4_t *ps_nbr_4x4;
        UWORD32 u4_temp_bs = *pu4_vert_bs;
        WORD32 vert_bit_offset;
        WORD32 ctr;

        /* every 4x4 takes 2 bits in the register this is taken care in the loop */
        /* deriving 4x4 position */
        vert_bit_offset = (ps_cu_final->b3_cu_pos_y << 3) >> 2;

        /* scanning through each 4x4 csb along vertical direction */
        for(ctr = 0; ctr < ((ps_cu_final->b4_cu_size << 3) >> 2); ctr++)
        {
            ps_nbr_4x4 = ps_left_nbr_4x4 + ctr * nbr_4x4_left_strd;
            if(ps_nbr_4x4->b1_intra_flag)
            {
                /* To store in BigEnd. format. BS[0]|BS[1]| .. |BS[15] */
                u4_temp_bs = (u4_temp_bs | (2U << (30 - 2 * (ctr + vert_bit_offset))));
            }
            else if(ps_nbr_4x4->b1_y_cbf)
            {
                /* To store in BigEnd. format. BS[0]|BS[1]| .. |BS[15] */
                u4_temp_bs = (u4_temp_bs | (1 << (30 - 2 * (ctr + vert_bit_offset))));
            }
        }

        /* storing the BS computed for first col based on left ctb Cus */
        *(pu4_vert_bs) = u4_temp_bs;
    }

    /* Passes through each TU inside the CU */
    for(i = 0; i < u2_num_tus_in_cu; i++)
    {
        UWORD32 u4_tu_pos_x, u4_tu_pos_y;
        UWORD32 u4_tu_size;
        UWORD32 *pu4_tu_top_edge;
        UWORD32 *pu4_tu_bottom_edge;
        UWORD32 *pu4_tu_left_edge;
        UWORD32 *pu4_tu_right_edge;
        UWORD32 u4_bs_value;
        WORD32 set_bs_flag = 0;
        WORD32 tbl_idx = 1;

        /* TU_size calculation */
        u4_tu_size = 1 << ((ps_enc_tu->s_tu.b3_size) + 2);

        /* TU X position in terms of min TU (4x4) units wrt ctb */
        u4_tu_pos_x = ps_enc_tu->s_tu.b4_pos_x;
        /* TU Y position in terms of min TU (4x4) units wrt ctb */
        u4_tu_pos_y = ps_enc_tu->s_tu.b4_pos_y;

        /* pointers to the edges of current TU */
        pu4_tu_top_edge = pu4_horz_bs + (u4_tu_pos_y >> 1);
        pu4_tu_bottom_edge = pu4_horz_bs + ((u4_tu_pos_y + 1) >> 1) + (u4_tu_size >> 3);
        pu4_tu_left_edge = pu4_vert_bs + (u4_tu_pos_x >> 1);
        pu4_tu_right_edge = pu4_vert_bs + ((u4_tu_pos_x + 1) >> 1) + (u4_tu_size >> 3);

        /* chooose the table index based on pred_mode */
        if(PRED_MODE_INTRA == u4_pred_mode_flag)
        {
            tbl_idx = 0;
        }

        /* get the BS value from table if required */
        if((ps_enc_tu->s_tu.b1_y_cbf) || (PRED_MODE_INTRA == u4_pred_mode_flag))
        {
            set_bs_flag = 1;
            u4_bs_value = gau2_bs_table[tbl_idx][(u4_tu_size >> 2) - 1];
        }

        if(1 == set_bs_flag)
        {
            /* Store the BS value */
            if(4 == u4_tu_size)
            {
                if(0 == (u4_tu_pos_y & 1))
                {
                    /* Only top TU edge came on a 8 pixel bounadey */
                    SET_VALUE_BIG((pu4_tu_top_edge), u4_bs_value, u4_tu_pos_x, u4_tu_size);
                }
                else
                {
                    /* Only bottom TU edge came on a 8 pixel bounadey */
                    SET_VALUE_BIG((pu4_tu_bottom_edge), u4_bs_value, u4_tu_pos_x, u4_tu_size);
                }
                if(0 == (u4_tu_pos_x & 1))
                {
                    /* Only left TU edge came on a 8 pixel bounadey */
                    SET_VALUE_BIG((pu4_tu_left_edge), u4_bs_value, u4_tu_pos_y, u4_tu_size);
                }
                else
                {
                    /* Only right TU edge came on a 8 pixel bounadey */
                    SET_VALUE_BIG((pu4_tu_right_edge), u4_bs_value, u4_tu_pos_y, u4_tu_size);
                }
            }
            /* set all edges for other TU sizes */
            else
            {
                /* setting top TU edge */
                SET_VALUE_BIG((pu4_tu_top_edge), u4_bs_value, u4_tu_pos_x, u4_tu_size);
                /* setting bottom TU edge */
                SET_VALUE_BIG((pu4_tu_bottom_edge), u4_bs_value, u4_tu_pos_x, u4_tu_size);
                /* setting left TU edge */
                SET_VALUE_BIG((pu4_tu_left_edge), u4_bs_value, u4_tu_pos_y, u4_tu_size);
                /* setting right TU edge */
                SET_VALUE_BIG((pu4_tu_right_edge), u4_bs_value, u4_tu_pos_y, u4_tu_size);
            }
        }

        /* point to next TU inside CU in TU order */
        ps_enc_tu++;
    }

    if(PRED_MODE_INTRA == u4_pred_mode_flag)
    {
        /* no mv based BS computation in INTRA case */
        return;
    }
    /* BS update due to PU mv.s */
    if(u4_part_mode == SIZE_2Nx2N) /* symmetric motion partition,  2Nx2N */
    {
        num_pus_in_cu = 1;
    }
    else if(u4_part_mode == SIZE_NxN) /* symmetric motion partition,  NxN */
    {
        num_pus_in_cu = 4;
    }
    else /* other sym. or asym. partiotions */
    {
        num_pus_in_cu = 2;
    }

    /* Go through each PU inside CU in PU order and set the top & bottom */
    /* PU edge BS accordingly */
    for(i = 0; i < num_pus_in_cu; i++)
    {
        WORD32 k;
        /* X Position of PU in terms of min PU (4x4) units in current ctb */
        WORD32 pu_pos_x = ps_pu->b4_pos_x;
        /* Y Position of PU in terms of min PU (4x4) units in current ctb */
        WORD32 pu_pos_y = ps_pu->b4_pos_y;
        /*  PU width in 4 pixel unit */
        WORD32 pu_wd = (ps_pu->b4_wd) + 1;
        /*  PU height in 4 pixel unit */
        WORD32 pu_ht = (ps_pu->b4_ht) + 1;
        /* Pred L0 flag */
        WORD32 cur_pred_l0_flag;
        /* pointer to current PU */
        nbr_4x4_t *ps_curr_nbr_4x4_pu;

        /* go through each 4x4 block along the PU edges and do BS calculation */
        /* can optimize further with proper checks according to PU size */
        /* but in that case also @CTB boundary, we should go by 4x4 nbr.s only*/

        /* load cur. PU parameters */
        WORD8 i1_cur_l0_ref_pic_buf_id, i1_cur_l1_ref_pic_buf_id;
        WORD32 cur_mv_no;
        WORD16 i2_mv_x0, i2_mv_y0, i2_mv_x1, i2_mv_y1;

        ps_curr_nbr_4x4_pu = ps_curr_nbr_4x4 + (pu_pos_x - (cu_pos_x << 1)) +
                             (pu_pos_y - (cu_pos_y << 1)) * num_4x4_in_ctb;

        cur_pred_l0_flag = ps_curr_nbr_4x4_pu->b1_pred_l0_flag;

        /* L0 & L1 unique ref. pic. id for cur. PU, (stored in upper 4 bits) */
        i1_cur_l0_ref_pic_buf_id = (ps_curr_nbr_4x4_pu->mv.i1_l0_ref_pic_buf_id);
        i1_cur_l1_ref_pic_buf_id = (ps_curr_nbr_4x4_pu->mv.i1_l1_ref_pic_buf_id);

        /* Number of motion vectors used for cur. PU */
        cur_mv_no = cur_pred_l0_flag + ps_curr_nbr_4x4_pu->b1_pred_l1_flag;

        /* x and y mv for L0 and L1, for cur. PU */
        i2_mv_x0 = ps_curr_nbr_4x4_pu->mv.s_l0_mv.i2_mvx;
        i2_mv_y0 = ps_curr_nbr_4x4_pu->mv.s_l0_mv.i2_mvy;
        i2_mv_x1 = ps_curr_nbr_4x4_pu->mv.s_l1_mv.i2_mvx;
        i2_mv_y1 = ps_curr_nbr_4x4_pu->mv.s_l1_mv.i2_mvy;

        /* two cases for updating TOP and LEFT edges respectively */
        /* k = 0 : TOP edge update, k = 1 : LEFT edge update */
        for(k = 0; k < 2; k++)
        {
            WORD32 pu_pos_pointer_calc, pu_pos_bit_calc;
            UWORD32 *pu4_pu_cur_edge;
            WORD32 pu_dim, nbr_inc;
            nbr_4x4_t *ps_nbr_4x4;

            /* TOP edge case */
            if(0 == k)
            {
                pu_pos_pointer_calc = pu_pos_y;
                pu_pos_bit_calc = pu_pos_x;
                pu4_pu_cur_edge = pu4_horz_bs + (pu_pos_y >> 1);
                pu_dim = pu_wd;

                /* top neighbours are accessed linearly */
                nbr_inc = 1;

                /* If the current 4x4 csb is in the first row of CTB */
                if(0 == pu_pos_pointer_calc)
                { /* then need to check if top CTB is physically available */
                    /* (slice bound. are considered as availabale) */
                    if(ps_deblk_prms->u1_not_first_ctb_row_of_frame)
                    {
                        ps_nbr_4x4 = ps_top_nbr_4x4 + (nbr_inc * (pu_pos_x - (cu_pos_x << 1)));
                    }
                    else
                    {
                        /* This is done for avoiding uninitialized memory access at pic. boundaries*/
                        ps_nbr_4x4 = ps_curr_nbr_4x4_pu;
                    }
                }
                /* within ctb, so top neighbour is available */
                else
                {
                    ps_nbr_4x4 = ps_curr_nbr_4x4_pu - num_4x4_in_ctb;
                }
            }
            /* LEFT edge case */
            else
            {
                pu_pos_pointer_calc = pu_pos_x;
                pu_pos_bit_calc = pu_pos_y;
                pu4_pu_cur_edge = pu4_vert_bs + (pu_pos_x >> 1);
                pu_dim = pu_ht;

                /* left neighbours are accessed using stride */
                nbr_inc = nbr_4x4_left_strd;

                /* If the current 4x4 csb is in the first col of CTB */
                if(0 == pu_pos_pointer_calc)
                { /* then need to check if left CTB is available */
                    if(ps_deblk_prms->u1_not_first_ctb_col_of_frame)
                    {
                        ps_nbr_4x4 = ps_left_nbr_4x4 + (nbr_inc * (pu_pos_y - (cu_pos_y << 1)));
                    }
                    else
                    {
                        /* This is done for avoiding uninitialized memory access at pic. boundaries*/
                        ps_nbr_4x4 = ps_curr_nbr_4x4_pu;
                        nbr_inc = num_4x4_in_ctb;
                    }
                }
                /* within ctb, so left neighbour is available */
                else
                {
                    ps_nbr_4x4 = ps_curr_nbr_4x4_pu - 1;
                    nbr_inc = num_4x4_in_ctb;
                }
            }

            /* Only if the current edge falls on 8 pixel grid and ... */
            if(0 == (pu_pos_pointer_calc & 1))
            {
                /* go through the edge in 4x4 unit. Can be optimized */
                /* In that case special case for CTB boundary */
                for(j = 0; j < pu_dim; j++)
                {
                    //nbr_4x4_t *ps_temp_nbr_4x4;

                    /* ... and if the BS not set yet */
                    if(0 == EXTRACT_VALUE_BIG(pu4_pu_cur_edge, (pu_pos_bit_calc + j)))
                    {
                        WORD8 i1_nbr_l0_ref_pic_buf_id, i1_nbr_l1_ref_pic_buf_id;
                        WORD32 nbr_mv_no;
                        WORD32 bs_flag = 0;
                        WORD32 nbr_pred_l0_flag = ps_nbr_4x4->b1_pred_l0_flag;

                        /* L0 & L1 unique ref. pic. id for nbr. csb, in upper 4 bits */
                        i1_nbr_l0_ref_pic_buf_id = (ps_nbr_4x4->mv.i1_l0_ref_pic_buf_id);
                        i1_nbr_l1_ref_pic_buf_id = (ps_nbr_4x4->mv.i1_l1_ref_pic_buf_id);

                        /* Number of motion vectors used */
                        nbr_mv_no = nbr_pred_l0_flag + ps_nbr_4x4->b1_pred_l1_flag;

                        /* If diff. no. of motion vectors used */
                        if(cur_mv_no != nbr_mv_no)
                        {
                            bs_flag = 1;
                        }
                        /* If One motion vector is used */
                        else if(1 == cur_mv_no)
                        {
                            WORD16 i2_mv_x, i2_mv_y;

                            if(cur_pred_l0_flag)
                            { /* L0 used for cur. */
                                if(nbr_pred_l0_flag)
                                { /* L0 used for nbr. */
                                    if(i1_cur_l0_ref_pic_buf_id != i1_nbr_l0_ref_pic_buf_id)
                                    {
                                        /* reference pictures used are different */
                                        bs_flag = 1;
                                    }
                                }
                                else
                                { /* L1 used for nbr. */
                                    if(i1_cur_l0_ref_pic_buf_id != i1_nbr_l1_ref_pic_buf_id)
                                    {
                                        /* reference pictures used are different */
                                        bs_flag = 1;
                                    }
                                }
                                if(!bs_flag)
                                {
                                    i2_mv_x = i2_mv_x0;
                                    i2_mv_y = i2_mv_y0;
                                }
                            }
                            else
                            { /* L1 used for cur. */
                                if(nbr_pred_l0_flag)
                                { /* L0 used for nbr. */
                                    if(i1_cur_l1_ref_pic_buf_id != i1_nbr_l0_ref_pic_buf_id)
                                    {
                                        /* reference pictures used are different */
                                        bs_flag = 1;
                                    }
                                }
                                else
                                { /* L1 used for nbr. */
                                    if(i1_cur_l1_ref_pic_buf_id != i1_nbr_l1_ref_pic_buf_id)
                                    {
                                        /* reference pictures used are different */
                                        bs_flag = 1;
                                    }
                                }
                                if(!bs_flag)
                                {
                                    i2_mv_x = i2_mv_x1;
                                    i2_mv_y = i2_mv_y1;
                                }
                            }

                            if(!bs_flag)
                            {
                                WORD16 i2_nbr_mv_x, i2_nbr_mv_y;

                                if(nbr_pred_l0_flag)
                                {
                                    i2_nbr_mv_x = ps_nbr_4x4->mv.s_l0_mv.i2_mvx;
                                    i2_nbr_mv_y = ps_nbr_4x4->mv.s_l0_mv.i2_mvy;
                                }
                                else
                                {
                                    i2_nbr_mv_x = ps_nbr_4x4->mv.s_l1_mv.i2_mvx;
                                    i2_nbr_mv_y = ps_nbr_4x4->mv.s_l1_mv.i2_mvy;
                                }
                                // clang-format off
                                bs_flag =
                                    (abs(i2_mv_x - i2_nbr_mv_x) < 4) &&
                                    (abs(i2_mv_y - i2_nbr_mv_y) < 4)
                                        ? 0
                                        : 1;
                                // clang-format on
                            }
                        }
                        /* If two motion vectors are used */
                        else if(2 == cur_mv_no)
                        {
                            /* check whether same reference pictures used */
                            if((i1_cur_l0_ref_pic_buf_id == i1_nbr_l0_ref_pic_buf_id &&
                                i1_cur_l1_ref_pic_buf_id == i1_nbr_l1_ref_pic_buf_id) ||
                               (i1_cur_l0_ref_pic_buf_id == i1_nbr_l1_ref_pic_buf_id &&
                                i1_cur_l1_ref_pic_buf_id == i1_nbr_l0_ref_pic_buf_id))
                            {
                                WORD16 i2_nbr_mv_x0, i2_nbr_mv_y0, i2_nbr_mv_x1, i2_nbr_mv_y1;

                                /* x and y mv for L0 and L1, for nbr. csb*/
                                i2_nbr_mv_x0 = ps_nbr_4x4->mv.s_l0_mv.i2_mvx;
                                i2_nbr_mv_y0 = ps_nbr_4x4->mv.s_l0_mv.i2_mvy;
                                i2_nbr_mv_x1 = ps_nbr_4x4->mv.s_l1_mv.i2_mvx;
                                i2_nbr_mv_y1 = ps_nbr_4x4->mv.s_l1_mv.i2_mvy;

                                /* Different L0 and L1 */
                                if(i1_cur_l0_ref_pic_buf_id != i1_cur_l1_ref_pic_buf_id)
                                {
                                    if(i1_cur_l0_ref_pic_buf_id == i1_nbr_l0_ref_pic_buf_id)
                                    {
                                        // clang-format off
                                        bs_flag =
                                            (abs(i2_mv_x0 - i2_nbr_mv_x0) < 4) &&
                                            (abs(i2_mv_y0 - i2_nbr_mv_y0) < 4) &&
                                            (abs(i2_mv_x1 - i2_nbr_mv_x1) < 4) &&
                                            (abs(i2_mv_y1 - i2_nbr_mv_y1) < 4)
                                                ? 0
                                                : 1;
                                        // clang-format on
                                    }
                                    else
                                    {
                                        // clang-format off
                                        bs_flag =
                                            (abs(i2_mv_x0 - i2_nbr_mv_x1) < 4) &&
                                            (abs(i2_mv_y0 - i2_nbr_mv_y1) < 4) &&
                                            (abs(i2_mv_x1 - i2_nbr_mv_x0) < 4) &&
                                            (abs(i2_mv_y1 - i2_nbr_mv_y0) < 4)
                                                ? 0
                                                : 1;
                                        // clang-format on
                                    }
                                }
                                else /* Same L0 and L1 */
                                {
                                    // clang-format off
                                    bs_flag =
                                        ((abs(i2_mv_x0 - i2_nbr_mv_x0) >= 4) ||
                                         (abs(i2_mv_y0 - i2_nbr_mv_y0) >= 4) ||
                                         (abs(i2_mv_x1 - i2_nbr_mv_x1) >= 4) ||
                                         (abs(i2_mv_y1 - i2_nbr_mv_y1) >= 4)) &&
                                        ((abs(i2_mv_x0 - i2_nbr_mv_x1) >= 4) ||
                                         (abs(i2_mv_y0 - i2_nbr_mv_y1) >= 4) ||
                                         (abs(i2_mv_x1 - i2_nbr_mv_x0) >= 4) ||
                                         (abs(i2_mv_y1 - i2_nbr_mv_y0) >= 4))
                                            ? 1
                                            : 0;
                                    // clang-format on
                                }
                            }
                            else /* If the reference pictures used are different */
                            {
                                bs_flag = 1;
                            }
                        }

                        if(bs_flag)
                        { /*Storing if BS set due to PU mvs */
                            /*Storing in BigEnd. format. BS[0]|BS[1]| .. |BS[15] & edge_size is 4*/
                            SET_VALUE_BIG((pu4_pu_cur_edge), BS_CBF_4, (pu_pos_bit_calc + j), 4);
                        }
                    }

                    /* increment the neighbour */
                    ps_nbr_4x4 += nbr_inc;
                }
            }
        }
        /* point to the next PU */
        ps_pu++;
    }
}

/**
*******************************************************************************
*
* @brief Clear the invalid Boundary Strength which may be set by
* ihevce_bs_compute_cu
*
* @par   Description
* Clear the invalid Boundary Strength which may be set by ihevce_bs_compute_cu
* (as it does all 4 edges in a shot for some cases)
*
* @param[out] ps_deblk_prms
* Pointer to structure s_deblk_prms, which contains
* s_deblk_prms.au4_horz_bs : max of 8 such conti. bs to be comp. for 64x64 ctb
* s_deblk_prms.au4_vert_bs : max of 8 such conti. bs to be comp. for 64x64 ctb
*
* @param[in] last_ctb_row_flag
* Flag for checking whether the current CTB is in last ctb_row
*
* @param[in] last_ctb_in_row_flag
* Flag for checking whether the current CTB is the last in current row
*
* @param[in] last_hz_ctb_wd
*  Valid Width (pixels) in the last CTB in every row (padding cases)
*
* @param[in] last_vt_ctb_ht
*  Valid Height (pixels) in the last CTB row (padding cases)
*
* @returns none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_bs_clear_invalid(
    deblk_bs_ctb_ctxt_t *ps_deblk_prms,
    WORD32 last_ctb_row_flag,
    WORD32 last_ctb_in_row_flag,
    WORD32 last_hz_ctb_wd,
    WORD32 last_vt_ctb_ht)
{
    /* Rightmost CTB. Right padding may be there */
    /* clear the last vert BS which might have set by ihevce_bs_compute_cu */
    if(1 == last_ctb_in_row_flag)
    {
        ps_deblk_prms->au4_vert_bs[last_hz_ctb_wd >> 3] = 0;
    }

    /* Bottommost CTB. Bottom padding may be there */
    /* clear the last horz BS which might have set by ihevce_bs_compute_cu */
    if(1 == last_ctb_row_flag)
    {
        ps_deblk_prms->au4_horz_bs[last_vt_ctb_ht >> 3] = 0;
    }
}
