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
 *  ihevc_quant_iquant_ssd.c
 *
 * @brief
 *  Contains function definitions for quantization, followed by Inverse
 *  quantization to find transform domain SSD
 *
 * @author
 *  100453, 100578
 *
 * @par List of Functions:
 *   - ihevc_quant_iquant_ssd()
 *   - ihevc_quant_iquant_ssd_flat_scale_mat()
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ihevc_typedefs.h"
#include "ihevc_macros.h"
#include "ihevc_platform_macros.h"
#include "ihevc_defs.h"
#include "ihevc_debug.h"
#include "ihevc_trans_tables.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_func_selector.h"
#include "ihevc_trans_macros.h"
#include <assert.h>

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/


/**
 *******************************************************************************
 *
 * @brief
 *  This function performs quantization, followed by Inverse
 *  quantization to find transform domain SSD
 *
 * @par Description:
 *  Performs quantization on coeffs
 *
 * @param[in] pi2_coeffs
 *  4x4 Coeffs
 *
 * @param[in] pi2_quant_coeff
 *  Scaling Matrix
 *
 * @param[out] pi2_dst
 *  Output 4x4 coefficients
 *
 * @param[in] qp_div
 *  Quantization parameter / 6
 *
 * @param[in] qp_rem
 *  Quantization parameter % 6
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[out] csbf
 *  coded sub block flag
 *
 * @param[in] csbf_strd
 *  coded sub block flag
 *
 * @param[out] zero_col
 *  zero column flag
 *
 * @param[out] zero_row
 *  zero column flag
 *
 * @returns  cbf
 * coded block flag
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

WORD32 ihevc_quant_iquant_ssd
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD32 val;
    WORD16 i2_temp;
    WORD32 ssd_cost = 0;

    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*  Quantization    */
            QUANT(pi2_q_dst[j], pi2_coeffs[j],
                  pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                  log2_size, q_add);

            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                   pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                   pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                   /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                   shift_iq,
                   qp_div);

            /*  SSD Computation & Accumulation  */
            val = i2_temp - pi2_iq_dst[j];
            ssd_cost += val*val;

        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }

    /* Store the cost */
    *pi8_cost = ssd_cost;

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs quantization, followed by Inverse
 *  quantization
 *
 * @par Description:
 *  Performs quantization on coeffs
 *
 * @param[in] pi2_coeffs
 *  4x4 Coeffs
 *
 * @param[in] pi2_quant_coeff
 *  Scaling Matrix
 *
 * @param[out] pi2_dst
 *  Output 4x4 coefficients
 *
 * @param[in] qp_div
 *  Quantization parameter / 6
 *
 * @param[in] qp_rem
 *  Quantization parameter % 6
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[out] csbf
 *  coded sub block flag
 *
 * @param[in] csbf_strd
 *  coded sub block flag
 *
 * @param[out] zero_col
 *  zero column flag
 *
 * @param[out] zero_row
 *  zero column flag
 *
 * @returns  cbf
 * coded block flag
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

WORD32 ihevc_quant_iquant
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD16 i2_temp;

    (void)pi8_cost;
    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*  Quantization    */
            QUANT(pi2_q_dst[j], pi2_coeffs[j],
                  pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                  log2_size, q_add);

            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                   pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                   pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                   shift_iq,
                   qp_div);
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }

            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs quantization, followed by Inverse
 *  quantization to find transform domain SSD
 *
 * @par Description:
 *  Performs quantization on coeffs
 *
 * @param[in] pi2_coeffs
 *  4x4 Coeffs
 *
 * @param[in] pi2_quant_coeff
 *  Scaling Matrix
 *
 * @param[out] pi2_dst
 *  Output 4x4 coefficients
 *
 * @param[in] qp_div
 *  Quantization parameter / 6
 *
 * @param[in] qp_rem
 *  Quantization parameter % 6
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[out] csbf
 *  coded sub block flag
 *
 * @param[in] csbf_strd
 *  coded sub block flag
 *
 * @param[out] zero_col
 *  zero column flag
 *
 * @param[out] zero_row
 *  zero column flag
 *
 * @returns  cbf
 * coded block flag
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

