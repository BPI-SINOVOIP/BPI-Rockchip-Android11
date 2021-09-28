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
*  ihevce_decomp_pre_intra_pass_neon.c
*
* @brief
*  Contains functions to perform input scaling
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
#include <string.h>
#include <assert.h>
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevc_macros.h"
#include "ihevc_platform_macros.h"
#include "itt_video_api.h"
#include "ihevc_defs.h"
#include "ihevc_cmn_utils_neon.h"
#include "ihevce_ipe_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void ihevce_scaling_filter_mxn(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_scrtch,
    WORD32 scrtch_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 ht,
    WORD32 wd)
{
#define FILT_TAP_Q 8
#define N_TAPS 7
    const WORD16 i4_ftaps[N_TAPS] = { -18, 0, 80, 132, 80, 0, -18 };
    WORD32 i, j;
    WORD32 tmp;
    UWORD8 *pu1_src_tmp = pu1_src - 3 * src_strd;
    UWORD8 *pu1_scrtch_tmp = pu1_scrtch;

    /* horizontal filtering */
    for(i = -3; i < ht + 2; i++)
    {
        for(j = 0; j < wd; j += 2)
        {
            tmp = (i4_ftaps[3] * pu1_src_tmp[j] +
                   i4_ftaps[2] * (pu1_src_tmp[j - 1] + pu1_src_tmp[j + 1]) +
                   i4_ftaps[1] * (pu1_src_tmp[j + 2] + pu1_src_tmp[j - 2]) +
                   i4_ftaps[0] * (pu1_src_tmp[j + 3] + pu1_src_tmp[j - 3]) +
                   (1 << (FILT_TAP_Q - 1))) >>
                  FILT_TAP_Q;
            pu1_scrtch_tmp[j >> 1] = CLIP_U8(tmp);
        }
        pu1_scrtch_tmp += scrtch_strd;
        pu1_src_tmp += src_strd;
    }
    /* vertical filtering */
    pu1_scrtch_tmp = pu1_scrtch + 3 * scrtch_strd;
    for(i = 0; i < ht; i += 2)
    {
        for(j = 0; j < (wd >> 1); j++)
        {
            tmp =
                (i4_ftaps[3] * pu1_scrtch_tmp[j] +
                 i4_ftaps[2] * (pu1_scrtch_tmp[j + scrtch_strd] + pu1_scrtch_tmp[j - scrtch_strd]) +
                 i4_ftaps[1] *
                     (pu1_scrtch_tmp[j + 2 * scrtch_strd] + pu1_scrtch_tmp[j - 2 * scrtch_strd]) +
                 i4_ftaps[0] *
                     (pu1_scrtch_tmp[j + 3 * scrtch_strd] + pu1_scrtch_tmp[j - 3 * scrtch_strd]) +
                 (1 << (FILT_TAP_Q - 1))) >>
                FILT_TAP_Q;
            pu1_dst[j] = CLIP_U8(tmp);
        }
        pu1_dst += dst_strd;
        pu1_scrtch_tmp += (scrtch_strd << 1);
    }
}

