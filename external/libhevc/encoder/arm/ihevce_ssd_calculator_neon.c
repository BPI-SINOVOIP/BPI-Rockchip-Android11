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
*  ihevce_ssd_calculator_neon.c
*
* @brief
*  Contains intrinsic definitions of functions for sad computation
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
static INLINE uint32x4_t ihevce_4x4_ssd_computer_neon(
    UWORD8 *pu1_src, UWORD8 *pu1_pred, WORD32 src_strd, WORD32 pred_strd,
    CHROMA_PLANE_ID_T chroma_plane)
{
    uint32x4_t ssd_low, ssd_high;
    uint8x16_t src, pred, abs;
    uint16x8_t sqabs_low, sqabs_high;

    if(chroma_plane == NULL_PLANE)
    {
        src = load_unaligned_u8q(pu1_src, src_strd);
        pred = load_unaligned_u8q(pu1_pred, pred_strd);
    }
    else
    {
        src = load_unaligned_u8qi(pu1_src + chroma_plane, src_strd);
        pred = load_unaligned_u8qi(pu1_pred + chroma_plane, pred_strd);
    }
    abs = vabdq_u8(src, pred);
    sqabs_low = vmull_u8(vget_low_u8(abs), vget_low_u8(abs));
    sqabs_high = vmull_u8(vget_high_u8(abs), vget_high_u8(abs));

    ssd_low = vaddl_u16(vget_low_u16(sqabs_low), vget_high_u16(sqabs_low));
    ssd_high = vaddl_u16(vget_low_u16(sqabs_high), vget_high_u16(sqabs_high));
    return vaddq_u32(ssd_low, ssd_high);
}

static INLINE uint32x4_t
    ihevce_1x8_ssd_computer_neon(UWORD8 *pu1_src, UWORD8 *pu1_pred,
    CHROMA_PLANE_ID_T chroma_plane)
{
    uint32x4_t ssd_val;
    uint8x8_t src, pred, abs;
    uint16x8_t sqabs;

    if(chroma_plane == NULL_PLANE)
    {
        src = vld1_u8(pu1_src);
        pred = vld1_u8(pu1_pred);
    }
    else
    {
        src = vld2_u8(pu1_src).val[chroma_plane];
        pred = vld2_u8(pu1_pred).val[chroma_plane];
    }
    abs = vabd_u8(src, pred);
    sqabs = vmull_u8(abs, abs);

    ssd_val = vaddl_u16(vget_low_u16(sqabs), vget_high_u16(sqabs));
    return ssd_val;
}

static INLINE uint32x4_t
    ihevce_1x16_ssd_computer_neon(UWORD8 *pu1_src, UWORD8 *pu1_pred,
    CHROMA_PLANE_ID_T chroma_plane)
{
    uint32x4_t ssd_low, ssd_high;
    uint8x16_t src, pred, abs;
    uint16x8_t sqabs_low, sqabs_high;

    if(chroma_plane == NULL_PLANE)
    {
        src = vld1q_u8(pu1_src);
        pred = vld1q_u8(pu1_pred);
    }
    else
    {
        src = vld2q_u8(pu1_src).val[chroma_plane];
        pred = vld2q_u8(pu1_pred).val[chroma_plane];
    }
    abs = vabdq_u8(src, pred);
    sqabs_low = vmull_u8(vget_low_u8(abs), vget_low_u8(abs));
    sqabs_high = vmull_u8(vget_high_u8(abs), vget_high_u8(abs));

    ssd_low = vaddl_u16(vget_low_u16(sqabs_low), vget_high_u16(sqabs_low));
    ssd_high = vaddl_u16(vget_low_u16(sqabs_high), vget_high_u16(sqabs_high));
    return vaddq_u32(ssd_low, ssd_high);
}

