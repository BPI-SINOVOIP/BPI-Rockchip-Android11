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
 *  ihevc_resi_trans_neon.c
 *
 * @brief
 *  Contains definitions of functions for computing residue and fwd transform
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *  - ihevc_resi_trans_4x4_neon()
 *  - ihevc_resi_trans_4x4_ttype1_neon()
 *  - ihevc_resi_trans_8x8_neon()
 *  - ihevc_resi_trans_16x16_neon()
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

/* System user files */
#include "ihevc_typedefs.h"
#include "ihevc_macros.h"
#include "ihevc_defs.h"
#include "ihevc_cmn_utils_neon.h"

#include "ihevc_trans_tables.h"
#include "ihevc_resi_trans.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
UWORD32 ihevc_resi_trans_4x4_neon(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 *pi4_temp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    UWORD32 sad;
    uint8x16_t inp_buf, pred_buf;
    int16x8_t diff_1, diff_2;
    int16x4_t diff_1_low, diff_1_high, diff_2_low, diff_2_high;
    int16x8_t e_01, o_32;
    int16x4_t e_0, e_1, o_0, o_1;
    int32x4_t e_0_a_e_1, e_0_s_e_1;
    int32x4_t temp1, temp2, temp3, temp4;
    int32x4_t o_1_m_trans_10, o_1_m_trans_11;
    int32x4_t e_03, e_12, o_03, o_12;
    int16x4_t out_0, out_1, out_2, out_3;
    uint16x8_t abs;
    uint32x4_t b;
    uint64x2_t c;

    (void)pi4_temp;
    if(e_chroma_plane == NULL_PLANE)
    {
        inp_buf = load_unaligned_u8q(pu1_src, src_strd);
        pred_buf = load_unaligned_u8q(pu1_pred, pred_strd);
    }
    else
    {
        inp_buf = load_unaligned_u8qi(pu1_src + e_chroma_plane, src_strd);
        pred_buf = load_unaligned_u8qi(pu1_pred + e_chroma_plane, pred_strd);
    }

    abs = vabdl_u8(vget_low_u8(inp_buf), vget_low_u8(pred_buf));
    abs = vabal_u8(abs, vget_high_u8(inp_buf), vget_high_u8(pred_buf));
    b = vpaddlq_u16(abs);
    c = vpaddlq_u32(b);
    sad = vget_lane_u32(vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                                 vreinterpret_u32_u64(vget_high_u64(c))),
            0);

    diff_1 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(inp_buf), vget_low_u8(pred_buf)));
    diff_2 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(inp_buf), vget_high_u8(pred_buf)));

    diff_1_low = vget_low_s16(diff_1);
    diff_1_high = vget_high_s16(diff_1);
    diff_2_low = vget_low_s16(diff_2);
    diff_2_high = vget_high_s16(diff_2);

    transpose_s16_4x4d(&diff_1_low, &diff_1_high, &diff_2_low, &diff_2_high);
    diff_1 = vcombine_s16(diff_1_low, diff_1_high);
    diff_2 = vcombine_s16(diff_2_high, diff_2_low);

    e_01 = vaddq_s16(diff_1, diff_2);
    o_32 = vsubq_s16(diff_1, diff_2);

    e_0 = vget_low_s16(e_01);
    e_1 = vget_high_s16(e_01);
    o_0 = vget_high_s16(o_32);
    o_1 = vget_low_s16(o_32);

    e_0_a_e_1 = vaddl_s16(e_0, e_1);
    e_0_s_e_1 = vsubl_s16(e_0, e_1);

    temp1 = vmulq_n_s32(e_0_a_e_1, (WORD32)g_ai2_ihevc_trans_4[0][0]);
    temp2 = vmulq_n_s32(e_0_s_e_1, (WORD32)g_ai2_ihevc_trans_4[0][0]);

    o_1_m_trans_10 = vmull_n_s16(o_1, (WORD32)g_ai2_ihevc_trans_4[1][0]);
    o_1_m_trans_11 = vmull_n_s16(o_1, (WORD32)g_ai2_ihevc_trans_4[1][1]);

    temp3 = vmlal_n_s16(o_1_m_trans_10, o_0, (WORD32)g_ai2_ihevc_trans_4[1][1]);
    temp4 = vmlsl_n_s16(o_1_m_trans_11, o_0, (WORD32)g_ai2_ihevc_trans_4[1][0]);

    transpose_s32_4x4(&temp1, &temp3, &temp2, &temp4);

    e_03 = vaddq_s32(temp1, temp4);
    e_12 = vaddq_s32(temp3, temp2);
    o_03 = vsubq_s32(temp1, temp4);
    o_12 = vsubq_s32(temp3, temp2);

    e_0_a_e_1 = vaddq_s32(e_03, e_12);
    e_0_s_e_1 = vsubq_s32(e_03, e_12);

    temp1 = vmulq_n_s32(e_0_a_e_1, (WORD32)g_ai2_ihevc_trans_4[0][0]);
    temp2 = vmulq_n_s32(e_0_s_e_1, (WORD32)g_ai2_ihevc_trans_4[0][0]);

    o_1_m_trans_10 = vmulq_n_s32(o_03, (WORD32)g_ai2_ihevc_trans_4[1][0]);
    o_1_m_trans_11 = vmulq_n_s32(o_03, (WORD32)g_ai2_ihevc_trans_4[1][1]);

    temp3 = vmlaq_n_s32(o_1_m_trans_10, o_12, (WORD32)g_ai2_ihevc_trans_4[1][1]);
    temp4 = vmlsq_n_s32(o_1_m_trans_11, o_12, (WORD32)g_ai2_ihevc_trans_4[1][0]);

    out_0 = vrshrn_n_s32(temp1, 9);
    out_1 = vrshrn_n_s32(temp3, 9);
    out_2 = vrshrn_n_s32(temp2, 9);
    out_3 = vrshrn_n_s32(temp4, 9);

    vst1_s16(pi2_dst, out_0);
    vst1_s16(pi2_dst + dst_strd, out_1);
    vst1_s16(pi2_dst + 2 * dst_strd, out_2);
    vst1_s16(pi2_dst + 3 * dst_strd, out_3);

    return sad;
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs residue calculation and forward  transform type 1
 *  on input pixels
 *
 * @par Description:
 *  Performs residue calculation by subtracting source and  prediction and
 *  followed by forward transform
 *
 * @param[in] pu1_src
 *  Input 4x4 pixels
 *
 * @param[in] pu1_pred
 *  Prediction data
 *
 * @param[in] pi2_tmp
 *  Temporary buffer of size 4x4
 *
 * @param[out] pi2_dst
 *  Output 4x4 coefficients
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] pred_strd
 *  Prediction Stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[in] e_chroma_plane
 *  Enum singalling chroma plane
 *
 * @returns  block sad
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
UWORD32 ihevc_resi_trans_4x4_ttype1_neon(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 *pi4_temp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    UWORD32 sad;
    int16x4_t src0_4x16b;
    int16x4_t src1_4x16b;
    int16x4_t src2_4x16b;
    int16x4_t src3_4x16b;
    int32x4_t src0_4x32b;
    int32x4_t src1_4x32b;
    int32x4_t src2_4x32b;
    int32x4_t src3_4x32b;
    /*load source and pred values */
    const uint8x16_t src_u8 = load_unaligned_u8q(pu1_src, src_strd);
    const uint8x16_t pred_u8 = load_unaligned_u8q(pu1_pred, pred_strd);

    const int16x8_t src_reg0 =
        vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(src_u8), vget_low_u8(pred_u8)));
    const int16x8_t src_reg1 =
        vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(src_u8), vget_high_u8(pred_u8)));

    int32x4_t add_val = vdupq_n_s32(1);

    uint16x8_t abs = vabdl_u8(vget_low_u8(src_u8), vget_low_u8(pred_u8));
    uint32x4_t b;
    uint64x2_t c;
    UNUSED(e_chroma_plane);
    abs = vabal_u8(abs, vget_high_u8(src_u8), vget_high_u8(pred_u8));
    b = vpaddlq_u16(abs);
    c = vpaddlq_u32(b);
    sad = vget_lane_u32(vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)),
                                 vreinterpret_u32_u64(vget_high_u64(c))),
            0);

    (void)pi4_temp;

    /*************************    4x4 16bit Transpose  ***********************/
    src0_4x16b = vget_low_s16(src_reg0);
    src1_4x16b = vget_high_s16(src_reg0);
    src2_4x16b = vget_low_s16(src_reg1);
    src3_4x16b = vget_high_s16(src_reg1);

    transpose_s16_4x4d(&src0_4x16b, &src1_4x16b, &src2_4x16b, &src3_4x16b);

    /**************************  4x4 Transpose End   *************************/

    /* Residue + Forward Transform 1st stage */
    /* coeff2_4x32b = 74 74 74 74 */
    const int32x4_t coeff2_4x32b =
        vdupq_n_s32(74);  //vld1q_s32(&g_ai4_ihevc_trans_dst_intr_4[2][0]);
    /* coeff0_4x32b = 29 29 29 29 */
    const int32x4_t coeff0_4x32b =
        vdupq_n_s32(29);  //vld1q_s32(&g_ai4_ihevc_trans_dst_intr_4[0][0]);
    /* coeff1_4x32b = 55 55 55 55 */
    const int32x4_t coeff1_4x32b =
        vdupq_n_s32(55);  //vld1q_s32(&g_ai4_ihevc_trans_dst_intr_4[1][0]);

    /* c0 to c3 calculation */
    int32x4_t c0_4x32b = vaddl_s16(src0_4x16b, src3_4x16b); /* r0+r3 */
    int32x4_t c1_4x32b = vaddl_s16(src1_4x16b, src3_4x16b); /* r1+r3 */
    int32x4_t c2_4x32b = vsubl_s16(src0_4x16b, src1_4x16b); /* r0-r1 */
    int32x4_t c3_4x32b = vmulq_s32(vmovl_s16(src2_4x16b), coeff2_4x32b); /* 74*r2 */
    src0_4x16b = vadd_s16(src0_4x16b, src1_4x16b); /* r0+r1 */

    src1_4x32b = vsubl_s16(src0_4x16b, src3_4x16b); /* r0+r1-r3 */
    src0_4x32b = vmlaq_s32(c3_4x32b, c0_4x32b, coeff0_4x32b); /* 29*c0 + c3 */
    src2_4x32b = vmulq_s32(c2_4x32b, coeff0_4x32b); /* 29*c2 - c3 */
    src3_4x32b = vmlaq_s32(c3_4x32b, c2_4x32b, coeff1_4x32b); /* 55*c2 + c3 */
    src2_4x32b = vsubq_s32(src2_4x32b, c3_4x32b);

    src0_4x32b = vmlaq_s32(src0_4x32b, c1_4x32b, coeff1_4x32b); /* 29*c0 + 55*c1 + c3 */
    src2_4x32b = vmlaq_s32(src2_4x32b, c0_4x32b, coeff1_4x32b); /* 29*c2 + 55*c0 - c3 */
    c1_4x32b = vmulq_s32(c1_4x32b, coeff0_4x32b); /* 55*c2 - 29*c1 + c3 */
    src1_4x32b = vmulq_s32(src1_4x32b, coeff2_4x32b); /*74*(r0+r1-r3)*/
    src3_4x32b = vsubq_s32(src3_4x32b, c1_4x32b);

    /* result + add */
    src1_4x32b = vaddq_s32(src1_4x32b, add_val);
    src0_4x32b = vaddq_s32(src0_4x32b, add_val);
    src2_4x32b = vaddq_s32(src2_4x32b, add_val);
    src3_4x32b = vaddq_s32(src3_4x32b, add_val);
    /* result >> shift */
    src1_4x32b = vshrq_n_s32(src1_4x32b, 1);
    src0_4x32b = vshrq_n_s32(src0_4x32b, 1);
    src2_4x32b = vshrq_n_s32(src2_4x32b, 1);
    src3_4x32b = vshrq_n_s32(src3_4x32b, 1);
    /* Forward transform 2nd stage */
    {
        /*************************    4x4 32bit Transpose  ***********************/

        transpose_s32_4x4(&src0_4x32b, &src1_4x32b, &src2_4x32b, &src3_4x32b);

        /**************************  4x4 Transpose End   *************************/

        /* add value */
        add_val = vdupq_n_s32(128);
        c0_4x32b = vaddq_s32(src0_4x32b, src3_4x32b); /* r0+r3 */
        c1_4x32b = vaddq_s32(src1_4x32b, src3_4x32b); /* r1+r3 */
        c2_4x32b = vsubq_s32(src0_4x32b, src1_4x32b); /* r0-r1 */
        c3_4x32b = vmulq_s32(src2_4x32b, coeff2_4x32b); /* 74*r2 */
        src1_4x32b = vaddq_s32(src0_4x32b, src1_4x32b); /* r0+r1 */

        src1_4x32b = vsubq_s32(src1_4x32b, src3_4x32b); /* r0+r1-r3 */
        src0_4x32b = vmlaq_s32(c3_4x32b, c0_4x32b, coeff0_4x32b); /* 29*c0 + c3 */
        src2_4x32b = vmulq_s32(c2_4x32b, coeff0_4x32b); /* 29*c2 - c3 */
        src3_4x32b = vmlaq_s32(c3_4x32b, c2_4x32b, coeff1_4x32b); /* 55*c2 + c3 */
        src2_4x32b = vsubq_s32(src2_4x32b, c3_4x32b);

        src0_4x32b = vmlaq_s32(src0_4x32b, c1_4x32b, coeff1_4x32b); /* 29*c0 + 55*c1 + c3 */
        src2_4x32b = vmlaq_s32(src2_4x32b, c0_4x32b, coeff1_4x32b); /* 29*c2 + 55*c0 - c3 */
        c1_4x32b = vmulq_s32(c1_4x32b, coeff0_4x32b); /* 55*c2 - 29*c1 + c3 */
        src1_4x32b = vmulq_s32(src1_4x32b, coeff2_4x32b); /*74*(r0+r1-r3)*/
        src3_4x32b = vsubq_s32(src3_4x32b, c1_4x32b);

        /* result + add */
        src1_4x32b = vaddq_s32(src1_4x32b, add_val);
        src0_4x32b = vaddq_s32(src0_4x32b, add_val);
        src2_4x32b = vaddq_s32(src2_4x32b, add_val);
        src3_4x32b = vaddq_s32(src3_4x32b, add_val);

        src1_4x32b = vshrq_n_s32(src1_4x32b, 8);
        src0_4x32b = vshrq_n_s32(src0_4x32b, 8);
        src2_4x32b = vshrq_n_s32(src2_4x32b, 8);
        src3_4x32b = vshrq_n_s32(src3_4x32b, 8);

        vst1_s16((pi2_dst + dst_strd), vmovn_s32(src1_4x32b));
        vst1_s16(pi2_dst, vmovn_s32(src0_4x32b));
        vst1_s16((pi2_dst + 2 * dst_strd), vmovn_s32(src2_4x32b));
        vst1_s16((pi2_dst + 3 * dst_strd), vmovn_s32(src3_4x32b));
    }
    return sad;
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs residue calculation and forward  transform on
 * input pixels
 *
 * @par Description:
 *  Performs residue calculation by subtracting source and  prediction and
 * followed by forward transform
 *
 * @param[in] pu1_src
 *  Input 8x8 pixels
 *
 * @param[in] pu1_pred
 *  Prediction data
 *
 * @param[in] pi2_tmp
 *  Temporary buffer of size 8x8
 *
 * @param[out] pi2_dst
 *  Output 8x8 coefficients
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] pred_strd
 *  Prediction Stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[in] e_chroma_plane
 *  Enum singalling chroma plane
 *
 * @returns  Void
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
UWORD32 ihevc_resi_trans_8x8_neon(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 *pi4_temp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    int16x8_t diff_16[8];
    int16x8_t abs = vdupq_n_s16(0);
    int32x4_t tmp_a;
    int64x2_t tmp_b;
    int32x2_t sad_v;
    int32x4x2_t a0, a1, a2, a3, a4, a5, a6, a7;
    UWORD32 sad;

    (void)pi4_temp;
#define RESIDUE(k)                                                                                 \
    if(NULL_PLANE == e_chroma_plane)                                                               \
    {                                                                                              \
        const uint8x8_t s##k = vld1_u8(pu1_src);                                                   \
        const uint8x8_t p##k = vld1_u8(pu1_pred);                                                  \
        diff_16[k] = vreinterpretq_s16_u16(vsubl_u8(s##k, p##k));                                  \
        pu1_src += src_strd;                                                                       \
        pu1_pred += pred_strd;                                                                     \
        abs = vaddq_s16(abs, vabsq_s16(diff_16[k]));                                               \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
        const uint8x8_t s##k = vld2_u8(pu1_src).val[e_chroma_plane];                               \
        const uint8x8_t p##k = vld2_u8(pu1_pred).val[e_chroma_plane];                              \
        diff_16[k] = vreinterpretq_s16_u16(vsubl_u8(s##k, p##k));                                  \
        pu1_src += src_strd;                                                                       \
        pu1_pred += pred_strd;                                                                     \
        abs = vaddq_s16(abs, vabsq_s16(diff_16[k]));                                               \
    }

    // stage 1
    RESIDUE(0);
    RESIDUE(1);
    RESIDUE(2);
    RESIDUE(3);
    RESIDUE(4);
    RESIDUE(5);
    RESIDUE(6);
    RESIDUE(7);

    tmp_a = vpaddlq_s16(abs);
    tmp_b = vpaddlq_s32(tmp_a);
    sad_v = vadd_s32(vreinterpret_s32_s64(vget_low_s64(tmp_b)),
                   vreinterpret_s32_s64(vget_high_s64(tmp_b)));
    sad = vget_lane_s32(sad_v, 0);

    transpose_s16_8x8(
        &diff_16[0],
        &diff_16[1],
        &diff_16[2],
        &diff_16[3],
        &diff_16[4],
        &diff_16[5],
        &diff_16[6],
        &diff_16[7]);

    {
        const int16x8_t o3 = vsubq_s16(diff_16[3], diff_16[4]); /*C3 - C4*/
        const int16x8_t o2 = vsubq_s16(diff_16[2], diff_16[5]); /*C2 - C5*/
        const int16x8_t o1 = vsubq_s16(diff_16[1], diff_16[6]); /*C1 - C6*/
        const int16x8_t o0 = vsubq_s16(diff_16[0], diff_16[7]); /*C0 - C7*/
        const int16x8_t e0 = vaddq_s16(diff_16[0], diff_16[7]); /*C0 + C7*/
        const int16x8_t e1 = vaddq_s16(diff_16[1], diff_16[6]); /*C1 + C6*/
        const int16x8_t e2 = vaddq_s16(diff_16[2], diff_16[5]); /*C2 + C5*/
        const int16x8_t e3 = vaddq_s16(diff_16[3], diff_16[4]); /*C3 + C4*/

        const int16x8_t ee0 = vaddq_s16(e0, e3); /*C0 + C3 + C4 + C7*/
        const int16x8_t ee1 = vaddq_s16(e1, e2); /*C1 + C2 + C5 + C6*/
        const int16x8_t eo0 = vsubq_s16(e0, e3); /*C0 - C3 - C4 + C7*/
        const int16x8_t eo1 = vsubq_s16(e1, e2); /*C1 - C2 - C5 + C6*/

        /*C0 + C1 + C2 + C3 + C4 + C5 + C6 + C7*/
        const int16x8_t eee = vaddq_s16(ee1, ee0);
        /*C0 - C1 - C2 + C3 + C4 - C5 - C6 + C7*/
        const int16x8_t eeo = vsubq_s16(ee0, ee1);

        /*F2[0] of 83*(C0 - C3 - C4 + C7)*/
        a2.val[0] = vmull_n_s16(vget_low_s16(eo0), 83);
        /*F6[0] of 36*(C0 - C3 - C4 + C7)*/
        a6.val[0] = vmull_n_s16(vget_low_s16(eo0), 36);
        /*F2[1] of 83*(C0 - C3 - C4 + C7)*/
        a2.val[1] = vmull_n_s16(vget_high_s16(eo0), 83);
        /*F6[1] of 36*(C0 - C3 - C4 + C7)*/
        a6.val[1] = vmull_n_s16(vget_high_s16(eo0), 36);

        /*F6[1] = 36*(C0 - C3 - C4 + C7) - 83*(C1 - C2 - C5 + C6)*/
        a6.val[1] = vmlsl_n_s16(a6.val[1], vget_high_s16(eo1), 83);
        /*F2[1] = 83*(C0 - C3 - C4 + C7) + 36*(C1 - C2 - C5 + C6)*/
        a2.val[1] = vmlal_n_s16(a2.val[1], vget_high_s16(eo1), 36);
        /*F6[0] = 36*(C0 - C3 - C4 + C7) - 83*(C1 - C2 - C5 + C6)*/
        a6.val[0] = vmlsl_n_s16(a6.val[0], vget_low_s16(eo1), 83);
        /*F2[0] = 83*(C0 - C3 - C4 + C7) + 36*(C1 - C2 - C5 + C6)*/
        a2.val[0] = vmlal_n_s16(a2.val[0], vget_low_s16(eo1), 36);

        /*F0[0] = 64*(C0 + C1 + C2 + C3 + C4 + C5 + C6 + C7)*/
        a0.val[0] = vshll_n_s16(vget_low_s16(eee), 6);
        /*F0[1] = 64*(C0 + C1 + C2 + C3 + C4 + C5 + C6 + C7)*/
        a0.val[1] = vshll_n_s16(vget_high_s16(eee), 6);
        /*F4[0] = 64*(C0 - C1 - C2 + C3 + C4 - C5 - C6 + C7)*/
        a4.val[0] = vshll_n_s16(vget_low_s16(eeo), 6);
        /*F4[1] = 64*(C0 - C1 - C2 + C3 + C4 - C5 - C6 + C7)*/
        a4.val[1] = vshll_n_s16(vget_high_s16(eeo), 6);

        a7.val[0] = vmull_n_s16(vget_low_s16(o0), 18); /*F7[0] = 18*(C0 - C7)*/
        a5.val[0] = vmull_n_s16(vget_low_s16(o0), 50); /*F5[0] = 50*(C0 - C7)*/
        a3.val[0] = vmull_n_s16(vget_low_s16(o0), 75); /*F3[0] = 75*(C0 - C7)*/
        a1.val[0] = vmull_n_s16(vget_low_s16(o0), 89); /*F1[0] = 89*(C0 - C7)*/
        a1.val[1] = vmull_n_s16(vget_high_s16(o0), 89); /*F1[1] = 89*(C0 - C7)*/
        a3.val[1] = vmull_n_s16(vget_high_s16(o0), 75); /*F3[1] = 75*(C0 - C7)*/
        a5.val[1] = vmull_n_s16(vget_high_s16(o0), 50); /*F5[1] = 50*(C0 - C7)*/
        a7.val[1] = vmull_n_s16(vget_high_s16(o0), 18); /*F7[1] = 18*(C0 - C7)*/

        /*F7[0] = 18*(C0 - C7) - 50*(C1 - C6)*/
        a7.val[0] = vmlsl_n_s16(a7.val[0], vget_low_s16(o1), 50);
        /*F5[0] = 50*(C0 - C7) - 89*(C1 - C6)*/
        a5.val[0] = vmlsl_n_s16(a5.val[0], vget_low_s16(o1), 89);
        /*F3[0] = 75*(C0 - C7) - 18*(C1 - C6)*/
        a3.val[0] = vmlsl_n_s16(a3.val[0], vget_low_s16(o1), 18);
        /*F1[0] = 89*(C0 - C7) + 75*(C1 - C6)*/
        a1.val[0] = vmlal_n_s16(a1.val[0], vget_low_s16(o1), 75);
        /*F1[1] = 89*(C0 - C7) + 75*(C1 - C6)*/
        a1.val[1] = vmlal_n_s16(a1.val[1], vget_high_s16(o1), 75);
        /*F3[1] = 75*(C0 - C7) - 18*(C1 - C6)*/
        a3.val[1] = vmlsl_n_s16(a3.val[1], vget_high_s16(o1), 18);
        /*F5[1] = 50*(C0 - C7) - 89*(C1 - C6)*/
        a5.val[1] = vmlsl_n_s16(a5.val[1], vget_high_s16(o1), 89);
        /*F7[1] = 18*(C0 - C7) - 50*(C1 - C6)*/
        a7.val[1] = vmlsl_n_s16(a7.val[1], vget_high_s16(o1), 50);

        /*F7[0] = 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5)*/
        a7.val[0] = vmlal_n_s16(a7.val[0], vget_low_s16(o2), 75);
        /*F5[0] = 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5)*/
        a5.val[0] = vmlal_n_s16(a5.val[0], vget_low_s16(o2), 18);
        /*F3[0] = 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5)*/
        a3.val[0] = vmlsl_n_s16(a3.val[0], vget_low_s16(o2), 89);
        /*F1[0] = 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5)*/
        a1.val[0] = vmlal_n_s16(a1.val[0], vget_low_s16(o2), 50);
        /*F1[1] = 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5)*/
        a1.val[1] = vmlal_n_s16(a1.val[1], vget_high_s16(o2), 50);
        /*F3[1] = 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5)*/
        a3.val[1] = vmlsl_n_s16(a3.val[1], vget_high_s16(o2), 89);
        /*F5[1] = 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5)*/
        a5.val[1] = vmlal_n_s16(a5.val[1], vget_high_s16(o2), 18);
        /*F7[1] = 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5)*/
        a7.val[1] = vmlal_n_s16(a7.val[1], vget_high_s16(o2), 75);

        /*F7[0] = 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5) - 89*(C3 - C4)*/
        a7.val[0] = vmlsl_n_s16(a7.val[0], vget_low_s16(o3), 89);
        /*F5[0] = 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5) + 75*(C3 - C4)*/
        a5.val[0] = vmlal_n_s16(a5.val[0], vget_low_s16(o3), 75);
        /*F3[0] = 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5) - 50*(C3 - C4)*/
        a3.val[0] = vmlsl_n_s16(a3.val[0], vget_low_s16(o3), 50);
        /*F1[0] = 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5) + 18*(C3 - C4)*/
        a1.val[0] = vmlal_n_s16(a1.val[0], vget_low_s16(o3), 18);
        /*F1[1] = 89*(C0 - C7) + 75*(C1 - C6) + 50*(C2 - C5) + 18*(C3 - C4)*/
        a1.val[1] = vmlal_n_s16(a1.val[1], vget_high_s16(o3), 18);
        /*F3[1] = 75*(C0 - C7) - 18*(C1 - C6) - 89*(C2 - C5) - 50*(C3 - C4)*/
        a3.val[1] = vmlsl_n_s16(a3.val[1], vget_high_s16(o3), 50);
        /*F5[1] = 50*(C0 - C7) - 89*(C1 - C6) + 18*(C2 - C5) + 75*(C3 - C4)*/
        a5.val[1] = vmlal_n_s16(a5.val[1], vget_high_s16(o3), 75);
        /*F7[1] = 18*(C0 - C7) - 50*(C1 - C6) + 75*(C2 - C5) - 89*(C3 - C4)*/
        a7.val[1] = vmlsl_n_s16(a7.val[1], vget_high_s16(o3), 89);
    }

    //Stage 2
    {
        int32x4_t h0, h1, h2, h3, h4, h5, h6, h7;
        int32x4_t e0_2, e1_2, e2_2, e3_2;
        int32x4_t o0_2, o1_2, o2_2, o3_2;
        int32x4_t ee1_2, eo1_2, eo0_2, ee0_2;
        int16x4_t row0, row1, row2, row3, row4, row5, row6, row7;

        /*Transposing second half of transform stage 1 (1)*/
        int32x4x2_t b1 = vtrnq_s32(a0.val[1], a1.val[1]);
        int32x4x2_t b3 = vtrnq_s32(a2.val[1], a3.val[1]);
        int32x4x2_t b0 = vtrnq_s32(a0.val[0], a1.val[0]);
        int32x4x2_t b2 = vtrnq_s32(a2.val[0], a3.val[0]);

        /*Transposing second half of transform stage 1 (2)*/
        a0.val[0] = vcombine_s32(vget_low_s32(b0.val[0]), vget_low_s32(b2.val[0]));
        a2.val[0] = vcombine_s32(vget_high_s32(b0.val[0]), vget_high_s32(b2.val[0]));
        a1.val[0] = vcombine_s32(vget_low_s32(b0.val[1]), vget_low_s32(b2.val[1]));
        a3.val[0] = vcombine_s32(vget_high_s32(b0.val[1]), vget_high_s32(b2.val[1]));
        a0.val[1] = vcombine_s32(vget_low_s32(b1.val[0]), vget_low_s32(b3.val[0]));
        a2.val[1] = vcombine_s32(vget_high_s32(b1.val[0]), vget_high_s32(b3.val[0]));
        a1.val[1] = vcombine_s32(vget_low_s32(b1.val[1]), vget_low_s32(b3.val[1]));
        a3.val[1] = vcombine_s32(vget_high_s32(b1.val[1]), vget_high_s32(b3.val[1]));

        o0_2 = vsubq_s32(a0.val[0], a3.val[1]); /*B0 - B7*/
        o1_2 = vsubq_s32(a1.val[0], a2.val[1]); /*B1 - B6*/
        o2_2 = vsubq_s32(a2.val[0], a1.val[1]); /*B2 - B5*/
        o3_2 = vsubq_s32(a3.val[0], a0.val[1]); /*B3 - B4*/
        e3_2 = vaddq_s32(a3.val[0], a0.val[1]); /*B3 + B4*/
        e2_2 = vaddq_s32(a2.val[0], a1.val[1]); /*B2 + B5*/
        e1_2 = vaddq_s32(a1.val[0], a2.val[1]); /*B1 + B6*/
        e0_2 = vaddq_s32(a0.val[0], a3.val[1]); /*B0 + B7*/

        eo1_2 = vsubq_s32(e1_2, e2_2); /*B1 - B2 - B5 + B6*/
        ee1_2 = vaddq_s32(e1_2, e2_2); /*B1 + B2 + B5 + B6*/
        eo0_2 = vsubq_s32(e0_2, e3_2); /*B0 - B3 - B4 + B7*/
        ee0_2 = vaddq_s32(e0_2, e3_2); /*B0 + B3 + B4 + B7*/

        /* F4 = B0 - B1 - B2 + B3 + B4 - B5 - B6 + B7*/
        h4 = vsubq_s32(ee0_2, ee1_2);
        /* F0 = B0 + B1 + B2 + B3 + B4 + B5 + B6 + B7*/
        h0 = vaddq_s32(ee0_2, ee1_2);
        /* Truncating last 11 bits in H0*/
        row0 = vrshrn_n_s32(h0, 5);
        /*First half-row of row 1 of transform stage 2 (H0) stored*/
        vst1_s16(pi2_dst, row0);
        /* Truncating last 11 bits in H4*/
        row4 = vrshrn_n_s32(h4, 5);
        /*First half-row of row 5 of transform stage 2 (H4) stored*/
        vst1_s16(pi2_dst + 4 * dst_strd, row4);

        /* F6 = 36*(B0 - B3 - B4 + B7) */
        h6 = vmulq_n_s32(eo0_2, 36);
        /* F2 = 83*(B0 - B3 - B4 + B7) */
        h2 = vmulq_n_s32(eo0_2, 83);
        /*H2 = 83*(B0 - B3 - B4 + B7) + 36*(B1 - B2 - B5 + B6)*/
        h2 = vmlaq_n_s32(h2, eo1_2, 36);
        /*H6 = 36*(B0 - B3 - B4 + B7) - 83*(B1 - B2 - B5 + B6)*/
        h6 = vmlsq_n_s32(h6, eo1_2, 83);
        /* Truncating last 11 bits in H6*/
        row6 = vrshrn_n_s32(h6, 11);
        /*First half-row of row 7 of transform stage 2 (H6) stored*/
        vst1_s16(pi2_dst + 6 * dst_strd, row6);
        /* Truncating last 11 bits in H2*/
        row2 = vrshrn_n_s32(h2, 11);
        /*First half-row of row 3 of transform stage 2 (H2) stored*/
        vst1_s16(pi2_dst + 2 * dst_strd, row2);

        h1 = vmulq_n_s32(o0_2, 89); /* H1 = 89*(B0 - B7) */
        h3 = vmulq_n_s32(o0_2, 75); /* H3 = 75*(B0 - B7) */
        h5 = vmulq_n_s32(o0_2, 50); /* H5 = 50*(B0 - B7) */
        h7 = vmulq_n_s32(o0_2, 18); /* H7 = 18*(B0 - B7) */

        h7 = vmlsq_n_s32(h7, o1_2, 50); /* H7 = 18*(B0 - B7) - 50*(B1 - B6) */
        h5 = vmlsq_n_s32(h5, o1_2, 89); /* H5 = 50*(B0 - B7) - 89*(B1 - B6) */
        h3 = vmlsq_n_s32(h3, o1_2, 18); /* H3 = 75*(B0 - B7) - 18*(B1 - B6) */
        h1 = vmlaq_n_s32(h1, o1_2, 75); /* H1 = 89*(B0 - B7) + 75*(B1 - B6) */

        /* H1 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5) */
        h1 = vmlaq_n_s32(h1, o2_2, 50);
        /* H3 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5) */
        h3 = vmlsq_n_s32(h3, o2_2, 89);
        /* H5 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5) */
        h5 = vmlaq_n_s32(h5, o2_2, 18);
        /* H7 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5) */
        h7 = vmlaq_n_s32(h7, o2_2, 75);

        /* H7 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5) - 89*(B3 - B4) */
        h7 = vmlsq_n_s32(h7, o3_2, 89);
        /* Truncating last 11 bits in H7*/
        row7 = vrshrn_n_s32(h7, 11);
        /*First half-row of row 8 of transform stage 2 (H7) stored*/
        vst1_s16(pi2_dst + 7 * dst_strd, row7);
        /* H5 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5) + 75*(B3 - B4) */
        h5 = vmlaq_n_s32(h5, o3_2, 75);
        /* Truncating last 11 bits in H5*/
        row5 = vrshrn_n_s32(h5, 11);
        /*First half-row of row 6 of transform stage 2 (H5) stored*/
        vst1_s16(pi2_dst + 5 * dst_strd, row5);
        /* H3 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5) - 50*(B3 - B4) */
        h3 = vmlsq_n_s32(h3, o3_2, 50);
        /* Truncating last 11 bits in H3*/
        row3 = vrshrn_n_s32(h3, 11);
        /*First half-row of row 4 of transform stage 2 (H3) stored*/
        vst1_s16(pi2_dst + 3 * dst_strd, row3);
        /* H1 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5) + 18*(B3 - B4) */
        h1 = vmlaq_n_s32(h1, o3_2, 18);
        /* Truncating last 11 bits in H1*/
        row1 = vrshrn_n_s32(h1, 11);
        /*First half-row of row 2 of transform stage 2 (H1) stored*/
        vst1_s16(pi2_dst + dst_strd, row1);
    }

    pi2_dst += 4;

    {
        int32x4_t h0, h1, h2, h3, h4, h5, h6, h7;
        int32x4_t e0_2, e1_2, e2_2, e3_2;
        int32x4_t o0_2, o1_2, o2_2, o3_2;
        int32x4_t ee1_2, eo1_2, eo0_2, ee0_2;
        int16x4_t row0, row1, row2, row3, row4, row5, row6, row7;

        /*Transposing second half of transform stage 1 (1)*/
        int32x4x2_t b1 = vtrnq_s32(a4.val[1], a5.val[1]);
        int32x4x2_t b3 = vtrnq_s32(a6.val[1], a7.val[1]);
        int32x4x2_t b0 = vtrnq_s32(a4.val[0], a5.val[0]);
        int32x4x2_t b2 = vtrnq_s32(a6.val[0], a7.val[0]);

        /*Transposing second half of transform stage 1 (2)*/
        a0.val[0] = vcombine_s32(vget_low_s32(b0.val[0]), vget_low_s32(b2.val[0]));
        a2.val[0] = vcombine_s32(vget_high_s32(b0.val[0]), vget_high_s32(b2.val[0]));
        a1.val[0] = vcombine_s32(vget_low_s32(b0.val[1]), vget_low_s32(b2.val[1]));
        a3.val[0] = vcombine_s32(vget_high_s32(b0.val[1]), vget_high_s32(b2.val[1]));
        a0.val[1] = vcombine_s32(vget_low_s32(b1.val[0]), vget_low_s32(b3.val[0]));
        a2.val[1] = vcombine_s32(vget_high_s32(b1.val[0]), vget_high_s32(b3.val[0]));
        a1.val[1] = vcombine_s32(vget_low_s32(b1.val[1]), vget_low_s32(b3.val[1]));
        a3.val[1] = vcombine_s32(vget_high_s32(b1.val[1]), vget_high_s32(b3.val[1]));

        o0_2 = vsubq_s32(a0.val[0], a3.val[1]); /*B0 - B7*/
        o1_2 = vsubq_s32(a1.val[0], a2.val[1]); /*B1 - B6*/
        o2_2 = vsubq_s32(a2.val[0], a1.val[1]); /*B2 - B5*/
        o3_2 = vsubq_s32(a3.val[0], a0.val[1]); /*B3 - B4*/
        e3_2 = vaddq_s32(a3.val[0], a0.val[1]); /*B3 + B4*/
        e2_2 = vaddq_s32(a2.val[0], a1.val[1]); /*B2 + B5*/
        e1_2 = vaddq_s32(a1.val[0], a2.val[1]); /*B1 + B6*/
        e0_2 = vaddq_s32(a0.val[0], a3.val[1]); /*B0 + B7*/

        eo1_2 = vsubq_s32(e1_2, e2_2); /*B1 - B2 - B5 + B6*/
        ee1_2 = vaddq_s32(e1_2, e2_2); /*B1 + B2 + B5 + B6*/
        eo0_2 = vsubq_s32(e0_2, e3_2); /*B0 - B3 - B4 + B7*/
        ee0_2 = vaddq_s32(e0_2, e3_2); /*B0 + B3 + B4 + B7*/

        /* F4 = B0 - B1 - B2 + B3 + B4 - B5 - B6 + B7*/
        h4 = vsubq_s32(ee0_2, ee1_2);
        /* F0 = B0 + B1 + B2 + B3 + B4 + B5 + B6 + B7*/
        h0 = vaddq_s32(ee0_2, ee1_2);
        /* Truncating last 11 bits in H0*/
        row0 = vrshrn_n_s32(h0, 5);
        /*First half-row of row 1 of transform stage 2 (H0) stored*/
        vst1_s16(pi2_dst, row0);
        /* Truncating last 11 bits in H4*/
        row4 = vrshrn_n_s32(h4, 5);
        /*First half-row of row 5 of transform stage 2 (H4) stored*/
        vst1_s16(pi2_dst + 4 * dst_strd, row4);

        /* F6 = 36*(B0 - B3 - B4 + B7) */
        h6 = vmulq_n_s32(eo0_2, 36);
        /* F2 = 83*(B0 - B3 - B4 + B7) */
        h2 = vmulq_n_s32(eo0_2, 83);
        /*H2 = 83*(B0 - B3 - B4 + B7) + 36*(B1 - B2 - B5 + B6)*/
        h2 = vmlaq_n_s32(h2, eo1_2, 36);
        /*H6 = 36*(B0 - B3 - B4 + B7) - 83*(B1 - B2 - B5 + B6)*/
        h6 = vmlsq_n_s32(h6, eo1_2, 83);
        /* Truncating last 11 bits in H6*/
        row6 = vrshrn_n_s32(h6, 11);
        /*First half-row of row 7 of transform stage 2 (H6) stored*/
        vst1_s16(pi2_dst + 6 * dst_strd, row6);
        /* Truncating last 11 bits in H2*/
        row2 = vrshrn_n_s32(h2, 11);
        /*First half-row of row 3 of transform stage 2 (H2) stored*/
        vst1_s16(pi2_dst + 2 * dst_strd, row2);

        h1 = vmulq_n_s32(o0_2, 89); /* H1 = 89*(B0 - B7) */
        h3 = vmulq_n_s32(o0_2, 75); /* H3 = 75*(B0 - B7) */
        h5 = vmulq_n_s32(o0_2, 50); /* H5 = 50*(B0 - B7) */
        h7 = vmulq_n_s32(o0_2, 18); /* H7 = 18*(B0 - B7) */

        h7 = vmlsq_n_s32(h7, o1_2, 50); /* H7 = 18*(B0 - B7) - 50*(B1 - B6) */
        h5 = vmlsq_n_s32(h5, o1_2, 89); /* H5 = 50*(B0 - B7) - 89*(B1 - B6) */
        h3 = vmlsq_n_s32(h3, o1_2, 18); /* H3 = 75*(B0 - B7) - 18*(B1 - B6) */
        h1 = vmlaq_n_s32(h1, o1_2, 75); /* H1 = 89*(B0 - B7) + 75*(B1 - B6) */

        /* H1 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5) */
        h1 = vmlaq_n_s32(h1, o2_2, 50);
        /* H3 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5) */
        h3 = vmlsq_n_s32(h3, o2_2, 89);
        /* H5 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5) */
        h5 = vmlaq_n_s32(h5, o2_2, 18);
        /* H7 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5) */
        h7 = vmlaq_n_s32(h7, o2_2, 75);

        /* H7 = 18*(B0 - B7) - 50*(B1 - B6) + 75*(B2 - B5) - 89*(B3 - B4) */
        h7 = vmlsq_n_s32(h7, o3_2, 89);
        /* Truncating last 11 bits in H7*/
        row7 = vrshrn_n_s32(h7, 11);
        /*First half-row of row 8 of transform stage 2 (H7) stored*/
        vst1_s16(pi2_dst + 7 * dst_strd, row7);
        /* H5 = 50*(B0 - B7) - 89*(B1 - B6) + 18*(B2 - B5) + 75*(B3 - B4) */
        h5 = vmlaq_n_s32(h5, o3_2, 75);
        /* Truncating last 11 bits in H5*/
        row5 = vrshrn_n_s32(h5, 11);
        /*First half-row of row 6 of transform stage 2 (H5) stored*/
        vst1_s16(pi2_dst + 5 * dst_strd, row5);
        /* H3 = 75*(B0 - B7) - 18*(B1 - B6) - 89*(B2 - B5) - 50*(B3 - B4) */
        h3 = vmlsq_n_s32(h3, o3_2, 50);
        /* Truncating last 11 bits in H3*/
        row3 = vrshrn_n_s32(h3, 11);
        /*First half-row of row 4 of transform stage 2 (H3) stored*/
        vst1_s16(pi2_dst + 3 * dst_strd, row3);
        /* H1 = 89*(B0 - B7) + 75*(B1 - B6) + 50*(B2 - B5) + 18*(B3 - B4) */
        h1 = vmlaq_n_s32(h1, o3_2, 18);
        /* Truncating last 11 bits in H1*/
        row1 = vrshrn_n_s32(h1, 11);
        /*First half-row of row 2 of transform stage 2 (H1) stored*/
        vst1_s16(pi2_dst + dst_strd, row1);
    }
    return sad;
}

static INLINE void load(const uint8_t *a, int stride, uint8x8_t *b,
                        CHROMA_PLANE_ID_T e_chroma_plane)
{
    int i;

    if(e_chroma_plane == NULL_PLANE)
    {
        for (i = 0; i < 16; i++)
        {
            b[i] = vld1_u8(a);
            a += stride;
        }
    }
    else
    {
        for (i = 0; i < 16; i++)
        {
            b[i] = vld2_u8(a).val[e_chroma_plane];
            a += stride;
        }
    }
}

// Store 8 16x8 values, assuming stride == 16.
static INLINE void store(WORD16 *a, int16x8_t *b /*[8]*/)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        vst1q_s16(a, b[i]);
        a += 16;
    }
}

static INLINE void cross_input_16(int16x8_t *a /*[16]*/, int16x8_t *b /*[16]*/)
{
    b[0] = vaddq_s16(a[0], a[15]);
    b[1] = vaddq_s16(a[1], a[14]);
    b[2] = vaddq_s16(a[2], a[13]);
    b[3] = vaddq_s16(a[3], a[12]);
    b[4] = vaddq_s16(a[4], a[11]);
    b[5] = vaddq_s16(a[5], a[10]);
    b[6] = vaddq_s16(a[6], a[9]);
    b[7] = vaddq_s16(a[7], a[8]);

    b[8] = vsubq_s16(a[7], a[8]);
    b[9] = vsubq_s16(a[6], a[9]);
    b[10] = vsubq_s16(a[5], a[10]);
    b[11] = vsubq_s16(a[4], a[11]);
    b[12] = vsubq_s16(a[3], a[12]);
    b[13] = vsubq_s16(a[2], a[13]);
    b[14] = vsubq_s16(a[1], a[14]);
    b[15] = vsubq_s16(a[0], a[15]);
}

static INLINE void cross_input_32(int32x4x2_t *a /*[16][2]*/, int32x4x2_t *b /*[16][2]*/)
{
    WORD32 i;
    for(i = 0; i < 2; i++)
    {
        b[0].val[i] = vaddq_s32(a[0].val[i], a[15].val[i]);
        b[1].val[i] = vaddq_s32(a[1].val[i], a[14].val[i]);
        b[2].val[i] = vaddq_s32(a[2].val[i], a[13].val[i]);
        b[3].val[i] = vaddq_s32(a[3].val[i], a[12].val[i]);
        b[4].val[i] = vaddq_s32(a[4].val[i], a[11].val[i]);
        b[5].val[i] = vaddq_s32(a[5].val[i], a[10].val[i]);
        b[6].val[i] = vaddq_s32(a[6].val[i], a[9].val[i]);
        b[7].val[i] = vaddq_s32(a[7].val[i], a[8].val[i]);

        b[8].val[i] = vsubq_s32(a[7].val[i], a[8].val[i]);
        b[9].val[i] = vsubq_s32(a[6].val[i], a[9].val[i]);
        b[10].val[i] = vsubq_s32(a[5].val[i], a[10].val[i]);
        b[11].val[i] = vsubq_s32(a[4].val[i], a[11].val[i]);
        b[12].val[i] = vsubq_s32(a[3].val[i], a[12].val[i]);
        b[13].val[i] = vsubq_s32(a[2].val[i], a[13].val[i]);
        b[14].val[i] = vsubq_s32(a[1].val[i], a[14].val[i]);
        b[15].val[i] = vsubq_s32(a[0].val[i], a[15].val[i]);
    }
}

static INLINE int32x4_t diff(uint8x8_t *a /*[16]*/, uint8x8_t *b /*[16]*/, int16x8_t *c /*[16]*/)
{
    int i;
    int16x8_t abs = vdupq_n_s16(0);

    for (i = 0; i < 16; i++)
    {
        c[i] = vreinterpretq_s16_u16(vsubl_u8(a[i], b[i]));
        abs = vaddq_s16(abs, vabsq_s16(c[i]));
    }
    return vpaddlq_s16(abs);
}

static INLINE void partial_round_shift(int32x4x2_t *a, int16x8_t *b /*[16]*/)
{
    WORD32 shift = 13, add;
    add = 1 << (shift - 1);

    const int32x4_t vecadd = vdupq_n_s32(add);
    b[0] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[0].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[0].val[1], vecadd), 13));
    b[1] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[1].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[1].val[1], vecadd), 13));
    b[2] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[2].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[2].val[1], vecadd), 13));
    b[3] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[3].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[3].val[1], vecadd), 13));
    b[4] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[4].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[4].val[1], vecadd), 13));
    b[5] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[5].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[5].val[1], vecadd), 13));
    b[6] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[6].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[6].val[1], vecadd), 13));
    b[7] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[7].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[7].val[1], vecadd), 13));
    b[8] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[8].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[8].val[1], vecadd), 13));
    b[9] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[9].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[9].val[1], vecadd), 13));
    b[10] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[10].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[10].val[1], vecadd), 13));
    b[11] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[11].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[11].val[1], vecadd), 13));
    b[12] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[12].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[12].val[1], vecadd), 13));
    b[13] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[13].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[13].val[1], vecadd), 13));
    b[14] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[14].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[14].val[1], vecadd), 13));
    b[15] = vcombine_s16(
        vshrn_n_s32(vaddq_s32(a[15].val[0], vecadd), 13),
        vshrn_n_s32(vaddq_s32(a[15].val[1], vecadd), 13));
}

