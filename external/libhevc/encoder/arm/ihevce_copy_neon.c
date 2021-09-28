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
*  ihevce_copy_neon.c
*
* @brief
*  Contains intrinsic definitions of functions for block copy
*
* @author
*  ittiam
*
* @par List of Functions:
*  - ihevce_2d_square_copy_luma_neon()
*  - ihevce_copy_2d_neon()
*  - ihevce_chroma_interleave_2d_copy_neon()
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
#include <string.h>
#include <assert.h>
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_platform_macros.h"

#include "ihevce_cmn_utils_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

void ihevce_chroma_interleave_2d_copy_neon(
    UWORD8 *pu1_uv_src,
    WORD32 src_strd,
    UWORD8 *pu1_uv_dst,
    WORD32 dst_strd,
    WORD32 w,
    WORD32 h,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    (void)h;
    assert(w == h);
    assert((e_chroma_plane == U_PLANE) || (e_chroma_plane == V_PLANE));

    if(w == 4)
    {
        uint16x4_t select = vdup_n_u16(0xff << (e_chroma_plane << 3));

        for(; w > 0; w--)
        {
            uint8x8_t src_0, dst_0;

            // row 0
            src_0 = vld1_u8(pu1_uv_src);
            dst_0 = vld1_u8(pu1_uv_dst);
            dst_0 = vbsl_u8(vreinterpret_u8_u16(select), src_0, dst_0);
            vst1_u8(pu1_uv_dst, dst_0);
            pu1_uv_src += src_strd;
            pu1_uv_dst += dst_strd;
        }
    }
    else
    {
        uint16x8_t select = vdupq_n_u16(0xff << (e_chroma_plane << 3));
        WORD32 i, j;

        assert(w % 8 == 0);
        for(j = 0; j < w; j += 1)
        {
            UWORD8 *dst_ol = pu1_uv_dst + j * dst_strd;
            UWORD8 *src_ol = pu1_uv_src + j * src_strd;

            for(i = 0; i < w; i += 8)
            {
                UWORD8 *dst_il = dst_ol + (i * 2);
                UWORD8 *src_il = src_ol + (i * 2);
                uint8x16_t src_0, dst_0;

                // row 0
                src_0 = vld1q_u8(src_il);
                dst_0 = vld1q_u8(dst_il);
                dst_0 = vbslq_u8(vreinterpretq_u8_u16(select), src_0, dst_0);
                vst1q_u8(dst_il, dst_0);
            }
        }
    }
}

