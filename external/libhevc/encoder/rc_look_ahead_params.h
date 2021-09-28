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
* \file rc_look_ahead_params.h
*
* \brief
*    TODO:
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef _RC_LOOK_AHEAD_PARAMS_H_
#define _RC_LOOK_AHEAD_PARAMS_H_

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/*
* Frame metrics
*/
typedef struct
{
    /* Frame variance. Spatial property */
    LWORD64 i8_8x8_var_lum;

    /* frame and histogram similarity */
    WORD32 ai4_hsim[3];
    WORD32 i4_fsim;

} rc_picture_metrics_t;

typedef struct
{
    /* common params for both lap_out and rc_lap_out */

    WORD32 i4_rc_pic_type;
    WORD32 i4_rc_poc;
    WORD32 i4_rc_temporal_lyr_id;
    WORD32 i4_rc_is_ref_pic;
    WORD32 i4_rc_scene_type;
    UWORD32 u4_rc_scene_num;
    WORD32 i4_rc_display_num;
    WORD32 i4_rc_quality_preset;
    WORD32 i4_rc_first_field;

    /* rc_lap_out specific params */

    /**
      * array of rc_lap_out_params_t pointer to access
      * the picture metrics of future pictures in capture order till
      * the look ahead frames
    */
    void *ps_rc_lap_out_next_encode;

    WORD32 i4_next_pic_type;

    WORD32 i4_is_I_only_scd;
    WORD32 i4_is_non_I_scd;

    LWORD64 i8_frame_satd_act_accum;
    LWORD64 i8_est_I_pic_header_bits;

    /*  Num pels in frame considered while accumulating the above satd metric */
    WORD32 i4_num_pels_in_frame_considered;
    /* Field type i.e either bottom or top is convyed */
    WORD32 i4_is_bottom_field;
    /* Coarse ME accumulated cost for entire frame */
    LWORD64 i8_frame_acc_coarse_me_cost;
    /* Coarse ME accumulated sad for entire frame */
    LWORD64 ai8_frame_acc_coarse_me_sad[52];
    /* L1 intra SATD */
    LWORD64 i8_pre_intra_satd;
    /* L1 intra SATD */
    LWORD64 ai8_pre_intra_sad[52];
    /* L1 IPE sad */
    LWORD64 i8_raw_pre_intra_sad;
    /* Frame - level L1 ME sad */
    LWORD64 i8_raw_l1_coarse_me_sad;
    /** Frame - level L1 satd/act accum*/
    LWORD64 i8_frame_satd_by_act_L1_accum;
    /** Frame - level L1 satd/act accum*/
    LWORD64 i8_satd_by_act_L1_accum_evaluated;
    /* Frame satd/act accumulated for L0 predicted based on L1 satd and qp used for L0 processing */
    LWORD64 i8_frm_satd_act_accum_L0_frm_L1;

    /* Frames for which online/offline model is not valid */
    WORD32 i4_is_model_valid;
    /* Steady State Frame */
    WORD32 i4_is_steady_state;

    LWORD64 i8_est_text_bits;
    LWORD64 i8_frame_num;

    frame_info_t *ps_frame_info;
    /* complexity metrics from LAP */
    rc_picture_metrics_t s_pic_metrics;

    WORD32 i4_is_cmplx_change_reset_model;
    WORD32 i4_is_cmplx_change_reset_bits;
    WORD32 i4_is_rc_model_needs_to_be_updated;
    WORD32 i4_next_sc_i_in_rc_look_ahead;
    WORD32 ai4_num_pic_type[MAX_PIC_TYPE];
    WORD32 ai4_offsets[5];
    WORD32 i4_offsets_set_flag;
    WORD32 i4_complexity_bin;
    WORD32 i4_ignore_for_rc_update;
    WORD32 i4_L1_qp;
    WORD32 i4_L0_qp;
    WORD32 i4_enable_lookahead;
    WORD32 i4_orig_rc_qp;
    WORD32 i4_use_offline_model_2pass;
    WORD32 i4_next_scene_type;
    WORD32 i4_perc_dc_blks;

    /* Used only in ix,vx versions */
    LWORD64 i8_frame_acc_satd_cost;
    WORD32 i4_l1_update_done;
    WORD32 i4_rc_i_pic_lamda_offset;
    float f_rc_pred_factor;

} rc_lap_out_params_t;

#endif
