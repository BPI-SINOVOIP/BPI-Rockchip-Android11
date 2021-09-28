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
* \file bit_allocation.h
*
* \brief
*    This file contain bit processing function declarations
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef _BIT_ALLOCATION_H_
#define _BIT_ALLOCATION_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MIN_THRESHOLD_VBV_GOP_ERROR (0.30)
#define MAX_THRESHOLD_VBV_GOP_ERROR (0.80)
#define MAX_THRESHOLD_VBV_FRM_ERROR (0.80)

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct bit_allocation_t *bit_allocation_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
WORD32 bit_allocation_num_fill_use_free_memtab(
    bit_allocation_handle *pps_bit_allocation,
    itt_memtab_t *ps_memtab,
    ITT_FUNC_TYPE_E e_func_type);

void init_bit_allocation(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_num_intra_frm_interval, /* num such intervals */
    WORD32 i4_bit_rate, /* num bits per second */
    WORD32 i4_frm_rate, /* num frms in 1000 seconds */
    WORD32 *i4_peak_bit_rate,
    WORD32 i4_min_bitrate, /* The minimum bit rate that is to be satisfied for a gop */
    WORD32 i4_pels_in_frame,
    WORD32 i4_is_hbr,
    WORD32 i4_num_active_pic_type,
    WORD32 i4_lap_window,
    WORD32 i4_field_pic,
    WORD32 rc_pass,
    WORD32 i4_luma_pels,
    WORD32 i4_enable_look_ahead);

LWORD64 ba_get_rbip_and_num_frames(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    WORD32 *pi4_num_frames);
void assign_complexity_coeffs(
    bit_allocation_handle ps_bit_allocation, float af_sum_weigh[MAX_PIC_TYPE][3]);

void init_prev_header_bits(
    bit_allocation_handle ps_bit_allocation, pic_handling_handle ps_pic_handling);
/* Estimates the number of texture bits required by the current frame */
WORD32 get_cur_frm_est_texture_bits(
    bit_allocation_handle ps_bit_allocation,
    rc_rd_model_handle *pps_rd_model,
    est_sad_handle ps_est_sad,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buffer,
    picture_type_e e_pic_type,
    WORD32 i4_use_model,
    WORD32 i4_is_scd_frame,
    WORD32 i4_call_type,
    float i_to_avg_ratio,
    WORD32 i4_is_model_valid);

WORD32 bit_alloc_get_intra_bits(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buf_handling,
    picture_type_e e_pic_type,
    number_t *pvq_complexity_estimate,
    WORD32 i4_is_scd,
    float scd_ratio,
    WORD32 i4_call_type,
    WORD32 i4_non_I_scd,
    float f_percent_head_bits);

/* Estimate the number of header bits required by the current frame */
WORD32
    get_cur_frm_est_header_bits(bit_allocation_handle ps_bit_allocation, picture_type_e e_pic_type);

/* Get the remaining bits allocated in the period */
WORD32 get_rem_bits_in_period(
    bit_allocation_handle ps_bit_allocation, pic_handling_handle ps_pic_handling);

WORD32 ba_get_frame_rate(bit_allocation_handle ps_bit_allocation);

WORD32 get_bits_per_frame(bit_allocation_handle ps_bit_allocation);

WORD32 ba_get_bit_rate(bit_allocation_handle ps_bit_allocation);
void ba_get_peak_bit_rate(bit_allocation_handle ps_bit_allocation, WORD32 *pi4_peak_bit_rate);

LWORD64 ba_get_buffer_play_bits_for_cur_gop(bit_allocation_handle ps_bit_allocation);
LWORD64 ba_get_gop_bits(bit_allocation_handle ps_bit_allocation);
LWORD64 ba_get_gop_sad(bit_allocation_handle ps_bit_allocation);

/* Updates the bit allocation module with the actual encoded values */
void update_cur_frm_consumed_bits(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buf_handle,
    WORD32 i4_total_frame_bits,
    WORD32 i4_model_updation_hdr_bits,
    picture_type_e e_pic_type,
    UWORD8 u1_is_scd,
    WORD32 i4_last_frm_in_gop,
    WORD32 i4_lap_comp_bits_reset,
    WORD32 i4_suppress_bpic_update,
    WORD32 i4_buffer_based_bit_error,
    WORD32 i4_stuff_bits,
    WORD32 i4_lap_window_comp,
    rc_type_e e_rc_type,
    WORD32 i4_num_gop,
    WORD32 i4_is_pause_to_resume,
    WORD32 i4_est_text_bits_ctr_update_qp,
    WORD32 *pi4_gop_correction,
    WORD32 *pi4_new_correction);