WORD32 ihevc_quant_iquant_ssd_rdoq
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD32 val;
    WORD16 i2_temp;
    WORD32 ssd_cost = 0;

    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*  Quantization    */
            QUANT(pi2_q_dst[j], pi2_coeffs[j],
                pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                log2_size, q_add);


            if (abs(pi2_q_dst[j]) > 1)
            {
                QUANT(pi2_q_dst[j],i2_temp,
                    pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));

            }


            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                shift_iq,
                qp_div);

            /*  SSD Computation & Accumulation  */
            val = i2_temp - pi2_iq_dst[j];
            ssd_cost += val*val;

        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }
    /* Store the cost */
    *pi8_cost = ssd_cost;

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

WORD32 ihevc_quant_iquant_rdoq
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD16 i2_temp;

    (void)pi8_cost;
    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*  Quantization    */
            QUANT(pi2_q_dst[j], pi2_coeffs[j],
                pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                log2_size, q_add);

            if (abs(pi2_q_dst[j]) > 1)
            {
                QUANT(pi2_q_dst[j],i2_temp,
                    pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));
            }

            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                shift_iq,
                qp_div);
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs quantization(using flat scale matrix), followed by
 *  inverse quantization to find transform domain SSD
 *
 * @par Description:
 *  Performs quantization on coeffs
 *
 * @param[in] pi2_coeffs
 *  4x4 Coeffs
 *
 * @param[in] pi2_quant_coeff
 *  Scaling Matrix
 *
 * @param[out] pi2_dst
 *  Output 4x4 coefficients
 *
 * @param[in] qp_div
 *  Quantization parameter / 6
 *
 * @param[in] qp_rem
 *  Quantization parameter % 6
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[out] csbf
 *  coded sub block flag
 *
 * @param[in] csbf_strd
 *  coded sub block flag
 *
 * @param[out] zero_col
 *  zero column flag
 *
 * @param[out] zero_row
 *  zero column flag
 *
 * @returns  cbf
 * coded block flag
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

WORD32 ihevc_quant_iquant_ssd_flat_scale_mat
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD32 val;
    WORD16 i2_temp;
    /* Initialize cost to zero */
    WORD32 ssd_cost = 0;

    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*QUANT(pi2_dst[j], pi2_coeffs[j],
            pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
            log2_size, q_add);*/

            /* modified by 1028 */
            /*  Quantization    */
            QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                  g_ihevc_quant_scales[qp_rem], qp_div,
                  log2_size, q_add);

            if(pi2_q_dst[j] == 0)
            {
                pi2_iq_dst[j] = 0;
            }
            else
            {
            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                    pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                    pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem], /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                    shift_iq,
                    qp_div);
            }

            /*  SSD Computation & Accumulation  */
            val = i2_temp - pi2_iq_dst[j];
            ssd_cost += val*val;

        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }
    /* Store the cost */
    *pi8_cost = ssd_cost;

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

WORD32 ihevc_quant_iquant_flat_scale_mat
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD16 i2_temp;

    (void)pi8_cost;
    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*  Quantization    */
            QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                  g_ihevc_quant_scales[qp_rem], qp_div,
                  log2_size, q_add);

            if(pi2_q_dst[j] == 0)
            {
                pi2_iq_dst[j] = 0;
            }
            else
            {
            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                    pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                    pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem], /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                    shift_iq,
                    qp_div);
            }
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