static INLINE int32x4_t
    add4(int32x4_t row1_low, int32x4_t row1_high, int32x4_t row2_low, int32x4_t row2_high)
{
    int32x4_t sum1, sum2;
    sum1 = vaddq_s32(row1_low, row1_high);
    sum2 = vaddq_s32(row2_low, row2_high);
    return vaddq_s32(sum1, sum2);
}

static INLINE void butterfly_one_coeff_16_32(
    int16x8_t a, int16x8_t b, int16_t c, int32x4x2_t *row1, int32x4x2_t *row2)
{
    const int32x4_t a0 = vmull_n_s16(vget_low_s16(a), c);
    const int32x4_t a1 = vmull_n_s16(vget_high_s16(a), c);
    //printf("multiply done\n");
    row1->val[0] = vmlal_n_s16(a0, vget_low_s16(b), c);
    row1->val[1] = vmlal_n_s16(a1, vget_high_s16(b), c);
    row2->val[0] = vmlsl_n_s16(a0, vget_low_s16(b), c);
    row2->val[1] = vmlsl_n_s16(a1, vget_high_s16(b), c);
}

static INLINE void butterfly_two_coeff_16_32(
    int16x8_t a, int16x8_t b, int16_t c0, int16_t c1, int32x4x2_t *row1, int32x4x2_t *row2)
{
    const int32x4_t a0 = vmull_n_s16(vget_low_s16(a), c0);
    const int32x4_t a1 = vmull_n_s16(vget_high_s16(a), c0);
    const int32x4_t a2 = vmull_n_s16(vget_low_s16(a), c1);
    const int32x4_t a3 = vmull_n_s16(vget_high_s16(a), c1);
    row1->val[0] = vmlal_n_s16(a2, vget_low_s16(b), c0);
    row1->val[1] = vmlal_n_s16(a3, vget_high_s16(b), c0);
    row2->val[0] = vmlsl_n_s16(a0, vget_low_s16(b), c1);
    row2->val[1] = vmlsl_n_s16(a1, vget_high_s16(b), c1);
}

