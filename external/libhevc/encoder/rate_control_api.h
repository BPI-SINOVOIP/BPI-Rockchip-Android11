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
* \file rate_control_api.h
*
* \brief
*    This file should only contain RC API function declarations
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef _RATE_CONTROL_API_H_
#define _RATE_CONTROL_API_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define RC_OK 0
#define RC_FAIL -1
#define RC_BENIGN_ERR -2

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

typedef struct rate_control_api_t *rate_control_handle;

WORD32 rate_control_num_fill_use_free_memtab(
    rate_control_handle *pps_rate_control_api,
    itt_memtab_t *ps_memtab,
    ITT_FUNC_TYPE_E e_func_type);

void initialise_rate_control(
    rate_control_handle ps_rate_control_api,
    rc_type_e e_rate_control_type,
    UWORD8 u1_is_mb_level_rc_on,
    UWORD32 u4_avg_bit_rate,
    UWORD32 *pu4_peak_bit_rate,
    UWORD32 u4_min_bit_rate,
    UWORD32 u4_frame_rate,
    UWORD32 u4_max_delay,
    UWORD32 u4_intra_frame_interval,
    UWORD32 u4_idr_period,
    WORD32 *pi4_init_qp,
    UWORD32 u4_max_vbv_buff_size,
    WORD32 i4_max_inter_frm_int,
    WORD32 i4_is_gop_closed,
    WORD32 *pi4_min_max_qp,
    WORD32 i4_use_est_intra_sad,
    UWORD32 u4_src_ticks,
    UWORD32 u4_tgt_ticks,
    WORD32 i4_frame_height,
    WORD32 i4_frame_width,
    WORD32 i4_num_active_pic_type,
    WORD32 i4_field_pic,
    WORD32 i4_quality_preset,
    WORD32 i4_lap_window,
    WORD32 i4_initial_decoder_delay_frames,
    float f_max_peak_rate_sustain_dur,
    LWORD64 i8_num_frames_to_encode,
    UWORD32 u4_min_scd_hevc_qp,
    UWORD8 u1_bit_depth,
    FILE *pf_rc_stat_file,
    WORD32 i4_rc_pass,
    void *pv_gop_stat,
    LWORD64 i8_num_gop_mem_alloc,
    WORD32 i4_is_infinite_gop,
    WORD32 i4_size_of_lap_out,
    WORD32 i4_size_of_rc_lap_out,
    void *pv_sys_api,
    WORD32 i4_fp_bit_alloc_in_sp,
    WORD32 i4_num_frame_parallel,
    WORD32 i4_capped_vbr_flag);

/*****************************************************************************
                         Process level API fuctions (FRAME LEVEL)
*****************************************************************************/
void flush_buf_frames(rate_control_handle ps_rate_control_api);

void post_encode_frame_skip(rate_control_handle ps_rate_control_api, picture_type_e e_pic_type);

void add_picture_to_stack(
    rate_control_handle rate_control_api, WORD32 i4_enc_pic_id, WORD32 i4_rc_in_pic);

void add_picture_to_stack_re_enc(
    rate_control_handle rate_control_api, WORD32 i4_enc_pic_id, picture_type_e e_pic_type);

void get_picture_details(
    rate_control_handle rate_control_api,
    WORD32 *pi4_pic_id,
    WORD32 *pi4_pic_disp_order_no,
    picture_type_e *pe_pic_type,
    WORD32 *pi4_is_scd);

WORD32 ihevce_rc_get_scaled_hevce_qp_q6(WORD32 i4_frame_qp_q6, UWORD8 u1_bit_depth);

void get_bits_for_final_qp(
    rate_control_handle ps_rate_control_api,
    WORD32 *pi4_modelQP,
    WORD32 *pi4_maxEbfQP,
    LWORD64 *pi8_bits_from_finalQP,
    WORD32 i4_clipQP,
    WORD32 i4_frame_qp_q6,
    WORD32 i4_cur_est_header_bits,
    WORD32 i4_est_tex_bits,
    WORD32 i4_buf_based_max_bits,
    picture_type_e e_pic_type,
    WORD32 i4_display_num);

WORD32 model_availability(rate_control_handle rate_control_api, picture_type_e e_pic_type);

WORD32 get_est_hdr_bits(rate_control_handle rate_control_api, picture_type_e e_pic_type);

