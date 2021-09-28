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
*  ihevce_deblk.c
*
* @brief
*  Contains definition for the ctb level deblk function
*
* @author
*  ittiam
*
* @List of Functions:
*  ihevce_deblk_populate_qp_map()
*  ihevce_deblk_ctb()
*  ihevce_hbd_deblk_ctb()
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
#include "ihevc_debug.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_deblk_tables.h"
#include "ihevc_common_tables.h"
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
#include "ihevce_common_utils.h"
#include "ihevce_global_tables.h"
#include "ihevce_deblk.h"
#include "ihevce_tile_interface.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_deblk_populate_qp_map \endif
*
* \brief
*
*
*****************************************************************************
*/
void ihevce_deblk_populate_qp_map(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    deblk_ctbrow_prms_t *ps_deblk_ctb_row_params,
    ctb_enc_loop_out_t *ps_ctb_out_dblk,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    ihevce_tile_params_t *ps_col_tile_params)
{
    ctb_enc_loop_out_t *ps_ctb_out;
    WORD32 ctb_ctr, ctb_start, ctb_end;
    WORD32 tile_qp_offset, tile_qp_size, i4_offset_for_last_cu_qp;
    /* Create the Qp map for the entire current CTB-row for deblocking purpose(only)*/
    /* Do this iff cur pic is referred or recon dump is enabled or psnr calc is on*/
    /*Qp of the last CU of previous CTB row*/
    WORD8 i1_last_cu_qp;
    /*A pointer pointing to the top 4x4 block's Qp for all CTb rows*/
    WORD8 *pi1_qp_top_4x4_ctb_row =
        ps_deblk_ctb_row_params->api1_qp_top_4x4_ctb_row[ps_ctxt->i4_enc_frm_id] +
        (ps_deblk_ctb_row_params->u4_qp_top_4x4_buf_size * ps_ctxt->i4_bitrate_instance_num);

    UWORD32 u4_qp_top_4x4_buf_strd = ps_deblk_ctb_row_params->u4_qp_top_4x4_buf_strd;

    /*The Qp map which has to be populated*/
    UWORD32 u4_qp_buffer_stride = ps_deblk_ctb_row_params->u4_qp_buffer_stride;
    WORD8 *pi1_ctb_tile_qp = ps_deblk_ctb_row_params->pi1_ctb_row_qp;

    /*Temporary pointers to Qp map at CTB level*/
    WORD8 *pi1_ctb_qp_map_tile;

    i4_offset_for_last_cu_qp = ps_ctxt->pi4_offset_for_last_cu_qp[ps_ctxt->i4_tile_col_idx];
    /* total QPs to be copied for current row is : */
    tile_qp_size = i4_offset_for_last_cu_qp + 1;
    /*Pointing to the first CTB of current CTB row*/
    ps_ctb_out = ps_ctb_out_dblk;
    /* Offset req. for the row QP to the tile start */
    tile_qp_offset = ps_col_tile_params->i4_first_ctb_x * (ps_frm_ctb_prms->i4_ctb_size / 4);

    ctb_start = ps_col_tile_params->i4_first_ctb_x;
    ctb_end =
        (ps_col_tile_params->i4_first_ctb_x + ps_col_tile_params->i4_curr_tile_wd_in_ctb_unit);

    if(vert_ctr) /*Not first CTB row of frame*/
    {
        /*copy from top4x4_array data stored by upper CTB-row to qp-map*/
        memcpy(
            pi1_ctb_tile_qp,
            (pi1_qp_top_4x4_ctb_row + (vert_ctr - 1) * u4_qp_top_4x4_buf_strd + tile_qp_offset),
            tile_qp_size);
    }

    /*pu1_ctb_row_qp points to top4x4 row in Qp-map.
    Now pointing pu1_ctb_qp_map to cur 4x4 row*/
    pi1_ctb_qp_map_tile = pi1_ctb_tile_qp + u4_qp_buffer_stride;

    /* This i1_last_cu_qp will be conditionally overwritten later */
    i1_last_cu_qp = ps_ctxt->i4_frame_qp;

    /* -- Loop over all the CTBs in a CTB-row for populating the Qp-map ----- */
    for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
    {
        WORD32 cu_ctr;
        cu_enc_loop_out_t *ps_curr_cu;

        /* Update i1_last_cu_qp based on CTB's position in tile */
        update_last_coded_cu_qp(
            (ps_deblk_ctb_row_params->pi1_ctb_row_qp + i4_offset_for_last_cu_qp),
            ps_ctxt->i1_entropy_coding_sync_enabled_flag,
            ps_frm_ctb_prms,
            ps_ctxt->i4_frame_qp,
            vert_ctr,
            ctb_ctr,
            &i1_last_cu_qp);

        /* store the pointer of first cu of current ctb */
        ps_curr_cu = ps_ctb_out->ps_enc_cu;

        /* --------- loop over all the CUs in the CTB --------------- */
        for(cu_ctr = 0; cu_ctr < ps_ctb_out->u1_num_cus_in_ctb; cu_ctr++)
        {
            UWORD8 u1_vert_4x4, u1_horz_4x4;  //for_loop counters
            WORD8 *pi1_cu_qp_map;

            WORD8 i1_qp, i1_qp_left, i1_qp_top;

            pi1_cu_qp_map = pi1_ctb_qp_map_tile +
                            (ps_curr_cu->b3_cu_pos_y * 2) * u4_qp_buffer_stride +
                            (ps_curr_cu->b3_cu_pos_x * 2);

            /*If the current CU is coded in skip_mode/zero_CBF then
            for deblocking, Qp of the previously coded CU will be used*/
            if(ps_curr_cu->b1_skip_flag || ps_curr_cu->b1_no_residual_syntax_flag)
            {
                if(0 == ps_curr_cu->b3_cu_pos_x)
                    i1_qp_left = i1_last_cu_qp;
                else
                    i1_qp_left = *(pi1_cu_qp_map - 1);

                if(0 == ps_curr_cu->b3_cu_pos_y)
                    i1_qp_top = i1_last_cu_qp;
                else
                    i1_qp_top = *(pi1_cu_qp_map - u4_qp_buffer_stride);

                i1_qp = (i1_qp_left + i1_qp_top + 1) / 2;

                if(0 == ps_curr_cu->b1_first_cu_in_qg)
                {
                    i1_qp = i1_last_cu_qp;
                }
            }
            else
            {
                i1_qp = ps_curr_cu->i1_cu_qp;
            }

            i1_last_cu_qp = i1_qp;

            /*---- Loop for populating Qp map for the current CU -------*/
            for(u1_vert_4x4 = 0; u1_vert_4x4 < (ps_curr_cu->b4_cu_size * 2); u1_vert_4x4++)
            {
                for(u1_horz_4x4 = 0; u1_horz_4x4 < (ps_curr_cu->b4_cu_size * 2); u1_horz_4x4++)
                {
                    pi1_cu_qp_map[u1_horz_4x4] = i1_qp;
                }
                pi1_cu_qp_map += u4_qp_buffer_stride;
            }
            /*Update Qp-map ptr. Qp map is at 4x4 level but b4_cu_size is at 8x8 level*/
            ps_curr_cu++;
        }
        pi1_ctb_qp_map_tile += (ps_frm_ctb_prms->i4_ctb_size / 4);  //one qp per 4x4 block.
        ps_ctb_out++;

    }  //for(ctb_ctr = 0; ctb_ctr < num_ctbs_horz; ctb_ctr++)

    /*fill into the top4x4_array Qp for the lower CTB-row from bottom part of cur CTB row*/
    memcpy(
        (pi1_qp_top_4x4_ctb_row + vert_ctr * u4_qp_top_4x4_buf_strd + tile_qp_offset),
        (pi1_ctb_tile_qp + (ps_frm_ctb_prms->i4_ctb_size / 4) * u4_qp_buffer_stride),
        tile_qp_size);
}