/**
 *******************************************************************************
 *
 * @brief
 *  This function performs quantization(using flat scale matrix), followed by
 *  inverse quantization to find transform domain SSD; when we perform RDOQ.
 *  In case the quantized value turns out to be grater than 1, we then requantize
 *  use half rounding.
 *
 * @par Description:
 *  Performs quantization on coeffs
 *
 * @param[in] pi2_coeffs
 *  4x4 Coeffs
 *
 * @param[in] pi2_quant_coeff
 *  Scaling Matrix
 *
 * @param[out] pi2_dst
 *  Output 4x4 coefficients
 *
 * @param[in] qp_div
 *  Quantization parameter / 6
 *
 * @param[in] qp_rem
 *  Quantization parameter % 6
 *
 * @param[in] src_strd
 *  Input stride
 *
 * @param[in] dst_strd
 *  Output Stride
 *
 * @param[out] csbf
 *  coded sub block flag
 *
 * @param[in] csbf_strd
 *  coded sub block flag
 *
 * @param[out] zero_col
 *  zero column flag
 *
 * @param[out] zero_row
 *  zero column flag
 *
 * @returns  cbf
 * coded block flag
 *
 * @remarks
 *  None
 *
 *******************************************************************************
 */

WORD32 ihevc_quant_iquant_ssd_flat_scale_mat_rdoq
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD32 val;
    WORD16 i2_temp;
    /* Initialize cost to zero */
    WORD32 ssd_cost = 0;

    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            WORD16 i2_temp1;
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*QUANT(pi2_dst[j], pi2_coeffs[j],
            pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
            log2_size, q_add);*/

            /* modified by 1028 */
            /*  Quantization    */

            if (1)
            {
                QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                  g_ihevc_quant_scales[qp_rem], qp_div,
                  log2_size, q_add);
            }
            else
            {                                                                                                                                                                \
                WORD16 inp = pi2_coeffs[j],out = pi2_q_dst[j];
                WORD32 quant_coeff = g_ihevc_quant_scales[qp_rem];
                WORD32 log2_trans_size = log2_size;
                WORD32 tmp;                                                                                                                                                  \
                WORD32 sign;                                                                                                                                                 \
                WORD32 bit_depth,transform_shift;                                                                                                                            \
                WORD32  q_bits, quant_multiplier;                                                                                                                            \
                                                                                                                                                                                \
                /* q_bits and q_add calculation*/                                                                                                                            \
                /* To be moved outside in neon. To be computer once per transform call */                                                                                    \
                bit_depth = 8;                                                                                                                                               \
                transform_shift = MAX_TR_DYNAMIC_RANGE - bit_depth - log2_trans_size;                                                                                        \
                quant_multiplier = 4 ; /* because quant_coeff are multiplied by 16. Instead of multiplying, we can reduce the division factor q_bits by 4 */                 \
                q_bits = QUANT_SHIFT + qp_div + transform_shift + SCALING_Q_SHIFT - quant_multiplier - FLAT_RESCALE_MAT_Q_SHIFT /* 2048 */;                                                                       \
                                                                                                                                                                                \
                sign = (inp)<0 ? -1:1;                                                                                                                                       \
                                                                                                                                                                                \
                tmp = (WORD32)(abs(inp));                                                                                                                                    \
                tmp = tmp * (quant_coeff);                                                                                                                                   \
                tmp = tmp + (((WORD32)q_add) << (q_bits - QUANT_ROUND_FACTOR_Q));                                                                                            \
                tmp = tmp >> q_bits;                                                                                                                                         \
                                                                                                                                                                                \
                tmp = tmp * sign;                                                                                                                                            \
                out = (WORD16) CLIP_S16(tmp);                                                                                                                                \
            }
            i2_temp1 = pi2_q_dst[j];
            if (abs(pi2_q_dst[j]) > 1)
            {
                QUANT_NO_WEIGHTMAT(pi2_q_dst[j], i2_temp,
                  g_ihevc_quant_scales[qp_rem], qp_div,
                  log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));
            }


            ASSERT(abs(i2_temp1-pi2_q_dst[j]) <= 1);
            ASSERT(abs(i2_temp1) <= abs(pi2_q_dst[j]));


            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                    pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                    pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem], /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                    shift_iq,
                    qp_div);

            /*  SSD Computation & Accumulation  */
            val = i2_temp - pi2_iq_dst[j];
            ssd_cost += val*val;

        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;

    }
    /* Store the cost */
    *pi8_cost = ssd_cost;

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }
    return cbf;
}

