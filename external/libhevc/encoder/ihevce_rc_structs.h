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
* @file ihevce_rc_structs.h
*
* @brief
*  This file contains rc interface structures and prototypes
*
* @author
*  Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_RC_STRUCTS_H_
#define _IHEVCE_RC_STRUCTS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define MAX_NUM_TEMPORAL_LAYERS 4
#define HALF_MAX_SCENE_ARRAY_QP MAX_SCENE_NUM / 2

/*moderate value of fsim to be passed when LAP is not enabled*/
#define MODERATE_FSIM_VALUE 110
#define MODERATE_LAP2_COMPLEXITY_Q7 25

/*also present in RATE CONTROL HEADER FILE with same name*/
#define MAX_LAP_COMPLEXITY_Q7 90

/*value of maximum variance in content used to generate offline model.*/
#define MAX_LAP_VAR 1000
#define AVG_LAP_VAR 400

/*buffer to store bit consumption between rdopt and entropy to calculate correction in entropy thread*/
#define NUM_BUF_RDOPT_ENT_CORRECT (NUM_FRMPROC_ENTCOD_BUFS + 1)  //+(1<<FRAME_PARALLEL_LVL))

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
/**
******************************************************************************
 *  @brief      Enumeration for memory records requested by entropy module
******************************************************************************
 */
typedef enum
{
    RC_CTXT = 0,
    RC_QSCALE_TO_QP,
    RC_QP_TO_QSCALE,
    RC_QP_TO_QSCALE_Q_FACTOR,
    RC_MULTI_PASS_GOP_STAT,

    /* should always be the last entry */
    NUM_RC_MEM_RECS

} IHEVCE_RC_MEM_TABS_T;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief  pre enc qp queue struct
******************************************************************************
 */
typedef struct
{
    WORD32 ai4_quant[NUM_RC_PIC_TYPE];
    WORD32 i4_scd_qp;
    WORD32 i4_is_qp_valid;
} pre_enc_qp_queue;

typedef struct
{
    LWORD64 ai8_L1_prev_I_intra_raw_satd[MAX_PIC_TYPE];
    LWORD64 ai8_L1_prev_pic_coarse_me_cost[MAX_PIC_TYPE];
    LWORD64 ai8_L1_prev_pic_coarse_me_sad[MAX_PIC_TYPE];
    UWORD32 au4_prev_scene_num[MAX_PIC_TYPE];
} rc_L1_state_t;

/**
******************************************************************************
 * vbv compliance testing struct
******************************************************************************
*/
typedef struct vbv_compliance_t
{
    /** frame rate  */
    float f_frame_rate;

    /** bit rate    */
    float f_bit_rate;

    /** current buffer level */
    float f_curr_buffer_level;

    /*current buffer level unclipped for current frame*/
    float f_curr_buffer_level_unclip;

    /** total buffer size */
    float f_buffer_size;

    /** drain rate */
    float f_drain_rate;
    /** previous cbp_removal_removal_delay minus 1**/
    UWORD32 u4_prev_cpb_removal_delay_minus1;

} vbv_compliance_t;

/* structure defined to maintain the qp's of Non reference b pictures based on reference */
/* b pictures of next layer  to handle in steadystate,SCD and Non_I_SCD's. The offset is */
/* based on the temporeal complexities of the sub GOP                                    */
typedef struct
{
    WORD32 i4_enc_order_num_rc;

    WORD32 i4_non_ref_B_pic_qp;

    UWORD32 u4_scene_num_rc;

} non_ref_b_qp_store_t;
/* structure to get high level stat from rc to adjust clip QP in case
if it causes encoder buffer  overflow*/
typedef struct
{
    /*online model valid flag*/
    WORD32 i4_is_model_valid;

    /*model given QP if model is valid either offline or online
    else set it to INVALID_QP*/
    WORD32 i4_modelQP;

    /*final RC QP,must be always valid*/
    WORD32 i4_finalQP;

    /* QP to reach maxEbf if model is valid*/
    WORD32 i4_maxEbfQP;

    /* bits for final QP if model is valid*/
    LWORD64 i8_bits_from_finalQP;

    /*offline model flag for I scd, non i scd, I only scd*/
    WORD32 i4_is_offline_model_used;

} rc_high_level_stat_t;