static INLINE uint32x4_t
    ihevce_1x32_ssd_computer_neon(UWORD8 *pu1_src, UWORD8 *pu1_pred,
    CHROMA_PLANE_ID_T chroma_plane)
{
    uint32x4_t ssd_0, ssd_1, ssd_2, ssd_3;
    uint8x16_t src_0, pred_0, src_1, pred_1, abs_0, abs_1;
    uint16x8_t sqabs_0, sqabs_1, sqabs_2, sqabs_3;

    if(chroma_plane == NULL_PLANE)
    {
        src_0 = vld1q_u8(pu1_src);
        pred_0 = vld1q_u8(pu1_pred);
        src_1 = vld1q_u8(pu1_src + 16);
        pred_1 = vld1q_u8(pu1_pred + 16);
    }
    else
    {
        src_0 = vld2q_u8(pu1_src).val[chroma_plane];
        pred_0 = vld2q_u8(pu1_pred).val[chroma_plane];
        src_1 = vld2q_u8(pu1_src + 32).val[chroma_plane];
        pred_1 = vld2q_u8(pu1_pred + 32).val[chroma_plane];
    }
    abs_0 = vabdq_u8(src_0, pred_0);
    abs_1 = vabdq_u8(src_1, pred_1);
    sqabs_0 = vmull_u8(vget_low_u8(abs_0), vget_low_u8(abs_0));
    sqabs_1 = vmull_u8(vget_high_u8(abs_0), vget_high_u8(abs_0));
    sqabs_2 = vmull_u8(vget_low_u8(abs_1), vget_low_u8(abs_1));
    sqabs_3 = vmull_u8(vget_high_u8(abs_1), vget_high_u8(abs_1));

    ssd_0 = vaddl_u16(vget_low_u16(sqabs_0), vget_high_u16(sqabs_0));
    ssd_1 = vaddl_u16(vget_low_u16(sqabs_1), vget_high_u16(sqabs_1));
    ssd_2 = vaddl_u16(vget_low_u16(sqabs_2), vget_high_u16(sqabs_2));
    ssd_3 = vaddl_u16(vget_low_u16(sqabs_3), vget_high_u16(sqabs_3));
    ssd_0 = vaddq_u32(ssd_0, ssd_1);
    ssd_2 = vaddq_u32(ssd_2, ssd_3);
    return vaddq_u32(ssd_0, ssd_2);
}

static INLINE uint32x4_t
    ihevce_1x64_ssd_computer_neon(UWORD8 *pu1_src, UWORD8 *pu1_pred,
    CHROMA_PLANE_ID_T chroma_plane)
{
    uint32x4_t ssd_0, ssd_1, ssd_2, ssd_3;
    uint32x4_t ssd_4, ssd_5, ssd_6, ssd_7;
    uint8x16_t src_0, src_1, src_2, src_3;
    uint8x16_t pred_0, pred_1, pred_2, pred_3;
    uint8x16_t abs_0, abs_1, abs_2, abs_3;
    uint16x8_t sqabs_0, sqabs_1, sqabs_2, sqabs_3;
    uint16x8_t sqabs_4, sqabs_5, sqabs_6, sqabs_7;

    if(chroma_plane == NULL_PLANE)
    {
        src_0 = vld1q_u8(pu1_src);
        pred_0 = vld1q_u8(pu1_pred);
        src_1 = vld1q_u8(pu1_src + 16);
        pred_1 = vld1q_u8(pu1_pred + 16);
        src_2 = vld1q_u8(pu1_src + 32);
        pred_2 = vld1q_u8(pu1_pred + 32);
        src_3 = vld1q_u8(pu1_src + 48);
        pred_3 = vld1q_u8(pu1_pred + 48);
    }
    else
    {
        src_0 = vld2q_u8(pu1_src).val[chroma_plane];
        pred_0 = vld2q_u8(pu1_pred).val[chroma_plane];
        src_1 = vld2q_u8(pu1_src + 32).val[chroma_plane];
        pred_1 = vld2q_u8(pu1_pred + 32).val[chroma_plane];
        src_2 = vld2q_u8(pu1_src + 64).val[chroma_plane];
        pred_2 = vld2q_u8(pu1_pred + 64).val[chroma_plane];
        src_3 = vld2q_u8(pu1_src + 96).val[chroma_plane];
        pred_3 = vld2q_u8(pu1_pred + 96).val[chroma_plane];
    }
    abs_0 = vabdq_u8(src_0, pred_0);
    abs_1 = vabdq_u8(src_1, pred_1);
    abs_2 = vabdq_u8(src_2, pred_2);
    abs_3 = vabdq_u8(src_3, pred_3);
    sqabs_0 = vmull_u8(vget_low_u8(abs_0), vget_low_u8(abs_0));
    sqabs_1 = vmull_u8(vget_high_u8(abs_0), vget_high_u8(abs_0));
    sqabs_2 = vmull_u8(vget_low_u8(abs_1), vget_low_u8(abs_1));
    sqabs_3 = vmull_u8(vget_high_u8(abs_1), vget_high_u8(abs_1));
    sqabs_4 = vmull_u8(vget_low_u8(abs_2), vget_low_u8(abs_2));
    sqabs_5 = vmull_u8(vget_high_u8(abs_2), vget_high_u8(abs_2));
    sqabs_6 = vmull_u8(vget_low_u8(abs_3), vget_low_u8(abs_3));
    sqabs_7 = vmull_u8(vget_high_u8(abs_3), vget_high_u8(abs_3));

    ssd_0 = vaddl_u16(vget_low_u16(sqabs_0), vget_high_u16(sqabs_0));
    ssd_1 = vaddl_u16(vget_low_u16(sqabs_1), vget_high_u16(sqabs_1));
    ssd_2 = vaddl_u16(vget_low_u16(sqabs_2), vget_high_u16(sqabs_2));
    ssd_3 = vaddl_u16(vget_low_u16(sqabs_3), vget_high_u16(sqabs_3));
    ssd_4 = vaddl_u16(vget_low_u16(sqabs_4), vget_high_u16(sqabs_4));
    ssd_5 = vaddl_u16(vget_low_u16(sqabs_5), vget_high_u16(sqabs_5));
    ssd_6 = vaddl_u16(vget_low_u16(sqabs_6), vget_high_u16(sqabs_6));
    ssd_7 = vaddl_u16(vget_low_u16(sqabs_7), vget_high_u16(sqabs_7));
    ssd_0 = vaddq_u32(ssd_0, ssd_1);
    ssd_2 = vaddq_u32(ssd_2, ssd_3);
    ssd_4 = vaddq_u32(ssd_4, ssd_5);
    ssd_6 = vaddq_u32(ssd_6, ssd_7);
    ssd_0 = vaddq_u32(ssd_0, ssd_2);
    ssd_4 = vaddq_u32(ssd_4, ssd_6);
    return vaddq_u32(ssd_0, ssd_4);
}

