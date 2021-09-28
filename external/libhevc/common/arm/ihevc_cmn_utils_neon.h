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
*  ihevc_cmn_utils_neon.h
*
* @brief
*  Structure definitions used in the decoder
*
* @author
*  ittiam
*
* @par List of Functions:
*
* @remarks
*  None
*
*******************************************************************************
*/

#ifndef _IHEVC_CMN_UTILS_NEON_H_
#define _IHEVC_CMN_UTILS_NEON_H_

#include <arm_neon.h>
#include "ihevc_platform_macros.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
static INLINE uint8x16_t load_unaligned_u8q(const uint8_t *buf, int stride)
{
    uint8_t a[16];

    if(stride == 4)
        return vld1q_u8(buf);
    memcpy(a, buf, 4);
    buf += stride;
    memcpy(a + 4, buf, 4);
    buf += stride;
    memcpy(a + 8, buf, 4);
    buf += stride;
    memcpy(a + 12, buf, 4);
    return vld1q_u8(a);
}

static INLINE uint8x16_t load_unaligned_u8qi(const uint8_t *buf, int stride)
{
    uint8_t a[16];
    uint8_t *b = a;
    int j;

    for(j = 0; j < 4; j++)
    {
        b[0] = buf[0];
        b[1] = buf[2];
        b[2] = buf[4];
        b[3] = buf[6];
        buf += stride;
        b += 4;
    }
    return vld1q_u8(a);
}

static INLINE void store_unaligned_u8q(uint8_t *buf, int stride, uint8x16_t b0)
{
    uint8_t a[16];

    vst1q_u8(a, b0);
    memcpy(buf, a, 4);
    buf += stride;
    memcpy(buf, a + 4, 4);
    buf += stride;
    memcpy(buf, a + 8, 4);
    buf += stride;
    memcpy(buf, a + 12, 4);
}

static INLINE int16x8x2_t vtrnq_s64_to_s16(int32x4_t a0, int32x4_t a1)
{
    int16x8x2_t b0;

    b0.val[0] = vcombine_s16(
        vreinterpret_s16_s32(vget_low_s32(a0)), vreinterpret_s16_s32(vget_low_s32(a1)));
    b0.val[1] = vcombine_s16(
        vreinterpret_s16_s32(vget_high_s32(a0)), vreinterpret_s16_s32(vget_high_s32(a1)));
    return b0;
}

static INLINE void transpose_s16_4x4d(int16x4_t *a0, int16x4_t *a1, int16x4_t *a2, int16x4_t *a3)
{
    // Swap 16 bit elements. Goes from:
    // a0: 00 01 02 03
    // a1: 10 11 12 13
    // a2: 20 21 22 23
    // a3: 30 31 32 33
    // to:
    // b0.val[0]: 00 10 02 12
    // b0.val[1]: 01 11 03 13
    // b1.val[0]: 20 30 22 32
    // b1.val[1]: 21 31 23 33

    const int16x4x2_t b0 = vtrn_s16(*a0, *a1);
    const int16x4x2_t b1 = vtrn_s16(*a2, *a3);

    // Swap 32 bit elements resulting in:
    // c0.val[0]: 00 10 20 30
    // c0.val[1]: 02 12 22 32
    // c1.val[0]: 01 11 21 31
    // c1.val[1]: 03 13 23 33

    const int32x2x2_t c0 =
        vtrn_s32(vreinterpret_s32_s16(b0.val[0]), vreinterpret_s32_s16(b1.val[0]));
    const int32x2x2_t c1 =
        vtrn_s32(vreinterpret_s32_s16(b0.val[1]), vreinterpret_s32_s16(b1.val[1]));

    *a0 = vreinterpret_s16_s32(c0.val[0]);
    *a1 = vreinterpret_s16_s32(c1.val[0]);
    *a2 = vreinterpret_s16_s32(c0.val[1]);
    *a3 = vreinterpret_s16_s32(c1.val[1]);
}

