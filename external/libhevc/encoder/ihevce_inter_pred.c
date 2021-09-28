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
*  ihevce_inter_pred.c
*
* @brief
*  Contains funtions for giving out prediction samples for a given pu
*
* @author
*  Ittiam
*
* @par List of Functions:
*   - ihevc_inter_pred()
*
*
*******************************************************************************
*/
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

#include "ihevc_debug.h"
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
#include "ihevce_inter_pred.h"
#include "ihevc_weighted_pred.h"

/*****************************************************************************/
/* Global tables                                                             */
/*****************************************************************************/

/**
******************************************************************************
* @brief  Table of filter tap coefficients for HEVC luma inter prediction
* input   : sub pel mv position (dx/dy = 0:3)
* output  : filter coeffs to be used for that position
*
* @remarks See section 8.5.2.2.2.1 Luma sample interpolation process of HEVC
******************************************************************************
*/
WORD8 gai1_hevc_luma_filter_taps[4][NTAPS_LUMA] = { { 0, 0, 0, 64, 0, 0, 0, 0 },
                                                    { -1, 4, -10, 58, 17, -5, 1, 0 },
                                                    { -1, 4, -11, 40, 40, -11, 4, -1 },
                                                    { 0, 1, -5, 17, 58, -10, 4, -1 } };

/**
******************************************************************************
* @brief  Table of filter tap coefficients for HEVC chroma inter prediction
* input   : chroma sub pel mv position (dx/dy = 0:7)
* output  : filter coeffs to be used for that position
*
* @remarks See section 8.5.2.2.2.2 Chroma sample interpolation process of HEVC
The filter uses only the first four elements in each array
******************************************************************************
*/
WORD8 gai1_hevc_chroma_filter_taps[8][NTAPS_CHROMA] = { { 0, 64, 0, 0 },    { -2, 58, 10, -2 },
                                                        { -4, 54, 16, -2 }, { -6, 46, 28, -4 },
                                                        { -4, 36, 36, -4 }, { -4, 28, 46, -6 },
                                                        { -2, 16, 54, -4 }, { -2, 10, 58, -2 } };

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief
*  Performs Luma inter pred based on sub pel position dxdy and store the result
*  in a 16 bit destination buffer
*
* @param[in] pu1_src
*  pointer to the source correspoding to integer pel position of a mv (left and
*  top justified integer position)
*
* @param[out] pi2_dst
*  WORD16 pointer to the destination
*
* @param[in] src_strd
*  source buffer stride
*
* @param[in] dst_strd
*  destination buffer stride
*
* @param[in] pi2_hdst_scratch
*  scratch buffer for intermediate storage of horizontal filter output; used as
*  input for vertical filtering when sub pel components (dx != 0) && (dy != 0)
*
*  Max scratch buffer required is w * (h + 7) * sizeof(WORD16)
*
* @param[in] ht
*  width of the prediction unit
*
* @param[in] wd
*  width of the prediction unit
*
* @param[in] dx
*  qpel position[0:3] of mv in x direction
*
* @param[in] dy
*  qpel position[0:3] of mv in y direction
*
* @returns
*   none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_luma_interpolate_16bit_dxdy(
    UWORD8 *pu1_src,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 dst_strd,
    WORD16 *pi2_hdst_scratch,
    WORD32 ht,
    WORD32 wd,
    WORD32 dy,
    WORD32 dx,
    func_selector_t *ps_func_selector)
{
    if((0 == dx) && (0 == dy))
    {
        /*--------- full pel position : copy input by upscaling-------*/

        ps_func_selector->ihevc_inter_pred_luma_copy_w16out_fptr(
            pu1_src, pi2_dst, src_strd, dst_strd, &gai1_hevc_luma_filter_taps[0][0], ht, wd);
    }
    else if((0 != dx) && (0 != dy))
    {
        /*----------sub pel in both x and y direction---------*/

        UWORD8 *pu1_horz_src = pu1_src - (3 * src_strd);
        WORD32 hdst_buf_stride = wd;
        WORD16 *pi2_vert_src = pi2_hdst_scratch + (3 * hdst_buf_stride);

        /* horizontal filtering of source done in a scratch buffer first  */
        ps_func_selector->ihevc_inter_pred_luma_horz_w16out_fptr(
            pu1_horz_src,
            pi2_hdst_scratch,
            src_strd,
            hdst_buf_stride,
            &gai1_hevc_luma_filter_taps[dx][0],
            (ht + NTAPS_LUMA - 1),
            wd);

        /* vertical filtering on scratch buffer and stored in desitnation  */
        ps_func_selector->ihevc_inter_pred_luma_vert_w16inp_w16out_fptr(
            pi2_vert_src,
            pi2_dst,
            hdst_buf_stride,
            dst_strd,
            &gai1_hevc_luma_filter_taps[dy][0],
            ht,
            wd);
    }
    else if(0 == dy)
    {
        /*----------sub pel in x direction only ---------*/

        ps_func_selector->ihevc_inter_pred_luma_horz_w16out_fptr(
            pu1_src, pi2_dst, src_strd, dst_strd, &gai1_hevc_luma_filter_taps[dx][0], ht, wd);
    }
    else /* if (0 == dx) */
    {
        /*----------sub pel in y direction only ---------*/

        ps_func_selector->ihevc_inter_pred_luma_vert_w16out_fptr(
            pu1_src, pi2_dst, src_strd, dst_strd, &gai1_hevc_luma_filter_taps[dy][0], ht, wd);
    }
}