WORD32 ihevc_quant_iquant_flat_scale_mat_rdoq
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD16 i2_temp;

    (void)pi8_cost;
    (void)pi4_quant_round_factor_0_1;
    (void)pi4_quant_round_factor_1_2;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            WORD16 i2_temp1;
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                g_ihevc_quant_scales[qp_rem], qp_div,
                log2_size, q_add);

            i2_temp1 = pi2_q_dst[j];

            if (abs(pi2_q_dst[j]) > 1)
            {
                QUANT_NO_WEIGHTMAT(pi2_q_dst[j], i2_temp,
                    g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));
            }

            ASSERT(abs(i2_temp1-pi2_q_dst[j]) <= 1);
            ASSERT(abs(i2_temp1) <= abs(pi2_q_dst[j]));

            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem], /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                shift_iq,
                qp_div);
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}


/**
*******************************************************************************
*
* @brief
*  This function performs quantization, followed by Inverse
*  quantization to find transform domain SSD
*
* @par Description:
*  Performs quantization on coeffs
*
* @param[in] pi2_coeffs
*  4x4 Coeffs
*
* @param[in] pi2_quant_coeff
*  Scaling Matrix
*
* @param[out] pi2_dst
*  Output 4x4 coefficients
*
* @param[in] qp_div
*  Quantization parameter / 6
*
* @param[in] qp_rem
*  Quantization parameter % 6
*
* @param[in] src_strd
*  Input stride
*
* @param[in] dst_strd
*  Output Stride
*
* @param[out] csbf
*  coded sub block flag
*
* @param[in] csbf_strd
*  coded sub block flag
*
* @param[out] zero_col
*  zero column flag
*
* @param[out] zero_row
*  zero column flag
*
* @returns  cbf
* coded block flag
*
* @remarks
*  None
*
*******************************************************************************
*/

WORD32 ihevc_q_iq_ssd_var_rnd_fact
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD32 val;
    WORD16 i2_temp;
    //WORD16 i2_temp_1;
    /* Initialize cost to zero */
    WORD32 ssd_cost = 0;

    (void)q_add;
    pi2_q_dst_orig  = pi2_q_dst;


    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];


            {
                /*  Quantization    */
                QUANT(pi2_q_dst[j],i2_temp,
                    pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, 0);
                if (abs(pi2_q_dst[j]) >= 2)
                {
                    QUANT(pi2_q_dst[j],i2_temp,
                        pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));

                }
                else if (abs(pi2_q_dst[j]) >= 1)
                {
                    QUANT(pi2_q_dst[j],i2_temp,
                        pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_1_2);
                }

                else
                {
                    /*  Quantization    */
                    QUANT(pi2_q_dst[j],i2_temp,
                        pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_0_1);
                }

            }



            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                shift_iq,
                qp_div);

            /*  SSD Computation & Accumulation  */
            val = i2_temp - pi2_iq_dst[j];
            ssd_cost += val*val;

            pi4_quant_round_factor_0_1++;
            pi4_quant_round_factor_1_2++;
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }
    /* Store the cost */
    *pi8_cost = ssd_cost;

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