static void copy_2d_neon(
    UWORD8 *pu1_dst, WORD32 dst_strd, UWORD8 *pu1_src, WORD32 src_strd, WORD32 blk_wd, WORD32 blk_ht)
{
    assert(blk_wd == 4 || blk_wd == 8 || blk_wd == 16 || blk_wd == 32 || (blk_wd % 64 == 0));

    if(blk_wd == 4)
    {
        for(; blk_ht > 0; blk_ht--)
        {
            memcpy(pu1_dst, pu1_src, 4);
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
    else if(blk_wd == 8)
    {
        for(; blk_ht > 0; blk_ht--)
        {
            uint8x8_t src = vld1_u8(pu1_src);

            vst1_u8(pu1_dst, src);
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
    else if(blk_wd == 16)
    {
        for(; blk_ht > 0; blk_ht--)
        {
            uint8x16_t src = vld1q_u8(pu1_src);

            vst1q_u8(pu1_dst, src);
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
    else if(blk_wd == 32)
    {
        for(; blk_ht > 0; blk_ht--)
        {
            uint8x16_t src_0, src_1;

            // row 0
            src_0 = vld1q_u8(pu1_src);
            vst1q_u8(pu1_dst, src_0);
            src_1 = vld1q_u8(pu1_src + 16);
            vst1q_u8(pu1_dst + 16, src_1);
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
    }
    else if(blk_wd % 64 == 0)
    {
        WORD32 i, j;

        for(j = 0; j < blk_ht; j += 1)
        {
            UWORD8 *dst_ol = pu1_dst + j * dst_strd;
            UWORD8 *src_ol = pu1_src + j * src_strd;

            for(i = 0; i < blk_wd; i += 64)
            {
                uint8x16_t src_0, src_1, src_2, src_3;
                UWORD8 *dst_il = dst_ol + i;
                UWORD8 *src_il = src_ol + i;

                src_0 = vld1q_u8(src_il);
                vst1q_u8(dst_il, src_0);
                src_1 = vld1q_u8(src_il + 16);
                vst1q_u8(dst_il + 16, src_1);
                src_2 = vld1q_u8(src_il + 32);
                vst1q_u8(dst_il + 32, src_2);
                src_3 = vld1q_u8(src_il + 48);
                vst1q_u8(dst_il + 48, src_3);
            }
        }
    }
}

void ihevce_2d_square_copy_luma_neon(
    void *p_dst,
    WORD32 dst_strd,
    void *p_src,
    WORD32 src_strd,
    WORD32 num_cols_to_copy,
    WORD32 unit_size)
{
    UWORD8 *pu1_dst = (UWORD8 *)p_dst;
    UWORD8 *pu1_src = (UWORD8 *)p_src;

    copy_2d_neon(
        pu1_dst,
        dst_strd * unit_size,
        pu1_src,
        src_strd * unit_size,
        num_cols_to_copy * unit_size,
        num_cols_to_copy);
}

void ihevce_copy_2d_neon(
    UWORD8 *pu1_dst, WORD32 dst_strd, UWORD8 *pu1_src, WORD32 src_strd, WORD32 blk_wd, WORD32 blk_ht)
{
    if(blk_wd == 0)
        return;

    if(blk_wd > 64)
    {
        copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 64, blk_ht);
        ihevce_copy_2d_neon(pu1_dst + 64, dst_strd, pu1_src + 64, src_strd, blk_wd - 64, blk_ht);
    }
    else if(blk_wd > 32)
    {
        copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 32, blk_ht);
        ihevce_copy_2d_neon(pu1_dst + 32, dst_strd, pu1_src + 32, src_strd, blk_wd - 32, blk_ht);
    }
    else if(blk_wd >= 16)
    {
        if(blk_ht % 2 == 0)
        {
            copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 16, blk_ht);
            ihevce_copy_2d_neon(
                pu1_dst + 16, dst_strd, pu1_src + 16, src_strd, blk_wd - 16, blk_ht);
        }
        else
        {
            copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 16, blk_ht - 1);
            memcpy(pu1_dst + (blk_ht - 1) * dst_strd, pu1_src + (blk_ht - 1) * src_strd, blk_wd);
            ihevce_copy_2d_neon(
                pu1_dst + 16, dst_strd, pu1_src + 16, src_strd, blk_wd - 16, blk_ht - 1);
        }
    }
    else if(blk_wd >= 8)
    {
        if(blk_ht % 2 == 0)
        {
            copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 8, blk_ht);
            ihevce_copy_2d_neon(pu1_dst + 8, dst_strd, pu1_src + 8, src_strd, blk_wd - 8, blk_ht);
        }
        else
        {
            copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 8, blk_ht - 1);
            memcpy(pu1_dst + (blk_ht - 1) * dst_strd, pu1_src + (blk_ht - 1) * src_strd, blk_wd);
            ihevce_copy_2d_neon(
                pu1_dst + 8, dst_strd, pu1_src + 8, src_strd, blk_wd - 8, blk_ht - 1);
        }
    }
    else if(blk_wd >= 4)
    {
        if(blk_ht % 2 == 0)
        {
            copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 4, blk_ht);
            ihevce_copy_2d_neon(pu1_dst + 4, dst_strd, pu1_src + 4, src_strd, blk_wd - 4, blk_ht);
        }
        else
        {
            copy_2d_neon(pu1_dst, dst_strd, pu1_src, src_strd, 4, blk_ht - 1);
            memcpy(pu1_dst + (blk_ht - 1) * dst_strd, pu1_src + (blk_ht - 1) * src_strd, blk_wd);
            ihevce_copy_2d_neon(
                pu1_dst + 4, dst_strd, pu1_src + 4, src_strd, blk_wd - 4, blk_ht - 1);
        }
    }
    else
    {
        ihevce_copy_2d(pu1_dst, dst_strd, pu1_src, src_strd, blk_wd, blk_ht);
    }
}
