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
* \file ihevce_deblk.h
*
* \brief
*    This file contains interface defination of deblock ctb function
*
* \date
*    06/11/2012
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_DEBLK_H_
#define _IHEVCE_DEBLK_H_

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

void ihevce_deblk_populate_qp_map(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    deblk_ctbrow_prms_t *ps_deblk_ctb_row_params,
    ctb_enc_loop_out_t *ps_ctb_out_dblk,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    ihevce_tile_params_t *ps_col_tile_params);

void ihevce_deblk_ctb(
    deblk_ctb_params_t *ps_deblk, WORD32 last_col, deblk_ctbrow_prms_t *ps_deblk_ctb_row_params);

#endif /* _IHEVCE_DEBLK_H_ */
