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
*  ihevce_had_compute_neon.c
*
* @brief
*  Contains intrinsic definitions of functions for computing had
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
#include <string.h>
#include <assert.h>
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_cmn_utils_neon.h"
#include "ihevce_had_satd.h"
#include "ihevce_cmn_utils_instr_set_router.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
const int16_t gu2_dc_mask[8] = { 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff };

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define RESIDUE(k, is_chroma)                                                                      \
    if(!is_chroma)                                                                                 \
    {                                                                                              \
        const uint8x8_t s##k = vld1_u8(pu1_src);                                                   \
        const uint8x8_t p##k = vld1_u8(pu1_pred);                                                  \
        *r##k = vreinterpretq_s16_u16(vsubl_u8(s##k, p##k));                                       \
        pu1_src += src_strd;                                                                       \
        pu1_pred += pred_strd;                                                                     \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
        const uint8x8_t s##k = vld2_u8(pu1_src).val[0];                                            \
        const uint8x8_t p##k = vld2_u8(pu1_pred).val[0];                                           \
        *r##k = vreinterpretq_s16_u16(vsubl_u8(s##k, p##k));                                       \
        pu1_src += src_strd;                                                                       \
        pu1_pred += pred_strd;                                                                     \
    }

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static INLINE void
    hadamard4x4_2_one_pass(int16x8_t *r0, int16x8_t *r1, int16x8_t *r2, int16x8_t *r3)
{
    const int16x8_t a0 = vaddq_s16(*r0, *r2);
    const int16x8_t a1 = vaddq_s16(*r1, *r3);
    const int16x8_t a2 = vsubq_s16(*r0, *r2);
    const int16x8_t a3 = vsubq_s16(*r1, *r3);

    *r0 = vaddq_s16(a0, a1);
    *r1 = vsubq_s16(a0, a1);
    *r2 = vaddq_s16(a2, a3);
    *r3 = vsubq_s16(a2, a3);
}

static INLINE void hadamard4x4_2(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    int16x8_t *r0,
    int16x8_t *r1,
    int16x8_t *r2,
    int16x8_t *r3)
{
    // compute error between src and pred
    RESIDUE(0, 0);
    RESIDUE(1, 0);
    RESIDUE(2, 0);
    RESIDUE(3, 0);

    // vertical hadamard tx
    hadamard4x4_2_one_pass(r0, r1, r2, r3);

    // transpose
    transpose_s16_4x4q(r0, r1, r2, r3);

    // horizontal hadamard tx
    hadamard4x4_2_one_pass(r0, r1, r2, r3);
}

static INLINE void hadamard4x4_4(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    int16x8_t *r0,
    int16x8_t *r1,
    int16x8_t *r2,
    int16x8_t *r3,
    int16x8_t *r4,
    int16x8_t *r5,
    int16x8_t *r6,
    int16x8_t *r7)
{
    // hadamard 4x4_2n
    hadamard4x4_2(pu1_src, src_strd, pu1_pred, pred_strd, r0, r1, r2, r3);

    // hadamard 4x4_2n
    pu1_src += (4 * src_strd);
    pu1_pred += (4 * pred_strd);
    hadamard4x4_2(pu1_src, src_strd, pu1_pred, pred_strd, r4, r5, r6, r7);
}

static INLINE WORD32 hadamard_sad4x4_4(int16x8_t *a, WORD32 *pi4_hsad, WORD32 hsad_stride)
{
    int16x8_t p[8];
    int32x4_t b01, b23;
    int64x2_t c01, c23;
    int32x2_t d01, d23;

    // satd
    p[0] = vabsq_s16(a[0]);
    p[1] = vabsq_s16(a[1]);
    p[0] = vaddq_s16(p[0], p[1]);
    p[2] = vabsq_s16(a[2]);
    p[3] = vabsq_s16(a[3]);
    p[2] = vaddq_s16(p[2], p[3]);

    p[4] = vabsq_s16(a[4]);
    p[5] = vabsq_s16(a[5]);
    p[4] = vaddq_s16(p[4], p[5]);
    p[6] = vabsq_s16(a[6]);
    p[7] = vabsq_s16(a[7]);
    p[6] = vaddq_s16(p[6], p[7]);

    p[0] = vaddq_s16(p[0], p[2]);
    b01 = vpaddlq_s16(p[0]);
    c01 = vpaddlq_s32(b01);
    d01 = vrshrn_n_s64(c01, 2);
    vst1_s32(pi4_hsad, d01);
    pi4_hsad += hsad_stride;

    p[4] = vaddq_s16(p[4], p[6]);
    b23 = vpaddlq_s16(p[4]);
    c23 = vpaddlq_s32(b23);
    d23 = vrshrn_n_s64(c23, 2);
    vst1_s32(pi4_hsad, d23);

    d01 = vadd_s32(d01, d23);

    return (WORD32)(vget_lane_s64(vpaddl_s32(d01), 0));
}