/**
*******************************************************************************
*
* @brief
*   Deblock CTB level function.
*
* @par Description:
*   For a given CTB, deblocking on both vertical and
*   horizontal edges is done. Both the luma and chroma
*   blocks are processed
*
* @param[in]
*   ps_deblk:   Pointer to the deblock context
*   last_col:   if the CTB is the last CTB of current CTB-row value is 1 else 0
*   ps_deblk_ctb_row_params: deblk ctb row params
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/
void ihevce_deblk_ctb(
    deblk_ctb_params_t *ps_deblk, WORD32 last_col, deblk_ctbrow_prms_t *ps_deblk_ctb_row_params)
{
    WORD32 ctb_size;
    UWORD32 u4_bs;
    WORD32 bs_lz; /*Leading zeros in boundary strength*/
    WORD32 qp_p, qp_q;
    UWORD8 *pu1_src;
    UWORD8 *pu1_src_uv;
    UWORD8 *pu1_curr_src;
    WORD32 col_size;
    WORD32 col, row, i4_edge_count;
    WORD32 num_columns_for_vert_filt;
    WORD32 num_blks_for_vert_filt;
    WORD32 num_rows_for_horz_filt;

    ihevc_deblk_chroma_horz_ft *pf_deblk_chroma_horz;
    ihevc_deblk_chroma_horz_ft *pf_deblk_chroma_vert;

    /* Filter flags are packed along with the qp info.
    6 out of the 8 bits correspond to qp and 1 to filter flag. */
    /* filter_p and filter_q are initialized to 1.
    They are to be extracted along with the qp info. */
    WORD32 filter_p, filter_q;
    WORD8 *pi1_ctb_row_qp_p, *pi1_ctb_row_qp_temp;
    WORD8 *pi1_ctb_row_qp_q;

    func_selector_t *ps_func_slector = ps_deblk->ps_func_selector;

    WORD32 left_luma_edge_filter_flag = ps_deblk->i4_deblock_left_ctb_edge;
    WORD32 top_luma_edge_filter_flag = ps_deblk->i4_deblock_top_ctb_edge;
    WORD32 left_chroma_edge_filter_flag = ps_deblk->i4_deblock_left_ctb_edge;
    WORD32 top_chroma_edge_filter_flag = ps_deblk->i4_deblock_top_ctb_edge;
    UWORD32 *bs_vert = ps_deblk_ctb_row_params->pu4_ctb_row_bs_vert;
    UWORD32 *bs_horz = ps_deblk_ctb_row_params->pu4_ctb_row_bs_horz;
    UWORD32 *bs_vert_uv = bs_vert;
    UWORD32 *bs_horz_uv = bs_horz;
    UWORD32 u4_qp_buffer_stride = ps_deblk_ctb_row_params->u4_qp_buffer_stride;
    UWORD8 u1_is_422 = (ps_deblk->u1_chroma_array_type == 2);

    if(u1_is_422)
    {
        pf_deblk_chroma_horz = ps_func_slector->ihevc_deblk_422chroma_horz_fptr;
        pf_deblk_chroma_vert = ps_func_slector->ihevc_deblk_422chroma_vert_fptr;
    }
    else
    {
        pf_deblk_chroma_horz = ps_func_slector->ihevc_deblk_chroma_horz_fptr;
        pf_deblk_chroma_vert = ps_func_slector->ihevc_deblk_chroma_vert_fptr;
    }

    ctb_size = ps_deblk->i4_ctb_size;

    /* The PCM filter flag and bypass trans flag are always set to 1 in encoder profile */
    /* Can be removed during optimization */
    filter_q = 1;
    filter_p = 1;

    //////////////////////////////////////////////////////////////////////////////
    /* Luma Veritcal Edge */
    pu1_src = ps_deblk->pu1_ctb_y;
    pi1_ctb_row_qp_temp = ps_deblk_ctb_row_params->pi1_ctb_row_qp + u4_qp_buffer_stride;
    num_columns_for_vert_filt = ctb_size / 8;
    num_blks_for_vert_filt = ctb_size / 4;

    for(i4_edge_count = 0; i4_edge_count < num_columns_for_vert_filt; i4_edge_count++)
    {
        u4_bs = *bs_vert;
        /* get the current 4x4 vertical pointer */
        pu1_curr_src = pu1_src;
        pi1_ctb_row_qp_q = pi1_ctb_row_qp_temp + (i4_edge_count << 1);

        /* If the current edge is not the 1st edge of frame or slice */
        if(1 == left_luma_edge_filter_flag)
        {
            for(row = 0; row < num_blks_for_vert_filt;)
            {
                bs_lz = CLZ(u4_bs) >> 1;
                /* If BS = 0, skip the egde filtering */
                if(0 != bs_lz)
                {
                    u4_bs = u4_bs << (bs_lz << 1);
                    pu1_curr_src += ((bs_lz << 2) * ps_deblk->i4_luma_pic_stride);
                    pi1_ctb_row_qp_q += (bs_lz * u4_qp_buffer_stride);
                    row += bs_lz;
                    continue;
                }
                qp_p = *(pi1_ctb_row_qp_q - 1);
                qp_q = *pi1_ctb_row_qp_q;

                ps_func_slector->ihevc_deblk_luma_vert_fptr(
                    pu1_curr_src,
                    ps_deblk->i4_luma_pic_stride,
                    (u4_bs >> 30), /* bits 31 and 30 are extracted */
                    qp_p,
                    qp_q,
                    ps_deblk->i4_beta_offset_div2,
                    ps_deblk->i4_tc_offset_div2,
                    filter_p,
                    filter_q);

                u4_bs = u4_bs << 2;
                pu1_curr_src += (ps_deblk->i4_luma_pic_stride << 2);
                pi1_ctb_row_qp_q += u4_qp_buffer_stride;
                row++;
            }
        }

        /* Increment the boundary strength and src pointer for the next column */
        bs_vert += 1;
        pu1_src += 8;

        /* Enable for the next edges of ctb*/
        left_luma_edge_filter_flag = 1;
    }

    //////////////////////////////////////////////////////////////////////////////
    /* Chroma Veritcal Edge */
    pu1_src_uv = ps_deblk->pu1_ctb_uv;
    pi1_ctb_row_qp_temp = ps_deblk_ctb_row_params->pi1_ctb_row_qp + u4_qp_buffer_stride;

    /* Column spacing is 4 for each chroma component */
    /* and hence 8 when they are interleaved. */
    /* But, only those columns with a x co-ordinate */
    /* that is divisiblee by 8 are filtered */
    /* Hence, denominator is 16 */
    num_columns_for_vert_filt = ctb_size / 16;
    /* blk_size is 4 and chroma_ctb_height is ctb_size/2 */
    num_blks_for_vert_filt = (0 == u1_is_422) ? (ctb_size / 2) / 4 : (ctb_size) / 4;

    for(i4_edge_count = 0; i4_edge_count < num_columns_for_vert_filt; i4_edge_count++)
    {
        /* Every alternate boundary strength value is used for 420 chroma */
        u4_bs = *(bs_vert_uv) & ((0 == u1_is_422) ? 0x88888888 : 0xaaaaaaaa);
        pu1_curr_src = pu1_src_uv;
        pi1_ctb_row_qp_q = pi1_ctb_row_qp_temp + (i4_edge_count << 2);

        /* If the current edge is not the 1st edge of frame or slice */
        if(1 == left_chroma_edge_filter_flag)
        {
            /* Each 'bs' is 2 bits long */
            /* The divby4 in 420 is */
            /* necessitated by the fact that */
            /* chroma ctb_ht is half that of luma */
            WORD32 i4_log2_num_bits_per_bs = ((0 == u1_is_422) + 1);
            /* i4_sub_heightC = 2 for 420 */
            /* i4_sub_heightC = 1 for 422 */
            WORD32 i4_sub_heightC = i4_log2_num_bits_per_bs;

            for(row = 0; row < num_blks_for_vert_filt;)
            {
                bs_lz = CLZ(u4_bs) >> i4_log2_num_bits_per_bs;

                /* If BS = 0, skip the egde filtering */
                if(0 != bs_lz)
                {
                    row += bs_lz;
                    u4_bs = u4_bs << (bs_lz << i4_log2_num_bits_per_bs);
                    /* '<<2' because of blk_size being 4x4 */
                    pu1_curr_src += ((bs_lz << 2) * ps_deblk->i4_chroma_pic_stride);

                    /* In 420, every alternate QP row is skipped, because chroma height */
                    /* In 422, no row is skipped */
                    pi1_ctb_row_qp_q += ((u4_qp_buffer_stride << (i4_sub_heightC - 1)) * bs_lz);

                    continue;
                }

                qp_p = *(pi1_ctb_row_qp_q - i4_sub_heightC);
                qp_q = *pi1_ctb_row_qp_q;

                pf_deblk_chroma_vert(
                    pu1_curr_src,
                    ps_deblk->i4_chroma_pic_stride,
                    qp_p,
                    qp_q,
                    ps_deblk->i4_cb_qp_indx_offset,
                    ps_deblk->i4_cr_qp_indx_offset,
                    ps_deblk->i4_tc_offset_div2,
                    filter_p,
                    filter_q);

                u4_bs = u4_bs << (1 << i4_log2_num_bits_per_bs);
                pu1_curr_src += (ps_deblk->i4_chroma_pic_stride << 2);
                pi1_ctb_row_qp_q += (u4_qp_buffer_stride << (i4_sub_heightC - 1));
                row++;
            }
        }
        /* Increment the boundary strength by 2 and src pointer for the next column */
        /* As the edge filtering happens for alternate column */
        bs_vert_uv += 2;
        pu1_src_uv += 16;
        left_chroma_edge_filter_flag = 1;
    }

    //////////////////////////////////////////////////////////////////////////////

    /* Luma Horizontal Edge */
    pu1_src = ps_deblk->pu1_ctb_y;
    col_size = ctb_size / 4;

    /* If the ctb is the 1st ctb of row,                     */
    /* Decrement the loop count to exclude filtering of last 4 pixels */
    /* else shift the src pointer by 4 pixels to do filtering for shifted ctb */
    if(ps_deblk->i4_deblock_left_ctb_edge == 1)
    {
        pu1_src -= 4;
        /*If the ctb is at the horizonatl end of PIC*/
        /* Increase the column size to filter last 4 pixels */
        col_size += last_col;
    }
    else if(!last_col)
    {
        col_size -= 1;
    }
    {
        UWORD8 *pu1_src_temp = pu1_src;
        //pu1_ctb_row_qp_p and pu1_ctb_row_qp_q point to alternate rows
        pi1_ctb_row_qp_p = ps_deblk_ctb_row_params->pi1_ctb_row_qp;

        num_rows_for_horz_filt = ctb_size / 8;

        for(i4_edge_count = 0; i4_edge_count < num_rows_for_horz_filt; i4_edge_count++)
        {
            WORD32 col_size_temp = col_size;
            pi1_ctb_row_qp_q = pi1_ctb_row_qp_p + u4_qp_buffer_stride;
            pu1_src = pu1_src_temp + (i4_edge_count * 8 * ps_deblk->i4_luma_pic_stride);

            if(1 == top_luma_edge_filter_flag)
            {
                //Deblock the last vertical_4x4_column of previous CTB
                if(ps_deblk->i4_deblock_left_ctb_edge == 1)
                {
                    u4_bs = ps_deblk->au1_prev_bs[i4_edge_count] & 0x3;
                    if(u4_bs != 0)
                    {
                        qp_p = *(pi1_ctb_row_qp_p - 1);
                        qp_q = *(pi1_ctb_row_qp_q - 1);

                        ps_func_slector->ihevc_deblk_luma_horz_fptr(
                            pu1_src,
                            ps_deblk->i4_luma_pic_stride,
                            u4_bs,
                            qp_p,
                            qp_q,
                            ps_deblk->i4_beta_offset_div2,
                            ps_deblk->i4_tc_offset_div2,
                            1,
                            1);
                    }

                    pu1_src += 4;
                    col_size_temp--;
                }
                //Start deblocking current CTB
                u4_bs = *(bs_horz);

                for(col = 0; col < col_size_temp;)
                {
                    bs_lz = CLZ(u4_bs) >> 1;
                    if(0 != bs_lz)
                    {
                        u4_bs = u4_bs << (bs_lz << 1);
                        pu1_src += 4 * bs_lz;
                        col += bs_lz;
                        continue;
                    }
                    qp_p = *(pi1_ctb_row_qp_p + col);
                    qp_q = *(pi1_ctb_row_qp_q + col);

                    ps_func_slector->ihevc_deblk_luma_horz_fptr(
                        pu1_src,
                        ps_deblk->i4_luma_pic_stride,
                        u4_bs >> (sizeof(u4_bs) * 8 - 2),
                        qp_p,
                        qp_q,
                        ps_deblk->i4_beta_offset_div2,
                        ps_deblk->i4_tc_offset_div2,
                        filter_p,
                        filter_q);

                    pu1_src += 4;
                    u4_bs = u4_bs << 2;
                    col++;
                }
                //Store the last vertical_4x4 column of CTB's info for next CTB deblocking
                u4_bs = *bs_horz;
                ps_deblk->au1_prev_bs[i4_edge_count] =
                    (UWORD8)(((u4_bs << ((ctb_size >> 1) - 2))) >> 30);
            }
            bs_horz += 1;
            pi1_ctb_row_qp_p += (u4_qp_buffer_stride << 1);
            top_luma_edge_filter_flag = 1;
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    /* Chroma Horizontal Edge */
    pu1_src_uv = ps_deblk->pu1_ctb_uv;
    col_size = ctb_size / 8;

    /* If the ctb is the 1st ctb of row,                     */
    /* Decrement the loop count to exclude filtering of last 4 pixels */
    /* else shift the src pointer by 8 (uv) pixels to do filtering for shifted ctb */
    if(ps_deblk->i4_deblock_left_ctb_edge == 1)
    {
        pu1_src_uv -= 8;

        /*If the ctb is at the horizonatl end of PIC*/
        /* Increase the column size to filter last 8 (uv) pixels */
        col_size += last_col;
    }
    else if(!last_col)
    {
        col_size--;
    }

    {
        UWORD8 *pu1_src_temp = pu1_src_uv;

        //pu1_ctb_row_qp_p and pu1_ctb_row_qp_q point to alternate rows
        pi1_ctb_row_qp_p = ps_deblk_ctb_row_params->pi1_ctb_row_qp;
        num_rows_for_horz_filt = ctb_size / ((0 == u1_is_422) ? 16 : 8);

        for(i4_edge_count = 0; i4_edge_count < num_rows_for_horz_filt; i4_edge_count++)
        {
            WORD32 col_size_temp = col_size;

            pi1_ctb_row_qp_q = pi1_ctb_row_qp_p + u4_qp_buffer_stride;
            pu1_src_uv = pu1_src_temp + (i4_edge_count * 8 * ps_deblk->i4_chroma_pic_stride);

            if(1 == top_chroma_edge_filter_flag)
            {
                //Deblock the last vertical _4x4_column of previous CTB
                if(ps_deblk->i4_deblock_left_ctb_edge == 1)
                {
                    u4_bs = ps_deblk->au1_prev_bs_uv[i4_edge_count] & 0x2;

                    if(u4_bs == 2)
                    {
                        qp_p = *(pi1_ctb_row_qp_p - 1);
                        qp_q = *(pi1_ctb_row_qp_q - 1);

                        pf_deblk_chroma_horz(
                            pu1_src_uv,
                            ps_deblk->i4_chroma_pic_stride,
                            qp_p,
                            qp_q,
                            ps_deblk->i4_cb_qp_indx_offset,
                            ps_deblk->i4_cr_qp_indx_offset,
                            ps_deblk->i4_tc_offset_div2,
                            1,
                            1);
                    }

                    pu1_src_uv += 8;
                    col_size_temp--;
                }

                //Start deblocking current CTB
                u4_bs = *(bs_horz_uv)&0x88888888;

                for(col = 0; col < col_size_temp;)
                {
                    bs_lz = CLZ(u4_bs) >> 2;

                    if(0 != bs_lz)
                    {
                        u4_bs = u4_bs << (bs_lz << 2);
                        pu1_src_uv += (8 * bs_lz);

                        col += bs_lz;
                        continue;
                    }

                    qp_p = *(pi1_ctb_row_qp_p + (col << 1));
                    qp_q = *(pi1_ctb_row_qp_q + (col << 1));

                    pf_deblk_chroma_horz(
                        pu1_src_uv,
                        ps_deblk->i4_chroma_pic_stride,
                        qp_p,
                        qp_q,
                        ps_deblk->i4_cb_qp_indx_offset,
                        ps_deblk->i4_cr_qp_indx_offset,
                        ps_deblk->i4_tc_offset_div2,
                        filter_p,
                        filter_q);

                    pu1_src_uv += 8;
                    u4_bs = u4_bs << 4;
                    col++;
                }

                //Store the last vertical_4x4 column of CTB's info for next CTB deblocking
                u4_bs = *bs_horz_uv;
                ps_deblk->au1_prev_bs_uv[i4_edge_count] =
                    (UWORD8)(((u4_bs << ((ctb_size >> 1) - 4))) >> 30);
            }

            bs_horz_uv += ((0 == u1_is_422) + 1);
            pi1_ctb_row_qp_p += (u4_qp_buffer_stride << ((0 == u1_is_422) + 1));
            top_chroma_edge_filter_flag = 1;
        }
    }

    return;
}
