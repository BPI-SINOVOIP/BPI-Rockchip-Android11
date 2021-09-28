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
* \file ihevce_frame_process.h
*
* \brief
*    This file contains interface defination of frame proceswsing pass
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_FRAME_PROCESS_H_
#define _IHEVCE_FRAME_PROCESS_H_

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

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
DOUBLE ihevce_compute_psnr(
    UWORD8 *pu1_ip_buf,
    UWORD8 *pu1_ref_buf,
    WORD32 width,
    WORD32 height,
    WORD32 horz_jmp,
    WORD32 ip_stride,
    WORD32 ref_stride,
    double *acc_mse,
    ihevce_logo_attrs_t *ps_logo_ctxt,
    WORD32 i4_is_chroma);

void ihevce_pre_enc_manage_ref_pics(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    pre_enc_me_ctxt_t *ps_curr_out,
    WORD32 i4_ping_pong);

void ihevce_manage_ref_pics(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    slice_header_t *ps_slice_header,
    WORD32 i4_me_frm_id,
    WORD32 i4_thrd_id,
    WORD32 i4_bitrate_instance_id);

void ihevce_get_frame_lambda_prms(
    enc_ctxt_t *ps_enc_ctxt,
    pre_enc_me_ctxt_t *ps_cur_pic_ctxt,
    WORD32 i4_cur_frame_qp,
    WORD32 first_field,
    WORD32 i4_is_ref_pic,
    WORD32 i4_temporal_lyr_id,
    double f_i_pic_lamda_modifier,
    WORD32 i4_inst_id,
    WORD32 i4_lambda_type);

void calc_l1_level_hme_intra_sad_different_qp(
    enc_ctxt_t *ps_enc_ctxt,
    pre_enc_me_ctxt_t *ps_curr_out,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    WORD32 i4_tot_ctb_l1_x,
    WORD32 i4_tot_ctb_l1_y);

WORD32 ihevce_pre_enc_process_frame_thrd(void *pv_hle_ctxt);

WORD32 ihevce_enc_frm_proc_slave_thrd(void *pv_frm_proc_thrd_ctxt);

void ihevce_set_pre_enc_prms(enc_ctxt_t *ps_enc_ctxt);

#endif /* _IHEVCE_FRAME_PROCESS_H_ */
