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
******************************************************************************
* @file ihevce_had_satd.c
*
* @brief
*    This file contains functions of Hadamard SAD and SATD
*
* @author
*    Ittiam
*
* List of Functions
*   <TODO: TO BE ADDED>
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
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
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
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static void ihevce_hadamard_4x4_8bit(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    WORD16 m[16];

    /*===== hadamard horz transform =====*/
    for(k = 0; k < 4; k++)
    {
        WORD32 r0, r1, r2, r3;
        WORD32 h0, h1, h2, h3;

        /* Compute the residue block */
        r0 = pu1_src[0] - pu1_pred[0];
        r1 = pu1_src[1] - pu1_pred[1];
        r2 = pu1_src[2] - pu1_pred[2];
        r3 = pu1_src[3] - pu1_pred[3];

        h0 = r0 + r1;
        h1 = r0 - r1;
        h2 = r2 + r3;
        h3 = r2 - r3;

        m[k * 4 + 0] = h0 + h2;
        m[k * 4 + 1] = h1 + h3;
        m[k * 4 + 2] = h0 - h2;
        m[k * 4 + 3] = h1 - h3;

        pu1_pred += pred_strd;
        pu1_src += src_strd;
    }

    /*===== hadamard vert transform =====*/
    for(k = 0; k < 4; k++)
    {
        WORD32 v0, v1, v2, v3;

        v0 = m[0 + k] + m[4 + k];
        v1 = m[0 + k] - m[4 + k];
        v2 = m[8 + k] + m[12 + k];
        v3 = m[8 + k] - m[12 + k];

        pi2_dst[0 * dst_strd + k] = v0 + v2;
        pi2_dst[1 * dst_strd + k] = v1 + v3;
        pi2_dst[2 * dst_strd + k] = v0 - v2;
        pi2_dst[3 * dst_strd + k] = v1 - v3;
    }
}

static void ihevce_hadamard_8x8_8bit(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 i;

    // y0
    ihevce_hadamard_4x4_8bit(pu1_src, src_strd, pu1_pred, pred_strd, pi2_dst, dst_strd);
    // y1
    ihevce_hadamard_4x4_8bit(pu1_src + 4, src_strd, pu1_pred + 4, pred_strd, pi2_dst + 4, dst_strd);
    // y2
    ihevce_hadamard_4x4_8bit(
        pu1_src + 4 * src_strd,
        src_strd,
        pu1_pred + 4 * pred_strd,
        pred_strd,
        pi2_dst + (4 * dst_strd),
        dst_strd);
    // y3
    ihevce_hadamard_4x4_8bit(
        pu1_src + 4 + 4 * src_strd,
        src_strd,
        pu1_pred + 4 + 4 * pred_strd,
        pred_strd,
        pi2_dst + (4 * dst_strd) + 4,
        dst_strd);

    /*   Child HAD results combined as follows to get Parent result */
    /*  _                                                 _         */
    /* |  (y0 + y1) + (y2 + y3)    (y0 - y1) + (y2 - y3)   |        */
    /* |  (y0 + y1) - (y2 + y3)    (y0 - y1) - (y2 - y3)   |        */
    /* \-                                                 -/        */
    for(i = 0; i < 16; i++)
    {
        WORD32 idx = (i >> 2) * dst_strd + (i % 4);
        WORD16 a0 = pi2_dst[idx];
        WORD16 a1 = pi2_dst[4 + idx];
        WORD16 a2 = pi2_dst[(4 * dst_strd) + idx];
        WORD16 a3 = pi2_dst[(4 * dst_strd) + 4 + idx];

        WORD16 b0 = (a0 + a1);
        WORD16 b1 = (a0 - a1);
        WORD16 b2 = (a2 + a3);
        WORD16 b3 = (a2 - a3);

        pi2_dst[idx] = b0 + b2;
        pi2_dst[4 + idx] = b1 + b3;
        pi2_dst[(4 * dst_strd) + idx] = b0 - b2;
        pi2_dst[(4 * dst_strd) + 4 + idx] = b1 - b3;
    }
}

static void ihevce_hadamard_16x16_8bit(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 i;

    // y0
    ihevce_hadamard_8x8_8bit(pu1_src, src_strd, pu1_pred, pred_strd, pi2_dst, dst_strd);
    // y1
    ihevce_hadamard_8x8_8bit(pu1_src + 8, src_strd, pu1_pred + 8, pred_strd, pi2_dst + 8, dst_strd);
    // y2
    ihevce_hadamard_8x8_8bit(
        pu1_src + 8 * src_strd,
        src_strd,
        pu1_pred + 8 * pred_strd,
        pred_strd,
        pi2_dst + (8 * dst_strd),
        dst_strd);
    // y3
    ihevce_hadamard_8x8_8bit(
        pu1_src + 8 + 8 * src_strd,
        src_strd,
        pu1_pred + 8 + 8 * pred_strd,
        pred_strd,
        pi2_dst + (8 * dst_strd) + 8,
        dst_strd);

    /*   Child HAD results combined as follows to get Parent result */
    /*  _                                                 _         */
    /* |  (y0 + y1) + (y2 + y3)    (y0 - y1) + (y2 - y3)   |        */
    /* |  (y0 + y1) - (y2 + y3)    (y0 - y1) - (y2 - y3)   |        */
    /* \-                                                 -/        */
    for(i = 0; i < 64; i++)
    {
        WORD32 idx = (i >> 3) * dst_strd + (i % 8);
        WORD16 a0 = pi2_dst[idx];
        WORD16 a1 = pi2_dst[8 + idx];
        WORD16 a2 = pi2_dst[(8 * dst_strd) + idx];
        WORD16 a3 = pi2_dst[(8 * dst_strd) + 8 + idx];

        WORD16 b0 = (a0 + a1) >> 1;
        WORD16 b1 = (a0 - a1) >> 1;
        WORD16 b2 = (a2 + a3) >> 1;
        WORD16 b3 = (a2 - a3) >> 1;

        pi2_dst[idx] = b0 + b2;
        pi2_dst[8 + idx] = b1 + b3;
        pi2_dst[(8 * dst_strd) + idx] = b0 - b2;
        pi2_dst[(8 * dst_strd) + 8 + idx] = b1 - b3;
    }
}