static INLINE void butterfly_one_coeff_32_32(
    int32x4x2_t a, int32x4x2_t b, int32_t c, int32x4x2_t *row1, int32x4x2_t *row2)
{
    const int32x4_t a0 = vmulq_n_s32(a.val[0], c);
    const int32x4_t a1 = vmulq_n_s32(a.val[1], c);
    row1->val[0] = vmlaq_n_s32(a0, b.val[0], c);
    row1->val[1] = vmlaq_n_s32(a1, b.val[1], c);
    row2->val[0] = vmlsq_n_s32(a0, b.val[0], c);
    row2->val[1] = vmlsq_n_s32(a1, b.val[1], c);
}

static INLINE void butterfly_two_coeff_32_32(
    int32x4x2_t a, int32x4x2_t b, int32_t c0, int32_t c1, int32x4x2_t *row1, int32x4x2_t *row2)
{
    const int32x4_t a0 = vmulq_n_s32(a.val[0], c0);
    const int32x4_t a1 = vmulq_n_s32(a.val[1], c0);
    const int32x4_t a2 = vmulq_n_s32(a.val[0], c1);
    const int32x4_t a3 = vmulq_n_s32(a.val[1], c1);
    row1->val[0] = vmlaq_n_s32(a2, b.val[0], c0);
    row1->val[1] = vmlaq_n_s32(a3, b.val[1], c0);
    row2->val[0] = vmlsq_n_s32(a0, b.val[0], c1);
    row2->val[1] = vmlsq_n_s32(a1, b.val[1], c1);
}