/**
*******************************************************************************
*
* @brief
*  Performs Luma inter pred based on sub pel position dxdy and store the result
*  in a 8 bit destination buffer
*
* @param[in] pu1_src
*  pointer to the source correspoding to integer pel position of a mv (left and
*  top justified integer position)
*
* @param[out] pu1_dst
*  UWORD8 pointer to the destination
*
* @param[in] src_strd
*  source buffer stride
*
* @param[in] dst_strd
*  destination buffer stride
*
* @param[in] pi2_hdst_scratch
*  scratch buffer for intermediate storage of horizontal filter output; used as
*  input for vertical filtering when sub pel components (dx != 0) && (dy != 0)
*
*  Max scratch buffer required is w * (h + 7) * sizeof(WORD16)
*
* @param[in] ht
*  width of the prediction unit
*
* @param[in] wd
*  width of the prediction unit
*
* @param[in] dx
*  qpel position[0:3] of mv in x direction
*
* @param[in] dy
*  qpel position[0:3] of mv in y direction
*
* @returns
*   none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_luma_interpolate_8bit_dxdy(
    UWORD8 *pu1_src,
    UWORD8 *pu1_dst,
    WORD32 src_strd,
    WORD32 dst_strd,
    WORD16 *pi2_hdst_scratch,
    WORD32 ht,
    WORD32 wd,
    WORD32 dy,
    WORD32 dx,
    func_selector_t *ps_func_selector)
{
    if((0 == dx) && (0 == dy))
    {
        /*--------- full pel position : copy input as is -------*/

        ps_func_selector->ihevc_inter_pred_luma_copy_fptr(
            pu1_src, pu1_dst, src_strd, dst_strd, &gai1_hevc_luma_filter_taps[0][0], ht, wd);
    }
    else if((0 != dx) && (0 != dy))
    {
        /*----------sub pel in both x and y direction---------*/

        UWORD8 *pu1_horz_src = pu1_src - (3 * src_strd);
        WORD32 hdst_buf_stride = wd;
        WORD16 *pi2_vert_src = pi2_hdst_scratch + (3 * hdst_buf_stride);

        /* horizontal filtering of source done in a scratch buffer first  */
        ps_func_selector->ihevc_inter_pred_luma_horz_w16out_fptr(
            pu1_horz_src,
            pi2_hdst_scratch,
            src_strd,
            hdst_buf_stride,
            &gai1_hevc_luma_filter_taps[dx][0],
            (ht + NTAPS_LUMA - 1),
            wd);

        /* vertical filtering on scratch buffer and stored in desitnation  */
        ps_func_selector->ihevc_inter_pred_luma_vert_w16inp_fptr(
            pi2_vert_src,
            pu1_dst,
            hdst_buf_stride,
            dst_strd,
            &gai1_hevc_luma_filter_taps[dy][0],
            ht,
            wd);
    }
    else if(0 == dy)
    {
        /*----------sub pel in x direction only ---------*/

        ps_func_selector->ihevc_inter_pred_luma_horz_fptr(
            pu1_src, pu1_dst, src_strd, dst_strd, &gai1_hevc_luma_filter_taps[dx][0], ht, wd);
    }
    else /* if (0 == dx) */
    {
        /*----------sub pel in y direction only ---------*/

        ps_func_selector->ihevc_inter_pred_luma_vert_fptr(
            pu1_src, pu1_dst, src_strd, dst_strd, &gai1_hevc_luma_filter_taps[dy][0], ht, wd);
    }
}