static INLINE WORD32 hadamard_sad8x8_using4x4(int16x8_t *a, WORD32 *early_cbf, WORD32 i4_frm_qstep)
{
    int16x8_t p[8];
    const int16x8_t threshold = vdupq_n_s16((int16_t)(i4_frm_qstep >> 8));
    int32x4_t b;
    int64x2_t c;
    int64_t satd;
    WORD32 i;

    for(i = 0; i < 4; i++)
    {
        int16x8_t p0 = vaddq_s16(a[i], a[i + 4]);
        int16x8_t p1 = vsubq_s16(a[i], a[i + 4]);

        int16x4_t q0 = vadd_s16(vget_low_s16(p0), vget_high_s16(p0));
        int16x4_t q1 = vsub_s16(vget_low_s16(p0), vget_high_s16(p0));
        int16x4_t q2 = vadd_s16(vget_low_s16(p1), vget_high_s16(p1));
        int16x4_t q3 = vsub_s16(vget_low_s16(p1), vget_high_s16(p1));

        a[i] = vcombine_s16(q0, q2);
        a[i + 4] = vcombine_s16(q1, q3);
    }

#define EARLY_EXIT(k)                                                                              \
    {                                                                                              \
        p[k] = vabsq_s16(a[k]);                                                                    \
        if(*early_cbf == 0)                                                                        \
        {                                                                                          \
            uint16x8_t cmp;                                                                        \
            cmp = vcgtq_s16(p[k], threshold);                                                      \
            if(vget_lane_s64(vreinterpret_s64_u16(vget_low_u16(cmp)), 0) ||                        \
               vget_lane_s64(vreinterpret_s64_u16(vget_high_u16(cmp)), 0))                         \
            {                                                                                      \
                *early_cbf = 1;                                                                    \
            }                                                                                      \
        }                                                                                          \
    }
    // satd
    EARLY_EXIT(0);
    EARLY_EXIT(1);
    p[0] = vaddq_s16(p[0], p[1]);
    EARLY_EXIT(2);
    EARLY_EXIT(3);
    p[2] = vaddq_s16(p[2], p[3]);

    EARLY_EXIT(4);
    EARLY_EXIT(5);
    p[4] = vaddq_s16(p[4], p[5]);
    EARLY_EXIT(6);
    EARLY_EXIT(7);
#undef EARLY_EXIT
    p[6] = vaddq_s16(p[6], p[7]);

    p[0] = vaddq_s16(p[0], p[2]);
    p[4] = vaddq_s16(p[4], p[6]);
    p[0] = vaddq_s16(p[0], p[4]);
    b = vpaddlq_s16(p[0]);
    c = vpaddlq_s32(b);
    satd = vget_lane_s64(vadd_s64(vget_low_s64(c), vget_high_s64(c)), 0);

    return ((satd + 4) >> 3);
}

static INLINE void hadamard8x8_one_pass(
    int16x8_t *r0,
    int16x8_t *r1,
    int16x8_t *r2,
    int16x8_t *r3,
    int16x8_t *r4,
    int16x8_t *r5,
    int16x8_t *r6,
    int16x8_t *r7)
{
    const int16x8_t a0 = vaddq_s16(*r0, *r4);
    const int16x8_t a4 = vsubq_s16(*r0, *r4);
    const int16x8_t a1 = vaddq_s16(*r1, *r5);
    const int16x8_t a5 = vsubq_s16(*r1, *r5);
    const int16x8_t a2 = vaddq_s16(*r2, *r6);
    const int16x8_t a6 = vsubq_s16(*r2, *r6);
    const int16x8_t a3 = vaddq_s16(*r3, *r7);
    const int16x8_t a7 = vsubq_s16(*r3, *r7);

    const int16x8_t b0 = vaddq_s16(a0, a2);
    const int16x8_t b2 = vsubq_s16(a0, a2);
    const int16x8_t b1 = vaddq_s16(a1, a3);
    const int16x8_t b3 = vsubq_s16(a1, a3);
    const int16x8_t b4 = vaddq_s16(a4, a6);
    const int16x8_t b6 = vsubq_s16(a4, a6);
    const int16x8_t b5 = vaddq_s16(a5, a7);
    const int16x8_t b7 = vsubq_s16(a5, a7);

    *r0 = vaddq_s16(b0, b1);
    *r1 = vsubq_s16(b0, b1);
    *r2 = vaddq_s16(b2, b3);
    *r3 = vsubq_s16(b2, b3);
    *r4 = vaddq_s16(b4, b5);
    *r5 = vsubq_s16(b4, b5);
    *r6 = vaddq_s16(b6, b7);
    *r7 = vsubq_s16(b6, b7);
}

static INLINE void hadamard8x8(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    int16x8_t *r0,
    int16x8_t *r1,
    int16x8_t *r2,
    int16x8_t *r3,
    int16x8_t *r4,
    int16x8_t *r5,
    int16x8_t *r6,
    int16x8_t *r7,
    WORD32 is_chroma)
{
    // compute error between src and pred
    RESIDUE(0, is_chroma);
    RESIDUE(1, is_chroma);
    RESIDUE(2, is_chroma);
    RESIDUE(3, is_chroma);
    RESIDUE(4, is_chroma);
    RESIDUE(5, is_chroma);
    RESIDUE(6, is_chroma);
    RESIDUE(7, is_chroma);

    // vertical hadamard tx
    hadamard8x8_one_pass(r0, r1, r2, r3, r4, r5, r6, r7);

    // transpose
    transpose_s16_8x8(r0, r1, r2, r3, r4, r5, r6, r7);

    // horizontal hadamard tx
    hadamard8x8_one_pass(r0, r1, r2, r3, r4, r5, r6, r7);
}

