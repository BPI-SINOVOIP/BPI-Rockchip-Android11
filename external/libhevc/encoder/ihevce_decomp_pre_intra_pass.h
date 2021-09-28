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
* \file ihevce_decomp_pre_intra_pass.h
*
* \brief
*    This file contains declarations related to frame decomposition done during
*    pre intra processing
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_DECOMP_PRE_INTRA_PASS_H_
#define _IHEVCE_DECOMP_PRE_INTRA_PASS_H_

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
extern WORD32 g_i4_ip_funcs[MAX_NUM_IP_MODES];

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void ihevce_ed_4x4_find_best_modes(
    UWORD8 *pu1_src,
    WORD32 src_stride,
    UWORD8 *pu1_nbr,
    UWORD16 *pu2_mode_bits_cost,
    UWORD8 *pu1_best_modes,
    WORD32 *pu1_best_sad_costs,
    WORD32 u1_low_resol,
    FT_SAD_COMPUTER *pf_4x4_sad_computer);

WORD32 ihevce_decomp_pre_intra_get_num_mem_recs(void);

WORD32 ihevce_decomp_pre_intra_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab, WORD32 i4_num_proc_thrds, WORD32 i4_mem_space);

void *ihevce_decomp_pre_intra_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    func_selector_t *ps_func_selector,
    WORD32 i4_resolution_id,
    UWORD8 u1_is_popcnt_available);

void ihevce_decomp_pre_intra_process(
    void *pv_ctxt,
    ihevce_lap_output_params_t *ps_lap_out_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    void *pv_multi_thrd_ctxt,
    WORD32 thrd_id,
    WORD32 i4_ping_pong);

void ihevce_decomp_pre_intra_frame_init(
    void *pv_ctxt,
    UWORD8 **ppu1_decomp_lyr_bufs,
    WORD32 *pi4_lyr_buf_stride,
    ihevce_ed_blk_t *ps_layer1_buf,
    ihevce_ed_blk_t *ps_layer2_buf,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    WORD32 i4_ol_sad_lambda_qf,
    ctb_analyse_t *ps_ctb_analyse);

void ihevce_decomp_pre_intra_curr_frame_pre_intra_deinit(
    void *pv_pre_intra_ctxt,
    pre_enc_me_ctxt_t *ps_curr_out,
    frm_ctb_ctxt_t *ps_frm_ctb_prms);

void ihevce_scale_by_2(
    UWORD8 *pu1_src,
    WORD32 src_stride,
    UWORD8 *pu1_dst,
    WORD32 dst_stride,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_wkg_mem,
    WORD32 ht_offset,
    WORD32 block_ht,
    WORD32 wd_offset,
    WORD32 block_wd,
    FT_COPY_2D *pf_copy_2d,
    FT_SCALING_FILTER_BY_2 *pf_scaling_filter_mxn);

void ihevce_ed_frame_init(void *pv_ed_ctxt, WORD32 i4_layer_no);

float fast_log2(float val);

WORD32 ihevce_cu_level_qp_mod(
    WORD32 frm_qscale,
    WORD32 cu_satd,
    long double frm_avg_activity,
    float f_mod_strength,
    WORD32 *pi4_act_factor,
    WORD32 *pi4_q_scale_mod,
    rc_quant_t *rc_quant_ctxt);

LWORD64 ihevce_decomp_pre_intra_get_frame_satd(void *pv_ctxt, WORD32 *wd, WORD32 *ht);

LWORD64 ihevce_decomp_pre_intra_get_frame_satd_squared(void *pv_ctxt, WORD32 *wd, WORD32 *ht);

#endif