/**
*******************************************************************************
*
* @brief
*  Performs Luma prediction for a inter prediction unit(PU)
*
* @par Description:
*  For a given PU, Inter prediction followed by weighted prediction (if
*  required)
*
* @param[in] ps_inter_pred_ctxt
*  context for inter prediction; contains ref list, weight offsets, ctb offsets
*
* @param[in] ps_pu
*  pointer to PU structure whose inter prediction needs to be done
*
* @param[in] pu1_dst_buf
*  pointer to destination buffer where the inter prediction is done
*
* @param[in] dst_stride
*  pitch of the destination buffer
*
* @returns
*   IV_FAIL for mvs going outside ref frame padded limits
*   IV_SUCCESS after completing mc for given inter pu
*
* @remarks
*
*******************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_luma_inter_pred_pu(
    void *pv_inter_pred_ctxt,
    pu_t *ps_pu,
    void *pv_dst_buf,
    WORD32 dst_stride,
    WORD32 i4_flag_inter_pred_source)
{
    inter_pred_ctxt_t *ps_inter_pred_ctxt = (inter_pred_ctxt_t *)pv_inter_pred_ctxt;
    func_selector_t *ps_func_selector = ps_inter_pred_ctxt->ps_func_selector;

    WORD32 inter_pred_idc = ps_pu->b2_pred_mode;
    UWORD8 *pu1_dst_buf = (UWORD8 *)pv_dst_buf;
    WORD32 pu_wd = (ps_pu->b4_wd + 1) << 2;
    WORD32 pu_ht = (ps_pu->b4_ht + 1) << 2;

    WORD32 wp_flag = ps_inter_pred_ctxt->i1_weighted_pred_flag ||
                     ps_inter_pred_ctxt->i1_weighted_bipred_flag;

    /* 16bit dest required for interpolate if weighted pred is on or bipred */
    WORD32 store_16bit_output;

    recon_pic_buf_t *ps_ref_pic_l0, *ps_ref_pic_l1;
    UWORD8 *pu1_ref_pic, *pu1_ref_int_pel;
    WORD32 ref_pic_stride;

    /* offset of reference block in integer pel units */
    WORD32 frm_x_ofst, frm_y_ofst;
    WORD32 frm_x_pu, frm_y_pu;

    /* scratch 16 bit buffers for interpolation in l0 and l1 direction */
    WORD16 *pi2_scr_buf_l0 = &ps_inter_pred_ctxt->ai2_scratch_buf_l0[0];
    WORD16 *pi2_scr_buf_l1 = &ps_inter_pred_ctxt->ai2_scratch_buf_l1[0];

    /* scratch buffer for horizontal interpolation destination */
    WORD16 *pi2_horz_scratch = &ps_inter_pred_ctxt->ai2_horz_scratch[0];

    WORD32 wgt0, wgt1, off0, off1, shift, lvl_shift0, lvl_shift1;

    /* get PU's frm x and frm y offset */
    frm_x_pu = ps_inter_pred_ctxt->i4_ctb_frm_pos_x + (ps_pu->b4_pos_x << 2);
    frm_y_pu = ps_inter_pred_ctxt->i4_ctb_frm_pos_y + (ps_pu->b4_pos_y << 2);

    /* sanity checks */
    ASSERT((wp_flag == 0) || (wp_flag == 1));
    ASSERT(dst_stride >= pu_wd);
    ASSERT(ps_pu->b1_intra_flag == 0);

    lvl_shift0 = 0;
    lvl_shift1 = 0;

    if(wp_flag)
    {
        UWORD8 u1_is_wgt_pred_L0, u1_is_wgt_pred_L1;

        if(inter_pred_idc != PRED_L1)
        {
            ps_ref_pic_l0 = ps_inter_pred_ctxt->ps_ref_list[0][ps_pu->mv.i1_l0_ref_idx];
            u1_is_wgt_pred_L0 = ps_ref_pic_l0->s_weight_offset.u1_luma_weight_enable_flag;
        }
        if(inter_pred_idc != PRED_L0)
        {
            ps_ref_pic_l1 = ps_inter_pred_ctxt->ps_ref_list[1][ps_pu->mv.i1_l1_ref_idx];
            u1_is_wgt_pred_L1 = ps_ref_pic_l1->s_weight_offset.u1_luma_weight_enable_flag;
        }
        if(inter_pred_idc == PRED_BI)
        {
            wp_flag = (u1_is_wgt_pred_L0 || u1_is_wgt_pred_L1);
        }
        else if(inter_pred_idc == PRED_L0)
        {
            wp_flag = u1_is_wgt_pred_L0;
        }
        else if(inter_pred_idc == PRED_L1)
        {
            wp_flag = u1_is_wgt_pred_L1;
        }
        else
        {
            /*other values are not allowed*/
            assert(0);
        }
    }
    store_16bit_output = (inter_pred_idc == PRED_BI) || (wp_flag);

    if(inter_pred_idc != PRED_L1)
    {
        /*****************************************************/
        /*              L0 inter prediction                  */
        /*****************************************************/

        /* motion vecs in qpel precision                    */
        WORD32 mv_x = ps_pu->mv.s_l0_mv.i2_mvx;
        WORD32 mv_y = ps_pu->mv.s_l0_mv.i2_mvy;

        /* sub pel offsets in x and y direction w.r.t integer pel   */
        WORD32 dx = mv_x & 0x3;
        WORD32 dy = mv_y & 0x3;

        /* ref idx is currently stored in the lower 4bits           */
        WORD32 ref_idx = (ps_pu->mv.i1_l0_ref_idx);

        /*  x and y integer offsets w.r.t frame start               */
        frm_x_ofst = (frm_x_pu + (mv_x >> 2));
        frm_y_ofst = (frm_y_pu + (mv_y >> 2));

        ps_ref_pic_l0 = ps_inter_pred_ctxt->ps_ref_list[0][ref_idx];

        /* picture buffer start and stride */
        if(i4_flag_inter_pred_source == 1)
        {
            pu1_ref_pic = (UWORD8 *)ps_ref_pic_l0->s_yuv_buf_desc_src.pv_y_buf;
        }
        else
        {
            pu1_ref_pic = (UWORD8 *)ps_ref_pic_l0->s_yuv_buf_desc.pv_y_buf;
        }
        ref_pic_stride = ps_ref_pic_l0->s_yuv_buf_desc.i4_y_strd;

        /* Error check for mvs going out of ref frame padded limits */
        {
            WORD32 min_x, max_x = ps_ref_pic_l0->s_yuv_buf_desc.i4_y_wd;
            WORD32 min_y, max_y = ps_ref_pic_l0->s_yuv_buf_desc.i4_y_ht;

            min_x =
                -(ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_LEFT]
                      ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_LEFT] - 4)
                      : (PAD_HORZ - 4));

            max_x += ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_RIGHT]
                         ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_RIGHT] - 4)
                         : (PAD_HORZ - 4);

            min_y =
                -(ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_TOP]
                      ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_TOP] - 4)
                      : (PAD_VERT - 4));

            max_y += ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_BOT]
                         ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_BOT] - 4)
                         : (PAD_VERT - 4);

            if((frm_x_ofst < min_x) || (frm_x_ofst + pu_wd) > max_x)
                //ASSERT(0);
                return (IV_FAIL);

            if((frm_y_ofst < min_y) || (frm_y_ofst + pu_ht) > max_y)
                //ASSERT(0);
                return (IV_FAIL);
        }

        /* point to reference start location in ref frame           */
        /* Assuming clipping of mv is not required here as ME would */
        /* take care of mv access not going beyond padded data      */
        pu1_ref_int_pel = pu1_ref_pic + frm_x_ofst + (ref_pic_stride * frm_y_ofst);

        /* level shifted for subpel with both x and y componenet being non 0 */
        /* this is because the interpolate function subtract this to contain */
        /* the resulting data in 16 bits                                     */
        lvl_shift0 = (dx != 0) && (dy != 0) ? OFFSET14 : 0;

        if(store_16bit_output)
        {
            /* do interpolation in 16bit L0 scratch buffer */
            ihevce_luma_interpolate_16bit_dxdy(
                pu1_ref_int_pel,
                pi2_scr_buf_l0,
                ref_pic_stride,
                pu_wd,
                pi2_horz_scratch,
                pu_ht,
                pu_wd,
                dy,
                dx,
                ps_func_selector);
        }
        else
        {
            /* do interpolation in 8bit destination buffer and return */
            ihevce_luma_interpolate_8bit_dxdy(
                pu1_ref_int_pel,
                pu1_dst_buf,
                ref_pic_stride,
                dst_stride,
                pi2_horz_scratch,
                pu_ht,
                pu_wd,
                dy,
                dx,
                ps_func_selector);

            return (IV_SUCCESS);
        }
    }

    if(inter_pred_idc != PRED_L0)
    {
        /*****************************************************/
        /*      L1 inter prediction                          */
        /*****************************************************/

        /* motion vecs in qpel precision                            */
        WORD32 mv_x = ps_pu->mv.s_l1_mv.i2_mvx;
        WORD32 mv_y = ps_pu->mv.s_l1_mv.i2_mvy;

        /* sub pel offsets in x and y direction w.r.t integer pel   */
        WORD32 dx = mv_x & 0x3;
        WORD32 dy = mv_y & 0x3;

        /* ref idx is currently stored in the lower 4bits           */
        WORD32 ref_idx = (ps_pu->mv.i1_l1_ref_idx);

        /*  x and y integer offsets w.r.t frame start               */
        frm_x_ofst = (frm_x_pu + (mv_x >> 2));
        frm_y_ofst = (frm_y_pu + (mv_y >> 2));

        ps_ref_pic_l1 = ps_inter_pred_ctxt->ps_ref_list[1][ref_idx];

        /* picture buffer start and stride */

        if(i4_flag_inter_pred_source == 1)
        {
            pu1_ref_pic = (UWORD8 *)ps_ref_pic_l1->s_yuv_buf_desc_src.pv_y_buf;
        }
        else
        {
            pu1_ref_pic = (UWORD8 *)ps_ref_pic_l1->s_yuv_buf_desc.pv_y_buf;
        }
        ref_pic_stride = ps_ref_pic_l1->s_yuv_buf_desc.i4_y_strd;

        /* Error check for mvs going out of ref frame padded limits */
        {
            WORD32 min_x, max_x = ps_ref_pic_l1->s_yuv_buf_desc.i4_y_wd;
            WORD32 min_y, max_y = ps_ref_pic_l1->s_yuv_buf_desc.i4_y_ht;

            min_x =
                -(ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_LEFT]
                      ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_LEFT] - 4)
                      : (PAD_HORZ - 4));

            max_x += ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_RIGHT]
                         ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_RIGHT] - 4)
                         : (PAD_HORZ - 4);

            min_y =
                -(ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_TOP]
                      ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_TOP] - 4)
                      : (PAD_VERT - 4));

            max_y += ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_BOT]
                         ? (ps_inter_pred_ctxt->ai4_tile_xtra_pel[E_BOT] - 4)
                         : (PAD_VERT - 4);

            if((frm_x_ofst < min_x) || (frm_x_ofst + pu_wd) > max_x)
                //ASSERT(0);
                return (IV_FAIL);

            if((frm_y_ofst < min_y) || (frm_y_ofst + pu_ht) > max_y)
                //ASSERT(0);
                return (IV_FAIL);
        }

        /* point to reference start location in ref frame           */
        /* Assuming clipping of mv is not required here as ME would */
        /* take care of mv access not going beyond padded data      */
        pu1_ref_int_pel = pu1_ref_pic + frm_x_ofst + (ref_pic_stride * frm_y_ofst);

        /* level shifted for subpel with both x and y componenet being non 0 */
        /* this is because the interpolate function subtract this to contain */
        /* the resulting data in 16 bits                                     */
        lvl_shift1 = (dx != 0) && (dy != 0) ? OFFSET14 : 0;

        if(store_16bit_output)
        {
            /* do interpolation in 16bit L1 scratch buffer */
            ihevce_luma_interpolate_16bit_dxdy(
                pu1_ref_int_pel,
                pi2_scr_buf_l1,
                ref_pic_stride,
                pu_wd,
                pi2_horz_scratch,
                pu_ht,
                pu_wd,
                dy,
                dx,
                ps_func_selector);
        }
        else
        {
            /* do interpolation in 8bit destination buffer and return */
            ihevce_luma_interpolate_8bit_dxdy(
                pu1_ref_int_pel,
                pu1_dst_buf,
                ref_pic_stride,
                dst_stride,
                pi2_horz_scratch,
                pu_ht,
                pu_wd,
                dy,
                dx,
                ps_func_selector);

            return (IV_SUCCESS);
        }
    }

    if((inter_pred_idc != PRED_BI) && wp_flag)
    {
        /*****************************************************/
        /*      unidirection weighted prediction             */
        /*****************************************************/
        ihevce_wght_offst_t *ps_weight_offset;
        WORD16 *pi2_src;
        WORD32 lvl_shift;

        /* intialize the weight, offsets and ref based on l0/l1 mode */
        if(inter_pred_idc == PRED_L0)
        {
            pi2_src = pi2_scr_buf_l0;
            ps_weight_offset = &ps_ref_pic_l0->s_weight_offset;
            lvl_shift = lvl_shift0;
        }
        else
        {
            pi2_src = pi2_scr_buf_l1;
            ps_weight_offset = &ps_ref_pic_l1->s_weight_offset;
            lvl_shift = lvl_shift1;
        }

        wgt0 = ps_weight_offset->i2_luma_weight;
        off0 = ps_weight_offset->i2_luma_offset;
        shift = ps_inter_pred_ctxt->i4_log2_luma_wght_denom + SHIFT_14_MINUS_BIT_DEPTH;

        /* do the uni directional weighted prediction */
        ps_func_selector->ihevc_weighted_pred_uni_fptr(
            pi2_src, pu1_dst_buf, pu_wd, dst_stride, wgt0, off0, shift, lvl_shift, pu_ht, pu_wd);
    }
    else
    {
        /*****************************************************/
        /*              Bipred  prediction                   */
        /*****************************************************/

        if(wp_flag)
        {
            /*****************************************************/
            /*      Bi pred  weighted prediction                 */
            /*****************************************************/
            wgt0 = ps_ref_pic_l0->s_weight_offset.i2_luma_weight;
            off0 = ps_ref_pic_l0->s_weight_offset.i2_luma_offset;

            wgt1 = ps_ref_pic_l1->s_weight_offset.i2_luma_weight;
            off1 = ps_ref_pic_l1->s_weight_offset.i2_luma_offset;

            shift = ps_inter_pred_ctxt->i4_log2_luma_wght_denom + SHIFT_14_MINUS_BIT_DEPTH + 1;

            ps_func_selector->ihevc_weighted_pred_bi_fptr(
                pi2_scr_buf_l0,
                pi2_scr_buf_l1,
                pu1_dst_buf,
                pu_wd,
                pu_wd,
                dst_stride,
                wgt0,
                off0,
                wgt1,
                off1,
                shift,
                lvl_shift0,
                lvl_shift1,
                pu_ht,
                pu_wd);
        }
        else
        {
            /*****************************************************/
            /*          Default Bi pred  prediction              */
            /*****************************************************/
            ps_func_selector->ihevc_weighted_pred_bi_default_fptr(
                pi2_scr_buf_l0,
                pi2_scr_buf_l1,
                pu1_dst_buf,
                pu_wd,
                pu_wd,
                dst_stride,
                lvl_shift0,
                lvl_shift1,
                pu_ht,
                pu_wd);
        }
    }

    return (IV_SUCCESS);
}