static INLINE UWORD32 ihevce_HAD_8x8_8bit_plane_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD32 is_chroma,
    WORD32 ac_only)
{
    int16x8_t a0, a1, a2, a3, a4, a5, a6, a7;
    int32x4_t b;
    int64x2_t c;
    int64_t satd;

    // hadamard 8x8
    hadamard8x8(
        pu1_src, src_strd, pu1_pred, pred_strd, &a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7, is_chroma);

    if(ac_only)
    {
        const int16x8_t mask = vld1q_s16(gu2_dc_mask);
        a0 = vandq_s16(a0, mask);
    }

    // satd
    a0 = vabsq_s16(a0);
    a1 = vabsq_s16(a1);
    a0 = vaddq_s16(a0, a1);
    a2 = vabsq_s16(a2);
    a3 = vabsq_s16(a3);
    a2 = vaddq_s16(a2, a3);

    a4 = vabsq_s16(a4);
    a5 = vabsq_s16(a5);
    a4 = vaddq_s16(a4, a5);
    a6 = vabsq_s16(a6);
    a7 = vabsq_s16(a7);
    a6 = vaddq_s16(a6, a7);

    a0 = vaddq_s16(a0, a2);
    a4 = vaddq_s16(a4, a6);
    a0 = vaddq_s16(a0, a4);
    b = vpaddlq_s16(a0);
    c = vpaddlq_s32(b);
    satd = vget_lane_s64(vadd_s64(vget_low_s64(c), vget_high_s64(c)), 0);

    return ((satd + 4) >> 3);
}

static INLINE UWORD32 ihevce_HAD_4x4_8bit_plane_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD32 is_chroma,
    WORD32 ac_only)
{
    uint8x16_t src_u8, pred_u8;
    int16x8_t res_01, res_23;
    int16x4_t h[4];
    int16x4_t v[4];
    int16x4x2_t trans_4[2];
    int16x8_t combined_rows[4];
    int32x4x2_t trans_8;
    int32x4_t sad_32_4[3];
    int32x2_t sad_32_2;
    int64x1_t sad_64_1;
    int32_t sad;

    if(!is_chroma)
    {
        src_u8 = load_unaligned_u8q(pu1_src, src_strd);
        pred_u8 = load_unaligned_u8q(pu1_pred, pred_strd);
    }
    else
    {
        src_u8 = load_unaligned_u8qi(pu1_src, src_strd);
        pred_u8 = load_unaligned_u8qi(pu1_pred, pred_strd);
    }
    res_01 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(src_u8), vget_low_u8(pred_u8)));
    res_23 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(src_u8), vget_high_u8(pred_u8)));

    h[0] = vadd_s16(vget_low_s16(res_01), vget_high_s16(res_23));
    h[1] = vadd_s16(vget_high_s16(res_01), vget_low_s16(res_23));
    h[2] = vsub_s16(vget_high_s16(res_01), vget_low_s16(res_23));
    h[3] = vsub_s16(vget_low_s16(res_01), vget_high_s16(res_23));

    v[0] = vadd_s16(h[0], h[1]);
    v[1] = vadd_s16(h[3], h[2]);
    v[2] = vsub_s16(h[0], h[1]);
    v[3] = vsub_s16(h[3], h[2]);

    trans_4[0] = vtrn_s16(v[0], v[2]);
    trans_4[1] = vtrn_s16(v[1], v[3]);

    combined_rows[0] = vcombine_s16(trans_4[0].val[0], trans_4[1].val[0]);
    combined_rows[1] = vcombine_s16(trans_4[0].val[1], trans_4[1].val[1]);

    combined_rows[2] = vaddq_s16(combined_rows[0], combined_rows[1]);
    combined_rows[3] = vsubq_s16(combined_rows[0], combined_rows[1]);

    trans_8 =
        vtrnq_s32(vreinterpretq_s32_s16(combined_rows[2]), vreinterpretq_s32_s16(combined_rows[3]));

    combined_rows[0] =
        vaddq_s16(vreinterpretq_s16_s32(trans_8.val[0]), vreinterpretq_s16_s32(trans_8.val[1]));
    combined_rows[0] = vabsq_s16(combined_rows[0]);
    combined_rows[1] =
        vsubq_s16(vreinterpretq_s16_s32(trans_8.val[0]), vreinterpretq_s16_s32(trans_8.val[1]));
    combined_rows[1] = vabsq_s16(combined_rows[1]);

    if(ac_only)
    {
        const int16x8_t mask = vld1q_s16(gu2_dc_mask);
        combined_rows[0] = vandq_s16(combined_rows[0], mask);
    }

    sad_32_4[0] = vpaddlq_s16(combined_rows[0]);
    sad_32_4[1] = vpaddlq_s16(combined_rows[1]);
    sad_32_4[2] = vaddq_s32(sad_32_4[0], sad_32_4[1]);
    sad_32_2 = vadd_s32(vget_high_s32(sad_32_4[2]), vget_low_s32(sad_32_4[2]));
    sad_64_1 = vpaddl_s32(sad_32_2);
    sad = vget_lane_s64(sad_64_1, 0);

    return ((sad + 2) >> 2);
}

