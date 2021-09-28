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
*  ihevce_itrans_recon_neon.c
*
* @brief
*  Contains functions to inverse transform and adds residue to pred buffer
*
* @author
*  Ittiam
*
* @par List of Functions:
*  - ihevce_itrans_recon_dc_neon
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
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_cmn_utils_neon.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevc_defs.h"
#include "ihevc_macros.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
static INLINE void ihevce_itrans_recon_dc_4x4_luma_neon(
    UWORD8 *pu1_pred, WORD32 pred_strd, UWORD8 *pu1_dst, WORD32 dst_strd, WORD32 dc_value)
{
    uint8x16_t src_u8;
    int16x8_t a0, a1, a2;
    uint8x8_t a3, a4;

    src_u8 = load_unaligned_u8q(pu1_pred, pred_strd);
    a0 = vdupq_n_s16(dc_value);
    a1 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(src_u8)));
    a2 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(src_u8)));
    a1 = vaddq_s16(a1, a0);
    a2 = vaddq_s16(a2, a0);
    a3 = vqmovun_s16(a1);
    a4 = vqmovun_s16(a2);
    uint32x2_t p0 = vreinterpret_u32_u8(a3);
    uint32x2_t p1 = vreinterpret_u32_u8(a4);
    *(UWORD32 *)(pu1_dst) = vget_lane_u32(p0, 0);
    *(UWORD32 *)(pu1_dst + dst_strd) = vget_lane_u32(p0, 1);
    *(UWORD32 *)(pu1_dst + 2 * dst_strd) = vget_lane_u32(p1, 0);
    *(UWORD32 *)(pu1_dst + 3 * dst_strd) = vget_lane_u32(p1, 1);
}

static INLINE void ihevce_itrans_recon_dc_4x4_chroma_neon(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD32 dc_value,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 i;
    int16x8_t a0, a1;
    uint8x8_t a2, a3;
    uint16x4_t select = vdup_n_u16(0xff << (e_chroma_plane << 3));

    a0 = vdupq_n_s16(dc_value);
    for(i = 0; i < trans_size; i++)
    {
        a1 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(pu1_pred + i * pred_strd)));
        a2 = vqmovun_s16(vaddq_s16(a0, a1));
        a3 = vld1_u8(pu1_dst + i * dst_strd);
        a3 = vbsl_u8(vreinterpret_u8_u16(select), a2, a3);
        vst1_u8(pu1_dst + i * dst_strd, a3);
    }
}

static INLINE void ihevce_itrans_recon_dc_8x8_luma_neon(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD32 dc_value)
{
    WORD32 i;
    uint8x16_t a1, a4, a5;
    uint8x8_t a0, a2, a3;

    a0 = (dc_value >= 0) ? vqmovun_s16(vdupq_n_s16(dc_value))
                         : vqmovun_s16(vabsq_s16(vdupq_n_s16(dc_value)));
    a1 = vcombine_u8(a0, a0);
    for(i = 0; i < trans_size; i += 2)
    {
        a2 = vld1_u8(pu1_pred + i * pred_strd);
        a3 = vld1_u8(pu1_pred + (i + 1) * pred_strd);
        a4 = vcombine_u8(a2, a3);
        a5 = (dc_value >= 0) ? vqaddq_u8(a1, a4) : vqsubq_u8(a4, a1);
        vst1_u8(pu1_dst + i * dst_strd, vget_low_u8(a5));
        vst1_u8(pu1_dst + (i + 1) * dst_strd, vget_high_u8(a5));
    }
}

static INLINE void ihevce_itrans_recon_dc_8x8_chroma_neon(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD32 dc_value,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 i;
    uint8x16_t a4, a0, a5;
    uint8x8_t a1, a2, a3;
    uint8x16x2_t a6;

    a1 = (dc_value >= 0) ? vqmovun_s16(vdupq_n_s16(dc_value))
                         : vqmovun_s16(vabsq_s16(vdupq_n_s16(dc_value)));
    a0 = vcombine_u8(a1, a1);
    for(i = 0; i < trans_size; i += 2)
    {
        a2 = vld2_u8(pu1_pred + i * pred_strd).val[e_chroma_plane];
        a3 = vld2_u8(pu1_pred + (i + 1) * pred_strd).val[e_chroma_plane];
        a4 = vcombine_u8(a2, a3);
        a4 = (dc_value >= 0) ? vqaddq_u8(a0, a4) : vqsubq_u8(a4, a0);
        a2 = vld2_u8(pu1_dst + i * dst_strd).val[!e_chroma_plane];
        a3 = vld2_u8(pu1_dst + (i + 1) * dst_strd).val[!e_chroma_plane];
        a5 = vcombine_u8(a2, a3);
        a6 = (e_chroma_plane == 0) ? vzipq_u8(a4, a5) : vzipq_u8(a5, a4);
        vst1q_u8(pu1_dst + i * dst_strd, a6.val[0]);
        vst1q_u8(pu1_dst + (i + 1) * dst_strd, a6.val[1]);
    }
}

