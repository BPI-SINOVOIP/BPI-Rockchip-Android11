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
/*!
******************************************************************************
* \file ihevce_chroma_had_satd.c
*
* \brief
*    This file contains function definitions of chroma HAD SATD functions
*
* \date
*    15/07/2013
*
* \author
*    Ittiam
*
* List of Functions
*  ihevce_chroma_HAD_4x4_8b()
*  ihevce_chroma_compute_AC_HAD_4x4_8bit()
*  ihevce_hbd_chroma_HAD_4x4()
*  ihevce_hbd_chroma_compute_AC_HAD_4x4()
*  ihevce_chroma_HAD_8x8_8bit()
*  ihevce_hbd_chroma_HAD_8x8()
*  ihevce_chroma_HAD_16x16_8bit()
*  ihevce_hbd_chroma_HAD_16x16()
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevc_debug.h"
#include "itt_video_api.h"

#include "ihevce_api.h"
#include "ihevce_defs.h"
#include "ihevce_had_satd.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief
*  Chroma Hadamard Transform of 4x4 block (8-bit input)
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the source block (u or v, interleaved)
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block (u or v, interleaved)
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd (u or v, interleaved)
*  WORD32 Destination stride
*
* @returns
*  Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_chroma_HAD_4x4_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    WORD16 diff[16], m[16], d[16];
    UWORD32 u4_sad = 0;

    (void)pi2_dst;
    (void)dst_strd;
    for(k = 0; k < 16; k += 4)
    {
        /* u or v, interleaved */
        diff[k + 0] = pu1_origin[2 * 0] - pu1_pred_buf[2 * 0];
        diff[k + 1] = pu1_origin[2 * 1] - pu1_pred_buf[2 * 1];
        diff[k + 2] = pu1_origin[2 * 2] - pu1_pred_buf[2 * 2];
        diff[k + 3] = pu1_origin[2 * 3] - pu1_pred_buf[2 * 3];

        pu1_pred_buf += pred_strd;
        pu1_origin += src_strd;
    }

    /*===== hadamard transform =====*/
    m[0] = diff[0] + diff[12];
    m[1] = diff[1] + diff[13];
    m[2] = diff[2] + diff[14];
    m[3] = diff[3] + diff[15];
    m[4] = diff[4] + diff[8];
    m[5] = diff[5] + diff[9];
    m[6] = diff[6] + diff[10];
    m[7] = diff[7] + diff[11];
    m[8] = diff[4] - diff[8];
    m[9] = diff[5] - diff[9];
    m[10] = diff[6] - diff[10];
    m[11] = diff[7] - diff[11];
    m[12] = diff[0] - diff[12];
    m[13] = diff[1] - diff[13];
    m[14] = diff[2] - diff[14];
    m[15] = diff[3] - diff[15];

    d[0] = m[0] + m[4];
    d[1] = m[1] + m[5];
    d[2] = m[2] + m[6];
    d[3] = m[3] + m[7];
    d[4] = m[8] + m[12];
    d[5] = m[9] + m[13];
    d[6] = m[10] + m[14];
    d[7] = m[11] + m[15];
    d[8] = m[0] - m[4];
    d[9] = m[1] - m[5];
    d[10] = m[2] - m[6];
    d[11] = m[3] - m[7];
    d[12] = m[12] - m[8];
    d[13] = m[13] - m[9];
    d[14] = m[14] - m[10];
    d[15] = m[15] - m[11];

    m[0] = d[0] + d[3];
    m[1] = d[1] + d[2];
    m[2] = d[1] - d[2];
    m[3] = d[0] - d[3];
    m[4] = d[4] + d[7];
    m[5] = d[5] + d[6];
    m[6] = d[5] - d[6];
    m[7] = d[4] - d[7];
    m[8] = d[8] + d[11];
    m[9] = d[9] + d[10];
    m[10] = d[9] - d[10];
    m[11] = d[8] - d[11];
    m[12] = d[12] + d[15];
    m[13] = d[13] + d[14];
    m[14] = d[13] - d[14];
    m[15] = d[12] - d[15];

    d[0] = m[0] + m[1];
    d[1] = m[0] - m[1];
    d[2] = m[2] + m[3];
    d[3] = m[3] - m[2];
    d[4] = m[4] + m[5];
    d[5] = m[4] - m[5];
    d[6] = m[6] + m[7];
    d[7] = m[7] - m[6];
    d[8] = m[8] + m[9];
    d[9] = m[8] - m[9];
    d[10] = m[10] + m[11];
    d[11] = m[11] - m[10];
    d[12] = m[12] + m[13];
    d[13] = m[12] - m[13];
    d[14] = m[14] + m[15];
    d[15] = m[15] - m[14];

    /*===== sad =====*/
    for(k = 0; k < 16; ++k)
    {
        u4_sad += (d[k] > 0 ? d[k] : -d[k]);
    }
    u4_sad = ((u4_sad + 2) >> 2);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Chroma Hadamard Transform of 4x4 block (8-bit input) with DC suppressed
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the source block (u or v, interleaved)
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block (u or v, interleaved)
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd (u or v, interleaved)
*  WORD32 Destination stride
*
* @returns
*  Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_chroma_compute_AC_HAD_4x4_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    WORD16 diff[16], m[16], d[16];
    UWORD32 u4_sad = 0;

    (void)pi2_dst;
    (void)dst_strd;
    for(k = 0; k < 16; k += 4)
    {
        /* u or v, interleaved */
        diff[k + 0] = pu1_origin[2 * 0] - pu1_pred_buf[2 * 0];
        diff[k + 1] = pu1_origin[2 * 1] - pu1_pred_buf[2 * 1];
        diff[k + 2] = pu1_origin[2 * 2] - pu1_pred_buf[2 * 2];
        diff[k + 3] = pu1_origin[2 * 3] - pu1_pred_buf[2 * 3];

        pu1_pred_buf += pred_strd;
        pu1_origin += src_strd;
    }

    /*===== hadamard transform =====*/
    m[0] = diff[0] + diff[12];
    m[1] = diff[1] + diff[13];
    m[2] = diff[2] + diff[14];
    m[3] = diff[3] + diff[15];
    m[4] = diff[4] + diff[8];
    m[5] = diff[5] + diff[9];
    m[6] = diff[6] + diff[10];
    m[7] = diff[7] + diff[11];
    m[8] = diff[4] - diff[8];
    m[9] = diff[5] - diff[9];
    m[10] = diff[6] - diff[10];
    m[11] = diff[7] - diff[11];
    m[12] = diff[0] - diff[12];
    m[13] = diff[1] - diff[13];
    m[14] = diff[2] - diff[14];
    m[15] = diff[3] - diff[15];

    d[0] = m[0] + m[4];
    d[1] = m[1] + m[5];
    d[2] = m[2] + m[6];
    d[3] = m[3] + m[7];
    d[4] = m[8] + m[12];
    d[5] = m[9] + m[13];
    d[6] = m[10] + m[14];
    d[7] = m[11] + m[15];
    d[8] = m[0] - m[4];
    d[9] = m[1] - m[5];
    d[10] = m[2] - m[6];
    d[11] = m[3] - m[7];
    d[12] = m[12] - m[8];
    d[13] = m[13] - m[9];
    d[14] = m[14] - m[10];
    d[15] = m[15] - m[11];

    m[0] = d[0] + d[3];
    m[1] = d[1] + d[2];
    m[2] = d[1] - d[2];
    m[3] = d[0] - d[3];
    m[4] = d[4] + d[7];
    m[5] = d[5] + d[6];
    m[6] = d[5] - d[6];
    m[7] = d[4] - d[7];
    m[8] = d[8] + d[11];
    m[9] = d[9] + d[10];
    m[10] = d[9] - d[10];
    m[11] = d[8] - d[11];
    m[12] = d[12] + d[15];
    m[13] = d[13] + d[14];
    m[14] = d[13] - d[14];
    m[15] = d[12] - d[15];

    d[0] = m[0] + m[1];
    d[1] = m[0] - m[1];
    d[2] = m[2] + m[3];
    d[3] = m[3] - m[2];
    d[4] = m[4] + m[5];
    d[5] = m[4] - m[5];
    d[6] = m[6] + m[7];
    d[7] = m[7] - m[6];
    d[8] = m[8] + m[9];
    d[9] = m[8] - m[9];
    d[10] = m[10] + m[11];
    d[11] = m[11] - m[10];
    d[12] = m[12] + m[13];
    d[13] = m[12] - m[13];
    d[14] = m[14] + m[15];
    d[15] = m[15] - m[14];

    /* DC masking */
    d[0] = 0;

    /*===== sad =====*/
    for(k = 0; k < 16; ++k)
    {
        u4_sad += (d[k] > 0 ? d[k] : -d[k]);
    }
    u4_sad = ((u4_sad + 2) >> 2);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Chroma Hadamard Transform of 8x8 block (8-bit input)
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the source block (u or v, interleaved)
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block (u or v, interleaved)
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd (u or v, interleaved)
*  WORD32 Destination stride
*
* @returns
*  Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_chroma_HAD_8x8_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k, i, j, jj;
    UWORD32 u4_sad = 0;
    WORD16 diff[64], m1[8][8], m2[8][8], m3[8][8];

    (void)pi2_dst;
    (void)dst_strd;
    for(k = 0; k < 64; k += 8)
    {
        /* u or v, interleaved */
        diff[k + 0] = pu1_origin[2 * 0] - pu1_pred_buf[2 * 0];
        diff[k + 1] = pu1_origin[2 * 1] - pu1_pred_buf[2 * 1];
        diff[k + 2] = pu1_origin[2 * 2] - pu1_pred_buf[2 * 2];
        diff[k + 3] = pu1_origin[2 * 3] - pu1_pred_buf[2 * 3];
        diff[k + 4] = pu1_origin[2 * 4] - pu1_pred_buf[2 * 4];
        diff[k + 5] = pu1_origin[2 * 5] - pu1_pred_buf[2 * 5];
        diff[k + 6] = pu1_origin[2 * 6] - pu1_pred_buf[2 * 6];
        diff[k + 7] = pu1_origin[2 * 7] - pu1_pred_buf[2 * 7];

        pu1_pred_buf += pred_strd;
        pu1_origin += src_strd;
    }

    /*===== hadamard transform =====*/
    // horizontal
    for(j = 0; j < 8; j++)
    {
        jj = j << 3;
        m2[j][0] = diff[jj] + diff[jj + 4];
        m2[j][1] = diff[jj + 1] + diff[jj + 5];
        m2[j][2] = diff[jj + 2] + diff[jj + 6];
        m2[j][3] = diff[jj + 3] + diff[jj + 7];
        m2[j][4] = diff[jj] - diff[jj + 4];
        m2[j][5] = diff[jj + 1] - diff[jj + 5];
        m2[j][6] = diff[jj + 2] - diff[jj + 6];
        m2[j][7] = diff[jj + 3] - diff[jj + 7];

        m1[j][0] = m2[j][0] + m2[j][2];
        m1[j][1] = m2[j][1] + m2[j][3];
        m1[j][2] = m2[j][0] - m2[j][2];
        m1[j][3] = m2[j][1] - m2[j][3];
        m1[j][4] = m2[j][4] + m2[j][6];
        m1[j][5] = m2[j][5] + m2[j][7];
        m1[j][6] = m2[j][4] - m2[j][6];
        m1[j][7] = m2[j][5] - m2[j][7];

        m2[j][0] = m1[j][0] + m1[j][1];
        m2[j][1] = m1[j][0] - m1[j][1];
        m2[j][2] = m1[j][2] + m1[j][3];
        m2[j][3] = m1[j][2] - m1[j][3];
        m2[j][4] = m1[j][4] + m1[j][5];
        m2[j][5] = m1[j][4] - m1[j][5];
        m2[j][6] = m1[j][6] + m1[j][7];
        m2[j][7] = m1[j][6] - m1[j][7];
    }

    // vertical
    for(i = 0; i < 8; i++)
    {
        m3[0][i] = m2[0][i] + m2[4][i];
        m3[1][i] = m2[1][i] + m2[5][i];
        m3[2][i] = m2[2][i] + m2[6][i];
        m3[3][i] = m2[3][i] + m2[7][i];
        m3[4][i] = m2[0][i] - m2[4][i];
        m3[5][i] = m2[1][i] - m2[5][i];
        m3[6][i] = m2[2][i] - m2[6][i];
        m3[7][i] = m2[3][i] - m2[7][i];

        m1[0][i] = m3[0][i] + m3[2][i];
        m1[1][i] = m3[1][i] + m3[3][i];
        m1[2][i] = m3[0][i] - m3[2][i];
        m1[3][i] = m3[1][i] - m3[3][i];
        m1[4][i] = m3[4][i] + m3[6][i];
        m1[5][i] = m3[5][i] + m3[7][i];
        m1[6][i] = m3[4][i] - m3[6][i];
        m1[7][i] = m3[5][i] - m3[7][i];

        m2[0][i] = m1[0][i] + m1[1][i];
        m2[1][i] = m1[0][i] - m1[1][i];
        m2[2][i] = m1[2][i] + m1[3][i];
        m2[3][i] = m1[2][i] - m1[3][i];
        m2[4][i] = m1[4][i] + m1[5][i];
        m2[5][i] = m1[4][i] - m1[5][i];
        m2[6][i] = m1[6][i] + m1[7][i];
        m2[7][i] = m1[6][i] - m1[7][i];
    }

    /*===== sad =====*/
    for(i = 0; i < 8; i++)
    {
        for(j = 0; j < 8; j++)
        {
            u4_sad += (m2[i][j] > 0 ? m2[i][j] : -m2[i][j]);
        }
    }
    u4_sad = ((u4_sad + 4) >> 3);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Chroma Hadamard Transform of 16x16 block (8-bit input)
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the source block (u or v, interleaved)
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block (u or v, interleaved)
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd (u or v, interleaved)
*  WORD32 Destination stride
*
* @returns
*  Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_chroma_HAD_16x16_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    UWORD32 au4_sad[4], u4_result = 0;
    WORD32 i;

    for(i = 0; i < 4; i++)
    {
        au4_sad[i] = ihevce_chroma_HAD_8x8_8bit(
            pu1_origin, src_strd, pu1_pred_buf, pred_strd, pi2_dst, dst_strd);

        if(i == 0 || i == 2)
        {
            pu1_origin += 16;
            pu1_pred_buf += 16;
        }

        if(i == 1)
        {
            pu1_origin += (8 * src_strd) - 16;
            pu1_pred_buf += (8 * pred_strd) - 16;
        }

        u4_result += au4_sad[i];
    }

    return u4_result;
}
