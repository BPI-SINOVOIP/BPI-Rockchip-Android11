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
* \file hme_coarse.h
*
* \brief
*    Prototypes for coarse layer functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_COARSE_H_
#define _HME_COARSE_H_

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

/**
********************************************************************************
*  @fn     void hme_coarse_frm_init(me_ctxt_t *ps_ctxt, coarse_prms_t *ps_coarse_prms)
*
*  @brief  Frame init entry point Coarse ME.
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_coarse_prms : Coarse layer config params
*
*  @return None
********************************************************************************
*/
void hme_coarse_frm_init(coarse_me_ctxt_t *ps_ctxt, coarse_prms_t *ps_coarse_prms);

/**
********************************************************************************
*  @fn     void hme_coarse(me_ctxt_t *ps_ctxt, coarse_prms_t *ps_coarse_prms)
*
*  @brief  Top level entry point for Coarse ME. Runs across blks and searches
*          at a 4x4 blk granularity by using 4x8 and 8x4 patterns.
*
*  @param[in,out]  ps_ctxt: ME Handle
*
*  @param[in]  ps_coarse_prms : Coarse layer config params
*
*  @return None
********************************************************************************
*/

void hme_coarsest(
    coarse_me_ctxt_t *ps_ctxt,
    coarse_prms_t *ps_coarse_prms,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 i4_ping_pong,
    void **ppv_dep_mngr_hme_sync);

#endif /* #ifndef _HME_COARSE_H_ */