/**
*******************************************************************************
*
* @brief
*  Performs Chroma inter pred based on sub pel position dxdy and store the
*  result in a 16 bit destination buffer
*
* @param[in] pu1_src
*  pointer to the source correspoding to integer pel position of a mv (left and
*  top justified integer position)
*
* @param[out] pi2_dst
*  WORD16 pointer to the destination
*
* @param[in] src_strd
*  source buffer stride
*
* @param[in] dst_strd
*  destination buffer stride
*
* @param[in] pi2_hdst_scratch
*  scratch buffer for intermediate storage of horizontal filter output; used as
*  input for vertical filtering when sub pel components (dx != 0) && (dy != 0)
*
*  Max scratch buffer required is w * (h + 3) * sizeof(WORD16)
*
* @param[in] ht
*  width of the prediction unit
*
* @param[in] wd
*  width of the prediction unit
*
* @param[in] dx
*  1/8th pel position[0:7] of mv in x direction
*
* @param[in] dy
*  1/8th pel position[0:7] of mv in y direction
*
* @returns
*   none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_chroma_interpolate_16bit_dxdy(
    UWORD8 *pu1_src,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 dst_strd,
    WORD16 *pi2_hdst_scratch,
    WORD32 ht,
    WORD32 wd,
    WORD32 dy,
    WORD32 dx,
    func_selector_t *ps_func_selector)
{
    if((0 == dx) && (0 == dy))
    {
        /*--------- full pel position : copy input by upscaling-------*/

        ps_func_selector->ihevc_inter_pred_chroma_copy_w16out_fptr(
            pu1_src, pi2_dst, src_strd, dst_strd, &gai1_hevc_chroma_filter_taps[0][0], ht, wd);
    }
    else if((0 != dx) && (0 != dy))
    {
        /*----------sub pel in both x and y direction---------*/

        UWORD8 *pu1_horz_src = pu1_src - src_strd;
        WORD32 hdst_buf_stride = (wd << 1); /* uv interleave */
        WORD16 *pi2_vert_src = pi2_hdst_scratch + hdst_buf_stride;

        /* horizontal filtering of source done in a scratch buffer first  */
        ps_func_selector->ihevc_inter_pred_chroma_horz_w16out_fptr(
            pu1_horz_src,
            pi2_hdst_scratch,
            src_strd,
            hdst_buf_stride,
            &gai1_hevc_chroma_filter_taps[dx][0],
            (ht + NTAPS_CHROMA - 1),
            wd);

        /* vertical filtering on scratch buffer and stored in desitnation  */
        ps_func_selector->ihevc_inter_pred_chroma_vert_w16inp_w16out_fptr(
            pi2_vert_src,
            pi2_dst,
            hdst_buf_stride,
            dst_strd,
            &gai1_hevc_chroma_filter_taps[dy][0],
            ht,
            wd);
    }
    else if(0 == dy)
    {
        /*----------sub pel in x direction only ---------*/

        ps_func_selector->ihevc_inter_pred_chroma_horz_w16out_fptr(
            pu1_src, pi2_dst, src_strd, dst_strd, &gai1_hevc_chroma_filter_taps[dx][0], ht, wd);
    }
    else /* if (0 == dx) */
    {
        /*----------sub pel in y direction only ---------*/

        ps_func_selector->ihevc_inter_pred_chroma_vert_w16out_fptr(
            pu1_src, pi2_dst, src_strd, dst_strd, &gai1_hevc_chroma_filter_taps[dy][0], ht, wd);
    }
}

