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
*  ihevce_scan_coeffs_neon.c
*
* @brief
*  Contains definitions for scanning quantized tu
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
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_defs.h"
#include "ihevc_debug.h"
#include "ihevce_api.h"
#include "ihevce_defs.h"
#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_structs.h"
#include "ihevce_multi_thrd_structs.h"

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
#include "ihevce_function_selector.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_common_utils.h"

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
FT_SCAN_COEFFS ihevce_scan_coeffs_neon;

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
static WORD32 movemask_neon(uint8x16_t input)
{
    const int8_t __attribute__((aligned(16))) xr[8] = { -7, -6, -5, -4, -3, -2, -1, 0 };
    uint8x8_t mask_and = vdup_n_u8(0x80);
    int8x8_t mask_shift = vld1_s8(xr);

    uint8x8_t lo = vget_low_u8(input);
    uint8x8_t hi = vget_high_u8(input);

    lo = vand_u8(lo, mask_and);
    lo = vshl_u8(lo, mask_shift);

    hi = vand_u8(hi, mask_and);
    hi = vshl_u8(hi, mask_shift);

    lo = vpadd_u8(lo, lo);
    lo = vpadd_u8(lo, lo);
    lo = vpadd_u8(lo, lo);

    hi = vpadd_u8(hi, hi);
    hi = vpadd_u8(hi, hi);
    hi = vpadd_u8(hi, hi);

    return ((hi[0] << 8) | (lo[0] & 0xFF));
}