UWORD32 ihevce_HAD_4x4_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    (void)pi2_dst;
    (void)dst_strd;
    return ihevce_HAD_4x4_8bit_plane_neon(pu1_src, src_strd, pu1_pred, pred_strd, 0, 0);
}

UWORD32 ihevce_chroma_compute_AC_HAD_4x4_8bit_neon(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    (void)pi2_dst;
    (void)dst_strd;
    return ihevce_HAD_4x4_8bit_plane_neon(pu1_origin, src_strd, pu1_pred_buf, pred_strd, 1, 1);
}

UWORD32 ihevce_HAD_8x8_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    (void)pi2_dst;
    (void)dst_strd;
    return ihevce_HAD_8x8_8bit_plane_neon(pu1_src, src_strd, pu1_pred, pred_strd, 0, 0);
}

UWORD32 ihevce_compute_ac_had_8x8_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    (void)pi2_dst;
    (void)dst_strd;
    return ihevce_HAD_8x8_8bit_plane_neon(pu1_src, src_strd, pu1_pred, pred_strd, 0, 1);
}

UWORD32 ihevce_HAD_16x16_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    int16x8_t b0[8];
    int16x8_t b1[8];
    int16x8_t b2[8];
    int16x8_t b3[8];
    uint32x4_t sum = vdupq_n_u32(0);
    uint64x2_t c;
    uint64_t satd;
    WORD32 i;

    (void)pi2_dst;
    (void)dst_strd;

    // hadamard 8x8 - b0
    hadamard8x8(
        pu1_src,
        src_strd,
        pu1_pred,
        pred_strd,
        &b0[0],
        &b0[1],
        &b0[2],
        &b0[3],
        &b0[4],
        &b0[5],
        &b0[6],
        &b0[7],
        0);
    // hadamard 8x8 - b1
    hadamard8x8(
        pu1_src + 8,
        src_strd,
        pu1_pred + 8,
        pred_strd,
        &b1[0],
        &b1[1],
        &b1[2],
        &b1[3],
        &b1[4],
        &b1[5],
        &b1[6],
        &b1[7],
        0);
    // hadamard 8x8 - b2
    hadamard8x8(
        pu1_src + (8 * src_strd),
        src_strd,
        pu1_pred + (8 * pred_strd),
        pred_strd,
        &b2[0],
        &b2[1],
        &b2[2],
        &b2[3],
        &b2[4],
        &b2[5],
        &b2[6],
        &b2[7],
        0);
    // hadamard 8x8 - b3
    hadamard8x8(
        pu1_src + (8 * src_strd) + 8,
        src_strd,
        pu1_pred + (8 * pred_strd) + 8,
        pred_strd,
        &b3[0],
        &b3[1],
        &b3[2],
        &b3[3],
        &b3[4],
        &b3[5],
        &b3[6],
        &b3[7],
        0);

    for(i = 0; i < 8; i++)
    {
        int16x8_t p0 = vhaddq_s16(b0[i], b1[i]);
        int16x8_t p1 = vhsubq_s16(b0[i], b1[i]);
        int16x8_t p2 = vhaddq_s16(b2[i], b3[i]);
        int16x8_t p3 = vhsubq_s16(b2[i], b3[i]);

        int16x8_t q0 = vaddq_s16(p0, p2);
        int16x8_t q1 = vsubq_s16(p0, p2);
        int16x8_t q2 = vaddq_s16(p1, p3);
        int16x8_t q3 = vsubq_s16(p1, p3);

        uint16x8_t r0 =
            vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(q0)), vreinterpretq_u16_s16(vabsq_s16(q1)));
        uint16x8_t r1 =
            vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(q2)), vreinterpretq_u16_s16(vabsq_s16(q3)));

        uint32x4_t s0 = vaddl_u16(vget_low_u16(r0), vget_high_u16(r0));
        uint32x4_t s1 = vaddl_u16(vget_low_u16(r1), vget_high_u16(r1));

        sum = vaddq_u32(sum, s0);
        sum = vaddq_u32(sum, s1);
    }

    c = vpaddlq_u32(sum);
    satd = vget_lane_u64(vadd_u64(vget_low_u64(c), vget_high_u64(c)), 0);

    return ((satd + 4) >> 3);
}

UWORD32 ihevce_chroma_HAD_4x4_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    (void)pi2_dst;
    (void)dst_strd;
    return ihevce_HAD_4x4_8bit_plane_neon(pu1_src, src_strd, pu1_pred, pred_strd, 1, 0);
}

UWORD32 ihevce_chroma_HAD_8x8_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    (void)pi2_dst;
    (void)dst_strd;
    return ihevce_HAD_8x8_8bit_plane_neon(pu1_src, src_strd, pu1_pred, pred_strd, 1, 0);
}