static void ihevce_hadamard_32x32_8bit(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 i;

    // y0
    ihevce_hadamard_16x16_8bit(pu1_src, src_strd, pu1_pred, pred_strd, pi2_dst, dst_strd);
    // y1
    ihevce_hadamard_16x16_8bit(
        pu1_src + 16, src_strd, pu1_pred + 16, pred_strd, pi2_dst + 16, dst_strd);
    // y2
    ihevce_hadamard_16x16_8bit(
        pu1_src + 16 * src_strd,
        src_strd,
        pu1_pred + 16 * pred_strd,
        pred_strd,
        pi2_dst + (16 * dst_strd),
        dst_strd);
    // y3
    ihevce_hadamard_16x16_8bit(
        pu1_src + 16 + 16 * src_strd,
        src_strd,
        pu1_pred + 16 + 16 * pred_strd,
        pred_strd,
        pi2_dst + (16 * dst_strd) + 16,
        dst_strd);

    /*   Child HAD results combined as follows to get Parent result */
    /*  _                                                 _         */
    /* |  (y0 + y1) + (y2 + y3)    (y0 - y1) + (y2 - y3)   |        */
    /* |  (y0 + y1) - (y2 + y3)    (y0 - y1) - (y2 - y3)   |        */
    /* \-                                                 -/        */
    for(i = 0; i < 256; i++)
    {
        WORD32 idx = (i >> 4) * dst_strd + (i % 16);
        WORD16 a0 = pi2_dst[idx] >> 2;
        WORD16 a1 = pi2_dst[16 + idx] >> 2;
        WORD16 a2 = pi2_dst[(16 * dst_strd) + idx] >> 2;
        WORD16 a3 = pi2_dst[(16 * dst_strd) + 16 + idx] >> 2;

        WORD16 b0 = (a0 + a1);
        WORD16 b1 = (a0 - a1);
        WORD16 b2 = (a2 + a3);
        WORD16 b3 = (a2 - a3);

        pi2_dst[idx] = b0 + b2;
        pi2_dst[16 + idx] = b1 + b3;
        pi2_dst[(16 * dst_strd) + idx] = b0 - b2;
        pi2_dst[(16 * dst_strd) + 16 + idx] = b1 - b3;
    }
}

/**
*******************************************************************************
*
* @brief
*  Compute Hadamard sad for 4x4 block with 8-bit input
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd
*  WORD32 Destination stride
*
* @param[in] size
*  WORD32 transform Block size
*
* @returns hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_HAD_4x4_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    WORD16 v[16];
    UWORD32 u4_sad = 0;

    (void)pi2_dst;
    (void)dst_strd;
    ihevce_hadamard_4x4_8bit(pu1_origin, src_strd, pu1_pred_buf, pred_strd, v, 4);

    for(k = 0; k < 16; ++k)
        u4_sad += abs(v[k]);
    u4_sad = ((u4_sad + 2) >> 2);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Computes Hadamard Sad for 8x8 block with 8-bit input
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd
*  WORD32 Destination stride
*
* @param[in] size
*  WORD32 transform Block size
*
* @returns Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_HAD_8x8_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    UWORD32 u4_sad = 0;
    WORD16 v[64];

    (void)pi2_dst;
    (void)dst_strd;
    ihevce_hadamard_8x8_8bit(pu1_origin, src_strd, pu1_pred_buf, pred_strd, v, 8);

    for(k = 0; k < 64; ++k)
        u4_sad += abs(v[k]);
    u4_sad = ((u4_sad + 4) >> 3);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Compute dc suppressed hadamard sad for 8x8 block with 8-bit input
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd
*  WORD32 Destination stride
*
* @param[in] size
*  WORD32 transform Block size
*
* @returns Hadamard SAD with DC Suppressed
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_compute_ac_had_8x8_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    UWORD32 u4_sad = 0;
    WORD16 v[64];

    (void)pi2_dst;
    (void)dst_strd;
    ihevce_hadamard_8x8_8bit(pu1_origin, src_strd, pu1_pred_buf, pred_strd, v, 8);

    v[0] = 0;
    for(k = 0; k < 64; ++k)
        u4_sad += abs(v[k]);
    u4_sad = ((u4_sad + 4) >> 3);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Computes Hadamard Sad for 16x16 block with 8-bit input
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd
*  WORD32 Destination stride
*
* @param[in] size
*  WORD32 transform Block size
*
* @returns Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_HAD_16x16_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    UWORD32 u4_sad = 0;
    WORD16 v[256];

    (void)pi2_dst;
    (void)dst_strd;
    ihevce_hadamard_16x16_8bit(pu1_origin, src_strd, pu1_pred_buf, pred_strd, v, 16);

    for(k = 0; k < 256; ++k)
        u4_sad += abs(v[k]);
    u4_sad = ((u4_sad + 4) >> 3);

    return u4_sad;
}

/**
*******************************************************************************
*
* @brief
*  Computes Hadamard Sad for 32x32 block with 8-bit input
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred_buf
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[in] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd
*  WORD32 Destination stride
*
* @param[in] size
*  WORD32 transform Block size
*
* @returns Hadamard SAD
*
* @remarks
*  Not updating the transform destination now. Only returning the SATD
*
*******************************************************************************
*/
UWORD32 ihevce_HAD_32x32_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd)
{
    WORD32 k;
    UWORD32 u4_sad = 0;
    WORD16 v[32 * 32];

    (void)pi2_dst;
    (void)dst_strd;
    ihevce_hadamard_32x32_8bit(pu1_origin, src_strd, pu1_pred_buf, pred_strd, v, 32);

    for(k = 0; k < 32 * 32; ++k)
        u4_sad += abs(v[k]);
    u4_sad = ((u4_sad + 2) >> 2);

    return u4_sad;
}