// Transpose 8x8 to a new location. Don't use transpose_neon.h because those
// are all in-place.
static INLINE void transpose_8x8(int32x4x2_t *a /*[8][2]*/, int32x4x2_t *b)
{
    const int32x4x2_t c0 = vtrnq_s32(a[0].val[0], a[1].val[0]);
    const int32x4x2_t c1 = vtrnq_s32(a[2].val[0], a[3].val[0]);
    const int32x4x2_t c2 = vtrnq_s32(a[4].val[0], a[5].val[0]);
    const int32x4x2_t c3 = vtrnq_s32(a[6].val[0], a[7].val[0]);
    const int32x4x2_t c4 = vtrnq_s32(a[0].val[1], a[1].val[1]);
    const int32x4x2_t c5 = vtrnq_s32(a[2].val[1], a[3].val[1]);
    const int32x4x2_t c6 = vtrnq_s32(a[4].val[1], a[5].val[1]);
    const int32x4x2_t c7 = vtrnq_s32(a[6].val[1], a[7].val[1]);

    const int32x4x2_t d0 = vtrnq_s64_to_s32(c0.val[0], c1.val[0]);
    const int32x4x2_t d1 = vtrnq_s64_to_s32(c0.val[1], c1.val[1]);
    const int32x4x2_t d2 = vtrnq_s64_to_s32(c2.val[0], c3.val[0]);
    const int32x4x2_t d3 = vtrnq_s64_to_s32(c2.val[1], c3.val[1]);
    const int32x4x2_t d4 = vtrnq_s64_to_s32(c4.val[0], c5.val[0]);
    const int32x4x2_t d5 = vtrnq_s64_to_s32(c4.val[1], c5.val[1]);
    const int32x4x2_t d6 = vtrnq_s64_to_s32(c6.val[0], c7.val[0]);
    const int32x4x2_t d7 = vtrnq_s64_to_s32(c6.val[1], c7.val[1]);

    b[0].val[0] = d0.val[0];
    b[0].val[1] = d2.val[0];
    b[1].val[0] = d1.val[0];
    b[1].val[1] = d3.val[0];
    b[2].val[0] = d0.val[1];
    b[2].val[1] = d2.val[1];
    b[3].val[0] = d1.val[1];
    b[3].val[1] = d3.val[1];
    b[4].val[0] = d4.val[0];
    b[4].val[1] = d6.val[0];
    b[5].val[0] = d5.val[0];
    b[5].val[1] = d7.val[0];
    b[6].val[0] = d4.val[1];
    b[6].val[1] = d6.val[1];
    b[7].val[0] = d5.val[1];
    b[7].val[1] = d7.val[1];
}

