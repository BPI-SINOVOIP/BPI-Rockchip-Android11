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
*  ihevce_common_utils_neon.c
*
* @brief
*  Contains intrinsic definitions of functions for sao param
*
* @author
*  ittiam
*
* @par List of Functions:
*  - ihevce_get_luma_eo_sao_params_neon()
*  - ihevce_get_chroma_eo_sao_params_neon()
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <arm_neon.h>

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
#include "ihevc_cmn_utils_neon.h"

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

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static void ihevce_wt_avg_2d_16x1_neon(
    UWORD8 *pu1_pred0,
    UWORD8 *pu1_pred1,
    UWORD8 *pu1_dst,
    WORD32 w0,
    WORD32 w1,
    WORD32 rnd,
    WORD32 shift)
{
    uint8x16_t a0, a1;
    int32x4_t a6, a7, a9;
    int32x4_t reg0[4], reg1[4];
    int16x8_t a2, a3, a4, a5, a8;

    a8 = vdupq_n_s16((WORD16)rnd);

    a6 = vdupq_n_s32(w0);
    a7 = vdupq_n_s32(w1);
    a9 = vdupq_n_s32(-shift);

    a0 = vld1q_u8(pu1_pred0);
    a1 = vld1q_u8(pu1_pred1);

    a2 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a0)));
    a3 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(a0)));
    a4 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(a1)));
    a5 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(a1)));

    reg0[0] = vmovl_s16(vget_low_s16(a2));
    reg0[1] = vmovl_s16(vget_high_s16(a2));
    reg0[2] = vmovl_s16(vget_low_s16(a3));
    reg0[3] = vmovl_s16(vget_high_s16(a3));

    reg1[0] = vmovl_s16(vget_low_s16(a4));
    reg1[1] = vmovl_s16(vget_high_s16(a4));
    reg1[2] = vmovl_s16(vget_low_s16(a5));
    reg1[3] = vmovl_s16(vget_high_s16(a5));

    reg0[0] = vmulq_s32(reg0[0], a6);
    reg0[1] = vmulq_s32(reg0[1], a6);
    reg0[2] = vmulq_s32(reg0[2], a6);
    reg0[3] = vmulq_s32(reg0[3], a6);

    reg1[0] = vmulq_s32(reg1[0], a7);
    reg1[1] = vmulq_s32(reg1[1], a7);
    reg1[2] = vmulq_s32(reg1[2], a7);
    reg1[3] = vmulq_s32(reg1[3], a7);

    reg0[0] = vaddq_s32(reg0[0], reg1[0]);
    reg0[1] = vaddq_s32(reg0[1], reg1[1]);
    reg0[2] = vaddq_s32(reg0[2], reg1[2]);
    reg0[3] = vaddq_s32(reg0[3], reg1[3]);

    reg0[0] = vshlq_s32(reg0[0], a9);
    reg0[1] = vshlq_s32(reg0[1], a9);
    reg0[2] = vshlq_s32(reg0[2], a9);
    reg0[3] = vshlq_s32(reg0[3], a9);  // (p0*w0 + p1*w1) >> shift

    a2 = vcombine_s16(vmovn_s32(reg0[0]), vmovn_s32(reg0[1]));
    a3 = vcombine_s16(vmovn_s32(reg0[2]), vmovn_s32(reg0[3]));

    a2 = vaddq_s16(a2, a8);
    a3 = vaddq_s16(a3, a8);  // ((p0*w0 + p1*w1) >> shift) + rnd
    a0 = vcombine_u8(vqmovun_s16(a2), vqmovun_s16(a3));

    vst1q_u8(pu1_dst, a0);
}

static void ihevce_wt_avg_2d_8x1_neon(
    UWORD8 *pu1_pred0,
    UWORD8 *pu1_pred1,
    UWORD8 *pu1_dst,
    WORD32 w0,
    WORD32 w1,
    WORD32 rnd,
    WORD32 shift)
{
    uint8x8_t a2, a3;
    int16x8_t a0, a1, a6;
    int32x4_t a4, a5, a7, a8, a9, a10, a11;

    a6 = vdupq_n_s16((WORD16)rnd);

    a4 = vdupq_n_s32(w0);
    a5 = vdupq_n_s32(w1);
    a7 = vdupq_n_s32((-shift));

    a2 = vld1_u8(pu1_pred0);
    a3 = vld1_u8(pu1_pred1);
    a0 = vreinterpretq_s16_u16(vmovl_u8(a2));
    a1 = vreinterpretq_s16_u16(vmovl_u8(a3));

    a8 = vmovl_s16(vget_low_s16(a0));
    a9 = vmovl_s16(vget_high_s16(a0));
    a10 = vmovl_s16(vget_low_s16(a1));
    a11 = vmovl_s16(vget_high_s16(a1));

    a8 = vmulq_s32(a8, a4);
    a9 = vmulq_s32(a9, a4);
    a10 = vmulq_s32(a10, a5);
    a11 = vmulq_s32(a11, a5);

    a8 = vaddq_s32(a8, a10);
    a10 = vaddq_s32(a9, a11);

    a8 = vshlq_s32(a8, a7);
    a10 = vshlq_s32(a10, a7);

    a0 = vcombine_s16(vmovn_s32(a8), vmovn_s32(a10));
    a0 = vaddq_s16(a0, a6);
    a2 = vqmovun_s16(a0);
    vst1_u8(pu1_dst, a2);
}

