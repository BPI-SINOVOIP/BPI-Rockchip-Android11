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
*  ihevce_sao.c
*
* @brief
*  Contains definition for the ctb level sao function
*
* @author
*  Ittiam
*
* @par List of Functions:
*  ihevce_sao_set_avilability()
*  ihevce_sao_ctb()
*  ihevce_sao_analyse()
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
#include "ihevce_cabac_rdo.h"
#include "ihevce_sao.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief
*     ihevce_sao_set_avilability
*
* @par Description:
*     Sets the availability flag for SAO.
*
* @param[in]
*   ps_sao_ctxt:   Pointer to SAO context
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/
void ihevce_sao_set_avilability(
    UWORD8 *pu1_avail, sao_ctxt_t *ps_sao_ctxt, ihevce_tile_params_t *ps_tile_params)
{
    WORD32 i;

    WORD32 ctb_x_pos = ps_sao_ctxt->i4_ctb_x;
    WORD32 ctb_y_pos = ps_sao_ctxt->i4_ctb_y;

    for(i = 0; i < 8; i++)
    {
        pu1_avail[i] = 255;
    }

    /* SAO_note_01: If the CTB lies on a tile or a slice boundary and
    in-loop filtering is enabled at tile and slice boundary, then SAO must
    be performed at tile/slice boundaries also.
    Hence the boundary checks should be based on frame position of CTB
    rather than s_ctb_nbr_avail_flags.u1_left_avail flags.
    Search for <SAO_note_01> in workspace to know more */
    /* Availaibility flags for first col*/
    if(ctb_x_pos == ps_tile_params->i4_first_ctb_x)
    {
        pu1_avail[0] = 0;
        pu1_avail[4] = 0;
        pu1_avail[6] = 0;
    }

    /* Availaibility flags for last col*/
    if((ctb_x_pos + 1) ==
       (ps_tile_params->i4_first_ctb_x + ps_tile_params->i4_curr_tile_wd_in_ctb_unit))
    {
        pu1_avail[1] = 0;
        pu1_avail[5] = 0;
        pu1_avail[7] = 0;
    }

    /* Availaibility flags for first row*/
    if(ctb_y_pos == ps_tile_params->i4_first_ctb_y)
    {
        pu1_avail[2] = 0;
        pu1_avail[4] = 0;
        pu1_avail[5] = 0;
    }

    /* Availaibility flags for last row*/
    if((ctb_y_pos + 1) ==
       (ps_tile_params->i4_first_ctb_y + ps_tile_params->i4_curr_tile_ht_in_ctb_unit))
    {
        pu1_avail[3] = 0;
        pu1_avail[6] = 0;
        pu1_avail[7] = 0;
    }
}