UWORD32 ihevce_chroma_HAD_16x16_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    UWORD32 au4_satd[4];

    (void)pi2_dst;
    (void)dst_strd;
    au4_satd[0] = ihevce_HAD_8x8_8bit_plane_neon(pu1_src, src_strd, pu1_pred, pred_strd, 1, 0);
    au4_satd[1] =
        ihevce_HAD_8x8_8bit_plane_neon(pu1_src + 16, src_strd, pu1_pred + 16, pred_strd, 1, 0);
    au4_satd[2] = ihevce_HAD_8x8_8bit_plane_neon(
        pu1_src + 8 * src_strd, src_strd, pu1_pred + 8 * pred_strd, pred_strd, 1, 0);
    au4_satd[3] = ihevce_HAD_8x8_8bit_plane_neon(
        pu1_src + 8 * src_strd + 16, src_strd, pu1_pred + 8 * pred_strd + 16, pred_strd, 1, 0);

    return au4_satd[0] + au4_satd[1] + au4_satd[2] + au4_satd[3];
}

UWORD32 ihevce_HAD_32x32_8bit_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    int16x8_t a[4][4][8];
    uint32x4_t sum = vdupq_n_u32(0);
    WORD32 b8, b16;
    uint64x2_t c;
    uint64_t satd;
    WORD32 i, j;

    (void)pi2_dst;
    (void)dst_strd;
    // hadamard 32x32
    for(b16 = 0; b16 < 4; b16++)
    {
        UWORD8 *pu1_src_b16 = pu1_src + (b16 >> 1) * (src_strd * 16) + ((b16 & 1) * 16);
        UWORD8 *pu1_pred_b16 = pu1_pred + (b16 >> 1) * (pred_strd * 16) + ((b16 & 1) * 16);
        // hadamard 16x16
        for(b8 = 0; b8 < 4; b8++)
        {
            UWORD8 *pu1_src_b8 = pu1_src_b16 + (b8 >> 1) * (src_strd * 8) + ((b8 & 1) * 8);
            UWORD8 *pu1_pred_b8 = pu1_pred_b16 + (b8 >> 1) * (pred_strd * 8) + ((b8 & 1) * 8);
            // hadamard 8x8
            hadamard8x8(
                pu1_src_b8,
                src_strd,
                pu1_pred_b8,
                pred_strd,
                &a[b16][b8][0],
                &a[b16][b8][1],
                &a[b16][b8][2],
                &a[b16][b8][3],
                &a[b16][b8][4],
                &a[b16][b8][5],
                &a[b16][b8][6],
                &a[b16][b8][7],
                0);
        }
        for(i = 0; i < 8; i++)
        {
            int16x8_t p0 = vhaddq_s16(a[b16][0][i], a[b16][1][i]);
            int16x8_t p1 = vhsubq_s16(a[b16][0][i], a[b16][1][i]);
            int16x8_t p2 = vhaddq_s16(a[b16][2][i], a[b16][3][i]);
            int16x8_t p3 = vhsubq_s16(a[b16][2][i], a[b16][3][i]);

            a[b16][0][i] = vaddq_s16(p0, p2);
            a[b16][1][i] = vsubq_s16(p0, p2);
            a[b16][2][i] = vaddq_s16(p1, p3);
            a[b16][3][i] = vsubq_s16(p1, p3);

            a[b16][0][i] = vshrq_n_s16(a[b16][0][i], 2);
            a[b16][1][i] = vshrq_n_s16(a[b16][1][i], 2);
            a[b16][2][i] = vshrq_n_s16(a[b16][2][i], 2);
            a[b16][3][i] = vshrq_n_s16(a[b16][3][i], 2);
        }
    }
    for(j = 0; j < 4; j++)
    {
        for(i = 0; i < 8; i++)
        {
            int16x8_t p0 = vaddq_s16(a[0][j][i], a[1][j][i]);
            int16x8_t p1 = vsubq_s16(a[0][j][i], a[1][j][i]);
            int16x8_t p2 = vaddq_s16(a[2][j][i], a[3][j][i]);
            int16x8_t p3 = vsubq_s16(a[2][j][i], a[3][j][i]);

            int16x8_t q0 = vaddq_s16(p0, p2);
            int16x8_t q1 = vsubq_s16(p0, p2);
            int16x8_t q2 = vaddq_s16(p1, p3);
            int16x8_t q3 = vsubq_s16(p1, p3);

            uint16x8_t r0 = vaddq_u16(
                vreinterpretq_u16_s16(vabsq_s16(q0)), vreinterpretq_u16_s16(vabsq_s16(q1)));
            uint16x8_t r1 = vaddq_u16(
                vreinterpretq_u16_s16(vabsq_s16(q2)), vreinterpretq_u16_s16(vabsq_s16(q3)));

            uint32x4_t s0 = vaddl_u16(vget_low_u16(r0), vget_high_u16(r0));
            uint32x4_t s1 = vaddl_u16(vget_low_u16(r1), vget_high_u16(r1));

            sum = vaddq_u32(sum, s0);
            sum = vaddq_u32(sum, s1);
        }
    }
    c = vpaddlq_u32(sum);
    satd = vget_lane_u64(vadd_u64(vget_low_u64(c), vget_high_u64(c)), 0);

    return ((satd + 2) >> 2);
}