static void ihevce_wt_avg_2d_4xn_neon(
    UWORD8 *pu1_pred0,
    UWORD8 *pu1_pred1,
    WORD32 pred0_strd,
    WORD32 pred1_strd,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 w0,
    WORD32 w1,
    WORD32 rnd,
    WORD32 shift)
{
    WORD32 i, j;
    uint8x16_t src0_u8, src1_u8;
    uint16x8_t a0, a1, a2, a3;
    int32x4_t reg0[4], reg1[4];
    int32x4_t a4, a5, a7;
    int16x8_t a8, a9, a6;
    uint32x2_t p0, p1;

    a6 = vdupq_n_s16((WORD16)rnd);

    a4 = vdupq_n_s32(w0);
    a5 = vdupq_n_s32(w1);
    a7 = vdupq_n_s32((-shift));

    for(i = 0; i < ht; i = i + 4)
    {
        for(j = 0; j < wd; j = j + 4)
        {
            src0_u8 = load_unaligned_u8q(pu1_pred0 + ((i * pred0_strd) + j), pred0_strd);
            src1_u8 = load_unaligned_u8q(pu1_pred1 + ((i * pred1_strd) + j), pred1_strd);

            a0 = vmovl_u8(vget_low_u8(src0_u8));
            a1 = vmovl_u8(vget_high_u8(src0_u8));
            a2 = vmovl_u8(vget_low_u8(src1_u8));
            a3 = vmovl_u8(vget_high_u8(src1_u8));

            reg0[0] = vmovl_s16(vreinterpret_s16_u16(vget_low_u16(a0)));
            reg0[1] = vmovl_s16(vreinterpret_s16_u16(vget_high_u16(a0)));
            reg0[2] = vmovl_s16(vreinterpret_s16_u16(vget_low_u16(a1)));
            reg0[3] = vmovl_s16(vreinterpret_s16_u16(vget_high_u16(a1)));

            reg1[0] = vmovl_s16(vreinterpret_s16_u16(vget_low_u16(a2)));
            reg1[1] = vmovl_s16(vreinterpret_s16_u16(vget_high_u16(a2)));
            reg1[2] = vmovl_s16(vreinterpret_s16_u16(vget_low_u16(a3)));
            reg1[3] = vmovl_s16(vreinterpret_s16_u16(vget_high_u16(a3)));

            reg0[0] = vmulq_s32(reg0[0], a4);
            reg0[1] = vmulq_s32(reg0[1], a4);
            reg0[2] = vmulq_s32(reg0[2], a4);
            reg0[3] = vmulq_s32(reg0[3], a4);

            reg1[0] = vmulq_s32(reg1[0], a5);
            reg1[1] = vmulq_s32(reg1[1], a5);
            reg1[2] = vmulq_s32(reg1[2], a5);
            reg1[3] = vmulq_s32(reg1[3], a5);

            reg0[0] = vaddq_s32(reg0[0], reg1[0]);
            reg0[1] = vaddq_s32(reg0[1], reg1[1]);
            reg0[2] = vaddq_s32(reg0[2], reg1[2]);
            reg0[3] = vaddq_s32(reg0[3], reg1[3]);

            reg0[0] = vshlq_s32(reg0[0], a7);
            reg0[1] = vshlq_s32(reg0[1], a7);
            reg0[2] = vshlq_s32(reg0[2], a7);
            reg0[3] = vshlq_s32(reg0[3], a7);

            a8 = vcombine_s16(vmovn_s32(reg0[0]), vmovn_s32(reg0[1]));
            a9 = vcombine_s16(vmovn_s32(reg0[2]), vmovn_s32(reg0[3]));

            a8 = vaddq_s16(a8, a6);
            a9 = vaddq_s16(a9, a6);

            p0 = vreinterpret_u32_u8(vqmovun_s16(a8));
            p1 = vreinterpret_u32_u8(vqmovun_s16(a9));

            *(UWORD32 *)pu1_dst = vget_lane_u32(p0, 0);
            *(UWORD32 *)(pu1_dst + dst_strd) = vget_lane_u32(p0, 1);
            *(UWORD32 *)(pu1_dst + 2 * dst_strd) = vget_lane_u32(p1, 0);
            *(UWORD32 *)(pu1_dst + 3 * dst_strd) = vget_lane_u32(p1, 1);

            pu1_dst += 4;
        }
        pu1_dst = pu1_dst - wd + 4 * dst_strd;
    }
}

