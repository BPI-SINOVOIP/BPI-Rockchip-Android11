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
*  ihevce_sad_compute_neon.c
*
* @brief
*  Contains definitions of functions to compute sad
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
#include "ihevc_macros.h"
#include "itt_video_api.h"
#include "ihevc_cmn_utils_neon.h"
#include "ihevce_ipe_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
UWORD16 ihevce_4x4_sad_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd)
{
    const uint8x16_t src_u8 = load_unaligned_u8q(pu1_src, src_strd);
    const uint8x16_t ref_u8 = load_unaligned_u8q(pu1_pred, pred_strd);
    uint16x8_t abs = vabdl_u8(vget_low_u8(src_u8), vget_low_u8(ref_u8));
    uint32x4_t b;
    uint64x2_t c;

    abs = vabal_u8(abs, vget_high_u8(src_u8), vget_high_u8(ref_u8));
    b = vpaddlq_u16(abs);
    c = vpaddlq_u32(b);
    return vget_lane_u32(
        vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)), vreinterpret_u32_u64(vget_high_u64(c))), 0);
}

static UWORD16 ihevce_8xn_sad_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd, WORD32 ht)
{
    uint16x8_t abs = vdupq_n_u16(0);
    uint32x4_t tmp_a;
    uint64x2_t tmp_b;
    uint32x2_t sad;
    WORD32 i;

    assert(ht <= 8);

    for(i = 0; i < ht; i++)
    {
        const uint8x8_t src = vld1_u8(pu1_src);
        const uint8x8_t pred = vld1_u8(pu1_pred);

        abs = vabal_u8(abs, src, pred);
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    tmp_a = vpaddlq_u16(abs);
    tmp_b = vpaddlq_u32(tmp_a);
    sad = vadd_u32(
        vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
    return vget_lane_u32(sad, 0);
}

static UWORD32 ihevce_16xn_sad_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd, WORD32 ht)
{
    uint16x8_t abs_0 = vdupq_n_u16(0);
    uint16x8_t abs_1 = vdupq_n_u16(0);
    uint32x4_t tmp_a;
    uint64x2_t tmp_b;
    uint32x2_t sad;
    WORD32 i;

    assert(ht <= 16);

    for(i = 0; i < ht; i++)
    {
        const uint8x16_t src = vld1q_u8(pu1_src);
        const uint8x16_t pred = vld1q_u8(pu1_pred);

        abs_0 = vabal_u8(abs_0, vget_low_u8(src), vget_low_u8(pred));
        abs_1 = vabal_u8(abs_1, vget_high_u8(src), vget_high_u8(pred));
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    tmp_a = vpaddlq_u16(abs_0);
    tmp_a = vpadalq_u16(tmp_a, abs_1);
    tmp_b = vpaddlq_u32(tmp_a);
    sad = vadd_u32(
        vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
    return vget_lane_u32(sad, 0);
}

static UWORD32 ihevce_32xn_sad_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd, WORD32 ht)
{
    uint16x8_t abs_0 = vdupq_n_u16(0);
    uint16x8_t abs_1 = vdupq_n_u16(0);
    uint32x4_t tmp_a;
    uint64x2_t tmp_b;
    uint32x2_t sad;
    WORD32 i;

    assert(ht <= 32);

    for(i = 0; i < ht; i++)
    {
        const uint8x16_t src_0 = vld1q_u8(pu1_src);
        const uint8x16_t pred_0 = vld1q_u8(pu1_pred);
        const uint8x16_t src_1 = vld1q_u8(pu1_src + 16);
        const uint8x16_t pred_1 = vld1q_u8(pu1_pred + 16);

        abs_0 = vabal_u8(abs_0, vget_low_u8(src_0), vget_low_u8(pred_0));
        abs_0 = vabal_u8(abs_0, vget_high_u8(src_0), vget_high_u8(pred_0));
        abs_1 = vabal_u8(abs_1, vget_low_u8(src_1), vget_low_u8(pred_1));
        abs_1 = vabal_u8(abs_1, vget_high_u8(src_1), vget_high_u8(pred_1));
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    tmp_a = vpaddlq_u16(abs_0);
    tmp_a = vpadalq_u16(tmp_a, abs_1);
    tmp_b = vpaddlq_u32(tmp_a);
    sad = vadd_u32(
        vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
    return vget_lane_u32(sad, 0);
}

static UWORD32 ihevce_64xn_sad_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd, WORD32 ht)
{
    uint16x8_t abs_0 = vdupq_n_u16(0);
    uint16x8_t abs_1 = vdupq_n_u16(0);
    uint32x4_t tmp_a;
    uint64x2_t tmp_b;
    uint32x2_t sad;
    WORD32 i;

    assert(ht <= 64);

    for(i = 0; i < ht; i++)
    {
        const uint8x16_t src_0 = vld1q_u8(pu1_src);
        const uint8x16_t pred_0 = vld1q_u8(pu1_pred);
        const uint8x16_t src_1 = vld1q_u8(pu1_src + 16);
        const uint8x16_t pred_1 = vld1q_u8(pu1_pred + 16);
        const uint8x16_t src_2 = vld1q_u8(pu1_src + 32);
        const uint8x16_t pred_2 = vld1q_u8(pu1_pred + 32);
        const uint8x16_t src_3 = vld1q_u8(pu1_src + 48);
        const uint8x16_t pred_3 = vld1q_u8(pu1_pred + 48);

        abs_0 = vabal_u8(abs_0, vget_low_u8(src_0), vget_low_u8(pred_0));
        abs_0 = vabal_u8(abs_0, vget_high_u8(src_0), vget_high_u8(pred_0));
        abs_0 = vabal_u8(abs_0, vget_low_u8(src_1), vget_low_u8(pred_1));
        abs_0 = vabal_u8(abs_0, vget_high_u8(src_1), vget_high_u8(pred_1));
        abs_1 = vabal_u8(abs_1, vget_low_u8(src_2), vget_low_u8(pred_2));
        abs_1 = vabal_u8(abs_1, vget_high_u8(src_2), vget_high_u8(pred_2));
        abs_1 = vabal_u8(abs_1, vget_low_u8(src_3), vget_low_u8(pred_3));
        abs_1 = vabal_u8(abs_1, vget_high_u8(src_3), vget_high_u8(pred_3));
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    tmp_a = vpaddlq_u16(abs_0);
    tmp_a = vpadalq_u16(tmp_a, abs_1);
    tmp_b = vpaddlq_u32(tmp_a);
    sad = vadd_u32(
        vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
    return vget_lane_u32(sad, 0);
}

UWORD32 ihevce_4mx4n_sad_computer_neon(
    UWORD8 *pu1_src,
    UWORD8 *pu1_pred,
    WORD32 src_strd,
    WORD32 pred_strd,
    WORD32 blk_wd,
    WORD32 blk_ht)
{
    WORD32 sad = 0;
    WORD32 i, j;

    assert(blk_wd % 4 == 0);
    assert(blk_ht % 4 == 0);

    if(((blk_wd & (blk_wd - 1)) == 0) && (blk_wd <= 64))
    {
        // blk_wd { 4, 8, 16, 32, 64 }
        for(i = 0; i < blk_ht;)
        {
            WORD32 ht = MIN(blk_wd, blk_ht - i);

            switch(blk_wd)
            {
            case 4:
                sad += ihevce_4x4_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd);
                break;
            case 8:
                sad += ihevce_8xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, ht);
                break;
            case 16:
                sad += ihevce_16xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, ht);
                break;
            case 32:
                sad += ihevce_32xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, ht);
                break;
            case 64:
                sad += ihevce_64xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, ht);
                break;
            default:
                // should not be here
                return -1;
            }
            i += ht;
            pu1_src += (ht * src_strd);
            pu1_pred += (ht * pred_strd);
        }
    }
    else
    {
        // Generic Case
        for(i = 0; i < blk_ht; i += 4)
        {
            for(j = 0; j < blk_wd;)
            {
                WORD32 wd = blk_wd - j;

                if(wd >= 32)
                {
                    sad += ihevce_32xn_sad_computer_neon(
                        pu1_src + j, pu1_pred + j, src_strd, pred_strd, 4);
                    j += 32;
                }
                else if(wd >= 16)
                {
                    sad += ihevce_16xn_sad_computer_neon(
                        pu1_src + j, pu1_pred + j, src_strd, pred_strd, 4);
                    j += 16;
                }
                else if(wd >= 8)
                {
                    sad += ihevce_8xn_sad_computer_neon(
                        pu1_src + j, pu1_pred + j, src_strd, pred_strd, 4);
                    j += 8;
                }
                else
                {
                    sad += ihevce_4x4_sad_computer_neon(
                        pu1_src + j, pu1_pred + j, src_strd, pred_strd);
                    j += 4;
                }
            }
            pu1_src += (4 * src_strd);
            pu1_pred += (4 * pred_strd);
        }
    }
    return sad;
}

UWORD16 ihevce_8x8_sad_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd)
{
    return ihevce_8xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, 8);
}

WORD32 ihevce_nxn_sad_computer_neon(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_pred, WORD32 pred_strd, WORD32 trans_size)
{
    switch(trans_size)
    {
    case 4:
        return ihevce_4x4_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd);
    case 8:
        return ihevce_8xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, 8);
    case 16:
        return ihevce_16xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, 16);
    case 32:
        return ihevce_32xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, 32);
    case 64:
        return ihevce_64xn_sad_computer_neon(pu1_src, pu1_pred, src_strd, pred_strd, 64);
    default:
        // should not be here
        return -1;
    }
}