static LWORD64 ihevce_ssd_calculator_plane_neon(
    UWORD8 *pu1_inp,
    UWORD8 *pu1_ref,
    UWORD32 inp_stride,
    UWORD32 ref_stride,
    UWORD32 wd,
    UWORD32 ht,
    CHROMA_PLANE_ID_T chroma_plane)
{
    uint32x4_t ssd = vdupq_n_u32(0);
    uint32x2_t sum;

    if(wd >= 8)
    {
        UWORD32 row;

        for(row = ht; row > 0; row--)
        {
            if(wd == 8)
                ssd = vaddq_u32(ssd, ihevce_1x8_ssd_computer_neon(pu1_inp, pu1_ref, chroma_plane));
            else if(wd == 16)
                ssd = vaddq_u32(ssd, ihevce_1x16_ssd_computer_neon(pu1_inp, pu1_ref, chroma_plane));
            else if(wd == 32)
                ssd = vaddq_u32(ssd, ihevce_1x32_ssd_computer_neon(pu1_inp, pu1_ref, chroma_plane));
            else if(wd == 64)
                ssd = vaddq_u32(ssd, ihevce_1x64_ssd_computer_neon(pu1_inp, pu1_ref, chroma_plane));
            else if(wd % 8 == 0)
            {
                UWORD32 col;
                UWORD8 *inp = pu1_inp, *ref = pu1_ref;

                for(col = 0; col < wd; col += 8)
                {
                    ssd = vaddq_u32(ssd, ihevce_1x8_ssd_computer_neon(inp, ref, chroma_plane));
                    ref = ref + 8;
                    inp = inp + 8;
                }
            }

            pu1_inp += inp_stride;
            pu1_ref += ref_stride;
        }
    }
    else if(wd == 4)
    {
        assert(ht == 4);
        ssd = ihevce_4x4_ssd_computer_neon(pu1_inp, pu1_ref, inp_stride, ref_stride, chroma_plane);
    }

    sum = vadd_u32(vget_low_u32(ssd), vget_high_u32(ssd));
    return vget_lane_u64(vpaddl_u32(sum), 0);
}

LWORD64 ihevce_ssd_calculator_neon(
    UWORD8 *pu1_inp, UWORD8 *pu1_ref, UWORD32 inp_stride, UWORD32 ref_stride, UWORD32 wd,
    UWORD32 ht, CHROMA_PLANE_ID_T chroma_plane)
{
    return ihevce_ssd_calculator_plane_neon(pu1_inp, pu1_ref, inp_stride, ref_stride, wd, ht,
                                            chroma_plane);
}

LWORD64 ihevce_chroma_interleave_ssd_calculator_neon(
    UWORD8 *pu1_inp, UWORD8 *pu1_ref, UWORD32 inp_stride, UWORD32 ref_stride, UWORD32 wd,
    UWORD32 ht, CHROMA_PLANE_ID_T chroma_plane)
{
    return ihevce_ssd_calculator_plane_neon(pu1_inp, pu1_ref, inp_stride, ref_stride, wd, ht,
                                            chroma_plane);
}