WORD32 ihevce_had4_4x4_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst4x4,
    WORD32 dst_strd,
    WORD32 *pi4_hsad,
    WORD32 hsad_stride,
    WORD32 i4_frm_qstep)
{
    int16x8_t a[8];

    (void)pi2_dst4x4;
    (void)dst_strd;
    (void)i4_frm_qstep;

    /* -------- Compute four 4x4 HAD Transforms of 8x8 in one call--------- */
    hadamard4x4_4(
        pu1_src,
        src_strd,
        pu1_pred,
        pred_strd,
        &a[0],
        &a[1],
        &a[2],
        &a[3],
        &a[4],
        &a[5],
        &a[6],
        &a[7]);

    return hadamard_sad4x4_4(a, pi4_hsad, hsad_stride);
}

WORD32 ihevce_had_8x8_using_4_4x4_r_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 **ppi4_tu_split,
    WORD32 **ppi4_tu_early_cbf,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    WORD32 i4_cur_depth,
    WORD32 i4_max_depth,
    WORD32 i4_max_tr_size,
    WORD32 *pi4_tu_split_cost,
    void *pv_func_sel)
{
    WORD32 pos_x = pos_x_y_4x4 & 0xFFFF;
    WORD32 pos_y = (pos_x_y_4x4 >> 16) & 0xFFFF;

    WORD32 *pi4_4x4_hsad;
    WORD32 *pi4_8x8_hsad;
    WORD32 *pi4_8x8_tu_split;
    WORD32 *pi4_8x8_tu_early_cbf;

    WORD32 cost_child, cost_parent;
    WORD32 best_cost;
    WORD32 early_cbf = 0;
    const UWORD8 u1_cur_tr_size = 8;

    WORD32 i;

    int16x8_t a[8];

    (void)pv_func_sel;

    assert(pos_x >= 0);
    assert(pos_y >= 0);

    /* Initialize pointers to  store 4x4 and 8x8 HAD SATDs */
    pi4_4x4_hsad = ppi4_hsad[HAD_4x4] + pos_x + pos_y * num_4x4_in_row;
    pi4_8x8_hsad = ppi4_hsad[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);
    pi4_8x8_tu_split = ppi4_tu_split[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);
    pi4_8x8_tu_early_cbf =
        ppi4_tu_early_cbf[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);

    /* -------- Compute four 4x4 HAD Transforms of 8x8 in one call--------- */
    hadamard4x4_4(
        pu1_src,
        src_strd,
        pu1_pred,
        pred_strd,
        &a[0],
        &a[1],
        &a[2],
        &a[3],
        &a[4],
        &a[5],
        &a[6],
        &a[7]);

    /* -------- cost child -------- */
    cost_child = hadamard_sad4x4_4(a, pi4_4x4_hsad, num_4x4_in_row);
    /* 4 CBF Flags, extra 1 becoz of the 0.5 bits per bin is assumed */
    cost_child += ((4) * lambda) >> (lambda_q_shift + 1);

    /* -------- cost parent -------- */
    cost_parent = hadamard_sad8x8_using4x4(a, &early_cbf, i4_frm_qstep);
    for(i = 0; i < 8; i++, pi2_dst += dst_strd)
        vst1q_s16(pi2_dst, a[i]);

    if(i4_cur_depth < i4_max_depth)
    {
        if((cost_child < cost_parent) || (i4_max_tr_size < u1_cur_tr_size))
        {
            *pi4_tu_split_cost += (4 * lambda) >> (lambda_q_shift + 1);
            best_cost = cost_child;
            best_cost <<= 1;
            best_cost++;
            pi4_8x8_tu_split[0] = 1;
            pi4_8x8_hsad[0] = cost_child;
        }
        else
        {
            best_cost = cost_parent;
            best_cost <<= 1;
            pi4_8x8_tu_split[0] = 0;
            pi4_8x8_hsad[0] = cost_parent;
        }
    }
    else
    {
        best_cost = cost_parent;
        best_cost <<= 1;
        pi4_8x8_tu_split[0] = 0;
        pi4_8x8_hsad[0] = cost_parent;
    }

    pi4_8x8_tu_early_cbf[0] = early_cbf;

    /* best cost has tu_split_flag at LSB(Least significant bit) */
    return ((best_cost << 1) + early_cbf);
}

