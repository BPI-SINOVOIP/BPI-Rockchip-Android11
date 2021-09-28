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
* \file ihevce_ipe_pass.h
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

#ifndef _IHEVCE_IPE_PASS_H_
#define _IHEVCE_IPE_PASS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IPE_TEST_DBG_L0 0
#define IPE_TEST_DBG_L1 0
#define IPE_TEST_DBG_L2 0
#define IPE_TEST_DBG_L3 0
#define IPE_TEST_DBG_L4 0

#define IPE_ME_DBG_L0 0
#define IPE_ME_DBG_L1 0

#define INTRA_PART_DBG 0  // Dump Debug Information related to intra partitioning

#define INTRA_NON_CTB_PIC_DBG 0

#define IPE_MODE_MAP_DBG 0

#define FAST_INTRA_8421_MODES_ENABLE 1

#define FAST_PART_WITH_OPTION_4 1

#define IPE_SAD_TYPE 0 /* 0 => Hadamard SAD, 1 => full SAD */
#define IPE_STEP_SIZE 1 /* Intra Prediction Mode Step Size During Analysis */
#define LAMBDA_DIV_FACTOR 1

/*satd/q_scale is accumualted cu level*/
#define SATD_BY_ACT_Q_FAC 10

/** defines the ratio of bits generated per cabac bin in Q8 format */
#define CABAC_BITS_PER_BIN 192

/** define modulation factor for qp modulation */
#define INTRA_QP_MOD_FACTOR_NUM 16
#define INTER_QP_MOD_FACTOR_NUM 4
#define QP_MOD_FACTOR_DEN 2

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    CU_1TU = 0,
    CU_4TU,
    SUB_CU_1TU,
    SUB_CU_4TU
} IPE_CU_TU_SPLIT_PATTERN;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
WORD32 ihevce_ipe_get_num_mem_recs(void);

WORD32
    ihevce_ipe_get_mem_recs(iv_mem_rec_t *ps_mem_tab, WORD32 i4_num_proc_thrds, WORD32 i4_mem_space);

void *ihevce_ipe_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_ref_id,
    func_selector_t *ps_func_selector,
    rc_quant_t *ps_rc_quant_ctxt,
    WORD32 i4_resolution_id,
    UWORD8 u1_is_popcnt_available);

void ihevce_intra_pred_ref_filtering(UWORD8 *pu1_src, WORD32 nt, UWORD8 *pu1_dst);
void ihevce_intra_pred_ref_filtering(UWORD8 *pu1_src, WORD32 nt, UWORD8 *pu1_dst);

UWORD32 ihevce_ipe_pass_satd(WORD16 *pi2_coeff, WORD32 coeff_stride, WORD32 trans_size);

void ihevce_ipe_process(
    void *pv_ctxt,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_L0_IPE_curr_out_pre_enc,
    ctb_analyse_t *ps_ctb_out,
    //cu_analyse_t     *ps_cu_out,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_ctb_out,
    void *pv_multi_thrd_ctxt,
    WORD32 slice_type,
    ihevce_ed_blk_t *ps_ed_pic_l1,
    ihevce_ed_blk_t *ps_ed_pic_l2,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_pic,
    WORD32 thrd_id,
    WORD32 i4_ping_pong);

void ihevce_populate_ipe_ol_cu_lambda_prms(
    void *pv_ctxt,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 i4_slice_type,
    WORD32 i4_temporal_lyr_id,
    WORD32 i4_lambda_type);

void ihevce_get_ipe_ol_cu_lambda_prms(void *pv_ctxt, WORD32 i4_cur_cu_qp);

void ihevce_populate_ipe_frame_init(
    void *pv_ctxt,
    ihevce_static_cfg_params_t *ps_stat_prms,
    WORD32 i4_curr_frm_qp,
    WORD32 i4_slice_type,
    WORD32 i4_thrd_id,
    pre_enc_me_ctxt_t *ps_curr_out,
    WORD8 i1_cu_qp_delta_enabled_flag,
    rc_quant_t *ps_rc_quant_ctxt,
    WORD32 i4_quality_preset,
    WORD32 i4_temporal_lyr_id,
    ihevce_lap_output_params_t *ps_lap_out);

LWORD64 ihevce_ipe_get_frame_intra_satd_cost(
    void *pv_ctxt,
    LWORD64 *pi8_frame_satd_by_qpmod,
    LWORD64 *pi8_frame_acc_mode_bits_cost,
    LWORD64 *pi8_frame_acc_activity_factor,
    LWORD64 *pi8_frame_l0_acc_satd);
#endif /* _IHEVCE_IPE_PASS_H_ */