static void dct_body_16_32(int16x8_t *in /*[16]*/, int32x4x2_t *out /*[16]*/)
{
    int16x8_t s[8];
    int16x8_t x[4];
    int32x4x2_t tmp0, tmp1, tmp2, tmp3;
    int32x4x2_t tmp4, tmp5, tmp6, tmp7;

    s[0] = vaddq_s16(in[0], in[7]);
    s[1] = vaddq_s16(in[1], in[6]);
    s[2] = vaddq_s16(in[2], in[5]);
    s[3] = vaddq_s16(in[3], in[4]);
    s[4] = vsubq_s16(in[3], in[4]);
    s[5] = vsubq_s16(in[2], in[5]);
    s[6] = vsubq_s16(in[1], in[6]);
    s[7] = vsubq_s16(in[0], in[7]);

    x[0] = vaddq_s16(s[0], s[3]);
    x[1] = vaddq_s16(s[1], s[2]);
    x[2] = vsubq_s16(s[1], s[2]);
    x[3] = vsubq_s16(s[0], s[3]);

    // Type 1
    // out[0] = fdct_round_shift((x0 + x1) * cospi_16_64)
    // out[8] = fdct_round_shift((x0 - x1) * cospi_16_64)
    butterfly_one_coeff_16_32(x[0], x[1], 64, &out[0], &out[8]);

    // out[4] = fdct_round_shift(x3 * cospi_8_64 + x2 * cospi_24_64);
    // out[12] = fdct_round_shift(x3 * cospi_24_64 - x2 * cospi_8_64);
    butterfly_two_coeff_16_32(x[3], x[2], 36, 83, &out[4], &out[12]);

    //  Type 2
    butterfly_two_coeff_16_32(s[7], s[4], 18, 89, &tmp0, &tmp1);
    butterfly_two_coeff_16_32(s[5], s[6], 75, 50, &tmp2, &tmp3);

    out[2].val[0] = vaddq_s32(tmp0.val[0], tmp2.val[0]);
    out[2].val[1] = vaddq_s32(tmp0.val[1], tmp2.val[1]);

    out[14].val[0] = vaddq_s32(tmp1.val[0], tmp3.val[0]);
    out[14].val[1] = vaddq_s32(tmp1.val[1], tmp3.val[1]);

    butterfly_two_coeff_16_32(s[7], s[4], 75, 50, &tmp0, &tmp1);
    butterfly_two_coeff_16_32(s[5], s[6], -89, 18, &tmp2, &tmp3);

    out[10].val[0] = vaddq_s32(tmp0.val[0], tmp2.val[0]);
    out[10].val[1] = vaddq_s32(tmp0.val[1], tmp2.val[1]);

    out[6].val[0] = vaddq_s32(tmp1.val[0], tmp3.val[0]);
    out[6].val[1] = vaddq_s32(tmp1.val[1], tmp3.val[1]);

    //  Type 3
    butterfly_two_coeff_16_32(in[8], in[15], 9, -90, &tmp0, &tmp1);
    butterfly_two_coeff_16_32(in[9], in[14], 87, 25, &tmp2, &tmp3);
    butterfly_two_coeff_16_32(in[10], in[13], 43, -80, &tmp4, &tmp5);
    butterfly_two_coeff_16_32(in[11], in[12], 70, 57, &tmp6, &tmp7);

    out[1].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[1].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);

    out[15].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[15].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    butterfly_two_coeff_16_32(in[8], in[15], 87, -25, &tmp0, &tmp1);
    butterfly_two_coeff_16_32(in[9], in[14], -70, -57, &tmp2, &tmp3);
    butterfly_two_coeff_16_32(in[10], in[13], 9, -90, &tmp4, &tmp5);
    butterfly_two_coeff_16_32(in[11], in[12], -80, 43, &tmp6, &tmp7);

    out[3].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[3].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    out[13].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[13].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);

    butterfly_two_coeff_16_32(in[8], in[15], 43, -80, &tmp0, &tmp1);
    butterfly_two_coeff_16_32(in[9], in[14], 9, 90, &tmp2, &tmp3);
    butterfly_two_coeff_16_32(in[10], in[13], 57, 70, &tmp4, &tmp5);
    butterfly_two_coeff_16_32(in[11], in[12], -87, -25, &tmp6, &tmp7);

    out[5].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[5].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);

    out[11].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[11].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    butterfly_two_coeff_16_32(in[8], in[15], 70, -57, &tmp0, &tmp1);
    butterfly_two_coeff_16_32(in[9], in[14], -80, 43, &tmp2, &tmp3);
    butterfly_two_coeff_16_32(in[10], in[13], -87, 25, &tmp4, &tmp5);
    butterfly_two_coeff_16_32(in[11], in[12], 90, -9, &tmp6, &tmp7);

    out[7].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[7].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    out[9].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[9].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);
}