//#if COMPUTE_16x16_R == C
/**
*******************************************************************************
*
* @brief
*   Computes 8x8 transform using children 4x4 hadamard results
*
* @par Description:
*
* @param[in] pi2_4x4_had
*  WORD16 pointer to 4x4 hadamard buffer(y0, y1, y2, y3 hadmard in Zscan order)
*
* @param[in] had4_strd
*  stride of 4x4 hadmard buffer pi2_y0, pi2_y1, pi2_y2, pi2_y3
*
* @param[out] pi2_dst
*  destination buffer where 8x8 hadamard result is stored
*
* @param[in] dst_stride
*  stride of destination block
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
* @returns
*  8x8 Hadamard SATD
* @remarks
*
*******************************************************************************
*/
static UWORD32 ihevce_compute_8x8HAD_using_4x4(
    WORD16 *pi2_4x4_had,
    WORD32 had4_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf)
{
    /* Qstep value is right shifted by 8 */
    WORD32 threshold = (i4_frm_qstep >> 8);

    /* Initialize pointers to 4 subblocks of 4x4 HAD buffer */
    WORD16 *pi2_y0 = pi2_4x4_had;
    WORD16 *pi2_y1 = pi2_4x4_had + 4;
    WORD16 *pi2_y2 = pi2_4x4_had + had4_strd * 4;
    WORD16 *pi2_y3 = pi2_4x4_had + had4_strd * 4 + 4;

    /* Initialize pointers to store 8x8 HAD output */
    WORD16 *pi2_dst0 = pi2_dst;
    WORD16 *pi2_dst1 = pi2_dst + 4;
    WORD16 *pi2_dst2 = pi2_dst + dst_strd * 4;
    WORD16 *pi2_dst3 = pi2_dst + dst_strd * 4 + 4;

    UWORD32 u4_satd = 0;
    WORD32 i;

    /*   Child HAD results combined as follows to get Parent result */
    /*  _                                                 _         */
    /* |  (y0 + y1) + (y2 + y3)    (y0 - y1) + (y2 - y3)   |        */
    /* |  (y0 + y1) - (y2 + y3)    (y0 - y1) - (y2 - y3)   |        */
    /* \-                                                 -/        */
    for(i = 0; i < 16; i++)
    {
        WORD32 src_idx = (i >> 2) * had4_strd + (i % 4);
        WORD32 dst_idx = (i >> 2) * dst_strd + (i % 4);

        WORD16 a0 = pi2_y0[src_idx];
        WORD16 a1 = pi2_y1[src_idx];
        WORD16 a2 = pi2_y2[src_idx];
        WORD16 a3 = pi2_y3[src_idx];

        WORD16 b0 = (a0 + a1);
        WORD16 b1 = (a0 - a1);
        WORD16 b2 = (a2 + a3);
        WORD16 b3 = (a2 - a3);

        pi2_dst0[dst_idx] = b0 + b2;
        pi2_dst1[dst_idx] = b1 + b3;
        pi2_dst2[dst_idx] = b0 - b2;
        pi2_dst3[dst_idx] = b1 - b3;

        if(ABS(pi2_dst0[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst1[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst2[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst3[dst_idx]) > threshold)
            *pi4_cbf = 1;

        u4_satd += ABS(pi2_dst0[dst_idx]);
        u4_satd += ABS(pi2_dst1[dst_idx]);
        u4_satd += ABS(pi2_dst2[dst_idx]);
        u4_satd += ABS(pi2_dst3[dst_idx]);
    }

    /* return the 8x8 satd */
    return (u4_satd);
}

/**
*******************************************************************************
*
* @brief
*    Computes Residue and Hadamard Transform for four 4x4 blocks (Z scan) of
*    a 8x8 block (Residue is computed for 8-bit src and prediction buffers)
*    Modified to incorporate the dead-zone implementation - Lokesh
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[out] pi2_dst
*  WORD16 pointer to the transform block
*
* @param[in] dst_strd
*  WORD32 Destination stride
*
* @param[out] pi4_hsad
*  array for storing hadmard sad of each 4x4 block
*
* @param[in] hsad_stride
*  stride of hadmard sad destination buffer (for Zscan order of storing sads)
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
* @returns
*
* @remarks
*
*******************************************************************************
*/
static WORD32 ihevce_had4_4x4(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst4x4,
    WORD32 dst_strd,
    WORD32 *pi4_hsad,
    WORD32 hsad_stride,
    WORD32 i4_frm_qstep)
{
    WORD32 i, k;
    WORD32 i4_child_total_sad = 0;

    (void)i4_frm_qstep;
    /* -------- Compute four 4x4 HAD Transforms ---------*/
    for(i = 0; i < 4; i++)
    {
        UWORD8 *pu1_pi0, *pu1_pi1;
        WORD16 *pi2_dst;
        WORD32 blkx, blky;
        UWORD32 u4_hsad = 0;
        // TODO: choose deadzone as f(qstep)
        WORD32 threshold = 0;

        /*****************************************************/
        /*    Assuming the looping structure of the four     */
        /*    blocks is in Z scan order of 4x4s in a 8x8     */
        /*    block instead of raster scan                   */
        /*****************************************************/
        blkx = (i & 0x1);
        blky = (i >> 1);

        pu1_pi0 = pu1_src + (blkx * 4) + (blky * 4 * src_strd);
        pu1_pi1 = pu1_pred + (blkx * 4) + (blky * 4 * pred_strd);
        pi2_dst = pi2_dst4x4 + (blkx * 4) + (blky * 4 * dst_strd);

        ihevce_hadamard_4x4_8bit(pu1_pi0, src_strd, pu1_pi1, pred_strd, pi2_dst, dst_strd);

        for(k = 0; k < 4; k++)
        {
            if(ABS(pi2_dst[0 * dst_strd + k]) < threshold)
                pi2_dst[0 * dst_strd + k] = 0;

            if(ABS(pi2_dst[1 * dst_strd + k]) < threshold)
                pi2_dst[1 * dst_strd + k] = 0;

            if(ABS(pi2_dst[2 * dst_strd + k]) < threshold)
                pi2_dst[2 * dst_strd + k] = 0;

            if(ABS(pi2_dst[3 * dst_strd + k]) < threshold)
                pi2_dst[3 * dst_strd + k] = 0;

            /* Accumulate the SATD */
            u4_hsad += ABS(pi2_dst[0 * dst_strd + k]);
            u4_hsad += ABS(pi2_dst[1 * dst_strd + k]);
            u4_hsad += ABS(pi2_dst[2 * dst_strd + k]);
            u4_hsad += ABS(pi2_dst[3 * dst_strd + k]);
        }

        /*===== Normalize the HSAD =====*/
        pi4_hsad[blkx + (blky * hsad_stride)] = ((u4_hsad + 2) >> 2);
        i4_child_total_sad += ((u4_hsad + 2) >> 2);
    }
    return i4_child_total_sad;
}

/**
*******************************************************************************
*
* @brief
*    HSAD is returned for the 4, 4x4 in 8x8
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[out] pi2_dst
*  WORD16 pointer to the transform output block
*
* @param[out] dst_strd
*  WORD32 Destination stride
*
* @param[out] ppi4_hsad
*   pointer to base pointers for storing hadmard sads of various
*   block sizes (4x4 to 32x32)
*
* @param[in] pos_x_y_4x4
*   Denotes packed x,y postion of current 4x4 block w.r.t to start of ctb/CU/MB
*   Lower 16bits denote xpos and upper 16ypos of the 4x4block
*
* @param[in] num_4x4_in_row
*   Denotes the number of current 4x4 blocks in a ctb/CU/MB
*
* @returns
*
* @remarks
*
*******************************************************************************
*/
void ihevce_had_8x8_using_4_4x4(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row)
{
    WORD16 ai2_4x4_had[64];
    WORD32 pos_x = pos_x_y_4x4 & 0xFFFF;
    WORD32 pos_y = (pos_x_y_4x4 >> 16) & 0xFFFF;
    WORD32 *pi4_4x4_hsad;
    WORD32 *pi4_8x8_hsad;

    (void)pi2_dst;
    (void)dst_strd;
    ASSERT(pos_x >= 0);
    ASSERT(pos_y >= 0);

    /* Initialize pointers to  store 4x4 and 8x8 HAD SATDs */
    pi4_4x4_hsad = ppi4_hsad[HAD_4x4] + pos_x + pos_y * num_4x4_in_row;
    pi4_8x8_hsad = ppi4_hsad[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);

    /* -------- Compute four 4x4 HAD Transforms of 8x8 in one call--------- */
    pi4_8x8_hsad[0] = ihevce_had4_4x4(
        pu1_src, src_strd, pu1_pred, pred_strd, ai2_4x4_had, 8, pi4_4x4_hsad, num_4x4_in_row, 0);
}

/**
*******************************************************************************
*
* @brief
*    Reursive Hadamard Transform for 8x8 block. HSAD is returned for the 8x8
*    block and its four subblocks(4x4).
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[out] pi2_dst
*  WORD16 pointer to the transform output block
*
* @param[out] dst_strd
*  WORD32 Destination stride
*
* @param[out] ppi4_hsad
*   pointer to base pointers for storing hadmard sads of various
*   block sizes (4x4 to 32x32)
*
* @param[in] pos_x_y_4x4
*   Denotes packed x,y postion of current 4x4 block w.r.t to start of ctb/CU/MB
*   Lower 16bits denote xpos and upper 16ypos of the 4x4block
*
* @param[in] num_4x4_in_row
*   Denotes the number of current 4x4 blocks in a ctb/CU/MB
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
* @returns
*
* @remarks
*
*******************************************************************************
*/
WORD32 ihevce_had_8x8_using_4_4x4_r(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 **ppi4_tu_split,
    WORD32 **ppi4_tu_early_cbf,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    WORD32 i4_cur_depth,
    WORD32 i4_max_depth,
    WORD32 i4_max_tr_size,
    WORD32 *pi4_tu_split_cost,
    void *pv_func_sel)
{
    WORD16 ai2_4x4_had[64];
    WORD32 pos_x = pos_x_y_4x4 & 0xFFFF;
    WORD32 pos_y = (pos_x_y_4x4 >> 16) & 0xFFFF;
    WORD32 *pi4_4x4_hsad;
    WORD32 *pi4_8x8_hsad;
    WORD32 *pi4_8x8_tu_split;

    WORD32 *pi4_8x8_tu_early_cbf;

    UWORD32 u4_satd;
    WORD32 cost_child = 0, cost_parent = 0;
    WORD32 early_cbf = 0;

    const UWORD8 u1_cur_tr_size = 8;
    /* Stores the best cost for the Current 8x8: Lokesh */
    WORD32 best_cost = 0;

    (void)pv_func_sel;
    ASSERT(pos_x >= 0);
    ASSERT(pos_y >= 0);

    /* Initialize pointers to  store 4x4 and 8x8 HAD SATDs */
    pi4_4x4_hsad = ppi4_hsad[HAD_4x4] + pos_x + pos_y * num_4x4_in_row;
    pi4_8x8_hsad = ppi4_hsad[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);
    pi4_8x8_tu_split = ppi4_tu_split[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);
    pi4_8x8_tu_early_cbf =
        ppi4_tu_early_cbf[HAD_8x8] + (pos_x >> 1) + (pos_y >> 1) * (num_4x4_in_row >> 1);

    /* -------- Compute four 4x4 HAD Transforms of 8x8 in one call--------- */
    cost_child = ihevce_had4_4x4(
        pu1_src, src_strd, pu1_pred, pred_strd, ai2_4x4_had, 8, pi4_4x4_hsad, num_4x4_in_row, 0);

    /* -------- Compute 8x8 HAD Transform using 4x4 results ------------- */
    u4_satd = ihevce_compute_8x8HAD_using_4x4(
        ai2_4x4_had, 8, pi2_dst, dst_strd, i4_frm_qstep, &early_cbf);

    /* store the normalized 8x8 satd */
    cost_parent = ((u4_satd + 4) >> 3);

    /* 4 CBF Flags, extra 1 becoz of the 0.5 bits per bin is assumed */
    cost_child += ((4) * lambda) >> (lambda_q_shift + 1);

    if(i4_cur_depth < i4_max_depth)
    {
        if((cost_child < cost_parent) || (i4_max_tr_size < u1_cur_tr_size))
        {
            //cost_child -= ((4) * lambda) >> (lambda_q_shift + 1);
            *pi4_tu_split_cost += (4 * lambda) >> (lambda_q_shift + 1);
            best_cost = cost_child;
            best_cost <<= 1;
            best_cost++;
            pi4_8x8_tu_split[0] = 1;
            pi4_8x8_hsad[0] = cost_child;
        }
        else
        {
            //cost_parent -= ((1) * lambda) >>  (lambda_q_shift + 1);
            best_cost = cost_parent;
            best_cost <<= 1;
            pi4_8x8_tu_split[0] = 0;
            pi4_8x8_hsad[0] = cost_parent;
        }
    }
    else
    {
        //cost_parent -= ((1) * lambda) >>  (lambda_q_shift + 1);
        best_cost = cost_parent;
        best_cost <<= 1;
        pi4_8x8_tu_split[0] = 0;
        pi4_8x8_hsad[0] = cost_parent;
    }

    pi4_8x8_tu_early_cbf[0] = early_cbf;

    /* best cost has tu_split_flag at LSB(Least significant bit) */
    return ((best_cost << 1) + early_cbf);
}

/**
*******************************************************************************
*
* @brief
*   Computes 16x16 transform using children 8x8 hadamard results
*    Modified to incorporate the dead-zone implementation - Lokesh
*
* @par Description:
*
* @param[in] pi2_8x8_had
*  WORD16 pointer to 8x8 hadamard buffer(y0, y1, y2, y3 hadmard in Zscan order)
*
* @param[in] had8_strd
*  stride of 8x8 hadmard buffer pi2_y0, pi2_y1, pi2_y2, pi2_y3
*
* @param[out] pi2_dst
*  destination buffer where 8x8 hadamard result is stored
*
* @param[in] dst_stride
*  stride of destination block
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
* @returns
*  16x16 Hadamard SATD
* @remarks
*
*******************************************************************************
*/
static UWORD32 ihevce_compute_16x16HAD_using_8x8(
    WORD16 *pi2_8x8_had,
    WORD32 had8_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf)
{
    /* Qstep value is right shifted by 8 */
    WORD32 threshold = (i4_frm_qstep >> 8);

    /* Initialize pointers to 4 subblocks of 8x8 HAD buffer */
    WORD16 *pi2_y0 = pi2_8x8_had;
    WORD16 *pi2_y1 = pi2_8x8_had + 8;
    WORD16 *pi2_y2 = pi2_8x8_had + had8_strd * 8;
    WORD16 *pi2_y3 = pi2_8x8_had + had8_strd * 8 + 8;

    /* Initialize pointers to store 8x8 HAD output */
    WORD16 *pi2_dst0 = pi2_dst;
    WORD16 *pi2_dst1 = pi2_dst + 8;
    WORD16 *pi2_dst2 = pi2_dst + dst_strd * 8;
    WORD16 *pi2_dst3 = pi2_dst + dst_strd * 8 + 8;

    UWORD32 u4_satd = 0;
    WORD32 i;

    /*   Child HAD results combined as follows to get Parent result */
    /*  _                                                 _         */
    /* |  (y0 + y1) + (y2 + y3)    (y0 - y1) + (y2 - y3)   |        */
    /* |  (y0 + y1) - (y2 + y3)    (y0 - y1) - (y2 - y3)   |        */
    /* \-                                                 -/        */
    for(i = 0; i < 64; i++)
    {
        WORD32 src_idx = (i >> 3) * had8_strd + (i % 8);
        WORD32 dst_idx = (i >> 3) * dst_strd + (i % 8);

        WORD16 a0 = pi2_y0[src_idx];
        WORD16 a1 = pi2_y1[src_idx];
        WORD16 a2 = pi2_y2[src_idx];
        WORD16 a3 = pi2_y3[src_idx];

        WORD16 b0 = (a0 + a1) >> 1;
        WORD16 b1 = (a0 - a1) >> 1;
        WORD16 b2 = (a2 + a3) >> 1;
        WORD16 b3 = (a2 - a3) >> 1;

        pi2_dst0[dst_idx] = b0 + b2;
        pi2_dst1[dst_idx] = b1 + b3;
        pi2_dst2[dst_idx] = b0 - b2;
        pi2_dst3[dst_idx] = b1 - b3;

        /* Make the value of dst to zerp, if it falls below the dead-zone */
        if(ABS(pi2_dst0[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst1[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst2[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst3[dst_idx]) > threshold)
            *pi4_cbf = 1;

        u4_satd += ABS(pi2_dst0[dst_idx]);
        u4_satd += ABS(pi2_dst1[dst_idx]);
        u4_satd += ABS(pi2_dst2[dst_idx]);
        u4_satd += ABS(pi2_dst3[dst_idx]);
    }

    /* return 16x16 satd */
    return (u4_satd);
}

/**
*******************************************************************************
*
* @brief
*    Hadamard Transform for 16x16 block with 8x8 and 4x4 SATD updates.
*    Uses recursive 8x8 had output to compute satd for 16x16 and its children
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[out] pi2_dst
*  WORD16 pointer to the transform output block
*
* @param[out] dst_strd
*  WORD32 Destination stride
*
* @param[out] ppi4_hsad
*   pointer to base pointers for storing hadmard sads of various
*   block sizes (4x4 to 32x32)
*
* @param[in] pos_x_y_4x4
*   Denotes packed x,y postion of current 4x4 block w.r.t to start of ctb/CU/MB
*   Lower 16bits denote xpos and upper 16ypos of the 4x4block
*
* @param[in] num_4x4_in_row
*   Denotes the number of current 4x4 blocks in a ctb/CU/MB
*
* @param[in] lambda
*  lambda values is the cost factor calculated based on QP
*
* @param[in] lambda_q_shift
*  lambda_q_shift used to reverse the lambda value back from q8 format
*
* @param[in] depth
*  depth gives the current TU depth with respect to the CU
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
* @returns
*
* @remarks
*
*******************************************************************************
*/

WORD32 ihevce_had_16x16_r(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 **ppi4_tu_split,
    WORD32 **ppi4_tu_early_cbf,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    WORD32 i4_cur_depth,
    WORD32 i4_max_depth,
    WORD32 i4_max_tr_size,
    WORD32 *pi4_tu_split_cost,
    void *pv_func_sel)
{
    WORD16 ai2_8x8_had[256];
    WORD32 *pi4_16x16_hsad;
    WORD32 *pi4_16x16_tu_split;

    WORD32 *pi4_16x16_tu_early_cbf;

    UWORD32 u4_satd = 0;
    WORD32 tu_split_flag = 0;
    WORD32 i4_early_cbf_flag = 0, early_cbf = 0;
    const UWORD8 u1_cur_tr_size = 16;

    /* cost_parent : Stores the cost of the parent HAD transform (16x16) */
    /* cost_child : Stores the cost of the child HAD transform (16x16) */
    WORD32 cost_parent = 0, cost_child = 0;

    /*best_cost returns the best cost at the end of the function */
    /*tu_split denoes whether the TU (16x16)is split or not */
    WORD32 best_cost = 0, best_cost_tu_split;
    WORD32 i;

    WORD16 *pi2_y0;
    UWORD8 *pu1_src0;
    UWORD8 *pu1_pred0;
    WORD32 pos_x_y_4x4_0;

    WORD32 pos_x = pos_x_y_4x4 & 0xFFFF;
    WORD32 pos_y = (pos_x_y_4x4 >> 16) & 0xFFFF;

    ASSERT(pos_x >= 0);
    ASSERT(pos_y >= 0);

    /* Initialize pointers to  store 16x16 SATDs */
    pi4_16x16_hsad = ppi4_hsad[HAD_16x16] + (pos_x >> 2) + (pos_y >> 2) * (num_4x4_in_row >> 2);

    pi4_16x16_tu_split =
        ppi4_tu_split[HAD_16x16] + (pos_x >> 2) + (pos_y >> 2) * (num_4x4_in_row >> 2);

    pi4_16x16_tu_early_cbf =
        ppi4_tu_early_cbf[HAD_16x16] + (pos_x >> 2) + (pos_y >> 2) * (num_4x4_in_row >> 2);

    /* -------- Compute four 8x8 HAD Transforms of 16x16 call--------- */
    for(i = 0; i < 4; i++)
    {
        pu1_src0 = pu1_src + (i & 0x01) * 8 + (i >> 1) * src_strd * 8;
        pu1_pred0 = pu1_pred + (i & 0x01) * 8 + (i >> 1) * pred_strd * 8;
        pi2_y0 = ai2_8x8_had + (i & 0x01) * 8 + (i >> 1) * 16 * 8;
        pos_x_y_4x4_0 = pos_x_y_4x4 + (i & 0x01) * 2 + (i >> 1) * (2 << 16);

        best_cost_tu_split = ihevce_had_8x8_using_4_4x4_r(
            pu1_src0,
            src_strd,
            pu1_pred0,
            pred_strd,
            pi2_y0,
            16,
            ppi4_hsad,
            ppi4_tu_split,
            ppi4_tu_early_cbf,
            pos_x_y_4x4_0,
            num_4x4_in_row,
            lambda,
            lambda_q_shift,
            i4_frm_qstep,
            i4_cur_depth + 1,
            i4_max_depth,
            i4_max_tr_size,
            pi4_tu_split_cost,
            pv_func_sel);

        /* Cost is shifted by two bits for Tu_split_flag and early cbf flag */
        best_cost = (best_cost_tu_split >> 2);

        /* Last but one bit stores the information regarding the TU_Split */
        tu_split_flag += (best_cost_tu_split & 0x3) >> 1;

        /* Last bit stores the information regarding the early_cbf */
        i4_early_cbf_flag += (best_cost_tu_split & 0x1);

        cost_child += best_cost;

        tu_split_flag <<= 1;
        i4_early_cbf_flag <<= 1;
    }

    /* -------- Compute 16x16 HAD Transform using 8x8 results ------------- */
    pi2_y0 = ai2_8x8_had;

    /* Threshold currently passed as "0" */
    u4_satd =
        ihevce_compute_16x16HAD_using_8x8(pi2_y0, 16, pi2_dst, dst_strd, i4_frm_qstep, &early_cbf);

    /* store the normalized satd */
    cost_parent = ((u4_satd + 4) >> 3);

    /* 4 TU_Split flags , 4 CBF Flags, extra 1 becoz of the 0.5 bits per bin is assumed */
    cost_child += ((4 + 4) * lambda) >> (lambda_q_shift + 1);

    i4_early_cbf_flag += early_cbf;

    /* Right now the depth is hard-coded to 4: The depth can be modified from the config file
    which decides the extent to which TU_REC needs to be done */
    if(i4_cur_depth < i4_max_depth)
    {
        if((cost_child < cost_parent) || (i4_max_tr_size < u1_cur_tr_size))
        {
            //cost_child -= ((4 + 4)  * lambda) >> (lambda_q_shift + 1);
            *pi4_tu_split_cost += ((4 + 4) * lambda) >> (lambda_q_shift + 1);
            tu_split_flag += 1;
            best_cost = cost_child;
        }
        else
        {
            //cost_parent -= ((1 + 1) * lambda) >>  (lambda_q_shift + 1);
            tu_split_flag += 0;
            best_cost = cost_parent;
        }
    }
    else
    {
        //cost_parent -= ((1 + 1) * lambda) >>  (lambda_q_shift + 1);
        tu_split_flag += 0;
        best_cost = cost_parent;
    }

    pi4_16x16_hsad[0] = best_cost;
    pi4_16x16_tu_split[0] = tu_split_flag;
    pi4_16x16_tu_early_cbf[0] = i4_early_cbf_flag;

    /*returning two values(best cost & tu_split_flag) as a single value*/
    return ((best_cost << 10) + (tu_split_flag << 5) + i4_early_cbf_flag);
}

//#endif
/**
*******************************************************************************
*
* @brief
*   Computes 32x32 transform using children 16x16 hadamard results
*
* @par Description:
*
* @param[in] pi2_16x16_had
*  WORD16 pointer to 16x16 hadamard buffer(y0, y1, y2, y3 hadmard in Zscan order)
*
* @param[in] had16_strd
*  stride of 16x16 hadmard buffer pi2_y0, pi2_y1, pi2_y2, pi2_y3
*
* @param[out] pi2_dst
*  destination buffer where 16x16 hadamard result is stored
*
* @param[in] dst_stride
*  stride of destination block
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
* @returns
*  32x32 Hadamard SATD
* @remarks
*
*******************************************************************************
*/
//#if COMPUTE_32x32_USING_16X16 == C
UWORD32 ihevce_compute_32x32HAD_using_16x16(
    WORD16 *pi2_16x16_had,
    WORD32 had16_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf)
{
    /* Qstep value is right shifted by 8 */
    WORD32 threshold = (i4_frm_qstep >> 8);

    /* Initialize pointers to 4 subblocks of 8x8 HAD buffer */
    WORD16 *pi2_y0 = pi2_16x16_had;
    WORD16 *pi2_y1 = pi2_16x16_had + 16;
    WORD16 *pi2_y2 = pi2_16x16_had + had16_strd * 16;
    WORD16 *pi2_y3 = pi2_16x16_had + had16_strd * 16 + 16;

    /* Initialize pointers to store 8x8 HAD output */
    WORD16 *pi2_dst0 = pi2_dst;
    WORD16 *pi2_dst1 = pi2_dst + 16;
    WORD16 *pi2_dst2 = pi2_dst + dst_strd * 16;
    WORD16 *pi2_dst3 = pi2_dst + dst_strd * 16 + 16;

    UWORD32 u4_satd = 0;
    WORD32 i;

    /*   Child HAD results combined as follows to get Parent result */
    /*  _                                                 _         */
    /* |  (y0 + y1) + (y2 + y3)    (y0 - y1) + (y2 - y3)   |        */
    /* |  (y0 + y1) - (y2 + y3)    (y0 - y1) - (y2 - y3)   |        */
    /* \-                                                 -/        */
    for(i = 0; i < 256; i++)
    {
        WORD32 src_idx = (i >> 4) * had16_strd + (i % 16);
        WORD32 dst_idx = (i >> 4) * dst_strd + (i % 16);

        WORD16 a0 = pi2_y0[src_idx] >> 2;
        WORD16 a1 = pi2_y1[src_idx] >> 2;
        WORD16 a2 = pi2_y2[src_idx] >> 2;
        WORD16 a3 = pi2_y3[src_idx] >> 2;

        WORD16 b0 = (a0 + a1);
        WORD16 b1 = (a0 - a1);
        WORD16 b2 = (a2 + a3);
        WORD16 b3 = (a2 - a3);

        pi2_dst0[dst_idx] = b0 + b2;
        pi2_dst1[dst_idx] = b1 + b3;
        pi2_dst2[dst_idx] = b0 - b2;
        pi2_dst3[dst_idx] = b1 - b3;

        /* Make the value of dst to zerp, if it falls below the dead-zone */
        if(ABS(pi2_dst0[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst1[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst2[dst_idx]) > threshold)
            *pi4_cbf = 1;
        if(ABS(pi2_dst3[dst_idx]) > threshold)
            *pi4_cbf = 1;

        u4_satd += ABS(pi2_dst0[dst_idx]);
        u4_satd += ABS(pi2_dst1[dst_idx]);
        u4_satd += ABS(pi2_dst2[dst_idx]);
        u4_satd += ABS(pi2_dst3[dst_idx]);
    }

    /* return 32x32 satd */
    return (u4_satd);
}
//#endif

/**
*******************************************************************************
*
* @brief
*    Hadamard Transform for 32x32 block with 16x6, 8x8 and 4x4 SATD updates.
*    Uses recursive 16x16 had output to compute satd for 32x32 and its children
*
* @par Description:
*
* @param[in] pu1_origin
*  UWORD8 pointer to the current block
*
* @param[in] src_strd
*  WORD32 Source stride
*
* @param[in] pu1_pred
*  UWORD8 pointer to the prediction block
*
* @param[in] pred_strd
*  WORD32 Pred stride
*
* @param[out] pi2_dst
*  WORD16 pointer to the transform output block
*
* @param[out] dst_strd
*  WORD32 Destination stride
*
* @param[out] ppi4_hsad
*   pointer to base pointers for storing hadmard sads of various
*   block sizes (4x4 to 32x32)
*
* @param[in] pos_x_y_4x4
*   Denotes packed x,y postion of current 4x4 block w.r.t to start of ctb/CU/MB
*   Lower 16bits denote xpos and upper 16ypos of the 4x4block
*
* @param[in] num_4x4_in_row
*   Denotes the number of current 4x4 blocks in a ctb/CU/MB
*
* @param[in] lambda
*  lambda values is the cost factor calculated based on QP
*
* @param[in] lambda_q_shift
*  lambda_q_shift used to reverse the lambda value back from q8 format
*
* @param[in] depth
*  depth gives the current TU depth with respect to the CU
*
* @param[in] i4_frm_qstep
*  frm_qstep value based on the which the threshold value is calculated
*
*
* @returns
*
* @remarks
*
*******************************************************************************
*/
void ihevce_had_32x32_r(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 **ppi4_tu_split,
    WORD32 **ppi4_tu_early_cbf,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    WORD32 i4_cur_depth,
    WORD32 i4_max_depth,
    WORD32 i4_max_tr_size,
    WORD32 *pi4_tu_split_cost,
    me_func_selector_t *ps_func_selector)

{
    WORD16 ai2_16x16_had[1024];
    WORD32 *pi4_32x32_hsad;
    WORD32 *pi4_32x32_tu_split;
    WORD32 *pi4_32x32_tu_early_cbf;

    WORD32 pos_x = pos_x_y_4x4 & 0xFFFF;
    WORD32 pos_y = (pos_x_y_4x4 >> 16) & 0xFFFF;
    WORD32 tu_split_flag = 0;
    const UWORD8 u1_cur_tr_size = 32;
    WORD32 i4_early_cbf_flag = 0, early_cbf = 0;

    /* cost_parent : Stores the cost of the parent HAD transform (16x16) */
    /* cost_child : Stores the cost of the child HAD transform (16x16) */
    WORD32 cost_child = 0, cost_parent = 0;

    /*retuned as the best cost for the entire TU (32x32) */
    WORD32 best_cost = 0;
    /*captures the best cost and tu_split at child level */
    WORD32 best_cost_tu_split;

    /* Initialize pointers to 4 8x8 blocks in 16x16 */
    WORD16 *pi2_y0 = ai2_16x16_had;
    WORD16 *pi2_y1 = ai2_16x16_had + 16;
    WORD16 *pi2_y2 = ai2_16x16_had + 32 * 16;
    WORD16 *pi2_y3 = ai2_16x16_had + 32 * 16 + 16;

    UWORD8 *pu1_src0 = pu1_src;
    UWORD8 *pu1_src1 = pu1_src + 16;
    UWORD8 *pu1_src2 = pu1_src + src_strd * 16;
    UWORD8 *pu1_src3 = pu1_src + src_strd * 16 + 16;

    UWORD8 *pu1_pred0 = pu1_pred;
    UWORD8 *pu1_pred1 = pu1_pred + 16;
    UWORD8 *pu1_pred2 = pu1_pred + pred_strd * 16;
    UWORD8 *pu1_pred3 = pu1_pred + pred_strd * 16 + 16;

    ASSERT(pos_x >= 0);
    ASSERT(pos_y >= 0);

    /* Initialize pointers to store 32x32 SATDs */
    pi4_32x32_hsad = ppi4_hsad[HAD_32x32] + (pos_x >> 3) + (pos_y >> 3) * (num_4x4_in_row >> 3);

    pi4_32x32_tu_split =
        ppi4_tu_split[HAD_32x32] + (pos_x >> 3) + (pos_y >> 3) * (num_4x4_in_row >> 3);

    pi4_32x32_tu_early_cbf =
        ppi4_tu_early_cbf[HAD_32x32] + (pos_x >> 3) + (pos_y >> 3) * (num_4x4_in_row >> 3);

    /* -------- Compute four 8x8 HAD Transforms of 16x16 call--------- */
    best_cost_tu_split = ps_func_selector->pf_had_16x16_r(
        pu1_src0,
        src_strd,
        pu1_pred0,
        pred_strd,
        pi2_y0,
        32,
        ppi4_hsad,
        ppi4_tu_split,
        ppi4_tu_early_cbf,
        pos_x_y_4x4,
        num_4x4_in_row,
        lambda,
        lambda_q_shift,
        i4_frm_qstep,
        i4_cur_depth + 1,
        i4_max_depth,
        i4_max_tr_size,
        pi4_tu_split_cost,
        NULL);

    /* cost is shifted by 10bits */
    best_cost = best_cost_tu_split >> 10;

    /* Tu split is present in the 6-10 bits */
    tu_split_flag += (best_cost_tu_split & 0x3E0) >> 5;

    /*Early CBF info is present in the last 5 bits */
    i4_early_cbf_flag += best_cost_tu_split & 0x1F;

    tu_split_flag <<= 5;
    i4_early_cbf_flag <<= 5;

    cost_child += best_cost;

    best_cost_tu_split = ps_func_selector->pf_had_16x16_r(
        pu1_src1,
        src_strd,
        pu1_pred1,
        pred_strd,
        pi2_y1,
        32,
        ppi4_hsad,
        ppi4_tu_split,
        ppi4_tu_early_cbf,
        pos_x_y_4x4 + 4,
        num_4x4_in_row,
        lambda,
        lambda_q_shift,
        i4_frm_qstep,
        i4_cur_depth + 1,
        i4_max_depth,
        i4_max_tr_size,
        pi4_tu_split_cost,
        NULL);

    /* cost is shifted by 10bits */
    best_cost = best_cost_tu_split >> 10;

    /* Tu split is present in the 6-10 bits */
    tu_split_flag += (best_cost_tu_split & 0x3E0) >> 5;

    /*Early CBF info is present in the last 5 bits */
    i4_early_cbf_flag += best_cost_tu_split & 0x1F;

    tu_split_flag <<= 5;
    i4_early_cbf_flag <<= 5;

    cost_child += best_cost;

    best_cost_tu_split = ps_func_selector->pf_had_16x16_r(
        pu1_src2,
        src_strd,
        pu1_pred2,
        pred_strd,
        pi2_y2,
        32,
        ppi4_hsad,
        ppi4_tu_split,
        ppi4_tu_early_cbf,
        pos_x_y_4x4 + (4 << 16),
        num_4x4_in_row,
        lambda,
        lambda_q_shift,
        i4_frm_qstep,
        i4_cur_depth + 1,
        i4_max_depth,
        i4_max_tr_size,
        pi4_tu_split_cost,
        NULL);

    /* cost is shifted by 10bits */
    best_cost = best_cost_tu_split >> 10;

    /* Tu split is present in the 6-10 bits */
    tu_split_flag += (best_cost_tu_split & 0x3E0) >> 5;

    /*Early CBF info is present in the last 5 bits */
    i4_early_cbf_flag += best_cost_tu_split & 0x1F;

    tu_split_flag <<= 5;
    i4_early_cbf_flag <<= 5;

    cost_child += best_cost;

    best_cost_tu_split = ps_func_selector->pf_had_16x16_r(
        pu1_src3,
        src_strd,
        pu1_pred3,
        pred_strd,
        pi2_y3,
        32,
        ppi4_hsad,
        ppi4_tu_split,
        ppi4_tu_early_cbf,
        pos_x_y_4x4 + (4 << 16) + 4,
        num_4x4_in_row,
        lambda,
        lambda_q_shift,
        i4_frm_qstep,
        i4_cur_depth + 1,
        i4_max_depth,
        i4_max_tr_size,
        pi4_tu_split_cost,
        NULL);

    /* cost is shifted by 10bits */
    best_cost = best_cost_tu_split >> 10;

    /* Tu split is present in the 6-10 bits */
    tu_split_flag += (best_cost_tu_split & 0x3E0) >> 5;

    /*Early CBF info is present in the last 5 bits */
    i4_early_cbf_flag += best_cost_tu_split & 0x1F;

    tu_split_flag <<= 1;
    i4_early_cbf_flag <<= 1;

    cost_child += best_cost;

    {
        UWORD32 u4_satd = 0;

        u4_satd = ps_func_selector->pf_compute_32x32HAD_using_16x16(
            pi2_y0, 32, pi2_dst, dst_strd, i4_frm_qstep, &early_cbf);

        cost_parent = ((u4_satd + 2) >> 2);
    }

    /* 4 TU_Split flags , 4 CBF Flags*/
    cost_child += ((4 + 4) * lambda) >> (lambda_q_shift + 1);

    i4_early_cbf_flag += early_cbf;

    /* 1 TU_SPlit flag, 1 CBF flag */
    //cost_parent += ((1 + 1)* lambda) >>  (lambda_q_shift + 1);

    if(i4_cur_depth < i4_max_depth)
    {
        if((cost_child < cost_parent) || (u1_cur_tr_size > i4_max_tr_size))
        {
            *pi4_tu_split_cost += ((4 + 4) * lambda) >> (lambda_q_shift + 1);
            best_cost = cost_child;
            tu_split_flag++;
        }
        else
        {
            tu_split_flag = 0;
            best_cost = cost_parent;
        }
    }
    else
    {
        tu_split_flag = 0;
        best_cost = cost_parent;
    }

    pi4_32x32_tu_split[0] = tu_split_flag;

    pi4_32x32_hsad[0] = best_cost;

    pi4_32x32_tu_early_cbf[0] = i4_early_cbf_flag;
}
