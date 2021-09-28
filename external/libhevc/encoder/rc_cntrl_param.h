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
* \file rc_cntrl_param.h
*
* \brief
*    This file should contain only enumerations and macros exported to codec
*    by RC
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef _RC_CNTRL_PARAM_H_
#define _RC_CNTRL_PARAM_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define QSCALE_Q_FAC 6
#define QSCALE_Q_FAC_3 3

/*corresponds to hevc qp = 4, not letting it go below 4 for better stability*/
#define MIN_QSCALE_Q6 64

#define INVALID_QP (-55)
#define SCD_MIN_HEVC_QP 18

/*For very high bitrate reduce min qp limit*/
#define SCD_MIN_HEVC_QP_HBR 14
#define SCD_MIN_HEVC_QP_VHBR 10

#define SCD_MAX_HEVC_QP 48
#define MAX_HEVC_QP 51

/*classification based on i_to_average rest ratio so that appropriate qp offset can be chosen*/
#define I_TO_REST_EXTREME_FAST (1.5)
#define I_TO_REST_VVFAST (3)
#define I_TO_REST_VFAST (5)
#define I_TO_REST_FAST (8)
#define I_TO_REST_MEDI (10)
#define I_TO_REST_SLOW (14)

#define MAX_NUM_FRAME_PARALLEL 8

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/* RC algo type */
typedef enum
{
    VBR_STORAGE = 0,
    VBR_STORAGE_DVD_COMP = 1,
    VBR_STREAMING = 2,
    CONST_QP = 3,
    CBR_LDRC = 4,
    CBR_NLDRC = 5,
    CBR_NLDRC_HBR = 6,
    MAX_RC_TYPE
} rc_type_e;

/**/
typedef enum
{
    FIELD_OFFSET = 4
} field_offset_e;

/* Picture type structure*/
typedef enum
{
    BUF_PIC = -1,
    I_PIC = 0,
    P_PIC,
    B_PIC,
    B1_PIC,
    B2_PIC,
    P1_PIC,
    BB_PIC,
    B11_PIC,
    B22_PIC,
    MAX_PIC_TYPE
} picture_type_e;

typedef enum
{
    I_PIC_SCD = 0x100,
    NA_PIC
} picture_type_SCD;

/* MB Type structure*/
typedef enum
{
    MB_TYPE_INTRA,
    MB_TYPE_INTER,
    MAX_MB_TYPE /* Based on MB TYPES added the array size increases */
} mb_type_e;

typedef enum
{
    VBV_NORMAL,
    VBV_UNDERFLOW,
    VBV_OVERFLOW,
    VBR_CAUTION
} vbv_buf_status_e;

#endif
