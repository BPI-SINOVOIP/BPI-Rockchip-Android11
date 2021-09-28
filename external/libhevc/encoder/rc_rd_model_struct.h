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
* \file rc_rd_model_struct.h
*
* \brief
*    This file contains rate control rd model struct and constant macro
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef RC_RD_MODEL_STRUCT
#define RC_RD_MODEL_STRUCT

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
/* Tool Set Switch */
#define ENABLE_QUAD_MODEL 1

/* Number of elements for QP */
/* #define MPEG2_QP_ELEM       (MAX_MPEG2_QP + 1) */

/*#define MAX_NUM_PIC_TYPES_FOR_RC_MODEL  3*/ /* how many types of pictures will the rate-control handle */

#define QUAD 1
#define MIN_FRAMES_FOR_QUAD_MODEL 5
#define MAX_ACTIVE_FRAMES 16
#define MIN_FRAMES_FOR_LIN_MODEL 3
#define INVALID_FRAME_INDEX 255

#define UP_THR_SM 1 /* (1  /pow(2,4) = 0.0625   */
#define UP_THR_E 4

#define LO_THR_SM 368 /* (368.64 / pow(2,14)) = 0.0225 */
#define LO_THR_E 14

#define QUAD_DEV_THR_SM 1 /* (1 / pow(1,2)) = .25*/
#define QUAD_DEV_THR_E 2

#define LIN_DEV_THR_SM 1 /* (1 / pow(1,2)) = .25*/
#define LIN_DEV_THR_E 2

#define QUAD_MODEL 0
#define LIN_MODEL 1
#define PREV_FRAME_MODEL 2

/* Q Factors used for fixed point calculation */
#define Q_FORMAT_GAMMA 8
#define Q_FORMAT_ETA 8

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct rc_rd_model_t
{
    UWORD8 u1_curr_frm_counter;
    UWORD8 u1_num_frms_in_model;
    UWORD8 u1_max_frms_to_model;
    UWORD8 u1_model_used;

    UWORD32 pi4_res_bits[MAX_FRAMES_MODELLED];
    LWORD64 pi8_sad[MAX_FRAMES_MODELLED];

    UWORD8 pu1_num_skips[MAX_FRAMES_MODELLED];
    /* UWORD8   pu1_avg_qp[MAX_FRAMES_MODELLED]; */
    WORD32 ai4_avg_qp[MAX_FRAMES_MODELLED];
    WORD32 ai4_avg_qp_q6[MAX_FRAMES_MODELLED];
    /* UWORD8   au1_num_frames[MPEG2_QP_ELEM];  */

    model_coeff model_coeff_a_quad;
    model_coeff model_coeff_b_quad;
    model_coeff model_coeff_c_quad;

    model_coeff model_coeff_a_lin;
    model_coeff model_coeff_b_lin;
    model_coeff model_coeff_c_lin;

    model_coeff model_coeff_a_lin_wo_int;
    model_coeff model_coeff_b_lin_wo_int;
    model_coeff model_coeff_c_lin_wo_int;
} rc_rd_model_t;

#endif /* RC_RD_MODEL_STRUCT*/
