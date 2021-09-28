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
* \file ihevce_inter_pred.h
*
* \brief
*    This file contains function prototypes of luma and chroma MC function
*    interfaces for a inter PU
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_INTER_PRED_H_
#define _IHEVCE_INTER_PRED_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/* enum to get availability of nbr by ANDing with nbr_mask */
typedef enum TILE_NBR_MASK_E
{
    TILE_TL_NBR = 0x10000000,
    TILE_TOP_NBR = 0x01000000,
    TILE_TR_NBR = 0x00100000,
    TILE_LT_NBR = 0x00010000,
    TILE_RT_NBR = 0x00001000,
    TILE_BL_NBR = 0x00000100,
    TILE_BOT_NBR = 0x00000010,
    TILE_BR_NBR = 0x00000001
} TILE_NBR_MASK_E;

/* enum to access an array of entries representing four directions */
typedef enum
{
    E_TOP = 0,
    E_LEFT = 1,
    E_RIGHT = 2,
    E_BOT = 3,

    E_FOUR_DIRECTIONS = 4
} IHEVCE_FOUR_DIRECTIONS_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

extern WORD8 gai1_hevc_luma_filter_taps[4][NTAPS_LUMA];
extern WORD8 gai1_hevc_chroma_filter_taps[8][NTAPS_CHROMA];

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

IV_API_CALL_STATUS_T ihevce_luma_inter_pred_pu(
    void *pv_inter_pred_ctxt,
    pu_t *ps_pu,
    void *pv_dst_buf,
    WORD32 dst_stride,
    WORD32 i4_flag_inter_pred_source);

IV_API_CALL_STATUS_T ihevce_luma_inter_pred_pu_high_speed(
    void *pv_inter_pred_ctxt,
    pu_t *ps_pu,
    UWORD8 **ppu1_dst_buf,
    WORD32 *pi4_dst_stride,
    func_selector_t *ps_func_selector);

void ihevce_chroma_inter_pred_pu(
    void *pv_inter_pred_ctxt, pu_t *ps_pu, UWORD8 *pu1_dst_buf, WORD32 dst_stride);

#endif /* _IHEVCE_INTER_PRED_H_ */
