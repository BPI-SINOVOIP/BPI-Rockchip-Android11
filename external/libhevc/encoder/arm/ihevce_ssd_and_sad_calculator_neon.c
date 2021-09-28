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
*  ihevce_ssd_and_sad_calculator_neon.c
*
* @brief
*  Contains intrinsic definitions of functions for ssd and sad computation
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
#include "ihevce_cmn_utils_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
LWORD64 ihevce_ssd_and_sad_calculator_neon(
    UWORD8 *pu1_recon,
    WORD32 recon_strd,
    UWORD8 *pu1_src,
    WORD32 src_strd,
    WORD32 trans_size,
    UWORD32 *pu4_blk_sad)
{
    WORD32 i, ssd = 0;

    if(4 == trans_size)
    {
        const uint8x16_t src_u8 = load_unaligned_u8q(pu1_src, src_strd);
        const uint8x16_t ref_u8 = load_unaligned_u8q(pu1_recon, recon_strd);
        const uint8x8_t abs_l = vabd_u8(vget_low_u8(src_u8), vget_low_u8(ref_u8));
        const uint8x8_t abs_h = vabd_u8(vget_high_u8(src_u8), vget_high_u8(ref_u8));
        const uint16x8_t sq_abs_l = vmull_u8(abs_l, abs_l);
        const uint16x8_t sq_abs_h = vmull_u8(abs_h, abs_h);
        uint16x8_t abs_sum;
        uint32x4_t b, d;
        uint32x2_t ssd, sad;
        uint64x2_t c;

        abs_sum = vaddl_u8(abs_l, abs_h);
        b = vpaddlq_u16(abs_sum);
        c = vpaddlq_u32(b);
        sad =
            vadd_u32(vreinterpret_u32_u64(vget_low_u64(c)), vreinterpret_u32_u64(vget_high_u64(c)));
        *pu4_blk_sad = vget_lane_u32(sad, 0);
        b = vaddl_u16(vget_low_u16(sq_abs_l), vget_high_u16(sq_abs_l));
        d = vaddl_u16(vget_low_u16(sq_abs_h), vget_high_u16(sq_abs_h));
        b = vaddq_u32(b, d);
        ssd = vadd_u32(vget_low_u32(b), vget_high_u32(b));

        return vget_lane_u64(vpaddl_u32(ssd), 0);
    }
    else if(8 == trans_size)
    {
        uint16x8_t abs_sum = vdupq_n_u16(0);
        uint32x4_t sqabs_sum = vdupq_n_u32(0);
        uint16x8_t abs, sqabs;
        uint32x4_t tmp_a;
        uint32x2_t sad, ssd;
        uint64x2_t tmp_b;

        for(i = 0; i < 8; i++)
        {
            const uint8x8_t src = vld1_u8(pu1_src);
            const uint8x8_t ref = vld1_u8(pu1_recon);

            abs = vabdl_u8(src, ref);
            sqabs = vmulq_u16(abs, abs);
            abs_sum = vaddq_u16(abs_sum, abs);
            tmp_a = vaddl_u16(vget_low_u16(sqabs), vget_high_u16(sqabs));
            sqabs_sum = vaddq_u32(sqabs_sum, tmp_a);

            pu1_src += src_strd;
            pu1_recon += recon_strd;
        }
        tmp_a = vpaddlq_u16(abs_sum);
        tmp_b = vpaddlq_u32(tmp_a);
        sad = vadd_u32(
            vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
        *pu4_blk_sad = vget_lane_u32(sad, 0);
        ssd = vadd_u32(vget_low_u32(sqabs_sum), vget_high_u32(sqabs_sum));

        return vget_lane_u64(vpaddl_u32(ssd), 0);
    }
    else if(16 == trans_size)
    {
        uint16x8_t abs_sum_l = vdupq_n_u16(0);
        uint16x8_t abs_sum_h = vdupq_n_u16(0);
        uint32x4_t sqabs_sum_l = vdupq_n_u32(0);
        uint32x4_t sqabs_sum_h = vdupq_n_u32(0);
        uint16x8_t abs_l, abs_h;
        uint16x8_t sqabs_l, sqabs_h;
        uint32x4_t tmp_a, tmp_c;
        uint64x2_t tmp_b;
        uint32x2_t sad, ssd;
        WORD32 i;

        for(i = 0; i < 16; i++)
        {
            const uint8x16_t src = vld1q_u8(pu1_src);
            const uint8x16_t pred = vld1q_u8(pu1_recon);

            abs_l = vabdl_u8(vget_low_u8(src), vget_low_u8(pred));
            abs_h = vabdl_u8(vget_high_u8(src), vget_high_u8(pred));

            sqabs_l = vmulq_u16(abs_l, abs_l);
            sqabs_h = vmulq_u16(abs_h, abs_h);

            abs_sum_l = vaddq_u16(abs_sum_l, abs_l);
            abs_sum_h = vaddq_u16(abs_sum_h, abs_h);

            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));

            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);
            pu1_src += src_strd;
            pu1_recon += recon_strd;
        }
        tmp_a = vpaddlq_u16(abs_sum_l);
        tmp_a = vpadalq_u16(tmp_a, abs_sum_h);
        tmp_b = vpaddlq_u32(tmp_a);
        sad = vadd_u32(
            vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
        *pu4_blk_sad = vget_lane_u32(sad, 0);

        sqabs_sum_l = vaddq_u32(sqabs_sum_l, sqabs_sum_h);
        ssd = vadd_u32(vget_low_u32(sqabs_sum_l), vget_high_u32(sqabs_sum_l));

        return vget_lane_u64(vpaddl_u32(ssd), 0);
    }
    else if(32 == trans_size)
    {
        uint16x8_t abs_sum = vdupq_n_u16(0);
        uint16x8_t abs_sum_l, abs_sum_h;
        uint32x4_t sqabs_sum_l = vdupq_n_u32(0);
        uint32x4_t sqabs_sum_h = vdupq_n_u32(0);
        uint8x8_t abs_l, abs_h;
        uint16x8_t sqabs_l, sqabs_h;
        uint32x4_t tmp_a, tmp_c;
        uint64x2_t tmp_b;
        uint32x2_t sad, ssd;
        WORD32 i;

        for(i = 0; i < 32; i++)
        {
            const uint8x16_t src_0 = vld1q_u8(pu1_src);
            const uint8x16_t pred_0 = vld1q_u8(pu1_recon);
            const uint8x16_t src_1 = vld1q_u8(pu1_src + 16);
            const uint8x16_t pred_1 = vld1q_u8(pu1_recon + 16);

            abs_l = vabd_u8(vget_low_u8(src_0), vget_low_u8(pred_0));
            abs_h = vabd_u8(vget_high_u8(src_0), vget_high_u8(pred_0));

            abs_sum_l = vaddl_u8(abs_l, abs_h);
            sqabs_l = vmull_u8(abs_l, abs_l);
            sqabs_h = vmull_u8(abs_h, abs_h);
            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));
            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);

            abs_l = vabd_u8(vget_low_u8(src_1), vget_low_u8(pred_1));
            abs_h = vabd_u8(vget_high_u8(src_1), vget_high_u8(pred_1));

            abs_sum_h = vaddl_u8(abs_l, abs_h);
            sqabs_l = vmull_u8(abs_l, abs_l);
            sqabs_h = vmull_u8(abs_h, abs_h);
            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));
            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);

            abs_sum_l = vaddq_u16(abs_sum_l, abs_sum_h);
            abs_sum = vaddq_u16(abs_sum, abs_sum_l);

            pu1_src += src_strd;
            pu1_recon += recon_strd;
        }
        tmp_a = vpaddlq_u16(abs_sum);
        tmp_b = vpaddlq_u32(tmp_a);
        sad = vadd_u32(
            vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
        *pu4_blk_sad = vget_lane_u32(sad, 0);

        sqabs_sum_l = vaddq_u32(sqabs_sum_l, sqabs_sum_h);
        ssd = vadd_u32(vget_low_u32(sqabs_sum_l), vget_high_u32(sqabs_sum_l));

        return vget_lane_u64(vpaddl_u32(ssd), 0);
    }
    else if(64 == trans_size)
    {
        uint32x4_t abs_sum = vdupq_n_u32(0);
        uint16x8_t abs_sum_0, abs_sum_1, abs_sum_2, abs_sum_3;
        uint32x4_t sqabs_sum_l = vdupq_n_u32(0);
        uint32x4_t sqabs_sum_h = vdupq_n_u32(0);
        uint8x8_t abs_l, abs_h;
        uint16x8_t sqabs_l, sqabs_h;
        uint32x4_t tmp_a, tmp_c;
        uint64x2_t tmp_b;
        uint32x2_t sad, ssd;
        WORD32 i;

        for(i = 0; i < 64; i++)
        {
            const uint8x16_t src_0 = vld1q_u8(pu1_src);
            const uint8x16_t pred_0 = vld1q_u8(pu1_recon);
            const uint8x16_t src_1 = vld1q_u8(pu1_src + 16);
            const uint8x16_t pred_1 = vld1q_u8(pu1_recon + 16);
            const uint8x16_t src_2 = vld1q_u8(pu1_src + 32);
            const uint8x16_t pred_2 = vld1q_u8(pu1_recon + 32);
            const uint8x16_t src_3 = vld1q_u8(pu1_src + 48);
            const uint8x16_t pred_3 = vld1q_u8(pu1_recon + 48);

            abs_l = vabd_u8(vget_low_u8(src_0), vget_low_u8(pred_0));
            abs_h = vabd_u8(vget_high_u8(src_0), vget_high_u8(pred_0));

            abs_sum_0 = vaddl_u8(abs_l, abs_h);
            sqabs_l = vmull_u8(abs_l, abs_l);
            sqabs_h = vmull_u8(abs_h, abs_h);
            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));
            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);

            abs_l = vabd_u8(vget_low_u8(src_1), vget_low_u8(pred_1));
            abs_h = vabd_u8(vget_high_u8(src_1), vget_high_u8(pred_1));

            abs_sum_1 = vaddl_u8(abs_l, abs_h);
            sqabs_l = vmull_u8(abs_l, abs_l);
            sqabs_h = vmull_u8(abs_h, abs_h);
            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));
            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);

            abs_l = vabd_u8(vget_low_u8(src_2), vget_low_u8(pred_2));
            abs_h = vabd_u8(vget_high_u8(src_2), vget_high_u8(pred_2));

            abs_sum_2 = vaddl_u8(abs_l, abs_h);
            sqabs_l = vmull_u8(abs_l, abs_l);
            sqabs_h = vmull_u8(abs_h, abs_h);
            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));
            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);

            abs_l = vabd_u8(vget_low_u8(src_3), vget_low_u8(pred_3));
            abs_h = vabd_u8(vget_high_u8(src_3), vget_high_u8(pred_3));

            abs_sum_3 = vaddl_u8(abs_l, abs_h);
            sqabs_l = vmull_u8(abs_l, abs_l);
            sqabs_h = vmull_u8(abs_h, abs_h);
            tmp_a = vaddl_u16(vget_low_u16(sqabs_l), vget_high_u16(sqabs_l));
            tmp_c = vaddl_u16(vget_low_u16(sqabs_h), vget_high_u16(sqabs_h));
            sqabs_sum_l = vaddq_u32(sqabs_sum_l, tmp_a);
            sqabs_sum_h = vaddq_u32(sqabs_sum_h, tmp_c);

            abs_sum_0 = vaddq_u16(abs_sum_0, abs_sum_1);
            abs_sum_2 = vaddq_u16(abs_sum_2, abs_sum_3);
            abs_sum_0 = vaddq_u16(abs_sum_0, abs_sum_2);
            tmp_a = vaddl_u16(vget_low_u16(abs_sum_0), vget_high_u16(abs_sum_0));
            abs_sum = vaddq_u32(abs_sum, tmp_a);

            pu1_src += src_strd;
            pu1_recon += recon_strd;
        }
        tmp_b = vpaddlq_u32(abs_sum);
        sad = vadd_u32(
            vreinterpret_u32_u64(vget_low_u64(tmp_b)), vreinterpret_u32_u64(vget_high_u64(tmp_b)));
        *pu4_blk_sad = vget_lane_u32(sad, 0);

        sqabs_sum_l = vaddq_u32(sqabs_sum_l, sqabs_sum_h);
        ssd = vadd_u32(vget_low_u32(sqabs_sum_l), vget_high_u32(sqabs_sum_l));

        return vget_lane_u64(vpaddl_u32(ssd), 0);
    }
    return (ssd);
}