static INLINE void ihevce_itrans_recon_dc_16x16_luma_neon(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD32 dc_value)
{
    WORD32 i;
    uint8x16_t a1, a3, a2;
    uint8x8_t a0;

    a0 = (dc_value >= 0) ? vqmovun_s16(vdupq_n_s16(dc_value))
                         : vqmovun_s16(vabsq_s16(vdupq_n_s16(dc_value)));
    a1 = vcombine_u8(a0, a0);
    for(i = 0; i < trans_size; i++)
    {
        a2 = vld1q_u8(pu1_pred + i * pred_strd);
        a3 = (dc_value >= 0) ? vqaddq_u8(a2, a1) : vqsubq_u8(a2, a1);
        vst1q_u8(pu1_dst + i * dst_strd, a3);
    }
}

static INLINE void ihevce_itrans_recon_dc_16x16_chroma_neon(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD32 dc_value,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 i;
    uint8x8_t a0;
    uint8x16_t a1, a2, a3;
    uint8x16x2_t a4;

    a0 = (dc_value >= 0) ? vqmovun_s16(vdupq_n_s16(dc_value))
                         : vqmovun_s16(vabsq_s16(vdupq_n_s16(dc_value)));
    a1 = vcombine_u8(a0, a0);
    for(i = 0; i < trans_size; i++)
    {
        a2 = vld2q_u8(pu1_pred + i * pred_strd).val[e_chroma_plane];
        a2 = (dc_value >= 0) ? vqaddq_u8(a2, a1) : vqsubq_u8(a2, a1);
        a3 = vld2q_u8(pu1_dst + i * dst_strd).val[!e_chroma_plane];
        a4 = (e_chroma_plane == 0) ? vzipq_u8(a2, a3) : vzipq_u8(a3, a2);
        vst1q_u8(pu1_dst + i * dst_strd, a4.val[0]);
        vst1q_u8(pu1_dst + i * dst_strd + 16, a4.val[1]);
    }
}

void ihevce_itrans_recon_dc_neon(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD16 i2_deq_value,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 add, shift;
    WORD32 dc_value;

    shift = IT_SHIFT_STAGE_1;
    add = 1 << (shift - 1);
    dc_value = CLIP_S16((i2_deq_value * 64 + add) >> shift);
    shift = IT_SHIFT_STAGE_2;
    add = 1 << (shift - 1);
    dc_value = CLIP_S16((dc_value * 64 + add) >> shift);

    switch(trans_size)
    {
    case 4:
        if(NULL_PLANE == e_chroma_plane)
        {
            ihevce_itrans_recon_dc_4x4_luma_neon(pu1_pred, pred_strd, pu1_dst, dst_strd, dc_value);
        }
        else
        {
            ihevce_itrans_recon_dc_4x4_chroma_neon(
                pu1_pred, pred_strd, pu1_dst, dst_strd, trans_size, dc_value, e_chroma_plane);
        }
        break;

    case 8:
        if(NULL_PLANE == e_chroma_plane)
        {
            ihevce_itrans_recon_dc_8x8_luma_neon(
                pu1_pred, pred_strd, pu1_dst, dst_strd, trans_size, dc_value);
        }
        else
        {
            ihevce_itrans_recon_dc_8x8_chroma_neon(
                pu1_pred, pred_strd, pu1_dst, dst_strd, trans_size, dc_value, e_chroma_plane);
        }
        break;

    case 16:
        if(NULL_PLANE == e_chroma_plane)
        {
            ihevce_itrans_recon_dc_16x16_luma_neon(
                pu1_pred, pred_strd, pu1_dst, dst_strd, trans_size, dc_value);
        }
        else
        {
            ihevce_itrans_recon_dc_16x16_chroma_neon(
                pu1_pred, pred_strd, pu1_dst, dst_strd, trans_size, dc_value, e_chroma_plane);
        }
        break;

    case 32:
        if(NULL_PLANE == e_chroma_plane)
        {
            WORD32 b16;

            for(b16 = 0; b16 < 4; b16++)
            {
                ihevce_itrans_recon_dc_16x16_luma_neon(
                    pu1_pred + ((b16 >> 1) * pred_strd * 16) + ((b16 & 1) * 16),
                    pred_strd,
                    pu1_dst + ((b16 >> 1) * dst_strd * 16) + ((b16 & 1) * 16),
                    dst_strd,
                    trans_size >> 1,
                    dc_value);
            }
        }
        else
        {
            WORD32 b16;

            for(b16 = 0; b16 < 4; b16++)
            {
                ihevce_itrans_recon_dc_16x16_chroma_neon(
                    pu1_pred + ((b16 >> 1) * pred_strd * 16) + ((b16 & 1) * 32),
                    pred_strd,
                    pu1_dst + ((b16 >> 1) * dst_strd * 16) + ((b16 & 1) * 32),
                    dst_strd,
                    trans_size >> 1,
                    dc_value,
                    e_chroma_plane);
            }
        }
        break;
    }
}
