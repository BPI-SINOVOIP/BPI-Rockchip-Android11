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
* \file ihevce_had_satd.h
*
* \brief
*    This file contains function prototypes of HAD and SATD functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_HAD_SATD_H_
#define _IHEVCE_HAD_SATD_H_

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

/* @breif enum for hadamard transform block sizes supported : 4x4 to 32x32 */
typedef enum
{
    HAD_4x4 = 0,
    HAD_8x8 = 1,
    HAD_16x16 = 2,
    HAD_32x32 = 3,
    HAD_INVALID = 4
} HAD_SIZE_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

UWORD32 ihevce_HAD_4x4_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd);

UWORD32 ihevce_HAD_8x8_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd);

UWORD32 ihevce_compute_ac_had_8x8_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd);

UWORD32 ihevce_HAD_16x16_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd);

UWORD32 ihevce_HAD_32x32_8bit(
    UWORD8 *pu1_origin,
    WORD32 src_strd,
    UWORD8 *pu1_pred_buf,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd);

typedef WORD32 FT_HAD_16X16_R(
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
    void *pv_func_sel);

typedef UWORD32 FT_HAD_32X32_USING_16X16(
    WORD16 *pi2_16x16_had,
    WORD32 had16_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf);

typedef UWORD32 ihevce_compute_16x16HAD_using_8x8_ft(
    WORD16 *pi2_8x8_had,
    WORD32 had8_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf);

typedef WORD32 FT_HAD_8X8_USING_4_4X4_R(
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
    void *pv_func_sel);

WORD32 ihevce_had_16x16_r_noise_detect(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 scaling_for_pred);

UWORD32 ihevce_compute_8x8HAD_using_4x4_noise_detect(
    WORD16 *pi2_4x4_had,
    WORD32 had4_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf);
void ihevce_had4_4x4_noise_detect(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst4x4,
    WORD16 *pi2_residue,
    WORD32 dst_strd,
    WORD32 scaling_for_pred);

void ihevce_had_8x8_using_4_4x4_noise_detect(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row,
    WORD32 scaling_for_pred);

void ihevce_had_8x8_using_4_4x4(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 **ppi4_hsad,
    WORD32 pos_x_y_4x4,
    WORD32 num_4x4_in_row);

typedef void ihevce_had_nxn_r_ft(
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
    void *pv_func_sel);

UWORD32 ihevce_mat_add_shift_satd_16bit(
    WORD16 *pi2_buf1,
    WORD32 buf1_strd,
    WORD16 *pi2_buf2,
    WORD32 buf2_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 size,
    WORD32 shift,
    WORD32 threshold,
    WORD32 *pi4_cbf);

UWORD32 ihevce_mat_sub_shift_satd_16bit(
    WORD16 *pi2_buf1,
    WORD32 buf1_strd,
    WORD16 *pi2_buf2,
    WORD32 buf2_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 size,
    WORD32 shift,
    WORD32 threshold,
    WORD32 *pi4_cbf);

void ihevce_mat_add_16bit(
    WORD16 *pi2_buf1,
    WORD32 buf1_strd,
    WORD16 *pi2_buf2,
    WORD32 buf2_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 size,
    WORD32 threshold);

void ihevce_mat_sub_16bit(
    WORD16 *pi2_buf1,
    WORD32 buf1_strd,
    WORD16 *pi2_buf2,
    WORD32 buf2_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 size,
    WORD32 threshold);

UWORD32 ihevce_compute_16x16HAD_using_8x8_noise_detect(
    WORD16 *pi2_8x8_had,
    WORD32 had8_strd,
    WORD16 *pi2_dst,
    WORD32 dst_strd,
    WORD32 i4_frm_qstep,
    WORD32 *pi4_cbf);

/******* C declarations ****************/
FT_HAD_8X8_USING_4_4X4_R ihevce_had_8x8_using_4_4x4_r;
FT_HAD_16X16_R ihevce_had_16x16_r;
FT_HAD_32X32_USING_16X16 ihevce_compute_32x32HAD_using_16x16;

/******** A9Q declarations **********/
FT_HAD_8X8_USING_4_4X4_R ihevce_had_8x8_using_4_4x4_r_neon;
FT_HAD_16X16_R ihevce_had_16x16_r_neon;
FT_HAD_32X32_USING_16X16 ihevce_compute_32x32HAD_using_16x16_neon;

#endif /* _IHEVCE_HAD_SATD_H_ */
