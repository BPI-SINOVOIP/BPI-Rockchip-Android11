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
*  ihevce_scale_by_2_neon.c
*
* @brief
*  Contains definitions of functions for scale by 2
*
* @author
*  Ittiam
*
* @par List of Functions:
*
* @remarks
*  None
*
********************************************************************************
*/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <arm_neon.h>

/* System user files */
#include "ihevc_typedefs.h"
#include "ihevc_macros.h"
#include "itt_video_api.h"
#include "ihevce_ipe_instr_set_router.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define FILT_TAP_Q 7

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static void ihevce_horz_scale_neon_w16(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_dst, WORD32 dst_strd, WORD32 wd, WORD32 ht)
{
    const int16x8_t prec = vdupq_n_s16(8192);
    const int16x8_t inv_prec = vdupq_n_s16(64);
    const uint8x8_t wt_0 = vdup_n_u8(66);
    const int8_t wt_1 = 40;
    const int8_t wt_2 = 9;
    WORD32 i, j;

    for(j = 0; j < ht; j++)
    {
        UWORD8 *pu1_src_tmp = pu1_src + j * src_strd - 3;
        UWORD8 *pu1_dst_tmp = pu1_dst + j * dst_strd;

        for(i = 0; i < wd;)
        {
            uint8x16x2_t src = vld2q_u8(pu1_src_tmp);
            uint8x8_t c, l0, r0, r3;
            int16x8_t p, q, r;
            int16x8_t sum;

            c = vext_u8(vget_low_u8(src.val[1]), vget_high_u8(src.val[1]), 1);
            l0 = vext_u8(vget_low_u8(src.val[0]), vget_high_u8(src.val[0]), 1);
            r0 = vext_u8(vget_low_u8(src.val[0]), vget_high_u8(src.val[0]), 2);
            r3 = vext_u8(vget_low_u8(src.val[0]), vget_high_u8(src.val[0]), 3);

            p = vreinterpretq_s16_u16(vmull_u8(c, wt_0));  // a[0] * 66
            q = vreinterpretq_s16_u16(vaddl_u8(l0, r0));
            q = vmulq_n_s16(q, wt_1);  // (a[-1] + a[1]) * 40
            r = vreinterpretq_s16_u16(vaddl_u8(r3, vget_low_u8(src.val[0])));
            r = vmulq_n_s16(r, wt_2);  // (a[-3] + a[3]) * 9

            // a[0] * 66 + (a[-1] + a[1]) * 40 - (a[-3] + a[3]) * 9
            sum = vsubq_s16(p, prec);
            q = vsubq_s16(q, r);
            sum = vaddq_s16(q, sum);
            sum = vrshrq_n_s16(sum, FILT_TAP_Q);
            sum = vaddq_s16(sum, inv_prec);

            // result
            c = vqmovun_s16(sum);
            vst1_u8(pu1_dst_tmp, c);

            i += 16;
            pu1_src_tmp += 16;
            pu1_dst_tmp += 8;
        }
    }
}