void check_and_update_bit_allocation(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_max_bits_inflow_per_frm);

/* Based on the change in frame/bit rate update the remaining bits in period */
void change_remaining_bits_in_period(
    bit_allocation_handle ps_bit_allocation,
    WORD32 i4_bit_rate,
    WORD32 i4_frame_rate,
    WORD32 *i4_peak_bit_rate);

/* Change the gop size in the middle of a current gop */
void change_gop_size(
    bit_allocation_handle ps_bit_allocation,
    WORD32 i4_intra_frm_interval,
    WORD32 i4_inter_frm_interval,
    WORD32 i4_num_intra_frm_interval);

void update_rem_frms_in_period(
    bit_allocation_handle ps_bit_allocation,
    picture_type_e e_pic_type,
    UWORD8 u1_is_first_frm,
    WORD32 i4_intra_frm_interval,
    WORD32 i4_num_intra_frm_interval);

void change_rem_bits_in_prd_at_force_I_frame(
    bit_allocation_handle ps_bit_allocation, pic_handling_handle ps_pic_handling);

void change_ba_peak_bit_rate(bit_allocation_handle ps_bit_allocation, WORD32 *ai4_peak_bit_rate);

void init_intra_header_bits(bit_allocation_handle ps_bit_allocation, WORD32 i4_intra_header_bits);
WORD32 get_prev_header_bits(bit_allocation_handle ps_bit_allocation, WORD32 pic_type);
void set_Kp_Kb_for_hi_motion(bit_allocation_handle ps_bit_allocation);

void ba_get_qp_offset_offline_data(
    WORD32 ai4_offsets[5],
    WORD32 i4_ratio,
    float f_ratio,
    WORD32 i4_num_active_pic_type,
    WORD32 *pi4_complexity_bin);

void reset_Kp_Kb(
    bit_allocation_handle ps_bit_allocation,
    float f_i_to_avg_ratio,
    WORD32 i4_num_active_pic_type,
    float f_hme_sad_per_pixel,
    float f_max_hme_sad_per_pixel,
    WORD32 *pi4_complexity_bin,
    WORD32 i4_rc_pass);

WORD32 get_Kp_Kb(bit_allocation_handle ps_bit_allocation, picture_type_e e_pic_type);

/*get total bits for scene cut frame*/
WORD32 get_scene_change_tot_frm_bits(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buf_handling,
    WORD32 i4_num_pixels,
    WORD32 i4_f_sim_lap,
    float i_to_avg_rest,
    WORD32 i4_call_type,
    WORD32 i4_non_I_scd,
    WORD32 i4_is_infinite_gop);

void update_estimate_status(
    bit_allocation_handle ps_bit_allocation,
    WORD32 i4_est_texture_bits,
    WORD32 i4_hdr_bits,
    WORD32 i4_est_text_bits_ctr_get_qp);

void bit_allocation_set_num_scd_lap_window(
    bit_allocation_handle ps_bit_allocation,
    WORD32 i4_num_scd_in_lap_window,
    WORD32 i4_next_sc_i_in_rc_look_ahead);

void bit_allocation_set_sc_i_in_rc_look_ahead(
    bit_allocation_handle ps_bit_allocation, WORD32 i4_num_scd_in_lap_window);

/*updates gop based bit error entropy and rdopt estimate*/
void bit_allocation_update_gop_level_bit_error(
    bit_allocation_handle ps_bit_allocation, WORD32 i4_error_bits);
/*
The parsing of stat file is done at the end of init (by that time bit allocation init would have already happened,
The memory for gop stat data is alocated inside the parse stat file code. Hence the pointer has to be updated again
*/

void ba_init_stat_data(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    void *pv_gop_stat,
    WORD32 *pi4_pic_dist_in_cur_gop,
    WORD32 i4_total_bits_in_period,
    WORD32 i4_excess_bits);