/**
*******************************************************************************
*
* @brief
*   Sao CTB level function.
*
* @par Description:
*   For a given CTB, sao is done. Both the luma and chroma
*   blocks are processed
*
* @param[in]
*   ps_sao_ctxt:   Pointer to SAO context
*
* @returns
*
* @remarks
*  None
*
*******************************************************************************
*/
void ihevce_sao_ctb(sao_ctxt_t *ps_sao_ctxt, ihevce_tile_params_t *ps_tile_params)
{
    sao_enc_t *ps_sao;
    UWORD8 u1_src_top_left_luma, u1_src_top_left_chroma[2];
    UWORD8 *pu1_src_left_luma_buf, *pu1_src_top_luma_buf;
    UWORD8 *pu1_src_left_chroma_buf, *pu1_src_top_chroma_buf;
    UWORD8 *pu1_src_luma, *pu1_src_chroma;
    WORD32 luma_src_stride, ctb_size;
    WORD32 chroma_src_stride;
    UWORD8 au1_avail_luma[8], au1_avail_chroma[8];
    WORD32 sao_blk_wd, sao_blk_ht, sao_wd_chroma, sao_ht_chroma;
    UWORD8 *pu1_top_left_luma, *pu1_top_left_chroma;
    UWORD8 *pu1_src_bot_left_luma, *pu1_src_top_right_luma;
    UWORD8 *pu1_src_bot_left_chroma, *pu1_src_top_right_chroma;
    UWORD8 u1_is_422 = (ps_sao_ctxt->ps_sps->i1_chroma_format_idc == 2);

    ps_sao = ps_sao_ctxt->ps_sao;

    ASSERT(
        (abs(ps_sao->u1_y_offset[1]) <= 7) && (abs(ps_sao->u1_y_offset[2]) <= 7) &&
        (abs(ps_sao->u1_y_offset[3]) <= 7) && (abs(ps_sao->u1_y_offset[4]) <= 7));
    ASSERT(
        (abs(ps_sao->u1_cb_offset[1]) <= 7) && (abs(ps_sao->u1_cb_offset[2]) <= 7) &&
        (abs(ps_sao->u1_cb_offset[3]) <= 7) && (abs(ps_sao->u1_cb_offset[4]) <= 7));
    ASSERT(
        (abs(ps_sao->u1_cr_offset[1]) <= 7) && (abs(ps_sao->u1_cr_offset[2]) <= 7) &&
        (abs(ps_sao->u1_cr_offset[3]) <= 7) && (abs(ps_sao->u1_cr_offset[4]) <= 7));
    ASSERT(
        (ps_sao->b5_y_band_pos <= 28) && (ps_sao->b5_cb_band_pos <= 28) &&
        (ps_sao->b5_cr_band_pos <= 28));

    if(ps_sao_ctxt->i1_slice_sao_luma_flag)
    {
        /*initialize the src pointer to current row*/
        luma_src_stride = ps_sao_ctxt->i4_cur_luma_recon_stride;

        ctb_size = ps_sao_ctxt->i4_ctb_size;

        /* 1 extra byte in top buf stride for top left of 1st ctb of every row*/
        ps_sao->u1_y_offset[0] = 0; /* 0th element is not being used  */
        sao_blk_wd = ps_sao_ctxt->i4_sao_blk_wd;
        sao_blk_ht = ps_sao_ctxt->i4_sao_blk_ht;

        pu1_src_luma = ps_sao_ctxt->pu1_cur_luma_recon_buf;
        /* Pointer to the top luma buffer corresponding to the current ctb row*/
        pu1_src_top_luma_buf = ps_sao_ctxt->pu1_curr_sao_src_top_luma;

        /* Pointer to left luma buffer corresponding to the current ctb row*/
        pu1_src_left_luma_buf = ps_sao_ctxt->au1_left_luma_scratch;

        /* Pointer to the top right luma buffer corresponding to the current ctb row*/
        pu1_src_top_right_luma = pu1_src_top_luma_buf /*- top_buf_stide*/ + sao_blk_wd;

        /* Pointer to the bottom left luma buffer corresponding to the current ctb row*/
        pu1_src_bot_left_luma =
            ps_sao_ctxt->pu1_frm_luma_recon_buf + ctb_size * ps_sao_ctxt->i4_frm_luma_recon_stride -
            1 + (ps_sao_ctxt->i4_frm_luma_recon_stride * ps_sao_ctxt->i4_ctb_y * ctb_size) +
            (ps_sao_ctxt->i4_ctb_x * ctb_size); /* Bottom left*/

        /* Back up the top left pixel for (x+1, y+1)th ctb*/
        u1_src_top_left_luma = *(pu1_src_top_luma_buf + sao_blk_wd - 1);
        pu1_top_left_luma = pu1_src_top_luma_buf - 1;

        if(SAO_BAND == ps_sao->b3_y_type_idx)
        {
            ihevc_sao_band_offset_luma(
                pu1_src_luma,
                luma_src_stride,
                pu1_src_left_luma_buf, /* Pass the pointer to the left luma buffer backed up in the (x-1,y)th ctb */
                pu1_src_top_luma_buf, /* Pass the ptr to the top luma buf backed up in the (x,y-1)th ctb */
                pu1_src_top_luma_buf - 1, /* Top left*/
                ps_sao->b5_y_band_pos,
                ps_sao->u1_y_offset,
                sao_blk_wd,
                sao_blk_ht);

            if((ps_sao_ctxt->i4_ctb_y > 0))
            {
                *(pu1_src_top_luma_buf + sao_blk_wd - 1) = u1_src_top_left_luma;
            }
        }
        else if(ps_sao->b3_y_type_idx >= SAO_EDGE_0_DEG)
        {
            /*In case of edge offset, 1st and 2nd offsets are always inferred as offsets
            * corresponding to EO category 1 and 2 which should be always positive
            * And 3rd and 4th offsets are always inferred as offsets corresponding to
            * EO category 3 and 4 which should be negative for all the EO classes(or EO typeidx)
            */
            // clang-format off
            ASSERT((ps_sao->u1_y_offset[1] >= 0) && (ps_sao->u1_y_offset[2] >= 0));
            ASSERT((ps_sao->u1_y_offset[3] <= 0) && (ps_sao->u1_y_offset[4] <= 0));
            // clang-format on

            ihevce_sao_set_avilability(au1_avail_luma, ps_sao_ctxt, ps_tile_params);

            ps_sao_ctxt->apf_sao_luma[ps_sao->b3_y_type_idx - 2](
                pu1_src_luma,
                luma_src_stride,
                pu1_src_left_luma_buf, /* Pass the pointer to the left luma buffer backed up in the (x-1,y)th ctb */
                pu1_src_top_luma_buf, /* Pass the ptr to the top luma buf backed up in the (x,y-1)th ctb */
                pu1_top_left_luma, /* Top left*/
                pu1_src_top_right_luma, /* Top right*/
                pu1_src_bot_left_luma, /* Bottom left*/
                au1_avail_luma,
                ps_sao->u1_y_offset,
                sao_blk_wd,
                sao_blk_ht);

            if((ps_sao_ctxt->i4_ctb_y > 0))
            {
                *(pu1_src_top_luma_buf + sao_blk_wd - 1) = u1_src_top_left_luma;
            }
        }
    }

    if(ps_sao_ctxt->i1_slice_sao_chroma_flag)
    {
        /*initialize the src pointer to current row*/
        chroma_src_stride = ps_sao_ctxt->i4_cur_chroma_recon_stride;
        ctb_size = ps_sao_ctxt->i4_ctb_size;

        /* 1 extra byte in top buf stride for top left of 1st ctb of every row*/
        //top_buf_stide = ps_sao_ctxt->u4_ctb_aligned_wd + 2;
        ps_sao->u1_cb_offset[0] = 0; /* 0th element is not used  */
        ps_sao->u1_cr_offset[0] = 0;
        sao_wd_chroma = ps_sao_ctxt->i4_sao_blk_wd;
        sao_ht_chroma = ps_sao_ctxt->i4_sao_blk_ht / (!u1_is_422 + 1);

        pu1_src_chroma = ps_sao_ctxt->pu1_cur_chroma_recon_buf;
        /* Pointer to the top luma buffer corresponding to the current ctb row*/
        pu1_src_top_chroma_buf = ps_sao_ctxt->pu1_curr_sao_src_top_chroma;
        // clang-format off
        /* Pointer to left luma buffer corresponding to the current ctb row*/
        pu1_src_left_chroma_buf = ps_sao_ctxt->au1_left_chroma_scratch;  //ps_sao_ctxt->au1_sao_src_left_chroma;
        // clang-format on
        /* Pointer to the top right chroma buffer corresponding to the current ctb row*/
        pu1_src_top_right_chroma = pu1_src_top_chroma_buf /*- top_buf_stide*/ + sao_wd_chroma;

        /* Pointer to the bottom left luma buffer corresponding to the current ctb row*/
        pu1_src_bot_left_chroma =
            ps_sao_ctxt->pu1_frm_chroma_recon_buf +
            (ctb_size >> !u1_is_422) * ps_sao_ctxt->i4_frm_chroma_recon_stride - 2 +
            (ps_sao_ctxt->i4_frm_chroma_recon_stride * ps_sao_ctxt->i4_ctb_y *
             (ctb_size >> !u1_is_422)) +
            (ps_sao_ctxt->i4_ctb_x * ctb_size); /* Bottom left*/

        /* Back up the top left pixel for (x+1, y+1)th ctb*/
        u1_src_top_left_chroma[0] = *(pu1_src_top_chroma_buf + sao_wd_chroma - 2);
        u1_src_top_left_chroma[1] = *(pu1_src_top_chroma_buf + sao_wd_chroma - 1);
        pu1_top_left_chroma = pu1_src_top_chroma_buf - 2;

        if(SAO_BAND == ps_sao->b3_cb_type_idx)
        {
            ihevc_sao_band_offset_chroma(
                pu1_src_chroma,
                chroma_src_stride,
                pu1_src_left_chroma_buf, /* Pass the pointer to the left luma buffer backed up in the (x-1,y)th ctb */
                pu1_src_top_chroma_buf, /* Pass the ptr to the top luma buf backed up in the (x,y-1)th ctb */
                pu1_top_left_chroma, /* Top left*/
                ps_sao->b5_cb_band_pos,
                ps_sao->b5_cr_band_pos,
                ps_sao->u1_cb_offset,
                ps_sao->u1_cr_offset,
                sao_wd_chroma,
                sao_ht_chroma);

            if((ps_sao_ctxt->i4_ctb_y > 0))
            {
                *(pu1_src_top_chroma_buf + sao_wd_chroma - 2) = u1_src_top_left_chroma[0];
                *(pu1_src_top_chroma_buf + sao_wd_chroma - 1) = u1_src_top_left_chroma[1];
            }
        }
        else if(ps_sao->b3_cb_type_idx >= SAO_EDGE_0_DEG)
        {
            /*In case of edge offset, 1st and 2nd offsets are always inferred as offsets
            * corresponding to EO category 1 and 2 which should be always positive
            * And 3rd and 4th offsets are always inferred as offsets corresponding to
            * EO category 3 and 4 which should be negative for all the EO classes(or EO typeidx)
            */
            ASSERT((ps_sao->u1_cb_offset[1] >= 0) && (ps_sao->u1_cb_offset[2] >= 0));
            ASSERT((ps_sao->u1_cb_offset[3] <= 0) && (ps_sao->u1_cb_offset[4] <= 0));

            ASSERT((ps_sao->u1_cr_offset[1] >= 0) && (ps_sao->u1_cr_offset[2] >= 0));
            ASSERT((ps_sao->u1_cr_offset[3] <= 0) && (ps_sao->u1_cr_offset[4] <= 0));

            ihevce_sao_set_avilability(au1_avail_chroma, ps_sao_ctxt, ps_tile_params);

            ps_sao_ctxt->apf_sao_chroma[ps_sao->b3_cb_type_idx - 2](
                pu1_src_chroma,
                chroma_src_stride,
                pu1_src_left_chroma_buf, /* Pass the pointer to the left luma buffer backed up in the (x-1,y)th ctb */
                pu1_src_top_chroma_buf, /* Pass the ptr to the top luma buf backed up in the (x,y-1)th ctb */
                pu1_top_left_chroma, /* Top left*/
                pu1_src_top_right_chroma, /* Top right*/
                pu1_src_bot_left_chroma, /* Bottom left*/
                au1_avail_chroma,
                ps_sao->u1_cb_offset,
                ps_sao->u1_cr_offset,
                sao_wd_chroma,
                sao_ht_chroma);

            if((ps_sao_ctxt->i4_ctb_y > 0))
            {
                *(pu1_src_top_chroma_buf + sao_wd_chroma - 2) = u1_src_top_left_chroma[0];
                *(pu1_src_top_chroma_buf + sao_wd_chroma - 1) = u1_src_top_left_chroma[1];
            }
        }
    }
}

