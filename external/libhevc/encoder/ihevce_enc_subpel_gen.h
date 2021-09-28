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
* \file ihevce_enc_subpel_gen.h
*
* \brief
*    This file contains interface defination of Subpel Plane generation
*    function
*
* \date
*    29/12/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENC_SUBPEL_GEN_H_
#define _IHEVCE_ENC_SUBPEL_GEN_H_

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

void ihevce_subpel_padding(
    UWORD8 *pu1_dst,
    WORD32 stride,
    WORD32 tot_wd,
    WORD32 tot_ht,
    WORD32 pad_subpel_x,
    WORD32 pad_subpel_y,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    func_selector_t *ps_func_selector);

void ihevce_pad_interp_recon_ctb(
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    WORD32 quality_preset,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD16 *pi2_hxhy_interm,
    WORD32 i4_bitrate_instance,
    func_selector_t *ps_func_selector);

void ihevce_recon_padding(
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    func_selector_t *ps_func_selector);

void ihevce_pad_interp_recon_src_ctb(
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 ctb_ctr,
    WORD32 vert_ctr,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 i4_bitrate_instance_id,
    func_selector_t *ps_func_selector,
    WORD32 is_chroma_needs_padding);

#endif /* _IHEVCE_ENC_SUBPEL_GEN_H_ */