/**
*******************************************************************************
*
* @brief
*  Performs Chroma inter pred based on sub pel position dxdy and store the
*  result in a 8 bit destination buffer
*
* @param[in] pu1_src
*  pointer to the source correspoding to integer pel position of a mv (left and
*  top justified integer position)
*
* @param[out] pu1_dst
*  UWORD8 pointer to the destination
*
* @param[in] src_strd
*  source buffer stride
*
* @param[in] dst_strd
*  destination buffer stride
*
* @param[in] pi2_hdst_scratch
*  scratch buffer for intermediate storage of horizontal filter output; used as
*  input for vertical filtering when sub pel components (dx != 0) && (dy != 0)
*
*  Max scratch buffer required is w * (h + 3) * sizeof(WORD16)
*
* @param[in] ht
*  width of the prediction unit
*
* @param[in] wd
*  width of the prediction unit
*
* @param[in] dx
*  1/8th pel position[0:7] of mv in x direction
*
* @param[in] dy
*  1/8th pel position[0:7] of mv in y direction
*
* @returns
*   none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_chroma_interpolate_8bit_dxdy(
    UWORD8 *pu1_src,
    UWORD8 *pu1_dst,
    WORD32 src_strd,
    WORD32 dst_strd,
    WORD16 *pi2_hdst_scratch,
    WORD32 ht,
    WORD32 wd,
    WORD32 dy,
    WORD32 dx,
    func_selector_t *ps_func_selector)
{
    if((0 == dx) && (0 == dy))
    {
        /*--------- full pel position : copy input as is -------*/
        ps_func_selector->ihevc_inter_pred_chroma_copy_fptr(
            pu1_src, pu1_dst, src_strd, dst_strd, &gai1_hevc_chroma_filter_taps[0][0], ht, wd);
    }
    else if((0 != dx) && (0 != dy))
    {
        /*----------sub pel in both x and y direction---------*/
        UWORD8 *pu1_horz_src = pu1_src - src_strd;
        WORD32 hdst_buf_stride = (wd << 1); /* uv interleave */
        WORD16 *pi2_vert_src = pi2_hdst_scratch + hdst_buf_stride;

        /* horizontal filtering of source done in a scratch buffer first  */
        ps_func_selector->ihevc_inter_pred_chroma_horz_w16out_fptr(
            pu1_horz_src,
            pi2_hdst_scratch,
            src_strd,
            hdst_buf_stride,
            &gai1_hevc_chroma_filter_taps[dx][0],
            (ht + NTAPS_CHROMA - 1),
            wd);

        /* vertical filtering on scratch buffer and stored in desitnation  */
        ps_func_selector->ihevc_inter_pred_chroma_vert_w16inp_fptr(
            pi2_vert_src,
            pu1_dst,
            hdst_buf_stride,
            dst_strd,
            &gai1_hevc_chroma_filter_taps[dy][0],
            ht,
            wd);
    }
    else if(0 == dy)
    {
        /*----------sub pel in x direction only ---------*/
        ps_func_selector->ihevc_inter_pred_chroma_horz_fptr(
            pu1_src, pu1_dst, src_strd, dst_strd, &gai1_hevc_chroma_filter_taps[dx][0], ht, wd);
    }
    else /* if (0 == dx) */
    {
        /*----------sub pel in y direction only ---------*/
        ps_func_selector->ihevc_inter_pred_chroma_vert_fptr(
            pu1_src, pu1_dst, src_strd, dst_strd, &gai1_hevc_chroma_filter_taps[dy][0], ht, wd);
    }
}

