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
* \file rc_frame_info_collector.h
*
* \brief
*    This file contains structs used by encoder to pass information to RC lib
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef _RC_FRAME_INFO_COLLECTOR_H_
#define _RC_FRAME_INFO_COLLECTOR_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define MAX_NUM_FRAME_IN_GOP 300
#define MAX_CHAR_IN_LINE 250
#define MAX_MEM_FOR_LINE (MAX_CHAR_IN_LINE + 1)

// Minimum allocate memory for 10 bit allocation period
#define MIN_GOP_FOR_MEM_ALLOC 10

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    WORD32 sm; /* MSB 1 bit sign & rest magnitude */
    WORD32 e; /* Q-format */
} number_t_frame;

typedef struct
{
    LWORD64 i8_frame_num;
    WORD32 i4_poc;
    picture_type_e e_pic_type;
    WORD32 i4_rc_hevc_qp;
    WORD32 i4_scene_type; /*Should be in sync with what lap signals*/
    float f_8bit_q_scale;
    float f_8bit_q_scale_without_offset;
    float f_hbd_q_scale;
    float f_hbd_q_scale_without_offset;
    LWORD64 i8_cl_sad;
    LWORD64 i8_tex_bits;
    LWORD64 i8_header_bits;
    LWORD64 i8_L1_me_sad;
    LWORD64 i8_L1_ipe_raw_sad;
    LWORD64 i8_L1_me_or_ipe_raw_sad;
    LWORD64 i8_L0_open_cost;
    LWORD64 i8_est_texture_bits;
    WORD32 i4_num_scd_in_lap_window;
    WORD32 i4_num_frames_b4_scd;
    WORD32 i4_num_entries;
    LWORD64 i8_frame_acc_coarse_me_cost;
    WORD32 i4_lap_f_sim;
    float i_to_avg_bit_ratio;
    WORD32 i4_lap_complexity_q7;
    WORD32 i4_lap_var;
    LWORD64 i8_num_bit_alloc_period;
    WORD8 i1_is_complexity_based_bits_reset;
    float af_sum_weigh[MAX_PIC_TYPE][3];
    number_t_frame model_coeff_a_lin_wo_int;
    WORD32 i4_flag_rc_model_update;
    WORD32 i4_non_i_scd;
} frame_info_t;

typedef struct
{
    LWORD64 i4_gop_count;
    WORD32 i4_tot_frm_in_gop;
    //WORD32    ai4_pic_dist_in_cur_gop[MAX_PIC_TYPE];
    float f_bits_complexity_l1_based;
    float f_bits_complexity_l1_based_peak_factor;
    float f_complexity_l1_based;
    LWORD64 i8_bits_allocated_to_gop;
    LWORD64 i8_tot_bits_consumed_first_pass;
    float f_tot_bits_into_qscale_first_pass;
    LWORD64 i8_L1_complexity_sad[MAX_NUM_FRAME_IN_GOP];
    WORD8 ai1_is_complexlity_reset_bits[MAX_NUM_FRAME_IN_GOP];
    WORD8 ai1_scene_type[MAX_NUM_FRAME_IN_GOP];
    float f_den_wt_bits;
    WORD32 ai4_pic_type[MAX_NUM_FRAME_IN_GOP];
    LWORD64 ai8_head_bits_consumed[MAX_NUM_FRAME_IN_GOP];
    LWORD64 ai8_tex_bits_consumed[MAX_NUM_FRAME_IN_GOP];
    WORD32 ai4_first_pass_qscale[MAX_NUM_FRAME_IN_GOP];
    WORD32 ai4_q6_frame_offsets[MAX_NUM_FRAME_IN_GOP];
    float f_gop_level_buffer_play_factor;
    //WORD32 i4_flag_query_buffer_excess;
    float f_hbd_avg_q_scale_gop_without_offset;
    WORD32 i4_num_scene_cuts;
    LWORD64 i8_minimum_gop_bits;
    WORD32 i4_is_below_avg_rate_gop_frame;
    LWORD64 i8_cur_gop_bit_consumption;
    LWORD64 i8_actual_bits_allocated_to_gop;
    //LWORD64 i8_buffer_play_bits;
    LWORD64 i8_buffer_play_bits_allocated_to_gop;
    WORD32 i4_peak_br_clip;
    float f_buffer_play_complexity;
    float f_avg_complexity_factor;
    //float f_gop_level_complexity_sum;
    LWORD64 i8_max_bit_for_gop;
    LWORD64 i8_acc_gop_sad;
} gop_level_stat_t;
/*This should be exact order in which data should be dumped and read back from stat file*/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    S_FRAME_NUM = 0,
    S_POC,
    S_PIC_TYPE,
    S_HEVCQP,
    S_SCENE_TYPE,
    S_QSCALE,
    S_CL_SAD,
    S_HEAD_BITS,
    S_TEXT_BITS,
    S_EST_TEX_BITS,
    S_L1_ME_SAD,
    S_L1_IPE_SAD,
    MAX_PARAM_DUMP
} DUMP_PARAM_TYPE_E;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/
void init_frame_info(frame_info_t *frame_info);

void multi_pass_dump_frame_level_stat_binary(
    FILE *pf_stat_file,
    frame_info_t *ps_finfo,
    void *pv_lap_out,
    WORD32 i4_size_of_lap_out,
    void *pv_rc_lap_out,
    WORD32 i4_size_of_rc_lap_out,
    void *pv_sys_rc_api,
    WORD32 i4_br_id_for_2pass);

WORD32 multi_pass_extract_frame_data_binary(
    FILE *pf_stat_file,
    frame_info_t *ps_finfo,
    void *pv_lap_out,
    WORD32 i4_sizeof_lap_out,
    void *pv_rc_lap_out,
    WORD32 i4_sizeof_rc_lap_out,
    LWORD64 i8_frame_offset,
    WORD32 i4_flag,
    void *pv_sys_rc_api,
    WORD32 i4_br_id_for_2pass);

#endif