static INLINE void transpose_s16_4x4q(int16x8_t *a0, int16x8_t *a1, int16x8_t *a2, int16x8_t *a3)
{
    // Swap 16 bit elements. Goes from:
    // a0: 00 01 02 03  04 05 06 07
    // a1: 10 11 12 13  14 15 16 17
    // a2: 20 21 22 23  24 25 26 27
    // a3: 30 31 32 33  34 35 36 37
    // to:
    // b0.val[0]: 00 10 02 12  04 14 06 16
    // b0.val[1]: 01 11 03 13  05 15 07 17
    // b1.val[0]: 20 30 22 32  24 34 26 36
    // b1.val[1]: 21 31 23 33  25 35 27 37

    const int16x8x2_t b0 = vtrnq_s16(*a0, *a1);
    const int16x8x2_t b1 = vtrnq_s16(*a2, *a3);

    // Swap 32 bit elements resulting in:
    // c0.val[0]: 00 10 20 30  04 14 24 34
    // c0.val[1]: 02 12 22 32  05 15 25 35
    // c1.val[0]: 01 11 21 31  06 16 26 36
    // c1.val[1]: 03 13 23 33  07 17 27 37

    const int32x4x2_t c0 =
        vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]), vreinterpretq_s32_s16(b1.val[0]));
    const int32x4x2_t c1 =
        vtrnq_s32(vreinterpretq_s32_s16(b0.val[1]), vreinterpretq_s32_s16(b1.val[1]));

    *a0 = vreinterpretq_s16_s32(c0.val[0]);
    *a1 = vreinterpretq_s16_s32(c1.val[0]);
    *a2 = vreinterpretq_s16_s32(c0.val[1]);
    *a3 = vreinterpretq_s16_s32(c1.val[1]);
}

static INLINE void transpose_s16_8x8(
    int16x8_t *a0,
    int16x8_t *a1,
    int16x8_t *a2,
    int16x8_t *a3,
    int16x8_t *a4,
    int16x8_t *a5,
    int16x8_t *a6,
    int16x8_t *a7)
{
    // Swap 16 bit elements. Goes from:
    // a0: 00 01 02 03 04 05 06 07
    // a1: 10 11 12 13 14 15 16 17
    // a2: 20 21 22 23 24 25 26 27
    // a3: 30 31 32 33 34 35 36 37
    // a4: 40 41 42 43 44 45 46 47
    // a5: 50 51 52 53 54 55 56 57
    // a6: 60 61 62 63 64 65 66 67
    // a7: 70 71 72 73 74 75 76 77
    // to:
    // b0.val[0]: 00 10 02 12 04 14 06 16
    // b0.val[1]: 01 11 03 13 05 15 07 17
    // b1.val[0]: 20 30 22 32 24 34 26 36
    // b1.val[1]: 21 31 23 33 25 35 27 37
    // b2.val[0]: 40 50 42 52 44 54 46 56
    // b2.val[1]: 41 51 43 53 45 55 47 57
    // b3.val[0]: 60 70 62 72 64 74 66 76
    // b3.val[1]: 61 71 63 73 65 75 67 77
    int16x8x2_t b0, b1, b2, b3, d0, d1, d2, d3;
    int32x4x2_t c0, c1, c2, c3;

    b0 = vtrnq_s16(*a0, *a1);
    b1 = vtrnq_s16(*a2, *a3);
    b2 = vtrnq_s16(*a4, *a5);
    b3 = vtrnq_s16(*a6, *a7);

    // Swap 32 bit elements resulting in:
    // c0.val[0]: 00 10 20 30 04 14 24 34
    // c0.val[1]: 02 12 22 32 06 16 26 36
    // c1.val[0]: 01 11 21 31 05 15 25 35
    // c1.val[1]: 03 13 23 33 07 17 27 37
    // c2.val[0]: 40 50 60 70 44 54 64 74
    // c2.val[1]: 42 52 62 72 46 56 66 76
    // c3.val[0]: 41 51 61 71 45 55 65 75
    // c3.val[1]: 43 53 63 73 47 57 67 77

    c0 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[0]), vreinterpretq_s32_s16(b1.val[0]));
    c1 = vtrnq_s32(vreinterpretq_s32_s16(b0.val[1]), vreinterpretq_s32_s16(b1.val[1]));
    c2 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[0]), vreinterpretq_s32_s16(b3.val[0]));
    c3 = vtrnq_s32(vreinterpretq_s32_s16(b2.val[1]), vreinterpretq_s32_s16(b3.val[1]));

    // Swap 64 bit elements resulting in:
    // d0.val[0]: 00 10 20 30 40 50 60 70
    // d0.val[1]: 04 14 24 34 44 54 64 74
    // d1.val[0]: 01 11 21 31 41 51 61 71
    // d1.val[1]: 05 15 25 35 45 55 65 75
    // d2.val[0]: 02 12 22 32 42 52 62 72
    // d2.val[1]: 06 16 26 36 46 56 66 76
    // d3.val[0]: 03 13 23 33 43 53 63 73
    // d3.val[1]: 07 17 27 37 47 57 67 77

    d0 = vtrnq_s64_to_s16(c0.val[0], c2.val[0]);
    d1 = vtrnq_s64_to_s16(c1.val[0], c3.val[0]);
    d2 = vtrnq_s64_to_s16(c0.val[1], c2.val[1]);
    d3 = vtrnq_s64_to_s16(c1.val[1], c3.val[1]);

    *a0 = d0.val[0];
    *a1 = d1.val[0];
    *a2 = d2.val[0];
    *a3 = d3.val[0];
    *a4 = d0.val[1];
    *a5 = d1.val[1];
    *a6 = d2.val[1];
    *a7 = d3.val[1];
}

