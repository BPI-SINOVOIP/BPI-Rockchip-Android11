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
* \file ihevce_multi_thrd_funcs.h
*
* \brief
*    This file contains interface defination of related to Job Ques and others,
*     required for multi threading
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_MULTI_THRD_FUNCS_H_
#define _IHEVCE_MULTI_THRD_FUNCS_H_

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
void *ihevce_enc_grp_get_next_job(
    void *pv_multi_thrd_ctxt, WORD32 i4_job_type, WORD32 i4_blocking_mode, WORD32 i4_curr_frm_id);

void *ihevce_pre_enc_grp_get_next_job(
    void *pv_multi_thrd_ctxt, WORD32 i4_job_type, WORD32 i4_blocking_mode, WORD32 i4_ping_pong);

void ihevce_pre_enc_grp_job_set_out_dep(
    void *pv_multi_thrd_ctxt, job_queue_t *ps_curr_job, WORD32 i4_ping_pong);

void ihevce_enc_grp_job_set_out_dep(
    void *pv_multi_thrd_ctxt, job_queue_t *ps_curr_job, WORD32 i4_curr_frm_id);

void ihevce_prepare_job_queue(
    void *pv_enc_ctxt, ihevce_lap_enc_buf_t *ps_curr_inp, WORD32 i4_curr_frm_id);

void ihevce_prepare_pre_enc_job_queue(
    void *pv_enc_ctxt, ihevce_lap_enc_buf_t *ps_curr_inp, WORD32 i4_ping_pong);

#endif /* _IHEVCE_MULTI_THRD_FUNCS_H_ */