/* Gets the frame level Qp (q scale in q6 format)*/
WORD32 get_frame_level_qp(
    rate_control_handle rate_control_api,
    picture_type_e pic_type,
    WORD32 i4_max_frm_bits,
    WORD32 *pi4_cur_est_texture_bits,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    WORD32 i4_call_type,
    float i_to_avg_ratio,
    frame_info_t *ps_frame_stat,
    WORD32 i4_complexity_bin,
    WORD32 i4_scene_num,
    WORD32 *i4_curr_bits_estimated,
    WORD32 *pi4_is_model_valid,
    WORD32 *pi4_vbv_buf_max_bits,
    WORD32 *pi4_est_tex_bits,
    WORD32 *pi4_cur_est_header_bits,
    WORD32 *pi4_maxEbfQP,
    WORD32 *pi4_modelQP,
    WORD32 *pi4_estimate_to_calc_frm_error);

WORD32 clip_qp_based_on_prev_ref(
    rate_control_handle rate_control_api,
    picture_type_e e_pic_type,
    WORD32 i4_call_type,
    WORD32 i4_scene_num);

/* Obtain the VBV buffer status information */
vbv_buf_status_e get_buffer_status(
    rate_control_handle rate_control_api,
    WORD32 i4_total_frame_bits, /* Total frame bits consumed */
    picture_type_e e_pic_type,
    WORD32 *pi4_num_bits_to_prevent_vbv_underflow);

/* Returns previous frame estimated bits for SCD validation*/
WORD32 get_prev_frm_est_bits(rate_control_handle ps_rate_control_api);

WORD32 rc_set_estimate_status(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_tex_bits,
    WORD32 i4_hdr_bits,
    WORD32 i4_est_text_bits_ctr_get_qp);

void rc_reset_pic_model(rate_control_handle ps_rate_control_api, picture_type_e pic_type);

/*reset the flag at qp query stage itself to differentiate scd frame for qp offset*/
void rc_reset_first_frame_coded_flag(
    rate_control_handle ps_rate_control_api, picture_type_e pic_type);

/*  get an estimate of total bits to find estimate of header bits after L1 stage in pre-enc*/
WORD32 rc_get_scene_change_est_header_bits(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_num_pixels,
    WORD32 i4_fsim_lap_avg,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    float i_to_avg_rest_ratio);

/* Used in case when picture handling module needs to move to next frame type. This happens
when the get frame qp and update frame qp do not happen within a frame and when there can be
multiple get frame qps beofre a update. If this function is called then i4_is_pic_handling_done
argument in update_frame_level_info should be set to 1 else 0 */
void update_pic_handling_state(rate_control_handle ps_rate_control_api, picture_type_e e_pic_type);

LWORD64 get_gop_sad(rate_control_handle ps_rate_control_api);

LWORD64 get_gop_bits(rate_control_handle ps_rate_control_api);

WORD32 check_if_current_GOP_is_simple(rate_control_handle ps_rate_control_api);

/* Updates the frame level changes in the Rate control */
void update_frame_level_info(
    rate_control_handle ps_rate_control_api,
    picture_type_e e_pic_type,
    LWORD64 *pi8_mb_type_sad, /* Frame level SAD for each type of MB[Intra/Inter] */
    WORD32 i4_total_frame_bits, /* Total frame bits actually consumed */
    WORD32 i4_model_updation_hdr_bits, /*header bits for model updation*/
    WORD32 *
        pi4_mb_type_tex_bits, /* Total texture bits consumed for each type of MB[Intra/Inter] used for model */
    LWORD64 *pi8_tot_mb_type_qp, /* Total qp of all MBs based on mb type */
    WORD32 *pi4_tot_mb_in_type, /* total number of mbs in each mb type */
    WORD32 i4_avg_activity, /* Average mb activity in frame */
    UWORD8 u1_is_scd, /* Is a scene change detected at the current frame */
    WORD32 i4_is_it_a_skip, /* If it's a pre-encode skip */
    WORD32 i4_intra_frm_cost, /* Sum of Intra cost for each frame */
    WORD32
        i4_is_pic_handling_done, /* Is pic handling [update_pic_handling_state] done before update */
    WORD32 i4_suppress_bpic_update,
    WORD32 i4_bits_to_be_stuffed,
    WORD32 i4_is_pause_to_resume,
    WORD32 i4_lap_window_comp,
    WORD32 i4_is_end_of_gop,
    WORD32 i4_lap_based_bits_reset,
    frame_info_t *ps_frame_info,
    WORD32 i4_is_rc_model_needs_to_be_updated,
    WORD8 i1_qp_offset,
    WORD32 i4_scene_num,
    WORD32 i4_num_frm_enc_in_scene,
    WORD32
        i4_est_text_bits_ctr_update_qp); /*complexity of future lap window used to set target buffer level at end if GOP*/

