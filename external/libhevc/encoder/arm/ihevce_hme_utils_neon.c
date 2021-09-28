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
*  ihevce_hme_utils_neon.c
*
* @brief
*  Contains function definitions for hme utils function in neon intrinsic
*
*
* @author
* ittian
*
* @par List of Functions:
*   - ihevce_get_wt_inp_8x8_neon()
*   - ihevce_get_wt_inp_ctb_neon()
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
#include <assert.h>
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_cmn_utils_neon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_debug.h"
#include "ihevc_deblk.h"
#include "ihevc_defs.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_macros.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_resi_trans.h"
#include "ihevc_sao.h"
#include "ihevc_structs.h"
#include "ihevc_weighted_pred.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevce_api.h"
#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_function_selector.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_enc_structs.h"
#include "ihevce_had_satd.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_global_tables.h"

#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_globals.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_refine.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IHEVCE_WT_PRED_SHIFT 15

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static INLINE void ihevce_get_wt_inp_4x8_neon(
    const UWORD8 *pu1_src,
    UWORD8 *pu1_dst,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    WORD32 u1_num_valid_refs,
    WORD32 *ai4_wt_refs,
    WORD32 src_stride,
    WORD32 dst_stride)
{
    S32 inv_wt;
    S16 off;
    uint8x8_t src0_8x8b, src1_8x8b, src2_8x8b, src3_8x8b;
    int16x8_t src0_8x16b, src1_8x16b, src2_8x16b, src3_8x16b;
    int16x8_t src4_8x16b, src5_8x16b, src6_8x16b, src7_8x16b, off_8x16b;
    int32x4_t dst0_4x32b, dst1_4x32b, dst2_4x32b, dst3_4x32b;
    int32x4_t dst4_4x32b, dst5_4x32b, dst6_4x32b, dst7_4x32b;
    int32x4_t add_4x32b, inv_wt_4x32b;
    U08 ref;
    int32x4_t log_wdc = vdupq_n_s32(ps_wt_inp_prms->wpred_log_wdc);

    src0_8x8b = vld1_u8((pu1_src + 0 * src_stride));
    src1_8x8b = vld1_u8((pu1_src + 1 * src_stride));
    src2_8x8b = vld1_u8((pu1_src + 2 * src_stride));
    src3_8x8b = vld1_u8((pu1_src + 3 * src_stride));
    /* Store */
    vst1_u8((pu1_dst + 0 * dst_stride), src0_8x8b);
    vst1_u8((pu1_dst + 1 * dst_stride), src1_8x8b);
    vst1_u8((pu1_dst + 2 * dst_stride), src2_8x8b);
    vst1_u8((pu1_dst + 3 * dst_stride), src3_8x8b);

    if(u1_num_valid_refs)
    {
        /* Right 4x4 Block */
        src0_8x16b = vreinterpretq_s16_u16(vmovl_u8(src0_8x8b));
        src1_8x16b = vreinterpretq_s16_u16(vmovl_u8(src1_8x8b));
        src2_8x16b = vreinterpretq_s16_u16(vmovl_u8(src2_8x8b));
        src3_8x16b = vreinterpretq_s16_u16(vmovl_u8(src3_8x8b));

        /* add value */
        add_4x32b = vdupq_n_s32(0x4000);
    }

    /* Run thro all ref ids, except ref==num_ref, which is already done */
    for(ref = 0; ref < u1_num_valid_refs; ref++)
    {
        S32 i4_ref_idx = ai4_wt_refs[ref];

        /* InvWt and off specific to this ref id */
        inv_wt = ps_wt_inp_prms->a_inv_wpred_wt[i4_ref_idx];
        off = (S16)ps_wt_inp_prms->a_wpred_off[i4_ref_idx];

        /* set1 uses multiple instructions : Try to AVOID it */
        off_8x16b = vdupq_n_s16(off);
        inv_wt_4x32b = vdupq_n_s32(inv_wt);

        /* Each ref id may have differnet wt/offset. */
        /* So we have unique inp buf for each ref id */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[i4_ref_idx];

        /* inp - off */
        src4_8x16b = vsubq_s16(src0_8x16b, off_8x16b);
        src5_8x16b = vsubq_s16(src1_8x16b, off_8x16b);
        src6_8x16b = vsubq_s16(src2_8x16b, off_8x16b);
        src7_8x16b = vsubq_s16(src3_8x16b, off_8x16b);

        dst0_4x32b = vmovl_s16(vget_low_s16(src4_8x16b));
        dst1_4x32b = vmovl_s16(vget_low_s16(src5_8x16b));
        dst2_4x32b = vmovl_s16(vget_low_s16(src6_8x16b));
        dst3_4x32b = vmovl_s16(vget_low_s16(src7_8x16b));

        dst4_4x32b = vmovl_s16(vget_high_s16(src4_8x16b));
        dst5_4x32b = vmovl_s16(vget_high_s16(src5_8x16b));
        dst6_4x32b = vmovl_s16(vget_high_s16(src6_8x16b));
        dst7_4x32b = vmovl_s16(vget_high_s16(src7_8x16b));

        /* (inp-off) << shift */
        dst0_4x32b = vshlq_s32(dst0_4x32b, log_wdc);
        dst1_4x32b = vshlq_s32(dst1_4x32b, log_wdc);
        dst2_4x32b = vshlq_s32(dst2_4x32b, log_wdc);
        dst3_4x32b = vshlq_s32(dst3_4x32b, log_wdc);

        /* (inp-off) << shift */
        dst4_4x32b = vshlq_s32(dst4_4x32b, log_wdc);
        dst5_4x32b = vshlq_s32(dst5_4x32b, log_wdc);
        dst6_4x32b = vshlq_s32(dst6_4x32b, log_wdc);
        dst7_4x32b = vshlq_s32(dst7_4x32b, log_wdc);

        /* ((inp-off) << shift) * inv_wt + 1<<14 */
        dst0_4x32b = vmlaq_s32(add_4x32b, dst0_4x32b, inv_wt_4x32b);
        dst1_4x32b = vmlaq_s32(add_4x32b, dst1_4x32b, inv_wt_4x32b);
        dst2_4x32b = vmlaq_s32(add_4x32b, dst2_4x32b, inv_wt_4x32b);
        dst3_4x32b = vmlaq_s32(add_4x32b, dst3_4x32b, inv_wt_4x32b);

        /* ((inp-off) << shift) * inv_wt + 1<<14 */
        dst4_4x32b = vmlaq_s32(add_4x32b, dst4_4x32b, inv_wt_4x32b);
        dst5_4x32b = vmlaq_s32(add_4x32b, dst5_4x32b, inv_wt_4x32b);
        dst6_4x32b = vmlaq_s32(add_4x32b, dst6_4x32b, inv_wt_4x32b);
        dst7_4x32b = vmlaq_s32(add_4x32b, dst7_4x32b, inv_wt_4x32b);

        /* (((inp-off) << shift) * inv_wt + 1<<14) >> 15 */
        src4_8x16b = vcombine_s16(
            vshrn_n_s32(dst0_4x32b, IHEVCE_WT_PRED_SHIFT),
            vshrn_n_s32(dst4_4x32b, IHEVCE_WT_PRED_SHIFT));
        src5_8x16b = vcombine_s16(
            vshrn_n_s32(dst1_4x32b, IHEVCE_WT_PRED_SHIFT),
            vshrn_n_s32(dst5_4x32b, IHEVCE_WT_PRED_SHIFT));
        src6_8x16b = vcombine_s16(
            vshrn_n_s32(dst2_4x32b, IHEVCE_WT_PRED_SHIFT),
            vshrn_n_s32(dst6_4x32b, IHEVCE_WT_PRED_SHIFT));
        src7_8x16b = vcombine_s16(
            vshrn_n_s32(dst3_4x32b, IHEVCE_WT_PRED_SHIFT),
            vshrn_n_s32(dst7_4x32b, IHEVCE_WT_PRED_SHIFT));
        /* Store */
        vst1_u8((pu1_dst + 0 * dst_stride), vqmovun_s16(src4_8x16b));
        vst1_u8((pu1_dst + 1 * dst_stride), vqmovun_s16(src5_8x16b));
        vst1_u8((pu1_dst + 2 * dst_stride), vqmovun_s16(src6_8x16b));
        vst1_u8((pu1_dst + 3 * dst_stride), vqmovun_s16(src7_8x16b));
    }
}

