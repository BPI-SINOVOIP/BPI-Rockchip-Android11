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
*  ihevc_quant_iquant_ssd_neon_intr.c
*
* @brief
*  Contains function definitions for quantization, followed by Inverse
*  quantization to find transform domain SSD
*
* @author
*  100736
*
* @par List of Functions:
*   - ihevc_quant_iquant_ssd_flat_scale_mat_neon()
*   - ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_neon()
*
* @remarks
*
*
*******************************************************************************
*/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevc_macros.h"
#include "ihevc_platform_macros.h"
#include "ihevc_defs.h"
#include "ihevc_debug.h"
#include "ihevc_trans_tables.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_func_selector.h"
#include "ihevc_trans_macros.h"
#include "arm_neon.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

WORD32 ihevc_quant_iquant_ssd_flat_scale_mat_neon(
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32 trans_size,
    WORD32 qp_div,
    WORD32 qp_rem,
    WORD32 q_add,
    WORD32 *pi4_quant_round_factor_0_1,
    WORD32 *pi4_quant_round_factor_1_2,
    WORD32 src_strd,
    WORD32 dst_q_strd,
    WORD32 dst_iq_strd,
    UWORD8 *csbf,
    WORD32 csbf_strd,
    WORD32 *zero_col,
    WORD32 *zero_row,
    WORD16 *pi2_dequant_coeff,
    LWORD64 *pi8_cost)
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD32 cbf = 0;

    WORD16 qm = 4;
    WORD16 bd = 8;
    WORD32 q_bits, tr, temp;
    WORD32 block_col = 0;
    WORD32 block_row = 0;
    WORD32 temp_zero_col = 0;
    WORD32 temp_zero_row = 0;

    WORD32 sh;
    WORD32 s_iq;
    WORD32 sh_tmp;

    // ssd
    int32x4_t ssd0 = vdupq_n_s32(0);
    int32x2_t ssd1;
    WORD32 ssd;
    // const
    const int16x4_t zero = vdup_n_s16(0);
    const int16x4_t zero_d = vdup_n_s16(0);
    const int16x4_t sq = vdup_n_s16(g_ihevc_quant_scales[qp_rem]);
    const int16x4_t siq = vdup_n_s16((g_ihevc_iquant_scales_flat_scale[qp_rem]));
    // src
    int16x4_t s0, s1, s2, s3;
    // q-iq
    int16x4_t q0, q1, q2, q3;
    int16x4_t iq0, iq1, iq2, iq3;
    // residue
    int32x4_t r0, r1, r2, r3;
    // sign
    uint16x4_t psgn0, psgn1, psgn2, psgn3;
    uint16x4_t nsgn0, nsgn1, nsgn2, nsgn3;
    // abs(src)
    int16x4_t abs_s0, abs_s1, abs_s2, abs_s3;
    // q-temp
    int32x4_t qtmp_0, qtmp_1, qtmp_2, qtmp_3;
    int16x4_t pq0, pq1, pq2, pq3;
    int16x4_t nq0, nq1, nq2, nq3;
    // iq-temp
    int32x4_t iqtmp_0, iqtmp_1, iqtmp_2, iqtmp_3;

    int32x4_t add_q;
    int32x4_t add_iq = vdupq_n_s32(1);
    int32x4_t sh_iq_1;
    int32x4_t sh_iq;
    int32x4_t q_v_bits;

    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    (void)pi2_dequant_coeff;

    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    tr = MAX_TR_DYNAMIC_RANGE - bd - log2_size;
    q_bits = QUANT_SHIFT + qp_div + tr + SCALING_Q_SHIFT - qm - FLAT_RESCALE_MAT_Q_SHIFT;
    temp = (((WORD32)q_add) << (q_bits - QUANT_ROUND_FACTOR_Q));

    q_v_bits = vdupq_n_s32(-q_bits);
    add_q = vdupq_n_s32(temp);

    sh = bd + log2_size - 5;

    sh_tmp = (sh - qp_div - 1);
    sh_iq_1 = vdupq_n_s32(sh_tmp);
    add_iq = vshlq_s32(add_iq, sh_iq_1);

    s_iq = (-(sh - qp_div));
    sh_iq = vdupq_n_s32(s_iq);

    for(i = 0; i < trans_size; i += 4)
    {
        for(j = 0; j < trans_size; j += 4)
        {
            s0 = vld1_s16(pi2_coeffs + j);
            s1 = vld1_s16(pi2_coeffs + j + (src_strd));
            s2 = vld1_s16(pi2_coeffs + j + (2 * src_strd));
            s3 = vld1_s16(pi2_coeffs + j + (3 * src_strd));

            /* quantization */
            /* sign */
            psgn0 = vcge_s16(s0, zero);
            psgn1 = vcge_s16(s1, zero);
            psgn2 = vcge_s16(s2, zero);
            psgn3 = vcge_s16(s3, zero);

            nsgn0 = vclt_s16(s0, zero);
            nsgn1 = vclt_s16(s1, zero);
            nsgn2 = vclt_s16(s2, zero);
            nsgn3 = vclt_s16(s3, zero);

            /* |src| */
            abs_s0 = vabs_s16(s0);
            abs_s1 = vabs_s16(s1);
            abs_s2 = vabs_s16(s2);
            abs_s3 = vabs_s16(s3);

            /* tmp = tmp * quant_coeff */
            qtmp_0 = vmull_s16(abs_s0, sq);
            qtmp_1 = vmull_s16(abs_s1, sq);
            qtmp_2 = vmull_s16(abs_s2, sq);
            qtmp_3 = vmull_s16(abs_s3, sq);

            /* tmp += (((WORD32)q_add) << (q_bits - QUANT_ROUND_FACTOR_Q)) */
            qtmp_0 = vaddq_s32(qtmp_0, add_q);
            qtmp_1 = vaddq_s32(qtmp_1, add_q);
            qtmp_2 = vaddq_s32(qtmp_2, add_q);
            qtmp_3 = vaddq_s32(qtmp_3, add_q);

            /* tmp >>= q_bits; */
            qtmp_0 = vshlq_s32(qtmp_0, q_v_bits);
            qtmp_1 = vshlq_s32(qtmp_1, q_v_bits);
            qtmp_2 = vshlq_s32(qtmp_2, q_v_bits);
            qtmp_3 = vshlq_s32(qtmp_3, q_v_bits);

            /* clip */
            q0 = vqmovn_s32(qtmp_0);
            q1 = vqmovn_s32(qtmp_1);
            q2 = vqmovn_s32(qtmp_2);
            q3 = vqmovn_s32(qtmp_3);

            /* restore sign */
            pq0 = vand_s16(q0, vreinterpret_s16_u16(psgn0));
            pq1 = vand_s16(q1, vreinterpret_s16_u16(psgn1));
            pq2 = vand_s16(q2, vreinterpret_s16_u16(psgn2));
            pq3 = vand_s16(q3, vreinterpret_s16_u16(psgn3));

            nq0 = vand_s16(q0, vreinterpret_s16_u16(nsgn0));
            nq1 = vand_s16(q1, vreinterpret_s16_u16(nsgn1));
            nq2 = vand_s16(q2, vreinterpret_s16_u16(nsgn2));
            nq3 = vand_s16(q3, vreinterpret_s16_u16(nsgn3));

            q0 = vsub_s16(pq0, nq0);
            q1 = vsub_s16(pq1, nq1);
            q2 = vsub_s16(pq2, nq2);
            q3 = vsub_s16(pq3, nq3);

            /* store */
            vst1_s16((pi2_q_dst + j), q0);
            vst1_s16((pi2_q_dst + j + dst_q_strd), q1);
            vst1_s16((pi2_q_dst + j + (2 * dst_q_strd)), q2);
            vst1_s16((pi2_q_dst + j + (3 * dst_q_strd)), q3);

            *(csbf + block_col) = 0;
            if(vget_lane_s64(vreinterpret_s64_s16(q0), 0) ||
               vget_lane_s64(vreinterpret_s64_s16(q1), 0) ||
               vget_lane_s64(vreinterpret_s64_s16(q2), 0) ||
               vget_lane_s64(vreinterpret_s64_s16(q3), 0))
            {
                *(csbf + block_col) = 1;
            }

            if(*(csbf + block_col) == 1)
            {
                temp_zero_col |= (0xF << block_col * 4);
                temp_zero_row |= (0xF << block_row);

                /* inverse quantization */
                iqtmp_0 = vmull_s16(q0, siq);
                iqtmp_1 = vmull_s16(q1, siq);
                iqtmp_2 = vmull_s16(q2, siq);
                iqtmp_3 = vmull_s16(q3, siq);

                iqtmp_0 = vaddq_s32(iqtmp_0, add_iq);
                iqtmp_1 = vaddq_s32(iqtmp_1, add_iq);
                iqtmp_2 = vaddq_s32(iqtmp_2, add_iq);
                iqtmp_3 = vaddq_s32(iqtmp_3, add_iq);

                iqtmp_0 = vshlq_s32(iqtmp_0, sh_iq);
                iqtmp_1 = vshlq_s32(iqtmp_1, sh_iq);
                iqtmp_2 = vshlq_s32(iqtmp_2, sh_iq);
                iqtmp_3 = vshlq_s32(iqtmp_3, sh_iq);

                /* clip */
                iq0 = vqmovn_s32(iqtmp_0);
                iq1 = vqmovn_s32(iqtmp_1);
                iq2 = vqmovn_s32(iqtmp_2);
                iq3 = vqmovn_s32(iqtmp_3);

                /* store */
                vst1_s16((pi2_iq_dst + j), iq0);
                vst1_s16((pi2_iq_dst + j + dst_iq_strd), iq1);
                vst1_s16((pi2_iq_dst + j + (2 * dst_iq_strd)), iq2);
                vst1_s16((pi2_iq_dst + j + (3 * dst_iq_strd)), iq3);

                /* ssd */
                /* trans_coeff - inv.quant */
                r0 = vsubl_s16(s0, iq0);
                r1 = vsubl_s16(s1, iq1);
                r2 = vsubl_s16(s2, iq2);
                r3 = vsubl_s16(s3, iq3);

                /* SD */
                r0 = vmulq_s32(r0, r0);
                r1 = vmulq_s32(r1, r1);
                r2 = vmulq_s32(r2, r2);
                r3 = vmulq_s32(r3, r3);
            }
            else
            {
                /* store */
                vst1_s16((pi2_iq_dst + j), zero_d);
                vst1_s16((pi2_iq_dst + j + dst_iq_strd), zero_d);
                vst1_s16((pi2_iq_dst + j + (2 * dst_iq_strd)), zero_d);
                vst1_s16((pi2_iq_dst + j + (3 * dst_iq_strd)), zero_d);

                /* SD */
                r0 = vmull_s16(s0, s0);
                r1 = vmull_s16(s1, s1);
                r2 = vmull_s16(s2, s2);
                r3 = vmull_s16(s3, s3);
            }

            /* SSD */
            r0 = vaddq_s32(r0, r1);
            r2 = vaddq_s32(r2, r3);

            r0 = vaddq_s32(r0, r2);

            /* SSD Accumulation */
            ssd0 = vaddq_s32(ssd0, r0);

            cbf = cbf || (*(csbf + block_col));  // cbf update
            block_col++;
        }

        block_col = 0;
        block_row += 4;
        csbf += csbf_strd;

        pi2_coeffs += 4 * src_strd;
        pi2_q_dst += 4 * dst_q_strd;
        pi2_iq_dst += 4 * dst_iq_strd;
        pi2_quant_coeff += 4 * trans_size;
    }

    /* SSD Computation */
    ssd1 = vpadd_s32(vget_low_s32(ssd0), vget_high_s32(ssd0));
    ssd1 = vpadd_s32(ssd1, ssd1);
    ssd = vget_lane_s32(ssd1, 0);

    *zero_col = ~temp_zero_col;  //final zero_col storing
    *zero_row = ~temp_zero_row;  //final zero_row storing

    /* Store the cost */
    *pi8_cost = ssd;

    return cbf;
}