typedef struct
{
    /* START of static parameters*/
    rate_control_handle rc_hdl;
    rc_type_e e_rate_control_type;
    UWORD8 u1_is_mb_level_rc_on;
    /* bit rate to achieved across the entire file size */
    UWORD32 u4_avg_bit_rate;
    /* max possible drain rate */
    UWORD32 au4_peak_bit_rate[MAX_PIC_TYPE];
    UWORD32 u4_min_bit_rate;
    /* frames per 1000 seconds */
    UWORD32 u4_max_frame_rate;
    /* Buffer delay for CBR */
    UWORD32 u4_max_delay;
    /* Intraframe interval equal to GOP size */
    UWORD32 u4_intra_frame_interval;
    /* IDR period which indicates occurance of open GOP */
    UWORD32 u4_idr_period;
    /* Initial Qp array for I and P frames */
    WORD32 ai4_init_qp[MAX_PIC_TYPE];
    //0x3fffffff; /* Max VBV buffer size */
    UWORD32 u4_max_vbv_buff_size;
    /* MAx interval between I and P frame */
    WORD32 i4_max_inter_frm_int;
    /* Whether GOP is open or closed */
    WORD32 i4_is_gop_closed;
    WORD32 ai4_min_max_qp[MAX_PIC_TYPE * 2];
    /* Whether to use estimated SAD or Previous I frame SAD */
    WORD32 i4_use_est_intra_sad;
    UWORD32 u4_src_ticks;
    UWORD32 u4_tgt_ticks;

    WORD32 i4_auto_generate_init_qp;

    WORD32 i4_frame_width;
    WORD32 i4_frame_height;

    WORD32 i4_min_frame_qp;
    WORD32 i4_max_frame_qp;

    WORD32 i4_init_vbv_fullness;
    /* Num frames in lap window*/
    WORD32 i4_num_frame_in_lap_window;
    /** Max temporal layer the configured at init time*/
    WORD32 i4_max_temporal_lyr;
    /*Number of active picture type. Depends on max temporal reference*/
    WORD32 i4_num_active_pic_type;
    /* User defined constant qp or init qp to be used during scene cut*/
    WORD32 i4_init_frame_qp_user;
    /* To remember whether the pic type is field:1 or not:0*/
    WORD32 i4_field_pic;
    /*To convey whether top field is encoded first:1 or bottom field :0*/
    WORD32 i4_top_field_first;
    /** Quality preset to choose offline model coeff*/
    WORD32 i4_quality_preset;
    /*populate init pre enc qp based on bpp for all pictype*/
    WORD32 ai4_init_pre_enc_qp[MAX_PIC_TYPE];
    WORD32 i4_initial_decoder_delay_frames;

    float f_vbr_max_peak_sustain_dur;
    LWORD64 i8_num_frms_to_encode;

    WORD32 i4_min_scd_hevc_qp;

    UWORD8 u1_bit_depth;

    rc_quant_t *ps_rc_quant_ctxt;

    WORD32 i4_rc_pass;
    /*Memory allocated for storing GOP level stat*/
    void *pv_gop_stat;

    LWORD64 i8_num_gop_mem_alloc;

    WORD32 i4_is_infinite_gop;

    WORD32 ai4_offsets[5];
    /*End of static parameters */

    /* Start of parameters updated and accessed during pre-enc*/
    rc_L1_state_t s_l1_state_metric;
    /*estimate of pre-enc header bits*/
    LWORD64 i8_est_I_pic_header_bits;
    /** previous frame estimated L0 SATD/act predicted using pre-enc intra SAD*/
    LWORD64 ai8_prev_frame_est_L0_satd[MAX_PIC_TYPE];

    LWORD64 ai8_prev_frame_pre_intra_sad[MAX_PIC_TYPE];

    LWORD64 ai8_prev_frame_hme_sad[MAX_PIC_TYPE];

    /** Is previous frame intra sad available. set = 1 when atleast one frame of each picture type has been encoded*/
    WORD32 i4_is_est_L0_intra_sad_available;

    FILE *pf_stat_file;

    /* END of parameters updated and accessed during pre-enc */

    /* START of parameters updated during update call and accessed in other threads (pre enc/entropy)*/

    /*variables related to creation of pre enc qp queue*/
    pre_enc_qp_queue as_pre_enc_qp_queue[MAX_PRE_ENC_RC_DELAY];
    /*Remember RDOPT opt concumption, and corresponding time stamp*/
    WORD32 ai4_rdopt_bit_consumption_estimate[NUM_BUF_RDOPT_ENT_CORRECT];

    WORD32 ai4_rdopt_bit_consumption_buf_id[NUM_BUF_RDOPT_ENT_CORRECT];

    WORD32 i4_rdopt_bit_count;

    /*Remember entropy bit consumption and corresponding time stamp*/
    WORD32 ai4_entropy_bit_consumption[NUM_BUF_RDOPT_ENT_CORRECT];

    WORD32 ai4_entropy_bit_consumption_buf_id[NUM_BUF_RDOPT_ENT_CORRECT];

    WORD32 i4_entropy_bit_count;

    WORD32 i4_pre_enc_qp_read_index;

    WORD32 i4_pre_enc_qp_write_index;

    WORD32 i4_use_qp_offset_pre_enc;

    WORD32 i4_num_frms_from_reset;
    /*CAll back functions for print/write operations*/
    ihevce_sys_api_t *ps_sys_rc_api;

    LWORD64 i8_num_frame_read;

    LWORD64 i8_num_bit_alloc_period;

    vbv_compliance_t s_vbv_compliance;

    WORD32 i4_next_sc_i_in_rc_look_ahead;

    LWORD64 i8_new_bitrate;
        /*Set to -1 when no request. Positive value indicates pending change in bitrate request*/  //FRAME

    LWORD64 i8_new_peak_bitrate;

    WORD32 i4_num_frames_subgop;

    WORD32 i4_is_last_frame_scan;

    LWORD64 i8_total_acc_coarse_me_sad;

    WORD32 i4_L0_frame_qp;

    /** prev pic scene num of same temporal id*/
    UWORD32 au4_scene_num_temp_id[MAX_NUM_TEMPORAL_LAYERS];

    /* END of parameters updated during update call and accessed in other threads (pre enc/entropy)*/

    /* START of parameters to be updated at the query QP level(updation) */

    /** Intra frame cost exported by pre-enc IPE for current frame*/
    ULWORD64 ai8_cur_frm_intra_cost[MAX_NUM_ENC_LOOP_PARALLEL];
    /** remember prev frame intra cost*/
    ULWORD64 i8_prev_i_frm_cost;
    /* Current frame inter cost from coarse ME*/
    LWORD64 ai8_cur_frame_coarse_ME_cost[MAX_NUM_ENC_LOOP_PARALLEL];
    /** Flag for first frame so that same logic as scd can be used(offline data)*/
    WORD32 i4_is_first_frame_encoded;
    /*Flag to remember to reset I model only based on SCD detecton based on open loop SATD
      of two consecutive I pic*/
    WORD32 ai4_I_model_only_reset[MAX_NUM_ENC_LOOP_PARALLEL];
    /** prev pic intra cost for I pic and coarse ME cost for rest of picture types
        For intra L0 cost is availbale and HME cost is on L1 layer*/
    LWORD64 ai8_prev_frm_pre_enc_cost[MAX_PIC_TYPE];
    /*previous qp used encoded*/
    WORD32 ai4_prev_pic_hevc_qp[MAX_SCENE_NUM][MAX_PIC_TYPE];

    WORD32 ai4_scene_numbers[MAX_SCENE_NUM];

    /* END of parameters to be updated at the query QP lecvel */

    /* START of parameters to be maintained array for Enc loop parallelism */

    /** is scene cut frame at base layer*/
    WORD32 ai4_is_frame_scd[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC
    /*Flag to remember frames that are detected as scene cut but not made I due to another SCD following it immediately*/
    WORD32 ai4_is_non_I_scd_pic[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC
    /*Flag to remember pause to resume so that only P and B models can be reset*/
    WORD32 ai4_is_pause_to_resume[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC
    /*Frame similarity over look ahead window*/
    WORD32 ai4_lap_f_sim[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC
    /*Overall lap complexity including inter and intra in q7 format*/
    WORD32 ai4_lap_complexity_q7[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC

    float af_sum_weigh[MAX_NUM_ENC_LOOP_PARALLEL][MAX_PIC_TYPE][3];

    WORD32 ai4_is_cmplx_change_reset_model[MAX_NUM_ENC_LOOP_PARALLEL];

    WORD32 ai4_is_cmplx_change_reset_bits[MAX_NUM_ENC_LOOP_PARALLEL];

    float ai_to_avg_bit_ratio[MAX_NUM_ENC_LOOP_PARALLEL];

    WORD32 ai4_num_scd_in_lap_window[MAX_NUM_ENC_LOOP_PARALLEL];

    WORD32 ai4_num_frames_b4_scd[MAX_NUM_ENC_LOOP_PARALLEL];

    /* END of parameters to be maintained array for Enc loop parallelism */

    UWORD32 u4_prev_scene_num;

    WORD32 ai4_qp_for_previous_scene[MAX_PIC_TYPE];

    UWORD32 au4_prev_scene_num_pre_enc[MAX_PIC_TYPE];

    WORD32 ai4_qp_for_previous_scene_pre_enc[MAX_PIC_TYPE];

    UWORD32 u4_scene_num_est_L0_intra_sad_available;

    non_ref_b_qp_store_t as_non_ref_b_qp[MAX_NON_REF_B_PICS_IN_QUEUE_SGI];

    UWORD32 au4_prev_scene_num_multi_scene[MAX_NON_REF_B_PICS_IN_QUEUE_SGI];

    WORD32 ai4_qp_for_previous_scene_multi_scene[MAX_NON_REF_B_PICS_IN_QUEUE_SGI][MAX_PIC_TYPE];

    WORD32 i4_prev_qp_ctr;

    WORD32 i4_cur_scene_num;

    WORD32 i4_non_ref_B_ctr;

    float af_sum_weigh_2_pass[MAX_PIC_TYPE][3];

    rc_bits_sad_t as_rc_frame_stat_store[MAX_NUM_ENC_LOOP_PARALLEL]
                                        [IHEVCE_MAX_NUM_BITRATES];  //ELP_RC

    WORD32 out_buf_id[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];  //ELP_RC

    WORD32 i4_pic_type[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC

    WORD32 cur_qp[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES];  //ELP_RC

    ihevce_lap_output_params_t as_lap_out[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC

    rc_lap_out_params_t as_rc_lap_out[MAX_NUM_ENC_LOOP_PARALLEL];  //ELP_RC

    WORD32 i4_complexity_bin;

    WORD32 i4_last_p_or_i_frame_gop;

    WORD32 i4_qp_at_I_frame_for_skip_sad;

    WORD32 i4_denominator_i_to_avg;

    WORD32 i4_no_more_set_rbip_for_cur_gop;

    WORD32 i4_num_frm_scnd_fr_alloc;

    WORD32 i4_last_disp_num_scanned;

    LWORD64 i8_l1_analysis_lap_comp;

    WORD32 i4_est_text_bits_ctr_get_qp;  //ELP_RC

    WORD32 i4_est_text_bits_ctr_update_qp;  //ELP_RC

    WORD32 i4_num_frame_parallel;  //ELP_RC

    WORD32 i4_scene_num_latest;

    WORD32 i4_pre_enc_rc_delay;

    /*Enable this falg to do bit allocation within a gop in
    in second pass based on first pass data*/
    WORD32 i4_fp_bit_alloc_in_sp;

    WORD32 i4_bitrate_changed;

    /* Flag which shows that capped vbr mode is enabled */
    WORD32 i4_capped_vbr_flag;

    rc_high_level_stat_t s_rc_high_lvl_stat;

    WORD32 i4_normal_inter_pic;

    WORD32 i4_br_id_for_2pass;

    WORD32 ai4_scene_num_last_pic[MAX_PIC_TYPE];

    WORD32 ai4_last_tw0_lyr0_pic_qp[2];
} rc_context_t;

/* NOTE:: Please add any new parameters accordin to the categorization as specified in the comments of */
/* the structure definition. strat and end of the category are present in the defifnition*/

#endif
