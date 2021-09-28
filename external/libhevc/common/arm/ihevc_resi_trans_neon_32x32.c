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
 *  ihevc_resi_trans_neon_32x32.c
 *
 * @brief
 *  Contains definitions of functions for computing residue and fwd transform
 *
 * @author
 *  Ittiam
 *
 * @par List of Functions:
 *  - ihevc_resi_trans_32x32_neon()
 *
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
 *  Input 32x32 pixels
 *
 * @param[in] pu1_pred
 *  Prediction data
 *
 * @param[in] pi2_tmp
 *  Temporary buffer of size 32x32
 *
 * @param[out] pi2_dst
 *  Output 32x32 coefficients
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
UWORD32 ihevc_resi_trans_32x32_neon(UWORD8 *pu1_src, UWORD8 *pu1_pred,
    WORD32 *pi4_temp, WORD16 *pi2_dst, WORD32 src_strd, WORD32 pred_strd,
    WORD32 dst_strd, CHROMA_PLANE_ID_T e_chroma_plane)
{
    int16x8_t diff_16[4][2];
    WORD32 i;
    int32x2_t sad;
    int64x2_t tmp_a;
    UWORD32 u4_blk_sad = 0;
    WORD32 *pi4_temp_orig = pi4_temp;
    int16x8_t abs = vdupq_n_s16(0);
    int32x4_t sum_val = vdupq_n_s32(0);
    UNUSED(e_chroma_plane);

    // Stage 1
    for(i = 0; i < 16; i++)
    {

        uint8x16_t src_buff, pred_buff;
        abs = vdupq_n_s16(0);

        src_buff = vld1q_u8(pu1_src);
        pred_buff = vld1q_u8(pu1_pred);
        diff_16[0][0] = vreinterpretq_s16_u16(
            vsubl_u8(vget_low_u8(src_buff), vget_low_u8(pred_buff)));
        diff_16[1][0] = vreinterpretq_s16_u16(
            vsubl_u8(vget_high_u8(src_buff), vget_high_u8(pred_buff)));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[0][0]));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[1][0]));

        src_buff = vld1q_u8(pu1_src + 16);
        pred_buff = vld1q_u8(pu1_pred + 16);
        diff_16[2][0] = vrev64q_s16(vreinterpretq_s16_u16(
            vsubl_u8(vget_low_u8(src_buff), vget_low_u8(pred_buff))));
        diff_16[2][0] = vcombine_s16(
            vget_high_s16(diff_16[2][0]), vget_low_s16(diff_16[2][0]));
        diff_16[3][0] = vrev64q_s16(vreinterpretq_s16_u16(
            vsubl_u8(vget_high_u8(src_buff), vget_high_u8(pred_buff))));
        diff_16[3][0] = vcombine_s16(
            vget_high_s16(diff_16[3][0]), vget_low_s16(diff_16[3][0]));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[2][0]));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[3][0]));

        pu1_src += src_strd;
        pu1_pred += pred_strd;

        src_buff = vld1q_u8(pu1_src);
        pred_buff = vld1q_u8(pu1_pred);
        diff_16[0][1] = vreinterpretq_s16_u16(
            vsubl_u8(vget_low_u8(src_buff), vget_low_u8(pred_buff)));
        diff_16[1][1] = vreinterpretq_s16_u16(
            vsubl_u8(vget_high_u8(src_buff), vget_high_u8(pred_buff)));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[0][1]));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[1][1]));

        src_buff = vld1q_u8(pu1_src + 16);
        pred_buff = vld1q_u8(pu1_pred + 16);
        diff_16[2][1] = vrev64q_s16(vreinterpretq_s16_u16(
            vsubl_u8(vget_low_u8(src_buff), vget_low_u8(pred_buff))));
        diff_16[2][1] = vcombine_s16(
            vget_high_s16(diff_16[2][1]), vget_low_s16(diff_16[2][1]));
        diff_16[3][1] = vrev64q_s16(vreinterpretq_s16_u16(
            vsubl_u8(vget_high_u8(src_buff), vget_high_u8(pred_buff))));
        diff_16[3][1] = vcombine_s16(
            vget_high_s16(diff_16[3][1]), vget_low_s16(diff_16[3][1]));

        abs = vaddq_s16(abs, vabsq_s16(diff_16[2][1]));
        abs = vaddq_s16(abs, vabsq_s16(diff_16[3][1]));

        sum_val = vaddq_s32(sum_val,vpaddlq_s16(abs));

        pu1_src += src_strd;
        pu1_pred += pred_strd;
        {
            static const int16x8_t g_ai2_ihevc_trans_32_01_8 = { 64, 83, 64, 36, 64, 36, -64, -83 };

            static const int16x4_t g_ai2_ihevc_trans_32_4_04 = { 89, 75, 50, 18 };
            static const int16x4_t g_ai2_ihevc_trans_32_12_04 = { 75, -18, -89, -50 };
            static const int16x4_t g_ai2_ihevc_trans_32_20_04 = { 50, -89, 18, 75 };
            static const int16x4_t g_ai2_ihevc_trans_32_28_04 = { 18, -50, 75, -89 };

            static const int16x8_t g_ai2_ihevc_trans_32_2_07 = { 90, 87, 80, 70, 57, 43, 25, 9 };
            static const int16x8_t g_ai2_ihevc_trans_32_6_07 = { 87, 57, 9, -43, -80, -90, -70, -25 };
            static const int16x8_t g_ai2_ihevc_trans_32_10_07 = { 80, 9, -70, -87, -25, 57, 90, 43 };
            static const int16x8_t g_ai2_ihevc_trans_32_14_07 = { 70, -43, -87, 9, 90, 25, -80, -57 };
            static const int16x8_t g_ai2_ihevc_trans_32_18_07 = { 57, -80, -25, 90, -9, -87, 43, 70 };
            static const int16x8_t g_ai2_ihevc_trans_32_22_07 = { 43, -90, 57, 25, -87, 70, 9, -80 };
            static const int16x8_t g_ai2_ihevc_trans_32_26_07 = { 25, -70, 90, -80, 43, 9, -57, 87 };
            static const int16x8_t g_ai2_ihevc_trans_32_30_07 = { 9, -25, 43, -57, 70, -80, 87, -90 };

            static const int16x8_t g_ai2_ihevc_trans_32_1_07 = { 90, 90, 88, 85, 82, 78, 73, 67 };
            static const int16x8_t g_ai2_ihevc_trans_32_1_815 = { 61, 54, 46, 38, 31, 22, 13, 4 };
            static const int16x8_t g_ai2_ihevc_trans_32_3_07 = { 90, 82, 67, 46, 22, -4, -31, -54 };
            static const int16x8_t g_ai2_ihevc_trans_32_3_815 = { -73, -85, -90, -88, -78, -61, -38, -13 };
            static const int16x8_t g_ai2_ihevc_trans_32_5_07 = { 88, 67, 31, -13, -54, -82, -90, -78 };
            static const int16x8_t g_ai2_ihevc_trans_32_5_815 = { -46, -4, 38, 73, 90, 85, 61, 22 };
            static const int16x8_t g_ai2_ihevc_trans_32_7_07 = { 85, 46, -13, -67, -90, -73, -22, 38 };
            static const int16x8_t g_ai2_ihevc_trans_32_7_815 = { 82, 88, 54, -4, -61, -90, -78, -31 };
            static const int16x8_t g_ai2_ihevc_trans_32_9_07 = { 82, 22, -54, -90, -61, 13, 78, 85 };
            static const int16x8_t g_ai2_ihevc_trans_32_9_815 = { 31, -46, -90, -67, 4, 73, 88, 38 };
            static const int16x8_t g_ai2_ihevc_trans_32_11_07 = { 78, -4, -82, -73, 13, 85, 67, -22 };
            static const int16x8_t g_ai2_ihevc_trans_32_11_815 = { -88, -61, 31, 90, 54, -38, -90, -46 };
            static const int16x8_t g_ai2_ihevc_trans_32_13_07 = { 73, -31, -90, -22, 78, 67, -38, -90 };
            static const int16x8_t g_ai2_ihevc_trans_32_13_815 = { -13, 82, 61, -46, -88, -4, 85, 54 };
            static const int16x8_t g_ai2_ihevc_trans_32_15_07 = { 67, -54, -78, 38, 85, -22, -90, 4 };
            static const int16x8_t g_ai2_ihevc_trans_32_15_815 = { 90, 13, -88, -31, 82, 46, -73, -61 };
            static const int16x8_t g_ai2_ihevc_trans_32_17_07 = { 61, -73, -46, 82, 31, -88, -13, 90 };
            static const int16x8_t g_ai2_ihevc_trans_32_17_815 = { -4, -90, 22, 85, -38, -78, 54, 67 };
            static const int16x8_t g_ai2_ihevc_trans_32_19_07 = { 54, -85, -4, 88, -46, -61, 82, 13 };
            static const int16x8_t g_ai2_ihevc_trans_32_19_815 = { -90, 38, 67, -78, -22, 90, -31, -73 };
            static const int16x8_t g_ai2_ihevc_trans_32_21_07 = { 46, -90, 38, 54, -90, 31, 61, -88 };
            static const int16x8_t g_ai2_ihevc_trans_32_21_815 = { 22, 67, -85, 13, 73, -82, 4, 78 };
            static const int16x8_t g_ai2_ihevc_trans_32_23_07 = { 38, -88, 73, -4, -67, 90, -46, -31 };
            static const int16x8_t g_ai2_ihevc_trans_32_23_815 = { 85, -78, 13, 61, -90, 54, 22, -82 };
            static const int16x8_t g_ai2_ihevc_trans_32_25_07 = { 31, -78, 90, -61, 4, 54, -88, 82 };
            static const int16x8_t g_ai2_ihevc_trans_32_25_815 = { -38, -22, 73, -90, 67, -13, -46, 85 };
            static const int16x8_t g_ai2_ihevc_trans_32_27_07 = { 22, -61, 85, -90, 73, -38, -4, 46 };
            static const int16x8_t g_ai2_ihevc_trans_32_27_815 = { -78, 90, -82, 54, -13, -31, 67, -88 };
            static const int16x8_t g_ai2_ihevc_trans_32_29_07 = { 13, -38, 61, -78, 88, -90, 85, -73 };
            static const int16x8_t g_ai2_ihevc_trans_32_29_815 = { 54, -31, 4, 22, -46, 67, -82, 90 };
            static const int16x8_t g_ai2_ihevc_trans_32_31_07 = { 4, -13, 22, -31, 38, -46, 54, -61 };
            static const int16x8_t g_ai2_ihevc_trans_32_31_815 = { 67, -73, 78, -82, 85, -88, 90, -90 };

            int32x4x2_t a[32];

            const int16x8_t o1_1 = vsubq_s16(
                diff_16[1][1], diff_16[2][1]); /*R2(9-16) - R2(24-17)*/
            const int16x8_t o1_0 = vsubq_s16(
                diff_16[0][1], diff_16[3][1]); /*R2(1- 8) - R2(32-25)*/
            const int16x8_t o0_1 = vsubq_s16(
                diff_16[1][0], diff_16[2][0]); /*R1(9-16) - R1(24-17)*/
            const int16x8_t o0_0 = vsubq_s16(
                diff_16[0][0], diff_16[3][0]); /*R1(1- 8) - R1(32-25)*/
            const int16x8_t e0_0 = vaddq_s16(
                diff_16[0][0], diff_16[3][0]); /*R1(1- 8) + R1(32-25)*/
            int16x8_t e0_1 = vrev64q_s16(vaddq_s16(
                diff_16[1][0], diff_16[2][0])); /*R1(9-16) + R1(24-17)*/
            e0_1 = vcombine_s16(vget_high_s16(e0_1), vget_low_s16(e0_1));
            const int16x8_t e1_0 = vaddq_s16(
                diff_16[0][1], diff_16[3][1]); /*R2(1- 8) + R2(32-25)*/
            int16x8_t e1_1 = vrev64q_s16(vaddq_s16(
                diff_16[1][1], diff_16[2][1])); /*R2(9-16) + R2(24-17)*/
            e1_1 = vcombine_s16(vget_high_s16(e1_1), vget_low_s16(e1_1));

            const int16x8_t ee0 = vaddq_s16(e0_0, e0_1); /*E1(1- 8) + E1(16-9)*/
            const int16x8_t ee1 = vaddq_s16(e1_0, e1_1); /*E2(1- 8) + E2(16-9)*/
            const int16x8_t eo1 = vsubq_s16(e1_0, e1_1); /*E2(1- 8) - E2(16-9)*/
            const int16x8_t eo0 = vsubq_s16(e0_0, e0_1); /*E1(1- 8) - E1(16-9)*/

            /*EE0(1-4) & EE1(1-4)*/
            const int16x8_t ee_a =
                vcombine_s16(vget_low_s16(ee0), vget_low_s16(ee1));
            /*EE0(8-5) & EE1(8-5)*/
            const int16x8_t ee_b = vcombine_s16(
                vrev64_s16(vget_high_s16(ee0)), vrev64_s16(vget_high_s16(ee1)));

            /*EE(1-4) - EE(8-5)*/
            const int16x8_t eeo = vsubq_s16(ee_a, ee_b);  //Q0
            /*EE(1-4) + EE(8-5)*/
            const int16x8_t eee = vaddq_s16(ee_a, ee_b);  //Q13

            /*EEEE Calculations*/
            const int32x2x2_t ee =
                vtrn_s32(vreinterpret_s32_s16(vget_low_s16(eee)),
                    vreinterpret_s32_s16(vget_high_s16(eee)));
            const int16x8_t eeee_a =
                vreinterpretq_s16_s32(vcombine_s32(ee.val[0], ee.val[0]));
            const int16x8_t eeee_b =
                vcombine_s16(vrev32_s16(vreinterpret_s16_s32(ee.val[1])),
                    vneg_s16(vrev32_s16(vreinterpret_s16_s32(ee.val[1]))));
            const int16x8_t eeee = vaddq_s16(eeee_a, eeee_b);  //q2
            const int16x4x2_t trans_eeee =
                vtrn_s16(vget_low_s16(eeee), vget_high_s16(eeee));
            const int16x4_t eeee_00 = vreinterpret_s16_s32(vdup_lane_s32(
                vreinterpret_s32_s16(trans_eeee.val[0]), 0));  //d8
            const int16x4_t eeee_10 = vreinterpret_s16_s32(vdup_lane_s32(
                vreinterpret_s32_s16(trans_eeee.val[0]), 1));  //d9
            const int16x4_t eeee_01 = vreinterpret_s16_s32(vdup_lane_s32(
                vreinterpret_s32_s16(trans_eeee.val[1]), 0));  //d10
            const int16x4_t eeee_11 = vreinterpret_s16_s32(vdup_lane_s32(
                vreinterpret_s32_s16(trans_eeee.val[1]), 1));  //d11

            /*Calculation of values 0 8 16 24*/
            a[0].val[0] =
                vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_01_8), eeee_00);
            a[0].val[0] = vmlal_s16(
                a[0].val[0], vget_high_s16(g_ai2_ihevc_trans_32_01_8), eeee_01);
            a[0].val[1] =
                vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_01_8), eeee_10);
            a[0].val[1] = vmlal_s16(
                a[0].val[1], vget_high_s16(g_ai2_ihevc_trans_32_01_8), eeee_11);

            int32x4x2_t val_8 = vzipq_s32(a[0].val[0], a[0].val[1]);

            /*Store*/
            vst1_s32(pi4_temp, vget_low_s32(val_8.val[0])); /*Value 0*/
            vst1_s32(pi4_temp + 256, vget_high_s32(val_8.val[0])); /*Value 8*/
            vst1_s32(pi4_temp + 512, vget_low_s32(val_8.val[1])); /*Value 16*/
            vst1_s32(pi4_temp + 768, vget_high_s32(val_8.val[1])); /*Value 24*/

            /*Calculation of values 4 12 20 28*/
            /*Multiplications*/
            a[4].val[0] =
                vmull_s16(g_ai2_ihevc_trans_32_4_04, vget_low_s16(eeo));
            a[12].val[0] =
                vmull_s16(g_ai2_ihevc_trans_32_12_04, vget_low_s16(eeo));
            a[20].val[0] =
                vmull_s16(g_ai2_ihevc_trans_32_20_04, vget_low_s16(eeo));
            a[28].val[0] =
                vmull_s16(g_ai2_ihevc_trans_32_28_04, vget_low_s16(eeo));

            a[4].val[1] =
                vmull_s16(g_ai2_ihevc_trans_32_4_04, vget_high_s16(eeo));
            a[12].val[1] =
                vmull_s16(g_ai2_ihevc_trans_32_12_04, vget_high_s16(eeo));
            a[20].val[1] =
                vmull_s16(g_ai2_ihevc_trans_32_20_04, vget_high_s16(eeo));
            a[28].val[1] =
                vmull_s16(g_ai2_ihevc_trans_32_28_04, vget_high_s16(eeo));

            /*Transposes*/
            int32x4x2_t val_4_0 =
                vtrnq_s32(a[4].val[0], a[12].val[0]);  //q15 q5
            int32x4x2_t val_4_1 =
                vtrnq_s32(a[4].val[1], a[12].val[1]);  //q4 q12
            int32x4x2_t val_20_0 =
                vtrnq_s32(a[20].val[0], a[28].val[0]);  //q8 q2
            int32x4x2_t val_20_1 =
                vtrnq_s32(a[20].val[1], a[28].val[1]);  //q9 q13

            /*Swap*/
            a[4].val[0] = vcombine_s32(vget_low_s32(val_4_0.val[0]),
                vget_low_s32(val_20_0.val[0]));  //q12
            a[4].val[1] = vcombine_s32(vget_high_s32(val_4_0.val[0]),
                vget_high_s32(val_20_0.val[0]));  //q2

            a[12].val[0] = vcombine_s32(vget_low_s32(val_4_0.val[1]),
                vget_low_s32(val_20_0.val[1]));  //q4
            a[12].val[1] = vcombine_s32(vget_high_s32(val_4_0.val[1]),
                vget_high_s32(val_20_0.val[1]));  //q8

            /*Additions*/
            a[12].val[0] = vaddq_s32(a[12].val[0], a[4].val[0]);  //q4
            a[12].val[1] = vaddq_s32(a[12].val[1], a[4].val[1]);  //q8
            a[12].val[1] = vaddq_s32(a[12].val[1], a[12].val[0]);  //q8

            a[20].val[0] = vcombine_s32(vget_low_s32(val_4_1.val[0]),
                vget_low_s32(val_20_1.val[0]));  //q5
            a[20].val[1] = vcombine_s32(vget_high_s32(val_4_1.val[0]),
                vget_high_s32(val_20_1.val[0]));  //q13

            a[28].val[0] = vcombine_s32(vget_low_s32(val_4_1.val[1]),
                vget_low_s32(val_20_1.val[1]));  //q15
            a[28].val[1] = vcombine_s32(vget_high_s32(val_4_1.val[1]),
                vget_high_s32(val_20_1.val[1]));  //q9

            a[28].val[0] = vaddq_s32(a[28].val[0], a[20].val[0]);  //q15
            a[28].val[1] = vaddq_s32(a[28].val[1], a[20].val[1]);  //q5
            a[28].val[1] = vaddq_s32(a[28].val[1], a[28].val[0]);  //q15

            int32x4x2_t val_4 = vzipq_s32(a[12].val[1], a[28].val[1]);

            /*Store*/
            vst1_s32(pi4_temp + 128, vget_low_s32(val_4.val[0])); /*Value 4*/
            vst1_s32(pi4_temp + 384, vget_high_s32(val_4.val[0])); /*Value 12*/
            vst1_s32(pi4_temp + 640, vget_low_s32(val_4.val[1])); /*Value 20*/
            vst1_s32(pi4_temp + 896, vget_high_s32(val_4.val[1])); /*Value 28*/

            /*Calculation of value 2 6 10 14 18 22 26 30*/
            /*Multiplications*/
            a[2].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_2_07),
                vget_low_s16(eo0));  //q2
            a[6].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_6_07),
                vget_low_s16(eo0));  //q5
            a[10].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_10_07),
                vget_low_s16(eo0));  //q9
            a[14].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_14_07),
                vget_low_s16(eo0));  //q8

            a[14].val[0] = vmlal_s16(a[14].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_14_07), vget_high_s16(eo0));
            a[10].val[0] = vmlal_s16(a[10].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_10_07), vget_high_s16(eo0));
            a[6].val[0] = vmlal_s16(a[6].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_6_07), vget_high_s16(eo0));
            a[2].val[0] = vmlal_s16(a[2].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_2_07), vget_high_s16(eo0));

            a[2].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_2_07),
                vget_low_s16(eo1));  //q4
            a[6].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_6_07),
                vget_low_s16(eo1));  //q13
            a[10].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_10_07),
                vget_low_s16(eo1));  //q12
            a[14].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_14_07),
                vget_low_s16(eo1));  //q15

            a[14].val[1] = vmlal_s16(a[14].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_14_07), vget_high_s16(eo1));
            a[10].val[1] = vmlal_s16(a[10].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_10_07), vget_high_s16(eo1));
            a[6].val[1] = vmlal_s16(a[6].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_6_07), vget_high_s16(eo1));
            a[2].val[1] = vmlal_s16(a[2].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_2_07), vget_high_s16(eo1));

            /*Transposes*/
            int32x4x2_t val_26_0 = vtrnq_s32(a[2].val[0], a[6].val[0]);  //q2 q5
            int32x4x2_t val_1014_0 =
                vtrnq_s32(a[10].val[0], a[14].val[0]);  //q9 q8
            int32x4x2_t val_26_1 =
                vtrnq_s32(a[2].val[1], a[6].val[1]);  //q4 q13
            int32x4x2_t val_1014_1 =
                vtrnq_s32(a[10].val[1], a[14].val[1]);  //q12 q15

            /*Swap*/
            a[2].val[0] = vcombine_s32(vget_low_s32(val_26_0.val[0]),
                vget_low_s32(val_1014_0.val[0]));  //q2
            a[2].val[1] = vcombine_s32(vget_high_s32(val_26_0.val[0]),
                vget_high_s32(val_1014_0.val[0]));  //q9

            a[6].val[0] = vcombine_s32(vget_low_s32(val_26_0.val[1]),
                vget_low_s32(val_1014_0.val[1]));  //q5
            a[6].val[1] = vcombine_s32(vget_high_s32(val_26_0.val[1]),
                vget_high_s32(val_1014_0.val[1]));  //q8

            a[10].val[0] = vcombine_s32(vget_low_s32(val_26_1.val[0]),
                vget_low_s32(val_1014_1.val[0]));  //q4
            a[10].val[1] = vcombine_s32(vget_high_s32(val_26_1.val[0]),
                vget_high_s32(val_1014_1.val[0]));  //q12

            a[14].val[0] = vcombine_s32(vget_low_s32(val_26_1.val[1]),
                vget_low_s32(val_1014_1.val[1]));  //q13
            a[14].val[1] = vcombine_s32(vget_high_s32(val_26_1.val[1]),
                vget_high_s32(val_1014_1.val[1]));  //q15

            /*Additions*/
            a[2].val[0] = vaddq_s32(a[2].val[0], a[6].val[0]);  //q2
            a[2].val[1] = vaddq_s32(a[2].val[1], a[6].val[1]);  //q9
            a[2].val[1] = vaddq_s32(a[2].val[1], a[2].val[0]);  //q9

            a[10].val[0] = vaddq_s32(a[10].val[0], a[14].val[0]);  //q4
            a[10].val[1] = vaddq_s32(a[10].val[1], a[14].val[1]);  //q12
            a[10].val[1] = vaddq_s32(a[10].val[1], a[10].val[0]);  //q12

            int32x4x2_t val_2 = vzipq_s32(a[2].val[1], a[10].val[1]);  //q9 q12

            /*Store*/
            vst1_s32(pi4_temp + 64, vget_low_s32(val_2.val[0])); /*Value 2*/
            vst1_s32(pi4_temp + 192, vget_high_s32(val_2.val[0])); /*Value 6*/
            vst1_s32(pi4_temp + 320, vget_low_s32(val_2.val[1])); /*Value 10*/
            vst1_s32(pi4_temp + 448, vget_high_s32(val_2.val[1])); /*Value 14*/

            a[18].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_18_07),
                vget_low_s16(eo0));  //q0
            a[22].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_22_07),
                vget_low_s16(eo0));  //q5
            a[26].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_26_07),
                vget_low_s16(eo0));  //q9
            a[30].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_30_07),
                vget_low_s16(eo0));  //q15

            a[30].val[0] = vmlal_s16(a[30].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_30_07), vget_high_s16(eo0));
            a[26].val[0] = vmlal_s16(a[26].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_26_07), vget_high_s16(eo0));
            a[22].val[0] = vmlal_s16(a[22].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_22_07), vget_high_s16(eo0));
            a[18].val[0] = vmlal_s16(a[18].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_18_07), vget_high_s16(eo0));

            a[18].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_18_07),
                vget_low_s16(eo1));  //q4
            a[22].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_22_07),
                vget_low_s16(eo1));  //q8
            a[26].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_26_07),
                vget_low_s16(eo1));  //q12
            a[30].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_30_07),
                vget_low_s16(eo1));  //q18

            a[30].val[1] = vmlal_s16(a[30].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_30_07), vget_high_s16(eo1));
            a[26].val[1] = vmlal_s16(a[26].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_26_07), vget_high_s16(eo1));
            a[22].val[1] = vmlal_s16(a[22].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_22_07), vget_high_s16(eo1));
            a[18].val[1] = vmlal_s16(a[18].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_18_07), vget_high_s16(eo1));

            /*Transposes*/
            int32x4x2_t val_1822_0 =
                vtrnq_s32(a[18].val[0], a[22].val[0]);  //q2 q5
            int32x4x2_t val_2630_0 =
                vtrnq_s32(a[26].val[0], a[30].val[0]);  //q9 q8
            int32x4x2_t val_1822_1 =
                vtrnq_s32(a[18].val[1], a[22].val[1]);  //q4 q13
            int32x4x2_t val_2630_1 =
                vtrnq_s32(a[26].val[1], a[30].val[1]);  //q12 q15

            /*Swap*/
            a[18].val[0] = vcombine_s32(vget_low_s32(val_1822_0.val[0]),
                vget_low_s32(val_2630_0.val[0]));  //q2
            a[18].val[1] = vcombine_s32(vget_high_s32(val_1822_0.val[0]),
                vget_high_s32(val_2630_0.val[0]));  //q9

            a[22].val[0] = vcombine_s32(vget_low_s32(val_1822_0.val[1]),
                vget_low_s32(val_2630_0.val[1]));  //q5
            a[22].val[1] = vcombine_s32(vget_high_s32(val_1822_0.val[1]),
                vget_high_s32(val_2630_0.val[1]));  //q8

            a[26].val[0] = vcombine_s32(vget_low_s32(val_1822_1.val[0]),
                vget_low_s32(val_2630_1.val[0]));  //q4
            a[26].val[1] = vcombine_s32(vget_high_s32(val_1822_1.val[0]),
                vget_high_s32(val_2630_1.val[0]));  //q12

            a[30].val[0] = vcombine_s32(vget_low_s32(val_1822_1.val[1]),
                vget_low_s32(val_2630_1.val[1]));  //q13
            a[30].val[1] = vcombine_s32(vget_high_s32(val_1822_1.val[1]),
                vget_high_s32(val_2630_1.val[1]));  //q15

            /*Additions*/
            a[18].val[0] = vaddq_s32(a[18].val[0], a[22].val[0]);  //q2
            a[18].val[1] = vaddq_s32(a[18].val[1], a[22].val[1]);  //q9
            a[18].val[1] = vaddq_s32(a[18].val[1], a[18].val[0]);  //q9

            a[26].val[0] = vaddq_s32(a[26].val[0], a[30].val[0]);  //q4
            a[26].val[1] = vaddq_s32(a[26].val[1], a[30].val[1]);  //q12
            a[26].val[1] = vaddq_s32(a[26].val[1], a[26].val[0]);  //q12

            int32x4x2_t val_18 =
                vzipq_s32(a[18].val[1], a[26].val[1]);  //q9 q12

            /*Store*/
            vst1_s32(pi4_temp + 576, vget_low_s32(val_18.val[0])); /*Value 18*/
            vst1_s32(pi4_temp + 704, vget_high_s32(val_18.val[0])); /*Value 22*/
            vst1_s32(pi4_temp + 832, vget_low_s32(val_18.val[1])); /*Value 26*/
            vst1_s32(pi4_temp + 960, vget_high_s32(val_18.val[1])); /*Value 30*/

            /*Calculations for odd indexes*/
            a[1].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_1_07),
                vget_low_s16(o0_0));  //q1
            a[3].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_3_07),
                vget_low_s16(o0_0));  //q5
            a[5].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_5_07),
                vget_low_s16(o0_0));  //q8
            a[7].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_7_07),
                vget_low_s16(o0_0));  //q12

            a[7].val[0] = vmlal_s16(a[7].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_7_07), vget_high_s16(o0_0));
            a[5].val[0] = vmlal_s16(a[5].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_5_07), vget_high_s16(o0_0));
            a[3].val[0] = vmlal_s16(a[3].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_3_07), vget_high_s16(o0_0));
            a[1].val[0] = vmlal_s16(a[1].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_1_07), vget_high_s16(o0_0));

            a[1].val[0] = vmlal_s16(a[1].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_1_815), vget_low_s16(o0_1));
            a[3].val[0] = vmlal_s16(a[3].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_3_815), vget_low_s16(o0_1));
            a[5].val[0] = vmlal_s16(a[5].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_5_815), vget_low_s16(o0_1));
            a[7].val[0] = vmlal_s16(a[7].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_7_815), vget_low_s16(o0_1));

            a[7].val[0] = vmlal_s16(a[7].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_7_815), vget_high_s16(o0_1));
            a[5].val[0] = vmlal_s16(a[5].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_5_815), vget_high_s16(o0_1));
            a[3].val[0] = vmlal_s16(a[3].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_3_815), vget_high_s16(o0_1));
            a[1].val[0] = vmlal_s16(a[1].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_1_815), vget_high_s16(o0_1));

            a[1].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_1_07),
                vget_low_s16(o1_0));  //q0
            a[3].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_3_07),
                vget_low_s16(o1_0));  //q4
            a[5].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_5_07),
                vget_low_s16(o1_0));  //q9
            a[7].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_7_07),
                vget_low_s16(o1_0));  //q13

            a[7].val[1] = vmlal_s16(a[7].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_7_07), vget_high_s16(o1_0));
            a[5].val[1] = vmlal_s16(a[5].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_5_07), vget_high_s16(o1_0));
            a[3].val[1] = vmlal_s16(a[3].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_3_07), vget_high_s16(o1_0));
            a[1].val[1] = vmlal_s16(a[1].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_1_07), vget_high_s16(o1_0));

            a[1].val[1] = vmlal_s16(a[1].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_1_815), vget_low_s16(o1_1));
            a[3].val[1] = vmlal_s16(a[3].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_3_815), vget_low_s16(o1_1));
            a[5].val[1] = vmlal_s16(a[5].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_5_815), vget_low_s16(o1_1));
            a[7].val[1] = vmlal_s16(a[7].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_7_815), vget_low_s16(o1_1));

            a[7].val[1] = vmlal_s16(a[7].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_7_815), vget_high_s16(o1_1));
            a[5].val[1] = vmlal_s16(a[5].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_5_815), vget_high_s16(o1_1));
            a[3].val[1] = vmlal_s16(a[3].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_3_815), vget_high_s16(o1_1));
            a[1].val[1] = vmlal_s16(a[1].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_1_815), vget_high_s16(o1_1));

            /*Transposes*/
            int32x4x2_t val_13_0 = vtrnq_s32(a[1].val[0], a[3].val[0]);  //q0 q4
            int32x4x2_t val_13_1 = vtrnq_s32(a[1].val[1], a[3].val[1]);  //q1 q5
            int32x4x2_t val_57_0 =
                vtrnq_s32(a[5].val[0], a[7].val[0]);  //q8 q12
            int32x4x2_t val_57_1 =
                vtrnq_s32(a[5].val[1], a[7].val[1]);  //q9 q13

            /*Swap*/
            a[1].val[0] = vcombine_s32(vget_low_s32(val_13_0.val[0]),
                vget_low_s32(val_57_0.val[0]));  //q0
            a[1].val[1] = vcombine_s32(vget_high_s32(val_13_0.val[0]),
                vget_high_s32(val_57_0.val[0]));  //q8

            a[3].val[0] = vcombine_s32(vget_low_s32(val_13_0.val[1]),
                vget_low_s32(val_57_0.val[1]));  //q1
            a[3].val[1] = vcombine_s32(vget_high_s32(val_13_0.val[1]),
                vget_high_s32(val_57_0.val[1]));  //q9

            a[5].val[0] = vcombine_s32(vget_low_s32(val_13_1.val[0]),
                vget_low_s32(val_57_1.val[0]));  //q4
            a[5].val[1] = vcombine_s32(vget_high_s32(val_13_1.val[0]),
                vget_high_s32(val_57_1.val[0]));  //q12

            a[7].val[0] = vcombine_s32(vget_low_s32(val_13_1.val[1]),
                vget_low_s32(val_57_1.val[1]));  //q5
            a[7].val[1] = vcombine_s32(vget_high_s32(val_13_1.val[1]),
                vget_high_s32(val_57_1.val[1]));  //q13

            /*Additions*/
            a[1].val[0] = vaddq_s32(a[1].val[0], a[3].val[0]);  //q0
            a[1].val[1] = vaddq_s32(a[1].val[1], a[3].val[1]);  //q8
            a[1].val[1] = vaddq_s32(a[1].val[1], a[1].val[0]);  //q8

            a[5].val[0] = vaddq_s32(a[5].val[0], a[7].val[0]);  //q1
            a[5].val[1] = vaddq_s32(a[5].val[1], a[7].val[1]);  //q9
            a[5].val[1] = vaddq_s32(a[5].val[1], a[5].val[0]);  //q9

            int32x4x2_t val_1 = vzipq_s32(a[1].val[1], a[5].val[1]);  //q8 q9

            /*Store*/
            vst1_s32(pi4_temp + 32, vget_low_s32(val_1.val[0])); /*Value 1*/
            vst1_s32(pi4_temp + 96, vget_high_s32(val_1.val[0])); /*Value 3*/
            vst1_s32(pi4_temp + 160, vget_low_s32(val_1.val[1])); /*Value 5*/
            vst1_s32(pi4_temp + 224, vget_high_s32(val_1.val[1])); /*Value 7*/

            a[9].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_9_07),
                vget_low_s16(o0_0));  //q2
            a[11].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_11_07),
                vget_low_s16(o0_0));  //q2
            a[13].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_13_07),
                vget_low_s16(o0_0));  //q2
            a[15].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_15_07),
                vget_low_s16(o0_0));  //q2

            a[15].val[0] = vmlal_s16(a[15].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_15_07), vget_high_s16(o0_0));
            a[13].val[0] = vmlal_s16(a[13].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_13_07), vget_high_s16(o0_0));
            a[11].val[0] = vmlal_s16(a[11].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_11_07), vget_high_s16(o0_0));
            a[9].val[0] = vmlal_s16(a[9].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_9_07), vget_high_s16(o0_0));

            a[9].val[0] = vmlal_s16(a[9].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_9_815), vget_low_s16(o0_1));
            a[11].val[0] = vmlal_s16(a[11].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_11_815), vget_low_s16(o0_1));
            a[13].val[0] = vmlal_s16(a[13].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_13_815), vget_low_s16(o0_1));
            a[15].val[0] = vmlal_s16(a[15].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_15_815), vget_low_s16(o0_1));

            a[15].val[0] = vmlal_s16(a[15].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_15_815),
                vget_high_s16(o0_1));
            a[13].val[0] = vmlal_s16(a[13].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_13_815),
                vget_high_s16(o0_1));
            a[11].val[0] = vmlal_s16(a[11].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_11_815),
                vget_high_s16(o0_1));
            a[9].val[0] = vmlal_s16(a[9].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_9_815), vget_high_s16(o0_1));

            a[9].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_9_07),
                vget_low_s16(o1_0));  //q2
            a[11].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_11_07),
                vget_low_s16(o1_0));  //q2
            a[13].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_13_07),
                vget_low_s16(o1_0));  //q2
            a[15].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_15_07),
                vget_low_s16(o1_0));  //q2

            a[15].val[1] = vmlal_s16(a[15].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_15_07), vget_high_s16(o1_0));
            a[13].val[1] = vmlal_s16(a[13].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_13_07), vget_high_s16(o1_0));
            a[11].val[1] = vmlal_s16(a[11].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_11_07), vget_high_s16(o1_0));
            a[9].val[1] = vmlal_s16(a[9].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_9_07), vget_high_s16(o1_0));

            a[9].val[1] = vmlal_s16(a[9].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_9_815), vget_low_s16(o1_1));
            a[11].val[1] = vmlal_s16(a[11].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_11_815), vget_low_s16(o1_1));
            a[13].val[1] = vmlal_s16(a[13].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_13_815), vget_low_s16(o1_1));
            a[15].val[1] = vmlal_s16(a[15].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_15_815), vget_low_s16(o1_1));

            a[15].val[1] = vmlal_s16(a[15].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_15_815),
                vget_high_s16(o1_1));
            a[13].val[1] = vmlal_s16(a[13].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_13_815),
                vget_high_s16(o1_1));
            a[11].val[1] = vmlal_s16(a[11].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_11_815),
                vget_high_s16(o1_1));
            a[9].val[1] = vmlal_s16(a[9].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_9_815), vget_high_s16(o1_1));

            int32x4x2_t val_911_0 =
                vtrnq_s32(a[9].val[0], a[11].val[0]);  //q0 q4
            int32x4x2_t val_911_1 =
                vtrnq_s32(a[9].val[1], a[11].val[1]);  //q1 q5
            int32x4x2_t val_1315_0 =
                vtrnq_s32(a[13].val[0], a[15].val[0]);  //q8 q12
            int32x4x2_t val_1315_1 =
                vtrnq_s32(a[13].val[1], a[15].val[1]);  //q9 q13

            a[9].val[0] = vcombine_s32(vget_low_s32(val_911_0.val[0]),
                vget_low_s32(val_1315_0.val[0]));  //q0
            a[9].val[1] = vcombine_s32(vget_high_s32(val_911_0.val[0]),
                vget_high_s32(val_1315_0.val[0]));  //q8

            a[11].val[0] = vcombine_s32(vget_low_s32(val_911_0.val[1]),
                vget_low_s32(val_1315_0.val[1]));  //q1
            a[11].val[1] = vcombine_s32(vget_high_s32(val_911_0.val[1]),
                vget_high_s32(val_1315_0.val[1]));  //q9

            a[13].val[0] = vcombine_s32(vget_low_s32(val_911_1.val[0]),
                vget_low_s32(val_1315_1.val[0]));  //q4
            a[13].val[1] = vcombine_s32(vget_high_s32(val_911_1.val[0]),
                vget_high_s32(val_1315_1.val[0]));  //q12

            a[15].val[0] = vcombine_s32(vget_low_s32(val_911_1.val[1]),
                vget_low_s32(val_1315_1.val[1]));  //q5
            a[15].val[1] = vcombine_s32(vget_high_s32(val_911_1.val[1]),
                vget_high_s32(val_1315_1.val[1]));  //q13

            a[9].val[0] = vaddq_s32(a[9].val[0], a[11].val[0]);  //q0
            a[9].val[1] = vaddq_s32(a[9].val[1], a[11].val[1]);  //q8
            a[9].val[1] = vaddq_s32(a[9].val[1], a[9].val[0]);  //q8

            a[13].val[0] = vaddq_s32(a[13].val[0], a[15].val[0]);  //q1
            a[13].val[1] = vaddq_s32(a[13].val[1], a[15].val[1]);  //q9
            a[13].val[1] = vaddq_s32(a[13].val[1], a[13].val[0]);  //q9

            int32x4x2_t val_9 = vzipq_s32(a[9].val[1], a[13].val[1]);  //q8 q9

            vst1_s32(pi4_temp + 288, vget_low_s32(val_9.val[0])); /*Value 9*/
            vst1_s32(pi4_temp + 352, vget_high_s32(val_9.val[0])); /*Value 11*/
            vst1_s32(pi4_temp + 416, vget_low_s32(val_9.val[1])); /*Value 13*/
            vst1_s32(pi4_temp + 480, vget_high_s32(val_9.val[1])); /*Value 15*/

            a[17].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_17_07),
                vget_low_s16(o0_0));  //q2
            a[19].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_19_07),
                vget_low_s16(o0_0));  //q2
            a[21].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_21_07),
                vget_low_s16(o0_0));  //q2
            a[23].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_23_07),
                vget_low_s16(o0_0));  //q2

            a[23].val[0] = vmlal_s16(a[23].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_23_07), vget_high_s16(o0_0));
            a[21].val[0] = vmlal_s16(a[21].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_21_07), vget_high_s16(o0_0));
            a[19].val[0] = vmlal_s16(a[19].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_19_07), vget_high_s16(o0_0));
            a[17].val[0] = vmlal_s16(a[17].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_17_07), vget_high_s16(o0_0));

            a[17].val[0] = vmlal_s16(a[17].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_17_815), vget_low_s16(o0_1));
            a[19].val[0] = vmlal_s16(a[19].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_19_815), vget_low_s16(o0_1));
            a[21].val[0] = vmlal_s16(a[21].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_21_815), vget_low_s16(o0_1));
            a[23].val[0] = vmlal_s16(a[23].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_23_815), vget_low_s16(o0_1));

            a[23].val[0] = vmlal_s16(a[23].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_23_815),
                vget_high_s16(o0_1));
            a[21].val[0] = vmlal_s16(a[21].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_21_815),
                vget_high_s16(o0_1));
            a[19].val[0] = vmlal_s16(a[19].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_19_815),
                vget_high_s16(o0_1));
            a[17].val[0] = vmlal_s16(a[17].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_17_815),
                vget_high_s16(o0_1));

            a[17].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_17_07),
                vget_low_s16(o1_0));  //q2
            a[19].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_19_07),
                vget_low_s16(o1_0));  //q2
            a[21].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_21_07),
                vget_low_s16(o1_0));  //q2
            a[23].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_23_07),
                vget_low_s16(o1_0));  //q2

            a[23].val[1] = vmlal_s16(a[23].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_23_07), vget_high_s16(o1_0));
            a[21].val[1] = vmlal_s16(a[21].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_21_07), vget_high_s16(o1_0));
            a[19].val[1] = vmlal_s16(a[19].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_19_07), vget_high_s16(o1_0));
            a[17].val[1] = vmlal_s16(a[17].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_17_07), vget_high_s16(o1_0));

            a[17].val[1] = vmlal_s16(a[17].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_17_815), vget_low_s16(o1_1));
            a[19].val[1] = vmlal_s16(a[19].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_19_815), vget_low_s16(o1_1));
            a[21].val[1] = vmlal_s16(a[21].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_21_815), vget_low_s16(o1_1));
            a[23].val[1] = vmlal_s16(a[23].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_23_815), vget_low_s16(o1_1));

            a[23].val[1] = vmlal_s16(a[23].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_23_815),
                vget_high_s16(o1_1));
            a[21].val[1] = vmlal_s16(a[21].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_21_815),
                vget_high_s16(o1_1));
            a[19].val[1] = vmlal_s16(a[19].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_19_815),
                vget_high_s16(o1_1));
            a[17].val[1] = vmlal_s16(a[17].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_17_815),
                vget_high_s16(o1_1));

            int32x4x2_t val_1719_0 =
                vtrnq_s32(a[17].val[0], a[19].val[0]);  //q0 q4
            int32x4x2_t val_1719_1 =
                vtrnq_s32(a[17].val[1], a[19].val[1]);  //q1 q5
            int32x4x2_t val_2123_0 =
                vtrnq_s32(a[21].val[0], a[23].val[0]);  //q8 q12
            int32x4x2_t val_2123_1 =
                vtrnq_s32(a[21].val[1], a[23].val[1]);  //q9 q13

            a[17].val[0] = vcombine_s32(vget_low_s32(val_1719_0.val[0]),
                vget_low_s32(val_2123_0.val[0]));  //q0
            a[17].val[1] = vcombine_s32(vget_high_s32(val_1719_0.val[0]),
                vget_high_s32(val_2123_0.val[0]));  //q8

            a[19].val[0] = vcombine_s32(vget_low_s32(val_1719_0.val[1]),
                vget_low_s32(val_2123_0.val[1]));  //q1
            a[19].val[1] = vcombine_s32(vget_high_s32(val_1719_0.val[1]),
                vget_high_s32(val_2123_0.val[1]));  //q9

            a[21].val[0] = vcombine_s32(vget_low_s32(val_1719_1.val[0]),
                vget_low_s32(val_2123_1.val[0]));  //q4
            a[21].val[1] = vcombine_s32(vget_high_s32(val_1719_1.val[0]),
                vget_high_s32(val_2123_1.val[0]));  //q12

            a[23].val[0] = vcombine_s32(vget_low_s32(val_1719_1.val[1]),
                vget_low_s32(val_2123_1.val[1]));  //q5
            a[23].val[1] = vcombine_s32(vget_high_s32(val_1719_1.val[1]),
                vget_high_s32(val_2123_1.val[1]));  //q13

            a[17].val[0] = vaddq_s32(a[17].val[0], a[19].val[0]);  //q0
            a[17].val[1] = vaddq_s32(a[17].val[1], a[19].val[1]);  //q8
            a[17].val[1] = vaddq_s32(a[17].val[1], a[17].val[0]);  //q8

            a[21].val[0] = vaddq_s32(a[21].val[0], a[23].val[0]);  //q1
            a[21].val[1] = vaddq_s32(a[21].val[1], a[23].val[1]);  //q9
            a[21].val[1] = vaddq_s32(a[21].val[1], a[21].val[0]);  //q9

            int32x4x2_t val_17 = vzipq_s32(a[17].val[1], a[21].val[1]);  //q8 q9

            vst1_s32(pi4_temp + 544, vget_low_s32(val_17.val[0])); /*Value 17*/
            vst1_s32(pi4_temp + 608, vget_high_s32(val_17.val[0])); /*Value 19*/
            vst1_s32(pi4_temp + 672, vget_low_s32(val_17.val[1])); /*Value 21*/
            vst1_s32(pi4_temp + 736, vget_high_s32(val_17.val[1])); /*Value 23*/

            a[25].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_25_07),
                vget_low_s16(o0_0));  //q2
            a[27].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_27_07),
                vget_low_s16(o0_0));  //q2
            a[29].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_29_07),
                vget_low_s16(o0_0));  //q2
            a[31].val[0] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_31_07),
                vget_low_s16(o0_0));  //q2

            a[31].val[0] = vmlal_s16(a[31].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_31_07), vget_high_s16(o0_0));
            a[29].val[0] = vmlal_s16(a[29].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_29_07), vget_high_s16(o0_0));
            a[27].val[0] = vmlal_s16(a[27].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_27_07), vget_high_s16(o0_0));
            a[25].val[0] = vmlal_s16(a[25].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_25_07), vget_high_s16(o0_0));

            a[25].val[0] = vmlal_s16(a[25].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_25_815), vget_low_s16(o0_1));
            a[27].val[0] = vmlal_s16(a[27].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_27_815), vget_low_s16(o0_1));
            a[29].val[0] = vmlal_s16(a[29].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_29_815), vget_low_s16(o0_1));
            a[31].val[0] = vmlal_s16(a[31].val[0],
                vget_low_s16(g_ai2_ihevc_trans_32_31_815), vget_low_s16(o0_1));

            a[31].val[0] = vmlal_s16(a[31].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_31_815),
                vget_high_s16(o0_1));
            a[29].val[0] = vmlal_s16(a[29].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_29_815),
                vget_high_s16(o0_1));
            a[27].val[0] = vmlal_s16(a[27].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_27_815),
                vget_high_s16(o0_1));
            a[25].val[0] = vmlal_s16(a[25].val[0],
                vget_high_s16(g_ai2_ihevc_trans_32_25_815),
                vget_high_s16(o0_1));

            a[25].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_25_07),
                vget_low_s16(o1_0));  //q2
            a[27].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_27_07),
                vget_low_s16(o1_0));  //q2
            a[29].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_29_07),
                vget_low_s16(o1_0));  //q2
            a[31].val[1] = vmull_s16(vget_low_s16(g_ai2_ihevc_trans_32_31_07),
                vget_low_s16(o1_0));  //q2

            a[31].val[1] = vmlal_s16(a[31].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_31_07), vget_high_s16(o1_0));
            a[29].val[1] = vmlal_s16(a[29].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_29_07), vget_high_s16(o1_0));
            a[27].val[1] = vmlal_s16(a[27].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_27_07), vget_high_s16(o1_0));
            a[25].val[1] = vmlal_s16(a[25].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_25_07), vget_high_s16(o1_0));

            a[25].val[1] = vmlal_s16(a[25].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_25_815), vget_low_s16(o1_1));
            a[27].val[1] = vmlal_s16(a[27].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_27_815), vget_low_s16(o1_1));
            a[29].val[1] = vmlal_s16(a[29].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_29_815), vget_low_s16(o1_1));
            a[31].val[1] = vmlal_s16(a[31].val[1],
                vget_low_s16(g_ai2_ihevc_trans_32_31_815), vget_low_s16(o1_1));

            a[31].val[1] = vmlal_s16(a[31].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_31_815),
                vget_high_s16(o1_1));
            a[29].val[1] = vmlal_s16(a[29].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_29_815),
                vget_high_s16(o1_1));
            a[27].val[1] = vmlal_s16(a[27].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_27_815),
                vget_high_s16(o1_1));
            a[25].val[1] = vmlal_s16(a[25].val[1],
                vget_high_s16(g_ai2_ihevc_trans_32_25_815),
                vget_high_s16(o1_1));

            int32x4x2_t val_2527_0 =
                vtrnq_s32(a[25].val[0], a[27].val[0]);  //q0 q4
            int32x4x2_t val_2527_1 =
                vtrnq_s32(a[25].val[1], a[27].val[1]);  //q1 q5
            int32x4x2_t val_2931_0 =
                vtrnq_s32(a[29].val[0], a[31].val[0]);  //q8 q12
            int32x4x2_t val_2931_1 =
                vtrnq_s32(a[29].val[1], a[31].val[1]);  //q9 q13

            a[25].val[0] = vcombine_s32(vget_low_s32(val_2527_0.val[0]),
                vget_low_s32(val_2931_0.val[0]));  //q0
            a[25].val[1] = vcombine_s32(vget_high_s32(val_2527_0.val[0]),
                vget_high_s32(val_2931_0.val[0]));  //q8

            a[27].val[0] = vcombine_s32(vget_low_s32(val_2527_0.val[1]),
                vget_low_s32(val_2931_0.val[1]));  //q1
            a[27].val[1] = vcombine_s32(vget_high_s32(val_2527_0.val[1]),
                vget_high_s32(val_2931_0.val[1]));  //q9

            a[29].val[0] = vcombine_s32(vget_low_s32(val_2527_1.val[0]),
                vget_low_s32(val_2931_1.val[0]));  //q4
            a[29].val[1] = vcombine_s32(vget_high_s32(val_2527_1.val[0]),
                vget_high_s32(val_2931_1.val[0]));  //q12

            a[31].val[0] = vcombine_s32(vget_low_s32(val_2527_1.val[1]),
                vget_low_s32(val_2931_1.val[1]));  //q5
            a[31].val[1] = vcombine_s32(vget_high_s32(val_2527_1.val[1]),
                vget_high_s32(val_2931_1.val[1]));  //q13

            a[25].val[0] = vaddq_s32(a[25].val[0], a[27].val[0]);  //q0
            a[25].val[1] = vaddq_s32(a[25].val[1], a[27].val[1]);  //q8
            a[25].val[1] = vaddq_s32(a[25].val[1], a[25].val[0]);  //q8

            a[29].val[0] = vaddq_s32(a[29].val[0], a[31].val[0]);  //q1
            a[29].val[1] = vaddq_s32(a[29].val[1], a[31].val[1]);  //q9
            a[29].val[1] = vaddq_s32(a[29].val[1], a[29].val[0]);  //q9

            int32x4x2_t val_25 = vzipq_s32(a[25].val[1], a[29].val[1]);  //q8 q9

            vst1_s32(pi4_temp + 800, vget_low_s32(val_25.val[0])); /*Value 25*/
            vst1_s32(pi4_temp + 864, vget_high_s32(val_25.val[0])); /*Value 27*/
            vst1_s32(pi4_temp + 928, vget_low_s32(val_25.val[1])); /*Value 29*/
            vst1_s32(pi4_temp + 992, vget_high_s32(val_25.val[1])); /*Value 31*/

            pi4_temp += 2;
        }
    }

    /*sad of the block*/
    tmp_a = vpaddlq_s32(sum_val);
    sad = vadd_s32(vreinterpret_s32_s64(vget_low_s64(tmp_a)),
                      vreinterpret_s32_s64(vget_high_s64(tmp_a)));
    u4_blk_sad = vget_lane_s32(sad, 0);

    //Stage 2
    {
        static const int32x4_t g_ai4_ihevc_trans_32_0_8 = { 64, -64, 83, -83 };
        static const int32x4_t g_ai4_ihevc_trans_32_1_8 = { 64, 64, 36, 36 };

        static const int32x4_t g_ai4_ihevc_trans_32_4_04 = { 89, 75, 50, 18 };
        static const int32x4_t g_ai4_ihevc_trans_32_12_04 = { 75, -18, -89, -50 };
        static const int32x4_t g_ai4_ihevc_trans_32_20_04 = { 50, -89, 18, 75 };
        static const int32x4_t g_ai4_ihevc_trans_32_28_04 = { 18, -50, 75, -89 };

        static const int32x4_t g_ai4_ihevc_trans_32_2_03 = { 90, 87, 80, 70 };
        static const int32x4_t g_ai4_ihevc_trans_32_2_47 = { 57, 43, 25, 9 };
        static const int32x4_t g_ai4_ihevc_trans_32_6_03 = { 87, 57, 9, -43 };
        static const int32x4_t g_ai4_ihevc_trans_32_6_47 = { -80, -90, -70,
            -25 };
        static const int32x4_t g_ai4_ihevc_trans_32_10_03 = { 80, 9, -70, -87 };
        static const int32x4_t g_ai4_ihevc_trans_32_10_47 = { -25, 57, 90, 43 };
        static const int32x4_t g_ai4_ihevc_trans_32_14_03 = { 70, -43, -87, 9 };
        static const int32x4_t g_ai4_ihevc_trans_32_14_47 = { 90, 25, -80, -57 };
        static const int32x4_t g_ai4_ihevc_trans_32_18_03 = { 57, -80, -25, 90 };
        static const int32x4_t g_ai4_ihevc_trans_32_18_47 = { -9, -87, 43, 70 };
        static const int32x4_t g_ai4_ihevc_trans_32_22_03 = { 43, -90, 57, 25 };
        static const int32x4_t g_ai4_ihevc_trans_32_22_47 = { -87, 70, 9, -80 };
        static const int32x4_t g_ai4_ihevc_trans_32_26_03 = { 25, -70, 90, -80 };
        static const int32x4_t g_ai4_ihevc_trans_32_26_47 = { 43, 9, -57, 87 };
        static const int32x4_t g_ai4_ihevc_trans_32_30_03 = { 9, -25, 43, -57 };
        static const int32x4_t g_ai4_ihevc_trans_32_30_47 = { 70, -80, 87, -90 };

        static const int32x4_t g_ai4_ihevc_trans_32_1_03 = { 90, 90, 88, 85 };
        static const int32x4_t g_ai4_ihevc_trans_32_1_47 = { 82, 78, 73, 67 };
        static const int32x4_t g_ai4_ihevc_trans_32_1_811 = { 61, 54, 46, 38 };
        static const int32x4_t g_ai4_ihevc_trans_32_1_1215 = { 31, 22, 13, 4 };
        static const int32x4_t g_ai4_ihevc_trans_32_3_03 = { 90, 82, 67, 46 };
        static const int32x4_t g_ai4_ihevc_trans_32_3_47 = { 22, -4, -31, -54 };
        static const int32x4_t g_ai4_ihevc_trans_32_3_811 = { -73, -85, -90, -88 };
        static const int32x4_t g_ai4_ihevc_trans_32_3_1215 = { -78, -61, -38, -13 };
        static const int32x4_t g_ai4_ihevc_trans_32_5_03 = { 88, 67, 31, -13 };
        static const int32x4_t g_ai4_ihevc_trans_32_5_47 = { -54, -82, -90, -78 };
        static const int32x4_t g_ai4_ihevc_trans_32_5_811 = { -46, -4, 38, 73 };
        static const int32x4_t g_ai4_ihevc_trans_32_5_1215 = { 90, 85, 61, 22 };
        static const int32x4_t g_ai4_ihevc_trans_32_7_03 = { 85, 46, -13, -67 };
        static const int32x4_t g_ai4_ihevc_trans_32_7_47 = { -90, -73, -22, 38 };
        static const int32x4_t g_ai4_ihevc_trans_32_7_811 = { 82, 88, 54, -4 };
        static const int32x4_t g_ai4_ihevc_trans_32_7_1215 = { -61, -90, -78, -31 };
        static const int32x4_t g_ai4_ihevc_trans_32_9_03 = { 82, 22, -54, -90 };
        static const int32x4_t g_ai4_ihevc_trans_32_9_47 = { -61, 13, 78, 85 };
        static const int32x4_t g_ai4_ihevc_trans_32_9_811 = { 31, -46, -90, -67 };
        static const int32x4_t g_ai4_ihevc_trans_32_9_1215 = { 4, 73, 88, 38 };
        static const int32x4_t g_ai4_ihevc_trans_32_11_03 = { 78, -4, -82, -73 };
        static const int32x4_t g_ai4_ihevc_trans_32_11_47 = { 13, 85, 67, -22 };
        static const int32x4_t g_ai4_ihevc_trans_32_11_811 = { -88, -61, 31, 90 };
        static const int32x4_t g_ai4_ihevc_trans_32_11_1215 = { 54, -38, -90, -46 };
        static const int32x4_t g_ai4_ihevc_trans_32_13_03 = { 73, -31, -90, -22 };
        static const int32x4_t g_ai4_ihevc_trans_32_13_47 = { 78, 67, -38, -90 };
        static const int32x4_t g_ai4_ihevc_trans_32_13_811 = { -13, 82, 61, -46 };
        static const int32x4_t g_ai4_ihevc_trans_32_13_1215 = { -88, -4, 85, 54 };
        static const int32x4_t g_ai4_ihevc_trans_32_15_03 = { 67, -54, -78, 38 };
        static const int32x4_t g_ai4_ihevc_trans_32_15_47 = { 85, -22, -90, 4 };
        static const int32x4_t g_ai4_ihevc_trans_32_15_811 = { 90, 13, -88, -31 };
        static const int32x4_t g_ai4_ihevc_trans_32_15_1215 = { 82, 46, -73, -61 };
        static const int32x4_t g_ai4_ihevc_trans_32_17_03 = { 61, -73, -46, 82 };
        static const int32x4_t g_ai4_ihevc_trans_32_17_47 = { 31, -88, -13, 90 };
        static const int32x4_t g_ai4_ihevc_trans_32_17_811 = { -4, -90, 22, 85 };
        static const int32x4_t g_ai4_ihevc_trans_32_17_1215 = { -38, -78, 54, 67 };
        static const int32x4_t g_ai4_ihevc_trans_32_19_03 = { 54, -85, -4, 88 };
        static const int32x4_t g_ai4_ihevc_trans_32_19_47 = { -46, -61, 82, 13 };
        static const int32x4_t g_ai4_ihevc_trans_32_19_811 = { -90, 38, 67, -78 };
        static const int32x4_t g_ai4_ihevc_trans_32_19_1215 = { -22, 90, -31, -73 };
        static const int32x4_t g_ai4_ihevc_trans_32_21_03 = { 46, -90, 38, 54 };
        static const int32x4_t g_ai4_ihevc_trans_32_21_47 = { -90, 31, 61, -88 };
        static const int32x4_t g_ai4_ihevc_trans_32_21_811 = { 22, 67, -85, 13 };
        static const int32x4_t g_ai4_ihevc_trans_32_21_1215 = { 73, -82, 4, 78 };
        static const int32x4_t g_ai4_ihevc_trans_32_23_03 = { 38, -88, 73, -4 };
        static const int32x4_t g_ai4_ihevc_trans_32_23_47 = { -67, 90, -46, -31 };
        static const int32x4_t g_ai4_ihevc_trans_32_23_811 = { 85, -78, 13, 61 };
        static const int32x4_t g_ai4_ihevc_trans_32_23_1215 = { -90, 54, 22, -82 };
        static const int32x4_t g_ai4_ihevc_trans_32_25_03 = { 31, -78, 90, -61 };
        static const int32x4_t g_ai4_ihevc_trans_32_25_47 = { 4, 54, -88, 82 };
        static const int32x4_t g_ai4_ihevc_trans_32_25_811 = { -38, -22, 73, -90 };
        static const int32x4_t g_ai4_ihevc_trans_32_25_1215 = { 67, -13, -46, 85 };
        static const int32x4_t g_ai4_ihevc_trans_32_27_03 = { 22, -61, 85, -90 };
        static const int32x4_t g_ai4_ihevc_trans_32_27_47 = { 73, -38, -4, 46 };
        static const int32x4_t g_ai4_ihevc_trans_32_27_811 = { -78, 90, -82, 54 };
        static const int32x4_t g_ai4_ihevc_trans_32_27_1215 = { -13, -31, 67, -88 };
        static const int32x4_t g_ai4_ihevc_trans_32_29_03 = { 13, -38, 61, -78 };
        static const int32x4_t g_ai4_ihevc_trans_32_29_47 = { 88, -90, 85, -73 };
        static const int32x4_t g_ai4_ihevc_trans_32_29_811 = { 54, -31, 4, 22 };
        static const int32x4_t g_ai4_ihevc_trans_32_29_1215 = { -46, 67, -82, 90 };
        static const int32x4_t g_ai4_ihevc_trans_32_31_03 = { 4, -13, 22, -31 };
        static const int32x4_t g_ai4_ihevc_trans_32_31_47 = { 38, -46, 54, -61 };
        static const int32x4_t g_ai4_ihevc_trans_32_31_811 = { 67, -73, 78, -82 };
        static const int32x4_t g_ai4_ihevc_trans_32_31_1215 = { 85, -88, 90, -90 };

        int32x4_t a[32];

        pi4_temp = pi4_temp_orig;
        for(i = 0; i < 32; i++)
        {
            int32x4_t temp_data[8];

            temp_data[0] = vld1q_s32(pi4_temp);
            temp_data[1] = vld1q_s32(pi4_temp + 4);
            temp_data[2] = vld1q_s32(pi4_temp + 8);
            temp_data[3] = vld1q_s32(pi4_temp + 12);

            temp_data[4] = vrev64q_s32(vld1q_s32(pi4_temp + 16));
            temp_data[4] = vcombine_s32(
                vget_high_s32(temp_data[4]), vget_low_s32(temp_data[4]));

            temp_data[5] = vrev64q_s32(vld1q_s32(pi4_temp + 20));
            temp_data[5] = vcombine_s32(
                vget_high_s32(temp_data[5]), vget_low_s32(temp_data[5]));

            temp_data[6] = vrev64q_s32(vld1q_s32(pi4_temp + 24));
            temp_data[6] = vcombine_s32(
                vget_high_s32(temp_data[6]), vget_low_s32(temp_data[6]));

            temp_data[7] = vrev64q_s32(vld1q_s32(pi4_temp + 28));
            temp_data[7] = vcombine_s32(
                vget_high_s32(temp_data[7]), vget_low_s32(temp_data[7]));

            pi4_temp += 32;

            const int32x4_t o0 =
                vsubq_s32(temp_data[0], temp_data[7]); /*R2(9-16) - R2(24-17)*/
            const int32x4_t o1 =
                vsubq_s32(temp_data[1], temp_data[6]); /*R2(1- 8) - R2(32-25)*/
            const int32x4_t o2 =
                vsubq_s32(temp_data[2], temp_data[5]); /*R1(9-16) - R1(24-17)*/
            const int32x4_t o3 =
                vsubq_s32(temp_data[3], temp_data[4]); /*R1(1- 8) - R1(32-25)*/

            int32x4_t e3 = vrev64q_s32(
                vaddq_s32(temp_data[3], temp_data[4])); /*R1(1- 8) + R1(32-25)*/
            e3 = vcombine_s32(vget_high_s32(e3), vget_low_s32(e3));
            int32x4_t e2 = vrev64q_s32(
                vaddq_s32(temp_data[2], temp_data[5])); /*R1(9-16) + R1(24-17)*/
            e2 = vcombine_s32(vget_high_s32(e2), vget_low_s32(e2));

            const int32x4_t e1 =
                vaddq_s32(temp_data[1], temp_data[6]); /*R2(1- 8) + R2(32-25)*/
            const int32x4_t e0 =
                vaddq_s32(temp_data[0], temp_data[7]); /*R2(9-16) + R2(24-17)*/

            const int32x4_t ee0 = vaddq_s32(e0, e3); /*E1(1- 8) + E1(16-9)*/
            int32x4_t ee1 =
                vrev64q_s32(vaddq_s32(e1, e2)); /*E2(1- 8) + E2(16-9)*/
            ee1 = vcombine_s32(vget_high_s32(ee1), vget_low_s32(ee1));
            const int32x4_t eo1 = vsubq_s32(e1, e2); /*E2(1- 8) - E2(16-9)*/
            const int32x4_t eo0 = vsubq_s32(e0, e3); /*E1(1- 8) - E1(16-9)*/

            /*EE(1-4) - EE(8-5)*/
            const int32x4_t eeo = vsubq_s32(ee0, ee1);  //Q5
            /*EE(1-4) + EE(8-5)*/
            const int32x4_t eee = vaddq_s32(ee0, ee1);  //Q4

            /*EEEE Calculations*/
            const int32x4_t eeee = vcombine_s32(
                vadd_s32(vget_low_s32(eee), vrev64_s32(vget_high_s32(eee))),
                vsub_s32(
                    vget_low_s32(eee), vrev64_s32(vget_high_s32(eee))));  //q6

            /*Calculation of values 0 8 16 24*/
            /*Multiplications*/
            a[0] = vmulq_s32(g_ai4_ihevc_trans_32_0_8, eeee);
            a[0] = vmlaq_s32(a[0], g_ai4_ihevc_trans_32_1_8, vrev64q_s32(eeee));
            /*Shift*/
            int16x4_t val_0 = vrshrn_n_s32(a[0], 15);
            /*Store*/
            vst1_lane_s16(pi2_dst, val_0, 0); /*Value 0*/
            vst1_lane_s16(pi2_dst + 8 * dst_strd, val_0, 2); /*Value 8*/
            vst1_lane_s16(pi2_dst + 16 * dst_strd, val_0, 1); /*Value 16*/
            vst1_lane_s16(pi2_dst + 24 * dst_strd, val_0, 3); /*Value 24*/

            /*Calculation of values 4 12 20 28*/
            /*Multiplications*/
            a[4] = vmulq_s32(g_ai4_ihevc_trans_32_4_04, eeo);
            a[12] = vmulq_s32(g_ai4_ihevc_trans_32_12_04, eeo);
            a[20] = vmulq_s32(g_ai4_ihevc_trans_32_20_04, eeo);
            a[28] = vmulq_s32(g_ai4_ihevc_trans_32_28_04, eeo);
            /*Transposes*/
            int32x4x2_t val_412 = vtrnq_s32(a[4], a[12]);  //q0 q9
            int32x4x2_t val_2028 = vtrnq_s32(a[20], a[28]);  //q10 q11
            /*Swap*/
            a[4] = vcombine_s32(vget_low_s32(val_412.val[0]),
                vget_low_s32(val_2028.val[0]));  //q0
            a[12] = vcombine_s32(vget_low_s32(val_412.val[1]),
                vget_low_s32(val_2028.val[1]));  //q9
            a[20] = vcombine_s32(vget_high_s32(val_412.val[0]),
                vget_high_s32(val_2028.val[0]));  //q10
            a[28] = vcombine_s32(vget_high_s32(val_412.val[1]),
                vget_high_s32(val_2028.val[1]));  //q11
            /*Additions*/
            a[4] = vaddq_s32(a[4], a[12]);  //q0
            a[20] = vaddq_s32(a[20], a[28]);  //q10
            a[4] = vaddq_s32(a[4], a[20]);  //q0
            /*Shift*/
            int16x4_t val_4 = vrshrn_n_s32(a[4], 15);
            /*Store*/
            vst1_lane_s16(pi2_dst + 4 * dst_strd, val_4, 0); /*Value 4*/
            vst1_lane_s16(pi2_dst + 12 * dst_strd, val_4, 1); /*Value 12*/
            vst1_lane_s16(pi2_dst + 20 * dst_strd, val_4, 2); /*Value 20*/
            vst1_lane_s16(pi2_dst + 28 * dst_strd, val_4, 3); /*Value 28*/

            /*Calculation of value 2 6 10 14 18 22 26 30*/
            /*Multiplications*/
            a[2] = vmulq_s32(g_ai4_ihevc_trans_32_2_03, eo0);  //q8
            a[6] = vmulq_s32(g_ai4_ihevc_trans_32_6_03, eo0);  //q2
            a[10] = vmulq_s32(g_ai4_ihevc_trans_32_10_03, eo0);  //q2
            a[14] = vmulq_s32(g_ai4_ihevc_trans_32_14_03, eo0);  //q2

            a[14] = vmlaq_s32(a[14], g_ai4_ihevc_trans_32_14_47, eo1);
            a[10] = vmlaq_s32(a[10], g_ai4_ihevc_trans_32_10_47, eo1);
            a[6] = vmlaq_s32(a[6], g_ai4_ihevc_trans_32_6_47, eo1);
            a[2] = vmlaq_s32(a[2], g_ai4_ihevc_trans_32_2_47, eo1);

            int32x2_t val_2 = vadd_s32(vget_low_s32(a[2]), vget_high_s32(a[2]));
            int32x2_t val_6 = vadd_s32(vget_low_s32(a[6]), vget_high_s32(a[6]));
            val_2 = vpadd_s32(val_2, val_6);

            int32x2_t val_10 =
                vadd_s32(vget_low_s32(a[10]), vget_high_s32(a[10]));
            int32x2_t val_14 =
                vadd_s32(vget_low_s32(a[14]), vget_high_s32(a[14]));
            val_10 = vpadd_s32(val_10, val_14);

            /*Shift*/
            int16x4_t val__2 =
                vrshrn_n_s32(vcombine_s32(val_2, val_10), 15);  //q9 q12

            /*Store*/
            vst1_lane_s16(pi2_dst + 2 * dst_strd, val__2, 0); /*Value 2*/
            vst1_lane_s16(pi2_dst + 6 * dst_strd, val__2, 1); /*Value 6*/
            vst1_lane_s16(pi2_dst + 10 * dst_strd, val__2, 2); /*Value 10*/
            vst1_lane_s16(pi2_dst + 14 * dst_strd, val__2, 3); /*Value 14*/

            a[18] = vmulq_s32(g_ai4_ihevc_trans_32_18_03, eo0);  //q2
            a[22] = vmulq_s32(g_ai4_ihevc_trans_32_22_03, eo0);  //q2
            a[26] = vmulq_s32(g_ai4_ihevc_trans_32_26_03, eo0);  //q2
            a[30] = vmulq_s32(g_ai4_ihevc_trans_32_30_03, eo0);  //q2

            a[30] = vmlaq_s32(a[30], g_ai4_ihevc_trans_32_30_47, eo1);
            a[26] = vmlaq_s32(a[26], g_ai4_ihevc_trans_32_26_47, eo1);
            a[22] = vmlaq_s32(a[22], g_ai4_ihevc_trans_32_22_47, eo1);
            a[18] = vmlaq_s32(a[18], g_ai4_ihevc_trans_32_18_47, eo1);

            int32x2_t val_18 =
                vadd_s32(vget_low_s32(a[18]), vget_high_s32(a[18]));
            int32x2_t val_22 =
                vadd_s32(vget_low_s32(a[22]), vget_high_s32(a[22]));
            val_18 = vpadd_s32(val_18, val_22);
            int32x2_t val_26 =
                vadd_s32(vget_low_s32(a[26]), vget_high_s32(a[26]));
            int32x2_t val_30 =
                vadd_s32(vget_low_s32(a[30]), vget_high_s32(a[30]));
            val_26 = vpadd_s32(val_26, val_30);

            int16x4_t val__18 =
                vrshrn_n_s32(vcombine_s32(val_18, val_26), 15);  //q9 q12

            vst1_lane_s16(pi2_dst + 18 * dst_strd, val__18, 0); /*Value 18*/
            vst1_lane_s16(pi2_dst + 22 * dst_strd, val__18, 1); /*Value 22*/
            vst1_lane_s16(pi2_dst + 26 * dst_strd, val__18, 2); /*Value 26*/
            vst1_lane_s16(pi2_dst + 30 * dst_strd, val__18, 3); /*Value 30*/

            /*Calculations for odd indexes*/
            a[7] = vmulq_s32(g_ai4_ihevc_trans_32_7_03, o0);  //q1
            a[5] = vmulq_s32(g_ai4_ihevc_trans_32_5_03, o0);  //q1
            a[3] = vmulq_s32(g_ai4_ihevc_trans_32_3_03, o0);  //q1
            a[1] = vmulq_s32(g_ai4_ihevc_trans_32_1_03, o0);  //q1

            a[1] = vmlaq_s32(a[1], g_ai4_ihevc_trans_32_1_47, o1);
            a[3] = vmlaq_s32(a[3], g_ai4_ihevc_trans_32_3_47, o1);
            a[5] = vmlaq_s32(a[5], g_ai4_ihevc_trans_32_5_47, o1);
            a[7] = vmlaq_s32(a[7], g_ai4_ihevc_trans_32_7_47, o1);

            a[7] = vmlaq_s32(a[7], g_ai4_ihevc_trans_32_7_811, o2);
            a[5] = vmlaq_s32(a[5], g_ai4_ihevc_trans_32_5_811, o2);
            a[3] = vmlaq_s32(a[3], g_ai4_ihevc_trans_32_3_811, o2);
            a[1] = vmlaq_s32(a[1], g_ai4_ihevc_trans_32_1_811, o2);

            a[1] = vmlaq_s32(a[1], g_ai4_ihevc_trans_32_1_1215, o3);
            int32x2_t val_1 = vadd_s32(vget_low_s32(a[1]), vget_high_s32(a[1]));
            a[3] = vmlaq_s32(a[3], g_ai4_ihevc_trans_32_3_1215, o3);
            int32x2_t val_3 = vadd_s32(vget_low_s32(a[3]), vget_high_s32(a[3]));
            val_1 = vpadd_s32(val_1, val_3);
            a[5] = vmlaq_s32(a[5], g_ai4_ihevc_trans_32_5_1215, o3);
            int32x2_t val_5 = vadd_s32(vget_low_s32(a[5]), vget_high_s32(a[5]));
            a[7] = vmlaq_s32(a[7], g_ai4_ihevc_trans_32_7_1215, o3);
            int32x2_t val_7 = vadd_s32(vget_low_s32(a[7]), vget_high_s32(a[7]));
            val_5 = vpadd_s32(val_5, val_7);

            /*Shift*/
            int16x4_t val__1 =
                vrshrn_n_s32(vcombine_s32(val_1, val_5), 15);  //q9 q12

            /*Store*/
            vst1_lane_s16(pi2_dst + 1 * dst_strd, val__1, 0); /*Value 1*/
            vst1_lane_s16(pi2_dst + 3 * dst_strd, val__1, 1); /*Value 3*/
            vst1_lane_s16(pi2_dst + 5 * dst_strd, val__1, 2); /*Value 5*/
            vst1_lane_s16(pi2_dst + 7 * dst_strd, val__1, 3); /*Value 7*/

            a[15] = vmulq_s32(g_ai4_ihevc_trans_32_15_03, o0);  //q1
            a[13] = vmulq_s32(g_ai4_ihevc_trans_32_13_03, o0);  //q1
            a[11] = vmulq_s32(g_ai4_ihevc_trans_32_11_03, o0);  //q1
            a[9] = vmulq_s32(g_ai4_ihevc_trans_32_9_03, o0);  //q1

            a[9] = vmlaq_s32(a[9], g_ai4_ihevc_trans_32_9_47, o1);
            a[11] = vmlaq_s32(a[11], g_ai4_ihevc_trans_32_11_47, o1);
            a[13] = vmlaq_s32(a[13], g_ai4_ihevc_trans_32_13_47, o1);
            a[15] = vmlaq_s32(a[15], g_ai4_ihevc_trans_32_15_47, o1);

            a[15] = vmlaq_s32(a[15], g_ai4_ihevc_trans_32_15_811, o2);
            a[13] = vmlaq_s32(a[13], g_ai4_ihevc_trans_32_13_811, o2);
            a[11] = vmlaq_s32(a[11], g_ai4_ihevc_trans_32_11_811, o2);
            a[9] = vmlaq_s32(a[9], g_ai4_ihevc_trans_32_9_811, o2);

            a[9] = vmlaq_s32(a[9], g_ai4_ihevc_trans_32_9_1215, o3);
            int32x2_t val_9 = vadd_s32(vget_low_s32(a[9]), vget_high_s32(a[9]));
            a[11] = vmlaq_s32(a[11], g_ai4_ihevc_trans_32_11_1215, o3);
            int32x2_t val_11 =
                vadd_s32(vget_low_s32(a[11]), vget_high_s32(a[11]));
            val_9 = vpadd_s32(val_9, val_11);
            a[13] = vmlaq_s32(a[13], g_ai4_ihevc_trans_32_13_1215, o3);
            int32x2_t val_13 =
                vadd_s32(vget_low_s32(a[13]), vget_high_s32(a[13]));
            a[15] = vmlaq_s32(a[15], g_ai4_ihevc_trans_32_15_1215, o3);
            int32x2_t val_15 =
                vadd_s32(vget_low_s32(a[15]), vget_high_s32(a[15]));
            val_13 = vpadd_s32(val_13, val_15);

            int16x4_t val__9 =
                vrshrn_n_s32(vcombine_s32(val_9, val_13), 15);  //q9 q12

            vst1_lane_s16(pi2_dst + 9 * dst_strd, val__9, 0); /*Value 9*/
            vst1_lane_s16(pi2_dst + 11 * dst_strd, val__9, 1); /*Value 11*/
            vst1_lane_s16(pi2_dst + 13 * dst_strd, val__9, 2); /*Value 13*/
            vst1_lane_s16(pi2_dst + 15 * dst_strd, val__9, 3); /*Value 15*/

            a[23] = vmulq_s32(g_ai4_ihevc_trans_32_23_03, o0);  //q1
            a[21] = vmulq_s32(g_ai4_ihevc_trans_32_21_03, o0);  //q1
            a[19] = vmulq_s32(g_ai4_ihevc_trans_32_19_03, o0);  //q1
            a[17] = vmulq_s32(g_ai4_ihevc_trans_32_17_03, o0);  //q1

            a[17] = vmlaq_s32(a[17], g_ai4_ihevc_trans_32_17_47, o1);
            a[19] = vmlaq_s32(a[19], g_ai4_ihevc_trans_32_19_47, o1);
            a[21] = vmlaq_s32(a[21], g_ai4_ihevc_trans_32_21_47, o1);
            a[23] = vmlaq_s32(a[23], g_ai4_ihevc_trans_32_23_47, o1);

            a[23] = vmlaq_s32(a[23], g_ai4_ihevc_trans_32_23_811, o2);
            a[21] = vmlaq_s32(a[21], g_ai4_ihevc_trans_32_21_811, o2);
            a[19] = vmlaq_s32(a[19], g_ai4_ihevc_trans_32_19_811, o2);
            a[17] = vmlaq_s32(a[17], g_ai4_ihevc_trans_32_17_811, o2);

            a[17] = vmlaq_s32(a[17], g_ai4_ihevc_trans_32_17_1215, o3);
            int32x2_t val_17 =
                vadd_s32(vget_low_s32(a[17]), vget_high_s32(a[17]));
            a[19] = vmlaq_s32(a[19], g_ai4_ihevc_trans_32_19_1215, o3);
            int32x2_t val_19 =
                vadd_s32(vget_low_s32(a[19]), vget_high_s32(a[19]));
            val_17 = vpadd_s32(val_17, val_19);
            a[21] = vmlaq_s32(a[21], g_ai4_ihevc_trans_32_21_1215, o3);
            int32x2_t val_21 =
                vadd_s32(vget_low_s32(a[21]), vget_high_s32(a[21]));
            a[23] = vmlaq_s32(a[23], g_ai4_ihevc_trans_32_23_1215, o3);
            int32x2_t val_23 =
                vadd_s32(vget_low_s32(a[23]), vget_high_s32(a[23]));
            val_21 = vpadd_s32(val_21, val_23);

            int16x4_t val__17 =
                vrshrn_n_s32(vcombine_s32(val_17, val_21), 15);  //q9 q12

            vst1_lane_s16(pi2_dst + 17 * dst_strd, val__17, 0); /*Value 17*/
            vst1_lane_s16(pi2_dst + 19 * dst_strd, val__17, 1); /*Value 19*/
            vst1_lane_s16(pi2_dst + 21 * dst_strd, val__17, 2); /*Value 21*/
            vst1_lane_s16(pi2_dst + 23 * dst_strd, val__17, 3); /*Value 23*/

            a[31] = vmulq_s32(g_ai4_ihevc_trans_32_31_03, o0);  //q10
            a[29] = vmulq_s32(g_ai4_ihevc_trans_32_29_03, o0);  //q1
            a[27] = vmulq_s32(g_ai4_ihevc_trans_32_27_03, o0);  //q1
            a[25] = vmulq_s32(g_ai4_ihevc_trans_32_25_03, o0);  //q1

            a[25] = vmlaq_s32(a[25], g_ai4_ihevc_trans_32_25_47, o1);
            a[27] = vmlaq_s32(a[27], g_ai4_ihevc_trans_32_27_47, o1);
            a[29] = vmlaq_s32(a[29], g_ai4_ihevc_trans_32_29_47, o1);
            a[31] = vmlaq_s32(a[31], g_ai4_ihevc_trans_32_31_47, o1);

            a[31] = vmlaq_s32(a[31], g_ai4_ihevc_trans_32_31_811, o2);
            a[29] = vmlaq_s32(a[29], g_ai4_ihevc_trans_32_29_811, o2);
            a[27] = vmlaq_s32(a[27], g_ai4_ihevc_trans_32_27_811, o2);
            a[25] = vmlaq_s32(a[25], g_ai4_ihevc_trans_32_25_811, o2);

            a[25] = vmlaq_s32(a[25], g_ai4_ihevc_trans_32_25_1215, o3);
            int32x2_t val_25 =
                vadd_s32(vget_low_s32(a[25]), vget_high_s32(a[25]));
            a[27] = vmlaq_s32(a[27], g_ai4_ihevc_trans_32_27_1215, o3);
            int32x2_t val_27 =
                vadd_s32(vget_low_s32(a[27]), vget_high_s32(a[27]));
            val_25 = vpadd_s32(val_25, val_27);
            a[29] = vmlaq_s32(a[29], g_ai4_ihevc_trans_32_29_1215, o3);
            int32x2_t val_29 =
                vadd_s32(vget_low_s32(a[29]), vget_high_s32(a[29]));
            a[31] = vmlaq_s32(a[31], g_ai4_ihevc_trans_32_31_1215, o3);
            int32x2_t val_31 =
                vadd_s32(vget_low_s32(a[31]), vget_high_s32(a[31]));
            val_29 = vpadd_s32(val_29, val_31);

            int16x4_t val__25 =
                vrshrn_n_s32(vcombine_s32(val_25, val_29), 15);  //q9 q12

            vst1_lane_s16(pi2_dst + 25 * dst_strd, val__25, 0); /*Value 25*/
            vst1_lane_s16(pi2_dst + 27 * dst_strd, val__25, 1); /*Value 27*/
            vst1_lane_s16(pi2_dst + 29 * dst_strd, val__25, 2); /*Value 29*/
            vst1_lane_s16(pi2_dst + 31 * dst_strd, val__25, 3); /*Value 31*/

            pi2_dst++;
        }
    }
    return u4_blk_sad;
}