WORD32 ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_neon(
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32 trans_size,
    WORD32 qp_div, /* qpscaled / 6 */
    WORD32 qp_rem, /* qpscaled % 6 */
    WORD32 q_add,
    WORD32 *pi4_quant_round_factor_0_1,
    WORD32 *pi4_quant_round_factor_1_2,
    WORD32 src_strd,
    WORD32 dst_q_strd,
    WORD32 dst_iq_strd,
    UWORD8 *csbf,
    WORD32 csbf_strd,
    WORD32 *zero_col,
    WORD32 *zero_row,
    WORD16 *pi2_dequant_coeff,
    LWORD64 *pi8_cost)
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD32 cbf = 0;

    WORD16 qm = 4;
    WORD16 bd = 8;
    WORD32 q_bits, tr;
    WORD32 block_col = 0;
    WORD32 block_row = 0;
    WORD32 temp_zero_col = 0;
    WORD32 temp_zero_row = 0;

    WORD32 sh;
    WORD32 s_iq;
    WORD32 sh_tmp;

    // ssd
    int32x4_t ssd0 = vdupq_n_s32(0);
    int32x2_t ssd1;
    WORD32 ssd;
    // const
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x4_t zero_d = vdup_n_s16(0);
    const int16x8_t one = vdupq_n_s16(1);
    const int16x8_t two = vdupq_n_s16(2);
    const int16x4_t sq = vdup_n_s16(g_ihevc_quant_scales[qp_rem]);
    const int16x4_t siq = vdup_n_s16((g_ihevc_iquant_scales_flat_scale[qp_rem]));
    // src
    int16x4_t s0, s1, s2, s3;
    // sign
    uint16x8_t psgn0, psgn1;
    uint16x8_t nsgn0, nsgn1;
    int16x8_t pq0, pq1;
    int16x8_t nq0, nq1;
    // abs(src)
    int16x4_t abs_s0, abs_s1, abs_s2, abs_s3;
    // q-temp
    int32x4_t mul_0, mul_1, mul_2, mul_3;
    int32x4_t q_tmp0, q_tmp1, q_tmp2, q_tmp3;
    int16x8_t q_00, q_01;
    int16x8_t q_10, q_11;
    int16x8_t q_20, q_21;
    int16x8_t q_30, q_31;
    // cmp
    uint16x8_t cmp_00, cmp_01;
    uint16x8_t cmp_10, cmp_11;
    uint16x8_t cmp_20, cmp_21;
    // iq-temp
    int32x4_t iqtmp_0, iqtmp_1, iqtmp_2, iqtmp_3;
    int16x4_t iq0, iq1, iq2, iq3;
    //residue
    int32x4_t r0, r1, r2, r3;
    // add_q
    int32x4_t add_q;
    int32x4_t add_q0, add_q1, add_q2, add_q3;
    int32x4_t add_iq = vdupq_n_s32(1);
    int32x4_t sh_iq_1;
    int32x4_t sh_iq;
    int32x4_t q_v_bits;
    int32x4_t stmp;

    (void)q_add;
    (void)pi2_dequant_coeff;
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    tr = MAX_TR_DYNAMIC_RANGE - bd - log2_size;
    q_bits = QUANT_SHIFT + qp_div + tr + SCALING_Q_SHIFT - qm - FLAT_RESCALE_MAT_Q_SHIFT;

    stmp = vdupq_n_s32(q_bits - QUANT_ROUND_FACTOR_Q);

    add_q = vdupq_n_s32((1 << QUANT_ROUND_FACTOR_Q) / 2);
    add_q = vshlq_s32(add_q, stmp);

    q_v_bits = vdupq_n_s32(-q_bits);

    sh = bd + log2_size - 5;

    sh_tmp = (sh - qp_div - 1);
    sh_iq_1 = vdupq_n_s32(sh_tmp);
    add_iq = vshlq_s32(add_iq, sh_iq_1);

    s_iq = (-(sh - qp_div));
    sh_iq = vdupq_n_s32(s_iq);

    for(i = 0; i < trans_size; i += 4)
    {
        for(j = 0; j < trans_size; j += 4)
        {
            s0 = vld1_s16(pi2_coeffs + j);
            s1 = vld1_s16(pi2_coeffs + j + (src_strd));
            s2 = vld1_s16(pi2_coeffs + j + (2 * src_strd));
            s3 = vld1_s16(pi2_coeffs + j + (3 * src_strd));

            /* quantization */
            /* sign */
            psgn0 = vcgeq_s16(vcombine_s16(s0, s1), zero);
            psgn1 = vcgeq_s16(vcombine_s16(s2, s3), zero);

            nsgn0 = vcltq_s16(vcombine_s16(s0, s1), zero);
            nsgn1 = vcltq_s16(vcombine_s16(s2, s3), zero);

            /* |src| */
            abs_s0 = vabs_s16(s0);
            abs_s1 = vabs_s16(s1);
            abs_s2 = vabs_s16(s2);
            abs_s3 = vabs_s16(s3);

            /* tmp = tmp * quant_coeff */
            mul_0 = vmull_s16(abs_s0, sq);
            mul_1 = vmull_s16(abs_s1, sq);
            mul_2 = vmull_s16(abs_s2, sq);
            mul_3 = vmull_s16(abs_s3, sq);

            /* qadd = 0 */
            /* tmp >>= q_bits; */
            q_tmp0 = vshlq_s32(mul_0, q_v_bits);
            q_tmp1 = vshlq_s32(mul_1, q_v_bits);
            q_tmp2 = vshlq_s32(mul_2, q_v_bits);
            q_tmp3 = vshlq_s32(mul_3, q_v_bits);

            /* clip */
            q_00 = vcombine_s16(vqmovn_s32(q_tmp0), vqmovn_s32(q_tmp1));
            q_01 = vcombine_s16(vqmovn_s32(q_tmp2), vqmovn_s32(q_tmp3));

            /* compare qtmp_10, qtmp_20 with 2*/
            cmp_00 = vcltq_s16(q_00, two);
            cmp_01 = vcltq_s16(q_01, two);

            /* qadd = (1 << QUANT_ROUND_FACTOR_Q)/2) */
            /* tmp >>= q_bits; */
            q_tmp0 = vaddq_s32(mul_0, add_q);
            q_tmp1 = vaddq_s32(mul_1, add_q);
            q_tmp2 = vaddq_s32(mul_2, add_q);
            q_tmp3 = vaddq_s32(mul_3, add_q);

            q_tmp0 = vshlq_s32(q_tmp0, q_v_bits);
            q_tmp1 = vshlq_s32(q_tmp1, q_v_bits);
            q_tmp2 = vshlq_s32(q_tmp2, q_v_bits);
            q_tmp3 = vshlq_s32(q_tmp3, q_v_bits);

            /* clip */
            q_10 = vcombine_s16(vqmovn_s32(q_tmp0), vqmovn_s32(q_tmp1));
            q_11 = vcombine_s16(vqmovn_s32(q_tmp2), vqmovn_s32(q_tmp3));

            if(vget_lane_s64(vreinterpret_s64_u16(vget_low_u16(cmp_00)), 0) ||
               vget_lane_s64(vreinterpret_s64_u16(vget_high_u16(cmp_00)), 0) ||
               vget_lane_s64(vreinterpret_s64_u16(vget_low_u16(cmp_01)), 0) ||
               vget_lane_s64(vreinterpret_s64_u16(vget_high_u16(cmp_01)), 0))
            {
                /* qadd = *pi4_quant_round_factor_1_2 */
                /* tmp >>= q_bits; */
                add_q0 = vld1q_s32(pi4_quant_round_factor_1_2 + j);
                add_q1 = vld1q_s32(pi4_quant_round_factor_1_2 + j + (trans_size));
                add_q2 = vld1q_s32(pi4_quant_round_factor_1_2 + j + (2 * trans_size));
                add_q3 = vld1q_s32(pi4_quant_round_factor_1_2 + j + (3 * trans_size));

                add_q0 = vshlq_s32(add_q0, stmp);
                add_q1 = vshlq_s32(add_q1, stmp);
                add_q2 = vshlq_s32(add_q2, stmp);
                add_q3 = vshlq_s32(add_q3, stmp);

                q_tmp0 = vaddq_s32(mul_0, add_q0);
                q_tmp1 = vaddq_s32(mul_1, add_q1);
                q_tmp2 = vaddq_s32(mul_2, add_q2);
                q_tmp3 = vaddq_s32(mul_3, add_q3);

                q_tmp0 = vshlq_s32(q_tmp0, q_v_bits);
                q_tmp1 = vshlq_s32(q_tmp1, q_v_bits);
                q_tmp2 = vshlq_s32(q_tmp2, q_v_bits);
                q_tmp3 = vshlq_s32(q_tmp3, q_v_bits);

                /* clip */
                q_20 = vcombine_s16(vqmovn_s32(q_tmp0), vqmovn_s32(q_tmp1));
                q_21 = vcombine_s16(vqmovn_s32(q_tmp2), vqmovn_s32(q_tmp3));

                /* qadd = *pi4_quant_round_factor_0_1 */
                /* tmp >>= q_bits; */
                add_q0 = vld1q_s32(pi4_quant_round_factor_0_1 + j);
                add_q1 = vld1q_s32(pi4_quant_round_factor_0_1 + j + (trans_size));
                add_q2 = vld1q_s32(pi4_quant_round_factor_0_1 + j + (2 * trans_size));
                add_q3 = vld1q_s32(pi4_quant_round_factor_0_1 + j + (3 * trans_size));

                add_q0 = vshlq_s32(add_q0, stmp);
                add_q1 = vshlq_s32(add_q1, stmp);
                add_q2 = vshlq_s32(add_q2, stmp);
                add_q3 = vshlq_s32(add_q3, stmp);

                q_tmp0 = vaddq_s32(mul_0, add_q0);
                q_tmp1 = vaddq_s32(mul_1, add_q1);
                q_tmp2 = vaddq_s32(mul_2, add_q2);
                q_tmp3 = vaddq_s32(mul_3, add_q3);

                q_tmp0 = vshlq_s32(q_tmp0, q_v_bits);
                q_tmp1 = vshlq_s32(q_tmp1, q_v_bits);
                q_tmp2 = vshlq_s32(q_tmp2, q_v_bits);
                q_tmp3 = vshlq_s32(q_tmp3, q_v_bits);

                /* clip */
                q_30 = vcombine_s16(vqmovn_s32(q_tmp0), vqmovn_s32(q_tmp1));
                q_31 = vcombine_s16(vqmovn_s32(q_tmp2), vqmovn_s32(q_tmp3));

                /* compare qtmp_10, qtmp_20 with 1*/
                cmp_10 = vcltq_s16(q_00, one);
                cmp_11 = vcltq_s16(q_01, one);

                cmp_20 = vbicq_u16(cmp_00, cmp_10);
                cmp_21 = vbicq_u16(cmp_01, cmp_11);

                q_10 = vbslq_s16(cmp_10, q_30, q_10);
                q_11 = vbslq_s16(cmp_11, q_31, q_11);

                q_10 = vbslq_s16(cmp_20, q_20, q_10);
                q_11 = vbslq_s16(cmp_21, q_21, q_11);
            }

            /* restore sign */
            pq0 = vandq_s16(q_10, vreinterpretq_s16_u16(psgn0));
            pq1 = vandq_s16(q_11, vreinterpretq_s16_u16(psgn1));

            nq0 = vandq_s16(q_10, vreinterpretq_s16_u16(nsgn0));
            nq1 = vandq_s16(q_11, vreinterpretq_s16_u16(nsgn1));

            q_10 = vsubq_s16(pq0, nq0);
            q_11 = vsubq_s16(pq1, nq1);

            /* store */
            vst1_s16((pi2_q_dst + j), vget_low_s16(q_10));
            vst1_s16((pi2_q_dst + j + dst_q_strd), vget_high_s16(q_10));
            vst1_s16((pi2_q_dst + j + (2 * dst_q_strd)), vget_low_s16(q_11));
            vst1_s16((pi2_q_dst + j + (3 * dst_q_strd)), vget_high_s16(q_11));

            *(csbf + block_col) = 0;
            if(vget_lane_s64(vreinterpret_s64_s16(vget_low_s16(q_10)), 0) ||
               vget_lane_s64(vreinterpret_s64_s16(vget_high_s16(q_10)), 0) ||
               vget_lane_s64(vreinterpret_s64_s16(vget_low_s16(q_11)), 0) ||
               vget_lane_s64(vreinterpret_s64_s16(vget_high_s16(q_11)), 0))
            {
                *(csbf + block_col) = 1;
            }

            if(*(csbf + block_col) == 1)
            {
                temp_zero_col |= (0xF << block_col * 4);
                temp_zero_row |= (0xF << block_row);

                /* inverse quantization */
                iqtmp_0 = vmull_s16(vget_low_s16(q_10), siq);
                iqtmp_1 = vmull_s16(vget_high_s16(q_10), siq);
                iqtmp_2 = vmull_s16(vget_low_s16(q_11), siq);
                iqtmp_3 = vmull_s16(vget_high_s16(q_11), siq);

                iqtmp_0 = vaddq_s32(iqtmp_0, add_iq);
                iqtmp_1 = vaddq_s32(iqtmp_1, add_iq);
                iqtmp_2 = vaddq_s32(iqtmp_2, add_iq);
                iqtmp_3 = vaddq_s32(iqtmp_3, add_iq);

                iqtmp_0 = vshlq_s32(iqtmp_0, sh_iq);
                iqtmp_1 = vshlq_s32(iqtmp_1, sh_iq);
                iqtmp_2 = vshlq_s32(iqtmp_2, sh_iq);
                iqtmp_3 = vshlq_s32(iqtmp_3, sh_iq);

                /* clip */
                iq0 = vqmovn_s32(iqtmp_0);
                iq1 = vqmovn_s32(iqtmp_1);
                iq2 = vqmovn_s32(iqtmp_2);
                iq3 = vqmovn_s32(iqtmp_3);

                /* store */
                vst1_s16((pi2_iq_dst + j), iq0);
                vst1_s16((pi2_iq_dst + j + dst_iq_strd), iq1);
                vst1_s16((pi2_iq_dst + j + (2 * dst_iq_strd)), iq2);
                vst1_s16((pi2_iq_dst + j + (3 * dst_iq_strd)), iq3);

                /* ssd */
                /* trans_coeff - inv.quant */
                r0 = vsubl_s16(s0, iq0);
                r1 = vsubl_s16(s1, iq1);
                r2 = vsubl_s16(s2, iq2);
                r3 = vsubl_s16(s3, iq3);

                /* SD */
                r0 = vmulq_s32(r0, r0);
                r1 = vmulq_s32(r1, r1);
                r2 = vmulq_s32(r2, r2);
                r3 = vmulq_s32(r3, r3);
            }
            else
            {
                /* store */
                vst1_s16((pi2_iq_dst + j), zero_d);
                vst1_s16((pi2_iq_dst + j + dst_iq_strd), zero_d);
                vst1_s16((pi2_iq_dst + j + (2 * dst_iq_strd)), zero_d);
                vst1_s16((pi2_iq_dst + j + (3 * dst_iq_strd)), zero_d);

                /* SD */
                r0 = vmull_s16(s0, s0);
                r1 = vmull_s16(s1, s1);
                r2 = vmull_s16(s2, s2);
                r3 = vmull_s16(s3, s3);
            }

            /* SSD */
            r0 = vaddq_s32(r0, r1);
            r2 = vaddq_s32(r2, r3);

            r0 = vaddq_s32(r0, r2);

            /* SSD Accumulation */
            ssd0 = vaddq_s32(ssd0, r0);

            cbf = cbf || (*(csbf + block_col));  // cbf update
            block_col++;
        }

        block_col = 0;
        block_row += 4;
        csbf += csbf_strd;

        pi2_coeffs += 4 * src_strd;
        pi2_q_dst += 4 * dst_q_strd;
        pi2_iq_dst += 4 * dst_iq_strd;
        pi2_quant_coeff += 4 * trans_size;
        pi4_quant_round_factor_1_2 += 4 * trans_size;
        pi4_quant_round_factor_0_1 += 4 * trans_size;
    }

    /* SSD Computation */
    ssd1 = vpadd_s32(vget_low_s32(ssd0), vget_high_s32(ssd0));
    ssd1 = vpadd_s32(ssd1, ssd1);
    ssd = vget_lane_s32(ssd1, 0);

    *zero_col = ~temp_zero_col;  //final zero_col storing
    *zero_row = ~temp_zero_row;  //final zero_row storing

    /* Store the cost */
    *pi8_cost = ssd;

    return cbf;
}