WORD32 ihevc_q_iq_var_rnd_fact
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD16 i2_temp;

    (void)q_add;
    (void)pi8_cost;
    pi2_q_dst_orig  = pi2_q_dst;

    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            i2_temp = pi2_coeffs[j];

            {
                QUANT(pi2_q_dst[j],i2_temp,
                    pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, 0);

                if (abs(pi2_q_dst[j]) >= 2)
                {
                    QUANT(pi2_q_dst[j],i2_temp,
                        pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));
                }
                else if (abs(pi2_q_dst[j]) >= 1)
                {
                    QUANT(pi2_q_dst[j],i2_temp,
                        pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_1_2);
                }
                else
                {
                    QUANT(pi2_q_dst[j],i2_temp,
                        pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_0_1);
                }
            }

            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                shift_iq,
                qp_div);

            pi4_quant_round_factor_0_1++;
            pi4_quant_round_factor_1_2++;
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;
    }

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }

    return cbf;
}

/**
*******************************************************************************
*
* @brief
*  This function performs quantization(using flat scale matrix), followed by
*  inverse quantization to find transform domain SSD; when we perform RDOQ.
*  In case the quantized value turns out to be grater than 1, we then requantize
*  use half rounding.
*
* @par Description:
*  Performs quantization on coeffs
*
* @param[in] pi2_coeffs
*  4x4 Coeffs
*
* @param[in] pi2_quant_coeff
*  Scaling Matrix
*
* @param[out] pi2_dst
*  Output 4x4 coefficients
*
* @param[in] qp_div
*  Quantization parameter / 6
*
* @param[in] qp_rem
*  Quantization parameter % 6
*
* @param[in] src_strd
*  Input stride
*
* @param[in] dst_strd
*  Output Stride
*
* @param[out] csbf
*  coded sub block flag
*
* @param[in] csbf_strd
*  coded sub block flag
*
* @param[out] zero_col
*  zero column flag
*
* @param[out] zero_row
*  zero column flag
*
* @returns  cbf
* coded block flag
*
* @remarks
*  None
*
*******************************************************************************
*/

WORD32 ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD32 val;
    WORD16 i2_temp;
    /* Initialize cost to zero */
    WORD32 ssd_cost = 0;

    (void)q_add;
    pi2_q_dst_orig  = pi2_q_dst;

    /* Quant initialization */
    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            WORD16 i2_temp1;
            /*  Back up the coefficients before Quantization    */
            i2_temp = pi2_coeffs[j];

            /*QUANT(pi2_dst[j], pi2_coeffs[j],
            pi2_quant_coeff[j] * g_ihevc_quant_scales[qp_rem], qp_div,
            log2_size, q_add);*/

            /* modified by 1028 */
            /*  Quantization    */


            {
                QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                    g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, 0);

                i2_temp1 = pi2_q_dst[j];

                if (abs(pi2_q_dst[j]) >= 2)
                {
                    QUANT_NO_WEIGHTMAT(pi2_q_dst[j], i2_temp,
                        g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));
                }
                else if (abs(pi2_q_dst[j]) >= 1)
                {
                    QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                        g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_1_2);
                }

                else
                {
                    QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                        g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_0_1);
                }

            }




            ASSERT(abs(i2_temp1-pi2_q_dst[j]) <= 1);


            /*  Inverse Quantization    */
            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem], /*pi2_dequant_coeff[index*trans_size] * g_ihevc_iquant_scales[qp_rem] */
                shift_iq,
                qp_div);

            /*  SSD Computation & Accumulation  */
            val = i2_temp - pi2_iq_dst[j];
            ssd_cost += val*val;

            pi4_quant_round_factor_0_1++;
            pi4_quant_round_factor_1_2++;
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;

    }
    /* Store the cost */
    *pi8_cost = ssd_cost;

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }
    return cbf;
}