WORD32 ihevce_scan_coeffs_neon(
    WORD16 *pi2_quant_coeffs,
    WORD32 *pi4_subBlock2csbfId_map,
    WORD32 scan_idx,
    WORD32 trans_size,
    UWORD8 *pu1_out_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 i4_csbf_stride)
{
    WORD32 i, trans_unit_idx, num_gt1_flag, num_gt0_flag;
    UWORD16 u2_csbf0flags;
    WORD32 num_bytes = 0;
    UWORD8 *pu1_trans_table;
    UWORD8 *pu1_csb_table;
    WORD32 shift_value, mask_value;
    WORD32 blk_row, blk_col;
    WORD32 x_pos, y_pos;
    WORD32 quant_coeff;

    UWORD8 *pu1_out_data_header;
    UWORD16 *pu2_out_data_coeff;

    int8x16_t one, shuffle, zero;
    int16x8_t ones;
    int8x8x2_t quant;

    (void)i4_csbf_stride;
    pu1_out_data_header = pu1_out_data;
    u2_csbf0flags = 0xBAD0;

    pu1_csb_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);

    GETRANGE(shift_value, trans_size);
    shift_value = shift_value - 3;
    mask_value = (trans_size / 4) - 1;

    switch(trans_size)
    {
    case 32:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_8x8[scan_idx][0]);
        break;
    case 16:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);
        break;
    case 8:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_2x2[scan_idx][0]);
        break;
    case 4:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_1x1[0]);
        break;
    }

    shuffle = vld1q_s8((WORD8 *)pu1_csb_table);
    zero = vdupq_n_s8(0);
    one = vdupq_n_s8(1);
    ones = vdupq_n_s16(1);

    for(trans_unit_idx = (trans_size * trans_size / 16) - 1; trans_unit_idx >= 0; trans_unit_idx--)
    {
        if(pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[trans_unit_idx]]])
        {
            WORD32 sig_coeff_abs_gt0_flags, sig_coeff_abs_gt1_flags;
            WORD32 sign_flag, pos_last_coded;
            UWORD8 u1_last_x, u1_last_y;
            WORD16 *pi2_temp_quant_coeff = pi2_quant_coeffs;

            int16x4_t quant0, quant1, quant2, quant3;
            int16x8_t quant01, quant23;
            int8x8_t a, b, c, d, shuffle_0, shuffle_1;
            int8x16_t shuffle_out, shuffle_out_abs;
            uint8x16_t sign, eq0, eq1;

            blk_row = pu1_trans_table[trans_unit_idx] >> shift_value;
            blk_col = pu1_trans_table[trans_unit_idx] & mask_value;

            pi2_temp_quant_coeff += (blk_col * 4 + (blk_row * 4) * trans_size);

            quant0 = vld1_s16(pi2_temp_quant_coeff + 0 * trans_size);
            quant1 = vld1_s16(pi2_temp_quant_coeff + 1 * trans_size);
            quant2 = vld1_s16(pi2_temp_quant_coeff + 2 * trans_size);
            quant3 = vld1_s16(pi2_temp_quant_coeff + 3 * trans_size);

            quant01 = vcombine_s16(quant0, quant1);
            quant23 = vcombine_s16(quant2, quant3);

            a = vqmovn_s16(quant01);
            b = vqmovn_s16(quant23);

            quant.val[0] = a;
            quant.val[1] = b;

            c = vget_low_s8(shuffle);
            d = vget_high_s8(shuffle);

            shuffle_0 = vtbl2_s8(quant, c);
            shuffle_1 = vtbl2_s8(quant, d);
            shuffle_out = vcombine_s8(shuffle_0, shuffle_1);

            shuffle_out_abs = vabsq_s8(shuffle_out);

            sign = vcgtq_s8(zero, shuffle_out);
            eq0 = vceqq_s8(shuffle_out, zero);
            eq1 = vceqq_s8(shuffle_out_abs, one);

            sign_flag = movemask_neon(sign);
            sig_coeff_abs_gt0_flags = movemask_neon(eq0);
            sig_coeff_abs_gt1_flags = movemask_neon(eq1);

            sig_coeff_abs_gt0_flags = ~sig_coeff_abs_gt0_flags;
            sig_coeff_abs_gt1_flags = ~sig_coeff_abs_gt1_flags;
            sig_coeff_abs_gt0_flags = sig_coeff_abs_gt0_flags & 0x0000FFFF;
            sig_coeff_abs_gt1_flags = sig_coeff_abs_gt1_flags & sig_coeff_abs_gt0_flags;

            ASSERT(sig_coeff_abs_gt0_flags != 0);
            GET_POS_MSB_32(pos_last_coded, sig_coeff_abs_gt0_flags);

            /* Update gt1 flag based on num_gt0_flag */
            num_gt0_flag = ihevce_num_ones_popcnt(sig_coeff_abs_gt0_flags);

            /* Find the position of 9th(MAX_GT_ONE+1) 1 in sig_coeff_abs_gt0_flags from MSB and update gt1 flag */
            if(num_gt0_flag > MAX_GT_ONE)
            {
                WORD32 gt0_first_byte = sig_coeff_abs_gt0_flags & 0xFF;
                WORD32 num_gt0_second_byte =
                    ihevce_num_ones_popcnt(sig_coeff_abs_gt0_flags & 0xFF00);
                WORD32 pos_nineth_one; /* pos. of 9th one from MSB of sig_coeff_abs_gt0_flags */
                WORD32 gt0_after_nineth_one, num_gt0_first_byte_to_nine;

                num_gt0_first_byte_to_nine = (MAX_GT_ONE + 1) - num_gt0_second_byte;

                while(num_gt0_first_byte_to_nine)
                {
                    GET_POS_MSB_32(pos_nineth_one, gt0_first_byte);
                    gt0_first_byte = CLEAR_BIT(
                        gt0_first_byte,
                        pos_nineth_one); /*gt0_second_byte &= (~(0x1<<pos_eighth_one));*/
                    num_gt0_first_byte_to_nine--;
                }

                /* Update gt1 based on pos_eighth_one */
                gt0_after_nineth_one = SET_BIT(gt0_first_byte, pos_nineth_one);
                sig_coeff_abs_gt1_flags = sig_coeff_abs_gt1_flags | gt0_after_nineth_one;
            }

            /* Get x_pos & y_pos of last coded in csb wrt to TU */
            u1_last_x = (pu1_csb_table[pos_last_coded] & 0x3) + blk_col * 4;
            u1_last_y = (pu1_csb_table[pos_last_coded] >> 2) + blk_row * 4;

            num_gt1_flag = ihevce_num_ones_popcnt(sig_coeff_abs_gt1_flags);

            /* storing last_x and last_y */
            *pu1_out_data_header = u1_last_x;
            pu1_out_data_header++;

            *pu1_out_data_header = u1_last_y;
            pu1_out_data_header++;

            /* storing the scan order */
            *pu1_out_data_header = (UWORD8)scan_idx;
            pu1_out_data_header++;

            /* storing last_sub_block pos. in scan order count */
            *pu1_out_data_header = (UWORD8)trans_unit_idx;
            pu1_out_data_header++;

            /*stored the first 4 bytes, now all are word16. So word16 pointer*/
            pu2_out_data_coeff = (UWORD16 *)pu1_out_data_header;

            /* u2_csbf0flags word */
            u2_csbf0flags = 0xBAD0 | 1; /*since right&bottom csbf is 0*/
            /* storing u2_csbf0flags word */
            *pu2_out_data_coeff = u2_csbf0flags;
            pu2_out_data_coeff++;

            /* storing u2_sig_coeff_abs_gt0_flags 2 bytes */
            *pu2_out_data_coeff = (UWORD16)sig_coeff_abs_gt0_flags;
            pu2_out_data_coeff++;

            /* storing u2_sig_coeff_abs_gt1_flags 2 bytes */
            *pu2_out_data_coeff = (UWORD16)sig_coeff_abs_gt1_flags;
            pu2_out_data_coeff++;

            /* storing u2_sign_flags 2 bytes */
            *pu2_out_data_coeff = (UWORD16)sign_flag;
            pu2_out_data_coeff++;

            /* Store the u2_abs_coeff_remaining[] */
            for(i = 0; i < num_gt1_flag; i++)
            {
                volatile WORD32 bit_pos;
                ASSERT(sig_coeff_abs_gt1_flags != 0);

                GET_POS_MSB_32(bit_pos, sig_coeff_abs_gt1_flags);
                sig_coeff_abs_gt1_flags = CLEAR_BIT(
                    sig_coeff_abs_gt1_flags,
                    bit_pos); /*sig_coeff_abs_gt1_flags &= (~(0x1<<bit_pos));*/

                x_pos = (pu1_csb_table[bit_pos] & 0x3);
                y_pos = (pu1_csb_table[bit_pos] >> 2);

                quant_coeff = pi2_temp_quant_coeff[x_pos + (y_pos * trans_size)];

                /* storing u2_abs_coeff_remaining[i] 2 bytes */
                *pu2_out_data_coeff = (UWORD16)abs(quant_coeff) - 1;
                pu2_out_data_coeff++;
            }

            break; /*We just need this loop for finding 1st non-zero csb only*/
        }
    }

    /* go through remaining csb in the scan order */
    for(trans_unit_idx = trans_unit_idx - 1; trans_unit_idx >= 0; trans_unit_idx--)
    {
        blk_row = pu1_trans_table[trans_unit_idx] >> shift_value; /*row of csb*/
        blk_col = pu1_trans_table[trans_unit_idx] & mask_value; /*col of csb*/

        /* u2_csbf0flags word */
        u2_csbf0flags = 0xBAD0 | /* assuming csbf_buf has only 0 or 1 values */
                        (pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[trans_unit_idx]]]);

        /********************************************************************/
        /* Minor hack: As per HEVC spec csbf in not signalled in stream for */
        /* block0, instead sig coeff map is directly signalled. This is     */
        /* taken care by forcing csbf for block0 to be 1 even if it is 0    */
        /********************************************************************/
        if(0 == trans_unit_idx)
        {
            u2_csbf0flags |= 1;
        }

        if((blk_col + 1 < trans_size / 4)) /* checking right boundary */
        {
            if(pu1_csbf_buf[pi4_subBlock2csbfId_map[blk_row * trans_size / 4 + blk_col + 1]])
            {
                /* set the 2nd bit of u2_csbf0flags for right csbf */
                u2_csbf0flags = u2_csbf0flags | (1 << 1);
            }
        }
        if((blk_row + 1 < trans_size / 4)) /* checking bottom oundary */
        {
            if(pu1_csbf_buf[pi4_subBlock2csbfId_map[(blk_row + 1) * trans_size / 4 + blk_col]])
            {
                /* set the 3rd bit of u2_csbf0flags  for bottom csbf */
                u2_csbf0flags = u2_csbf0flags | (1 << 2);
            }
        }

        /* storing u2_csbf0flags word */
        *pu2_out_data_coeff = u2_csbf0flags;
        pu2_out_data_coeff++;

        /* check for the csb flag in our scan order */
        if(u2_csbf0flags & 0x1)
        {
            WORD32 sig_coeff_abs_gt0_flags, sig_coeff_abs_gt1_flags;
            WORD32 sign_flag;

            int16x4_t quant0, quant1, quant2, quant3;
            int16x8_t quant01, quant23;
            int8x8_t a, b, c, d, shuffle_0, shuffle_1;
            int8x16_t shuffle_out, shuffle_out_abs;
            uint8x16_t sign, eq0, eq1;

            /* x_pos=blk_col*4, y_pos=blk_row*4 */
            WORD16 *pi2_temp_quant_coeff =
                pi2_quant_coeffs + blk_col * 4 + (blk_row * 4) * trans_size;

            /* Load Quant Values */
            quant0 = vld1_s16(pi2_temp_quant_coeff + 0 * trans_size);
            quant1 = vld1_s16(pi2_temp_quant_coeff + 1 * trans_size);
            quant2 = vld1_s16(pi2_temp_quant_coeff + 2 * trans_size);
            quant3 = vld1_s16(pi2_temp_quant_coeff + 3 * trans_size);

            /* Two quant rows together */
            quant01 = vcombine_s16(quant0, quant1);
            quant23 = vcombine_s16(quant2, quant3);

            /* All 4 rows: For sign, gt0, gt1 flags, even 8 bit version is enough! */
            a = vqmovn_s16(quant01);
            b = vqmovn_s16(quant23);

            quant.val[0] = a;
            quant.val[1] = b;

            c = vget_low_s8(shuffle);
            d = vget_high_s8(shuffle);

            shuffle_0 = vtbl2_s8(quant, c);
            shuffle_1 = vtbl2_s8(quant, d);
            shuffle_out = vcombine_s8(shuffle_0, shuffle_1);

            /* ABS values */
            shuffle_out_abs = vabsq_s8(shuffle_out);

            /* sign bits : Will get 0xFF if (0 > shuffle_out) */
            sign = vcgtq_s8(zero, shuffle_out);
            /* gt0 : Will get 0xFF if ( shuffle_out == 0 ) */
            eq0 = vceqq_s8(shuffle_out, zero);
            /* gt1 : Will get 0xFF if ( abs(shuffle_out) == 1 ) */
            eq1 = vceqq_s8(shuffle_out_abs, one);

            /* movemask:0 extended upper 16bits,Only low16 bits are required while storing */
            sign_flag = movemask_neon(sign);
            sig_coeff_abs_gt0_flags = movemask_neon(eq0);
            sig_coeff_abs_gt1_flags = movemask_neon(eq1);

            /* Update gt0 and gt1 based on ==0 and ==1 flag */
            sig_coeff_abs_gt0_flags = ~sig_coeff_abs_gt0_flags; /* != 0 */
            sig_coeff_abs_gt1_flags = ~sig_coeff_abs_gt1_flags; /* (abs) != 1 */
            sig_coeff_abs_gt0_flags = sig_coeff_abs_gt0_flags & 0x0000FFFF; /* Clear high Word */
            sig_coeff_abs_gt1_flags = sig_coeff_abs_gt1_flags & sig_coeff_abs_gt0_flags;

            /* Update gt1 flag based on num_gt0_flag */
            num_gt0_flag = ihevce_num_ones_popcnt(sig_coeff_abs_gt0_flags);

            /* Find the position of 9th(MAX_GT_ONE+1) 1 in sig_coeff_abs_gt0_flags from MSB and update gt1 flag */
            if(num_gt0_flag > MAX_GT_ONE)
            {
                WORD32 gt0_first_byte = sig_coeff_abs_gt0_flags & 0xFF;
                WORD32 num_gt0_second_byte =
                    ihevce_num_ones_popcnt(sig_coeff_abs_gt0_flags & 0xFF00);
                WORD32 pos_nineth_one; /* pos. of 9th one from MSB of sig_coeff_abs_gt0_flags */
                WORD32 gt0_after_nineth_one, num_gt0_first_byte_to_nine;

                num_gt0_first_byte_to_nine = (MAX_GT_ONE + 1) - num_gt0_second_byte;

                while(num_gt0_first_byte_to_nine)
                {
                    GET_POS_MSB_32(pos_nineth_one, gt0_first_byte);
                    gt0_first_byte = CLEAR_BIT(
                        gt0_first_byte,
                        pos_nineth_one); /*gt0_second_byte &= (~(0x1<<pos_eighth_one));*/
                    num_gt0_first_byte_to_nine--;
                }

                /* Update gt1 based on pos_eighth_one */
                gt0_after_nineth_one = SET_BIT(gt0_first_byte, pos_nineth_one);
                sig_coeff_abs_gt1_flags = sig_coeff_abs_gt1_flags | gt0_after_nineth_one;
            }

            num_gt1_flag = ihevce_num_ones_popcnt(sig_coeff_abs_gt1_flags);

            /* storing u2_sig_coeff_abs_gt0_flags 2 bytes */
            *pu2_out_data_coeff = (UWORD16)sig_coeff_abs_gt0_flags;
            pu2_out_data_coeff++;

            /* storing u2_sig_coeff_abs_gt1_flags 2 bytes */
            *pu2_out_data_coeff = (UWORD16)sig_coeff_abs_gt1_flags;
            pu2_out_data_coeff++;

            /* storing u2_sign_flags 2 bytes */
            *pu2_out_data_coeff = (UWORD16)sign_flag;
            pu2_out_data_coeff++;

            /* Store the u2_abs_coeff_remaining[] */
            for(i = 0; i < num_gt1_flag; i++)
            {
                volatile WORD32 bit_pos;
                ASSERT(sig_coeff_abs_gt1_flags != 0);

                GET_POS_MSB_32(bit_pos, sig_coeff_abs_gt1_flags);
                sig_coeff_abs_gt1_flags = CLEAR_BIT(
                    sig_coeff_abs_gt1_flags,
                    bit_pos); /*sig_coeff_abs_gt1_flags &= (~(0x1<<bit_pos));*/

                x_pos = (pu1_csb_table[bit_pos] & 0x3);
                y_pos = (pu1_csb_table[bit_pos] >> 2);

                quant_coeff = pi2_temp_quant_coeff[x_pos + (y_pos * trans_size)];

                /* storing u2_abs_coeff_remaining[i] 2 bytes */
                *pu2_out_data_coeff = (UWORD16)abs(quant_coeff) - 1;
                pu2_out_data_coeff++;
            }
        }
    }

    num_bytes = (UWORD8 *)pu2_out_data_coeff - pu1_out_data;
    return num_bytes; /* Return the number of bytes written to out_data */
}