void ihevce_scale_by_2_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_wkg_mem,
    WORD32 ht_offset,
    WORD32 block_ht,
    WORD32 wd_offset,
    WORD32 block_wd,
    FT_COPY_2D *pf_copy_2d)
{
#define MAX_BLK_SZ (MAX_CTB_SIZE + ((N_TAPS >> 1) << 1))
    UWORD8 au1_cpy[MAX_BLK_SZ * MAX_BLK_SZ];
    UWORD32 cpy_strd = MAX_BLK_SZ;
    UWORD8 *pu1_cpy = au1_cpy + cpy_strd * (N_TAPS >> 1) + (N_TAPS >> 1);

    UWORD8 *pu1_in, *pu1_out;
    WORD32 in_strd, wkg_mem_strd;

    WORD32 row_start, row_end;
    WORD32 col_start, col_end;
    WORD32 i, fun_select;
    WORD32 ht_tmp, wd_tmp;
    FT_SCALING_FILTER_BY_2 *ihevce_scaling_filters[2];

    assert((wd & 1) == 0);
    assert((ht & 1) == 0);
    assert(block_wd <= MAX_CTB_SIZE);
    assert(block_ht <= MAX_CTB_SIZE);

    /* function pointers for filtering different dimensions */
    ihevce_scaling_filters[0] = ihevce_scaling_filter_mxn;
    ihevce_scaling_filters[1] = ihevce_scaling_filter_mxn_neon;

    /* handle boundary blks */
    col_start = (wd_offset < (N_TAPS >> 1)) ? 1 : 0;
    row_start = (ht_offset < (N_TAPS >> 1)) ? 1 : 0;
    col_end = ((wd_offset + block_wd) > (wd - (N_TAPS >> 1))) ? 1 : 0;
    row_end = ((ht_offset + block_ht) > (ht - (N_TAPS >> 1))) ? 1 : 0;
    if(col_end && (wd % block_wd != 0))
    {
        block_wd = (wd % block_wd);
    }
    if(row_end && (ht % block_ht != 0))
    {
        block_ht = (ht % block_ht);
    }

    /* boundary blks needs to be padded, copy src to tmp buffer */
    if(col_start || col_end || row_end || row_start)
    {
        UWORD8 *pu1_src_tmp = pu1_src + wd_offset + ht_offset * src_strd;

        pu1_cpy -= (3 * (1 - col_start) + cpy_strd * 3 * (1 - row_start));
        pu1_src_tmp -= (3 * (1 - col_start) + src_strd * 3 * (1 - row_start));
        ht_tmp = block_ht + 3 * (1 - row_start) + 3 * (1 - row_end);
        wd_tmp = block_wd + 3 * (1 - col_start) + 3 * (1 - col_end);
        pf_copy_2d(pu1_cpy, cpy_strd, pu1_src_tmp, src_strd, wd_tmp, ht_tmp);
        pu1_in = au1_cpy + cpy_strd * 3 + 3;
        in_strd = cpy_strd;
    }
    else
    {
        pu1_in = pu1_src + wd_offset + ht_offset * src_strd;
        in_strd = src_strd;
    }

    /*top padding*/
    if(row_start)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + cpy_strd * 3;

        pu1_cpy = au1_cpy + cpy_strd * (3 - 1);
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy -= cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy -= cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
    }

    /*bottom padding*/
    if(row_end)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + cpy_strd * 3 + (block_ht - 1) * cpy_strd;

        pu1_cpy = pu1_cpy_tmp + cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy += cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
        pu1_cpy += cpy_strd;
        memcpy(pu1_cpy, pu1_cpy_tmp, block_wd + 6);
    }

    /*left padding*/
    if(col_start)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + 3;

        pu1_cpy = au1_cpy;
        for(i = 0; i < block_ht + 6; i++)
        {
            pu1_cpy[0] = pu1_cpy[1] = pu1_cpy[2] = pu1_cpy_tmp[0];
            pu1_cpy += cpy_strd;
            pu1_cpy_tmp += cpy_strd;
        }
    }

    /*right padding*/
    if(col_end)
    {
        UWORD8 *pu1_cpy_tmp = au1_cpy + 3 + block_wd - 1;

        pu1_cpy = au1_cpy + 3 + block_wd;
        for(i = 0; i < block_ht + 6; i++)
        {
            pu1_cpy[0] = pu1_cpy[1] = pu1_cpy[2] = pu1_cpy_tmp[0];
            pu1_cpy += cpy_strd;
            pu1_cpy_tmp += cpy_strd;
        }
    }

    wkg_mem_strd = block_wd >> 1;
    pu1_out = pu1_dst + (wd_offset >> 1);
    fun_select = (block_wd % 16 == 0);
    ihevce_scaling_filters[fun_select](
        pu1_in, in_strd, pu1_wkg_mem, wkg_mem_strd, pu1_out, dst_strd, block_ht, block_wd);

    /* Left padding of 16 for 1st block of every row */
    if(wd_offset == 0)
    {
        UWORD8 u1_val;
        WORD32 pad_wd = 16;
        WORD32 pad_ht = block_ht >> 1;
        UWORD8 *dst = pu1_dst;

        for(i = 0; i < pad_ht; i++)
        {
            u1_val = dst[0];
            memset(&dst[-pad_wd], u1_val, pad_wd);
            dst += dst_strd;
        }
    }

    if(wd == wd_offset + block_wd)
    {
        /* Right padding of (16 + (CEIL16(wd/2))-wd/2) for last block of every row */
        /* Right padding is done only after processing of last block of that row is done*/
        UWORD8 u1_val;
        WORD32 pad_wd = 16 + CEIL16((wd >> 1)) - (wd >> 1) + 4;
        WORD32 pad_ht = block_ht >> 1;
        UWORD8 *dst = pu1_dst + (wd >> 1) - 1;

        for(i = 0; i < pad_ht; i++)
        {
            u1_val = dst[0];
            memset(&dst[1], u1_val, pad_wd);
            dst += dst_strd;
        }

        if(ht_offset == 0)
        {
            /* Top padding of 16 is done for 1st row only after we reach end of that row */
            WORD32 pad_wd = dst_strd;
            WORD32 pad_ht = 16;
            UWORD8 *dst = pu1_dst - 16;

            for(i = 1; i <= pad_ht; i++)
            {
                memcpy(dst - (i * dst_strd), dst, pad_wd);
            }
        }

        /* Bottom padding of (16 + (CEIL16(ht/2)) - ht/2) is done only if we have
         reached end of frame */
        if(ht - ht_offset - block_ht == 0)
        {
            WORD32 pad_wd = dst_strd;
            WORD32 pad_ht = 16 + CEIL16((ht >> 1)) - (ht >> 1) + 4;
            UWORD8 *dst = pu1_dst + (((block_ht >> 1) - 1) * dst_strd) - 16;

            for(i = 1; i <= pad_ht; i++)
                memcpy(dst + (i * dst_strd), dst, pad_wd);
        }
    }
}