static WORD32 ihevce_compute_16x16HAD_using_8x8_neon(
    WORD16 *pi2_8x8_had,
    WORD32 had8_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf)
{
    int16x8_t b0[8];
    int16x8_t b1[8];
    int16x8_t b2[8];
    int16x8_t b3[8];
    const int16x8_t threshold = vdupq_n_s16((int16_t)(i4_frm_qstep >> 8));
    uint32x4_t sum = vdupq_n_u32(0);
    uint64x2_t c;
    uint64_t satd;
    WORD32 i;

    for(i = 0; i < 8; i++, pi2_8x8_had += had8_strd)
    {
        b0[i] = vld1q_s16(pi2_8x8_had);
        b1[i] = vld1q_s16(pi2_8x8_had + 8);
    }
    for(i = 0; i < 8; i++, pi2_8x8_had += had8_strd)
    {
        b2[i] = vld1q_s16(pi2_8x8_had);
        b3[i] = vld1q_s16(pi2_8x8_had + 8);
    }

#define EARLY_EXIT(k)                                                                              \
    {                                                                                              \
        p##k = vabsq_s16(q##k);                                                                    \
        if(*pi4_cbf == 0)                                                                          \
        {                                                                                          \
            uint16x8_t cmp;                                                                        \
            cmp = vcgtq_s16(p##k, threshold);                                                      \
            if(vget_lane_s64(vreinterpret_s64_u16(vget_low_u16(cmp)), 0) ||                        \
               vget_lane_s64(vreinterpret_s64_u16(vget_high_u16(cmp)), 0))                         \
            {                                                                                      \
                *pi4_cbf = 1;                                                                      \
            }                                                                                      \
        }                                                                                          \
    }
    for(i = 0; i < 8; i++, pi2_dst += dst_strd)
    {
        int16x8_t p0 = vhaddq_s16(b0[i], b1[i]);
        int16x8_t p1 = vhsubq_s16(b0[i], b1[i]);
        int16x8_t p2 = vhaddq_s16(b2[i], b3[i]);
        int16x8_t p3 = vhsubq_s16(b2[i], b3[i]);

        int16x8_t q0 = vaddq_s16(p0, p2);
        int16x8_t q1 = vsubq_s16(p0, p2);
        int16x8_t q2 = vaddq_s16(p1, p3);
        int16x8_t q3 = vsubq_s16(p1, p3);

        vst1q_s16(pi2_dst, q0);
        EARLY_EXIT(0);
        vst1q_s16(pi2_dst + 8, q1);
        EARLY_EXIT(1);
        vst1q_s16(pi2_dst + 8 * dst_strd, q2);
        EARLY_EXIT(2);
        vst1q_s16(pi2_dst + 8 * dst_strd + 8, q3);
        EARLY_EXIT(3);
        uint16x8_t r0 = vaddq_u16(vreinterpretq_u16_s16(p0), vreinterpretq_u16_s16(p1));
        uint16x8_t r1 = vaddq_u16(vreinterpretq_u16_s16(p2), vreinterpretq_u16_s16(p3));

        uint32x4_t s0 = vaddl_u16(vget_low_u16(r0), vget_high_u16(r0));
        uint32x4_t s1 = vaddl_u16(vget_low_u16(r1), vget_high_u16(r1));

        sum = vaddq_u32(sum, s0);
        sum = vaddq_u32(sum, s1);
    }

    c = vpaddlq_u32(sum);
    satd = vget_lane_u64(vadd_u64(vget_low_u64(c), vget_high_u64(c)), 0);

    return ((satd + 4) >> 3);
}

WORD32 ihevce_had_16x16_r_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 **ppi4_tu_split,
    WORD32 **ppi4_tu_early_cbf,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    WORD32 i4_cur_depth,
    WORD32 i4_max_depth,
    WORD32 i4_max_tr_size,
    WORD32 *pi4_tu_split_cost,
    void *pv_func_sel)
{
    WORD16 ai2_8x8_had[256];

    WORD32 *pi4_16x16_hsad;
    WORD32 *pi4_16x16_tu_split;
    WORD32 *pi4_16x16_tu_early_cbf;

    WORD32 best_cost, best_cost_tu_split;
    WORD32 tu_split_flag = 0;
    WORD32 i4_early_cbf_flag = 0, early_cbf = 0;
    WORD32 cost_parent, cost_child = 0;

    const UWORD8 u1_cur_tr_size = 16;

    WORD32 i;

    WORD16 *pi2_y0;
    UWORD8 *src, *pred;
    WORD32 pos_x_y_4x4_0;

    WORD32 pos_x = pos_x_y_4x4 & 0xFFFF;
    WORD32 pos_y = (pos_x_y_4x4 >> 16) & 0xFFFF;

    assert(pos_x >= 0);
    assert(pos_y >= 0);

    /* Initialize pointers to  store 16x16 SATDs */
    pi4_16x16_hsad = ppi4_hsad[HAD_16x16] + (pos_x >> 2) + (pos_y >> 2) * (num_4x4_in_row >> 2);

    pi4_16x16_tu_split =
        ppi4_tu_split[HAD_16x16] + (pos_x >> 2) + (pos_y >> 2) * (num_4x4_in_row >> 2);

    pi4_16x16_tu_early_cbf =
        ppi4_tu_early_cbf[HAD_16x16] + (pos_x >> 2) + (pos_y >> 2) * (num_4x4_in_row >> 2);

    /* -------- Compute four 8x8 HAD Transforms of 16x16 call--------- */
    for(i = 0; i < 4; i++)
    {
        src = pu1_src + (i & 0x01) * 8 + (i >> 1) * src_strd * 8;
        pred = pu1_pred + (i & 0x01) * 8 + (i >> 1) * pred_strd * 8;
        pi2_y0 = ai2_8x8_had + (i & 0x01) * 8 + (i >> 1) * 16 * 8;
        pos_x_y_4x4_0 = pos_x_y_4x4 + (i & 0x01) * 2 + (i >> 1) * (2 << 16);

        best_cost_tu_split = ihevce_had_8x8_using_4_4x4_r_neon(
            src,
            src_strd,
            pred,
            pred_strd,
            pi2_y0,
            16,
            ppi4_hsad,
            ppi4_tu_split,
            ppi4_tu_early_cbf,
            pos_x_y_4x4_0,
            num_4x4_in_row,
            lambda,
            lambda_q_shift,
            i4_frm_qstep,
            i4_cur_depth + 1,
            i4_max_depth,
            i4_max_tr_size,
            pi4_tu_split_cost,
            pv_func_sel);

        /* Cost is shifted by two bits for Tu_split_flag and early cbf flag */
        best_cost = (best_cost_tu_split >> 2);

        /* Last but one bit stores the information regarding the TU_Split */
        tu_split_flag += (best_cost_tu_split & 0x3) >> 1;

        /* Last bit stores the information regarding the early_cbf */
        i4_early_cbf_flag += (best_cost_tu_split & 0x1);

        cost_child += best_cost;

        tu_split_flag <<= 1;
        i4_early_cbf_flag <<= 1;
    }

    /* -------- Compute 16x16 HAD Transform using 8x8 results ------------- */
    pi2_y0 = ai2_8x8_had;

    /* Threshold currently passed as "0" */
    cost_parent = ihevce_compute_16x16HAD_using_8x8_neon(
        pi2_y0, 16, pi2_dst, dst_strd, i4_frm_qstep, &early_cbf);

    /* 4 TU_Split flags , 4 CBF Flags, extra 1 becoz of the 0.5 bits per bin is assumed */
    cost_child += ((4 + 4) * lambda) >> (lambda_q_shift + 1);

    i4_early_cbf_flag += early_cbf;

    /* Right now the depth is hard-coded to 4: The depth can be modified from the config file
    which decides the extent to which TU_REC needs to be done */
    if(i4_cur_depth < i4_max_depth)
    {
        if((cost_child < cost_parent) || (i4_max_tr_size < u1_cur_tr_size))
        {
            *pi4_tu_split_cost += ((4 + 4) * lambda) >> (lambda_q_shift + 1);
            tu_split_flag += 1;
            best_cost = cost_child;
        }
        else
        {
            tu_split_flag += 0;
            best_cost = cost_parent;
        }
    }
    else
    {
        tu_split_flag += 0;
        best_cost = cost_parent;
    }

    pi4_16x16_hsad[0] = best_cost;
    pi4_16x16_tu_split[0] = tu_split_flag;
    pi4_16x16_tu_early_cbf[0] = i4_early_cbf_flag;

    /*returning two values(best cost & tu_split_flag) as a single value*/
    return ((best_cost << 10) + (tu_split_flag << 5) + i4_early_cbf_flag);
}