void update_frame_rc_get_frame_qp_info(
    rate_control_handle ps_rate_control_api,
    picture_type_e rc_pic_type,
    WORD32 i4_is_scd,
    WORD32 i4_is_pause_to_resume,
    WORD32 i4_avg_frame_qp_q6,
    WORD32 i4_suppress_bpic_update,
    WORD32 i4_scene_num,
    WORD32 i4_num_frm_enc_in_scene);

void reset_rc_for_pause_to_play_transition(rate_control_handle ps_rate_control_api);

WORD32 is_first_frame_coded(rate_control_handle ps_rate_control_api);

void rc_put_sad(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_cur_intra_sad,
    WORD32 i4_cur_sad,
    WORD32 i4_cur_pic_type);

WORD32 rc_get_qp_for_scd_frame(
    rate_control_handle ps_rate_control_api,
    picture_type_e e_pic_type,
    LWORD64 i8_satd_act_accum,
    WORD32 i4_num_pels_in_frame,
    WORD32 i4_est_I_pic_head_bits,
    WORD32 i4_f_sim_lap_avg,
    void *offline_model_coeff,
    float i_to_avg_ratio,
    WORD32 i4_true_scd,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    frame_info_t *ps_frame_stat,
    WORD32 i4_rc_2_pass,
    WORD32 i4_is_not_an_I_pic,
    WORD32 i4_ref_first_pass,
    WORD32 i4_call_type,
    WORD32 *pi4_total_bits,
    WORD32 *i4_curr_bits_estimated,
    WORD32 i4_use_offline_model_2pass,
    LWORD64 *pi8_i_tex_bits,
    float *pf_i_qs,
    WORD32 i4_best_br_id,
    WORD32 *pi4_estimate_to_calc_frm_error);

void rc_set_num_scd_in_lap_window(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_num_scd_in_lap_window,
    WORD32 i4_num_frames_b4_scd);

void rc_set_next_sc_i_in_rc_look_ahead(
    rate_control_handle ps_rate_control_api, WORD32 i4_next_sc_i_in_rc_look_ahead);

void rc_update_mismatch_error(rate_control_handle ps_rate_control_api, WORD32 i4_error_bits);

/*temp function to verify I only model*/
WORD32 rc_get_qp_scene_change_bits(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_total_bits,
    LWORD64 i8_satd_by_act_accum,
    WORD32 i4_num_pixel,
    void *offline_model_coeff,
    float f_i_to_average_rest,
    WORD32 i4_call_type);

WORD32 rc_get_bpp_based_scene_cut_qp(
    rate_control_handle ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 i4_num_pels_in_frame,
    WORD32 i4_f_sim_lap,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    WORD32 i4_call_type);

/*****************************************************************************
                        MB LEVEL API (just wrapper fucntions)
*****************************************************************************/
/* Intitalises frame level information for mb level qp */
void init_mb_rc_frame_level(
    rate_control_handle ps_rate_control_api, UWORD8 u1_frame_qp); /* Current frame qp*/

WORD32 get_bits_to_stuff(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_tot_consumed_bits,
    picture_type_e e_pic_type);

/******************************************************************************
                          Control Level API functions
Logic: The control call sets the state structure of the rate control api
accordingly such that the next process call would implement the same.
******************************************************************************/
/* Re-initialise the rate control module with the same old parameters */
/* void re_init_rate_control(rate_control_handle ps_rate_control_api); */

/* RC API call to change the inter frame interval */
void change_inter_frm_int_call(rate_control_handle ps_rate_control_api, WORD32 i4_inter_frm_int);

/* RC API call to change the intra frame interval */
void change_intra_frm_int_call(rate_control_handle ps_rate_control_api, WORD32 i4_intra_frm_int);

/* Sets the necessary changes for the new average bit rate */
void change_avg_bit_rate(
    rate_control_handle ps_rate_control_api, UWORD32 u4_average_bit_rate, UWORD32 u4_peak_bit_rate);

