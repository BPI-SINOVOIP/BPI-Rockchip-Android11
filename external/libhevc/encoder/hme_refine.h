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
* \file hme_refine.h
*
* \brief
*    Prototypes for coarse layer refine functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_REFINE_H_
#define _HME_REFINE_H_

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

void hme_refine(
    me_ctxt_t *ps_ctxt,
    refine_prms_t *ps_refine_prms,
    PF_EXT_UPDATE_FXN_T pf_ext_update_fxn,
    //PF_GET_INTRA_CU_AND_COST pf_get_intra_cu_and_cost,
    layer_ctxt_t *ps_coarse_layer,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    S32 lyr_job_type,
    S32 thrd_id,
    S32 me_frm_id,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_l0_ipe_input);

void hme_refine_no_encode(
    coarse_me_ctxt_t *ps_ctxt,
    refine_prms_t *ps_refine_prms,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    S32 lyr_job_type,
    WORD32 i4_ping_pong,
    void **ppv_dep_mngr_hme_sync);

void hme_refine_frm_init(
    layer_ctxt_t *ps_curr_layer, refine_prms_t *ps_refine_prms, layer_ctxt_t *ps_coarse_layer);

#endif /* #ifndef _HME_REFINE_H_ */
