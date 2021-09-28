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
* \file rc_rd_model.h
*
* \brief
*    This file contains all the necessary declarations for
*    Rate Distortion related functions
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef RC_RD_MODEL
#define RC_RD_MODEL

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define RC_FIXED_POINT 1
#define MAX_FRAMES_MODELLED 16

#if !RC_FIXED_POINT
typedef float model_coeff;
#else
typedef number_t model_coeff;
#endif
typedef struct rc_rd_model_t *rc_rd_model_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
WORD32 rc_rd_model_num_fill_use_free_memtab(
    rc_rd_model_handle *pps_rc_rd_model, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type);
/* Interface Functions */
/* Initialise the rate distortion model */
void init_frm_rc_rd_model(rc_rd_model_handle ps_rd_model, UWORD8 u1_max_frames_modelled);

/* Reset the rate distortion model */
void reset_frm_rc_rd_model(rc_rd_model_handle ps_rd_model);

/* Returns the Qp to be used for the given bits and SAD */
WORD32 find_qp_for_target_bits(
    rc_rd_model_handle ps_rd_model,
    UWORD32 u4_target_res_bits,
    UWORD32 u4_estimated_sad,
    WORD32 i4_max_qp_q6,
    WORD32 i4_min_qp_q6);
/* Updates the frame level statistics after encoding a frame */
void add_frame_to_rd_model(
    rc_rd_model_handle ps_rd_model,
    UWORD32 i4_res_bits,
    WORD32 i4_avg_mp2qp_q6,
    LWORD64 i8_sad_h264,
    UWORD8 u1_num_skips);

UWORD32 estimate_bits_for_qp(
    rc_rd_model_handle ps_rd_model, UWORD32 u4_estimated_sad, WORD32 i4_avg_qp_q6);
/* Get the Linear model coefficient */
model_coeff get_linear_coefficient(rc_rd_model_handle ps_rd_model);

void set_linear_coefficient(rc_rd_model_handle ps_rd_model, number_t model_coeff_a_lin_wo_int);

WORD32 is_model_valid(rc_rd_model_handle ps_rd_model);

#endif