/* This is used for SOURCE FRAME RATE change from the application
   use case. Target frame rate change is taken care using the
   change_frm_rate_for_bit_alloc interface and modify frame rate
   module */
void change_frame_rate(
    rate_control_handle ps_rate_control_api,
    UWORD32 u4_frame_rate,
    UWORD32 u4_src_ticks,
    UWORD32 u4_target_ticks);

/* When the change in frame should affect only the bit_allocation
   This makes sense when the target frame rate changes. This change
   is gradually done with the use of modify frame rate. Refer the
   test application for beeter usecase */
void change_frm_rate_for_bit_alloc(rate_control_handle ps_rate_control_api, UWORD32 u4_frame_rate);

/* Set the init Qp values */
void change_init_qp(
    rate_control_handle ps_rate_control_api, WORD32 *pi4_init_qp, WORD32 i4_scene_num);

/* Sets the necessary changes for the new peak bit rate */

void force_I_frame(rate_control_handle ps_rate_control_api);

void change_min_max_qp(rate_control_handle ps_rate_control_api, WORD32 *pi4_min_max_qp);

/********************************************************************************
                            Getter functions
For getting the current state of the rate control structures
********************************************************************************/
UWORD32 rc_get_frame_rate(rate_control_handle ps_rate_control_api);
UWORD32 rc_get_bit_rate(rate_control_handle ps_rate_control_api);
UWORD32 rc_get_intra_frame_interval(rate_control_handle ps_rate_control_api);
UWORD32 rc_get_inter_frame_interval(rate_control_handle ps_rate_control_api);
rc_type_e rc_get_rc_type(rate_control_handle ps_rate_control_api);
WORD32 rc_get_bits_per_frame(rate_control_handle ps_rate_control_api);

UWORD32 rc_get_peak_bit_rate(rate_control_handle ps_rate_control_api, WORD32 i4_index);
UWORD32 rc_get_max_delay(rate_control_handle ps_rate_control_api);
UWORD32 rc_get_seq_no(rate_control_handle ps_rate_control_api);

WORD32 rc_get_rem_bits_in_period(rate_control_handle ps_rate_control_api);
WORD32 rc_get_vbv_buf_fullness(rate_control_handle ps_rate_control_api);
WORD32 rc_get_vbv_buf_size(rate_control_handle ps_rate_control_api);
WORD32 rc_get_vbv_fulness_with_cur_bits(rate_control_handle ps_rate_control_api, UWORD32 u4_bits);
WORD32 get_rc_target_bits(rate_control_handle ps_rate_control_api);
WORD32 get_orig_rc_target_bits(rate_control_handle ps_rate_control_api);
WORD32 rc_get_prev_header_bits(rate_control_handle ps_rate_control_api, WORD32 pic_type);
WORD32 rc_get_prev_P_QP(rate_control_handle ps_rate_control_api, WORD32 i4_scene_num);
WORD32 rc_update_ppic_sad(
    rate_control_handle ps_rate_control_api, WORD32 i4_est_sad, WORD32 i4_prev_ppic_sad);
void rc_get_sad(rate_control_handle ps_rate_control_api, WORD32 *pi4_sad);
WORD32 rc_get_ebf(rate_control_handle ps_rate_control_api);
void rc_init_set_ebf(rate_control_handle ps_rate_control_api, WORD32 i32_init_ebf);
void rc_update_prev_frame_intra_sad(
    rate_control_handle ps_rate_control_api, WORD32 i4_intra_frame_sad);
WORD32 rc_get_prev_frame_intra_sad(rate_control_handle ps_rate_control_api);
/*TO DO: previous frame intra SAD update function can also be replaced by below function*/
void rc_update_prev_frame_sad(
    rate_control_handle ps_rate_control_api, WORD32 i4_intra_frame_sad, picture_type_e e_pic_type);
WORD32 rc_get_prev_frame_sad(rate_control_handle ps_rate_control_api, picture_type_e e_pic_type);

/*update fsim of lap whenever fsim is updated in rc context*/
void rc_put_temp_comp_lap(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_lap_fsim,
    LWORD64 i8_per_pixel_p_frm_hme_sad_q10,
    picture_type_e e_pic_type);

void rc_get_pic_distribution(
    rate_control_handle ps_rate_control_api, WORD32 ai4_pic_type[MAX_PIC_TYPE]);

