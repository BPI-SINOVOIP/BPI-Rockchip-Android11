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
* \file ihevce_rc_interface.h
*
* \brief
*    This file contains interface defination of HEVC Rate control library
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************/

#ifndef _IHEVCE_RC_INTERFACE_H_
#define _IHEVCE_RC_INTERFACE_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define ES_TO_PQ_FACTOR (1.0)

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/*call type to distinguish call from enc/ preenc stage*/
typedef enum
{
    ENC_GET_QP = 0,
    PRE_ENC_GET_QP
} IHEVCE_RC_CALL_TYPE;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
WORD32 ihevce_rc_get_num_mem_recs(void);

WORD32 ihevce_rc_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 mem_space,
    ihevce_sys_api_t *ps_sys_api);

void *ihevce_rc_mem_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_bitrate_instance_id,
    rc_quant_t *ps_rc_quant,
    WORD32 i4_resolution_id,
    WORD32 i4_look_ahead_frames_in_first_pass);

void ihevce_rc_init(
    void *pv_ctxt,
    ihevce_src_params_t *ps_run_time_src_param,
    ihevce_tgt_params_t *ps_tgt_params,
    rc_quant_t *ps_rc_quant,
    ihevce_sys_api_t *ps_sys_api,
    ihevce_lap_params_t *ps_lap_prms,
    WORD32 i4_num_frame_parallel);

void ihevce_rc_update_pic_info(
    void *pv_ctxt,
    UWORD32 u4_total_bits_consumed,
    UWORD32 u4_total_header_bits,
    UWORD32 u4_frame_sad,
    UWORD32 u4_frame_intra_sad,
    IV_PICTURE_CODING_TYPE_T pic_type,
    WORD32 i4_avg_frame_qp,
    WORD32 i4_suppress_bpic_update,
    WORD32 *pi4_qp_normalized_8x8_cu_sum,
    WORD32 *pi4_8x8_cu_sum,
    LWORD64 *pi8_sad_by_qscale,
    ihevce_lap_output_params_t *ps_lap_out,
    rc_lap_out_params_t *ps_rc_lap_out,
    WORD32 i4_buf_id,
    UWORD32 u4_open_loop_intra_sad,
    LWORD64 i8_total_ssd_frame,
    WORD32 i4_enc_frm_id);

WORD32 ihevce_rc_get_pic_quant(
    void *pv_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    IHEVCE_RC_CALL_TYPE call_type,
    WORD32 i4_enc_frm_id,
    WORD32 i4_update_delay,
    WORD32 *pi4_curr_bits_estimated);

WORD32 ihevce_rc_get_cu_quant(WORD32 i4_frame_qp, WORD32 i4_cu_size);

void ihevce_rc_update_cur_frm_intra_satd(
    void *pv_ctxt, LWORD64 i8_cur_frm_intra_satd, WORD32 i4_enc_frm_id);

WORD32 ihevce_rc_get_scaled_mpeg2_qp(WORD32 i4_frame_qp, rc_quant_t *ps_rc_quant_ctxt);

WORD32 ihevce_rc_get_scaled_mpeg2_qp_q6(WORD32 i4_frame_qp, UWORD8 u1_bit_depth);

/*funtion top return hevce qp when input mpeg2 qp is in q6 format*/
WORD32 ihevce_rc_get_scaled_hevce_qp_q6(WORD32 i4_frame_qp_q6, UWORD8 u1_bit_depth);

WORD32 ihevce_rc_get_scaled_hevce_qp_q3(WORD32 i4_frame_qp, UWORD8 u1_bit_depth);

WORD32 ihevce_rc_get_scaled_hevc_qp_from_qs_q3(WORD32 i4_frame_qs_q3, rc_quant_t *ps_rc_quant_ctxt);

/* Functions dependent on lap input*/
WORD32 ihevce_rc_lap_get_scene_type(rc_lap_out_params_t *ps_rc_lap_out);

/*funciton that calculates scene change qp based on offline stat*/
WORD32 ihevce_rc_get_new_scene_qp(
    WORD32 i4_est_texture_bits,
    LWORD64 i8_satd_by_act_accum,
    WORD32 i4_variance,
    WORD32 i4_num_pixel);
/* Function to be called in entropy thread to account for error between rdopt bits
    estimate and actual bits generated in entropy thread*/
void ihevce_rc_rdopt_entropy_bit_correct(
    void *pv_rc_ctxt, WORD32 i4_cur_entropy_consumption, WORD32 i4_cur_time_stamp_low);
/*Funciton to get qp after L1 analysis using estimated L0 sadt/act so that L1 can happen with closer qp as that will be used by enc*/
WORD32 ihevce_get_L0_est_satd_based_scd_qp(
    void *pv_rc_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    LWORD64 i8_est_L0_satd_act,
    float i_to_avg_rest_ratio);