/**
********************************************************************************
*
*  @brief  Weighted pred of 2 predictor buffers as per spec
*
*  @param[in] pu1_pred0 : Pred0 buffer
*
*  @param[in] pu1_pred1 : Pred1 buffer
*
*  @param[in] pred0_strd : Stride of pred0 buffer
*
*  @param[in] pred1_strd : Stride of pred1 buffer
*
*  @param[in] wd : Width of pred block
*
*  @param[in] ht : Height of pred block
*
*  @param[out] pu1_dst : Destination buffer that will hold result
*
*  @param[in] dst_strd : Stride of dest buffer
*
*  @param[in] w0 : Weighting factor of Pred0
*
*  @param[in] w1 : weighting factor of pred1
*
*  @param[in] o0 : offset for pred0
*
*  @param[in] o1 : offset for pred1
*
*  @param[in] log_wdc : shift factor as per spec
*
*  @return none
*
********************************************************************************
*/
void ihevce_wt_avg_2d_neon(
    UWORD8 *pu1_pred0,
    UWORD8 *pu1_pred1,
    WORD32 pred0_strd,
    WORD32 pred1_strd,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 w0,
    WORD32 w1,
    WORD32 o0,
    WORD32 o1,
    WORD32 log_wdc)
{
    /* Total Rounding term to be added, including offset */
    WORD32 rnd = (o0 + o1 + 1) >> 1;  // << log_wdc;
    /* Downshift */
    WORD32 shift = log_wdc + 1;
    /* loop counters */
    WORD32 i, j;

    switch(wd)
    {
    case 4:
    case 12:
        ihevce_wt_avg_2d_4xn_neon(
            pu1_pred0,
            pu1_pred1,
            pred0_strd,
            pred1_strd,
            wd,
            ht,
            pu1_dst,
            dst_strd,
            w0,
            w1,
            rnd,
            shift);
        break;
    case 8:
    case 24:
        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j = j + 8)
            {
                ihevce_wt_avg_2d_8x1_neon(
                    pu1_pred0 + ((i * pred0_strd) + j),
                    pu1_pred1 + ((i * pred1_strd) + j),
                    pu1_dst + ((i * dst_strd) + j),
                    w0,
                    w1,
                    rnd,
                    shift);
            }
        }
        break;
    case 16:
        for(i = 0; i < ht; i++)
            ihevce_wt_avg_2d_16x1_neon(
                pu1_pred0 + (i * pred0_strd),
                pu1_pred1 + (i * pred1_strd),
                pu1_dst + (i * dst_strd),
                w0,
                w1,
                rnd,
                shift);
        break;
    case 32:
    case 64:
        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j = j + 16)
            {
                ihevce_wt_avg_2d_16x1_neon(
                    pu1_pred0 + ((i * pred0_strd) + j),
                    pu1_pred1 + ((i * pred1_strd) + j),
                    pu1_dst + ((i * dst_strd) + j),
                    w0,
                    w1,
                    rnd,
                    shift);
            }
        }
        break;
    case 48:
        for(i = 0; i < ht; i++)
        {
            for(j = 0; j < wd; j = j + 16)
            {
                ihevce_wt_avg_2d_16x1_neon(
                    pu1_pred0 + ((i * pred0_strd) + j),
                    pu1_pred1 + ((i * pred1_strd) + j),
                    pu1_dst + ((i * dst_strd) + j),
                    w0,
                    w1,
                    rnd,
                    shift);
            }
        }
        break;
    default:
        assert(0);
        break;
    }
    return;
}

static INLINE WORD32 sad_cal(int16x8_t temp_reg)
{
    int64x2_t sad_reg = vpaddlq_s32(vpaddlq_s16(temp_reg));

    return (vget_lane_s32(
        vadd_s32(
            vreinterpret_s32_s64(vget_low_s64(sad_reg)),
            vreinterpret_s32_s64(vget_high_s64(sad_reg))),
        0));
}

