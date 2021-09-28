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
* \file hme_subpel.h
*
* \brief
*    Prototypes for subpel modules called from refinement layer
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_SUBPEL_H_
#define _HME_SUBPEL_H_

#include "ihevce_me_instr_set_router.h"

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/

void hme_subpel_refine_cu_hs(
    hme_subpel_prms_t *ps_prms,
    layer_ctxt_t *ps_curr_layer,
    search_results_t *ps_search_results,
    S32 search_idx,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    WORD32 blk_8x8_mask,
    me_func_selector_t *ps_func_selector,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list);

#endif /* #ifndef _HME_SUBPEL_H_ */
