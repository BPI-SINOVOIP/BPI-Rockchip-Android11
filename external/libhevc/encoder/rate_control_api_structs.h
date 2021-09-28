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
* \file rate_control_api_structs.h
*
* \brief
*    This file contains rate_control API struct and constant macro
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef _RATE_CONTROL_API_STRUCTS_H_
#define _RATE_CONTROL_API_STRUCTS_H_

/* The following definitions were present in rc_cntrl_param.h, moved to this file
   as it is used by rate_control_api.c*/
/*#define VBR_BIT_ALLOC_PERIOD 3  num_frm_in_period = BIT_ALLOC_PERIOD*intra_frame_interval */
/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define CBR_BIT_ALLOC_PERIOD 1
#define MAX_SCENE_NUM_RC 30
#define HALF_MAX_SCENE_NUM_RC MAX_SCENE_NUM_RC / 2

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
/* Rate control state structure */
typedef struct rate_control_api_t
{
    rc_type_e e_rc_type; /* RC Algorithm */
    UWORD8 u1_is_mb_level_rc_on; /* Whether MB level rc is enabled or not */
    /* rate_control_param_t s_rate_control_param;                 Store a copy of input parameters for re-initialisation */
    pic_handling_handle ps_pic_handling; /* Picture handling struct */
    rc_rd_model_handle aps_rd_model[MAX_PIC_TYPE]; /* Model struct for I and P frms */
    vbr_storage_vbv_handle ps_vbr_storage_vbv; /* VBR storage VBV structure */
    est_sad_handle ps_est_sad; /* Calculate the estimated SAD */
    bit_allocation_handle ps_bit_allocation; /* Allocation of bits for each frame */
    mb_rate_control_handle ps_mb_rate_control; /* MB Level rate control state structure */
    sad_acc_handle ps_sad_acc; /* Sad accumulator */
    UWORD8 au1_is_first_frm_coded[MAX_PIC_TYPE];
    WORD32 ai4_prev_frm_qp[MAX_SCENE_NUM_RC][MAX_PIC_TYPE];
    WORD32 ai4_prev_frm_qp_q6[MAX_SCENE_NUM_RC][MAX_PIC_TYPE];

    cbr_buffer_handle ps_cbr_buffer;
    UWORD8 au1_avg_bitrate_changed[MAX_PIC_TYPE];
    UWORD8 u1_is_first_frm;
    /* UWORD8               au1_min_max_qp[(MAX_PIC_TYPE << 1)]; */
    WORD32 ai4_min_qp[MAX_PIC_TYPE];
    WORD32 ai4_max_qp[MAX_PIC_TYPE];
    WORD32 ai4_max_qp_q6[MAX_PIC_TYPE];
    WORD32 ai4_min_qp_q6[MAX_PIC_TYPE];

    WORD32 i4_prev_frm_est_bits;
    WORD32 i4_orig_frm_est_bits;
    vbr_str_prms_t s_vbr_str_prms;
    init_qp_handle ps_init_qp;
    /* Store the values which are to be impacted after a delay */
    UWORD32 u4_frms_in_delay_prd_for_peak_bit_rate_change;
    UWORD32 au4_new_peak_bit_rate[MAX_NUM_DRAIN_RATES];
    picture_type_e prev_ref_pic_type;
    WORD32 i4_P_to_I_ratio;
    WORD32 ai4_min_texture_bits[MAX_PIC_TYPE];
    /* Complexity based buffer movement */
    WORD32 i4_prev_ref_is_scd;
    WORD32 i4_is_hbr; /*Flag to indicate CBR_NLDRC_HBR*/
    WORD32 i4_num_active_pic_type;
    WORD32 i4_lap_f_sim;
    WORD32 i4_quality_preset;
    WORD32 i4_scd_I_frame_estimated_tot_bits;
    WORD32 i4_I_frame_qp_model; /*offline = 0, online = 1*/
    LWORD64 i8_per_pixel_p_frm_hme_sad_q10;
    UWORD32 u4_min_scd_hevc_qp;
    UWORD32 u4_bit_depth_based_max_qp;
    UWORD8 u1_bit_depth;
    FILE *pf_rc_stat_file;
    WORD32 i4_rc_pass; /*variable to differentiate first pass and second pass*/
    WORD32 i4_max_frame_width;
    WORD32 i4_max_frame_height;
    void *pv_2pass_gop_summary;
    WORD32 i4_num_gop;
    void *pv_rc_sys_api;
    /*In static cases signal the future underflow warning to lower the qp*/
    WORD32 i4_underflow_warning;
    float f_max_hme_sad_per_pixel;
    /*f_p_to_i_comp_ratio is for comparison of pre intra complexity of  i & p frames
    It is used for jacking up of p frame qp if i frame was
    extremely simple to avoid overconsumption of bits in p frame*/
    float f_p_to_i_comp_ratio;
    /*i4_scd_in_period_2_pass is used to signal the scd in period for 2 pass
    this signal is one of the criteria for clipping the sudden increase of qp*/
    WORD32 i4_scd_in_period_2_pass;
    WORD32 i4_is_infinite_gop;
    WORD32 i4_frames_since_last_scd;
    WORD32 i4_num_frame_parallel;
    WORD32 ai4_est_tot_bits[MAX_NUM_FRAME_PARALLEL];
    WORD32 i4_capped_vbr_flag;
} rate_control_api_t;

#endif /*_RATE_CONTROL_API_STRUCTS_H_*/