static INLINE int32x4x2_t vtrnq_s64_to_s32(int32x4_t a0, int32x4_t a1)
{
    int32x4x2_t b0;
    b0.val[0] = vcombine_s32(vget_low_s32(a0), vget_low_s32(a1));
    b0.val[1] = vcombine_s32(vget_high_s32(a0), vget_high_s32(a1));
    return b0;
}

static INLINE void transpose_s32_4x4(int32x4_t *a0, int32x4_t *a1, int32x4_t *a2, int32x4_t *a3)
{
    // Swap 32 bit elements. Goes from:
    // a0: 00 01 02 03
    // a1: 10 11 12 13
    // a2: 20 21 22 23
    // a3: 30 31 32 33
    // to:
    // b0.val[0]: 00 10 02 12
    // b0.val[1]: 01 11 03 13
    // b1.val[0]: 20 30 22 32
    // b1.val[1]: 21 31 23 33

    const int32x4x2_t b0 = vtrnq_s32(*a0, *a1);
    const int32x4x2_t b1 = vtrnq_s32(*a2, *a3);

    // Swap 64 bit elements resulting in:
    // c0.val[0]: 00 10 20 30
    // c0.val[1]: 02 12 22 32
    // c1.val[0]: 01 11 21 31
    // c1.val[1]: 03 13 23 33

    const int32x4x2_t c0 = vtrnq_s64_to_s32(b0.val[0], b1.val[0]);
    const int32x4x2_t c1 = vtrnq_s64_to_s32(b0.val[1], b1.val[1]);

    *a0 = c0.val[0];
    *a1 = c1.val[0];
    *a2 = c0.val[1];
    *a3 = c1.val[1];
}