void hme_get_wt_inp_8x8_neon(
    layer_ctxt_t *ps_curr_layer,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 dst_stride,
    S32 pos_x,
    S32 pos_y,
    S32 size,
    S32 num_ref,
    U08 u1_is_wt_pred_on)
{
    WORD32 ref;
    UWORD8 *pu1_src, *pu1_dst;
    WORD32 x_count, y_count;
    WORD32 src_stride = ps_curr_layer->i4_inp_stride;

    /* Make sure the start positions of block are inside frame limits */
    pos_x = MIN(pos_x, ps_curr_layer->i4_wd - 1);
    pos_y = MIN(pos_y, ps_curr_layer->i4_ht - 1);

    /* In case we handle imcomplete CTBs, we copy only as much as reqd */
    /* from input buffers to prevent out of bound accesses. In this    */
    /* case, we do padding in x or y or both dirns */
    x_count = MIN(size, (ps_curr_layer->i4_wd - pos_x));
    y_count = MIN(size, (ps_curr_layer->i4_ht - pos_y));

    /* Fixed source */
    pu1_src = ps_curr_layer->pu1_inp;
    pu1_src += (pos_x + (pos_y * src_stride));

    if(!u1_is_wt_pred_on)
    {
        uint8x8_t src0_8x8b, src1_8x8b, src2_8x8b, src3_8x8b;

        /*************         Top 4x8 Processing        ****************/
        /* Load Source : Lower 64 bit */
        src0_8x8b = vld1_u8(pu1_src + 0 * src_stride);
        src1_8x8b = vld1_u8(pu1_src + 1 * src_stride);
        src2_8x8b = vld1_u8(pu1_src + 2 * src_stride);
        src3_8x8b = vld1_u8(pu1_src + 3 * src_stride);

        /* ref==num_ref */ /* last ref will be non weighted input */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];
        /* Store */
        vst1_u8((pu1_dst + 0 * dst_stride), src0_8x8b);
        vst1_u8((pu1_dst + 1 * dst_stride), src1_8x8b);
        vst1_u8((pu1_dst + 2 * dst_stride), src2_8x8b);
        vst1_u8((pu1_dst + 3 * dst_stride), src3_8x8b);

        /*************       Bottom 4x8 Processing        ****************/
        pu1_src += 4 * src_stride;
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref] + 4 * dst_stride;

        /* Load Source : Lower 64 bit */
        src0_8x8b = vld1_u8(pu1_src + 0 * src_stride);
        src1_8x8b = vld1_u8(pu1_src + 1 * src_stride);
        src2_8x8b = vld1_u8(pu1_src + 2 * src_stride);
        src3_8x8b = vld1_u8(pu1_src + 3 * src_stride);
        /* ref==num_ref */ /* last ref will be non weighted input */
        /* Store */
        vst1_u8((pu1_dst + 0 * dst_stride), src0_8x8b);
        vst1_u8((pu1_dst + 1 * dst_stride), src1_8x8b);
        vst1_u8((pu1_dst + 2 * dst_stride), src2_8x8b);
        vst1_u8((pu1_dst + 3 * dst_stride), src3_8x8b);

        pu1_dst = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];

        if(x_count != size)
        {
            hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
        }

        /* Check and do padding in bottom directino if need be */
        if(y_count != size)
        {
            hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
        }

        for(ref = 0; ref < num_ref + 1; ref++)
        {
            ps_wt_inp_prms->apu1_wt_inp[ref] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];
        }
    }
    else
    {
        S32 wt, off;
        S32 ai4_wt_refs[MAX_NUM_REF];
        U08 u1_num_valid_refs = 0;

        for(ref = 0; ref < num_ref; ref++)
        {
            wt = ps_wt_inp_prms->a_wpred_wt[ref];
            off = ps_wt_inp_prms->a_wpred_off[ref];

            if((WGHT_DEFAULT == wt) && (0 == off))
            {
                ps_wt_inp_prms->apu1_wt_inp[ref] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];
            }
            else
            {
                ai4_wt_refs[u1_num_valid_refs++] = ref;
                ps_wt_inp_prms->apu1_wt_inp[ref] = ps_wt_inp_prms->apu1_wt_inp_buf_array[ref];
            }
        }

        ps_wt_inp_prms->apu1_wt_inp[num_ref] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];

        /*************         Top 4x8 Processing        ****************/
        /* ref==num_ref */ /* last ref will be non weighted input */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[num_ref];
        ihevce_get_wt_inp_4x8_neon(
            pu1_src,
            pu1_dst,
            ps_wt_inp_prms,
            u1_num_valid_refs,
            ai4_wt_refs,
            src_stride,
            dst_stride);
        /*************       Bottom 4x8 Processing        ****************/
        pu1_src += 4 * src_stride;
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[num_ref] + 4 * dst_stride;
        ihevce_get_wt_inp_4x8_neon(
            pu1_src,
            pu1_dst,
            ps_wt_inp_prms,
            u1_num_valid_refs,
            ai4_wt_refs,
            src_stride,
            dst_stride);

        for(ref = 0; ref < u1_num_valid_refs; ref++)
        {
            /* Check and do padding in right direction if need be */
            pu1_dst = ps_wt_inp_prms->apu1_wt_inp[ai4_wt_refs[ref]];
            if(x_count != size)
            {
                hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
            }

            /* Check and do padding in bottom directino if need be */
            if(y_count != size)
            {
                hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
            }
        }

        /* Check and do padding in right direction if need be */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[num_ref];
        if(x_count != size)
        {
            hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
        }

        /* Check and do padding in bottom directino if need be */
        if(y_count != size)
        {
            hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
        }
    }
}

