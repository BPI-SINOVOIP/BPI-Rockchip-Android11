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
* \file ihevce_recur_bracketing.h
*
* \brief
*    This file contains interface definition of structs and fucntions for
*    recursive bracketing
*
* \date
*    12/02/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_RECUR_BRACKETING_H_
#define _IHEVCE_RECUR_BRACKETING_H_

//*****************************************************************************/
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
/* Function Declarations                                                     */
/*****************************************************************************/
void ihevce_recur_bracketing(
    ihevce_ipe_ctxt_t *ps_ctxt,
    ihevce_ipe_cu_tree_t *ps_cu_node,
    iv_enc_yuv_buf_t *ps_curr_src,
    ctb_analyse_t *ps_ctb_out,
    cu_analyse_t *ps_row_cu);

void ihevce_bracketing_analysis(
    ihevce_ipe_ctxt_t *ps_ctxt,
    ihevce_ipe_cu_tree_t *ps_cu_node,
    iv_enc_yuv_buf_t *ps_curr_src,
    ctb_analyse_t *ps_ctb_out,
    //cu_analyse_t         *ps_row_cu,
    ihevce_ed_blk_t *ps_ed_l1_ctb,
    ihevce_ed_blk_t *ps_ed_l2_ctb,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    ipe_l0_ctb_analyse_for_me_t *ps_l0_ipe_out_ctb);

void ihevce_mode_eval_filtering(
    ihevce_ipe_cu_tree_t *ps_cu_node,
    ihevce_ipe_cu_tree_t *ps_child_cu_node,
    ihevce_ipe_ctxt_t *ps_ctxt,
    iv_enc_yuv_buf_t *ps_curr_src,
    WORD32 best_amode,
    WORD32 *best_costs_4x4,
    UWORD8 *best_modes_4x4,
    WORD32 step2_bypass,
    WORD32 tu_eq_cu);

void ihevce_update_cand_list(
    ihevce_ipe_cu_tree_t *ps_cu_node, ihevce_ed_blk_t *ps_ed_blk_l1, ihevce_ipe_ctxt_t *ps_ctxt);

WORD32 sad_nxn_blk(
    UWORD8 *pu1_inp, WORD32 i4_inp_stride, UWORD8 *pu1_ref, WORD32 i4_ref_stride, WORD32 trans_size);

#endif /* _IHEVCE_RECUR_BRACKETING_H_ */