static INLINE void transpose_s32_8x8(
    int32x4x2_t *a0,
    int32x4x2_t *a1,
    int32x4x2_t *a2,
    int32x4x2_t *a3,
    int32x4x2_t *a4,
    int32x4x2_t *a5,
    int32x4x2_t *a6,
    int32x4x2_t *a7)
{
    // Swap 32 bit elements. Goes from:
    // a0: 00 01 02 03 04 05 06 07
    // a1: 10 11 12 13 14 15 16 17
    // a2: 20 21 22 23 24 25 26 27
    // a3: 30 31 32 33 34 35 36 37
    // a4: 40 41 42 43 44 45 46 47
    // a5: 50 51 52 53 54 55 56 57
    // a6: 60 61 62 63 64 65 66 67
    // a7: 70 71 72 73 74 75 76 77
    // to:
    // b0: 00 10 02 12 01 11 03 13
    // b1: 20 30 22 32 21 31 23 33
    // b2: 40 50 42 52 41 51 43 53
    // b3: 60 70 62 72 61 71 63 73
    // b4: 04 14 06 16 05 15 07 17
    // b5: 24 34 26 36 25 35 27 37
    // b6: 44 54 46 56 45 55 47 57
    // b7: 64 74 66 76 65 75 67 77

    const int32x4x2_t b0 = vtrnq_s32(a0->val[0], a1->val[0]);
    const int32x4x2_t b1 = vtrnq_s32(a2->val[0], a3->val[0]);
    const int32x4x2_t b2 = vtrnq_s32(a4->val[0], a5->val[0]);
    const int32x4x2_t b3 = vtrnq_s32(a6->val[0], a7->val[0]);
    const int32x4x2_t b4 = vtrnq_s32(a0->val[1], a1->val[1]);
    const int32x4x2_t b5 = vtrnq_s32(a2->val[1], a3->val[1]);
    const int32x4x2_t b6 = vtrnq_s32(a4->val[1], a5->val[1]);
    const int32x4x2_t b7 = vtrnq_s32(a6->val[1], a7->val[1]);

    // Swap 64 bit elements resulting in:
    // c0: 00 10 20 30 02 12 22 32
    // c1: 01 11 21 31 03 13 23 33
    // c2: 40 50 60 70 42 52 62 72
    // c3: 41 51 61 71 43 53 63 73
    // c4: 04 14 24 34 06 16 26 36
    // c5: 05 15 25 35 07 17 27 37
    // c6: 44 54 64 74 46 56 66 76
    // c7: 45 55 65 75 47 57 67 77
    const int32x4x2_t c0 = vtrnq_s64_to_s32(b0.val[0], b1.val[0]);
    const int32x4x2_t c1 = vtrnq_s64_to_s32(b0.val[1], b1.val[1]);
    const int32x4x2_t c2 = vtrnq_s64_to_s32(b2.val[0], b3.val[0]);
    const int32x4x2_t c3 = vtrnq_s64_to_s32(b2.val[1], b3.val[1]);
    const int32x4x2_t c4 = vtrnq_s64_to_s32(b4.val[0], b5.val[0]);
    const int32x4x2_t c5 = vtrnq_s64_to_s32(b4.val[1], b5.val[1]);
    const int32x4x2_t c6 = vtrnq_s64_to_s32(b6.val[0], b7.val[0]);
    const int32x4x2_t c7 = vtrnq_s64_to_s32(b6.val[1], b7.val[1]);

    // Swap 128 bit elements resulting in:
    // a0: 00 10 20 30 40 50 60 70
    // a1: 01 11 21 31 41 51 61 71
    // a2: 02 12 22 32 42 52 62 72
    // a3: 03 13 23 33 43 53 63 73
    // a4: 04 14 24 34 44 54 64 74
    // a5: 05 15 25 35 45 55 65 75
    // a6: 06 16 26 36 46 56 66 76
    // a7: 07 17 27 37 47 57 67 77
    a0->val[0] = c0.val[0];
    a0->val[1] = c2.val[0];
    a1->val[0] = c1.val[0];
    a1->val[1] = c3.val[0];
    a2->val[0] = c0.val[1];
    a2->val[1] = c2.val[1];
    a3->val[0] = c1.val[1];
    a3->val[1] = c3.val[1];
    a4->val[0] = c4.val[0];
    a4->val[1] = c6.val[0];
    a5->val[0] = c5.val[0];
    a5->val[1] = c7.val[0];
    a6->val[0] = c4.val[1];
    a6->val[1] = c6.val[1];
    a7->val[0] = c5.val[1];
    a7->val[1] = c7.val[1];
}
#endif /* _IHEVC_CMN_UTILS_NEON_H_ */