static void ihevce_vert_scale_neon_w16(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_dst, WORD32 dst_strd, WORD32 wd, WORD32 ht)
{
    const int16x8_t prec = vdupq_n_s16(8192);
    const int16x8_t inv_prec = vdupq_n_s16(64);
    const uint8x8_t wt_0 = vdup_n_u8(66);
    const int8_t wt_1 = 40;
    const int8_t wt_2 = 9;
    WORD32 i, j;

#define LOAD_ROW()                                                                                 \
    {                                                                                              \
        src[mod8] = vld1q_u8(pu1_src_tmp);                                                         \
        pu1_src_tmp += src_strd;                                                                   \
        mod8++;                                                                                    \
        mod8 &= 7;                                                                                 \
    }

    for(i = 0; i < wd; i += 16)
    {
        UWORD8 *pu1_src_tmp = pu1_src - 3 * src_strd + i;
        WORD32 lut_id = 0;
        UWORD8 mod8 = 0;
        uint8x16_t src[8];

        LOAD_ROW()  // r[-3]
        LOAD_ROW()  // r[-2]
        LOAD_ROW()  // r[-1]
        LOAD_ROW()  // r[0]
        LOAD_ROW()  // r[1]

        for(j = 0; j < ht; j += 2)
        {
            UWORD8 *pu1_dst_tmp = pu1_dst + (j >> 1) * dst_strd + i;
            UWORD8 c, t1, b1, t2, b2;
            int16x8_t p, q, r;
            int16x8_t sum;

            LOAD_ROW()  // r[2]
            LOAD_ROW()  // r[3]

            t2 = (lut_id & 7);
            t1 = (lut_id + 2) & 7;
            c = (lut_id + 3) & 7;
            b1 = (lut_id + 4) & 7;
            b2 = (lut_id + 6) & 7;
            lut_id += 2;

            // a[0] * 66
            p = vreinterpretq_s16_u16(vmull_u8(vget_low_u8(src[c]), wt_0));
            // (a[-1] + a[1]) * 40
            q = vreinterpretq_s16_u16(vaddl_u8(vget_low_u8(src[t1]), vget_low_u8(src[b1])));
            q = vmulq_n_s16(q, wt_1);
            // (a[-3] + a[3]) * 9
            r = vreinterpretq_s16_u16(vaddl_u8(vget_low_u8(src[t2]), vget_low_u8(src[b2])));
            r = vmulq_n_s16(r, wt_2);

            // a[0] * 66 + (a[-1] + a[1]) * 40 - (a[-3] + a[3]) * 9
            sum = vsubq_s16(p, prec);
            q = vsubq_s16(q, r);
            sum = vaddq_s16(q, sum);
            sum = vrshrq_n_s16(sum, FILT_TAP_Q);
            sum = vaddq_s16(sum, inv_prec);

            vst1_u8(pu1_dst_tmp, vqmovun_s16(sum));

            // a[0] * 66
            p = vreinterpretq_s16_u16(vmull_u8(vget_high_u8(src[c]), wt_0));
            // (a[-1] + a[1]) * 40
            q = vreinterpretq_s16_u16(vaddl_u8(vget_high_u8(src[t1]), vget_high_u8(src[b1])));
            q = vmulq_n_s16(q, wt_1);
            // (a[-3] + a[3]) * 9
            r = vreinterpretq_s16_u16(vaddl_u8(vget_high_u8(src[t2]), vget_high_u8(src[b2])));
            r = vmulq_n_s16(r, wt_2);

            // a[0] * 66 + (a[-1] + a[1]) * 40 - (a[-3] + a[3]) * 9
            sum = vsubq_s16(p, prec);
            q = vsubq_s16(q, r);
            sum = vaddq_s16(q, sum);
            sum = vrshrq_n_s16(sum, FILT_TAP_Q);
            sum = vaddq_s16(sum, inv_prec);

            vst1_u8(pu1_dst_tmp + 8, vqmovun_s16(sum));

            pu1_dst_tmp += 16;
        }
    }
}

void ihevce_scaling_filter_mxn_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_scrtch,
    WORD32 scrtch_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 ht,
    WORD32 wd)
{
    WORD32 i, j;

    assert(wd >= 16 && wd % 16 == 0);
    assert(ht % 2 == 0);
    for(j = 0; j < ht;)
    {
        UWORD8 *pu1_src_tmp = pu1_src + j * src_strd;
        UWORD8 *pu1_dst_tmp = pu1_dst + (j >> 1) * dst_strd;
        WORD32 rows = MIN(64, (ht - j));

        for(i = 0; i < wd;)
        {
            WORD32 cols;

            if((wd - i) >= 64)
                cols = 64;
            else if((wd - i) >= 32)
                cols = 32;
            else
                cols = 16;

            ihevce_horz_scale_neon_w16(
                pu1_src_tmp - 3 * src_strd + i,
                src_strd,
                pu1_scrtch,
                scrtch_strd,
                cols,
                (3 + rows + 2));
            ihevce_vert_scale_neon_w16(
                pu1_scrtch + 3 * scrtch_strd,
                scrtch_strd,
                pu1_dst_tmp + (i >> 1),
                dst_strd,
                (cols >> 1),
                rows);
            i += cols;
        }
        j += rows;
    }
}