void hme_get_wt_inp_ctb_neon(
    layer_ctxt_t *ps_curr_layer,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S32 dst_stride,
    S32 pos_x,
    S32 pos_y,
    S32 size,
    S32 num_ref,
    U08 u1_is_wt_pred_on)
{
    WORD32 ref, i, j;
    UWORD8 *pu1_src, *pu1_dst;
    WORD32 x_count, y_count;
    WORD32 src_stride = ps_curr_layer->i4_inp_stride;

    /* In case we handle imcomplete CTBs, we copy only as much as reqd */
    /* from input buffers to prevent out of bound accesses. In this    */
    /* case, we do padding in x or y or both dirns */
    x_count = MIN(size, (ps_curr_layer->i4_wd - pos_x));
    y_count = MIN(size, (ps_curr_layer->i4_ht - pos_y));

    /* Fixed source */
    pu1_src = ps_curr_layer->pu1_inp;
    pu1_src += (pos_x + (pos_y * src_stride));

    if(!u1_is_wt_pred_on)
    {
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];

        if(0 == (x_count & 15))
        {
            uint8x16_t src0_16x8b, src1_16x8b, src2_16x8b, src3_16x8b;

            for(i = 0; i < y_count; i += 4) /* 4 rows at a time */
            {
                for(j = 0; j < x_count; j += 16) /* 16 cols at a time */
                {
                    /* Load 4x16 Source */
                    src0_16x8b = vld1q_u8(pu1_src + 0 * src_stride);
                    src1_16x8b = vld1q_u8(pu1_src + 1 * src_stride);
                    src2_16x8b = vld1q_u8(pu1_src + 2 * src_stride);
                    src3_16x8b = vld1q_u8(pu1_src + 3 * src_stride);

                    /* ref==num_ref */ /* last ref will be non weighted input */
                    /* Store */
                    vst1q_u8((pu1_dst + 0 * dst_stride), src0_16x8b);
                    vst1q_u8((pu1_dst + 1 * dst_stride), src1_16x8b);
                    vst1q_u8((pu1_dst + 2 * dst_stride), src2_16x8b);
                    vst1q_u8((pu1_dst + 3 * dst_stride), src3_16x8b);

                    pu1_src += 16;
                    pu1_dst += 16;
                }

                pu1_src = pu1_src - x_count + 4 * src_stride;
                pu1_dst = pu1_dst - x_count + 4 * dst_stride;
            }
        }
        else if(0 == (x_count & 7)) /* wd multiple of 8 case */
        {
            uint8x8_t src0_8x8b, src1_8x8b, src2_8x8b, src3_8x8b;
            for(i = 0; i < y_count; i += 4) /* 4 rows at a time */
            {
                for(j = 0; j < x_count; j += 8) /* 8 cols at a time */
                {
                    /* Load 4x8 Source */
                    src0_8x8b = vld1_u8(pu1_src + 0 * src_stride);
                    src1_8x8b = vld1_u8(pu1_src + 1 * src_stride);
                    src2_8x8b = vld1_u8(pu1_src + 2 * src_stride);
                    src3_8x8b = vld1_u8(pu1_src + 3 * src_stride);

                    /* ref==num_ref */ /* last ref will be non weighted input */
                    /* Store */
                    vst1_u8((pu1_dst + 0 * dst_stride), src0_8x8b);
                    vst1_u8((pu1_dst + 1 * dst_stride), src1_8x8b);
                    vst1_u8((pu1_dst + 2 * dst_stride), src2_8x8b);
                    vst1_u8((pu1_dst + 3 * dst_stride), src3_8x8b);

                    pu1_src += 8;
                    pu1_dst += 8;
                }

                pu1_src = pu1_src - x_count + 4 * src_stride;
                pu1_dst = pu1_dst - x_count + 4 * dst_stride;
            }
        }
        else /* wd multiple of 4 case */
        {
            for(i = 0; i < y_count; i += 4) /* 4 rows at a time */
            {
                for(j = 0; j < x_count; j += 4) /* 4 cols at a time */
                {
                    /* ref==num_ref */ /* last ref will be non weighted input */
                    *(WORD32 *)(&pu1_dst[0 * dst_stride]) = *(WORD32 *)(&pu1_src[0 * src_stride]);
                    *(WORD32 *)(&pu1_dst[1 * dst_stride]) = *(WORD32 *)(&pu1_src[1 * src_stride]);
                    *(WORD32 *)(&pu1_dst[2 * dst_stride]) = *(WORD32 *)(&pu1_src[2 * src_stride]);
                    *(WORD32 *)(&pu1_dst[3 * dst_stride]) = *(WORD32 *)(&pu1_src[3 * src_stride]);

                    pu1_src += 4;
                    pu1_dst += 4;
                }

                pu1_src -= x_count + 4 * src_stride;
                pu1_dst = pu1_dst - x_count + 4 * dst_stride;
            }
        }

        for(i = 0; i < num_ref + 1; i++)
        {
            ps_wt_inp_prms->apu1_wt_inp[i] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];
        }

        /* Padding */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[num_ref];

        if(x_count != size)
        {
            hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
        }

        /* Check and do padding in bottom directino if need be */
        if(y_count != size)
        {
            hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
        }
    }
    else
    {
        S32 ai4_wt_refs[MAX_NUM_REF];
        U08 u1_num_valid_refs = 0;
        int32x4_t dst0_4x32b, dst1_4x32b, dst2_4x32b, dst3_4x32b;
        int32x4_t inv_wt_4x32b, off_4x32b;
        int16x8_t src0_8x16b, src1_8x16b, src2_8x16b, src3_8x16b;

        /* add value */
        int32x4_t add_4x32b = vdupq_n_s32(0x4000);
        int32x4_t log_wdc = vdupq_n_s32(ps_wt_inp_prms->wpred_log_wdc);

        for(i = 0; i < num_ref; i++)
        {
            if((WGHT_DEFAULT == (ps_wt_inp_prms->a_wpred_wt[i])) &&
               (0 == (ps_wt_inp_prms->a_wpred_off[i])))
            {
                ps_wt_inp_prms->apu1_wt_inp[i] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];
            }
            else
            {
                ai4_wt_refs[u1_num_valid_refs++] = i;
                ps_wt_inp_prms->apu1_wt_inp[i] = ps_wt_inp_prms->apu1_wt_inp_buf_array[i];
            }
        }

        ps_wt_inp_prms->apu1_wt_inp[num_ref] = ps_wt_inp_prms->apu1_wt_inp_buf_array[num_ref];

        if(0 == (x_count & 7)) /* wd multiple of 8 case */
        {
            uint8x8_t src0_8x8b, src1_8x8b, src2_8x8b, src3_8x8b;
            int16x8_t src4_8x16b, src5_8x16b, src6_8x16b, src7_8x16b, off_8x16b;
            int32x4_t dst4_4x32b, dst5_4x32b, dst6_4x32b, dst7_4x32b;

            for(i = 0; i < y_count; i += 4) /* 4 rows at a time */
            {
                for(j = 0; j < x_count; j += 8) /* 8 cols at a time */
                {
                    /* Load 4x8 Source */
                    /* Load Source : Lower 32 bit, Upper 32 bit neglected */
                    src0_8x8b = vld1_u8(pu1_src + 0 * src_stride);
                    src1_8x8b = vld1_u8(pu1_src + 1 * src_stride);
                    src2_8x8b = vld1_u8(pu1_src + 2 * src_stride);
                    src3_8x8b = vld1_u8(pu1_src + 3 * src_stride);

                    /* ref==num_ref */ /* last ref will be non weighted input */
                    pu1_dst = (ps_wt_inp_prms->apu1_wt_inp[num_ref]) + (i * dst_stride) + j;

                    /* Store */
                    vst1_u8((pu1_dst + 0 * dst_stride), src0_8x8b);
                    vst1_u8((pu1_dst + 1 * dst_stride), src1_8x8b);
                    vst1_u8((pu1_dst + 2 * dst_stride), src2_8x8b);
                    vst1_u8((pu1_dst + 3 * dst_stride), src3_8x8b);

                    if(u1_num_valid_refs)
                    {
                        /* Right 4x4 Block */
                        src0_8x16b = vreinterpretq_s16_u16(vmovl_u8(src0_8x8b));
                        src1_8x16b = vreinterpretq_s16_u16(vmovl_u8(src1_8x8b));
                        src2_8x16b = vreinterpretq_s16_u16(vmovl_u8(src2_8x8b));
                        src3_8x16b = vreinterpretq_s16_u16(vmovl_u8(src3_8x8b));
                    }

                    /* Run thro all ref ids, except ref==num_ref, which is already done */
                    for(ref = 0; ref < u1_num_valid_refs; ref++)
                    {
                        U08 u1_ref_idx = ai4_wt_refs[ref];

                        /* Each ref id may have differnet wt/offset. */
                        /* So we have unique inp buf for each ref id */
                        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[u1_ref_idx] + (i * dst_stride) + j;

                        /* InvWt and off specific to this ref id */
                        off_8x16b = vdupq_n_s16(ps_wt_inp_prms->a_wpred_off[u1_ref_idx]);
                        inv_wt_4x32b = vdupq_n_s32(ps_wt_inp_prms->a_inv_wpred_wt[u1_ref_idx]);

                        /* inp - off */
                        src4_8x16b = vsubq_s16(src0_8x16b, off_8x16b);
                        src5_8x16b = vsubq_s16(src1_8x16b, off_8x16b);
                        src6_8x16b = vsubq_s16(src2_8x16b, off_8x16b);
                        src7_8x16b = vsubq_s16(src3_8x16b, off_8x16b);

                        dst0_4x32b = vmovl_s16(vget_low_s16(src4_8x16b));
                        dst1_4x32b = vmovl_s16(vget_low_s16(src5_8x16b));
                        dst2_4x32b = vmovl_s16(vget_low_s16(src6_8x16b));
                        dst3_4x32b = vmovl_s16(vget_low_s16(src7_8x16b));

                        dst4_4x32b = vmovl_s16(vget_high_s16(src4_8x16b));
                        dst5_4x32b = vmovl_s16(vget_high_s16(src5_8x16b));
                        dst6_4x32b = vmovl_s16(vget_high_s16(src6_8x16b));
                        dst7_4x32b = vmovl_s16(vget_high_s16(src7_8x16b));

                        /* (inp-off) << shift */
                        dst0_4x32b = vshlq_s32(dst0_4x32b, log_wdc);
                        dst1_4x32b = vshlq_s32(dst1_4x32b, log_wdc);
                        dst2_4x32b = vshlq_s32(dst2_4x32b, log_wdc);
                        dst3_4x32b = vshlq_s32(dst3_4x32b, log_wdc);

                        /* (inp-off) << shift */
                        dst4_4x32b = vshlq_s32(dst4_4x32b, log_wdc);
                        dst5_4x32b = vshlq_s32(dst5_4x32b, log_wdc);
                        dst6_4x32b = vshlq_s32(dst6_4x32b, log_wdc);
                        dst7_4x32b = vshlq_s32(dst7_4x32b, log_wdc);

                        /* ((inp-off) << shift) * inv_wt + 1<<14 */
                        dst0_4x32b = vmlaq_s32(add_4x32b, dst0_4x32b, inv_wt_4x32b);
                        dst1_4x32b = vmlaq_s32(add_4x32b, dst1_4x32b, inv_wt_4x32b);
                        dst2_4x32b = vmlaq_s32(add_4x32b, dst2_4x32b, inv_wt_4x32b);
                        dst3_4x32b = vmlaq_s32(add_4x32b, dst3_4x32b, inv_wt_4x32b);

                        /* ((inp-off) << shift) * inv_wt + 1<<14 */
                        dst4_4x32b = vmlaq_s32(add_4x32b, dst4_4x32b, inv_wt_4x32b);
                        dst5_4x32b = vmlaq_s32(add_4x32b, dst5_4x32b, inv_wt_4x32b);
                        dst6_4x32b = vmlaq_s32(add_4x32b, dst6_4x32b, inv_wt_4x32b);
                        dst7_4x32b = vmlaq_s32(add_4x32b, dst7_4x32b, inv_wt_4x32b);

                        /* (((inp-off) << shift) * inv_wt + 1<<14) >> 15 */
                        src4_8x16b = vcombine_s16(
                            vshrn_n_s32(dst0_4x32b, IHEVCE_WT_PRED_SHIFT),
                            vshrn_n_s32(dst4_4x32b, IHEVCE_WT_PRED_SHIFT));
                        src5_8x16b = vcombine_s16(
                            vshrn_n_s32(dst1_4x32b, IHEVCE_WT_PRED_SHIFT),
                            vshrn_n_s32(dst5_4x32b, IHEVCE_WT_PRED_SHIFT));
                        src6_8x16b = vcombine_s16(
                            vshrn_n_s32(dst2_4x32b, IHEVCE_WT_PRED_SHIFT),
                            vshrn_n_s32(dst6_4x32b, IHEVCE_WT_PRED_SHIFT));
                        src7_8x16b = vcombine_s16(
                            vshrn_n_s32(dst3_4x32b, IHEVCE_WT_PRED_SHIFT),
                            vshrn_n_s32(dst7_4x32b, IHEVCE_WT_PRED_SHIFT));
                        /* Store */
                        vst1_u8((pu1_dst + 0 * dst_stride), vqmovun_s16(src4_8x16b));
                        vst1_u8((pu1_dst + 1 * dst_stride), vqmovun_s16(src5_8x16b));
                        vst1_u8((pu1_dst + 2 * dst_stride), vqmovun_s16(src6_8x16b));
                        vst1_u8((pu1_dst + 3 * dst_stride), vqmovun_s16(src7_8x16b));
                    }
                    /* Pointer update */
                    pu1_src += 8;
                }
                /* Pointer update */
                pu1_src = pu1_src - x_count + 4 * src_stride;
            }
        }
        else /* wd multiple of 4 case */
        {
            uint8x16_t src0_16x8b;
            int32x4_t src0_4x32b, src1_4x32b, src2_4x32b, src3_4x32b;
            WORD32 dst0, dst1, dst2, dst3;
            pu1_dst = ps_wt_inp_prms->apu1_wt_inp[num_ref];
            for(i = 0; i < y_count; i += 4) /* 4 rows at a time */
            {
                for(j = 0; j < x_count; j += 4) /* 4 cols at a time */
                {
                    /* ref==num_ref */ /* last ref will be non weighted input */

                    *(WORD32 *)(&pu1_dst[0 * dst_stride]) = *(WORD32 *)(&pu1_src[0 * src_stride]);
                    *(WORD32 *)(&pu1_dst[1 * dst_stride]) = *(WORD32 *)(&pu1_src[1 * src_stride]);
                    *(WORD32 *)(&pu1_dst[2 * dst_stride]) = *(WORD32 *)(&pu1_src[2 * src_stride]);
                    *(WORD32 *)(&pu1_dst[3 * dst_stride]) = *(WORD32 *)(&pu1_src[3 * src_stride]);

                    /* Pointer update */
                    pu1_src += 4;
                    pu1_dst += 4;
                }
                /* Pointer update */
                pu1_src = pu1_src - x_count + 4 * src_stride;
                pu1_dst = pu1_dst - x_count + 4 * dst_stride;
            }

            if(u1_num_valid_refs)
            {
                pu1_src = ps_curr_layer->pu1_inp;
                pu1_src += (pos_x + (pos_y * src_stride));

                /* Run thro all ref ids, except ref==num_ref, which is already done */
                for(ref = 0; ref < u1_num_valid_refs; ref++)
                {
                    U08 u1_ref_idx = ai4_wt_refs[ref];

                    pu1_dst = ps_wt_inp_prms->apu1_wt_inp[u1_ref_idx];

                    /* InvWt and off specific to this ref id */
                    off_4x32b = vdupq_n_s32(ps_wt_inp_prms->a_wpred_off[u1_ref_idx]);
                    inv_wt_4x32b = vdupq_n_s32(ps_wt_inp_prms->a_inv_wpred_wt[u1_ref_idx]);

                    for(i = 0; i < y_count; i += 4) /* 4 rows at a time */
                    {
                        for(j = 0; j < x_count; j += 4) /* 4 cols at a time */
                        {
                            src0_16x8b = load_unaligned_u8q(pu1_src, src_stride);

                            src0_8x16b = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(src0_16x8b)));
                            src1_8x16b = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(src0_16x8b)));

                            src0_4x32b = vmovl_s16(vget_low_s16(src0_8x16b));
                            src1_4x32b = vmovl_s16(vget_high_s16(src0_8x16b));
                            src2_4x32b = vmovl_s16(vget_low_s16(src1_8x16b));
                            src3_4x32b = vmovl_s16(vget_high_s16(src1_8x16b));

                            /* inp - off */
                            dst0_4x32b = vsubq_s32(src0_4x32b, off_4x32b);
                            dst1_4x32b = vsubq_s32(src1_4x32b, off_4x32b);
                            dst2_4x32b = vsubq_s32(src2_4x32b, off_4x32b);
                            dst3_4x32b = vsubq_s32(src3_4x32b, off_4x32b);

                            /* (inp-off) << shift */
                            dst0_4x32b = vshlq_s32(dst0_4x32b, log_wdc);
                            dst1_4x32b = vshlq_s32(dst1_4x32b, log_wdc);
                            dst2_4x32b = vshlq_s32(dst2_4x32b, log_wdc);
                            dst3_4x32b = vshlq_s32(dst3_4x32b, log_wdc);

                            /* ((inp-off) << shift) * inv_wt */
                            dst0_4x32b = vmlaq_s32(add_4x32b, dst0_4x32b, inv_wt_4x32b);
                            dst1_4x32b = vmlaq_s32(add_4x32b, dst1_4x32b, inv_wt_4x32b);
                            dst2_4x32b = vmlaq_s32(add_4x32b, dst2_4x32b, inv_wt_4x32b);
                            dst3_4x32b = vmlaq_s32(add_4x32b, dst3_4x32b, inv_wt_4x32b);

                            /* (((inp-off) << shift) * inv_wt + 1<<14) >> 15 */
                            dst0 = (WORD32)vget_lane_u64(
                                vreinterpret_u64_u16(
                                    vqshrun_n_s32(dst0_4x32b, IHEVCE_WT_PRED_SHIFT)),
                                0);
                            dst1 = (WORD32)vget_lane_u64(
                                vreinterpret_u64_u16(
                                    vqshrun_n_s32(dst1_4x32b, IHEVCE_WT_PRED_SHIFT)),
                                0);
                            dst2 = (WORD32)vget_lane_u64(
                                vreinterpret_u64_u16(
                                    vqshrun_n_s32(dst2_4x32b, IHEVCE_WT_PRED_SHIFT)),
                                0);
                            dst3 = (WORD32)vget_lane_u64(
                                vreinterpret_u64_u16(
                                    vqshrun_n_s32(dst3_4x32b, IHEVCE_WT_PRED_SHIFT)),
                                0);

                            *(WORD32 *)(&pu1_dst[0 * dst_stride]) = dst0;
                            *(WORD32 *)(&pu1_dst[1 * dst_stride]) = dst1;
                            *(WORD32 *)(&pu1_dst[2 * dst_stride]) = dst2;
                            *(WORD32 *)(&pu1_dst[3 * dst_stride]) = dst3;

                            /* Pointer update */
                            pu1_src += 4;
                            pu1_dst += 4;
                        }
                        /* Pointer update */
                        pu1_src = pu1_src - x_count + 4 * src_stride;
                        pu1_dst = pu1_dst - x_count + 4 * dst_stride;
                    }
                }
            }
        }

        /* Padding */
        for(ref = 0; ref < u1_num_valid_refs; ref++)
        {
            /* Check and do padding in right direction if need be */
            pu1_dst = ps_wt_inp_prms->apu1_wt_inp[ai4_wt_refs[ref]];
            if(x_count != size)
            {
                hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
            }

            /* Check and do padding in bottom directino if need be */
            if(y_count != size)
            {
                hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
            }
        }

        /* Check and do padding in right direction if need be */
        pu1_dst = ps_wt_inp_prms->apu1_wt_inp[num_ref];

        if(x_count != size)
        {
            hme_pad_right(pu1_dst + x_count - 1, dst_stride, size - x_count, y_count);
        }

        /* Check and do padding in bottom directino if need be */
        if(y_count != size)
        {
            hme_pad_bot(pu1_dst + (y_count - 1) * dst_stride, dst_stride, size - y_count, size);
        }
    }
}
