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
* \file ihevce_enc_subpel_gen.c
*
* \brief
*    This file contains Padding and Subpel plane generation functions
*    at CTB level
*
* \date
*    29/12/2012
*
* \author
*    Ittiam
*
*
* List of Functions
* - ihevce_suppel_padding()
* - ihevce_pad_interp_recon_ctb()
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
#include "ihevc_debug.h"
#include "ihevc_macros.h"
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
#include "ihevc_trans_tables.h"
#include "ihevc_trans_macros.h"

#include "ihevce_defs.h"
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
#include "ihevce_global_tables.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_inter_pred.h"
#include "ihevce_common_utils.h"

/*!
******************************************************************************
* \if Function name : ihevce_suppel_padding \endif
*
* \brief
*    Subpel Plane planes Padding Function
*
* \param[in] pu1_dst : pointer to subpel plane
*            stride  : subpel plane stride same as recon stride
*            tot_wd  : width of the block in subpel plane
*            tot_ht  : hieght of the block in subpel plane
*            ctb_ctr   : ctb horizontal position
*            vert_ctr  : ctb vertical position
*            ps_frm_ctb_prms : CTB characteristics parameters
* \return
*    None
*
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_subpel_padding(
    UWORD8 *pu1_dst,
    WORD32 stride,
    WORD32 tot_wd,
    WORD32 tot_ht,
    WORD32 pad_subpel_x,
    WORD32 pad_subpel_y,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    WORD32 i4_num_ctbs_horz,
    WORD32 i4_num_ctbs_vert,
    func_selector_t *ps_func_selector)
{
    ihevc_pad_top_ft *pf_pad_top = ps_func_selector->ihevc_pad_top_fptr;
    ihevc_pad_bottom_ft *pf_pad_bottom = ps_func_selector->ihevc_pad_bottom_fptr;
    ihevc_pad_left_luma_ft *pf_pad_left_luma = ps_func_selector->ihevc_pad_left_luma_fptr;
    ihevc_pad_right_luma_ft *pf_pad_right_luma = ps_func_selector->ihevc_pad_right_luma_fptr;

    UWORD8 *pu1_dst_tmp = pu1_dst;
    WORD32 cpy_ht = tot_ht;

    /* Top padding*/
    if(vert_ctr == 0)
    {
        PAD_BUF_VER(pu1_dst, stride, tot_wd, pad_subpel_x, pad_subpel_y, pf_pad_top);
        /*if curr ctb is 1st ctb in ctb row, update dst pointer for Left padding*/
        pu1_dst_tmp = pu1_dst - pad_subpel_y * stride;
        cpy_ht += pad_subpel_y;
    }
    /*bottom padding*/
    if(vert_ctr == (i4_num_ctbs_vert - 1))
    {
        PAD_BUF_VER(
            (pu1_dst + (tot_ht * stride)),
            stride,
            tot_wd,
            pad_subpel_x,
            pad_subpel_y,
            pf_pad_bottom);
        /*if curr ctb is 1st ctb in ctb row, update dst pointer for right padding*/
        cpy_ht += pad_subpel_y;
    }

    /*left padding*/
    if(ctb_ctr == 0)
    {
        PAD_BUF_HOR(pu1_dst_tmp, stride, cpy_ht, pad_subpel_x, pad_subpel_y, pf_pad_left_luma);
    }

    /*right padding*/
    if(ctb_ctr == (i4_num_ctbs_horz - 1))
    {
        PAD_BUF_HOR(
            pu1_dst_tmp + tot_wd, stride, cpy_ht, pad_subpel_x, pad_subpel_y, pf_pad_right_luma);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_pad_interp_recon_ctb \endif
*
* \brief
*    Ctb level Subpel Plane generation and padding function
*
* \param[in]
* s_cu_prms : coding unit params structures (recon buffers)
*            ctb_ctr   : ctb horizontal position
*            vert_ctr  : ctb vertical position
*            ps_frm_ctb_prms : CTB characteristics parameters
* i4_dist_nbr_mask : nbr-mask for distributed mode. Should be 0 for standalone
*                    or distributed-single-client mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_pad_interp_recon_ctb(
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    WORD32 quality_preset,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD16 *pi2_hxhy_interm,
    WORD32 i4_bitrate_instance_id,
    func_selector_t *ps_func_selector)
{
    UWORD8 *pu1_src, *pu1_src_uv;
    WORD32 stride, stride_uv, wd, ht, wd_uv, ht_uv, pad_x, pad_y, pad_subpel_x, pad_subpel_y;
    WORD32 tot_wd, tot_ht, offset, cpy_ht_y, cpy_ht_uv;
    WORD32 i4_chroma_vert_pad_default;

    WORD32 ctb_size = ps_frm_ctb_prms->i4_ctb_size;
    UWORD8 *pu1_dst_hxfy = ps_pad_interp_recon->pu1_sbpel_hxfy +
                           (vert_ctr * ctb_size * ps_pad_interp_recon->i4_luma_recon_stride) +
                           (ctb_ctr * ctb_size);
    UWORD8 *pu1_dst_fxhy = ps_pad_interp_recon->pu1_sbpel_fxhy +
                           (vert_ctr * ctb_size * ps_pad_interp_recon->i4_luma_recon_stride) +
                           (ctb_ctr * ctb_size);
    UWORD8 *pu1_dst_hxhy = ps_pad_interp_recon->pu1_sbpel_hxhy +
                           (vert_ctr * ctb_size * ps_pad_interp_recon->i4_luma_recon_stride) +
                           (ctb_ctr * ctb_size);
    UWORD8 u1_is_422 = (ps_pad_interp_recon->u1_chroma_array_type == 2);

    ihevc_inter_pred_ft *pf_inter_pred_luma_horz =
        ps_func_selector->ihevc_inter_pred_luma_horz_fptr;
    ihevc_inter_pred_ft *pf_inter_pred_luma_vert =
        ps_func_selector->ihevc_inter_pred_luma_vert_fptr;
    ihevc_inter_pred_w16out_ft *pf_inter_pred_luma_horz_w16out =
        ps_func_selector->ihevc_inter_pred_luma_horz_w16out_fptr;
    ihevc_inter_pred_w16inp_ft *pf_inter_pred_luma_vert_w16inp =
        ps_func_selector->ihevc_inter_pred_luma_vert_w16inp_fptr;
    stride = ps_pad_interp_recon->i4_luma_recon_stride;
    wd = ps_pad_interp_recon->i4_ctb_size;
    ht = ps_pad_interp_recon->i4_ctb_size;

    pu1_src = (UWORD8 *)ps_pad_interp_recon->pu1_luma_recon + (vert_ctr * ctb_size * stride) +
              (ctb_ctr * ctb_size);

    stride_uv = ps_pad_interp_recon->i4_chrm_recon_stride;
    wd_uv = ps_pad_interp_recon->i4_ctb_size;
    ht_uv = ps_pad_interp_recon->i4_ctb_size >> (0 == u1_is_422);

    pu1_src_uv = (UWORD8 *)ps_pad_interp_recon->pu1_chrm_recon +
                 (vert_ctr * (ctb_size >> (0 == u1_is_422)) * stride_uv) + (ctb_ctr * ctb_size);

    pad_x = ALIGN8(NTAPS_LUMA);
    pad_y = ALIGN8(NTAPS_LUMA);
    pad_subpel_x = PAD_HORZ - pad_x;
    pad_subpel_y = PAD_VERT - pad_y;

    offset = pad_x + (pad_y * stride);

    tot_wd = wd + (pad_x << 1);
    tot_ht = ht + (pad_y << 1);

    i4_chroma_vert_pad_default = PAD_VERT >> (0 == u1_is_422);

    if(ctb_ctr == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
    {
        WORD32 last_ctb_x =
            ps_frm_ctb_prms->i4_cu_aligned_pic_wd -
            ((ps_frm_ctb_prms->i4_num_ctbs_horz - 1) * ps_pad_interp_recon->i4_ctb_size);
        wd = last_ctb_x;
        wd_uv = last_ctb_x;
    }
    if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
    {
        WORD32 last_ctb_y =
            ps_frm_ctb_prms->i4_cu_aligned_pic_ht -
            ((ps_frm_ctb_prms->i4_num_ctbs_vert - 1) * ps_pad_interp_recon->i4_ctb_size);
        ht = last_ctb_y;
        ht_uv = last_ctb_y >> (0 == u1_is_422);
    }
    tot_ht = ht;
    tot_wd = wd;

    /*top padding*/
    if(vert_ctr == 0)
    {
        tot_ht = pad_y + ht - 8;
    }
    /*bottom padding*/
    if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
    {
        tot_ht = pad_y + ht + 8;
    }

    /*Left padding*/
    if(ctb_ctr == 0)
    {
        tot_wd = pad_x + wd - 8;
    }
    /*right padding*/
    if(ctb_ctr == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
    {
        tot_wd = pad_x + wd + 8;
    }

    pu1_src -= offset;
    pu1_dst_hxhy -= offset;
    pu1_dst_hxfy -= offset;
    pu1_dst_fxhy -= offset;

    {
        tot_wd = ALIGN16(tot_wd);
        if(0 ==
           i4_bitrate_instance_id)  //do the following subpel calculations for reference bit-rate instance only
        {
            /* HxFY plane */
            pf_inter_pred_luma_horz(
                pu1_src,
                pu1_dst_hxfy,
                stride,
                stride,
                (WORD8 *)gai1_hevc_luma_filter_taps[2],
                tot_ht,
                tot_wd);

            pf_inter_pred_luma_vert(
                pu1_src,
                pu1_dst_fxhy,
                stride,
                stride,
                (WORD8 *)gai1_hevc_luma_filter_taps[2],
                tot_ht,
                tot_wd);

            pf_inter_pred_luma_horz_w16out(
                pu1_src - 3 * stride,
                pi2_hxhy_interm,
                stride,
                tot_wd,
                (WORD8 *)gai1_hevc_luma_filter_taps[2],
                (tot_ht + 7),
                tot_wd);

            /* "Stride" of intermediate buffer in pixels,equals tot_wd */
            pf_inter_pred_luma_vert_w16inp(
                pi2_hxhy_interm + (3 * tot_wd),
                pu1_dst_hxhy,
                tot_wd,
                stride,
                (WORD8 *)gai1_hevc_luma_filter_taps[2],
                tot_ht,
                tot_wd);

            ihevce_subpel_padding(
                pu1_dst_fxhy,
                stride,
                tot_wd,
                tot_ht,
                pad_subpel_x,
                pad_subpel_y,
                ctb_ctr,
                vert_ctr,
                ps_frm_ctb_prms->i4_num_ctbs_horz,
                ps_frm_ctb_prms->i4_num_ctbs_vert,
                ps_func_selector);

            ihevce_subpel_padding(
                pu1_dst_hxfy,
                stride,
                tot_wd,
                tot_ht,
                pad_subpel_x,
                pad_subpel_y,
                ctb_ctr,
                vert_ctr,
                ps_frm_ctb_prms->i4_num_ctbs_horz,
                ps_frm_ctb_prms->i4_num_ctbs_vert,
                ps_func_selector);

            ihevce_subpel_padding(
                pu1_dst_hxhy,
                stride,
                tot_wd,
                tot_ht,
                pad_subpel_x,
                pad_subpel_y,
                ctb_ctr,
                vert_ctr,
                ps_frm_ctb_prms->i4_num_ctbs_horz,
                ps_frm_ctb_prms->i4_num_ctbs_vert,
                ps_func_selector);
        }
    }
}

void ihevce_recon_padding(
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    func_selector_t *ps_func_selector)
{
    UWORD8 *pu1_src, *pu1_src_uv, *pu1_buf_y, *pu1_buf_uv;
    WORD32 stride, stride_uv, wd, ht, wd_uv, ht_uv;
    WORD32 cpy_ht_y, cpy_ht_uv;
    WORD32 i4_chroma_vert_pad_default;

    WORD32 top_extra_pix = 0, left_extra_pix = 0;
    WORD32 ctb_size = ps_frm_ctb_prms->i4_ctb_size;
    UWORD8 u1_is_422 = (ps_pad_interp_recon->u1_chroma_array_type == 2);

    ihevc_pad_top_ft *pf_pad_top = ps_func_selector->ihevc_pad_top_fptr;
    ihevc_pad_bottom_ft *pf_pad_bottom = ps_func_selector->ihevc_pad_bottom_fptr;
    ihevc_pad_left_luma_ft *pf_pad_left_luma = ps_func_selector->ihevc_pad_left_luma_fptr;
    ihevc_pad_left_chroma_ft *pf_pad_left_chroma = ps_func_selector->ihevc_pad_left_chroma_fptr;
    ihevc_pad_right_luma_ft *pf_pad_right_luma = ps_func_selector->ihevc_pad_right_luma_fptr;
    ihevc_pad_right_chroma_ft *pf_pad_right_chroma = ps_func_selector->ihevc_pad_right_chroma_fptr;

    stride = ps_pad_interp_recon->i4_luma_recon_stride;
    wd = ps_pad_interp_recon->i4_ctb_size;
    ht = ps_pad_interp_recon->i4_ctb_size;

    pu1_src = (UWORD8 *)ps_pad_interp_recon->pu1_luma_recon + (vert_ctr * ctb_size * stride) +
              (ctb_ctr * ctb_size);

    stride_uv = ps_pad_interp_recon->i4_chrm_recon_stride;
    wd_uv = ps_pad_interp_recon->i4_ctb_size;
    ht_uv = ps_pad_interp_recon->i4_ctb_size >> (0 == u1_is_422);

    pu1_src_uv = (UWORD8 *)ps_pad_interp_recon->pu1_chrm_recon +
                 (vert_ctr * (ctb_size >> (0 == u1_is_422)) * stride_uv) + (ctb_ctr * ctb_size);

    i4_chroma_vert_pad_default = PAD_VERT >> (0 == u1_is_422);

    if(ctb_ctr == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
    {
        WORD32 last_ctb_x =
            ps_frm_ctb_prms->i4_cu_aligned_pic_wd -
            ((ps_frm_ctb_prms->i4_num_ctbs_horz - 1) * ps_pad_interp_recon->i4_ctb_size);
        wd = last_ctb_x;
        wd_uv = last_ctb_x;
    }
    if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
    {
        WORD32 last_ctb_y =
            ps_frm_ctb_prms->i4_cu_aligned_pic_ht -
            ((ps_frm_ctb_prms->i4_num_ctbs_vert - 1) * ps_pad_interp_recon->i4_ctb_size);
        ht = last_ctb_y;
        ht_uv = last_ctb_y >> (0 == u1_is_422);
    }

    pu1_buf_y = pu1_src;
    pu1_buf_uv = pu1_src_uv;
    cpy_ht_y = ht;
    cpy_ht_uv = ht_uv;
    if(vert_ctr > 0)
    {
        top_extra_pix = 8;
    }
    if(ctb_ctr > 0)
    {
        left_extra_pix = 8;
    }

    /*top padding*/
    if(vert_ctr == 0)
    {
        PAD_BUF_VER(
            pu1_src - left_extra_pix, stride, wd + left_extra_pix, PAD_HORZ, PAD_VERT, pf_pad_top);
        PAD_BUF_VER(
            pu1_src_uv - left_extra_pix,
            stride_uv,
            wd_uv + left_extra_pix,
            PAD_HORZ,
            i4_chroma_vert_pad_default,
            pf_pad_top);
        /*if curr ctb is 1st ctb in ctb row, update dst pointer for Left padding*/
        pu1_buf_y = pu1_src - PAD_VERT * stride;
        pu1_buf_uv = pu1_src_uv - i4_chroma_vert_pad_default * stride_uv;
        cpy_ht_y += PAD_VERT;
        cpy_ht_uv += i4_chroma_vert_pad_default;
    }

    /*bottom padding*/
    if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
    {
        PAD_BUF_VER(
            ((pu1_src - left_extra_pix) + (ht * stride)),
            stride,
            wd + left_extra_pix,
            PAD_HORZ,
            PAD_VERT,
            pf_pad_bottom);
        PAD_BUF_VER(
            ((pu1_src_uv - left_extra_pix) + (ht_uv * stride_uv)),
            stride_uv,
            wd_uv + left_extra_pix,
            PAD_HORZ,
            i4_chroma_vert_pad_default,
            pf_pad_bottom);
        /*if curr ctb is 1st ctb in ctb row, update dst pointer for right padding*/
        cpy_ht_y += PAD_VERT;
        cpy_ht_uv += i4_chroma_vert_pad_default;
    }

    /*Left padding*/
    if(ctb_ctr == 0)
    {
        PAD_BUF_HOR(
            (pu1_buf_y - top_extra_pix * stride),
            stride,
            cpy_ht_y + top_extra_pix,
            PAD_HORZ,
            PAD_VERT,
            pf_pad_left_luma);
        PAD_BUF_HOR(
            pu1_buf_uv - (top_extra_pix >> 1) * (u1_is_422 + 1) * stride_uv,
            stride_uv,
            cpy_ht_uv + (top_extra_pix >> 1) * (u1_is_422 + 1),
            PAD_HORZ,
            i4_chroma_vert_pad_default,
            pf_pad_left_chroma);
    }

    /*right padding*/
    if(ctb_ctr == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
    {
        PAD_BUF_HOR(
            ((pu1_buf_y - (top_extra_pix * stride)) + wd),
            stride,
            cpy_ht_y + top_extra_pix,
            PAD_HORZ,
            PAD_VERT,
            pf_pad_right_luma);
        PAD_BUF_HOR(
            ((pu1_buf_uv - ((top_extra_pix >> 1) * (u1_is_422 + 1) * stride_uv)) + wd_uv),
            stride_uv,
            cpy_ht_uv + (top_extra_pix >> 1) * (u1_is_422 + 1),
            PAD_HORZ,
            i4_chroma_vert_pad_default,
            pf_pad_right_chroma);
    }
}

void ihevce_pad_interp_recon_src_ctb(
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 i4_bitrate_instance_id,
    func_selector_t *ps_func_selector,
    WORD32 is_chroma_needs_padding)
{
    UWORD8 *pu1_src, *pu1_src_uv;
    WORD32 stride, stride_uv, wd, ht, wd_uv, ht_uv, pad_x, pad_y;
    WORD32 tot_wd, tot_ht;
    WORD32 i4_chroma_vert_pad_default;

    WORD32 ctb_size = ps_frm_ctb_prms->i4_ctb_size;
    UWORD8 u1_is_422 = (ps_pad_interp_recon->u1_chroma_array_type == 2);

    ihevc_pad_top_ft *pf_pad_top = ps_func_selector->ihevc_pad_top_fptr;
    ihevc_pad_bottom_ft *pf_pad_bottom = ps_func_selector->ihevc_pad_bottom_fptr;
    ihevc_pad_left_luma_ft *pf_pad_left_luma = ps_func_selector->ihevc_pad_left_luma_fptr;
    ihevc_pad_left_chroma_ft *pf_pad_left_chroma = ps_func_selector->ihevc_pad_left_chroma_fptr;
    ihevc_pad_right_luma_ft *pf_pad_right_luma = ps_func_selector->ihevc_pad_right_luma_fptr;
    ihevc_pad_right_chroma_ft *pf_pad_right_chroma = ps_func_selector->ihevc_pad_right_chroma_fptr;

    /* Luma padding */
    pu1_src = (UWORD8 *)ps_pad_interp_recon->pu1_luma_recon_src +
              (vert_ctr * ctb_size * ps_pad_interp_recon->i4_luma_recon_stride) +
              (ctb_ctr * ctb_size);

    stride = ps_pad_interp_recon->i4_luma_recon_stride;
    wd = ps_pad_interp_recon->i4_ctb_size;
    ht = ps_pad_interp_recon->i4_ctb_size;

    pu1_src_uv =
        (UWORD8 *)ps_pad_interp_recon->pu1_chrm_recon_src +
        (vert_ctr * (ctb_size >> (0 == u1_is_422)) * ps_pad_interp_recon->i4_chrm_recon_stride) +
        (ctb_ctr * ctb_size);

    stride_uv = ps_pad_interp_recon->i4_chrm_recon_stride;
    wd_uv = ps_pad_interp_recon->i4_ctb_size;
    ht_uv = ps_pad_interp_recon->i4_ctb_size >> (0 == u1_is_422);

    pad_x = ALIGN8(NTAPS_LUMA);
    pad_y = ALIGN8(NTAPS_LUMA);

    tot_wd = wd + (pad_x << 1);
    tot_ht = ht + (pad_y << 1);

    i4_chroma_vert_pad_default = PAD_VERT >> (0 == u1_is_422);

    if(ctb_ctr == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
    {
        WORD32 last_ctb_x =
            ps_frm_ctb_prms->i4_cu_aligned_pic_wd -
            ((ps_frm_ctb_prms->i4_num_ctbs_horz - 1) * ps_pad_interp_recon->i4_ctb_size);
        wd = last_ctb_x;
        wd_uv = last_ctb_x;
    }
    if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
    {
        WORD32 last_ctb_y =
            ps_frm_ctb_prms->i4_cu_aligned_pic_ht -
            ((ps_frm_ctb_prms->i4_num_ctbs_vert - 1) * ps_pad_interp_recon->i4_ctb_size);
        ht = last_ctb_y;
        ht_uv = last_ctb_y >> (0 == u1_is_422);
    }

    if(ctb_ctr == 0)
    {
        if(vert_ctr == 0)
        {
            PAD_BUF_HOR(pu1_src, stride, ht, PAD_HORZ, PAD_VERT, pf_pad_left_luma);
            PAD_BUF_VER(pu1_src - PAD_HORZ, stride, PAD_HORZ + wd, PAD_HORZ, PAD_VERT, pf_pad_top);
            if(is_chroma_needs_padding)
            {
                PAD_BUF_HOR(
                    pu1_src_uv,
                    stride_uv,
                    ht_uv,
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_left_chroma);
                PAD_BUF_VER(
                    pu1_src_uv - PAD_HORZ,
                    stride_uv,
                    PAD_HORZ + wd_uv,
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_top);
            }
        }
        else if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
        {
            PAD_BUF_HOR(pu1_src - 8 * stride, stride, ht + 8, PAD_HORZ, PAD_VERT, pf_pad_left_luma);
            PAD_BUF_VER(
                (pu1_src - PAD_HORZ + (ht * stride)),
                stride,
                PAD_HORZ + wd,
                PAD_HORZ,
                PAD_VERT,
                pf_pad_bottom);
            if(is_chroma_needs_padding)
            {
                PAD_BUF_HOR(
                    pu1_src_uv - 4 * (u1_is_422 + 1) * stride_uv,
                    stride_uv,
                    ht_uv + 4 * (u1_is_422 + 1),
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_left_chroma);
                PAD_BUF_VER(
                    (pu1_src_uv - PAD_HORZ + (ht_uv * stride_uv)),
                    stride_uv,
                    PAD_HORZ + wd_uv,
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_bottom);
            }
        }
        else
        {
            PAD_BUF_HOR(pu1_src - 8 * stride, stride, ht + 8, PAD_HORZ, PAD_VERT, pf_pad_left_luma);
            if(is_chroma_needs_padding)
            {
                PAD_BUF_HOR(
                    pu1_src_uv - 4 * (u1_is_422 + 1) * stride_uv,
                    stride_uv,
                    ht_uv + 4 * (u1_is_422 + 1),
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_left_chroma);
            }
        }
    }
    else if(ctb_ctr == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
    {
        if(vert_ctr == 0)
        {
            PAD_BUF_HOR(pu1_src + wd, stride, ht, PAD_HORZ, PAD_VERT, pf_pad_right_luma);
            PAD_BUF_VER(pu1_src - 8, stride, PAD_HORZ + (wd + 8), PAD_HORZ, PAD_VERT, pf_pad_top);
            if(is_chroma_needs_padding)
            {
                PAD_BUF_HOR(
                    pu1_src_uv + wd_uv,
                    stride_uv,
                    ht_uv,
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_right_chroma);
                PAD_BUF_VER(
                    pu1_src_uv - 8,
                    stride_uv,
                    PAD_HORZ + (wd_uv + 8),
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_top);
            }
        }
        else if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
        {
            PAD_BUF_HOR(
                (pu1_src - (8 * stride) + wd),
                stride,
                ht + 8,
                PAD_HORZ,
                PAD_VERT,
                pf_pad_right_luma);
            PAD_BUF_VER(
                (pu1_src - 8 + (ht * stride)),
                stride,
                PAD_HORZ + (wd + 8),
                PAD_HORZ,
                PAD_VERT,
                pf_pad_bottom);
            if(is_chroma_needs_padding)
            {
                PAD_BUF_HOR(
                    (pu1_src_uv - (4 * (u1_is_422 + 1) * stride_uv) + wd_uv),
                    stride_uv,
                    ht_uv + 4 * (u1_is_422 + 1),
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_right_chroma);
                PAD_BUF_VER(
                    (pu1_src_uv - 8 + (ht_uv * stride_uv)),
                    stride_uv,
                    PAD_HORZ + (wd_uv + 8),
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_bottom);
            }
        }
        else
        {
            PAD_BUF_HOR(
                (pu1_src - (8 * stride) + wd),
                stride,
                ht + 8,
                PAD_HORZ,
                PAD_VERT,
                pf_pad_right_luma);
            if(is_chroma_needs_padding)
            {
                PAD_BUF_HOR(
                    (pu1_src_uv - (4 * (u1_is_422 + 1) * stride_uv) + wd_uv),
                    stride_uv,
                    ht_uv + 4 * (u1_is_422 + 1),
                    PAD_HORZ,
                    i4_chroma_vert_pad_default,
                    pf_pad_right_chroma);
            }
        }
    }
    else if(vert_ctr == 0)
    {
        PAD_BUF_VER(pu1_src - 8, stride, (wd + 8), PAD_HORZ, PAD_VERT, pf_pad_top);
        if(is_chroma_needs_padding)
        {
            PAD_BUF_VER(
                pu1_src_uv - 8,
                stride_uv,
                (wd_uv + 8),
                PAD_HORZ,
                i4_chroma_vert_pad_default,
                pf_pad_top);
        }
    }
    else if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
    {
        PAD_BUF_VER(
            (pu1_src - 8 + (ht * stride)), stride, (wd + 8), PAD_HORZ, PAD_VERT, pf_pad_bottom);
        if(is_chroma_needs_padding)
        {
            PAD_BUF_VER(
                (pu1_src_uv - 8 + (ht_uv * stride_uv)),
                stride_uv,
                (wd_uv + 8),
                PAD_HORZ,
                i4_chroma_vert_pad_default,
                pf_pad_bottom);
        }
    }
}
