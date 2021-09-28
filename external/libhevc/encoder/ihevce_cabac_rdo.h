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

/**
******************************************************************************
*
* @file ihevce_cabac_rdo.h
*
* @brief
*  This file contains function prototypes for rdopt cabac entropy modules
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_CABAC_RDO_H_
#define _IHEVCE_CABAC_RDO_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    UPDATE_ENT_SYNC_RDO_STATE = 0,
    STORE_ENT_SYNC_RDO_STATE = 1,
} CABAC_RDO_COPY_STATE_T;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_entropy_rdo_frame_init(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    vps_t *ps_vps,
    UWORD8 *pu1_cu_skip_top_row,
    rc_quant_t *ps_rc_quant_ctxt);

void ihevce_entropy_rdo_ctb_init(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, WORD32 ctb_x, WORD32 ctb_y);

WORD32 ihevce_entropy_rdo_encode_cu(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    enc_loop_cu_final_prms_t *ps_cu_prms,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 cu_size,
    WORD32 top_avail,
    WORD32 left_avail,
    void *pv_ecd_coeff,
    WORD32 *pi4_cu_rdopt_tex_bits);

void ihevce_entropy_update_best_cu_states(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 cu_size,
    WORD32 cu_skip_flag,
    WORD32 rdopt_best_cu_idx);

WORD32 ihevce_entropy_rdo_encode_tu(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    void *pv_ecd_coeff,
    WORD32 transform_size,
    WORD32 is_luma,
    WORD32 perform_sbh);

WORD32 ihevce_cabac_rdo_encode_sao(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, ctb_enc_loop_out_t *ps_ctb_enc_loop_out);

WORD32 ihevce_update_best_sao_cabac_state(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, WORD32 i4_best_buf_idx);

WORD32 ihevce_entropy_rdo_encode_tu_rdoq(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    void *pv_ecd_coeff,
    WORD32 transform_size,
    WORD32 is_luma,
    rdoq_sbh_ctxt_t *ps_rdoq_ctxt,
    LWORD64 *pi8_coded_tu_dist,
    LWORD64 *pi8_not_coded_tu_dist,
    WORD32 perform_sbh);

void ihevce_entropy_rdo_copy_states(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, UWORD8 *pu1_entropy_sync_states, WORD32 copy_mode);

#endif /* _IHEVCE_CABAC_RDO_H_ */