/**
*******************************************************************************
*
* @brief
*   CTB level function to do SAO analysis.
*
* @par Description:
*   For a given CTB, sao analysis is done for both luma and chroma.
*
*
* @param[in]
*   ps_sao_ctxt:   Pointer to SAO context
*   ps_ctb_enc_loop_out : pointer to ctb level output structure from enc loop
*
* @returns
*
* @remarks
*  None
*
* @Assumptions:
*   1) Initial Cabac state for current ctb to be sao'ed (i.e (x-1,y-1)th ctb) is assumed to be
*      almost same as cabac state of (x,y)th ctb.
*   2) Distortion is calculated in spatial domain but lamda used to calculate the cost is
*      in freq domain.
*******************************************************************************
*/
void ihevce_sao_analyse(
    sao_ctxt_t *ps_sao_ctxt,
    ctb_enc_loop_out_t *ps_ctb_enc_loop_out,
    UWORD32 *pu4_frame_rdopt_header_bits,
    ihevce_tile_params_t *ps_tile_params)
{
    UWORD8 *pu1_luma_scratch_buf;
    UWORD8 *pu1_chroma_scratch_buf;
    UWORD8 *pu1_src_luma, *pu1_recon_luma;
    UWORD8 *pu1_src_chroma, *pu1_recon_chroma;
    WORD32 luma_src_stride, luma_recon_stride, ctb_size, ctb_wd, ctb_ht;
    WORD32 chroma_src_stride, chroma_recon_stride;
    WORD32 i4_luma_scratch_buf_stride;
    WORD32 i4_chroma_scratch_buf_stride;
    sao_ctxt_t s_sao_ctxt;
    UWORD32 ctb_bits = 0, distortion = 0, curr_cost = 0, best_cost = 0;
    LWORD64 i8_cl_ssd_lambda_qf, i8_cl_ssd_lambda_chroma_qf;
    WORD32 rdo_cand, num_luma_rdo_cand = 0, num_rdo_cand = 0;
    WORD32 curr_buf_idx, best_buf_idx, best_cand_idx;
    WORD32 row;
    WORD32 edgeidx;
    WORD32 acc_error_category[5] = { 0, 0, 0, 0, 0 }, category_count[5] = { 0, 0, 0, 0, 0 };
    sao_enc_t s_best_luma_chroma_cand;
    WORD32 best_ctb_sao_bits = 0;
#if DISABLE_SAO_WHEN_NOISY && !defined(ENC_VER_v2)
    UWORD8 u1_force_no_offset =
        ps_sao_ctxt
            ->ps_ctb_data
                [ps_sao_ctxt->i4_ctb_x + ps_sao_ctxt->i4_ctb_data_stride * ps_sao_ctxt->i4_ctb_y]
            .s_ctb_noise_params.i4_noise_present;
#endif
    UWORD8 u1_is_422 = (ps_sao_ctxt->ps_sps->i1_chroma_format_idc == 2);

    *pu4_frame_rdopt_header_bits = 0;

    ctb_size = ps_sao_ctxt->i4_ctb_size;
    ctb_wd = ps_sao_ctxt->i4_sao_blk_wd;
    ctb_ht = ps_sao_ctxt->i4_sao_blk_ht;

    s_sao_ctxt = ps_sao_ctxt[0];

    /* Memset the best luma_chroma_cand structure to avoid asserts in debug mode*/
    memset(&s_best_luma_chroma_cand, 0, sizeof(sao_enc_t));

    /* Initialize the pointer and strides for luma buffers*/
    pu1_recon_luma = ps_sao_ctxt->pu1_cur_luma_recon_buf;
    luma_recon_stride = ps_sao_ctxt->i4_cur_luma_recon_stride;

    pu1_src_luma = ps_sao_ctxt->pu1_cur_luma_src_buf;
    luma_src_stride = ps_sao_ctxt->i4_cur_luma_src_stride;
    i4_luma_scratch_buf_stride = SCRATCH_BUF_STRIDE;

    /* Initialize the pointer and strides for luma buffers*/
    pu1_recon_chroma = ps_sao_ctxt->pu1_cur_chroma_recon_buf;
    chroma_recon_stride = ps_sao_ctxt->i4_cur_chroma_recon_stride;

    pu1_src_chroma = ps_sao_ctxt->pu1_cur_chroma_src_buf;
    chroma_src_stride = ps_sao_ctxt->i4_cur_chroma_src_stride;
    i4_chroma_scratch_buf_stride = SCRATCH_BUF_STRIDE;

    i8_cl_ssd_lambda_qf = ps_sao_ctxt->i8_cl_ssd_lambda_qf;
    i8_cl_ssd_lambda_chroma_qf = ps_sao_ctxt->i8_cl_ssd_lambda_chroma_qf;

    /*****************************************************/
    /********************RDO FOR LUMA CAND****************/
    /*****************************************************/

#if !DISABLE_SAO_WHEN_NOISY
    if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_luma_flag)