UWORD32 ihevce_compute_32x32HAD_using_16x16_neon(
    WORD16 *pi2_16x16_had,
    WORD32 had16_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf)
{
    int16x8_t a[4][4][8];
    uint32x4_t sum = vdupq_n_u32(0);
    const int16x8_t threshold = vdupq_n_s16((int16_t)(i4_frm_qstep >> 8));
    WORD32 b8, b16;
    uint64x2_t c;
    WORD32 i, j;

    (void)pi2_dst;
    (void)dst_strd;

    for(b16 = 0; b16 < 4; b16++)
    {
        WORD16 *pi2_b16 = pi2_16x16_had + (b16 >> 1) * (had16_strd * 16) + ((b16 & 1) * 16);

        for(b8 = 0; b8 < 4; b8++)
        {
            WORD16 *pi2_b8 = pi2_b16 + (b8 >> 1) * (had16_strd * 8) + ((b8 & 1) * 8);

            for(i = 0; i < 8; i++, pi2_b8 += had16_strd)
            {
                a[b16][b8][i] = vld1q_s16(pi2_b8);
                a[b16][b8][i] = vshrq_n_s16(a[b16][b8][i], 2);
            }
        }
    }

    for(j = 0; j < 4; j++)
    {
        for(i = 0; i < 8; i++)
        {
            int16x8_t p0 = vaddq_s16(a[0][j][i], a[1][j][i]);
            int16x8_t p1 = vsubq_s16(a[0][j][i], a[1][j][i]);
            int16x8_t p2 = vaddq_s16(a[2][j][i], a[3][j][i]);
            int16x8_t p3 = vsubq_s16(a[2][j][i], a[3][j][i]);

            int16x8_t q0 = vaddq_s16(p0, p2);
            int16x8_t q1 = vsubq_s16(p0, p2);
            int16x8_t q2 = vaddq_s16(p1, p3);
            int16x8_t q3 = vsubq_s16(p1, p3);

            EARLY_EXIT(0);
            EARLY_EXIT(1);
            EARLY_EXIT(2);
            EARLY_EXIT(3);

            uint16x8_t r0 = vaddq_u16(vreinterpretq_u16_s16(p0), vreinterpretq_u16_s16(p1));
            uint16x8_t r1 = vaddq_u16(vreinterpretq_u16_s16(p2), vreinterpretq_u16_s16(p3));

            uint32x4_t s0 = vaddl_u16(vget_low_u16(r0), vget_high_u16(r0));
            uint32x4_t s1 = vaddl_u16(vget_low_u16(r1), vget_high_u16(r1));

            sum = vaddq_u32(sum, s0);
            sum = vaddq_u32(sum, s1);
        }
    }
    c = vpaddlq_u32(sum);

    return vget_lane_u64(vadd_u64(vget_low_u64(c), vget_high_u64(c)), 0);
}
