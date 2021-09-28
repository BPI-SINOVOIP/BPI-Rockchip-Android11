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
* \file ihevce_lap_enc_structs.h
*
* \brief
*    This file contains structure definations shared between Encoder and RC
*
* \date
*    15/01/2013
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_RC_ENC_STRUCTS_H_
#define _IHEVCE_RC_ENC_STRUCTS_H_

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

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*SAD/Qscale is calculated over all CUs and summed into a 64 bit variable*/
/*Assuming 8*8 CU, sad can be 14 bit value */
/*For 4k*2k, number of 8*8 CUs is 131072 which is a 18 bit value */
/*Finally Qscale is mutlipled to this variable and it is 8 bit value*/
/*hence qformat can max be 64 - 14 - 18 - 8 - 1(sign) - 1(safty value) = 22 */
#define SAD_BY_QSCALE_Q 22
typedef struct
{
    UWORD32 u4_total_header_bits;
    UWORD32 u4_total_texture_bits;
    UWORD32 u4_total_sad;
    UWORD32 u4_total_intra_sad;
    UWORD32 u4_open_loop_intra_sad;
    WORD32 i4_qp_normalized_8x8_cu_sum[2];
    WORD32 i4_8x8_cu_sum[2];
    LWORD64 i8_sad_by_qscale[2];
    LWORD64 i8_total_ssd_frame;
    WORD32 i4_curr_qp_acc;
} rc_bits_sad_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void ihevce_enc_loop_get_frame_rc_prms(
    void *pv_enc_loop_ctxt, rc_bits_sad_t *ps_rc_prms, WORD32 i4_br_id, WORD32 i4_enc_frm_id);

#endif /* _IHEVCE_RC_ENC_STRUCTS_H_ */