#else
    if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_luma_flag && !u1_force_no_offset)
#endif
    {
        /* Candidate for Edge offset SAO*/
        /* Following is the convention for curr pixel and
        * two neighbouring pixels for 0 deg, 90 deg, 135 deg and 45 deg */
        /*
        * 0 deg :  a c b     90 deg:  a       135 deg: a          45 deg:     a
        *                             c                  c                  c
        *                             b                    b              b
        */

        /* 0 deg SAO CAND*/
        /* Reset the error and edge count*/
        for(edgeidx = 0; edgeidx < 5; edgeidx++)
        {
            acc_error_category[edgeidx] = 0;
            category_count[edgeidx] = 0;
        }

        /* Call the funciton to populate the EO parameter for this ctb for 0 deg EO class*/
        // clang-format off
        ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_get_luma_eo_sao_params(ps_sao_ctxt, SAO_EDGE_0_DEG,
                acc_error_category, category_count);
        // clang-format on
        // clang-format off
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_y_type_idx = SAO_EDGE_0_DEG;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[1] = category_count[0]
                ? (CLIP3(acc_error_category[0] / category_count[0], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[2] = category_count[1]
                ? (CLIP3(acc_error_category[1] / category_count[1], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[3] = category_count[3]
                ? (CLIP3(acc_error_category[3] / category_count[3], -7, 0))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[4] =category_count[4]
                ? (CLIP3(acc_error_category[4] / category_count[4], -7, 0))
                : 0;
        // clang-format on
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_y_band_pos = 0;
        // clang-format off
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cb_type_idx = SAO_NONE;
        // clang-format on
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cb_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cr_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cr_band_pos = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_left_flag = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_up_flag = 0;

        num_luma_rdo_cand++;

        /* 90 degree SAO CAND*/
        for(edgeidx = 0; edgeidx < 5; edgeidx++)
        {
            acc_error_category[edgeidx] = 0;
            category_count[edgeidx] = 0;
        }

        /* Call the funciton to populate the EO parameter for this ctb for 90 deg EO class*/
        // clang-format off
        ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_get_luma_eo_sao_params(ps_sao_ctxt, SAO_EDGE_90_DEG,
                acc_error_category, category_count);

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_y_type_idx = SAO_EDGE_90_DEG;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[1] = category_count[0]
                ? (CLIP3(acc_error_category[0] / category_count[0], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[2] = category_count[1]
                ? (CLIP3(acc_error_category[1] / category_count[1], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[3] = category_count[3]
                ? (CLIP3(acc_error_category[3] / category_count[3], -7, 0))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[4] = category_count[4]
                ? (CLIP3(acc_error_category[4] / category_count[4], -7, 0))
                : 0;
        // clang-format on
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_y_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cb_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cb_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cr_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cr_band_pos = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_left_flag = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_up_flag = 0;

        num_luma_rdo_cand++;

        /* 135 degree SAO CAND*/
        for(edgeidx = 0; edgeidx < 5; edgeidx++)
        {
            acc_error_category[edgeidx] = 0;
            category_count[edgeidx] = 0;
        }

        /* Call the funciton to populate the EO parameter for this ctb for 135 deg EO class*/
        // clang-format off
        ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_get_luma_eo_sao_params(ps_sao_ctxt, SAO_EDGE_135_DEG,
                acc_error_category, category_count);

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_y_type_idx = SAO_EDGE_135_DEG;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[1] = category_count[0]
                ? (CLIP3(acc_error_category[0] / category_count[0], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[2] = category_count[1]
                ? (CLIP3(acc_error_category[1] / category_count[1], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[3] = category_count[3]
                ? (CLIP3(acc_error_category[3] / category_count[3], -7, 0))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[4] = category_count[4]
                ? (CLIP3(acc_error_category[4] / category_count[4], -7, 0))
                : 0;
        // clang-format on
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_y_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cb_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cb_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cr_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cr_band_pos = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_left_flag = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_up_flag = 0;

        num_luma_rdo_cand++;

        /* 45 degree SAO CAND*/
        for(edgeidx = 0; edgeidx < 5; edgeidx++)
        {
            acc_error_category[edgeidx] = 0;
            category_count[edgeidx] = 0;
        }

        /* Call the funciton to populate the EO parameter for this ctb for 45 deg EO class*/
        // clang-format off
        ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_get_luma_eo_sao_params(ps_sao_ctxt, SAO_EDGE_45_DEG,
                acc_error_category, category_count);

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_y_type_idx = SAO_EDGE_45_DEG;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[1] = category_count[0]
                ? (CLIP3(acc_error_category[0] / category_count[0], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[2] = category_count[1]
                ? (CLIP3(acc_error_category[1] / category_count[1], 0, 7))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[3] = category_count[3]
                ? (CLIP3(acc_error_category[3] / category_count[3], -7, 0))
                : 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_y_offset[4] = category_count[4]
                ? (CLIP3(acc_error_category[4] / category_count[4], -7, 0))
                : 0;
        // clang-format on
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_y_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cb_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cb_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cb_band_pos = 0;

        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b3_cr_type_idx = SAO_NONE;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[1] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[2] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[3] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].u1_cr_offset[4] = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b5_cr_band_pos = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_left_flag = 0;
        ps_sao_ctxt->as_sao_rd_cand[num_luma_rdo_cand].b1_sao_merge_up_flag = 0;

        num_luma_rdo_cand++;

        /* First cand will be best cand after 1st iteration*/
        curr_buf_idx = 0;
        best_buf_idx = 1;
        best_cost = 0xFFFFFFFF;
        best_cand_idx = 0;

        /*Back up the top pixels for (x,y+1)th ctb*/
        if(!ps_sao_ctxt->i4_is_last_ctb_row)
        {
            memcpy(
                ps_sao_ctxt->pu1_curr_sao_src_top_luma + ps_sao_ctxt->i4_frm_top_luma_buf_stride,
                pu1_recon_luma + luma_recon_stride * (ctb_size - 1),
                ps_sao_ctxt->i4_sao_blk_wd);
        }

        for(rdo_cand = 0; rdo_cand < num_luma_rdo_cand; rdo_cand++)
        {
            s_sao_ctxt.ps_sao = &ps_sao_ctxt->as_sao_rd_cand[rdo_cand];

            /* This memcpy is required because cabac uses parameters from this structure
            * to evaluate bits and this structure ptr is sent to cabac through
            * "ihevce_cabac_rdo_encode_sao" function
            */
            memcpy(&ps_ctb_enc_loop_out->s_sao, s_sao_ctxt.ps_sao, sizeof(sao_enc_t));

            /* Copy the left pixels to the scratch buffer for evry rdo cand because its
            overwritten by the sao leaf level function for next ctb*/
            memcpy(
                s_sao_ctxt.au1_left_luma_scratch,
                ps_sao_ctxt->au1_sao_src_left_luma,
                ps_sao_ctxt->i4_sao_blk_ht);

            /* Copy the top and top left pixels to the scratch buffer for evry rdo cand because its
            overwritten by the sao leaf level function for next ctb*/
            memcpy(
                s_sao_ctxt.au1_top_luma_scratch,
                ps_sao_ctxt->pu1_curr_sao_src_top_luma - 1,
                ps_sao_ctxt->i4_sao_blk_wd + 2);
            s_sao_ctxt.pu1_curr_sao_src_top_luma = s_sao_ctxt.au1_top_luma_scratch + 1;

            pu1_luma_scratch_buf = ps_sao_ctxt->au1_sao_luma_scratch[curr_buf_idx];

            ASSERT(
                (abs(s_sao_ctxt.ps_sao->u1_y_offset[1]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_y_offset[2]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_y_offset[3]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_y_offset[4]) <= 7));
            ASSERT(
                (abs(s_sao_ctxt.ps_sao->u1_cb_offset[1]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_cb_offset[2]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_cb_offset[3]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_cb_offset[4]) <= 7));
            ASSERT(
                (abs(s_sao_ctxt.ps_sao->u1_cr_offset[1]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_cr_offset[2]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_cr_offset[3]) <= 7) &&
                (abs(s_sao_ctxt.ps_sao->u1_cr_offset[4]) <= 7));
            ASSERT(
                (s_sao_ctxt.ps_sao->b5_y_band_pos <= 28) &&
                (s_sao_ctxt.ps_sao->b5_cb_band_pos <= 28) &&
                (s_sao_ctxt.ps_sao->b5_cr_band_pos <= 28));

            /* Copy the deblocked recon data to scratch buffer to do sao*/

            ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_copy_2d(
                pu1_luma_scratch_buf,
                i4_luma_scratch_buf_stride,
                pu1_recon_luma,
                luma_recon_stride,
                SCRATCH_BUF_STRIDE,
                ctb_ht + 1);

            s_sao_ctxt.pu1_cur_luma_recon_buf = pu1_luma_scratch_buf;
            s_sao_ctxt.i4_cur_luma_recon_stride = i4_luma_scratch_buf_stride;

            s_sao_ctxt.i1_slice_sao_luma_flag = s_sao_ctxt.ps_slice_hdr->i1_slice_sao_luma_flag;
            s_sao_ctxt.i1_slice_sao_chroma_flag = 0;

            ihevce_sao_ctb(&s_sao_ctxt, ps_tile_params);

            /* Calculate the distortion between sao'ed ctb and original src ctb*/
            // clang-format off
            distortion =
                ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_ssd_calculator(pu1_src_luma,
                        s_sao_ctxt.pu1_cur_luma_recon_buf, luma_src_stride,
                        s_sao_ctxt.i4_cur_luma_recon_stride, ctb_wd, ctb_ht, NULL_PLANE);
            // clang-format on

            ps_sao_ctxt->ps_rdopt_entropy_ctxt->i4_curr_buf_idx = curr_buf_idx;
            ctb_bits = ihevce_cabac_rdo_encode_sao(
                ps_sao_ctxt->ps_rdopt_entropy_ctxt, ps_ctb_enc_loop_out);

            /* Calculate the cost as D+(lamda)*R   */
            curr_cost = distortion +
                        COMPUTE_RATE_COST_CLIP30(ctb_bits, i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

            if(curr_cost < best_cost)
            {
                best_cost = curr_cost;
                best_buf_idx = ps_sao_ctxt->ps_rdopt_entropy_ctxt->i4_curr_buf_idx;
                best_cand_idx = rdo_cand;
                curr_buf_idx = !curr_buf_idx;
            }
        }

        /* Copy the sao parameters of the best luma cand into the luma_chroma cnad structure for next stage of RDO
        * between luma_chroma combined cand, NO SAO cand, LEFT and TOP merge cand
        */
        s_best_luma_chroma_cand.b3_y_type_idx =
            ps_sao_ctxt->as_sao_rd_cand[best_cand_idx].b3_y_type_idx;
        s_best_luma_chroma_cand.u1_y_offset[1] =
            ps_sao_ctxt->as_sao_rd_cand[best_cand_idx].u1_y_offset[1];
        s_best_luma_chroma_cand.u1_y_offset[2] =
            ps_sao_ctxt->as_sao_rd_cand[best_cand_idx].u1_y_offset[2];
        s_best_luma_chroma_cand.u1_y_offset[3] =
            ps_sao_ctxt->as_sao_rd_cand[best_cand_idx].u1_y_offset[3];
        s_best_luma_chroma_cand.u1_y_offset[4] =
            ps_sao_ctxt->as_sao_rd_cand[best_cand_idx].u1_y_offset[4];
        s_best_luma_chroma_cand.b5_y_band_pos =
            ps_sao_ctxt->as_sao_rd_cand[best_cand_idx].b5_y_band_pos;
    }
    else
    {
        /*Back up the top pixels for (x,y+1)th ctb*/
        if(!ps_sao_ctxt->i4_is_last_ctb_row)
        {
            memcpy(
                ps_sao_ctxt->pu1_curr_sao_src_top_luma + ps_sao_ctxt->i4_frm_top_luma_buf_stride,
                pu1_recon_luma + luma_recon_stride * (ctb_size - 1),
                ps_sao_ctxt->i4_sao_blk_wd);
        }

        s_best_luma_chroma_cand.b3_y_type_idx = SAO_NONE;
        s_best_luma_chroma_cand.u1_y_offset[1] = 0;
        s_best_luma_chroma_cand.u1_y_offset[2] = 0;
        s_best_luma_chroma_cand.u1_y_offset[3] = 0;
        s_best_luma_chroma_cand.u1_y_offset[4] = 0;
        s_best_luma_chroma_cand.b5_y_band_pos = 0;
        s_best_luma_chroma_cand.b1_sao_merge_left_flag = 0;
        s_best_luma_chroma_cand.b1_sao_merge_up_flag = 0;

        s_best_luma_chroma_cand.b3_cb_type_idx = SAO_NONE;
        s_best_luma_chroma_cand.u1_cb_offset[1] = 0;
        s_best_luma_chroma_cand.u1_cb_offset[2] = 0;
        s_best_luma_chroma_cand.u1_cb_offset[3] = 0;
        s_best_luma_chroma_cand.u1_cb_offset[4] = 0;
        s_best_luma_chroma_cand.b5_cb_band_pos = 0;

        s_best_luma_chroma_cand.b3_cr_type_idx = SAO_NONE;
        s_best_luma_chroma_cand.u1_cr_offset[1] = 0;
        s_best_luma_chroma_cand.u1_cr_offset[2] = 0;
        s_best_luma_chroma_cand.u1_cr_offset[3] = 0;
        s_best_luma_chroma_cand.u1_cr_offset[4] = 0;
        s_best_luma_chroma_cand.b5_cr_band_pos = 0;
    }
    /*****************************************************/
    /********************RDO FOR CHROMA CAND**************/
    /*****************************************************/
#if !DISABLE_SAO_WHEN_NOISY
    if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_chroma_flag)
#else
    if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_chroma_flag && !u1_force_no_offset)
#endif
    {
        /*Back up the top pixels for (x,y+1)th ctb*/
        if(!ps_sao_ctxt->i4_is_last_ctb_row)
        {
            memcpy(
                ps_sao_ctxt->pu1_curr_sao_src_top_chroma +
                    ps_sao_ctxt->i4_frm_top_chroma_buf_stride,
                pu1_recon_chroma + chroma_recon_stride * ((ctb_size >> !u1_is_422) - 1),
                ps_sao_ctxt->i4_sao_blk_wd);
        }

        /* Reset the error and edge count*/
        for(edgeidx = 0; edgeidx < 5; edgeidx++)
        {
            acc_error_category[edgeidx] = 0;
            category_count[edgeidx] = 0;
        }
        // clang-format off
        ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_get_chroma_eo_sao_params(ps_sao_ctxt,
                s_best_luma_chroma_cand.b3_y_type_idx, acc_error_category,
                category_count);
        // clang-format on

        /* Copy the sao parameters of the best luma cand into the luma_chroma cnad structure for next stage of RDO
        * between luma_chroma combined cand, NO SAO cand, LEFT and TOP merge cand
        */
        // clang-format off
        s_best_luma_chroma_cand.b3_cb_type_idx = s_best_luma_chroma_cand.b3_y_type_idx;
        s_best_luma_chroma_cand.u1_cb_offset[1] = category_count[0]
                ? (CLIP3(acc_error_category[0] / category_count[0], 0, 7))
                : 0;
        s_best_luma_chroma_cand.u1_cb_offset[2] = category_count[1]
                ? (CLIP3(acc_error_category[1] / category_count[1], 0, 7))
                : 0;
        s_best_luma_chroma_cand.u1_cb_offset[3] = category_count[3]
                ? (CLIP3(acc_error_category[3] / category_count[3], -7, 0))
                : 0;
        s_best_luma_chroma_cand.u1_cb_offset[4] = category_count[4]
                ? (CLIP3(acc_error_category[4] / category_count[4], -7, 0))
                : 0;
        s_best_luma_chroma_cand.b5_cb_band_pos = 0;

        s_best_luma_chroma_cand.b3_cr_type_idx = s_best_luma_chroma_cand.b3_y_type_idx;
        s_best_luma_chroma_cand.u1_cr_offset[1] = category_count[0]
                ? (CLIP3(acc_error_category[0] / category_count[0], 0, 7))
                : 0;
        s_best_luma_chroma_cand.u1_cr_offset[2] = category_count[1]
                ? (CLIP3(acc_error_category[1] / category_count[1], 0, 7))
                : 0;
        s_best_luma_chroma_cand.u1_cr_offset[3] = category_count[3]
                ? (CLIP3(acc_error_category[3] / category_count[3], -7, 0))
                : 0;
        s_best_luma_chroma_cand.u1_cr_offset[4] = category_count[4]
                ? (CLIP3(acc_error_category[4] / category_count[4], -7, 0))
                : 0;
        // clang-format on
        s_best_luma_chroma_cand.b5_cr_band_pos = 0;
    }
    else
    {
        /*Back up the top pixels for (x,y+1)th ctb*/
        if(!ps_sao_ctxt->i4_is_last_ctb_row)
        {
            memcpy(
                ps_sao_ctxt->pu1_curr_sao_src_top_chroma +
                    ps_sao_ctxt->i4_frm_top_chroma_buf_stride,
                pu1_recon_chroma + chroma_recon_stride * ((ctb_size >> !u1_is_422) - 1),
                ps_sao_ctxt->i4_sao_blk_wd);
        }

        s_best_luma_chroma_cand.b3_cb_type_idx = SAO_NONE;
        s_best_luma_chroma_cand.u1_cb_offset[1] = 0;
        s_best_luma_chroma_cand.u1_cb_offset[2] = 0;
        s_best_luma_chroma_cand.u1_cb_offset[3] = 0;
        s_best_luma_chroma_cand.u1_cb_offset[4] = 0;
        s_best_luma_chroma_cand.b5_cb_band_pos = 0;

        s_best_luma_chroma_cand.b3_cr_type_idx = SAO_NONE;
        s_best_luma_chroma_cand.u1_cr_offset[1] = 0;
        s_best_luma_chroma_cand.u1_cr_offset[2] = 0;
        s_best_luma_chroma_cand.u1_cr_offset[3] = 0;
        s_best_luma_chroma_cand.u1_cr_offset[4] = 0;
        s_best_luma_chroma_cand.b5_cr_band_pos = 0;

        s_best_luma_chroma_cand.b1_sao_merge_left_flag = 0;
        s_best_luma_chroma_cand.b1_sao_merge_up_flag = 0;
    }

    s_best_luma_chroma_cand.b1_sao_merge_left_flag = 0;
    s_best_luma_chroma_cand.b1_sao_merge_up_flag = 0;

    /*****************************************************/
    /**RDO for Best Luma - Chroma combined, No SAO,*******/
    /*************Left merge and Top merge****************/
    /*****************************************************/

    /* No SAO cand*/
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_left_flag = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_up_flag = 0;

    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b3_y_type_idx = SAO_NONE;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_y_offset[1] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_y_offset[2] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_y_offset[3] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_y_offset[4] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b5_y_band_pos = 0;

    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b3_cb_type_idx = SAO_NONE;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cb_offset[1] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cb_offset[2] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cb_offset[3] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cb_offset[4] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b5_cb_band_pos = 0;

    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b3_cr_type_idx = SAO_NONE;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cr_offset[1] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cr_offset[2] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cr_offset[3] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].u1_cr_offset[4] = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b5_cr_band_pos = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_left_flag = 0;
    ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_up_flag = 0;

    num_rdo_cand++;

    /* SAO_note_01: If the CTB lies on a tile or a slice boundary, then
    the standard mandates that the merge candidates must be set to unavailable.
    Hence, check for tile boundary condition by reading
    s_ctb_nbr_avail_flags.u1_left_avail rather than frame position of CTB.
    A special case: Merge-candidates should be available at dependent-slices boundaries.
    Search for <SAO_note_01> in workspace to know more */

#if !DISABLE_SAO_WHEN_NOISY
    if(1)
#else
    if(!u1_force_no_offset)
#endif
    {
        /* Merge left cand*/
        if(ps_ctb_enc_loop_out->s_ctb_nbr_avail_flags.u1_left_avail)
        {
            memcpy(
                &ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand],
                &ps_sao_ctxt->s_left_ctb_sao,
                sizeof(sao_enc_t));
            ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_left_flag = 1;
            ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_up_flag = 0;
            num_rdo_cand++;
        }

        /* Merge top cand*/
        if(ps_ctb_enc_loop_out->s_ctb_nbr_avail_flags.u1_top_avail)
        {
            memcpy(
                &ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand],
                (ps_sao_ctxt->ps_top_ctb_sao - ps_sao_ctxt->u4_num_ctbs_horz),
                sizeof(sao_enc_t));
            ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_left_flag = 0;
            ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand].b1_sao_merge_up_flag = 1;
            num_rdo_cand++;
        }

        /* Best luma-chroma candidate*/
        memcpy(
            &ps_sao_ctxt->as_sao_rd_cand[num_rdo_cand],
            &s_best_luma_chroma_cand,
            sizeof(sao_enc_t));
        num_rdo_cand++;
    }

    {
        UWORD32 luma_distortion = 0, chroma_distortion = 0;
        /* First cand will be best cand after 1st iteration*/
        curr_buf_idx = 0;
        best_buf_idx = 1;
        best_cost = 0xFFFFFFFF;
        best_cand_idx = 0;

        for(rdo_cand = 0; rdo_cand < num_rdo_cand; rdo_cand++)
        {
            s_sao_ctxt.ps_sao = &ps_sao_ctxt->as_sao_rd_cand[rdo_cand];

            distortion = 0;

            /* This memcpy is required because cabac uses parameters from this structure
            * to evaluate bits and this structure ptr is sent to cabac through
            * "ihevce_cabac_rdo_encode_sao" function
            */
            memcpy(&ps_ctb_enc_loop_out->s_sao, s_sao_ctxt.ps_sao, sizeof(sao_enc_t));

            if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_luma_flag)
            {
                /* Copy the left pixels to the scratch buffer for evry rdo cand because its
                overwritten by the sao leaf level function for next ctb*/
                memcpy(
                    s_sao_ctxt.au1_left_luma_scratch,
                    ps_sao_ctxt->au1_sao_src_left_luma,
                    ps_sao_ctxt->i4_sao_blk_ht);

                /* Copy the top and top left pixels to the scratch buffer for evry rdo cand because its
                overwritten by the sao leaf level function for next ctb*/
                memcpy(
                    s_sao_ctxt.au1_top_luma_scratch,
                    ps_sao_ctxt->pu1_curr_sao_src_top_luma - 1,
                    ps_sao_ctxt->i4_sao_blk_wd + 2);
                s_sao_ctxt.pu1_curr_sao_src_top_luma = s_sao_ctxt.au1_top_luma_scratch + 1;

                pu1_luma_scratch_buf = ps_sao_ctxt->au1_sao_luma_scratch[curr_buf_idx];

                /* Copy the deblocked recon data to scratch buffer to do sao*/

                ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_luma_scratch_buf,
                    i4_luma_scratch_buf_stride,
                    pu1_recon_luma,
                    luma_recon_stride,
                    SCRATCH_BUF_STRIDE,
                    ctb_ht + 1);
                s_sao_ctxt.pu1_cur_luma_recon_buf = pu1_luma_scratch_buf;
                s_sao_ctxt.i4_cur_luma_recon_stride = i4_luma_scratch_buf_stride;

                ASSERT(
                    (abs(s_sao_ctxt.ps_sao->u1_y_offset[1]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_y_offset[2]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_y_offset[3]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_y_offset[4]) <= 7));
            }
            if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_chroma_flag)
            {
                /* Copy the left pixels to the scratch buffer for evry rdo cand because its
                overwritten by the sao leaf level function for next ctb*/
                memcpy(
                    s_sao_ctxt.au1_left_chroma_scratch,
                    ps_sao_ctxt->au1_sao_src_left_chroma,
                    (ps_sao_ctxt->i4_sao_blk_ht >> !u1_is_422) * 2);

                /* Copy the top and top left pixels to the scratch buffer for evry rdo cand because its
                overwritten by the sao leaf level function for next ctb*/
                memcpy(
                    s_sao_ctxt.au1_top_chroma_scratch,
                    ps_sao_ctxt->pu1_curr_sao_src_top_chroma - 2,
                    ps_sao_ctxt->i4_sao_blk_wd + 4);

                s_sao_ctxt.pu1_curr_sao_src_top_chroma = s_sao_ctxt.au1_top_chroma_scratch + 2;

                pu1_chroma_scratch_buf = ps_sao_ctxt->au1_sao_chroma_scratch[curr_buf_idx];

                /* Copy the deblocked recon data to scratch buffer to do sao*/

                ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_copy_2d(
                    pu1_chroma_scratch_buf,
                    i4_chroma_scratch_buf_stride,
                    pu1_recon_chroma,
                    chroma_recon_stride,
                    SCRATCH_BUF_STRIDE,
                    (ctb_ht >> !u1_is_422) + 1);

                s_sao_ctxt.pu1_cur_chroma_recon_buf = pu1_chroma_scratch_buf;
                s_sao_ctxt.i4_cur_chroma_recon_stride = i4_chroma_scratch_buf_stride;

                ASSERT(
                    (abs(s_sao_ctxt.ps_sao->u1_cb_offset[1]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_cb_offset[2]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_cb_offset[3]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_cb_offset[4]) <= 7));
                ASSERT(
                    (abs(s_sao_ctxt.ps_sao->u1_cr_offset[1]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_cr_offset[2]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_cr_offset[3]) <= 7) &&
                    (abs(s_sao_ctxt.ps_sao->u1_cr_offset[4]) <= 7));
            }

            ASSERT(
                (s_sao_ctxt.ps_sao->b5_y_band_pos <= 28) &&
                (s_sao_ctxt.ps_sao->b5_cb_band_pos <= 28) &&
                (s_sao_ctxt.ps_sao->b5_cr_band_pos <= 28));

            s_sao_ctxt.i1_slice_sao_luma_flag = s_sao_ctxt.ps_slice_hdr->i1_slice_sao_luma_flag;
            s_sao_ctxt.i1_slice_sao_chroma_flag = s_sao_ctxt.ps_slice_hdr->i1_slice_sao_chroma_flag;

            ihevce_sao_ctb(&s_sao_ctxt, ps_tile_params);

            if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_luma_flag)
            {  // clang-format off
                luma_distortion =
                    ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_ssd_calculator(pu1_src_luma,
                            s_sao_ctxt.pu1_cur_luma_recon_buf, luma_src_stride,
                            s_sao_ctxt.i4_cur_luma_recon_stride, ctb_wd,
                            ctb_ht,
                            NULL_PLANE);
            }  // clang-format on

            if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_chroma_flag)
            {  // clang-format off
                chroma_distortion =
                    ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_ssd_calculator(pu1_src_chroma,
                            s_sao_ctxt.pu1_cur_chroma_recon_buf,
                            chroma_src_stride,
                            s_sao_ctxt.i4_cur_chroma_recon_stride, ctb_wd,
                            (ctb_ht >> !u1_is_422),
                            NULL_PLANE);
            }  // clang-format on

            /*chroma distortion is added after correction because of lambda difference*/
            distortion =
                luma_distortion +
                (UWORD32)(chroma_distortion * (i8_cl_ssd_lambda_qf / i8_cl_ssd_lambda_chroma_qf));

            ps_sao_ctxt->ps_rdopt_entropy_ctxt->i4_curr_buf_idx = curr_buf_idx;
            ctb_bits = ihevce_cabac_rdo_encode_sao(
                ps_sao_ctxt->ps_rdopt_entropy_ctxt, ps_ctb_enc_loop_out);

            /* Calculate the cost as D+(lamda)*R   */
            curr_cost = distortion +
                        COMPUTE_RATE_COST_CLIP30(ctb_bits, i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

            if(curr_cost < best_cost)
            {
                best_ctb_sao_bits = ctb_bits;
                best_cost = curr_cost;
                best_buf_idx = ps_sao_ctxt->ps_rdopt_entropy_ctxt->i4_curr_buf_idx;
                best_cand_idx = rdo_cand;
                curr_buf_idx = !curr_buf_idx;
            }
        }
        /*Adding sao bits to header bits*/
        *pu4_frame_rdopt_header_bits = best_ctb_sao_bits;

        ihevce_update_best_sao_cabac_state(ps_sao_ctxt->ps_rdopt_entropy_ctxt, best_buf_idx);

        /* store the sao parameters of curr ctb for top merge and left merge*/
        memcpy(
            ps_sao_ctxt->ps_top_ctb_sao,
            &ps_sao_ctxt->as_sao_rd_cand[best_cand_idx],
            sizeof(sao_enc_t));
        memcpy(
            &ps_sao_ctxt->s_left_ctb_sao,
            &ps_sao_ctxt->as_sao_rd_cand[best_cand_idx],
            sizeof(sao_enc_t));

        /* Copy the sao parameters of winning candidate into the structure which will be sent to entropy thrd*/
        memcpy(
            &ps_ctb_enc_loop_out->s_sao,
            &ps_sao_ctxt->as_sao_rd_cand[best_cand_idx],
            sizeof(sao_enc_t));

        if(!ps_sao_ctxt->i4_is_last_ctb_col)
        {
            /* Update left luma buffer for next ctb */
            for(row = 0; row < ps_sao_ctxt->i4_sao_blk_ht; row++)
            {
                ps_sao_ctxt->au1_sao_src_left_luma[row] =
                    ps_sao_ctxt->pu1_cur_luma_recon_buf
                        [row * ps_sao_ctxt->i4_cur_luma_recon_stride +
                         (ps_sao_ctxt->i4_sao_blk_wd - 1)];
            }
        }

        if(!ps_sao_ctxt->i4_is_last_ctb_col)
        {
            /* Update left chroma buffer for next ctb */
            for(row = 0; row < (ps_sao_ctxt->i4_sao_blk_ht >> 1); row++)
            {
                *(UWORD16 *)(ps_sao_ctxt->au1_sao_src_left_chroma + row * 2) =
                    *(UWORD16 *)(ps_sao_ctxt->pu1_cur_chroma_recon_buf +
                                 row * ps_sao_ctxt->i4_cur_chroma_recon_stride +
                                 (ps_sao_ctxt->i4_sao_blk_wd - 2));
            }
        }

        if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_luma_flag)
        {
            /* Copy the sao'ed output of the best candidate to the recon buffer*/

            ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_copy_2d(
                ps_sao_ctxt->pu1_cur_luma_recon_buf,
                ps_sao_ctxt->i4_cur_luma_recon_stride,
                ps_sao_ctxt->au1_sao_luma_scratch[best_buf_idx],
                i4_luma_scratch_buf_stride,
                ctb_wd,
                ctb_ht);
        }
        if(ps_sao_ctxt->ps_slice_hdr->i1_slice_sao_chroma_flag)
        {
            /* Copy the sao'ed output of the best candidate to the chroma recon buffer*/

            ps_sao_ctxt->ps_cmn_utils_optimised_function_list->pf_copy_2d(
                ps_sao_ctxt->pu1_cur_chroma_recon_buf,
                ps_sao_ctxt->i4_cur_chroma_recon_stride,
                ps_sao_ctxt->au1_sao_chroma_scratch[best_buf_idx],
                i4_chroma_scratch_buf_stride,
                ctb_wd,
                ctb_ht >> !u1_is_422);
        }
    }
}