void get_prev_frame_total_header_bits(
    bit_allocation_handle ps_bit_allocation,
    WORD32 *pi4_prev_frame_total_bits,
    WORD32 *pi4_prev_frame_header_bits,
    picture_type_e e_pic_type);

void rc_update_bit_distribution_gop_level_2pass(
    bit_allocation_handle ps_bit_allocation,
    pic_handling_handle ps_pic_handle,
    void *pv_gop_stat,
    rc_type_e e_rc_type,
    WORD32 i4_num_gop,
    WORD32 i4_start_gop_number,
    float f_avg_qscale_first_pass,
    WORD32 i4_max_ebf,
    WORD32 i4_ebf,
    LWORD64 i8_tot_bits_sequence,
    WORD32 i4_comp_error);

LWORD64 bit_alloc_get_gop_num(bit_allocation_handle ps_bit_allocation);

float get_cur_peak_factor_2pass(bit_allocation_handle ps_bit_allocation);
float get_cur_min_complexity_factor_2pass(bit_allocation_handle ps_bit_allocation);

void set_2pass_total_gops(bit_allocation_handle ps_bit_allocation, WORD32 i4_num_gop);
WORD32 ba_get_min_bits_per_frame(bit_allocation_handle ps_bit_allocation);

void set_bit_allocation_i_frames(
    bit_allocation_handle ps_bit_allocation,
    cbr_buffer_handle ps_cbr_buffer,
    pic_handling_handle ps_pic_handle,
    WORD32 i4_lap_window_comp,
    WORD32 i4_num_frames);

void bit_alloc_set_curr_i_to_sum_i(bit_allocation_handle ps_bit_allocation, float f_i_to_sum);

void ba_set_gop_stat_in_bit_alloc(
    bit_allocation_handle ps_bit_allocation, void *pv_gop_stat_summary);

WORD32 ba_get_luma_pels(bit_allocation_handle ps_bit_allocation);

void overflow_avoided_summation(WORD32 *pi4_accumulator, WORD32 i4_input);

float ba_get_sum_complexity_segment_cross_peak(bit_allocation_handle ps_bit_allocation);

WORD32 ba_get_prev_frame_tot_est_bits(bit_allocation_handle ps_bit_allocation, WORD32 i4_pic);

WORD32 ba_get_prev_frame_tot_bits(bit_allocation_handle ps_bit_allocation, WORD32 i4_pic);

void ba_set_avg_qscale_first_pass(
    bit_allocation_handle ps_bit_allocation, float f_average_qscale_1st_pass);

void ba_set_max_avg_qscale_first_pass(
    bit_allocation_handle ps_bit_allocation, float f_average_qscale_1st_pass);

float ba_get_max_avg_qscale_first_pass(bit_allocation_handle ps_bit_allocation);

float ba_get_avg_qscale_first_pass(bit_allocation_handle ps_bit_allocation);

float ba_get_min_complexity_for_peak_br(
    WORD32 i4_peak_bit_rate,
    WORD32 i4_bit_rate,
    float f_peak_rate_factor,
    float f_max_val,
    float f_min_val,
    WORD32 i4_pass);

float ba_gop_info_average_qscale_gop_without_offset(bit_allocation_handle ps_bit_allocation);

float ba_get_qscale_max_clip_in_second_pass(bit_allocation_handle ps_bit_allocation);

float ba_gop_info_average_qscale_gop(bit_allocation_handle ps_bit_allocation);
WORD32 ba_get_frame_number_in_gop(bit_allocation_handle ps_bit_allocation);

void bit_alloc_set_2pass_total_frames(
    bit_allocation_handle ps_bit_allocation, WORD32 i4_total_2pass_frames);

WORD32 ba_get_2pass_total_frames(bit_allocation_handle ps_bit_allocation);

WORD32 ba_get_2pass_bit_rate(bit_allocation_handle ps_bit_allocation);

void ba_set_2pass_bit_rate(bit_allocation_handle ps_bit_allocation, WORD32 i4_2pass_bit_rate);

void ba_set_2pass_avg_bit_rate(
    bit_allocation_handle ps_bit_allocation, LWORD64 i8_2pass_avg_bit_rate);

void ba_set_enable_look_ahead(bit_allocation_handle ps_bit_allocation, WORD32 i4_enable_look_ahead);
#endif