void ihevce_get_luma_eo_sao_params_neon(
    void *pv_sao_ctxt,
    WORD32 eo_sao_class,
    WORD32 *pi4_acc_error_category,
    WORD32 *pi4_category_count)
{
    /*temp var*/
    UWORD8 *pu1_luma_recon_buf, *pu1_luma_src_buf;
    UWORD8 *pu1_luma_src_buf_copy, *pu1_luma_recon_buf_copy;
    WORD32 row_end, col_end, row, col;
    WORD32 row_start = 0, col_start = 0;
    WORD32 wd, rem_wd;
    WORD32 a, b, c, edge_idx, pel_err;

    int16x8_t temp_reg0, temp_reg1, temp_reg2, temp_reg3, temp_reg4;
    int16x8_t edgeidx_reg0, edgeidx_reg1, edgeidx_reg2, edgeidx_reg3, edgeidx_reg4;
    int16x8_t edgeidx_reg5, edgeidx_reg6, edgeidx_reg7;
    int16x8_t pel_error, pel_error1;
    int16x8_t sign_reg0, sign_reg1, sign_reg, sign_reg2, sign_reg3;
    int16x8_t edgeidx, edgeidx1;
    int16x8_t temp_reg5, temp_reg6, temp_reg7;
    uint8x16_t src_buf_8x16, recon_buf_8x16, recon_buf0_8x16, recon_buf1_8x16;
    uint8x8_t src_buf, recon_buf, recon_buf0, recon_buf1;

    sao_ctxt_t *ps_sao_ctxt = (sao_ctxt_t *)pv_sao_ctxt;
    const WORD32 i4_luma_recon_strd = ps_sao_ctxt->i4_cur_luma_recon_stride;
    const WORD32 i4_luma_src_strd = ps_sao_ctxt->i4_cur_luma_src_stride;

    const int16x8_t const_2 = vdupq_n_s16(2);
    const int16x8_t const_0 = vdupq_n_s16(0);
    const int16x8_t const_1 = vdupq_n_s16(1);
    const int16x8_t const_3 = vdupq_n_s16(3);
    const int16x8_t const_4 = vdupq_n_s16(4);

    row_end = ps_sao_ctxt->i4_sao_blk_ht;
    col_end = ps_sao_ctxt->i4_sao_blk_wd;

    if((ps_sao_ctxt->i4_ctb_x == 0) && (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_start = 1;
    }

    if(((ps_sao_ctxt->i4_ctb_x + 1) == ps_sao_ctxt->ps_sps->i2_pic_wd_in_ctb) &&
       (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_end = col_end - 1;
    }

    if((ps_sao_ctxt->i4_ctb_y == 0) && (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_start = 1;
    }

    if(((ps_sao_ctxt->i4_ctb_y + 1) == ps_sao_ctxt->ps_sps->i2_pic_ht_in_ctb) &&
       (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_end = row_end - 1;
    }
    wd = col_end - col_start;
    rem_wd = wd;
    pu1_luma_recon_buf =
        ps_sao_ctxt->pu1_cur_luma_recon_buf + col_start + (row_start * i4_luma_recon_strd);
    pu1_luma_src_buf =
        ps_sao_ctxt->pu1_cur_luma_src_buf + col_start + (row_start * i4_luma_src_strd);

    switch(eo_sao_class)
    {
    case SAO_EDGE_0_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_luma_src_buf_copy = pu1_luma_src_buf;
            pu1_luma_recon_buf_copy = pu1_luma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load src and recon data*/
                src_buf_8x16 = vld1q_u8(pu1_luma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_luma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_luma_recon_buf - 1);
                recon_buf1_8x16 = vld1q_u8(pu1_luma_recon_buf + 1);

                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));

                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);
                /*edgidx*/
                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);

                /*store peel error*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                /*store edgeidx account*/
                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_luma_recon_buf += 16;
                pu1_luma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load data*/
                src_buf = vld1_u8(pu1_luma_src_buf);
                recon_buf = vld1_u8(pu1_luma_recon_buf);
                recon_buf0 = vld1_u8(pu1_luma_recon_buf - 1);
                recon_buf1 = vld1_u8(pu1_luma_recon_buf + 1);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg3 = vabsq_s16(temp_reg3);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store */
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_luma_recon_buf += 8;
                pu1_luma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_luma_recon_buf[col];
                    a = pu1_luma_recon_buf[col - 1];
                    b = pu1_luma_recon_buf[col + 1];
                    pel_err = pu1_luma_src_buf[col] - pu1_luma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);

                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_luma_recon_buf = pu1_luma_recon_buf_copy + i4_luma_recon_strd;
            pu1_luma_src_buf = pu1_luma_src_buf_copy + i4_luma_src_strd;
            rem_wd = wd;
        }
        break;
    case SAO_EDGE_90_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_luma_src_buf_copy = pu1_luma_src_buf;
            pu1_luma_recon_buf_copy = pu1_luma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load src and recon data*/
                src_buf_8x16 = vld1q_u8(pu1_luma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_luma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_luma_recon_buf - i4_luma_recon_strd);
                recon_buf1_8x16 = vld1q_u8(pu1_luma_recon_buf + i4_luma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);
                /*edgeidx*/
                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);
                /* store */
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);
                /*store account*/
                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_luma_recon_buf += 16;
                pu1_luma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load*/
                src_buf = vld1_u8(pu1_luma_src_buf);
                recon_buf = vld1_u8(pu1_luma_recon_buf);
                recon_buf0 = vld1_u8(pu1_luma_recon_buf - i4_luma_recon_strd);
                recon_buf1 = vld1_u8(pu1_luma_recon_buf + i4_luma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg3 = vabsq_s16(temp_reg3);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_luma_recon_buf += 8;
                pu1_luma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_luma_recon_buf[col];
                    a = pu1_luma_recon_buf[col - i4_luma_recon_strd];
                    b = pu1_luma_recon_buf[col + i4_luma_recon_strd];
                    pel_err = pu1_luma_src_buf[col] - pu1_luma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);

                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_luma_recon_buf = pu1_luma_recon_buf_copy + i4_luma_recon_strd;
            pu1_luma_src_buf = pu1_luma_src_buf_copy + i4_luma_src_strd;
            rem_wd = wd;
        }
        break;
    case SAO_EDGE_135_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_luma_src_buf_copy = pu1_luma_src_buf;
            pu1_luma_recon_buf_copy = pu1_luma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load src and recon data*/
                src_buf_8x16 = vld1q_u8(pu1_luma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_luma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_luma_recon_buf - 1 - i4_luma_recon_strd);
                recon_buf1_8x16 = vld1q_u8(pu1_luma_recon_buf + 1 + i4_luma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_luma_recon_buf += 16;
                pu1_luma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load data*/
                src_buf = vld1_u8(pu1_luma_src_buf);
                recon_buf = vld1_u8(pu1_luma_recon_buf);
                recon_buf0 = vld1_u8(pu1_luma_recon_buf - 1 - i4_luma_recon_strd);
                recon_buf1 = vld1_u8(pu1_luma_recon_buf + 1 + i4_luma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg4 = vabsq_s16(temp_reg4);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg3);
                pi4_acc_error_category[4] += sad_cal(temp_reg4);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg3);
                pi4_category_count[4] += sad_cal(edgeidx_reg4);
                pu1_luma_recon_buf += 8;
                pu1_luma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_luma_recon_buf[col];
                    a = pu1_luma_recon_buf[col - 1 - i4_luma_recon_strd];
                    b = pu1_luma_recon_buf[col + 1 + i4_luma_recon_strd];
                    pel_err = pu1_luma_src_buf[col] - pu1_luma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);

                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_luma_recon_buf = pu1_luma_recon_buf_copy + i4_luma_recon_strd;
            pu1_luma_src_buf = pu1_luma_src_buf_copy + i4_luma_src_strd;
            rem_wd = wd;
        }
        break;
    case SAO_EDGE_45_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_luma_src_buf_copy = pu1_luma_src_buf;
            pu1_luma_recon_buf_copy = pu1_luma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load data*/
                src_buf_8x16 = vld1q_u8(pu1_luma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_luma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_luma_recon_buf + 1 - i4_luma_recon_strd);
                recon_buf1_8x16 = vld1q_u8(pu1_luma_recon_buf - 1 + i4_luma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_luma_recon_buf += 16;
                pu1_luma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load*/
                src_buf = vld1_u8(pu1_luma_src_buf);
                recon_buf = vld1_u8(pu1_luma_recon_buf);
                recon_buf0 = vld1_u8(pu1_luma_recon_buf + 1 - i4_luma_recon_strd);
                recon_buf1 = vld1_u8(pu1_luma_recon_buf - 1 + i4_luma_recon_strd);

                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));

                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg4 = vabsq_s16(temp_reg4);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg3);
                pi4_acc_error_category[4] += sad_cal(temp_reg4);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg3);
                pi4_category_count[4] += sad_cal(edgeidx_reg4);
                pu1_luma_recon_buf += 8;
                pu1_luma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_luma_recon_buf[col];
                    a = pu1_luma_recon_buf[col + 1 - i4_luma_recon_strd];
                    b = pu1_luma_recon_buf[col - 1 + i4_luma_recon_strd];
                    pel_err = pu1_luma_src_buf[col] - pu1_luma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);
                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_luma_recon_buf = pu1_luma_recon_buf_copy + i4_luma_recon_strd;
            pu1_luma_src_buf = pu1_luma_src_buf_copy + i4_luma_src_strd;
            rem_wd = wd;
        }
        break;
    default:
        break;
    }
}