WORD32 ihevc_q_iq_flat_scale_mat_var_rnd_fact
    (
    WORD16 *pi2_coeffs,
    WORD16 *pi2_quant_coeff,
    WORD16 *pi2_q_dst,
    WORD16 *pi2_iq_dst,
    WORD32  trans_size,
    WORD32 qp_div,/* qpscaled / 6 */
    WORD32 qp_rem,/* qpscaled % 6 */
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
    LWORD64 *pi8_cost
    )
{
    WORD32 i, j;
    WORD32 log2_size;
    WORD16 *pi2_q_dst_orig;
    WORD32 cbf = 0;
    WORD32 bit_depth,shift_iq;
    WORD16 i2_temp;

    (void)q_add;
    (void)pi8_cost;
    pi2_q_dst_orig  = pi2_q_dst;

    GETRANGE(log2_size, trans_size);
    log2_size -= 1;

    bit_depth = 8 + 0;
    shift_iq = bit_depth + log2_size - 5;

    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            WORD16 i2_temp1;

            i2_temp = pi2_coeffs[j];

            {
                QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                    g_ihevc_quant_scales[qp_rem], qp_div,
                    log2_size, 0);

                i2_temp1 = pi2_q_dst[j];

                if (abs(pi2_q_dst[j]) >= 2)
                {
                    QUANT_NO_WEIGHTMAT(pi2_q_dst[j], i2_temp,
                        g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, ((1 << QUANT_ROUND_FACTOR_Q)/2));
                }
                else if (abs(pi2_q_dst[j]) >= 1)
                {
                    QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                        g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_1_2);
                }
                else
                {
                    QUANT_NO_WEIGHTMAT(pi2_q_dst[j], pi2_coeffs[j],
                        g_ihevc_quant_scales[qp_rem], qp_div,
                        log2_size, *pi4_quant_round_factor_0_1);
                }
            }

            ASSERT(abs(i2_temp1-pi2_q_dst[j]) <= 1);

            IQUANT(pi2_iq_dst[j],
                pi2_q_dst[j], /*pi2_src[index*src_strd]*/
                pi2_dequant_coeff[j]*g_ihevc_iquant_scales[qp_rem],
                shift_iq,
                qp_div);

            pi4_quant_round_factor_0_1++;
            pi4_quant_round_factor_1_2++;
        }

        pi2_q_dst   += dst_q_strd;
        pi2_iq_dst  += dst_iq_strd;
        pi2_quant_coeff += trans_size;
        pi2_coeffs += src_strd;
        pi2_dequant_coeff += trans_size;

    }

    /* CSBF update */
    {
        WORD32 block_row, block_col;
        WORD32 row, col;
        WORD16 *pi2_block;
        UWORD32 temp_zero_col = 0;
        UWORD32 temp_zero_row = 0;

        pi2_q_dst = pi2_q_dst_orig;

        for(block_row = 0; block_row < trans_size; block_row += 4)
        {
            //block_col is incrementing by 1 for easy update of csbf pointer
            for(block_col = 0; block_col < trans_size / 4; block_col++)
            {
                pi2_block = pi2_q_dst + block_row * dst_q_strd + block_col * 4;
                *(csbf + block_col) = 0;

                for(row = 0; row < 4; row++)
                {
                    for(col = 0; col < 4; col++)
                    {
                        if(pi2_block[row * dst_q_strd + col] != 0)
                        {
                            *(csbf + block_col) = 1;
                            break;
                        }
                    }
                    if(*(csbf + block_col) == 1)
                    {
                        /* zero_col update *//* temp_zero_col = ~zero_col */
                        temp_zero_col = (temp_zero_col) | (0xFU << block_col * 4);
                        // zero col can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 colums of 4x4 block
                        // even if any 4x4 csbf is set

                        /* zero row update */ /* temp_zero_row = ~zero_row */
                        temp_zero_row = (temp_zero_row) | (0xFU << block_row);
                        // zero row can be optimized further. Now clearing the
                        // entire 4 bits corresponding to 4 rows of 4x4 block
                        // even if any 4x4 csbf is set

                        break;
                    }
                }

                cbf = cbf || (*(csbf + block_col)); // cbf update
            }
            csbf += csbf_strd;
        }

        *zero_col = ~temp_zero_col; //final zero_col storing
        *zero_row = ~temp_zero_row; //final zero_row storing
    }
    return cbf;
}
