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
* \file ihevce_enc_loop_pass.h
*
* \brief
*    This file contains interface defination of Encode loop pass function
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENC_LOOP_PASS_H_
#define _IHEVCE_ENC_LOOP_PASS_H_

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
WORD32 ihevce_enc_loop_get_num_mem_recs(WORD32 i4_num_bitrate_inst, WORD32 i4_num_enc_frm_parallel);

WORD32 ihevce_enc_loop_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_num_bitrate_inst,
    WORD32 i4_num_enc_frm_parallel,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id);

void *ihevce_enc_loop_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    void *pv_osal_handle,
    func_selector_t *ps_func_selector,
    rc_quant_t *ps_rc_quant_ctxt,
    ihevce_tile_params_t *ps_tile_params_base,
    WORD32 i4_resolution_id,
    WORD32 i4_num_enc_loop_frm_pllel,
    UWORD8 u1_is_popcnt_available);

void ihevce_enc_loop_reg_sem_hdls(
    void *pv_enc_loop_ctxt, void **ppv_sem_hdls, WORD32 i4_num_proc_thrds);

void ihevce_enc_loop_dep_mngr_frame_reset(void *pv_enc_loop_ctxt, WORD32 enc_frm_id);

void ihevce_enc_loop_delete(void *pv_enc_loop_ctxt);

void ihevce_enc_loop_frame_init(
    void *pv_enc_loop_ctxt,
    WORD32 i4_frm_qp,
    recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2],
    recon_pic_buf_t *ps_frm_recon,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    vps_t *ps_vps,
    WORD8 i1_weighted_pred_flag,
    WORD8 i1_weighted_bipred_flag,
    WORD32 log2_luma_wght_denom,
    WORD32 log2_chroma_wght_denom,
    WORD32 cur_poc,
    WORD32 i4_display_num,
    enc_ctxt_t *ps_enc_ctxt,
    me_enc_rdopt_ctxt_t *ps_cur_pic_ctxt,
    WORD32 i4_bitrate_instance_num,
    WORD32 i4_thrd_id,
    WORD32 i4_enc_frm_id,
    WORD32 i4_num_bitrates,
    WORD32 i4_quality_preset,
    void *pv_dep_mngr_encloop_dep_me);

void ihevce_enc_loop_process(
    void *pv_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    ctb_analyse_t *ps_ctb_in,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse,
    recon_pic_buf_t *ps_frm_recon,
    cur_ctb_cu_tree_t *ps_cu_tree_out,
    ctb_enc_loop_out_t *ps_ctb_out,
    cu_enc_loop_out_t *ps_cu_out,
    tu_enc_loop_out_t *ps_tu_out,
    pu_t *ps_pu_out,
    UWORD8 *pu1_frm_ecd_data,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 thrd_id,
    WORD32 i4_enc_frm_id,
    WORD32 i4_pass);

LWORD64 ihevce_cu_mode_decide(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    cu_analyse_t *ps_cu_analyse,
    final_mode_state_t *ps_final_mode_state,
    UWORD8 *pu1_ecd_data,
    pu_col_mv_t *ps_col_pu,
    UWORD8 *pu1_col_pu_map,
    WORD32 col_start_pu_idx);

#endif /* _IHEVCE_ENC_LOOP_PASS_H_ */
