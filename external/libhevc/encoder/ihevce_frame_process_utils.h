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
* \file ihevce_frame_process_utils.h
*
* \brief
*    This file contains declarations of top level functions related to frame
*    processing
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_FRAME_PROCESS_UTILS_H_
#define _IHEVCE_FRAME_PROCESS_UTILS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define C1 13317 /* This value is twice of (0.01*255)^2 in Q11*/
#define C2 119854 /* This value is twice of (0.03*255)^2 in Q11*/

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

WORD32 ihevce_get_cur_frame_qp(
    WORD32 static_params_frame_qp,
    WORD32 slice_type,
    WORD32 temporal_id,
    WORD32 min_qp,
    WORD32 max_qp,
    rc_quant_t *ps_rc_quant_ctxt);

void ihevce_fill_sei_payload(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    frm_proc_ent_cod_ctxt_t *ps_curr_out);

void ihevce_dyn_bitrate(void *pv_hle_ctxt, void *pv_dyn_bitrate_prms);

#endif /* _IHEVCE_FRAME_PROCESS_UTILS_H_ */