void ihevce_get_chroma_eo_sao_params_neon(
    void *pv_sao_ctxt,
    WORD32 eo_sao_class,
    WORD32 *pi4_acc_error_category,
    WORD32 *pi4_category_count)
{
    /*temp var*/
    UWORD8 *pu1_chroma_recon_buf, *pu1_chroma_src_buf;
    UWORD8 *pu1_chroma_src_buf_copy, *pu1_chroma_recon_buf_copy;
    WORD32 row_end, col_end, row, col;
    WORD32 row_start = 0, col_start = 0;
    WORD32 wd, rem_wd;
    WORD32 a, b, c, edge_idx, pel_err;

    int16x8_t temp_reg0, temp_reg1, temp_reg2, temp_reg3, temp_reg4;
    int16x8_t edgeidx_reg0, edgeidx_reg1, edgeidx_reg2, edgeidx_reg3, edgeidx_reg4;
    int16x8_t edgeidx_reg5, edgeidx_reg6, edgeidx_reg7;
    int16x8_t pel_error, pel_error1;
    int16x8_t sign_reg0, sign_reg1, sign_reg, sign_reg2, sign_reg3;
    int16x8_t edgeidx, edgeidx1;
    int16x8_t temp_reg5, temp_reg6, temp_reg7;
    uint8x16_t src_buf_8x16, recon_buf_8x16, recon_buf0_8x16, recon_buf1_8x16;
    uint8x8_t src_buf, recon_buf, recon_buf0, recon_buf1;

    sao_ctxt_t *ps_sao_ctxt = (sao_ctxt_t *)pv_sao_ctxt;
    const WORD32 i4_chroma_recon_strd = ps_sao_ctxt->i4_cur_chroma_recon_stride;
    const WORD32 i4_chroma_src_strd = ps_sao_ctxt->i4_cur_chroma_src_stride;

    const int16x8_t const_2 = vdupq_n_s16(2);
    const int16x8_t const_0 = vdupq_n_s16(0);
    const int16x8_t const_1 = vdupq_n_s16(1);
    const int16x8_t const_3 = vdupq_n_s16(3);
    const int16x8_t const_4 = vdupq_n_s16(4);

    row_end = ps_sao_ctxt->i4_sao_blk_ht >> 1;
    col_end = ps_sao_ctxt->i4_sao_blk_wd;

    if((ps_sao_ctxt->i4_ctb_x == 0) && (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_start = 2;
    }

    if(((ps_sao_ctxt->i4_ctb_x + 1) == ps_sao_ctxt->ps_sps->i2_pic_wd_in_ctb) &&
       (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_end = col_end - 2;
    }

    if((ps_sao_ctxt->i4_ctb_y == 0) && (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_start = 1;
    }

    if(((ps_sao_ctxt->i4_ctb_y + 1) == ps_sao_ctxt->ps_sps->i2_pic_ht_in_ctb) &&
       (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_end = row_end - 1;
    }
    wd = col_end - col_start;
    rem_wd = wd;
    pu1_chroma_recon_buf =
        ps_sao_ctxt->pu1_cur_chroma_recon_buf + col_start + (row_start * i4_chroma_recon_strd);
    pu1_chroma_src_buf =
        ps_sao_ctxt->pu1_cur_chroma_src_buf + col_start + (row_start * i4_chroma_src_strd);

    switch(eo_sao_class)
    {
    case SAO_EDGE_0_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_chroma_src_buf_copy = pu1_chroma_src_buf;
            pu1_chroma_recon_buf_copy = pu1_chroma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load src and recon data*/
                src_buf_8x16 = vld1q_u8(pu1_chroma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_chroma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_chroma_recon_buf - 2);
                recon_buf1_8x16 = vld1q_u8(pu1_chroma_recon_buf + 2);

                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));

                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);
                /*edgidx*/
                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);

                /*store peel error*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                /*store edgeidx account*/
                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_chroma_recon_buf += 16;
                pu1_chroma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load data*/
                src_buf = vld1_u8(pu1_chroma_src_buf);
                recon_buf = vld1_u8(pu1_chroma_recon_buf);
                recon_buf0 = vld1_u8(pu1_chroma_recon_buf - 2);
                recon_buf1 = vld1_u8(pu1_chroma_recon_buf + 2);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg3 = vabsq_s16(temp_reg3);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store */
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_chroma_recon_buf += 8;
                pu1_chroma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_chroma_recon_buf[col];
                    a = pu1_chroma_recon_buf[col - 2];
                    b = pu1_chroma_recon_buf[col + 2];
                    pel_err = pu1_chroma_src_buf[col] - pu1_chroma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);

                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_chroma_recon_buf = pu1_chroma_recon_buf_copy + i4_chroma_recon_strd;
            pu1_chroma_src_buf = pu1_chroma_src_buf_copy + i4_chroma_src_strd;
            rem_wd = wd;
        }
        break;
    case SAO_EDGE_90_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_chroma_src_buf_copy = pu1_chroma_src_buf;
            pu1_chroma_recon_buf_copy = pu1_chroma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load src and recon data*/
                src_buf_8x16 = vld1q_u8(pu1_chroma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_chroma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_chroma_recon_buf - i4_chroma_recon_strd);
                recon_buf1_8x16 = vld1q_u8(pu1_chroma_recon_buf + i4_chroma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);
                /*edgeidx*/
                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);
                /* store */
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);
                /*store account*/
                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_chroma_recon_buf += 16;
                pu1_chroma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load*/
                src_buf = vld1_u8(pu1_chroma_src_buf);
                recon_buf = vld1_u8(pu1_chroma_recon_buf);
                recon_buf0 = vld1_u8(pu1_chroma_recon_buf - i4_chroma_recon_strd);
                recon_buf1 = vld1_u8(pu1_chroma_recon_buf + i4_chroma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg3 = vabsq_s16(temp_reg3);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_chroma_recon_buf += 8;
                pu1_chroma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_chroma_recon_buf[col];
                    a = pu1_chroma_recon_buf[col - i4_chroma_recon_strd];
                    b = pu1_chroma_recon_buf[col + i4_chroma_recon_strd];
                    pel_err = pu1_chroma_src_buf[col] - pu1_chroma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);

                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_chroma_recon_buf = pu1_chroma_recon_buf_copy + i4_chroma_recon_strd;
            pu1_chroma_src_buf = pu1_chroma_src_buf_copy + i4_chroma_src_strd;
            rem_wd = wd;
        }
        break;
    case SAO_EDGE_135_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_chroma_src_buf_copy = pu1_chroma_src_buf;
            pu1_chroma_recon_buf_copy = pu1_chroma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load src and recon data*/
                src_buf_8x16 = vld1q_u8(pu1_chroma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_chroma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_chroma_recon_buf - 2 - i4_chroma_recon_strd);
                recon_buf1_8x16 = vld1q_u8(pu1_chroma_recon_buf + 2 + i4_chroma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_chroma_recon_buf += 16;
                pu1_chroma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load data*/
                src_buf = vld1_u8(pu1_chroma_src_buf);
                recon_buf = vld1_u8(pu1_chroma_recon_buf);
                recon_buf0 = vld1_u8(pu1_chroma_recon_buf - 2 - i4_chroma_recon_strd);
                recon_buf1 = vld1_u8(pu1_chroma_recon_buf + 2 + i4_chroma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg4 = vabsq_s16(temp_reg4);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg3);
                pi4_acc_error_category[4] += sad_cal(temp_reg4);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg3);
                pi4_category_count[4] += sad_cal(edgeidx_reg4);
                pu1_chroma_recon_buf += 8;
                pu1_chroma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_chroma_recon_buf[col];
                    a = pu1_chroma_recon_buf[col - 2 - i4_chroma_recon_strd];
                    b = pu1_chroma_recon_buf[col + 2 + i4_chroma_recon_strd];
                    pel_err = pu1_chroma_src_buf[col] - pu1_chroma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);

                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_chroma_recon_buf = pu1_chroma_recon_buf_copy + i4_chroma_recon_strd;
            pu1_chroma_src_buf = pu1_chroma_src_buf_copy + i4_chroma_src_strd;
            rem_wd = wd;
        }
        break;
    case SAO_EDGE_45_DEG:
        for(row = row_start; row < row_end; row++)
        {
            pu1_chroma_src_buf_copy = pu1_chroma_src_buf;
            pu1_chroma_recon_buf_copy = pu1_chroma_recon_buf;
            for(col = wd; col > 15; col -= 16)
            {
                /*load data*/
                src_buf_8x16 = vld1q_u8(pu1_chroma_src_buf);
                recon_buf_8x16 = vld1q_u8(pu1_chroma_recon_buf);
                recon_buf0_8x16 = vld1q_u8(pu1_chroma_recon_buf + 2 - i4_chroma_recon_strd);
                recon_buf1_8x16 = vld1q_u8(pu1_chroma_recon_buf - 2 + i4_chroma_recon_strd);
                /*pel_error*/
                pel_error = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(src_buf_8x16), vget_low_u8(recon_buf_8x16)));
                pel_error1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(src_buf_8x16), vget_high_u8(recon_buf_8x16)));
                /*sign*/
                sign_reg0 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_low_u8(recon_buf_8x16), vget_low_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                sign_reg2 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf0_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg2, const_0);
                sign_reg2 = (int16x8_t)vcltq_s16(sign_reg2, const_0);
                sign_reg2 = vsubq_s16(sign_reg2, sign_reg);

                sign_reg3 = vreinterpretq_s16_u16(
                    vsubl_u8(vget_high_u8(recon_buf_8x16), vget_high_u8(recon_buf1_8x16)));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg3, const_0);
                sign_reg3 = (int16x8_t)vcltq_s16(sign_reg3, const_0);
                sign_reg3 = vsubq_s16(sign_reg3, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);
                edgeidx1 = vaddq_s16(vaddq_s16(sign_reg2, const_2), sign_reg3);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                edgeidx_reg5 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error1));
                edgeidx1 = vandq_s16(edgeidx_reg5, edgeidx1);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_0, edgeidx1);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg5 = (int16x8_t)vceqq_s16(const_1, edgeidx1);

                temp_reg2 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg6 = (int16x8_t)vceqq_s16(const_3, edgeidx1);
                temp_reg3 = (int16x8_t)vceqq_s16(const_4, edgeidx);
                temp_reg7 = (int16x8_t)vceqq_s16(const_4, edgeidx1);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg5 = vabsq_s16(temp_reg5);

                edgeidx_reg2 = vabsq_s16(temp_reg2);
                edgeidx_reg6 = vabsq_s16(temp_reg6);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg7 = vabsq_s16(temp_reg7);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error1);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg5 = vandq_s16(temp_reg5, pel_error1);

                temp_reg2 = vandq_s16(temp_reg2, pel_error);
                temp_reg6 = vandq_s16(temp_reg6, pel_error1);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg7 = vandq_s16(temp_reg7, pel_error1);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                edgeidx_reg4 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg4));

                temp_reg0 = vaddq_s16(temp_reg0, temp_reg4);
                temp_reg1 = vaddq_s16(temp_reg1, temp_reg5);
                temp_reg2 = vaddq_s16(temp_reg2, temp_reg6);
                temp_reg3 = vaddq_s16(temp_reg3, temp_reg7);

                edgeidx_reg0 = vaddq_s16(edgeidx_reg0, edgeidx_reg4);
                edgeidx_reg1 = vaddq_s16(edgeidx_reg1, edgeidx_reg5);
                edgeidx_reg2 = vaddq_s16(edgeidx_reg2, edgeidx_reg6);
                edgeidx_reg3 = vaddq_s16(edgeidx_reg3, edgeidx_reg7);
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg2);
                pi4_acc_error_category[4] += sad_cal(temp_reg3);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg2);
                pi4_category_count[4] += sad_cal(edgeidx_reg3);
                pu1_chroma_recon_buf += 16;
                pu1_chroma_src_buf += 16;
            }
            rem_wd &= 0x0F;

            if(rem_wd > 7)
            {
                /*load*/
                src_buf = vld1_u8(pu1_chroma_src_buf);
                recon_buf = vld1_u8(pu1_chroma_recon_buf);
                recon_buf0 = vld1_u8(pu1_chroma_recon_buf + 2 - i4_chroma_recon_strd);
                recon_buf1 = vld1_u8(pu1_chroma_recon_buf - 2 + i4_chroma_recon_strd);

                pel_error = vreinterpretq_s16_u16(vsubl_u8(src_buf, recon_buf));

                sign_reg0 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf0));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg0, const_0);
                sign_reg0 = (int16x8_t)vcltq_s16(sign_reg0, const_0);
                sign_reg0 = vsubq_s16(sign_reg0, sign_reg);

                sign_reg1 = vreinterpretq_s16_u16(vsubl_u8(recon_buf, recon_buf1));
                sign_reg = (int16x8_t)vcgtq_s16(sign_reg1, const_0);
                sign_reg1 = (int16x8_t)vcltq_s16(sign_reg1, const_0);
                sign_reg1 = vsubq_s16(sign_reg1, sign_reg);

                edgeidx = vaddq_s16(vaddq_s16(sign_reg0, const_2), sign_reg1);

                edgeidx_reg0 = vmvnq_s16((int16x8_t)vceqq_s16(const_0, pel_error));
                edgeidx = vandq_s16(edgeidx_reg0, edgeidx);

                temp_reg0 = (int16x8_t)vceqq_s16(const_0, edgeidx);
                temp_reg1 = (int16x8_t)vceqq_s16(const_1, edgeidx);
                temp_reg3 = (int16x8_t)vceqq_s16(const_3, edgeidx);
                temp_reg4 = (int16x8_t)vceqq_s16(const_4, edgeidx);

                edgeidx_reg1 = vabsq_s16(temp_reg1);
                edgeidx_reg3 = vabsq_s16(temp_reg3);
                edgeidx_reg4 = vabsq_s16(temp_reg4);

                temp_reg0 = vandq_s16(temp_reg0, pel_error);
                temp_reg1 = vandq_s16(temp_reg1, pel_error);
                temp_reg3 = vandq_s16(temp_reg3, pel_error);
                temp_reg4 = vandq_s16(temp_reg4, pel_error);

                edgeidx_reg0 = vaddq_s16(const_1, (int16x8_t)vceqq_s16(const_0, temp_reg0));
                /*store*/
                pi4_acc_error_category[0] += sad_cal(temp_reg0);
                pi4_acc_error_category[1] += sad_cal(temp_reg1);
                pi4_acc_error_category[3] += sad_cal(temp_reg3);
                pi4_acc_error_category[4] += sad_cal(temp_reg4);

                pi4_category_count[0] += sad_cal(edgeidx_reg0);
                pi4_category_count[1] += sad_cal(edgeidx_reg1);
                pi4_category_count[3] += sad_cal(edgeidx_reg3);
                pi4_category_count[4] += sad_cal(edgeidx_reg4);
                pu1_chroma_recon_buf += 8;
                pu1_chroma_src_buf += 8;
            }
            rem_wd &= 0x7;
            if(rem_wd)
            {
                for(col = 0; col < rem_wd; col++)
                {
                    c = pu1_chroma_recon_buf[col];
                    a = pu1_chroma_recon_buf[col + 2 - i4_chroma_recon_strd];
                    b = pu1_chroma_recon_buf[col - 2 + i4_chroma_recon_strd];
                    pel_err = pu1_chroma_src_buf[col] - pu1_chroma_recon_buf[col];
                    edge_idx = 2 + SIGN(c - a) + SIGN(c - b);
                    if(pel_err != 0)
                    {
                        pi4_acc_error_category[edge_idx] += pel_err;
                        pi4_category_count[edge_idx]++;
                    }
                }
            }
            pu1_chroma_recon_buf = pu1_chroma_recon_buf_copy + i4_chroma_recon_strd;
            pu1_chroma_src_buf = pu1_chroma_src_buf_copy + i4_chroma_src_strd;
            rem_wd = wd;
        }
        break;
    default:
        break;
    }
}