void rc_get_actual_pic_distribution(
    rate_control_handle ps_rate_control_api, WORD32 ai4_pic_type[MAX_PIC_TYPE]);

void rc_reset_Kp_Kb(
    rate_control_handle ps_rate_control_api,
    float f_i_to_avg_rest,
    WORD32 i4_num_active_pic_type,
    float f_curr_hme_sad_per_pixel,
    WORD32 *pi4_complexity_bin,
    WORD32 i4_rc_pass);

WORD32 rc_get_kp_kb(rate_control_handle ps_rate_control_api, picture_type_e e_pic_type);
WORD32 rc_get_ebf(rate_control_handle ps_rate_control_api);

float rc_get_cur_peak_factor_2pass(rate_control_handle ps_rate_control_api);
float rc_get_offline_normalized_complexity(
    WORD32 i4_intra_int, WORD32 i4_luma_pels, float f_per_pixel_complexity, WORD32 i4_pass_number);

void rc_bit_alloc_detect_ebf_stuff_scenario(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_num_frm_bef_scd_lap2,
    LWORD64 i4_total_bits_est_consu_lap2,
    WORD32 i4_max_inter_frm_int);

LWORD64 rc_get_rbip_and_num_frames(rate_control_handle ps_rate_contro_api, WORD32 *pi4_num_frames);

WORD32 bit_alloc_get_estimated_bits_for_pic(
    rate_control_handle ps_rate_contro_api,
    WORD32 i4_cur_frm_est_cl_sad,
    WORD32 i4_prev_frm_cl_sad,
    picture_type_e e_pic_type);

void rc_get_max_hme_sad_per_pixel(rate_control_handle ps_rate_control_api, WORD32 i4_total_pixels);

void rc_update_pic_distn_lap_to_rc(
    rate_control_handle ps_rate_contro_api, WORD32 ai4_num_pic_type[MAX_PIC_TYPE]);

void rc_set_bits_based_on_complexity(
    rate_control_handle ps_rate_contro_api, WORD32 i4_lap_window_comp, WORD32 i4_num_frames);

void rc_set_avg_qscale_first_pass(
    rate_control_handle ps_rate_contro_api, float f_average_qscale_1st_pass);

void rc_set_max_avg_qscale_first_pass(
    rate_control_handle ps_rate_control_api, float f_max_average_qscale_1st_pass);

void rc_set_i_to_sum_api_ba(rate_control_handle ps_rate_contro_api, float f_curr_i_to_sum);

float rc_get_min_complexity_factor_2pass(rate_control_handle ps_rate_contro_api);

void rc_set_p_to_i_complexity_ratio(
    rate_control_handle ps_rate_contro_api, float f_p_to_i_comp_ratio);

void rc_set_scd_in_period(rate_control_handle ps_rate_contro_api, WORD32 i4_scd_in_period);

void rc_ba_get_qp_offset_offline_data(
    rate_control_handle ps_rate_contro_api,
    WORD32 ai4_offsets[5],
    float f_hme_sad_per_pixel,
    WORD32 i4_num_active_pic_type,
    WORD32 *pi4_complexity_bin);

float rc_api_gop_level_averagae_q_scale_without_offset(rate_control_handle ps_rate_control_api);
picture_type_e rc_getprev_ref_pic_type(rate_control_handle ps_rate_control_api);
WORD32 rc_get_actual_intra_frame_int(rate_control_handle ps_rate_control_api);
float rc_get_qscale_max_clip_in_second_pass(rate_control_handle ps_rate_control_api);
void rc_set_2pass_total_frames(
    rate_control_handle ps_rate_control_api, WORD32 i4_total_2pass_frames);
void rc_set_2pass_avg_bit_rate(
    rate_control_handle ps_rate_control_api, LWORD64 i8_2pass_avg_bit_rate);

void rc_set_enable_look_ahead(rate_control_handle ps_rate_control_api, WORD32 i4_enable_look_ahead);

void rc_add_est_tot(rate_control_handle ps_rate_control_api, WORD32 i4_tot_tex_bits);
void rc_init_buffer_info(
    rate_control_handle ps_rate_control_api,
    WORD32 *pi4_vbv_buffer_size,
    WORD32 *pi4_currEbf,
    WORD32 *pi4_maxEbf,
    WORD32 *pi4_drain_rate);

#endif