static void dct_body_32_32(int32x4x2_t *in /*[16]*/, int32x4x2_t *out /*[16]*/)
{
    int32x4x2_t s[8];
    int32x4x2_t x[4];
    int32x4x2_t tmp0, tmp1, tmp2, tmp3;
    int32x4x2_t tmp4, tmp5, tmp6, tmp7;
    WORD32 i;

    for(i = 0; i < 2; i++)
    {
        s[0].val[i] = vaddq_s32(in[0].val[i], in[7].val[i]);
        s[1].val[i] = vaddq_s32(in[1].val[i], in[6].val[i]);
        s[2].val[i] = vaddq_s32(in[2].val[i], in[5].val[i]);
        s[3].val[i] = vaddq_s32(in[3].val[i], in[4].val[i]);
        s[4].val[i] = vsubq_s32(in[3].val[i], in[4].val[i]);
        s[5].val[i] = vsubq_s32(in[2].val[i], in[5].val[i]);
        s[6].val[i] = vsubq_s32(in[1].val[i], in[6].val[i]);
        s[7].val[i] = vsubq_s32(in[0].val[i], in[7].val[i]);

        x[0].val[i] = vaddq_s32(s[0].val[i], s[3].val[i]);
        x[1].val[i] = vaddq_s32(s[1].val[i], s[2].val[i]);
        x[2].val[i] = vsubq_s32(s[1].val[i], s[2].val[i]);
        x[3].val[i] = vsubq_s32(s[0].val[i], s[3].val[i]);
    }

    // Type 1
    // out[0] = fdct_round_shift((x0 + x1) * cospi_16_64)
    // out[8] = fdct_round_shift((x0 - x1) * cospi_16_64)
    butterfly_one_coeff_32_32(x[0], x[1], 64, &out[0], &out[8]);
    // out[4] = fdct_round_shift(x3 * cospi_8_64 + x2 * cospi_24_64);
    // out[12] = fdct_round_shift(x3 * cospi_24_64 - x2 * cospi_8_64);
    butterfly_two_coeff_32_32(x[3], x[2], 36, 83, &out[4], &out[12]);

    //  Type 2
    butterfly_two_coeff_32_32(s[7], s[4], 18, 89, &tmp0, &tmp1);
    butterfly_two_coeff_32_32(s[5], s[6], 75, 50, &tmp2, &tmp3);

    out[2].val[0] = vaddq_s32(tmp0.val[0], tmp2.val[0]);
    out[2].val[1] = vaddq_s32(tmp0.val[1], tmp2.val[1]);

    out[14].val[0] = vaddq_s32(tmp1.val[0], tmp3.val[0]);
    out[14].val[1] = vaddq_s32(tmp1.val[1], tmp3.val[1]);

    butterfly_two_coeff_32_32(s[7], s[4], 75, 50, &tmp0, &tmp1);
    butterfly_two_coeff_32_32(s[5], s[6], -89, 18, &tmp2, &tmp3);

    out[10].val[0] = vaddq_s32(tmp0.val[0], tmp2.val[0]);
    out[10].val[1] = vaddq_s32(tmp0.val[1], tmp2.val[1]);

    out[6].val[0] = vaddq_s32(tmp1.val[0], tmp3.val[0]);
    out[6].val[1] = vaddq_s32(tmp1.val[1], tmp3.val[1]);

    //  Type 3
    butterfly_two_coeff_32_32(in[8], in[15], 9, -90, &tmp0, &tmp1);
    butterfly_two_coeff_32_32(in[9], in[14], 87, 25, &tmp2, &tmp3);
    butterfly_two_coeff_32_32(in[10], in[13], 43, -80, &tmp4, &tmp5);
    butterfly_two_coeff_32_32(in[11], in[12], 70, 57, &tmp6, &tmp7);

    out[1].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[1].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);

    out[15].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[15].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    butterfly_two_coeff_32_32(in[8], in[15], 87, -25, &tmp0, &tmp1);
    butterfly_two_coeff_32_32(in[9], in[14], -70, -57, &tmp2, &tmp3);
    butterfly_two_coeff_32_32(in[10], in[13], 9, -90, &tmp4, &tmp5);
    butterfly_two_coeff_32_32(in[11], in[12], -80, 43, &tmp6, &tmp7);

    out[3].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[3].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    out[13].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[13].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);

    butterfly_two_coeff_32_32(in[8], in[15], 43, -80, &tmp0, &tmp1);
    butterfly_two_coeff_32_32(in[9], in[14], 9, 90, &tmp2, &tmp3);
    butterfly_two_coeff_32_32(in[10], in[13], 57, 70, &tmp4, &tmp5);
    butterfly_two_coeff_32_32(in[11], in[12], -87, -25, &tmp6, &tmp7);

    out[5].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[5].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);

    out[11].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[11].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    butterfly_two_coeff_32_32(in[8], in[15], 70, -57, &tmp0, &tmp1);
    butterfly_two_coeff_32_32(in[9], in[14], -80, 43, &tmp2, &tmp3);
    butterfly_two_coeff_32_32(in[10], in[13], -87, 25, &tmp4, &tmp5);
    butterfly_two_coeff_32_32(in[11], in[12], 90, -9, &tmp6, &tmp7);

    out[7].val[0] = add4(tmp0.val[0], tmp3.val[0], tmp4.val[0], tmp7.val[0]);
    out[7].val[1] = add4(tmp0.val[1], tmp3.val[1], tmp4.val[1], tmp7.val[1]);

    out[9].val[0] = add4(tmp1.val[0], tmp2.val[0], tmp5.val[0], tmp6.val[0]);
    out[9].val[1] = add4(tmp1.val[1], tmp2.val[1], tmp5.val[1], tmp6.val[1]);
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs residue calculation and forward  transform on
 * input pixels
 *
 * @par Description:
 *  Performs residue calculation by subtracting source and  prediction and
 * followed by forward transform
 *
 * @param[in] pu1_src
 *  Input 16x16 pixels
 *
 * @param[in] pu1_pred
 *  Prediction data
 *
 * @param[in] pi2_tmp
 *  Temporary buffer of size 16x16
 *
 * @param[out] pi2_dst
 *  Output 16x16 coefficients
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] pred_strd
 *  Prediction Stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[in] e_chroma_plane
 *  Enum singalling chroma plane
 *
 * @returns  Void
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */
UWORD32 ihevc_resi_trans_16x16_neon(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 *pi4_temp,
    WORD16 *pi2_dst,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 dst_strd,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    UWORD32 u4_blk_sad = 0;
    WORD32 chroma_flag;
    uint8x8_t temp0[16], temp1[16];
    int16x8_t temp2[16], temp3[16];
    int32x4_t tmp_a, tmp_b;
    int64x2_t tmp_c;
    int32x2_t sad_v;
    int32x4x2_t out0[16], out1[16], temp4[16], temp5[16];

    (void)pi4_temp;
    chroma_flag = e_chroma_plane != NULL_PLANE;
    /* Residue + Forward Transform 1st stage */
    // Left half.
    load(pu1_src, src_strd, temp0, e_chroma_plane);
    load(pu1_pred, pred_strd, temp1, e_chroma_plane);

    tmp_a = diff(temp0, temp1, temp2);
    cross_input_16(temp2, temp3);
    dct_body_16_32(temp3, out0);

    // Right half.
    load(pu1_src + 8 * (1 + chroma_flag), src_strd, temp0, e_chroma_plane);
    load(pu1_pred + 8 * (1 + chroma_flag), pred_strd, temp1, e_chroma_plane);

    tmp_b = diff(temp0, temp1, temp2);
    cross_input_16(temp2, temp3);
    dct_body_16_32(temp3, out1);

    tmp_a = vaddq_s32(tmp_a, tmp_b);
    tmp_c = vpaddlq_s32(tmp_a);
    sad_v = vadd_s32(vreinterpret_s32_s64(vget_low_s64(tmp_c)),
                   vreinterpret_s32_s64(vget_high_s64(tmp_c)));
    u4_blk_sad = vget_lane_s32(sad_v, 0);


    // Transpose top left and top right quarters into one contiguous location to
    // process to the top half.
    transpose_8x8(&out0[0], &temp4[0]);
    transpose_8x8(&out1[0], &temp4[8]);

    cross_input_32(temp4, temp5);
    dct_body_32_32(temp5, temp4);
    partial_round_shift(temp4, temp2);
    transpose_s16_8x8(
        &temp2[0], &temp2[1], &temp2[2], &temp2[3], &temp2[4], &temp2[5], &temp2[6], &temp2[7]);
    transpose_s16_8x8(
        &temp2[8], &temp2[9], &temp2[10], &temp2[11], &temp2[12], &temp2[13], &temp2[14], &temp2[15]);

    store(pi2_dst, &temp2[0]);
    store(pi2_dst + 8, &temp2[8]);
    pi2_dst += 8 * dst_strd;

    // Transpose bottom left and bottom right quarters into one contiguous
    // location to process to the bottom half.
    transpose_8x8(&out0[8], &out1[0]);
    transpose_s32_8x8(
        &out1[8], &out1[9], &out1[10], &out1[11], &out1[12], &out1[13], &out1[14], &out1[15]);

    cross_input_32(out1, temp5);
    dct_body_32_32(temp5, temp4);
    partial_round_shift(temp4, temp2);
    transpose_s16_8x8(
        &temp2[0], &temp2[1], &temp2[2], &temp2[3], &temp2[4], &temp2[5], &temp2[6], &temp2[7]);
    transpose_s16_8x8(
        &temp2[8], &temp2[9], &temp2[10], &temp2[11], &temp2[12], &temp2[13], &temp2[14], &temp2[15]);
    store(pi2_dst, &temp2[0]);
    store(pi2_dst + 8, &temp2[8]);

    return u4_blk_sad;
}