/**
*******************************************************************************
*
* @brief
*  Performs Chroma prediction for a inter prediction unit(PU)
*
* @par Description:
*  For a given PU, Inter prediction followed by weighted prediction (if
*  required). The reference and destination buffers are uv interleaved
*
* @param[in] ps_inter_pred_ctxt
*  context for inter prediction; contains ref list, weight offsets, ctb offsets
*
* @param[in] ps_pu
*  pointer to PU structure whose inter prediction needs to be done
*
* @param[in] pu1_dst_buf
*  pointer to destination buffer where the inter prediction is done
*
* @param[in] dst_stride
*  pitch of the destination buffer
*
* @returns
*   none
*
* @remarks
*
*******************************************************************************
*/
void ihevce_chroma_inter_pred_pu(
    void *pv_inter_pred_ctxt, pu_t *ps_pu, UWORD8 *pu1_dst_buf, WORD32 dst_stride)
{
    inter_pred_ctxt_t *ps_inter_pred_ctxt = (inter_pred_ctxt_t *)pv_inter_pred_ctxt;
    func_selector_t *ps_func_selector = ps_inter_pred_ctxt->ps_func_selector;

    WORD32 inter_pred_idc = ps_pu->b2_pred_mode;
    UWORD8 u1_is_422 = (ps_inter_pred_ctxt->u1_chroma_array_type == 2);
    /* chroma width and height are half of luma width and height */
    WORD32 pu_wd_chroma = (ps_pu->b4_wd + 1) << 1;
    WORD32 pu_ht_chroma = (ps_pu->b4_ht + 1) << (u1_is_422 + 1);

    WORD32 wp_flag = ps_inter_pred_ctxt->i1_weighted_pred_flag ||
                     ps_inter_pred_ctxt->i1_weighted_bipred_flag;

    /* 16bit dest required for interpolate if weighted pred is on or bipred */
    WORD32 store_16bit_output;

    recon_pic_buf_t *ps_ref_pic_l0, *ps_ref_pic_l1;
    UWORD8 *pu1_ref_pic, *pu1_ref_int_pel;
    WORD32 ref_pic_stride;

    /* offset of reference block in integer pel units */
    WORD32 frm_x_ofst, frm_y_ofst;
    WORD32 frm_x_pu, frm_y_pu;

    /* scratch 16 bit buffers for interpolation in l0 and l1 direction */
    WORD16 *pi2_scr_buf_l0 = &ps_inter_pred_ctxt->ai2_scratch_buf_l0[0];
    WORD16 *pi2_scr_buf_l1 = &ps_inter_pred_ctxt->ai2_scratch_buf_l1[0];

    /* scratch buffer for horizontal interpolation destination */
    WORD16 *pi2_horz_scratch = &ps_inter_pred_ctxt->ai2_horz_scratch[0];

    /* get PU's frm x and frm y offset : Note uv is interleaved */
    frm_x_pu = ps_inter_pred_ctxt->i4_ctb_frm_pos_x + (ps_pu->b4_pos_x << 2);
    frm_y_pu = (ps_inter_pred_ctxt->i4_ctb_frm_pos_y >> (u1_is_422 == 0)) +
               (ps_pu->b4_pos_y << (u1_is_422 + 1));

    /* sanity checks */
    ASSERT((wp_flag == 0) || (wp_flag == 1));
    ASSERT(dst_stride >= (pu_wd_chroma << 1)); /* uv interleaved */
    ASSERT(ps_pu->b1_intra_flag == 0);

    if(wp_flag)
    {
        UWORD8 u1_is_wgt_pred_L0, u1_is_wgt_pred_L1;

        if(inter_pred_idc != PRED_L1)
        {
            ps_ref_pic_l0 = ps_inter_pred_ctxt->ps_ref_list[0][ps_pu->mv.i1_l0_ref_idx];
            u1_is_wgt_pred_L0 = ps_ref_pic_l0->s_weight_offset.u1_chroma_weight_enable_flag;
        }
        if(inter_pred_idc != PRED_L0)
        {
            ps_ref_pic_l1 = ps_inter_pred_ctxt->ps_ref_list[1][ps_pu->mv.i1_l1_ref_idx];
            u1_is_wgt_pred_L1 = ps_ref_pic_l1->s_weight_offset.u1_chroma_weight_enable_flag;
        }
        if(inter_pred_idc == PRED_BI)
        {
            wp_flag = (u1_is_wgt_pred_L0 || u1_is_wgt_pred_L1);
        }
        else if(inter_pred_idc == PRED_L0)
        {
            wp_flag = u1_is_wgt_pred_L0;
        }
        else if(inter_pred_idc == PRED_L1)
        {
            wp_flag = u1_is_wgt_pred_L1;
        }
        else
        {
            /*other values are not allowed*/
            assert(0);
        }
    }
    store_16bit_output = (inter_pred_idc == PRED_BI) || (wp_flag);

    if(inter_pred_idc != PRED_L1)
    {
        /*****************************************************/
        /*              L0 inter prediction(Chroma )         */
        /*****************************************************/

        /* motion vecs in qpel precision                    */
        WORD32 mv_x = ps_pu->mv.s_l0_mv.i2_mvx;
        WORD32 mv_y = ps_pu->mv.s_l0_mv.i2_mvy;

        /* sub pel offsets in x and y direction w.r.t integer pel   */
        WORD32 dx = mv_x & 0x7;
        WORD32 dy = (mv_y & ((1 << (!u1_is_422 + 2)) - 1)) << u1_is_422;

        /* ref idx is currently stored in the lower 4bits           */
        WORD32 ref_idx = (ps_pu->mv.i1_l0_ref_idx);

        /*  x and y integer offsets w.r.t frame start               */

        frm_x_ofst = (frm_x_pu + ((mv_x >> 3) << 1)); /* uv interleaved */
        frm_y_ofst = (frm_y_pu + ((mv_y >> (3 - u1_is_422))));

        ps_ref_pic_l0 = ps_inter_pred_ctxt->ps_ref_list[0][ref_idx];

        /* picture buffer start and stride */
        pu1_ref_pic = (UWORD8 *)ps_ref_pic_l0->s_yuv_buf_desc.pv_u_buf;
        ref_pic_stride = ps_ref_pic_l0->s_yuv_buf_desc.i4_uv_strd;

        /* point to reference start location in ref frame           */
        /* Assuming clipping of mv is not required here as ME would */
        /* take care of mv access not going beyond padded data      */
        pu1_ref_int_pel = pu1_ref_pic + frm_x_ofst + (ref_pic_stride * frm_y_ofst);

        if(store_16bit_output)
        {
            /* do interpolation in 16bit L0 scratch buffer */
            ihevce_chroma_interpolate_16bit_dxdy(
                pu1_ref_int_pel,
                pi2_scr_buf_l0,
                ref_pic_stride,
                (pu_wd_chroma << 1),
                pi2_horz_scratch,
                pu_ht_chroma,
                pu_wd_chroma,
                dy,
                dx,
                ps_func_selector);
        }
        else
        {
            /* do interpolation in 8bit destination buffer and return */
            ihevce_chroma_interpolate_8bit_dxdy(
                pu1_ref_int_pel,
                pu1_dst_buf,
                ref_pic_stride,
                dst_stride,
                pi2_horz_scratch,
                pu_ht_chroma,
                pu_wd_chroma,
                dy,
                dx,
                ps_func_selector);

            return;
        }
    }

    if(inter_pred_idc != PRED_L0)
    {
        /*****************************************************/
        /*      L1 inter prediction(Chroma)                  */
        /*****************************************************/

        /* motion vecs in qpel precision                            */
        WORD32 mv_x = ps_pu->mv.s_l1_mv.i2_mvx;
        WORD32 mv_y = ps_pu->mv.s_l1_mv.i2_mvy;

        /* sub pel offsets in x and y direction w.r.t integer pel   */
        WORD32 dx = mv_x & 0x7;
        WORD32 dy = (mv_y & ((1 << (!u1_is_422 + 2)) - 1)) << u1_is_422;

        /* ref idx is currently stored in the lower 4bits           */
        WORD32 ref_idx = (ps_pu->mv.i1_l1_ref_idx);

        /*  x and y integer offsets w.r.t frame start               */
        frm_x_ofst = (frm_x_pu + ((mv_x >> 3) << 1)); /* uv interleaved */
        frm_y_ofst = (frm_y_pu + ((mv_y >> (3 - u1_is_422))));

        ps_ref_pic_l1 = ps_inter_pred_ctxt->ps_ref_list[1][ref_idx];

        /* picture buffer start and stride */
        pu1_ref_pic = (UWORD8 *)ps_ref_pic_l1->s_yuv_buf_desc.pv_u_buf;
        ref_pic_stride = ps_ref_pic_l1->s_yuv_buf_desc.i4_uv_strd;

        /* point to reference start location in ref frame           */
        /* Assuming clipping of mv is not required here as ME would */
        /* take care of mv access not going beyond padded data      */
        pu1_ref_int_pel = pu1_ref_pic + frm_x_ofst + (ref_pic_stride * frm_y_ofst);

        if(store_16bit_output)
        {
            /* do interpolation in 16bit L1 scratch buffer */
            ihevce_chroma_interpolate_16bit_dxdy(
                pu1_ref_int_pel,
                pi2_scr_buf_l1,
                ref_pic_stride,
                (pu_wd_chroma << 1),
                pi2_horz_scratch,
                pu_ht_chroma,
                pu_wd_chroma,
                dy,
                dx,
                ps_func_selector);
        }
        else
        {
            /* do interpolation in 8bit destination buffer and return */
            ihevce_chroma_interpolate_8bit_dxdy(
                pu1_ref_int_pel,
                pu1_dst_buf,
                ref_pic_stride,
                dst_stride,
                pi2_horz_scratch,
                pu_ht_chroma,
                pu_wd_chroma,
                dy,
                dx,
                ps_func_selector);

            return;
        }
    }

    if((inter_pred_idc != PRED_BI) && wp_flag)
    {
        /*****************************************************/
        /*      unidirection weighted prediction(Chroma)     */
        /*****************************************************/
        ihevce_wght_offst_t *ps_weight_offset;
        WORD16 *pi2_src;
        WORD32 lvl_shift = 0;
        WORD32 wgt_cb, wgt_cr, off_cb, off_cr;
        WORD32 shift;

        /* intialize the weight, offsets and ref based on l0/l1 mode */
        if(inter_pred_idc == PRED_L0)
        {
            pi2_src = pi2_scr_buf_l0;
            ps_weight_offset = &ps_ref_pic_l0->s_weight_offset;
        }
        else
        {
            pi2_src = pi2_scr_buf_l1;
            ps_weight_offset = &ps_ref_pic_l1->s_weight_offset;
        }

        wgt_cb = ps_weight_offset->i2_cb_weight;
        off_cb = ps_weight_offset->i2_cb_offset;
        wgt_cr = ps_weight_offset->i2_cr_weight;
        off_cr = ps_weight_offset->i2_cr_offset;

        shift = ps_inter_pred_ctxt->i4_log2_chroma_wght_denom + SHIFT_14_MINUS_BIT_DEPTH;

        /* do the uni directional weighted prediction */
        ps_func_selector->ihevc_weighted_pred_chroma_uni_fptr(
            pi2_src,
            pu1_dst_buf,
            (pu_wd_chroma << 1),
            dst_stride,
            wgt_cb,
            wgt_cr,
            off_cb,
            off_cr,
            shift,
            lvl_shift,
            pu_ht_chroma,
            pu_wd_chroma);
    }
    else
    {
        /*****************************************************/
        /*              Bipred  prediction(Chroma)           */
        /*****************************************************/
        if(wp_flag)
        {
            WORD32 wgt0_cb, wgt1_cb, wgt0_cr, wgt1_cr;
            WORD32 off0_cb, off1_cb, off0_cr, off1_cr;
            WORD32 shift;

            /*****************************************************/
            /*      Bi pred  weighted prediction (Chroma)        */
            /*****************************************************/
            wgt0_cb = ps_ref_pic_l0->s_weight_offset.i2_cb_weight;
            off0_cb = ps_ref_pic_l0->s_weight_offset.i2_cb_offset;

            wgt0_cr = ps_ref_pic_l0->s_weight_offset.i2_cr_weight;
            off0_cr = ps_ref_pic_l0->s_weight_offset.i2_cr_offset;

            wgt1_cb = ps_ref_pic_l1->s_weight_offset.i2_cb_weight;
            off1_cb = ps_ref_pic_l1->s_weight_offset.i2_cb_offset;

            wgt1_cr = ps_ref_pic_l1->s_weight_offset.i2_cr_weight;
            off1_cr = ps_ref_pic_l1->s_weight_offset.i2_cr_offset;

            shift = ps_inter_pred_ctxt->i4_log2_chroma_wght_denom + SHIFT_14_MINUS_BIT_DEPTH + 1;

            ps_func_selector->ihevc_weighted_pred_chroma_bi_fptr(
                pi2_scr_buf_l0,
                pi2_scr_buf_l1,
                pu1_dst_buf,
                (pu_wd_chroma << 1),
                (pu_wd_chroma << 1),
                dst_stride,
                wgt0_cb,
                wgt0_cr,
                off0_cb,
                off0_cr,
                wgt1_cb,
                wgt1_cr,
                off1_cb,
                off1_cr,
                shift,
                0,
                0,
                pu_ht_chroma,
                pu_wd_chroma);
        }
        else
        {
            /*****************************************************/
            /*          Default Bi pred  prediction (Chroma)     */
            /*****************************************************/
            ps_func_selector->ihevc_weighted_pred_chroma_bi_default_fptr(
                pi2_scr_buf_l0,
                pi2_scr_buf_l1,
                pu1_dst_buf,
                (pu_wd_chroma << 1),
                (pu_wd_chroma << 1),
                dst_stride,
                0,
                0,
                pu_ht_chroma,
                pu_wd_chroma);
        }
    }
}