/*Function to calculate L0 satd using L1 satd based on offline stat*/
LWORD64 ihevce_get_L0_satd_based_on_L1(
    LWORD64 i8_satd_by_act_L1, WORD32 i4_num_pixel, WORD32 i4_cur_q_scale);

/*Function to get qp for Lap-1 to get qp for L1 analysis*/
WORD32 ihevce_rc_get_bpp_based_frame_qp(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out);

/*L1 data is registred so that L1 qp can be computed assuming previous frame data*/
void ihevce_rc_register_L1_analysis_data(
    void *pv_rc_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    LWORD64 i8_est_L0_satd_by_act,
    LWORD64 i8_pre_intra_sad,
    LWORD64 i8_l1_hme_sad);

/*populates qp for future frames for all possible pic type*/
void ihevce_rc_cal_pre_enc_qp(void *pv_rc_ctxt);

WORD32 ihevce_rc_pre_enc_qp_query(
    void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out, WORD32 i4_update_delay);
/*In flush mode L0 IPE has to wait till encoder populates pre-enc*/
/*THIS FUNCTION IS MEANT TO BE CALLED WITHOUT MUTEX LOCK*/
WORD32 ihevce_rc_check_is_pre_enc_qp_valid(void *pv_rc_ctxt, volatile WORD32 *pi4_force_end_flag);

float ihevce_get_i_to_avg_ratio(
    void *pv_rc_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    WORD32 i_to_p_qp_offset,
    WORD32 i4_offset_flag,
    WORD32 i4_call_type,
    WORD32 ai4_qp_offsets[4],
    WORD32 i4_update_delay);

/*funtion to detect scene change inside LAP*/
void ihevce_rc_check_non_lap_scd(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out);

void ihevce_get_dbf_buffer_size(
    void *pv_rc_ctxt, UWORD32 *pi4_buffer_size, UWORD32 *pi4_dbf, UWORD32 *pi4_bit_rate);

void ihevce_vbv_compliance_frame_level_update(
    void *pv_rc_ctxt,
    WORD32 i4_bits_generated,
    WORD32 i4_resolution_id,
    WORD32 i4_appln_bitrate_inst,
    UWORD32 u4_cur_cpb_removal_delay_minus1);

void ihevce_vbv_complaince_init_level(void *pv_ctxt, vui_t *ps_vui);

void ihevce_rc_register_dyn_change_bitrate(
    void *pv_ctxt, LWORD64 i8_new_bitrate, LWORD64 i8_new_peak_bitrate);
LWORD64 ihevce_rc_get_new_bitrate(void *pv_ctxt);

LWORD64 ihevce_rc_get_new_peak_bitrate(void *pv_ctxt);

LWORD64 ihevce_rc_change_avg_bitrate(void *pv_ctxt);

LWORD64 ihevce_rc_change_rate_factor(void *pv_ctxt);

void get_avg_bitrate_bufsize(void *pv_ctxt, LWORD64 *pi8_bitrate, LWORD64 *pi8_ebf);

void change_bitrate_vbv_complaince(void *pv_ctxt, LWORD64 i8_new_bitrate, LWORD64 i8_buffer_size);

void ihevce_compute_temporal_complexity_reset_Kp_Kb(
    rc_lap_out_params_t *ps_rc_lap_out, void *pv_rc_ctxt, WORD32 i4_Kp_Kb_reset_flag);

/* SGI & Enc Loop Parallelism related changes*/
void ihevce_rc_interface_update(
    void *pv_ctxt,
    IV_PICTURE_CODING_TYPE_T pic_type,
    rc_lap_out_params_t *ps_rc_lap_out,
    WORD32 i4_avg_frame_hevc_qp,
    WORD32 i4_enc_frm_id);

void ihevce_rc_store_retrive_update_info(
    void *pv_ctxt,
    rc_bits_sad_t *ps_rc_frame_stat,
    WORD32 i4_enc_frm_id,
    WORD32 bit_rate_id,
    WORD32 i4_store_retrive,
    WORD32 *pout_buf_id,
    WORD32 *pi4_pic_type,
    WORD32 *pcur_qp,
    void *ps_lap_out,
    void *ps_rc_lap_out);

void ihevce_set_L0_scd_qp(void *pv_rc_ctxt, WORD32 i4_scd_qp);

void rc_set_gop_inter_complexity(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out);

void rc_set_subgop_inter_complexity(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out);

void rc_set_gop_complexity_for_bit_allocation(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out);

float rc_get_buffer_level_unclip(void *pv_rc_ctxt);

void ihevce_rc_populate_common_params(
    ihevce_lap_output_params_t *ps_lap_out, rc_lap_out_params_t *ps_rc_lap_out);
#endif
