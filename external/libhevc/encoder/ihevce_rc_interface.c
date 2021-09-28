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
* @file ihevce_rc_interface.c
*
* @brief
*  This file contains function definitions for rc api interface
*
* @author
*  Ittiam
*
* List of Functions
*  <TODO: Update this>
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <math.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"
#include "mem_req_and_acq.h"
#include "rate_control_api.h"
#include "var_q_operator.h"

#include "ihevc_defs.h"
#include "ihevc_debug.h"
#include "ihevc_macros.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_sao.h"
#include "ihevc_resi_trans.h"
#include "ihevc_quant_iquant_ssd.h"

#include "ihevce_defs.h"
#include "ihevce_hle_interface.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_lap_interface.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_rc_structs.h"
#include "ihevce_rc_interface.h"
#include "ihevce_frame_process_utils.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define USE_USER_FIRST_FRAME_QP 0
#define DEBUG_PRINT 0
#define DETERMINISTIC_RC 1
#define USE_QP_OFFSET_POST_SCD 1
#define USE_SQRT 0
#define K_SCALING_FACTOR 8
#define ENABLE_2_PASS_BIT_ALLOC_FRM_1ST 0

#define VBV_THRSH_I_PIC_DELTA_QP_1 (0.85)
#define VBV_THRSH_I_PIC_DELTA_QP_2 (0.75)
#define VBV_THRSH_P_PIC_DELTA_QP_1 (0.80)
#define VBV_THRSH_P_PIC_DELTA_QP_2 (0.70)
#define VBV_THRSH_BR_PIC_DELTA_QP_1 (0.75)
#define VBV_THRSH_BR_PIC_DELTA_QP_2 (0.65)
#define VBV_THRSH_BNR_PIC_DELTA_QP_1 (0.75)
#define VBV_THRSH_BNR_PIC_DELTA_QP_2 (0.65)
#define VBV_THRSH_DELTA_QP (0.6)

#define VBV_THRSH_FRM_PRLL_I_PIC_DELTA_QP_1 (0.70)
#define VBV_THRSH_FRM_PRLL_I_PIC_DELTA_QP_2 (0.60)
#define VBV_THRSH_FRM_PRLL_P_PIC_DELTA_QP_1 (0.65)
#define VBV_THRSH_FRM_PRLL_P_PIC_DELTA_QP_2 (0.55)
#define VBV_THRSH_FRM_PRLL_BR_PIC_DELTA_QP_1 (0.60)
#define VBV_THRSH_FRM_PRLL_BR_PIC_DELTA_QP_2 (0.50)
#define VBV_THRSH_FRM_PRLL_BNR_PIC_DELTA_QP_1 (0.60)
#define VBV_THRSH_FRM_PRLL_BNR_PIC_DELTA_QP_2 (0.50)
#define VBV_THRSH_FRM_PRLL_DELTA_QP (0.45)

#define TRACE_SUPPORT 0

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

/*
Modified bpp vs nor satd/act/qp :
=================================

Prestine Quality
-----------------
480p  y = -0.1331x3 - 0.0589x2 + 2.5091x - 0.0626
720p  y = -0.3603x3 + 0.4504x2 + 2.2056x - 0.0411
1080p y = -0.7085x3 + 0.9743x2 + 1.939x - 0.0238
2160p y = -1.2447x3 + 2.1218x2 + 1.4995x - 0.0108

High Quality
-------------
480p  y = -0.1348x3 - 0.0557x2 + 2.5055x - 0.0655
720p  y = -0.0811x3 + 0.1988x2 + 1.246x - 0.0385
1080p y = -0.74x3 + 1.0552x2 + 1.8942x - 0.0251
2160p y = -1.3851x3 + 2.3372x2 + 1.4255x - 0.0113

Medium Speed
-------------
480p  y = -0.143x3 - 0.0452x2 + 2.5581x - 0.0765
720p  y = -0.3997x3 + 0.542x2 + 2.201x - 0.0507
1080p y = -0.816x3 + 1.2048x2 + 1.8689x - 0.0298
2160p y = -1.5169x3 + 2.5857x2 + 1.3478x - 0.0126

High Speed
-----------
480p  y = -0.1472x3 - 0.0341x2 + 2.5605x - 0.0755
720p  y = -0.3967x3 + 0.526x2 + 2.2228x - 0.0504
1080p y = -0.8008x3 + 1.1713x2 + 1.8897x - 0.0297
2160p y = -1.503x3 + 2.576x2 + 1.3476x - 0.0123

Extreme Speed
--------------
480p  y = -0.1379x3 - 0.059x2 + 2.5716x - 0.0756
720p  y = -0.3938x3 + 0.521x2 + 2.2239x - 0.0505
1080p y = -0.8041x3 + 1.1725x2 + 1.8874x - 0.0293
2160p y = -1.4863x3 + 2.556x2 + 1.344x - 0.0122

*/

const double g_offline_i_model_coeff[20][4] = {

    /*ultra_HD*/
    { -1.2447, 2.1218, 1.4995, -0.0108 }, /*Prestine quality*/
    { -1.3851, 2.3372, 1.4255, -0.0113 }, /*High quality*/
    { -1.5169, 2.5857, 1.3478, -0.0126 }, /*Medium speed*/
    { -1.503, 2.576, 1.3476, -0.0123 }, /*high speed*/
    { -1.4863, 2.556, 1.344, -0.0122 }, /*Extreme Speed*/

    /*Full HD*/
    { -0.7085, 0.9743, 1.939, -0.0238 }, /*Prestine quality*/
    { -0.74, 1.0552, 1.8942, -0.0251 }, /*High quality*/
    { -0.816, 1.2048, 1.8689, -0.0298 }, /*Medium speed*/
    { -0.8008, 1.1713, 1.8897, -0.0297 }, /*high speed*/
    { -0.8041, 1.1725, 1.8874, -0.0293 }, /*Extreme Speed*/

    /*720p*/
    { -0.3603, 0.4504, 2.2056, -0.0411 }, /*Prestine quality*/
    // {-0.0811, 0.1988, 1.246, - 0.0385},/*High quality*/
    { -0.3997, 0.542, 2.201, -0.0507 },
    { -0.3997, 0.542, 2.201, -0.0507 }, /*Medium speed*/
    { -0.3967, 0.526, 2.2228, -0.0504 }, /*high speed*/
    { -0.3938, 0.521, 2.2239, -0.0505 }, /*Extreme Speed*/

    /*SD*/
    { -0.1331, -0.0589, 2.5091, -0.0626 }, /*Prestine quality*/
    { -0.1348, -0.0557, 2.5055, -0.0655 }, /*High quality*/
    { -0.143, -0.0452, 2.5581, -0.0765 }, /*Medium speed*/
    { -0.1472, -0.0341, 2.5605, -0.0755 }, /*high speed*/
    { -0.1379, -0.059, 2.5716, -0.0756 } /*Extreme Speed*/

};

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

picture_type_e ihevce_rc_conv_pic_type(
    IV_PICTURE_CODING_TYPE_T pic_type,
    WORD32 i4_field_pic,
    WORD32 i4_temporal_layer_id,
    WORD32 i4_is_bottom_field,
    WORD32 i4_top_field_first);

WORD32 ihevce_rc_get_scaled_mpeg2_qp(WORD32 i4_frame_qp, rc_quant_t *ps_rc_quant_ctxt);

static WORD32 ihevce_get_offline_index(rc_context_t *ps_rc_ctxt, WORD32 i4_num_pels_in_frame);

static void ihevce_rc_get_pic_param(
    picture_type_e rc_pic_type, WORD32 *pi4_tem_lyr, WORD32 *pi4_is_bottom_field);

static double ihevce_get_frame_lambda_modifier(
    WORD8 slice_type,
    WORD32 i4_rc_temporal_lyr_id,
    WORD32 i4_first_field,
    WORD32 i4_rc_is_ref_pic,
    WORD32 i4_num_b_frms);

static WORD32 ihevce_clip_min_max_qp(
    rc_context_t *ps_rc_ctxt,
    WORD32 i4_hevc_frame_qp,
    picture_type_e rc_pic_type,
    WORD32 i4_rc_temporal_lyr_id);

WORD32 ihevce_ebf_based_rc_correction_to_avoid_overflow(
    rc_context_t *ps_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out, WORD32 *pi4_tot_bits_estimated);

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
************************************************************************
* @brief
*    return number of records used by RC
************************************************************************
*/
WORD32 ihevce_rc_get_num_mem_recs(void)
{
    WORD32 i4_num_rc_mem_tab = 0;

    /*get the number of memtab request from RC*/
    rate_control_handle ps_rate_control_api;
    itt_memtab_t *ps_memtab = NULL;
    i4_num_rc_mem_tab =
        rate_control_num_fill_use_free_memtab(&ps_rate_control_api, ps_memtab, GET_NUM_MEMTAB);

    return ((NUM_RC_MEM_RECS + i4_num_rc_mem_tab));
}

/*!
************************************************************************
* @brief
*    return each record attributes of RC
************************************************************************
*/
WORD32 ihevce_rc_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 mem_space,
    ihevce_sys_api_t *ps_sys_api)
{
    float f_temp;
    WORD32 i4_temp_size;
    WORD32 i4_num_memtab = 0;
    WORD32 i4_num_rc_mem_tab, i;
    rate_control_handle ps_rate_control_api;
    itt_memtab_t *ps_itt_memtab = NULL;
    itt_memtab_t as_rc_mem_tab[30];

    /*memory requirements to store RC context */
    ps_mem_tab[RC_CTXT].i4_mem_size = sizeof(rc_context_t);
    //DBG_PRINTF("size of RC context = %d\n",sizeof(rc_context_t));
    ps_mem_tab[RC_CTXT].e_mem_type = (IV_MEM_TYPE_T)mem_space;

    ps_mem_tab[RC_CTXT].i4_mem_alignment = 64;

    (void)ps_sys_api;
    //i4_temp_size = (51 + ((ps_init_prms->s_src_prms.i4_bit_depth - 8) * 6));
    i4_temp_size = (51 + ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth - 8) * 6));

    ps_mem_tab[RC_QP_TO_QSCALE].i4_mem_size = (i4_temp_size + 1) * 4;
    ps_mem_tab[RC_QP_TO_QSCALE].e_mem_type = (IV_MEM_TYPE_T)mem_space;
    ps_mem_tab[RC_QP_TO_QSCALE].i4_mem_alignment = 64;

    ps_mem_tab[RC_QP_TO_QSCALE_Q_FACTOR].i4_mem_size = (i4_temp_size + 1) * 4;
    ps_mem_tab[RC_QP_TO_QSCALE_Q_FACTOR].e_mem_type = (IV_MEM_TYPE_T)mem_space;
    ps_mem_tab[RC_QP_TO_QSCALE_Q_FACTOR].i4_mem_alignment = 64;

    f_temp = (float)(51 + ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth - 8) * 6));
    f_temp = ((float)(f_temp - 4) / 6);
    i4_temp_size = (WORD32)((float)pow(2, f_temp) + 0.5);
    i4_temp_size = (i4_temp_size << 3);  // Q3 format is mantained for accuarate calc at lower qp

    ps_mem_tab[RC_QSCALE_TO_QP].i4_mem_size = (i4_temp_size + 1) * sizeof(UWORD32);
    ps_mem_tab[RC_QSCALE_TO_QP].e_mem_type = (IV_MEM_TYPE_T)mem_space;
    ps_mem_tab[RC_QSCALE_TO_QP].i4_mem_alignment = 64;

    /*memory requirements to store RC context */
    ps_mem_tab[RC_MULTI_PASS_GOP_STAT].i4_mem_size = sizeof(gop_level_stat_t);
    ps_mem_tab[RC_MULTI_PASS_GOP_STAT].e_mem_type = (IV_MEM_TYPE_T)mem_space;
    ps_mem_tab[RC_MULTI_PASS_GOP_STAT].i4_mem_alignment = 64;

    i4_num_rc_mem_tab =
        rate_control_num_fill_use_free_memtab(&ps_rate_control_api, ps_itt_memtab, GET_NUM_MEMTAB);

    i4_num_memtab =
        rate_control_num_fill_use_free_memtab(&ps_rate_control_api, as_rc_mem_tab, FILL_MEMTAB);

    for(i = 0; i < i4_num_memtab; i++)
    {
        ps_mem_tab[i + NUM_RC_MEM_RECS].i4_mem_size = as_rc_mem_tab[i].u4_size;
        ps_mem_tab[i + NUM_RC_MEM_RECS].i4_mem_alignment = as_rc_mem_tab[i].i4_alignment;
        ps_mem_tab[i + NUM_RC_MEM_RECS].e_mem_type = (IV_MEM_TYPE_T)mem_space;
    }
    return (i4_num_memtab + NUM_RC_MEM_RECS);
}

/**
******************************************************************************
*
*  @brief Initilizes the rate control module
*
*  @par   Description
*
*  @param[inout]   ps_mem_tab
*  pointer to memory descriptors table
*
*  @param[in]      ps_init_prms
*  Create time static parameters
*
*  @return      void
*
******************************************************************************
*/
void *ihevce_rc_mem_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_bitrate_instance_id,
    rc_quant_t *ps_rc_quant,
    WORD32 i4_resolution_id,
    WORD32 i4_look_ahead_frames_in_first_pass)
{
    rc_context_t *ps_rc_ctxt;
    WORD32 i4_num_memtab, i, j, i4_avg_bitrate, u4_buf_size;
    WORD32 i4_cdr_period = 0, i4_idr_period = 0;
    WORD32 i4_peak_bitrate_factor;
    rate_control_handle ps_rate_control_api;
    itt_memtab_t as_rc_mem_tab[30];
    itt_memtab_t *ps_itt_memtab = NULL;
    ps_rc_ctxt = (rc_context_t *)ps_mem_tab[RC_CTXT].pv_base;
    memset(ps_rc_ctxt, 0, sizeof(rc_context_t));

    ps_rc_ctxt->i4_br_id_for_2pass = i4_bitrate_instance_id;
    if(ps_init_prms->s_coding_tools_prms.i4_max_cra_open_gop_period)
    {
        i4_cdr_period = ps_init_prms->s_coding_tools_prms.i4_max_cra_open_gop_period;
    }
    if(ps_init_prms->s_coding_tools_prms.i4_max_i_open_gop_period)
    {
        i4_cdr_period = ps_init_prms->s_coding_tools_prms.i4_max_i_open_gop_period;
    }
    i4_idr_period = ps_init_prms->s_coding_tools_prms.i4_max_closed_gop_period;

    ps_rc_quant->pi4_qscale_to_qp = (WORD32 *)ps_mem_tab[RC_QSCALE_TO_QP].pv_base;

    ps_rc_quant->pi4_qp_to_qscale_q_factor = (WORD32 *)ps_mem_tab[RC_QP_TO_QSCALE_Q_FACTOR].pv_base;

    ps_rc_quant->pi4_qp_to_qscale = (WORD32 *)ps_mem_tab[RC_QP_TO_QSCALE].pv_base;

    ps_rc_ctxt->pv_gop_stat = (void *)ps_mem_tab[RC_MULTI_PASS_GOP_STAT].pv_base;

    /*assign memtabs to rc module*/
    i4_num_memtab =
        rate_control_num_fill_use_free_memtab(&ps_rate_control_api, ps_itt_memtab, GET_NUM_MEMTAB);

    i4_num_memtab =
        rate_control_num_fill_use_free_memtab(&ps_rate_control_api, as_rc_mem_tab, FILL_MEMTAB);
    for(i = 0; i < i4_num_memtab; i++)
    {
        as_rc_mem_tab[i].pv_base = ps_mem_tab[i + NUM_RC_MEM_RECS].pv_base;
    }
    i4_num_memtab =
        rate_control_num_fill_use_free_memtab(&ps_rate_control_api, as_rc_mem_tab, USE_BASE);

    ps_rc_ctxt->rc_hdl =
        ps_rate_control_api; /*handle to entire RC structure private to RC library*/
    ps_rc_ctxt->i4_field_pic = ps_init_prms->s_src_prms.i4_field_pic;

    ps_rc_ctxt->i4_is_first_frame_encoded = 0;
    /*added for field encoding*/
    ps_rc_ctxt->i4_max_inter_frm_int =
        1 << (ps_init_prms->s_coding_tools_prms.i4_max_temporal_layers + ps_rc_ctxt->i4_field_pic);
    ps_rc_ctxt->i4_max_temporal_lyr = ps_init_prms->s_coding_tools_prms.i4_max_temporal_layers;
    /*Number of picture types used if different models are used for hierarchial B frames*/

    if(i4_idr_period == 1 || i4_cdr_period == 1)
        ps_rc_ctxt->i4_num_active_pic_type = 1;
    else
        ps_rc_ctxt->i4_num_active_pic_type =
            2 + ps_init_prms->s_coding_tools_prms.i4_max_temporal_layers;

    ps_rc_ctxt->i4_quality_preset =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;

    if(ps_rc_ctxt->i4_quality_preset == IHEVCE_QUALITY_P7)
    {
        ps_rc_ctxt->i4_quality_preset = IHEVCE_QUALITY_P6;
    }

    ps_rc_ctxt->i4_rc_pass = ps_init_prms->s_pass_prms.i4_pass;
    ps_rc_ctxt->i8_num_gop_mem_alloc = 0;

    ps_rc_ctxt->u1_is_mb_level_rc_on = 0; /*no mb level RC*/

    ps_rc_ctxt->i4_is_infinite_gop = 0;
    ps_rc_ctxt->u1_bit_depth = (UWORD8)ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth;

    //ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset = ((ps_init_prms->s_src_prms.i4_bit_depth-8)*6);
    ps_rc_quant->i1_qp_offset = ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth - 8) * 6);

    ps_rc_quant->i2_max_qp = MIN(ps_init_prms->s_config_prms.i4_max_frame_qp,
                                 51);  // FOR Encoder
    ps_rc_quant->i2_min_qp =
        MAX(-(ps_rc_quant->i1_qp_offset), ps_init_prms->s_config_prms.i4_min_frame_qp);

    if(ps_init_prms->s_lap_prms.i4_rc_look_ahead_pics)
    {
        ps_rc_ctxt->i4_num_frame_in_lap_window =
            ps_init_prms->s_lap_prms.i4_rc_look_ahead_pics + MIN_L1_L0_STAGGER_NON_SEQ;
    }
    else
        ps_rc_ctxt->i4_num_frame_in_lap_window = 0;

    if(i4_cdr_period > 0 && i4_idr_period > 0)
    {
        /*both IDR and CDR are positive*/
        //WORD32 i4_rem;
        ps_rc_ctxt->u4_intra_frame_interval = i4_cdr_period;
        ps_rc_ctxt->u4_idr_period = i4_idr_period;

        /*Allow configuration where IDR period is multiple of CDR period. Though any configuiration is supported by LAP rate control
        does not handle assymeteric GOPS, Bit-allocation is exposed to CDR or IDR. It treats everything as I pic*/
    }
    else if(!i4_idr_period && i4_cdr_period > 0)
    {
        ps_rc_ctxt->u4_intra_frame_interval = i4_cdr_period;
        ps_rc_ctxt->u4_idr_period = 0;
    }
    else if(!i4_cdr_period && i4_idr_period > 0)
    {
        ps_rc_ctxt->u4_intra_frame_interval = i4_idr_period;
        ps_rc_ctxt->u4_idr_period = i4_idr_period;
    }
    else
    {
        /*ASSERT(0);*/

        ps_rc_ctxt->u4_intra_frame_interval =
            INFINITE_GOP_CDR_TIME_S *
            ((ps_init_prms->s_src_prms.i4_frm_rate_num /
              (ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_frm_rate_scale_factor *
               ps_init_prms->s_src_prms.i4_frm_rate_denom)));
        ps_rc_ctxt->u4_idr_period = 0;
        ps_rc_ctxt->i4_is_infinite_gop = 1;
    }

    /*If cdr period is 0 then only it is closed gop*/
    ps_rc_ctxt->i4_is_gop_closed = 0;
    if(i4_cdr_period == 0)
    {
        ps_rc_ctxt->i4_is_gop_closed = 1;
    }
    /*This is required because the intra sad returned by non I pic is not correct. Use only I pic sad for next I pic qp calculation*/
    ps_rc_ctxt->i4_use_est_intra_sad = 0;
    ps_rc_ctxt->u4_src_ticks = 1000;
    ps_rc_ctxt->u4_tgt_ticks = 1000;
    ps_rc_ctxt->i4_auto_generate_init_qp = 1;

    ps_rc_ctxt->i8_prev_i_frm_cost = 0;

    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        /* -1 cost indicates the picture type not been encoded*/
        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[i] = -1;
        ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i] = -1;
        ps_rc_ctxt->ai8_prev_frame_hme_sad[i] = -1;
        ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[i] = -1;
        /*L1 state metrics*/
        ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[i] = -1;
        ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_cost[i] = -1;
        ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_sad[i] = -1;
        /* SGI & Enc Loop Parallelism related changes*/
        ps_rc_ctxt->s_l1_state_metric.au4_prev_scene_num[i] = 0;
        ps_rc_ctxt->au4_prev_scene_num_pre_enc[i] = 0xFFFFFFFF;
        ps_rc_ctxt->ai4_qp_for_previous_scene_pre_enc[i] = 0;
    }
    ps_rc_ctxt->u4_scene_num_est_L0_intra_sad_available = 0xFFFFFFFF;

    for(i = 0; i < MAX_NON_REF_B_PICS_IN_QUEUE_SGI; i++)
    {
        ps_rc_ctxt->as_non_ref_b_qp[i].i4_enc_order_num_rc = 0x7FFFFFFF;
        ps_rc_ctxt->as_non_ref_b_qp[i].i4_non_ref_B_pic_qp = 0x7FFFFFFF;
        ps_rc_ctxt->as_non_ref_b_qp[i].u4_scene_num_rc = MAX_SCENE_NUM + 1;
    }
    ps_rc_ctxt->i4_non_ref_B_ctr = 0;
    ps_rc_ctxt->i4_prev_qp_ctr = 0;
    ps_rc_ctxt->i4_cur_scene_num = 0;

    /*init = 0 set to 1 when atleast one frame of each picture type has completed L1 stage*/
    ps_rc_ctxt->i4_is_est_L0_intra_sad_available = 0;

    /*Min and max qp from user*/
    ps_rc_ctxt->i4_min_frame_qp = ps_init_prms->s_config_prms.i4_min_frame_qp;
    ps_rc_ctxt->i4_max_frame_qp = ps_init_prms->s_config_prms.i4_max_frame_qp;
    ASSERT(ps_rc_ctxt->i4_min_frame_qp >= ps_rc_quant->i2_min_qp);
    ASSERT(ps_rc_ctxt->i4_max_frame_qp <= ps_rc_quant->i2_max_qp);
    /*bitrate init*/
    /*take average bitrate from comfig file*/
    i4_avg_bitrate = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                         .ai4_tgt_bitrate[i4_bitrate_instance_id];

    if((ps_init_prms->s_config_prms.i4_rate_control_mode == VBR_STREAMING) &&
       (ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
            .ai4_peak_bitrate[i4_bitrate_instance_id] < (1050 * (i4_avg_bitrate / 1000))))
    {
        ps_init_prms->s_config_prms.i4_rate_control_mode = CBR_NLDRC;
    }

    ps_rc_ctxt->e_rate_control_type = (rc_type_e)ps_init_prms->s_config_prms.i4_rate_control_mode;
    ps_rc_ctxt->i4_capped_vbr_flag = 0;
    if(1 == ps_init_prms->s_config_prms.i4_rate_control_mode)
    {
        /* The path taken by capped vbr mode is same as normal VBR mode. Only a flag needs to be enabled
           which tells the rc module that encoder is running in capped vbr mode */
        ps_rc_ctxt->e_rate_control_type = VBR_STREAMING;
        ps_rc_ctxt->i4_capped_vbr_flag = 1;
    }
    ASSERT(
        (ps_rc_ctxt->e_rate_control_type == CBR_NLDRC) ||
        (ps_rc_ctxt->e_rate_control_type == CONST_QP) ||
        (ps_rc_ctxt->e_rate_control_type == VBR_STREAMING));

    ps_rc_ctxt->u4_avg_bit_rate = i4_avg_bitrate;
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        if(ps_rc_ctxt->e_rate_control_type == VBR_STREAMING)
        {
            ps_rc_ctxt->au4_peak_bit_rate[i] =
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                    .ai4_peak_bitrate[i4_bitrate_instance_id];
        }
        else
        {
            /*peak bitrate parameter is ignored in CBR*/
            ps_rc_ctxt->au4_peak_bit_rate[i] = i4_avg_bitrate;
        }
    }
    ps_rc_ctxt->u4_min_bit_rate = i4_avg_bitrate;

    /*buffer size init*/
    u4_buf_size = (WORD32)(ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                               .ai4_max_vbv_buffer_size[i4_bitrate_instance_id]);
    ps_rc_ctxt->u4_max_delay = (UWORD32)(
        (float)u4_buf_size / i4_avg_bitrate * 1000); /*delay in milli-seconds based on buffer size*/
    ps_rc_ctxt->u4_max_vbv_buff_size = u4_buf_size; /*buffer size should be in bits*/
    /*This dictates the max deviaiton allowed for file size in VBR mode. */
    ps_rc_ctxt->f_vbr_max_peak_sustain_dur =
        ((float)ps_init_prms->s_config_prms.i4_vbr_max_peak_rate_dur) / 1000;
    ps_rc_ctxt->i8_num_frms_to_encode = (WORD32)ps_init_prms->s_config_prms.i4_num_frms_to_encode;
    i4_peak_bitrate_factor = (ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                                  .ai4_peak_bitrate[i4_bitrate_instance_id] /
                              i4_avg_bitrate) *
                             1000;
    {
        //float f_delay = ((float)ps_init_prms->s_config_prms.i4_max_vbv_buffer_size*1000)/i4_peak_bitrate_factor;
        float f_delay = ((float)ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                             .ai4_max_vbv_buffer_size[i4_bitrate_instance_id] *
                         1000) /
                        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                            .ai4_peak_bitrate[i4_bitrate_instance_id];
        ps_rc_ctxt->i4_initial_decoder_delay_frames = (WORD32)(
            ((f_delay) * (ps_init_prms->s_src_prms.i4_frm_rate_num /
                          (ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                               .i4_frm_rate_scale_factor *
                           ps_init_prms->s_src_prms.i4_frm_rate_denom))) /
            1000);
    }
    /*Initial buffer fullness*/
    ps_rc_ctxt->i4_init_vbv_fullness = ps_init_prms->s_config_prms.i4_init_vbv_fullness;

    /*Init Qp updation. This seems to be used for pre enc stage of second frame. Needs to be looked into*/
    ps_rc_ctxt->i4_init_frame_qp_user = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                                            .ai4_frame_qp[i4_bitrate_instance_id];

    for(i = 0; i < MAX_SCENE_NUM; i++)
    {
        for(j = 0; j < MAX_PIC_TYPE; j++)
            ps_rc_ctxt->ai4_prev_pic_hevc_qp[i][j] = INIT_HEVCE_QP_RC;
    }
    memset(&ps_rc_ctxt->ai4_scene_numbers[0], 0, sizeof(ps_rc_ctxt->ai4_scene_numbers));
    memset(&ps_rc_ctxt->ai4_scene_num_last_pic[0], 0, sizeof(ps_rc_ctxt->ai4_scene_num_last_pic));
    ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[0] = ps_rc_ctxt->i4_min_frame_qp - 1;
    ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[1] = ps_rc_ctxt->i4_min_frame_qp - 1;
    /* SGI & Enc Loop Parallelism related changes*/
    for(i = 0; i < MAX_NUM_ENC_LOOP_PARALLEL; i++)
    {
        ps_rc_ctxt->ai8_cur_frm_intra_cost[i] = 0;
        ps_rc_ctxt->ai8_cur_frame_coarse_ME_cost[i] = 0;
        ps_rc_ctxt->ai4_I_model_only_reset[i] = 0;
        ps_rc_ctxt->ai4_is_non_I_scd_pic[i] = 0;
        ps_rc_ctxt->ai4_is_pause_to_resume[i] = 0;
        ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i] = 0;
        ps_rc_ctxt->ai4_is_cmplx_change_reset_bits[i] = 0;
        /*initialize assuming 30 percent intra and 70 percent inter weightage*/
        ps_rc_ctxt->ai4_lap_complexity_q7[i] = MODERATE_LAP2_COMPLEXITY_Q7;

        ps_rc_ctxt->ai4_lap_f_sim[i] = MODERATE_FSIM_VALUE;
    }

    /*Init variables required to handle entropy and rdopt consumption mismatch*/
    ps_rc_ctxt->i4_rdopt_bit_count = 0;
    ps_rc_ctxt->i4_entropy_bit_count = 0;
    for(i = 0; i < NUM_BUF_RDOPT_ENT_CORRECT; i++)
    {
        ps_rc_ctxt->ai4_rdopt_bit_consumption_estimate[i] =
            -1; /*negative bit signifies that value is not populated*/
        ps_rc_ctxt->ai4_rdopt_bit_consumption_buf_id[i] = -1;
        ps_rc_ctxt->ai4_entropy_bit_consumption[i] = -1;
        ps_rc_ctxt->ai4_entropy_bit_consumption_buf_id[i] = -1;
    }

    /** scd model reset related param init*/
    for(i = 0; i < MAX_NUM_TEMPORAL_LAYERS; i++)
    {
        ps_rc_ctxt->au4_scene_num_temp_id[i] = 0;
    }
    /* SGI & Enc Loop Parallelism related changes*/
    for(i = 0; i < MAX_NUM_ENC_LOOP_PARALLEL; i++)
    {
        ps_rc_ctxt->ai4_is_frame_scd[i] = 0;
    }

    /*Stat file pointer passed from applicaition*/
    ps_rc_ctxt->pf_stat_file = NULL;
    ps_rc_ctxt->i8_num_frame_read = 0;

    return ps_rc_ctxt;
}

/*###############################################*/
/******* END OF RC MEM INIT FUNCTIONS **********/
/*###############################################*/

/*###############################################*/
/******* START OF RC INIT FUNCTIONS **************/
/*###############################################*/
/**
******************************************************************************
*
*  @brief Initialises teh Rate control ctxt
*
*  @par   Description
*
*  @param[inout]   pv_ctxt
*  pointer to memory descriptors table
*
*  @param[in]      ps_run_time_src_param
*  Create time static parameters
*
*  @return      void
*
******************************************************************************
*/
void ihevce_rc_init(
    void *pv_ctxt,
    ihevce_src_params_t *ps_run_time_src_param,
    ihevce_tgt_params_t *ps_tgt_params,
    rc_quant_t *ps_rc_quant,
    ihevce_sys_api_t *ps_sys_api,
    ihevce_lap_params_t *ps_lap_prms,
    WORD32 i4_num_frame_parallel)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    WORD32 i, i_temp, j;
    float f_temp;

    /*run time width and height has to considered*/
    ps_rc_ctxt->i4_frame_height = ps_tgt_params->i4_height;
    ps_rc_ctxt->i4_frame_width = ps_tgt_params->i4_width;
    ps_rc_ctxt->i4_field_pic = ps_run_time_src_param->i4_field_pic;
    ps_rc_ctxt->i8_num_bit_alloc_period = 0;
    ps_rc_ctxt->i8_new_bitrate = -1; /*-1 indicates no dynamic change in bitrate request pending*/
    ps_rc_ctxt->i8_new_peak_bitrate = -1;

    ps_rc_ctxt->i4_is_last_frame_scan = 0;

    memset(ps_rc_ctxt->ai4_offsets, 0, 5 * sizeof(WORD32));

    ps_rc_ctxt->i4_complexity_bin = 5;
    ps_rc_ctxt->i4_last_p_or_i_frame_gop = 0;
    ps_rc_ctxt->i4_qp_at_I_frame_for_skip_sad = 1;
    ps_rc_ctxt->i4_denominator_i_to_avg = 1;
    ps_rc_ctxt->i4_fp_bit_alloc_in_sp = 0;

    ps_rc_ctxt->ai4_offsets[0] = 0;
    ps_rc_ctxt->ai4_offsets[1] = 1;
    ps_rc_ctxt->ai4_offsets[2] = 2;
    ps_rc_ctxt->ai4_offsets[3] = 3;
    ps_rc_ctxt->ai4_offsets[4] = 4;

    ps_rc_ctxt->i4_num_frames_subgop = 0;
    ps_rc_ctxt->i8_total_acc_coarse_me_sad = 0;

    ps_rc_ctxt->i4_L0_frame_qp = 1;

    ps_rc_ctxt->i4_est_text_bits_ctr_get_qp = 0;
    ps_rc_ctxt->i4_est_text_bits_ctr_update_qp = 0;

    /*CAllback functions need to be copied for use inside RC*/
    ps_rc_ctxt->ps_sys_rc_api = ps_sys_api;

    f_temp = ((float)(ps_rc_quant->i2_max_qp + ps_rc_quant->i1_qp_offset - 4) / 6);

    ps_rc_quant->i2_max_qscale = (WORD16)((float)pow(2, f_temp) + 0.5) << 3;

    f_temp = ((float)(ps_rc_quant->i2_min_qp + ps_rc_quant->i1_qp_offset - 4) / 6);

    ps_rc_quant->i2_min_qscale = (WORD16)((float)pow(2, f_temp) + 0.5);

    f_temp =
        ((float)(51 + ps_rc_quant->i1_qp_offset - 4) /
         6);  // default MPEG2 to HEVC and HEVC to MPEG2 Qp conversion tables
    i_temp = (WORD16)((float)pow(2, f_temp) + 0.5);

    i_temp = (i_temp << 3);  // Q3 format is mantained for accuarate calc at lower qp

    for(i = 0; i <= i_temp; i++)
    {
        ps_rc_quant->pi4_qscale_to_qp[i] =
            ihevce_rc_get_scaled_hevce_qp_q3(i, ps_rc_ctxt->u1_bit_depth);
    }

    for(i = (0 - ps_rc_quant->i1_qp_offset); i <= 51; i++)
    {
        ps_rc_quant->pi4_qp_to_qscale_q_factor[i + ps_rc_quant->i1_qp_offset] =
            ihevce_rc_get_scaled_mpeg2_qp_q6(
                i + ps_rc_quant->i1_qp_offset, ps_rc_ctxt->u1_bit_depth);
        ps_rc_quant->pi4_qp_to_qscale[i + ps_rc_quant->i1_qp_offset] =
            ((ps_rc_quant->pi4_qp_to_qscale_q_factor[i + ps_rc_quant->i1_qp_offset] +
              (1 << (QSCALE_Q_FAC_3 - 1))) >>
             QSCALE_Q_FAC_3);
    }

    if(ps_rc_quant->i2_min_qscale < 1)
    {
        ps_rc_quant->i2_min_qscale = 1;
    }

    ps_rc_ctxt->ps_rc_quant_ctxt = ps_rc_quant;

    /*Frame rate init*/
    ps_rc_ctxt->u4_max_frame_rate =
        ps_run_time_src_param->i4_frm_rate_num / ps_tgt_params->i4_frm_rate_scale_factor;
    ps_rc_ctxt->i4_top_field_first = ps_run_time_src_param->i4_topfield_first; /**/
    /*min and max qp initialization*/
    if(ps_rc_ctxt->i4_field_pic == 0)
    {
        WORD32 i4_max_qp = 0;

        if(ps_rc_ctxt->u1_bit_depth == 10)
        {
            i4_max_qp = MAX_HEVC_QP_10bit;
        }
        else if(ps_rc_ctxt->u1_bit_depth == 12)
        {
            i4_max_qp = MAX_HEVC_QP_12bit;
        }
        else
        {
            i4_max_qp = MAX_HEVC_QP;
        }

        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            if((ps_rc_ctxt->i4_init_frame_qp_user + (2 * i) +
                ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset) <=
               i4_max_qp)  //BUG_FIX related to init QP allocation
            {
                ps_rc_ctxt->ai4_init_qp[i] = (ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale
                                                  [(ps_rc_ctxt->i4_init_frame_qp_user + (2 * i)) +
                                                   ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset] +
                                              (1 << (QSCALE_Q_FAC_3 - 1))) >>
                                             QSCALE_Q_FAC_3;
            }
            else
            {
                ps_rc_ctxt->ai4_init_qp[i] =
                    (ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale[i4_max_qp] +
                     (1 << (QSCALE_Q_FAC_3 - 1))) >>
                    QSCALE_Q_FAC_3;  // + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset];
            }
            ps_rc_ctxt->ai4_min_max_qp[i * 2] =
                ps_rc_ctxt->ps_rc_quant_ctxt->i2_min_qscale; /*min qp for each picture type*/
            ps_rc_ctxt->ai4_min_max_qp[i * 2 + 1] = ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qscale >>
                                                    QSCALE_Q_FAC_3; /*max qp for each picture type*/
        }
    }
    else
    {
        WORD32 i4_num_pic_types = MAX_PIC_TYPE;
        WORD32 i4_max_qp = 0;

        if(ps_rc_ctxt->u1_bit_depth == 10)
        {
            i4_max_qp = MAX_HEVC_QP_10bit;
        }
        else if(ps_rc_ctxt->u1_bit_depth == 12)
        {
            i4_max_qp = MAX_HEVC_QP_12bit;
        }
        else
        {
            i4_max_qp = MAX_HEVC_QP;
        }

        i4_num_pic_types >>= 1;

        for(i = 0; i < i4_num_pic_types; i++)
        {
            if((ps_rc_ctxt->i4_init_frame_qp_user + (2 * i) +
                ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset) <= i4_max_qp)
            {
                ps_rc_ctxt->ai4_init_qp[i] = (ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale
                                                  [(ps_rc_ctxt->i4_init_frame_qp_user + (2 * i)) +
                                                   ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset] +
                                              (1 << (QSCALE_Q_FAC_3 - 1))) >>
                                             QSCALE_Q_FAC_3;

                if(i != 0)
                    ps_rc_ctxt->ai4_init_qp[i + FIELD_OFFSET] = ps_rc_ctxt->ai4_init_qp[i];
            }
            else
            {
                ps_rc_ctxt->ai4_init_qp[i] =
                    (ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale[i4_max_qp] +
                     (1 << (QSCALE_Q_FAC_3 - 1))) >>
                    QSCALE_Q_FAC_3;  // + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset];

                if(i != 0)
                    ps_rc_ctxt->ai4_init_qp[i + FIELD_OFFSET] = ps_rc_ctxt->ai4_init_qp[i];
            }
            ps_rc_ctxt->ai4_min_max_qp[i * 2] =
                ps_rc_ctxt->ps_rc_quant_ctxt->i2_min_qscale; /*min qp for each picture type*/
            ps_rc_ctxt->ai4_min_max_qp[i * 2 + 1] = ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qscale >>
                                                    QSCALE_Q_FAC_3; /*max qp for each picture type*/
            if(i != 0)
            {
                ps_rc_ctxt->ai4_min_max_qp[(i + FIELD_OFFSET) * 2] =
                    ps_rc_ctxt->ps_rc_quant_ctxt->i2_min_qscale; /*min qp for each picture type*/
                ps_rc_ctxt->ai4_min_max_qp[(i + FIELD_OFFSET) * 2 + 1] =
                    ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qscale; /*max qp for each picture type*/
            }
        }
    }

    for(j = 0; i < MAX_NUM_ENC_LOOP_PARALLEL; i++)
    {
        /*initialise the coeffs to 1 in case lap is not used */
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rc_ctxt->af_sum_weigh[j][i][0] = 1.0;
            ps_rc_ctxt->af_sum_weigh[j][i][1] = 0.0;
            ps_rc_ctxt->af_sum_weigh[j][i][2] = 0.0;
        }
    }

    ps_rc_ctxt->i4_num_frame_parallel = i4_num_frame_parallel;  //ELP_RC
    i4_num_frame_parallel = (i4_num_frame_parallel > 1) ? i4_num_frame_parallel : 0;

    if(ps_rc_ctxt->i4_num_frame_parallel > 1)
    {
        ps_rc_ctxt->i4_pre_enc_rc_delay = MAX_PRE_ENC_RC_DELAY;
    }
    else
    {
        ps_rc_ctxt->i4_pre_enc_rc_delay = MIN_PRE_ENC_RC_DELAY;
    }
    /*Bitrate and resolutioon based scene cut min qp*/
    {
        /*The min qp for scene cut frame is chosen based on bitrate*/
        float i4_bpp = ((float)ps_rc_ctxt->u4_avg_bit_rate / ps_rc_ctxt->u4_max_frame_rate) * 1000 /
                       (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width);
        if(ps_rc_ctxt->u4_intra_frame_interval == 1)
        {
            /*Ultra High resolution)*/
            if((ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) > 5000000)
            {
                if(i4_bpp > 0.24)
                {
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP_VHBR;
                }
                else if(i4_bpp > 0.16)
                    ps_rc_ctxt->i4_min_scd_hevc_qp =
                        SCD_MIN_HEVC_QP_HBR; /*corresponds to bitrate greater than 40mbps for 4k 30p*/
                else
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP;
            }
            else
            {
                if(i4_bpp > 0.32)
                {
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP_VHBR;
                }
                else if(i4_bpp > 0.24)
                    ps_rc_ctxt->i4_min_scd_hevc_qp =
                        SCD_MIN_HEVC_QP_HBR; /*corresponds to bitrate greater than 15mbps for 1080 30p*/
                else
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP;
            }
        }
        else
        {
            /*Ultra High resolution)*/
            if((ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) > 5000000)
            {
                if(i4_bpp > 0.16)
                {
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP_VHBR;
                }
                else if(i4_bpp > 0.08)
                    ps_rc_ctxt->i4_min_scd_hevc_qp =
                        SCD_MIN_HEVC_QP_HBR; /*corresponds to bitrate greater than 20mbps for 4k 30p*/
                else
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP;
            }
            else
            {
                /*Resolution lesser than full HD (including )*/
                if(i4_bpp > 0.24)
                {
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP_VHBR;
                }
                else if(i4_bpp > 0.16)
                    ps_rc_ctxt->i4_min_scd_hevc_qp =
                        SCD_MIN_HEVC_QP_HBR; /*corresponds to bitrate greater than 10mbps for 1080 30p*/
                else
                    ps_rc_ctxt->i4_min_scd_hevc_qp = SCD_MIN_HEVC_QP;
            }
        }
    }

    initialise_rate_control(
        ps_rc_ctxt->rc_hdl,
        ps_rc_ctxt->e_rate_control_type,
        ps_rc_ctxt->u1_is_mb_level_rc_on,  //0,/*disabling MB level RC*/
        ps_rc_ctxt->u4_avg_bit_rate,
        ps_rc_ctxt->au4_peak_bit_rate,
        ps_rc_ctxt->u4_min_bit_rate,
        ps_rc_ctxt->u4_max_frame_rate,
        ps_rc_ctxt->u4_max_delay, /*max delay in milli seconds based on buffer size*/
        ps_rc_ctxt->u4_intra_frame_interval,
        ps_rc_ctxt->u4_idr_period,
        ps_rc_ctxt->ai4_init_qp,
        ps_rc_ctxt->u4_max_vbv_buff_size,
        ps_rc_ctxt->i4_max_inter_frm_int,
        ps_rc_ctxt->i4_is_gop_closed,
        ps_rc_ctxt->ai4_min_max_qp, /*min and max qp to be used for each of picture type*/
        ps_rc_ctxt->i4_use_est_intra_sad,
        ps_rc_ctxt->u4_src_ticks,
        ps_rc_ctxt->u4_tgt_ticks,
        ps_rc_ctxt->i4_frame_height, /*pels in frame considering 420 semi planar format*/
        ps_rc_ctxt->i4_frame_width,
        ps_rc_ctxt->i4_num_active_pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_ctxt->i4_quality_preset,
        ps_rc_ctxt->i4_num_frame_in_lap_window,
        ps_rc_ctxt->i4_initial_decoder_delay_frames,
        ps_rc_ctxt->f_vbr_max_peak_sustain_dur,
        ps_rc_ctxt->i8_num_frms_to_encode,
        ps_rc_ctxt->i4_min_scd_hevc_qp,
        ps_rc_ctxt->u1_bit_depth,
        ps_rc_ctxt->pf_stat_file,
        ps_rc_ctxt->i4_rc_pass,
        ps_rc_ctxt->pv_gop_stat,
        ps_rc_ctxt->i8_num_gop_mem_alloc,
        ps_rc_ctxt->i4_is_infinite_gop,
        sizeof(ihevce_lap_output_params_t),
        sizeof(rc_lap_out_params_t),
        (void *)ps_sys_api,
        ps_rc_ctxt->i4_fp_bit_alloc_in_sp,
        i4_num_frame_parallel,
        ps_rc_ctxt->i4_capped_vbr_flag);

    //ps_rc_ctxt->i4_init_vbv_fullness = 500000;
    rc_init_set_ebf(ps_rc_ctxt->rc_hdl, ps_rc_ctxt->i4_init_vbv_fullness);

    /*get init qp based on ebf for rate control*/
    if(ps_rc_ctxt->e_rate_control_type != CONST_QP)
    {
        WORD32 I_frame_qp, I_frame_mpeg2_qp;
        /*assume moderate fsim*/
        WORD32 i4_fsim_global = MODERATE_FSIM_VALUE;
        I_frame_mpeg2_qp = rc_get_bpp_based_scene_cut_qp(
            ps_rc_ctxt->rc_hdl,
            I_PIC,
            ((3 * ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) >> 1),
            i4_fsim_global,
            ps_rc_ctxt->af_sum_weigh[0],
            1);

        I_frame_qp = ihevce_rc_get_scaled_hevc_qp_from_qs_q3(
            I_frame_mpeg2_qp << QSCALE_Q_FAC_3, ps_rc_ctxt->ps_rc_quant_ctxt);

        I_frame_qp = I_frame_qp + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

        if(I_frame_qp > 44)
            I_frame_qp = 44;

        ps_rc_ctxt->ai4_init_pre_enc_qp[I_PIC] = I_frame_qp;
        ps_rc_ctxt->ai4_init_pre_enc_qp[P_PIC] = I_frame_qp + 1;
        ps_rc_ctxt->ai4_init_pre_enc_qp[B_PIC] = I_frame_qp + 2;
        ps_rc_ctxt->ai4_init_pre_enc_qp[B1_PIC] = I_frame_qp + 3;
        ps_rc_ctxt->ai4_init_pre_enc_qp[B2_PIC] = I_frame_qp + 4;
        /*Bottom fields*/
        ps_rc_ctxt->ai4_init_pre_enc_qp[P1_PIC] = I_frame_qp + 1;
        ps_rc_ctxt->ai4_init_pre_enc_qp[BB_PIC] = I_frame_qp + 2;
        ps_rc_ctxt->ai4_init_pre_enc_qp[B11_PIC] = I_frame_qp + 3;
        ps_rc_ctxt->ai4_init_pre_enc_qp[B22_PIC] = I_frame_qp + 4;

        ps_rc_ctxt->i4_pre_enc_qp_read_index = 0;
        ps_rc_ctxt->i4_pre_enc_qp_write_index = ps_rc_ctxt->i4_pre_enc_rc_delay - 1;
        for(i = 0; i < ps_rc_ctxt->i4_pre_enc_rc_delay; i++)
        {
            /*initialize it to -1 to indicate it as not produced*/
            ps_rc_ctxt->as_pre_enc_qp_queue[i].i4_is_qp_valid = -1;
        }
        for(i = 0; i < (ps_rc_ctxt->i4_pre_enc_qp_write_index); i++)
        {
            WORD32 j;
            ps_rc_ctxt->as_pre_enc_qp_queue[i].i4_is_qp_valid = 1;
            for(j = 0; j < MAX_PIC_TYPE; j++)
            {
                ps_rc_ctxt->as_pre_enc_qp_queue[i].ai4_quant[j] =
                    ps_rc_ctxt->ai4_init_pre_enc_qp[j];
                ps_rc_ctxt->as_pre_enc_qp_queue[i].i4_scd_qp =
                    ps_rc_ctxt->ai4_init_pre_enc_qp[I_PIC];
            }
        }

        ps_rc_ctxt->i4_use_qp_offset_pre_enc = 1;
        ps_rc_ctxt->i4_num_frms_from_reset = 0;
        /* SGI & Enc Loop Parallelism related changes*/
        ps_rc_ctxt->u4_prev_scene_num = 0;
        //ps_rc_ctxt->i4_use_init_qp_for_pre_enc = 0;
        for(j = 0; j < MAX_NON_REF_B_PICS_IN_QUEUE_SGI; j++)
        {
            ps_rc_ctxt->au4_prev_scene_num_multi_scene[j] = 0x3FFFFFFF;
            for(i = 0; i < MAX_PIC_TYPE; i++)
            {
                ps_rc_ctxt->ai4_qp_for_previous_scene_multi_scene[j][i] =
                    ps_rc_ctxt->ai4_init_pre_enc_qp[i];
            }
        }

        /* SGI & Enc Loop Parallelism related changes*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rc_ctxt->ai4_qp_for_previous_scene[i] = ps_rc_ctxt->ai4_init_pre_enc_qp[i];
        }
    }
    else
    {
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rc_ctxt->ai4_init_pre_enc_qp[i] = ps_rc_ctxt->i4_init_frame_qp_user;
            ps_rc_ctxt->ai4_qp_for_previous_scene[i] = ps_rc_ctxt->i4_init_frame_qp_user;
        }
    }
}

/**
******************************************************************************
*
*  @brief Populate common params from lap_out structure to rc_lap_out structure
*         Also the init of some rc_lap_out params done here
*  @par   Description
*
*  @param[in]   ps_lap_out
*  pointer to lap_out structure
*
*  @param[out]      ps_rc_lap_out
*  pointer to rc_lap_out structure
*
*  @return      void
*
******************************************************************************
*/

void ihevce_rc_populate_common_params(
    ihevce_lap_output_params_t *ps_lap_out, rc_lap_out_params_t *ps_rc_lap_out)
{
    /* Update common params */

    ps_rc_lap_out->i4_rc_pic_type = ps_lap_out->i4_pic_type;
    ps_rc_lap_out->i4_rc_poc = ps_lap_out->i4_poc;
    ps_rc_lap_out->i4_rc_temporal_lyr_id = ps_lap_out->i4_temporal_lyr_id;
    ps_rc_lap_out->i4_rc_is_ref_pic = ps_lap_out->i4_is_ref_pic;
    ps_rc_lap_out->i4_rc_scene_type = ps_lap_out->i4_scene_type;
    ps_rc_lap_out->u4_rc_scene_num = ps_lap_out->u4_scene_num;
    ps_rc_lap_out->i4_rc_display_num = ps_lap_out->i4_display_num;
    ps_rc_lap_out->i4_rc_quality_preset = ps_lap_out->i4_quality_preset;
    ps_rc_lap_out->i4_rc_first_field = ps_lap_out->i4_first_field;

    /*params populated in LAP-2*/
    ps_rc_lap_out->i8_frame_acc_coarse_me_cost = -1;
    memset(ps_rc_lap_out->ai8_frame_acc_coarse_me_sad, -1, sizeof(WORD32) * 52);

    ps_rc_lap_out->i8_pre_intra_satd = -1;

    ps_rc_lap_out->i8_raw_pre_intra_sad = -1;

    ps_rc_lap_out->i8_raw_l1_coarse_me_sad = -1;

    ps_rc_lap_out->i4_is_rc_model_needs_to_be_updated = 1;
    /* SGI & Enc Loop Parallelism related changes*/
    ps_rc_lap_out->i4_ignore_for_rc_update = 0;

    /*For 1 pass HQ I frames*/

    ps_rc_lap_out->i4_complexity_bin = 5;
    {
        WORD32 ai4_offsets[5] = { 0, 1, 2, 3, 4 };
        memmove(ps_rc_lap_out->ai4_offsets, ai4_offsets, sizeof(WORD32) * 5);
        ps_rc_lap_out->i4_offsets_set_flag = -1;
    }

    ps_rc_lap_out->i4_L1_qp = -1;
    ps_rc_lap_out->i4_L0_qp = -1;
}

/*###############################################*/
/******* END OF RC INIT FUNCTIONS **************/
/*###############################################*/

/*#########################################################*/
/******* START OF PRE-ENC QP QUERY FUNCTIONS **************/
/*#######################################################*/

/**
******************************************************************************
*
*  @name  ihevce_rc_get_bpp_based_frame_qp
*
*  @par   Description
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*               ps_rc_lap_out
*  @return      frame qp
*
******************************************************************************
*/
WORD32 ihevce_rc_get_bpp_based_frame_qp(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    WORD32 i4_frame_qs_q3, i4_hevc_frame_qp, i;
    frame_info_t *ps_frame_info;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);
    /*initialise the coeffs to 1 in case lap is not used */
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_rc_ctxt->af_sum_weigh[0][i][0] = 1.0;
        ps_rc_ctxt->af_sum_weigh[0][i][1] = 0.0;
        ps_rc_ctxt->af_sum_weigh[0][i][2] = 0.0;
    }
    {
        /*scene cut handling during pre-enc stage*/
        /*assume lap fsim as 117. not used since ratio is direclt sent*/
        if(ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
        {
            for(i = 0; i < MAX_PIC_TYPE; i++)
            {
                ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i] = -1;
                ps_rc_ctxt->ai8_prev_frame_hme_sad[i] = -1;
                ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[i] = -1;
            }
            ps_rc_ctxt->i4_is_est_L0_intra_sad_available = 0;
        }

        if(ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT ||
           !ps_rc_ctxt->i4_is_est_L0_intra_sad_available)
        {
            /*compute bpp based qp if current frame is scene cut or data is not sufficient*/
            i4_frame_qs_q3 = rc_get_bpp_based_scene_cut_qp(
                ps_rc_ctxt->rc_hdl,
                I_PIC,
                ((3 * ps_rc_lap_out->i4_num_pels_in_frame_considered) >> 1),
                117,
                ps_rc_ctxt->af_sum_weigh[0],
                0);
            i4_frame_qs_q3 = i4_frame_qs_q3 << QSCALE_Q_FAC_3;
        }
        else
        {
            /*using previous one sub-gop data calculate i to rest ratio and qp assuming it is I frame*/
            WORD32 i4_num_b, i, ai4_pic_dist[MAX_PIC_TYPE], index, i4_total_bits;
            LWORD64 i8_average_pre_intra_sad = 0, i8_average_est_l0_satd_by_act = 0;
            double lambda_modifier[MAX_PIC_TYPE], complexity[MAX_PIC_TYPE], den = 0.0f,
                                                                            i_to_rest_bit_ratio;
            WORD32 i4_curr_bits_estimated = 0;

            for(i = 0; i < MAX_PIC_TYPE; i++)
            {
                complexity[i] = 0;
                lambda_modifier[i] = 0;
                ai4_pic_dist[i] = 0;
            }

            index = ihevce_get_offline_index(
                ps_rc_ctxt, ps_rc_lap_out->i4_num_pels_in_frame_considered);
            if(ps_rc_ctxt->i4_max_temporal_lyr)
            {
                i4_num_b = ((WORD32)pow((float)2, ps_rc_ctxt->i4_max_temporal_lyr)) - 1;
            }
            else
            {
                i4_num_b = 0;
            }

            lambda_modifier[I_PIC] =
                ihevce_get_frame_lambda_modifier((WORD8)I_PIC, 0, 1, 1, i4_num_b);
            lambda_modifier[P_PIC] =
                ihevce_get_frame_lambda_modifier((WORD8)P_PIC, 0, 1, 1, i4_num_b) *
                pow((float)1.125, 1);
            lambda_modifier[B_PIC] =
                ihevce_get_frame_lambda_modifier(
                    (WORD8)B_PIC, 1, (ps_rc_ctxt->i4_max_temporal_lyr > 1), 1, i4_num_b) *
                pow((float)1.125, 2);
            lambda_modifier[B1_PIC] =
                ihevce_get_frame_lambda_modifier(
                    (WORD8)B1_PIC, 2, 1, (ps_rc_ctxt->i4_max_temporal_lyr > 2), i4_num_b) *
                pow((float)1.125, 3);
            lambda_modifier[B2_PIC] =
                ihevce_get_frame_lambda_modifier((WORD8)B2_PIC, 3, 1, 0, i4_num_b) *
                pow((float)1.125, 4);

            /*consider average of one sub-gop for intra sad*/

            if(ps_rc_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6)
            {
                for(i = 0; i < 2; i++)
                {
                    i8_average_pre_intra_sad += ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[i];
                    i8_average_est_l0_satd_by_act += ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i];
                    if(ps_rc_ctxt->i4_field_pic == 1 && i != 0)
                    {
                        i8_average_pre_intra_sad +=
                            ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[i + FIELD_OFFSET];
                        i8_average_est_l0_satd_by_act +=
                            ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i + FIELD_OFFSET];
                    }
                }
                if(ps_rc_ctxt->i4_field_pic == 1)
                {
                    i8_average_pre_intra_sad /= 3;
                    i8_average_est_l0_satd_by_act /= 3;
                }
                else
                {
                    i8_average_pre_intra_sad <<= 1;
                    i8_average_est_l0_satd_by_act <<= 1;
                }
            }
            else
            {
                for(i = 0; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
                {
                    i8_average_pre_intra_sad += ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[i];
                    i8_average_est_l0_satd_by_act += ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i];
                    if(ps_rc_ctxt->i4_field_pic == 1 && i != 0)
                    {
                        i8_average_pre_intra_sad +=
                            ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[i + FIELD_OFFSET];
                        i8_average_est_l0_satd_by_act +=
                            ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i + FIELD_OFFSET];
                    }
                }
                if(ps_rc_ctxt->i4_field_pic == 1)
                {
                    i8_average_pre_intra_sad /= ((i << 1) - 1);
                    i8_average_est_l0_satd_by_act /= ((i << 1) - 1);
                }
                else
                {
                    i8_average_pre_intra_sad /= i;
                    i8_average_est_l0_satd_by_act /= i;
                }
            }

            /*no lambda modifier is considered for I pic as other lambda are scaled according to I frame lambda*/
            complexity[I_PIC] = (double)i8_average_pre_intra_sad;

            for(i = 1; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
            {
#if !USE_SQRT
                complexity[i] = ps_rc_ctxt->ai8_prev_frame_hme_sad[i] / pow(1.125, i);

                if(ps_rc_ctxt->i4_field_pic == 1)
                {
                    complexity[i + FIELD_OFFSET] =
                        ps_rc_ctxt->ai8_prev_frame_hme_sad[i + FIELD_OFFSET] / pow(1.125, i);
                }
#else
                complexity[i] = ps_rc_ctxt->ai8_prev_frame_hme_sad[i] /
                                (sqrt(lambda_modifier[i] / lambda_modifier[I_PIC]) * pow(1.125, i));
#endif
            }
            /*get picture type distribution in LAP*/
            rc_get_pic_distribution(ps_rc_ctxt->rc_hdl, &ai4_pic_dist[0]);

            for(i = 0; i < MAX_PIC_TYPE; i++)
            {
                den += complexity[i] * ai4_pic_dist[i];
            }
            /*subtract I frame complexity to get I to rest ratio*/
            {
                WORD32 num_inter_pic = 0;
                for(i = 1; i < MAX_PIC_TYPE; i++)
                {
                    num_inter_pic += ai4_pic_dist[i];
                }
                if(num_inter_pic > 0)
                    den = (den - (complexity[I_PIC] * ai4_pic_dist[I_PIC])) / num_inter_pic;
                else
                    den = complexity[I_PIC];
            }

            if(den > 0)
                i_to_rest_bit_ratio = (float)((complexity[I_PIC]) / den);
            else
                i_to_rest_bit_ratio = 15;

            /*get qp for scene cut frame based on offline data*/
            i4_frame_qs_q3 = rc_get_qp_for_scd_frame(
                ps_rc_ctxt->rc_hdl,
                I_PIC,
                i8_average_est_l0_satd_by_act,
                ps_rc_lap_out->i4_num_pels_in_frame_considered,
                -1,
                MODERATE_FSIM_VALUE,
                (void *)&g_offline_i_model_coeff[index][0],
                (float)i_to_rest_bit_ratio,
                0,
                ps_rc_ctxt->af_sum_weigh[0],
                ps_rc_lap_out->ps_frame_info,
                ps_rc_ctxt->i4_rc_pass,
                0,
                0,
                0,
                &i4_total_bits,
                &i4_curr_bits_estimated,
                ps_rc_lap_out->i4_use_offline_model_2pass,
                0,
                0,
                -1,
                NULL);
        }

        i4_hevc_frame_qp =
            ihevce_rc_get_scaled_hevc_qp_from_qs_q3(i4_frame_qs_q3, ps_rc_ctxt->ps_rc_quant_ctxt);

        i4_hevc_frame_qp = i4_hevc_frame_qp + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

        if(i4_hevc_frame_qp > ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp)
            i4_hevc_frame_qp = ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp;

        /*offset depending on current picture type*/
        if(rc_pic_type != I_PIC)
            i4_hevc_frame_qp += ps_rc_lap_out->i4_rc_temporal_lyr_id + 1;
        /*clip min and max qp to be within range*/
        i4_hevc_frame_qp = ihevce_clip_min_max_qp(
            ps_rc_ctxt, i4_hevc_frame_qp, rc_pic_type, ps_rc_lap_out->i4_rc_temporal_lyr_id);

        ps_rc_ctxt->ai4_qp_for_previous_scene_pre_enc[rc_pic_type] = i4_hevc_frame_qp;
        ps_rc_ctxt->au4_prev_scene_num_pre_enc[rc_pic_type] = ps_rc_lap_out->u4_rc_scene_num;
    }

    return i4_hevc_frame_qp;
}
/**
******************************************************************************
*
*  @name  ihevce_rc_get_pre_enc_pic_quant
*
*  @par   Description - Called from ihevce_rc_cal_pre_enc_qp. updates frame qp
*                       which will be used by next frame of same pic type in
*                       pre-enc stage
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*  @return      void
*
******************************************************************************
*/
WORD32
    ihevce_rc_get_pre_enc_pic_quant(void *pv_ctxt, picture_type_e rc_pic_type, WORD32 *pi4_scd_qp)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    WORD32 i4_frame_qp, i4_frame_qp_q6, i4_hevc_frame_qp = -1;
    WORD32 i4_max_frame_bits = (1 << 30);
    WORD32 i4_temporal_layer_id, i4_is_bottom_field, i4_cur_est_texture_bits;

    ihevce_rc_get_pic_param(rc_pic_type, &i4_temporal_layer_id, &i4_is_bottom_field);

    {
        WORD32 is_scd_ref_frame = 0, i4_num_scd_in_lap_window = 0, num_frames_b4_scd = 0;

        /*treat even first frame as scd frame*/
        if(!ps_rc_ctxt->i4_is_first_frame_encoded)
        {
            is_scd_ref_frame = 1;
        }

        {
            /*Only I frames are considered as scd pic during pre-enc*/
            is_scd_ref_frame &= (rc_pic_type == I_PIC);
        }

        rc_set_num_scd_in_lap_window(
            ps_rc_ctxt->rc_hdl, i4_num_scd_in_lap_window, num_frames_b4_scd);

        /** Pre-enc thread as of now SCD handling is not present */
        //if(!(is_scd_ref_frame || ps_rc_ctxt->i4_is_pause_to_resume) || call_type == PRE_ENC_GET_QP)
        {
            WORD32 i4_is_first_frame_coded;
            /*Once first frame has been encoded use prev frame intra satd and cur frame satd to alter est intra sad for cur frame*/
            i4_is_first_frame_coded = is_first_frame_coded(ps_rc_ctxt->rc_hdl);
            {
                int i;
                WORD32 i4_curr_bits_estimated, i4_is_model_valid;
                /*initialise the coeffs to 1 and 0in case lap is not used */
                for(i = 0; i < MAX_PIC_TYPE; i++)
                {
                    ps_rc_ctxt->af_sum_weigh[0][i][0] = 1.0;
                    ps_rc_ctxt->af_sum_weigh[0][i][1] = 0.0;
                }

                i4_frame_qp_q6 = get_frame_level_qp(
                    ps_rc_ctxt->rc_hdl,
                    rc_pic_type,
                    i4_max_frame_bits,
                    &i4_cur_est_texture_bits,  //this value is returned by rc
                    ps_rc_ctxt->af_sum_weigh[0],
                    0,
                    8.0f,
                    NULL,
                    ps_rc_ctxt->i4_complexity_bin,
                    ps_rc_ctxt->i4_scene_num_latest, /*no pause resume concept*/
                    &i4_curr_bits_estimated,
                    &i4_is_model_valid,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL);

                /** The usage of global table will truncate the input given as qp format and hence will not return very low qp values desirable at very
                low bitrate. Hence on the fly calculation is enabled*/
                i4_hevc_frame_qp =
                    ihevce_rc_get_scaled_hevce_qp_q6(i4_frame_qp_q6, ps_rc_ctxt->u1_bit_depth);

                if(rc_pic_type == I_PIC)
                {
                    /*scene cut handling during pre-enc stage*/
                    i4_frame_qp = rc_get_bpp_based_scene_cut_qp(
                        ps_rc_ctxt->rc_hdl,
                        rc_pic_type,
                        ((3 * ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) >> 1),
                        ps_rc_ctxt->ai4_lap_f_sim[0],
                        ps_rc_ctxt->af_sum_weigh[0],
                        0);

                    *pi4_scd_qp = ihevce_rc_get_scaled_hevc_qp_from_qs_q3(
                        i4_frame_qp << QSCALE_Q_FAC_3, ps_rc_ctxt->ps_rc_quant_ctxt);
                    *pi4_scd_qp = *pi4_scd_qp + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset;
                    if(*pi4_scd_qp > ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp)
                        *pi4_scd_qp = ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp;
                }
                else
                {
                    /*scene cut qp is only valid when queried for I_PIC*/
                    *pi4_scd_qp = i4_hevc_frame_qp;
                }
            }
        }

        ASSERT(i4_hevc_frame_qp >= (-ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset));

        /*constraint qp swing based on neighbour frames*/
        if(is_first_frame_coded(ps_rc_ctxt->rc_hdl))
        {
            if(ps_rc_ctxt->i4_field_pic == 0)
            {
                if((rc_pic_type != I_PIC && rc_pic_type != P_PIC) &&
                   i4_hevc_frame_qp >
                       ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                       [rc_pic_type - 1] +
                           3)
                {
                    /*allow max of +3 compared to previous frame*/
                    i4_hevc_frame_qp =
                        ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                        [rc_pic_type - 1] +
                        3;
                }
                if((rc_pic_type != I_PIC && rc_pic_type != P_PIC) &&
                   i4_hevc_frame_qp <
                       (ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                        [rc_pic_type - 1]))
                {
                    i4_hevc_frame_qp =
                        ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                        [rc_pic_type - 1];
                }

                /** Force non-ref B pic qp to be ref_B_PIC_qp - 1. This is not valid for when max teporla later is less than 2*/
                if(i4_temporal_layer_id == ps_rc_ctxt->i4_max_temporal_lyr &&
                   ps_rc_ctxt->i4_max_temporal_lyr > 1)
                {
                    i4_hevc_frame_qp =
                        ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                        [rc_pic_type - 1] +
                        1;
                }
            }
            else /*for field case*/
            {
                if(i4_temporal_layer_id >= 1)
                {
                    /*To make the comparison of qp with the top field's of previous layer tempor layer id matches with the pic type. */
                    if(i4_hevc_frame_qp >
                       ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                       [i4_temporal_layer_id] +
                           3)
                    {
                        /*allow max of +3 compared to previous frame*/
                        i4_hevc_frame_qp =
                            ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                            [i4_temporal_layer_id] +
                            3;
                    }
                    if(i4_hevc_frame_qp <
                       ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                       [i4_temporal_layer_id])
                    {
                        i4_hevc_frame_qp =
                            ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                            [i4_temporal_layer_id];
                    }
                    /** Force non-ref B pic qp to be ref_B_PIC_qp - 1. This is not valid for when max teporla later is less than 2*/
                    if(i4_temporal_layer_id == ps_rc_ctxt->i4_max_temporal_lyr &&
                       ps_rc_ctxt->i4_max_temporal_lyr > 1)
                    {
                        i4_hevc_frame_qp =
                            ps_rc_ctxt->ai4_prev_pic_hevc_qp[ps_rc_ctxt->i4_scene_num_latest]
                                                            [i4_temporal_layer_id] +
                            1;
                    }
                }
            }
        }

#if USE_USER_FIRST_FRAME_QP
        /*I_PIC check is necessary coz pre-enc can query for qp even before first frame update has happened*/
        if(!ps_rc_ctxt->i4_is_first_frame_encoded && rc_pic_type == I_PIC)
        {
            i4_hevc_frame_qp = ps_rc_ctxt->i4_init_frame_qp_user;
            DBG_PRINTF("FIXED START QP PATH *************************\n");
        }
#endif
        /**clip to min qp which is user configurable*/
        i4_hevc_frame_qp =
            ihevce_clip_min_max_qp(ps_rc_ctxt, i4_hevc_frame_qp, rc_pic_type, i4_temporal_layer_id);

        return i4_hevc_frame_qp;
    }
}
/**
******************************************************************************
*
*  @name  ihevce_rc_cal_pre_enc_qp
*
*  @par   Description - Called from enc_loop_init. updates frame qp which will
                        be used by next frame of same pic type in pre-enc stage
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*  @return      void
*
******************************************************************************
*/
void ihevce_rc_cal_pre_enc_qp(void *pv_rc_ctxt)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    WORD32 i, i4_frame_qp, i4_scd_qp;
    WORD32 i4_delay_l0_enc = 0;

    i4_delay_l0_enc = ps_rc_ctxt->i4_pre_enc_rc_delay;

    if(ps_rc_ctxt->e_rate_control_type != CONST_QP)
    {
        //DBG_PRINTF("\ncheck query read = %d write = %d",ps_rc_ctxt->i4_pre_enc_qp_read_index,ps_rc_ctxt->i4_pre_enc_qp_write_index);
#if DETERMINISTIC_RC
        ASSERT(
            ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_write_index].i4_is_qp_valid ==
            -1);
#endif
        for(i = 0; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
        {
            i4_frame_qp =
                ihevce_rc_get_pre_enc_pic_quant(ps_rc_ctxt, (picture_type_e)i, &i4_scd_qp);

            ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_write_index].ai4_quant[i] =
                i4_frame_qp;
            /*returns valid scene cut qp only when queried as I_PIC*/
            if(i == 0)
            {
                ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_write_index].i4_scd_qp =
                    i4_scd_qp;
            }

            if(ps_rc_ctxt->i4_field_pic && i > 0)
            {
                i4_frame_qp = ihevce_rc_get_pre_enc_pic_quant(
                    ps_rc_ctxt, (picture_type_e)(i + FIELD_OFFSET), &i4_scd_qp);

                ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_write_index]
                    .ai4_quant[i + FIELD_OFFSET] = i4_frame_qp;
            }
        }
        /*mark index as populated*/
        ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_write_index].i4_is_qp_valid = 1;

        ps_rc_ctxt->i4_pre_enc_qp_write_index =
            (ps_rc_ctxt->i4_pre_enc_qp_write_index + 1) % i4_delay_l0_enc;
    }
}
/**
******************************************************************************
*
*  @brief function to get updated qp after L1 analysis for L0. '
*           This uses estimated L0 satd based on L1 satd/act
*
*  @par   Description
*
*  @param[in]   pv_rc_ctxt
*               void pointer to rc ctxt
*  @param[in]   rc_lap_out_params_t *
pointer to lap out structure
*   @param[in]  i8_est_L0_satd_act
*               estimated L0 satd/act based on L1 satd/act
*  @return      void
*
******************************************************************************
*/
WORD32 ihevce_get_L0_est_satd_based_scd_qp(
    void *pv_rc_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    LWORD64 i8_est_L0_satd_act,
    float i_to_avg_rest_ratio)
{
    rc_context_t *ps_ctxt = (rc_context_t *)pv_rc_ctxt;
    WORD32 i4_frame_qs_q3, i4_hevc_qp, i4_est_header_bits, index, i, i4_total_bits;
    picture_type_e rc_pic_type;

    rc_pic_type = ihevce_rc_conv_pic_type(
        (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type,
        ps_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_ctxt->i4_top_field_first);

    /*initialise the coeffs to 1 in case lap is not used */

    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_ctxt->af_sum_weigh[0][i][0] = 1.0;
        ps_ctxt->af_sum_weigh[0][i][1] = 0.0;
    }

    /*get bits to find estimate of header bits*/
    i4_est_header_bits = rc_get_scene_change_est_header_bits(
        ps_ctxt->rc_hdl,
        ps_rc_lap_out->i4_num_pels_in_frame_considered,
        ps_ctxt->ai4_lap_f_sim[0],
        ps_ctxt->af_sum_weigh[0],
        i_to_avg_rest_ratio);

    index = ihevce_get_offline_index(ps_ctxt, ps_rc_lap_out->i4_num_pels_in_frame_considered);
    {
        WORD32 i4_true_scd = 0;
        WORD32 i4_curr_bits_estimated;

        i4_frame_qs_q3 = rc_get_qp_for_scd_frame(
            ps_ctxt->rc_hdl,
            I_PIC,
            i8_est_L0_satd_act,
            ps_rc_lap_out->i4_num_pels_in_frame_considered,
            i4_est_header_bits,
            ps_ctxt->ai4_lap_f_sim[0],
            (void *)&g_offline_i_model_coeff[index][0],
            i_to_avg_rest_ratio,
            i4_true_scd,
            ps_ctxt->af_sum_weigh[0],
            ps_rc_lap_out->ps_frame_info,
            ps_ctxt->i4_rc_pass,
            0,
            0,
            0,
            &i4_total_bits,
            &i4_curr_bits_estimated,
            ps_rc_lap_out->i4_use_offline_model_2pass,
            0,
            0,
            -1,
            NULL);
    }
    i4_hevc_qp = ihevce_rc_get_scaled_hevc_qp_from_qs_q3(i4_frame_qs_q3, ps_ctxt->ps_rc_quant_ctxt);
    i4_hevc_qp = i4_hevc_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

    if(i4_hevc_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qp)
        i4_hevc_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qp;

    if(i4_hevc_qp < (SCD_MIN_HEVC_QP -
                     ps_ctxt->ps_rc_quant_ctxt
                         ->i1_qp_offset))  // since outside RC the QP range is -12 to 51 for 10 bit
    {
        i4_hevc_qp = (SCD_MIN_HEVC_QP - ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset);
    }
    else if(i4_hevc_qp > SCD_MAX_HEVC_QP)
    {
        i4_hevc_qp = SCD_MAX_HEVC_QP;
    }
    /*this is done outside loop*/

    return i4_hevc_qp;
}
/**
******************************************************************************
*
*  @name  ihevce_rc_pre_enc_qp_query
*
*  @par   Description - Called from pre enc thrd for getting the qp of non scd
                        frames. updates frame qp from reverse queue from enc loop
                        when its available
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*  @param[in]   i4_update_delay : The Delay in the update. This can happen for dist. case!
*               All decision should consider this delay for updation!
*
*  @return      void
*
******************************************************************************
*/

WORD32 ihevce_rc_pre_enc_qp_query(
    void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out, WORD32 i4_update_delay)
{
    WORD32 scene_type, i4_is_scd = 0, i4_frame_qp, slice_type;
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    rc_type_e e_rc_type = ps_rc_ctxt->e_rate_control_type;
    IV_PICTURE_CODING_TYPE_T pic_type = (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);
    WORD32 i4_use_offset_flag = 0, k = 0;
    WORD32 i4_inter_frame_interval = rc_get_inter_frame_interval(ps_rc_ctxt->rc_hdl);
    WORD32 ai4_offsets[5] = { 0, 1, 2, 3, 4 };
    rc_lap_out_params_t *ps_rc_lap_out_temp = ps_rc_lap_out;

    /* The window for which your update is guaranteed */
    WORD32 updated_window = ps_rc_ctxt->i4_num_frame_in_lap_window - i4_update_delay;

    k = 0;
    if((updated_window >= i4_inter_frame_interval) && (ps_rc_ctxt->i4_rc_pass != 2) &&
       ((rc_pic_type == I_PIC) || (rc_pic_type == P_PIC)))
    {
        WORD32 i4_count = 0;

        for(i4_count = 0; i4_count < updated_window; i4_count++)
        {
            picture_type_e rc_pic_type_temp = ihevce_rc_conv_pic_type(
                (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out_temp->i4_rc_pic_type,
                ps_rc_ctxt->i4_field_pic,
                ps_rc_lap_out_temp->i4_rc_temporal_lyr_id,
                ps_rc_lap_out_temp->i4_is_bottom_field,
                ps_rc_ctxt->i4_top_field_first);

            if((rc_pic_type_temp == I_PIC) || (rc_pic_type_temp == P_PIC))
                ihevce_compute_temporal_complexity_reset_Kp_Kb(ps_rc_lap_out_temp, pv_rc_ctxt, 0);

            ps_rc_lap_out_temp =
                (rc_lap_out_params_t *)ps_rc_lap_out_temp->ps_rc_lap_out_next_encode;

            if(ps_rc_lap_out_temp == NULL)
                break;
        }
    }

    if(updated_window >= i4_inter_frame_interval)
    {
        i4_use_offset_flag = 1;
        memmove(ai4_offsets, ps_rc_lap_out->ai4_offsets, sizeof(WORD32) * 5);
    }

    if(CONST_QP == e_rc_type)
    {
        switch(pic_type)
        {
        case IV_I_FRAME:
        case IV_IDR_FRAME:
        {
            slice_type = ISLICE;
            break;
        }
        case IV_P_FRAME:
        {
            slice_type = PSLICE;
            break;
        }
        case IV_B_FRAME:
        {
            slice_type = BSLICE;
            break;
        }
        }

        i4_frame_qp = ihevce_get_cur_frame_qp(
            ps_rc_ctxt->i4_init_frame_qp_user,
            slice_type,
            ps_rc_lap_out->i4_rc_temporal_lyr_id,
            ps_rc_ctxt->i4_min_frame_qp,
            ps_rc_ctxt->i4_max_frame_qp,
            ps_rc_ctxt->ps_rc_quant_ctxt);

        return i4_frame_qp;
    }
    else
    {
        /*check scene type*/
        scene_type = ihevce_rc_lap_get_scene_type(ps_rc_lap_out);

        if(scene_type == SCENE_TYPE_SCENE_CUT)
        {
            i4_is_scd = 1;
            ps_rc_ctxt->i4_num_frms_from_reset = 0;
#if USE_QP_OFFSET_POST_SCD
            ps_rc_ctxt->i4_use_qp_offset_pre_enc = 1;
#else
            ps_rc_ctxt->i4_use_qp_offset_pre_enc = 0;
#endif
        }
        ASSERT(
            ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_read_index].i4_is_qp_valid ==
                1 ||
            ps_rc_lap_out->i4_rc_poc < 20);

        if(ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_read_index].i4_is_qp_valid ==
           1)
        {
            if(i4_is_scd || ps_rc_ctxt->i4_use_qp_offset_pre_enc)
            {
#if 1  //The qp will be populated assuming the frame is I_PIC. Adjust according to current pic type
                i4_frame_qp =
                    ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_read_index].i4_scd_qp;
                if(rc_pic_type == P_PIC)
                    i4_frame_qp++;
                else
                    i4_frame_qp = i4_frame_qp + ps_rc_lap_out->i4_rc_temporal_lyr_id;
#endif
                if(i4_use_offset_flag)
                {
                    if(rc_pic_type > B2_PIC)
                        i4_frame_qp = ps_rc_ctxt->i4_L0_frame_qp + ai4_offsets[rc_pic_type - 4];
                    else
                        i4_frame_qp = ps_rc_ctxt->i4_L0_frame_qp + ai4_offsets[rc_pic_type];
                }
            }
            else
            {
#if DETERMINISTIC_RC
                i4_frame_qp = ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_read_index]
                                  .ai4_quant[rc_pic_type];
#else
                /*read the latest qp updated by enc*/
                i4_frame_qp =
                    ps_rc_ctxt
                        ->as_pre_enc_qp_queue
                            [(ps_rc_ctxt->i4_pre_enc_qp_write_index + MAX_PRE_ENC_RC_DELAY - 1) %
                             MAX_PRE_ENC_RC_DELAY]
                        .ai4_quant[rc_pic_type];
#endif
            }

            ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_read_index].i4_is_qp_valid =
                -1;
            /*once encoder starts reading from qp queue it should always read from qp queue*/
            //ps_rc_ctxt->i4_use_init_qp_for_pre_enc = 0;
        }
        else
        {
            i4_frame_qp = ps_rc_ctxt->ai4_init_pre_enc_qp[rc_pic_type];
        }
        {
            WORD32 i4_delay_l0_enc = ps_rc_ctxt->i4_pre_enc_rc_delay;
            ps_rc_ctxt->i4_pre_enc_qp_read_index =
                (ps_rc_ctxt->i4_pre_enc_qp_read_index + 1) % i4_delay_l0_enc;

            if(ps_rc_ctxt->i4_num_frms_from_reset < i4_delay_l0_enc)
            {
                ps_rc_ctxt->i4_num_frms_from_reset++;
                if(ps_rc_ctxt->i4_num_frms_from_reset >= i4_delay_l0_enc)
                    ps_rc_ctxt->i4_use_qp_offset_pre_enc = 0;
            }
        }

        i4_frame_qp = CLIP3(i4_frame_qp, ps_rc_ctxt->i4_min_frame_qp, ps_rc_ctxt->i4_max_frame_qp);
        return i4_frame_qp;
    }
}
/**
******************************************************************************
*
*  @brief function to estimate L0 satd based on L1 satd. '
*
*
*  @par   Description
*
*  @param[in]   pv_rc_ctxt
*               void pointer to rc ctxt
*  @param[in]   rc_lap_out_params_t *
pointer to lap out structure
*   @param[in]  i8_est_L0_satd_act
*               estimated L0 satd/act based on L1 satd/act
*  @return      void
*
******************************************************************************
*/
LWORD64 ihevce_get_L0_satd_based_on_L1(
    LWORD64 i8_satd_by_act_L1, WORD32 i4_num_pixel, WORD32 i4_cur_q_scale)
{
    LWORD64 est_L0_satd_by_act;
    float m, c;
    /** choose coeff based on resolution*/
    if(i4_num_pixel > 5184000)
    {
        m = (float)2.3911;
        c = (float)86329;
    }
    else if(i4_num_pixel > 1497600)
    {
        m = (float)2.7311;
        c = (float)-1218.9;
    }
    else if(i4_num_pixel > 633600)
    {
        m = (float)3.1454;
        c = (float)-5836.1;
    }
    else
    {
        m = (float)3.5311;
        c = (float)-2377.2;
    }
    /*due to qp difference between I and  P, For P pic for same */
    est_L0_satd_by_act = (LWORD64)(i8_satd_by_act_L1 / i4_cur_q_scale * m + c) * i4_cur_q_scale;

    {
        if(est_L0_satd_by_act < (i4_num_pixel >> 3))
            est_L0_satd_by_act = (i4_num_pixel >> 3);
    }
    return est_L0_satd_by_act;
}
/**
******************************************************************************
*
*  @name  ihevce_rc_register_L1_analysis_data
*
*  @par   Description
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*               ps_rc_lap_out
*               i8_est_L0_satd_by_act
*               i8_pre_intra_sad
*               i8_l1_hme_sad
*  @return      void
*
******************************************************************************
*/
void ihevce_rc_register_L1_analysis_data(
    void *pv_rc_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    LWORD64 i8_est_L0_satd_by_act,
    LWORD64 i8_pre_intra_sad,
    LWORD64 i8_l1_hme_sad)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    WORD32 i, data_available = 1;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);

    //if( ps_rc_ctxt->u4_rc_scene_num_est_L0_intra_sad_available == ps_rc_lap_out->u4_rc_scene_num)
    {
        /*update current frame's data*/
        ps_rc_ctxt->ai8_prev_frame_est_L0_satd[rc_pic_type] = i8_est_L0_satd_by_act;
        ps_rc_ctxt->ai8_prev_frame_hme_sad[rc_pic_type] = i8_l1_hme_sad;
        ps_rc_ctxt->ai8_prev_frame_pre_intra_sad[rc_pic_type] = i8_pre_intra_sad;
    }
    /*check if data is available for all picture type*/
    if(!ps_rc_ctxt->i4_is_est_L0_intra_sad_available)
    {
        for(i = 0; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
        {
            data_available &= (ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i] >= 0);
            if(ps_rc_ctxt->i4_field_pic == 1 && i != 0)
                data_available &= (ps_rc_ctxt->ai8_prev_frame_est_L0_satd[i + FIELD_OFFSET] >= 0);
        }
        ps_rc_ctxt->i4_is_est_L0_intra_sad_available = data_available;
    }
}

/*#######################################################*/
/******* END OF PRE-ENC QP QUERY FUNCTIONS **************/
/*#####################################################*/

/*##########################################################*/
/******* START OF ENC THRD QP QUERY FUNCTIONS **************/
/*########################################################*/

/**
******************************************************************************
*
*  @brief function to get ihevce_rc_get_pic_quant
*
*  @par   Description
*  @param[in]   i4_update_delay : The Delay in the update. This can happen for dist. case!
*               All decision should consider this delay for updation!
******************************************************************************
*/

WORD32 ihevce_rc_get_pic_quant(
    void *pv_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    IHEVCE_RC_CALL_TYPE call_type,
    WORD32 i4_enc_frm_id,
    WORD32 i4_update_delay,
    WORD32 *pi4_tot_bits_estimated)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    WORD32 i4_frame_qp, i4_frame_qp_q6, i4_hevc_frame_qp = -1, i4_deltaQP = 0;
    WORD32 i4_max_frame_bits = (1 << 30);
    rc_type_e e_rc_type = ps_rc_ctxt->e_rate_control_type;
    WORD32 slice_type, index, i4_num_frames_in_cur_gop, i4_cur_est_texture_bits;
    WORD32 temporal_layer_id = ps_rc_lap_out->i4_rc_temporal_lyr_id;
    IV_PICTURE_CODING_TYPE_T pic_type = (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);
    float i_to_avg_bit_ratio;
    frame_info_t s_frame_info_temp;
    WORD32 i4_scene_num = ps_rc_lap_out->u4_rc_scene_num % MAX_SCENE_NUM;
    WORD32 i4_vbv_buf_max_bits;
    WORD32 i4_est_tex_bits;
    WORD32 i4_cur_est_header_bits, i4_fade_scene;
    WORD32 i4_model_available, i4_is_no_model_scd;
    WORD32 i4_estimate_to_calc_frm_error;

    /* The window for which your update is guaranteed */
    WORD32 updated_window = ps_rc_ctxt->i4_num_frame_in_lap_window - i4_update_delay;

    ps_rc_ctxt->i4_scene_num_latest = i4_scene_num;

    ps_rc_ctxt->s_rc_high_lvl_stat.i4_modelQP = INVALID_QP;
    ps_rc_ctxt->s_rc_high_lvl_stat.i4_finalQP = INVALID_QP;
    ps_rc_ctxt->s_rc_high_lvl_stat.i4_maxEbfQP = INVALID_QP;

    ps_rc_ctxt->i4_quality_preset = ps_rc_lap_out->i4_rc_quality_preset;
    ps_rc_ctxt->s_rc_high_lvl_stat.i4_finalQP = INVALID_QP;

    if(1 == ps_rc_ctxt->i4_bitrate_changed)
    {
        ps_rc_ctxt->i4_bitrate_changed = 0;
    }
    if(CONST_QP == e_rc_type)
    {
        switch(pic_type)
        {
        case IV_I_FRAME:
        case IV_IDR_FRAME:
        {
            slice_type = ISLICE;
            break;
        }
        case IV_P_FRAME:
        {
            slice_type = PSLICE;
            break;
        }
        case IV_B_FRAME:
        {
            slice_type = BSLICE;
            break;
        }
        }

        i4_frame_qp = ihevce_get_cur_frame_qp(
            ps_rc_ctxt->i4_init_frame_qp_user,
            slice_type,
            temporal_layer_id,
            ps_rc_ctxt->i4_min_frame_qp,
            ps_rc_ctxt->i4_max_frame_qp,
            ps_rc_ctxt->ps_rc_quant_ctxt);
        return i4_frame_qp;
    }
    else
    {
        WORD32 is_scd_ref_frame = 0, i4_num_scd_in_lap_window = 0, num_frames_b4_scd = 0,
               scene_type = 0, i;
        //ihevce_lap_output_params_t *ps_cur_rc_lap_out;

        if(ps_rc_ctxt->ai4_scene_num_last_pic[rc_pic_type] !=
           (WORD32)ps_rc_lap_out->u4_rc_scene_num)
        {
            rc_reset_pic_model(ps_rc_ctxt->rc_hdl, rc_pic_type);
            rc_reset_first_frame_coded_flag(ps_rc_ctxt->rc_hdl, rc_pic_type);
        }
        ps_rc_ctxt->ai4_scene_num_last_pic[rc_pic_type] = ps_rc_lap_out->u4_rc_scene_num;

        if(call_type == ENC_GET_QP)
        {
            i4_model_available = model_availability(ps_rc_ctxt->rc_hdl, rc_pic_type);

            ps_rc_lap_out->i8_est_text_bits = -1;
        }

        if((rc_pic_type == I_PIC) || (rc_pic_type == P_PIC) || (rc_pic_type == P1_PIC))
        {
            ps_rc_ctxt->i4_cur_scene_num = ps_rc_lap_out->u4_rc_scene_num;
        }

        {
            if(!(pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME))
            {
                ps_rc_ctxt->ai8_cur_frame_coarse_ME_cost[i4_enc_frm_id] =
                    ps_rc_lap_out->i8_frame_acc_coarse_me_cost;
            }
            /*check if frame is scene cut*/
            /* If scd do not query the model. obtain qp from offline data model*/
            scene_type = ihevce_rc_lap_get_scene_type(ps_rc_lap_out);

            if(ps_rc_ctxt->ai4_scene_numbers[ps_rc_lap_out->u4_rc_scene_num] == 0 &&
               (scene_type != SCENE_TYPE_SCENE_CUT))
            {
                scene_type = SCENE_TYPE_SCENE_CUT;
            }

            if(ps_rc_ctxt->ai4_scene_numbers[ps_rc_lap_out->u4_rc_scene_num] > 0 &&
               (scene_type == SCENE_TYPE_SCENE_CUT))
            {
                scene_type = SCENE_TYPE_NORMAL;
            }
            if(scene_type == SCENE_TYPE_SCENE_CUT)
            {
                if((ps_rc_lap_out->i4_rc_quality_preset == IHEVCE_QUALITY_P6) &&
                   (rc_pic_type > P_PIC))
                {
                    is_scd_ref_frame = 0;
                }
                else
                {
                    is_scd_ref_frame = 1;
                }
            }
            else if(scene_type == SCENE_TYPE_PAUSE_TO_RESUME)
            {
                /*pause to resume flag will only be set in layer 0 frames( I and P pic)*/
                /*I PIC can handle this by detecting I_only SCD which is based on open loop SATD hence explicit handling for pause to resume is required only for P_PIC*/

                if(ps_rc_lap_out->i4_rc_quality_preset == IHEVCE_QUALITY_P6)
                {
                    if(call_type == ENC_GET_QP && rc_pic_type == P_PIC)
                    {
                        ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] = 1;
                    }
                }
                else
                {
                    if(call_type == ENC_GET_QP && rc_pic_type != I_PIC)
                    {
                        ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] = 1;
                    }
                }
            }

            ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i4_enc_frm_id] =
                ps_rc_lap_out->i4_is_cmplx_change_reset_model;
            ps_rc_ctxt->ai4_is_cmplx_change_reset_bits[i4_enc_frm_id] =
                ps_rc_lap_out->i4_is_cmplx_change_reset_bits;

            /*initialise the coeffs to 1 in case lap is not used */
            for(i = 0; i < MAX_PIC_TYPE; i++)
            {
                ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][i][0] = 1.0;
                ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][i][1] = 0.0;
                ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][i][2] = 0.0;
            }

            /*treat even first frame as scd frame*/
            if(!ps_rc_ctxt->i4_is_first_frame_encoded)
            {
                is_scd_ref_frame = 1;
            }

            /*special case SCD handling for Non-I pic*/
            if(!(pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME) && call_type == ENC_GET_QP)
            {
                if(is_scd_ref_frame)
                {
                    /*A non-I pic will only be marked as scene cut only if there is another SCD follows within another subgop*/
                    ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] = 1;
                }
                /*check if current sad is very different from previous SAD and */
                else if(
                    !ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] &&
                    ps_rc_lap_out->i4_is_non_I_scd)
                {
                    ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] = 1;
                    is_scd_ref_frame = 1;
                }
            }

            if(call_type == PRE_ENC_GET_QP)
            {
                /*Only I frames are considered as scd pic during pre-enc*/
                is_scd_ref_frame &= (pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME);
            }

            /*special case SCD handling for I pic*/
            if((pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME) && !is_scd_ref_frame)
            {
                /*If open loop SATD's of two I picture are very different then treat the I pic as SCD and reset only model as this can
                happen during fade-in and fade-out where other picture types would have learnt. Reset is required only for I.*/

                if(ps_rc_lap_out->i4_is_I_only_scd)
                {
                    is_scd_ref_frame = 1;
                    ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id] = 1;
                }
            }
            /*should be recalculated for every picture*/
            if((updated_window) > 0 && (call_type == ENC_GET_QP) && (ps_rc_ctxt->i4_rc_pass != 2))
            {
                rc_lap_out_params_t *ps_cur_rc_lap_out;

                UWORD32 u4_L1_based_lap_complexity_q7;
                WORD32 i = 0, k = 0, i4_f_sim = 0, i4_h_sim = 0, i4_var_sum = 0,
                       i4_num_pic_metric_count = 0, i4_is_first_frm = 1,
                       i4_intra_frame_interval = 0;
                LWORD64 i8_l1_analysis_lap_comp = 0;
                LWORD64 nor_frm_hme_sad_q10;
                picture_type_e curr_rc_pic_type;
                WORD32 ai4_pic_dist[MAX_PIC_TYPE] = { 0 };
                LWORD64 i8_sad_first_frame_pic_type[MAX_PIC_TYPE] = { 0 },
                        i8_total_sad_pic_type[MAX_PIC_TYPE] = { 0 };
                LWORD64 i8_last_frame_pic_type[MAX_PIC_TYPE] = { 0 }, i8_esti_consum_bits = 0;
                WORD32 i4_num_pic_type[MAX_PIC_TYPE] = { 0 }, i4_frames_in_lap_end = 0,
                       i4_first_frame_coded_flag, i4_gop_end_flag = 1, i4_num_frame_for_ebf = 0;
                i4_first_frame_coded_flag = is_first_frame_coded(ps_rc_ctxt->rc_hdl);

                /*Setting the next scene cut as well as pic distribution for the gop*/

                ps_cur_rc_lap_out = (rc_lap_out_params_t *)ps_rc_lap_out;
                i4_intra_frame_interval = rc_get_intra_frame_interval(ps_rc_ctxt->rc_hdl);

                /*Set the rc sc i next*/
                if(ps_cur_rc_lap_out != NULL)
                {
                    WORD32 i4_count = 0;
                    do
                    {
                        if(((rc_lap_out_params_t *)ps_cur_rc_lap_out->ps_rc_lap_out_next_encode ==
                            NULL))  //||((( (ihevce_lap_output_params_t*)ps_cur_rc_lap_out->ps_lap_out_next)->i8_pre_intra_sad == -1) || (  ((ihevce_lap_output_params_t*)ps_cur_rc_lap_out->ps_lap_out_next)->i8_raw_pre_intra_sad == -1) ||(  ((ihevce_lap_output_params_t*)ps_cur_rc_lap_out->ps_lap_out_next)->i8_raw_l1_coarse_me_sad == -1) ||(((ihevce_lap_output_params_t*)ps_cur_rc_lap_out->ps_lap_out_next)->i8_frame_acc_coarse_me_sad == -1)))
                            break;

                        ps_cur_rc_lap_out =
                            (rc_lap_out_params_t *)ps_cur_rc_lap_out->ps_rc_lap_out_next_encode;
                        i4_count++;

                    } while((i4_count + 1) < updated_window);

                    rc_set_next_sc_i_in_rc_look_ahead(
                        ps_rc_ctxt->rc_hdl, ps_cur_rc_lap_out->i4_next_sc_i_in_rc_look_ahead);
                    rc_update_pic_distn_lap_to_rc(
                        ps_rc_ctxt->rc_hdl, ps_cur_rc_lap_out->ai4_num_pic_type);

                    ps_rc_ctxt->i4_next_sc_i_in_rc_look_ahead =
                        ps_cur_rc_lap_out->i4_next_sc_i_in_rc_look_ahead;
                }

                ps_cur_rc_lap_out = (rc_lap_out_params_t *)ps_rc_lap_out;
                if(ps_cur_rc_lap_out != NULL)
                {
                    /*initialise the coeffs to 1 in case lap is not used */
                    for(i = 0; i < MAX_PIC_TYPE; i++)
                    {
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][i][0] = 0.0;
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][i][1] = 0.0;
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][i][2] = 0.0;
                    }
                    i = 0;
                    k = 0;

                    //ASSERT(ps_cur_rc_lap_out != NULL);
                    do
                    {
                        curr_rc_pic_type = ihevce_rc_conv_pic_type(
                            (IV_PICTURE_CODING_TYPE_T)ps_cur_rc_lap_out->i4_rc_pic_type,
                            ps_rc_ctxt->i4_field_pic,
                            ps_cur_rc_lap_out->i4_rc_temporal_lyr_id,
                            ps_cur_rc_lap_out->i4_is_bottom_field,
                            ps_rc_ctxt->i4_top_field_first);
                        if(ps_rc_ctxt->i4_is_first_frame_encoded || !i4_is_first_frm)
                        {
                            /*Ignore first frame Fsim as it is not valid for first frame*/
                            i4_f_sim += ps_cur_rc_lap_out->s_pic_metrics.i4_fsim;
                            i4_h_sim += ps_cur_rc_lap_out->s_pic_metrics.ai4_hsim[0];
                            i4_var_sum += (WORD32)ps_cur_rc_lap_out->s_pic_metrics.i8_8x8_var_lum;
                            i4_num_pic_metric_count++;
                            //DBG_PRINTF("\n fsim = %d i = %d",ps_cur_rc_lap_out->s_pic_metrics.i4_fsim,i);
                            //ASSERT(ps_cur_rc_lap_out->s_pic_metrics.i4_fsim <= 128);
                        }

                        /*accumulate complexity from LAP2*/
                        if(curr_rc_pic_type == I_PIC)
                        {
                            i8_l1_analysis_lap_comp +=
                                (LWORD64)(1.17 * ps_cur_rc_lap_out->i8_raw_pre_intra_sad);
                        }
                        else
                        {
                            if(curr_rc_pic_type <= B2_PIC)
                                i8_l1_analysis_lap_comp += (LWORD64)(
                                    (float)ps_cur_rc_lap_out->i8_raw_l1_coarse_me_sad /
                                    pow(1.125f, curr_rc_pic_type));
                            else
                                i8_l1_analysis_lap_comp += (LWORD64)(
                                    (float)ps_cur_rc_lap_out->i8_raw_l1_coarse_me_sad /
                                    pow(1.125f, curr_rc_pic_type - B2_PIC));
                        }
                        i++;
                        i4_is_first_frm = 0;

                        /*CAll the function for predictting the ebf and stuffing condition check*/
                        /*rd model pass lapout l1 pass ebf return estimated ebf and signal*/

                        {
                            if(i4_first_frame_coded_flag && (i4_gop_end_flag != 0))
                            {
                                if(curr_rc_pic_type == 0)
                                    i4_gop_end_flag = 0;

                                if(i4_gop_end_flag)
                                {
                                    WORD32 prev_frm_cl_sad =
                                        rc_get_prev_frame_sad(ps_rc_ctxt->rc_hdl, curr_rc_pic_type);
                                    WORD32 cur_frm_est_cl_sad = (WORD32)(
                                        (ps_cur_rc_lap_out->i8_frame_acc_coarse_me_cost *
                                         prev_frm_cl_sad) /
                                        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[curr_rc_pic_type]);
                                    i8_esti_consum_bits += bit_alloc_get_estimated_bits_for_pic(
                                        ps_rc_ctxt->rc_hdl,
                                        cur_frm_est_cl_sad,
                                        prev_frm_cl_sad,
                                        curr_rc_pic_type);
                                    i4_num_frame_for_ebf++;
                                }
                            }
                        }
                        ps_cur_rc_lap_out =
                            (rc_lap_out_params_t *)ps_cur_rc_lap_out->ps_rc_lap_out_next_encode;
                        /*The scene cut is lap window other than current frame is used to reduce bit alloc window for I pic*/
                        if(ps_cur_rc_lap_out != NULL &&
                           ps_cur_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
                        {
                            i4_num_scd_in_lap_window++;
                            if(i4_num_scd_in_lap_window == 1)
                            {
                                /*Note how many frames are parsed before first scd is hit*/
                                num_frames_b4_scd = i + 1;
                            }
                        }

                        if((ps_cur_rc_lap_out == NULL ||
                            (i >=
                             (updated_window -
                              k))))  //||((( -1 == ps_cur_rc_lap_out->i8_pre_intra_sad ) || ( -1 == ps_cur_rc_lap_out->i8_raw_pre_intra_sad ) ||( -1 == ps_cur_rc_lap_out->i8_raw_l1_coarse_me_sad) ||(-1 == ps_cur_rc_lap_out->i8_frame_acc_coarse_me_sad))))
                            break;
                        if(0)  //(( -1 == ps_cur_rc_lap_out->i8_pre_intra_sad ) || ( -1 == ps_cur_rc_lap_out->i8_raw_pre_intra_sad ) ||( -1 == ps_cur_rc_lap_out->i8_raw_l1_coarse_me_sad) ||(-1 == ps_cur_rc_lap_out->i8_frame_acc_coarse_me_sad)))
                        {
                            k++;
                            ps_cur_rc_lap_out =
                                (rc_lap_out_params_t *)ps_cur_rc_lap_out->ps_rc_lap_out_next_encode;
                            if(ps_cur_rc_lap_out == NULL)
                                break;
                            continue;
                        }

                    } while(1);
                    ;
                }
                /*For the first subgop we cant have underflow prevention logic
                since once picture of each type is not encoded also happens for static contents thants high i_to avg_ratio */
                if(i4_first_frame_coded_flag &&
                   (ps_rc_ctxt->ai_to_avg_bit_ratio[i4_enc_frm_id] > I_TO_REST_SLOW))
                {
                    if(!(i4_num_frame_for_ebf < ps_rc_ctxt->i4_max_inter_frm_int))
                        rc_bit_alloc_detect_ebf_stuff_scenario(
                            ps_rc_ctxt->rc_hdl,
                            i4_num_frame_for_ebf,
                            i8_esti_consum_bits,
                            ps_rc_ctxt->i4_max_inter_frm_int);
                }

                k = 0;

                i4_frames_in_lap_end = 0;
                {
                    rc_lap_out_params_t *ps_cur_rc_lap_out1;

                    ps_cur_rc_lap_out1 = (rc_lap_out_params_t *)ps_rc_lap_out;
                    do
                    {
                        curr_rc_pic_type = ihevce_rc_conv_pic_type(
                            (IV_PICTURE_CODING_TYPE_T)ps_cur_rc_lap_out1->i4_rc_pic_type,
                            ps_rc_ctxt->i4_field_pic,
                            ps_cur_rc_lap_out1->i4_rc_temporal_lyr_id,
                            ps_cur_rc_lap_out1->i4_is_bottom_field,
                            ps_rc_ctxt->i4_top_field_first);
                        /*accumulate complexity from LAP2*/

                        if(curr_rc_pic_type == I_PIC)
                        {
                            i8_total_sad_pic_type[I_PIC] +=
                                ps_cur_rc_lap_out1->i8_raw_pre_intra_sad;
                            i8_last_frame_pic_type[I_PIC] =
                                ps_cur_rc_lap_out1->i8_raw_pre_intra_sad;
                        }
                        else
                        {
                            i8_total_sad_pic_type[curr_rc_pic_type] +=
                                ps_cur_rc_lap_out1->i8_raw_l1_coarse_me_sad;
                            i8_last_frame_pic_type[curr_rc_pic_type] =
                                ps_cur_rc_lap_out1->i8_raw_l1_coarse_me_sad;
                        }
                        if(i4_num_pic_type[curr_rc_pic_type] == 0)
                        {
                            if(curr_rc_pic_type == I_PIC)
                            {
                                i8_sad_first_frame_pic_type[I_PIC] =
                                    ps_cur_rc_lap_out1->i8_raw_pre_intra_sad;
                            }
                            else
                            {
                                i8_sad_first_frame_pic_type[curr_rc_pic_type] =
                                    ps_cur_rc_lap_out1->i8_raw_l1_coarse_me_sad;
                            }
                        }
                        i4_num_pic_type[curr_rc_pic_type]++;

                        i4_frames_in_lap_end++;

                        ps_cur_rc_lap_out1 =
                            (rc_lap_out_params_t *)ps_cur_rc_lap_out1->ps_rc_lap_out_next_encode;
                        if((ps_cur_rc_lap_out1 == NULL ||
                            (i4_frames_in_lap_end >=
                             (updated_window -
                              k))))  //||((( -1 == ps_cur_rc_lap_out1->i8_pre_intra_sad ) || ( -1 == ps_cur_rc_lap_out1->i8_raw_pre_intra_sad ) ||( -1 == ps_cur_rc_lap_out1->i8_raw_l1_coarse_me_sad) ||(-1 == ps_cur_rc_lap_out1->i8_frame_acc_coarse_me_sad))))
                        {
                            break;
                        }
                        if(0)  //((( -1 == ps_cur_rc_lap_out1->i8_pre_intra_sad ) || ( -1 == ps_cur_rc_lap_out1->i8_raw_pre_intra_sad ) ||( -1 == ps_cur_rc_lap_out1->i8_raw_l1_coarse_me_sad) ||(-1 == ps_cur_rc_lap_out1->i8_frame_acc_coarse_me_sad))))
                        {
                            k++;
                            ps_cur_rc_lap_out1 = (rc_lap_out_params_t *)
                                                     ps_cur_rc_lap_out1->ps_rc_lap_out_next_encode;
                            if(ps_cur_rc_lap_out1 == NULL)
                                break;
                            continue;
                        }

                    } while(i4_frames_in_lap_end < (ps_rc_ctxt->i4_next_sc_i_in_rc_look_ahead - k));
                }

                /*get picture type distribution in LAP*/
                rc_get_pic_distribution(ps_rc_ctxt->rc_hdl, &ai4_pic_dist[0]);

                {
                    float f_prev_comp;
                    WORD32 j;
                    float af_sum_weigh[MAX_PIC_TYPE], af_nume_weight[MAX_PIC_TYPE];
                    float af_average_sad_pic_type[MAX_PIC_TYPE] = { 0 };
                    for(j = 0; j < MAX_PIC_TYPE; j++)
                    {
                        if(i4_num_pic_type[j] > 0)
                        {
                            af_average_sad_pic_type[j] =
                                (float)i8_total_sad_pic_type[j] / i4_num_pic_type[j];
                        }

                        f_prev_comp = 1.;

                        i4_num_pic_type[j] = (i4_num_pic_type[j] > ai4_pic_dist[j])
                                                 ? ai4_pic_dist[j]
                                                 : i4_num_pic_type[j];

                        af_sum_weigh[j] = (float)i4_num_pic_type[j];
                        af_nume_weight[j] = 1.0;

                        if(i4_num_pic_type[j] > 1 && (af_average_sad_pic_type[j] > 0))
                        {
                            af_nume_weight[j] =
                                (float)i8_sad_first_frame_pic_type[j] / af_average_sad_pic_type[j];

                            f_prev_comp =
                                (float)i8_last_frame_pic_type[j] / af_average_sad_pic_type[j];
                        }
                        //if(rc_pic_type != I_PIC)
                        {
                            af_sum_weigh[j] += f_prev_comp * (ai4_pic_dist[j] - i4_num_pic_type[j]);
                        }
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][j][0] = af_nume_weight[j];
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][j][1] = af_sum_weigh[j];
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][j][2] = af_average_sad_pic_type[j];

                        /*Disabling steady state complexity based bit movement*/
                        /*Enable it in CBR and not in VBR since VBR already has complexity based bit movement*/

                        if(0) /*i4_frames_in_lap_end < (updated_window) || ps_rc_ctxt->e_rate_control_type == VBR_STREAMING)*/
                        {
                            ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][j][0] = 1.0;
                            ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id][j][1] =
                                0;  //(float)ai4_pic_dist[j];
                        }
                    }
                    memmove(
                        ps_rc_lap_out->ps_frame_info->af_sum_weigh,
                        ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id],
                        sizeof(float) * MAX_PIC_TYPE * 3);
                }

                if(i4_num_pic_metric_count > 0)
                {
                    i4_f_sim = i4_f_sim / i4_num_pic_metric_count;
                    i4_h_sim = i4_h_sim / i4_num_pic_metric_count;
                    i4_var_sum = i4_var_sum / i4_num_pic_metric_count;
                }
                else
                {
                    i4_f_sim = MODERATE_FSIM_VALUE;
                    i4_h_sim = MODERATE_FSIM_VALUE;
                }

                if(i > 0)
                {
                    float lap_L1_comp =
                        (float)i8_l1_analysis_lap_comp /
                        (i * ps_rc_ctxt->i4_frame_height *
                         ps_rc_ctxt->i4_frame_width);  //per frame per pixel complexity

                    lap_L1_comp = rc_get_offline_normalized_complexity(
                        ps_rc_ctxt->u4_intra_frame_interval,
                        ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width,
                        lap_L1_comp,
                        ps_rc_ctxt->i4_rc_pass);

                    u4_L1_based_lap_complexity_q7 = (WORD32)((lap_L1_comp * (1 << 7)) + .05f);
                }
                else
                {
                    u4_L1_based_lap_complexity_q7 = 25;
                }
                ps_rc_ctxt->ai4_lap_complexity_q7[i4_enc_frm_id] =
                    (WORD32)u4_L1_based_lap_complexity_q7;
                /*clip f_sim to 0.3 for better stability*/
                if(i4_f_sim < 38)
                    i4_f_sim = 128 - MAX_LAP_COMPLEXITY_Q7;
                ps_rc_ctxt->ai4_lap_f_sim[i4_enc_frm_id] = i4_f_sim;

                /*calculate normalized per pixel sad*/
                nor_frm_hme_sad_q10 = (ps_rc_lap_out->i8_frame_acc_coarse_me_cost << 10) /
                                      (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width);
                /*if(rc_pic_type == P_PIC)
                DBG_PRINTF("\n P frm hme sad = %f  ",((float)nor_frm_hme_sad_q10/ (1 << 10)));  */
                rc_put_temp_comp_lap(
                    ps_rc_ctxt->rc_hdl, i4_f_sim, nor_frm_hme_sad_q10, rc_pic_type);

                rc_set_num_scd_in_lap_window(
                    ps_rc_ctxt->rc_hdl, i4_num_scd_in_lap_window, num_frames_b4_scd);

                if(rc_pic_type == I_PIC && updated_window > (ps_rc_ctxt->i4_max_inter_frm_int << 1))
                {
                    float i_to_avg_bit_ratio = ihevce_get_i_to_avg_ratio(
                        (void *)ps_rc_ctxt,
                        ps_rc_lap_out,
                        1,
                        1,
                        1,
                        ps_rc_lap_out->ai4_offsets,
                        i4_update_delay);
                    i_to_avg_bit_ratio = i_to_avg_bit_ratio * 1;
                }

                /* accumulation of the hme sad over next sub gop to find the temporal comlexity of the sub GOP*/
                if((rc_pic_type == I_PIC) || (rc_pic_type == P_PIC))
                {
                    ihevce_compute_temporal_complexity_reset_Kp_Kb(
                        ps_rc_lap_out, (void *)ps_rc_ctxt, 1);
                }

                if(i4_var_sum > MAX_LAP_VAR)
                {
                    i4_var_sum = MAX_LAP_VAR;
                }

                {
                    /*Filling for dumping data */

                    ps_rc_ctxt->ai4_num_scd_in_lap_window[i4_enc_frm_id] = i4_num_scd_in_lap_window;
                    ps_rc_ctxt->ai4_num_frames_b4_scd[i4_enc_frm_id] = num_frames_b4_scd;
                }
            }
        }

        if((ps_rc_lap_out->i4_rc_quality_preset == IHEVCE_QUALITY_P6) && (rc_pic_type > P_PIC))
        {
            ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] = 0;
            is_scd_ref_frame = 0;
        }
        i4_fade_scene = 0;
        /*Scene type fade is marked only for P pics which are in fade regions*/
        if((ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_FADE_IN ||
            ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_FADE_OUT) &&
           (ps_rc_lap_out->i4_rc_temporal_lyr_id == 0))
        {
            is_scd_ref_frame = 1;
            i4_fade_scene = 1;
        }

        if((!(is_scd_ref_frame || ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id])) &&
           (((is_first_frame_coded(ps_rc_ctxt->rc_hdl)) && (pic_type == IV_I_FRAME)) ||
            (pic_type != IV_I_FRAME)))
        {
            WORD32 i4_is_first_frame_coded = is_first_frame_coded(ps_rc_ctxt->rc_hdl);
            i4_is_no_model_scd = 0;
            if(call_type == ENC_GET_QP)
            {
                if(((0 == i4_model_available) || (!i4_is_first_frame_coded)))
                {
                    /*No scene change but model not available*/
                    i4_is_no_model_scd = 1;
                }
            }
        }
        else
        {
            /*actual scene changes*/
            i4_is_no_model_scd = 2;
        }
        /** Pre-enc thread as of now SCD handling is not present */
        if(!i4_is_no_model_scd)
        {
            WORD32 i4_is_first_frame_coded, i4_prev_I_frm_sad, i4_cur_I_frm_sad;
            /*Once first frame has been encoded use prev frame intra satd and cur frame satd to alter est intra sad for cur frame*/
            i4_is_first_frame_coded = is_first_frame_coded(ps_rc_ctxt->rc_hdl);

            /*prev I frame sad i changes only in enc stage. For pre enc cur and prev will be same*/
            if(ps_rc_ctxt->i8_prev_i_frm_cost > 0)
            {
                if(i4_is_first_frame_coded && (pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME))
                {
                    i4_prev_I_frm_sad = rc_get_prev_frame_intra_sad(ps_rc_ctxt->rc_hdl);
                    i4_cur_I_frm_sad = (WORD32)(
                        (ps_rc_ctxt->ai8_cur_frm_intra_cost[i4_enc_frm_id] * i4_prev_I_frm_sad) /
                        ps_rc_ctxt->i8_prev_i_frm_cost);
                    rc_update_prev_frame_intra_sad(ps_rc_ctxt->rc_hdl, i4_cur_I_frm_sad);
                }
            }
            /*scale previous frame closed loop SAD with current frame HME SAD to be considered as current frame SAD*/
            if(i4_is_first_frame_coded && !(pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME) &&
               call_type == ENC_GET_QP)
            {
                if(ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[rc_pic_type] > 0)
                {
                    WORD32 prev_frm_cl_sad = rc_get_prev_frame_sad(ps_rc_ctxt->rc_hdl, rc_pic_type);
                    WORD32 cur_frm_est_cl_sad = (WORD32)(
                        (ps_rc_ctxt->ai8_cur_frame_coarse_ME_cost[i4_enc_frm_id] *
                         prev_frm_cl_sad) /
                        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[rc_pic_type]);
                    rc_update_prev_frame_sad(ps_rc_ctxt->rc_hdl, cur_frm_est_cl_sad, rc_pic_type);
                }
            }

            if(rc_pic_type == I_PIC && updated_window > (ps_rc_ctxt->i4_max_inter_frm_int << 1))
            {
                ps_rc_ctxt->ai_to_avg_bit_ratio[i4_enc_frm_id] = ihevce_get_i_to_avg_ratio(
                    (void *)ps_rc_ctxt,
                    ps_rc_lap_out,
                    1,
                    0,
                    1,
                    ps_rc_lap_out->ai4_offsets,
                    i4_update_delay);
            }

            ps_rc_ctxt->s_rc_high_lvl_stat.i8_bits_from_finalQP = -1;
            i4_frame_qp_q6 = get_frame_level_qp(
                ps_rc_ctxt->rc_hdl,
                rc_pic_type,
                i4_max_frame_bits,
                &i4_cur_est_texture_bits,  //this value is returned by rc
                ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id],
                1,
                ps_rc_ctxt->ai_to_avg_bit_ratio[i4_enc_frm_id],
                ps_rc_lap_out->ps_frame_info,
                ps_rc_lap_out->i4_complexity_bin,
                i4_scene_num, /*no pause resume concept*/
                pi4_tot_bits_estimated,
                &ps_rc_lap_out->i4_is_model_valid,
                &i4_vbv_buf_max_bits,
                &i4_est_tex_bits,
                &i4_cur_est_header_bits,
                &ps_rc_ctxt->s_rc_high_lvl_stat.i4_maxEbfQP,
                &ps_rc_ctxt->s_rc_high_lvl_stat.i4_modelQP,
                &i4_estimate_to_calc_frm_error);
            ASSERT(*pi4_tot_bits_estimated != 0);
            /** The usage of global table will truncate the input given as qp format and hence will not return very low qp values desirable at very
            low bitrate. Hence on the fly calculation is enabled*/

            i4_hevc_frame_qp =
                ihevce_rc_get_scaled_hevce_qp_q6(i4_frame_qp_q6, ps_rc_ctxt->u1_bit_depth);

            if(1 == ps_rc_lap_out->i4_is_model_valid)
                ps_rc_lap_out->i4_is_steady_state = 1;
            else
                ps_rc_lap_out->i4_is_steady_state = 0;

            ps_rc_ctxt->s_rc_high_lvl_stat.i4_is_offline_model_used = 0;
            ps_rc_ctxt->i8_est_I_pic_header_bits = i4_cur_est_header_bits;
        }
        else
        {
            WORD32 i4_count = 0, i4_total_bits, i4_min_error_hevc_qp = 0;
            float f_percent_error = 0.0f, f_min_error = 10000.0f;
            WORD32 i4_current_bits_estimated = 0;
            float i4_i_to_rest_ratio_final;
            WORD32 i4_best_br_id = 0;
            float af_i_qs[2];
            LWORD64 ai8_i_tex_bits[2];
            WORD32 i4_ref_qscale = ihevce_rc_get_scaled_mpeg2_qp(
                ps_rc_lap_out->i4_L0_qp, ps_rc_ctxt->ps_rc_quant_ctxt);
            WORD32 ai4_header_bits[2];

            ps_rc_lap_out->i4_is_steady_state = 0;

            if(ps_rc_lap_out->i4_L0_qp > 44)
                ps_rc_lap_out->i4_L0_qp = 44;
            if(ps_rc_lap_out->i4_L0_qp < 7 - ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset)
                ps_rc_lap_out->i4_L0_qp = 7 - ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

            ps_rc_lap_out->i4_L0_qp = ps_rc_lap_out->i4_L0_qp - 9;
            ps_rc_lap_out->i4_is_model_valid = 0;
            ps_rc_ctxt->s_rc_high_lvl_stat.i4_is_offline_model_used = 1;
            ps_rc_ctxt->s_rc_high_lvl_stat.i8_bits_from_finalQP = -1;

            ps_rc_ctxt->i4_normal_inter_pic = (i4_is_no_model_scd == 1);
            while(1)
            {
                WORD32 i4_frame_qs_q3;
                WORD32 i4_estimate_to_calc_frm_error_temp;

                i_to_avg_bit_ratio = ihevce_get_i_to_avg_ratio(
                    (void *)ps_rc_ctxt,
                    ps_rc_lap_out,
                    1,
                    0,
                    1,
                    ps_rc_lap_out->ai4_offsets,
                    i4_update_delay);

                ps_rc_ctxt->ai_to_avg_bit_ratio[i4_enc_frm_id] = i_to_avg_bit_ratio;

                /** Use estimate of header bits from pre-enc*/
                if(1 == i4_is_no_model_scd)
                {
                    ps_rc_ctxt->i8_est_I_pic_header_bits =
                        get_est_hdr_bits(ps_rc_ctxt->rc_hdl, rc_pic_type);
                }
                else
                {
                    WORD32 i4_curr_qscale = ihevce_rc_get_scaled_mpeg2_qp(
                        ps_rc_lap_out->i4_L0_qp, ps_rc_ctxt->ps_rc_quant_ctxt);
                    /*Assume that 30% of header bits are constant and remaining are dependent on Qp
                    and map them accordingly*/
                    ps_rc_ctxt->i8_est_I_pic_header_bits = (LWORD64)(
                        (.3 * ps_rc_lap_out->i8_est_I_pic_header_bits +
                         (1. - .3) * ps_rc_lap_out->i8_est_I_pic_header_bits * i4_ref_qscale) /
                        i4_curr_qscale);
                }

                /*get qp for scene cut frame based on offline data*/
                index = ihevce_get_offline_index(
                    ps_rc_ctxt, ps_rc_lap_out->i4_num_pels_in_frame_considered);

                /*Sub pic rC bits extraction */
                i4_frame_qs_q3 = rc_get_qp_for_scd_frame(
                    ps_rc_ctxt->rc_hdl,
                    I_PIC,
                    ps_rc_lap_out->i8_frame_satd_act_accum,
                    ps_rc_lap_out->i4_num_pels_in_frame_considered,
                    (WORD32)ps_rc_ctxt->i8_est_I_pic_header_bits,
                    ps_rc_ctxt->ai4_lap_f_sim[i4_enc_frm_id],
                    (void *)&g_offline_i_model_coeff[index][0],
                    i_to_avg_bit_ratio,
                    1,
                    ps_rc_ctxt->af_sum_weigh[i4_enc_frm_id],
                    ps_rc_lap_out->ps_frame_info,
                    ps_rc_ctxt->i4_rc_pass,
                    (rc_pic_type != I_PIC),
                    ((ps_rc_lap_out->i4_rc_temporal_lyr_id != ps_rc_ctxt->i4_max_temporal_lyr) ||
                     (!ps_rc_ctxt->i4_max_temporal_lyr)),
                    1,
                    &i4_total_bits,
                    &i4_current_bits_estimated,
                    ps_rc_lap_out->i4_use_offline_model_2pass,
                    ai8_i_tex_bits,
                    af_i_qs,
                    i4_best_br_id,
                    &i4_estimate_to_calc_frm_error_temp);

                i4_hevc_frame_qp = ihevce_rc_get_scaled_hevc_qp_from_qs_q3(
                    i4_frame_qs_q3, ps_rc_ctxt->ps_rc_quant_ctxt);

                /*Get corresponding q scale*/
                i4_frame_qp =
                    ihevce_rc_get_scaled_mpeg2_qp(i4_hevc_frame_qp, ps_rc_ctxt->ps_rc_quant_ctxt);

                if(i4_hevc_frame_qp > ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp)
                    i4_hevc_frame_qp = ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp;

                {
                    WORD32 i4_init_qscale = ihevce_rc_get_scaled_mpeg2_qp(
                        ps_rc_lap_out->i4_L0_qp, ps_rc_ctxt->ps_rc_quant_ctxt);
                    f_percent_error = (float)(abs(i4_init_qscale - i4_frame_qp)) / i4_init_qscale;
                    if(f_percent_error < f_min_error)
                    {
                        f_min_error = f_percent_error;
                        i4_min_error_hevc_qp = i4_hevc_frame_qp;
                        i4_i_to_rest_ratio_final = i_to_avg_bit_ratio;
                        /*Get the bits estimated for least error*/
                        *pi4_tot_bits_estimated = i4_current_bits_estimated;
                        i4_estimate_to_calc_frm_error = i4_estimate_to_calc_frm_error_temp;
                    }
                    else
                    {}
                    ASSERT(*pi4_tot_bits_estimated != 0);
                }
                i4_count++;
                if(/*(ps_rc_lap_out->i4_L0_qp == i4_hevc_frame_qp) ||*/ (i4_count > 17))
                    break;
                ps_rc_lap_out->i4_L0_qp++;
            }
            ps_rc_lap_out->i4_L0_qp = i4_min_error_hevc_qp;

            i4_hevc_frame_qp = i4_min_error_hevc_qp;
            if(2 == i4_is_no_model_scd)
            {
                /* SGI & Enc Loop Parallelism related changes*/

                /*model reset not required if it is first frame*/
                if(ps_rc_ctxt->i4_is_first_frame_encoded && !i4_fade_scene &&
                   !ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id] &&
                   !ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] &&
                   !ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] &&
                   !ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i4_enc_frm_id])
                {
                    ps_rc_ctxt->ai4_is_frame_scd[i4_enc_frm_id] = 1;
                    /*reset all pic type is first frame encoded flag*/

                    ASSERT(pic_type == IV_IDR_FRAME || pic_type == IV_I_FRAME);
                }
                else if(ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id])
                {
                    rc_reset_first_frame_coded_flag(ps_rc_ctxt->rc_hdl, I_PIC);
                    ASSERT(rc_pic_type == I_PIC);
                    ASSERT(ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] == 0);
                }
                else if(
                    ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] ||
                    ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] ||
                    ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i4_enc_frm_id] || i4_fade_scene)
                {
                    /*Only when there are back to back scene cuts we need a non- Ipic will be marked as scene cut*/
                    /* Same path can also be followed during pause to resume detection to determine cur frame qp however handling during update is different*/
                    WORD32 i4_prev_qp, i, i4_new_qp_hevc_qp, I_hevc_qp, cur_hevc_qp;

                    /*both cannot be set at same time since lap cannot mark same frame as both scene cut and pause to resume flag*/
                    ASSERT(
                        (ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] &&
                         ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id]) == 0);

                    I_hevc_qp = i4_hevc_frame_qp;

                    /*alter ai4_prev_pic_hevc_qp so that qp restriction ll not let even other pictures temporary scd are thrashed*/
                    //if(ps_rc_lap_out->i4_rc_temporal_lyr_id != ps_rc_ctxt->i4_max_temporal_lyr)
                    {
                        if(ps_rc_ctxt->i4_field_pic == 0)
                        {
                            for(i = 1; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
                            {
                                i4_prev_qp = ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i];
                                i4_new_qp_hevc_qp = I_hevc_qp + i;
                                i4_new_qp_hevc_qp = ihevce_clip_min_max_qp(
                                    ps_rc_ctxt, i4_new_qp_hevc_qp, (picture_type_e)i, i - 1);
                                if(i4_prev_qp < i4_new_qp_hevc_qp)
                                {
                                    ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i] =
                                        i4_new_qp_hevc_qp;
                                }
                            }
                        }
                        else
                        { /*field case*/

                            for(i = 1; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
                            {
                                i4_prev_qp = ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i];
                                i4_new_qp_hevc_qp = I_hevc_qp + i;
                                i4_new_qp_hevc_qp = ihevce_clip_min_max_qp(
                                    ps_rc_ctxt, i4_new_qp_hevc_qp, (picture_type_e)i, i - 1);
                                if(i4_prev_qp < i4_new_qp_hevc_qp)
                                {
                                    ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i] =
                                        i4_new_qp_hevc_qp;
                                }

                                i4_prev_qp =
                                    ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i + FIELD_OFFSET];
                                i4_new_qp_hevc_qp = I_hevc_qp + i;
                                i4_new_qp_hevc_qp = ihevce_clip_min_max_qp(
                                    ps_rc_ctxt, i4_new_qp_hevc_qp, (picture_type_e)i, i - 1);
                                if(i4_prev_qp < i4_new_qp_hevc_qp)
                                {
                                    ps_rc_ctxt
                                        ->ai4_prev_pic_hevc_qp[i4_scene_num][i + FIELD_OFFSET] =
                                        i4_new_qp_hevc_qp;
                                }
                            }
                        }
                    }
                    {
                        WORD32 i4_updated_qp = ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i];
                        WORD32 i4_scale;

                        if(I_hevc_qp == i4_updated_qp)
                            i4_scale = 16;
                        else if(I_hevc_qp == (i4_updated_qp - 1))
                            i4_scale = 14;
                        else if(I_hevc_qp == (i4_updated_qp - 2))
                            i4_scale = 12;
                        else
                            i4_scale = 10;

                        *pi4_tot_bits_estimated = (i4_scale * (*pi4_tot_bits_estimated)) >> 4;
                        i4_estimate_to_calc_frm_error =
                            (i4_scale * i4_estimate_to_calc_frm_error) >> 4;
                    }
                    if(call_type == ENC_GET_QP)
                    {
                        ps_rc_lap_out->i8_est_text_bits = *pi4_tot_bits_estimated;
                    }
                    ASSERT(*pi4_tot_bits_estimated != 0);

                    /*use previous frame qp of same pic type or SCD i frame qp with offset whichever is maximum*/
                    /*For field case adding of  grater than 4 results in the qp increasing greatly when compared to previous pics/fields*/
                    if(rc_pic_type <= FIELD_OFFSET)
                        cur_hevc_qp = I_hevc_qp + rc_pic_type;
                    else
                        cur_hevc_qp = I_hevc_qp + (rc_pic_type - FIELD_OFFSET);

                    i4_prev_qp = ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type];

                    if((cur_hevc_qp < i4_prev_qp) && (ps_rc_ctxt->i4_num_active_pic_type > 2) &&
                       (is_first_frame_coded(ps_rc_ctxt->rc_hdl)) && (!i4_fade_scene))
                    {
                        cur_hevc_qp = i4_prev_qp;
                    }
                    i4_frame_qp =
                        ihevce_rc_get_scaled_mpeg2_qp(cur_hevc_qp, ps_rc_ctxt->ps_rc_quant_ctxt);
                    i4_hevc_frame_qp = cur_hevc_qp;
                    //ps_rc_ctxt->i4_is_non_I_scd_pic = 0;

                    rc_reset_first_frame_coded_flag(ps_rc_ctxt->rc_hdl, rc_pic_type);
                }
                else
                {}
            }
            if((1 == i4_is_no_model_scd) && (call_type == ENC_GET_QP))
            {
                WORD32 i4_clip_QP;
                i4_frame_qp_q6 =
                    clip_qp_based_on_prev_ref(ps_rc_ctxt->rc_hdl, rc_pic_type, 1, i4_scene_num);
                i4_clip_QP =
                    ihevce_rc_get_scaled_hevce_qp_q6(i4_frame_qp_q6, ps_rc_ctxt->u1_bit_depth);
                if(ps_rc_ctxt->i4_rc_pass != 2)
                {
                    i4_hevc_frame_qp = i4_clip_QP;
                }
                if((rc_pic_type == P_PIC) || (rc_pic_type == P1_PIC))
                {
                    *pi4_tot_bits_estimated = (*pi4_tot_bits_estimated * 11) >> 4; /* P picture*/
                    i4_estimate_to_calc_frm_error = (i4_estimate_to_calc_frm_error * 11) >> 4;
                }
                else if((rc_pic_type == B_PIC) || (rc_pic_type == BB_PIC))
                {
                    *pi4_tot_bits_estimated = (*pi4_tot_bits_estimated * 9) >> 4; /* B layer 1*/
                    i4_estimate_to_calc_frm_error = (i4_estimate_to_calc_frm_error * 9) >> 4;
                }
                else if((rc_pic_type == B1_PIC) || (rc_pic_type == B11_PIC))
                {
                    *pi4_tot_bits_estimated = (*pi4_tot_bits_estimated * 7) >> 4; /* B layer 2*/
                    i4_estimate_to_calc_frm_error = (i4_estimate_to_calc_frm_error * 7) >> 4;
                }
                else if((rc_pic_type == B2_PIC) || (rc_pic_type == B22_PIC))
                {
                    *pi4_tot_bits_estimated = (*pi4_tot_bits_estimated * 5) >> 4; /* B layer 3*/
                    i4_estimate_to_calc_frm_error = (i4_estimate_to_calc_frm_error * 5) >> 4;
                }
            }
            rc_add_est_tot(ps_rc_ctxt->rc_hdl, *pi4_tot_bits_estimated);
        }

        ASSERT(i4_hevc_frame_qp >= -ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset);

        /*constraint qp swing based on neighbour frames*/
        if(is_first_frame_coded(ps_rc_ctxt->rc_hdl))
        {
            if(ps_rc_ctxt->i4_field_pic == 0)
            {
                /*In dissolve case the p frame comes before an I pic and ref b comes after then what
                happens is b frame qp is restricted by the p frame qp so changed it to prev ref pic type*/
                if(rc_pic_type != I_PIC && rc_pic_type != P_PIC)
                {
                    if(ps_rc_lap_out->i4_rc_temporal_lyr_id == 1)
                    {
                        picture_type_e prev_ref_pic_type =
                            rc_getprev_ref_pic_type(ps_rc_ctxt->rc_hdl);

                        if(i4_hevc_frame_qp >
                           ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][prev_ref_pic_type] + 3)
                        {
                            if(ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][prev_ref_pic_type] >
                               0)
                                i4_hevc_frame_qp =
                                    ps_rc_ctxt
                                        ->ai4_prev_pic_hevc_qp[i4_scene_num][prev_ref_pic_type] +
                                    3;
                        }
                    }
                    else if(
                        i4_hevc_frame_qp >
                        (ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type - 1] + 3))
                    {
                        /*allow max of +3 compared to previous frame*/
                        if(ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type - 1] > 0)
                            i4_hevc_frame_qp =
                                ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type - 1] + 3;
                    }
                }

                if((rc_pic_type != I_PIC && rc_pic_type != P_PIC) &&
                   (i4_hevc_frame_qp <
                    ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type - 1]))
                {
                    i4_hevc_frame_qp =
                        ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type - 1];
                }

                /** Force non-ref B pic qp to be ref_B_PIC_qp - 1. This is not valid for when max teporla later is less than 2*/
                if(temporal_layer_id == ps_rc_ctxt->i4_max_temporal_lyr &&
                   ps_rc_ctxt->i4_max_temporal_lyr > 1)
                {
                    i4_hevc_frame_qp =
                        ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type - 1] + 1;
                }
            }
            else /*for field case*/
            {
                if(ps_rc_lap_out->i4_rc_temporal_lyr_id >= 1)
                {
                    /*To make the comparison of qp with the top field's of previous layer tempor layer id matches with the pic type. */
                    if(i4_hevc_frame_qp >
                       ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num]
                                                       [ps_rc_lap_out->i4_rc_temporal_lyr_id] +
                           3)
                    {
                        /*allow max of +3 compared to previous frame*/
                        if(0 <
                           ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num]
                                                           [ps_rc_lap_out->i4_rc_temporal_lyr_id])
                            i4_hevc_frame_qp =
                                ps_rc_ctxt
                                    ->ai4_prev_pic_hevc_qp[i4_scene_num]
                                                          [ps_rc_lap_out->i4_rc_temporal_lyr_id] +
                                3;
                    }
                    if(i4_hevc_frame_qp <
                       ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num]
                                                       [ps_rc_lap_out->i4_rc_temporal_lyr_id])
                    {
                        i4_hevc_frame_qp =
                            ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num]
                                                            [ps_rc_lap_out->i4_rc_temporal_lyr_id];
                    }

                    /** Force non-ref B pic qp to be ref_B_PIC_qp - 1. This is not valid for when max teporla later is less than 2*/
                    if(temporal_layer_id == ps_rc_ctxt->i4_max_temporal_lyr &&
                       ps_rc_ctxt->i4_max_temporal_lyr > 1)
                    {
                        i4_hevc_frame_qp =
                            ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num]
                                                            [ps_rc_lap_out->i4_rc_temporal_lyr_id] +
                            1;
                    }
                }
                /** At lower range qp swing for same pic type is also imposed to make sure
                    qp does not fall from 10 to 4 since they differ by only one q scale*/
            }
        }

        /**clip to min qp which is user configurable*/
        i4_hevc_frame_qp = ihevce_clip_min_max_qp(
            ps_rc_ctxt, i4_hevc_frame_qp, rc_pic_type, ps_rc_lap_out->i4_rc_temporal_lyr_id);

#if 1  //FRAME_PARALLEL_LVL
        ps_rc_ctxt->i4_est_text_bits_ctr_get_qp++;  //ELP_RC
        ps_rc_ctxt->i4_est_text_bits_ctr_get_qp =
            (ps_rc_ctxt->i4_est_text_bits_ctr_get_qp % (ps_rc_ctxt->i4_num_frame_parallel));
#endif
        /** the estimates are reset only duing enc call*/

#if USE_USER_FIRST_FRAME_QP
        /*I_PIC check is necessary coz pre-enc can query for qp even before first frame update has happened*/
        if(!ps_rc_ctxt->i4_is_first_frame_encoded && rc_pic_type == I_PIC)
        {
            i4_hevc_frame_qp = ps_rc_ctxt->i4_init_frame_qp_user;
            DBG_PRINTF("FIXED START QP PATH *************************\n");
        }
#endif
    }

    if(CONST_QP != e_rc_type)
    {
        ASSERT(*pi4_tot_bits_estimated != 0);
    }

    ps_rc_ctxt->s_rc_high_lvl_stat.i4_finalQP = i4_hevc_frame_qp;
    if(ps_rc_lap_out->i4_is_model_valid)
    {
        get_bits_for_final_qp(
            ps_rc_ctxt->rc_hdl,
            &ps_rc_ctxt->s_rc_high_lvl_stat.i4_modelQP,
            &ps_rc_ctxt->s_rc_high_lvl_stat.i4_maxEbfQP,
            &ps_rc_ctxt->s_rc_high_lvl_stat.i8_bits_from_finalQP,
            i4_hevc_frame_qp,
            ihevce_rc_get_scaled_mpeg2_qp_q6(
                i4_hevc_frame_qp + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset,
                ps_rc_ctxt->u1_bit_depth),
            i4_cur_est_header_bits,
            i4_est_tex_bits,
            i4_vbv_buf_max_bits,
            rc_pic_type,
            ps_rc_lap_out->i4_rc_display_num);
    }
    i4_deltaQP = ihevce_ebf_based_rc_correction_to_avoid_overflow(
        ps_rc_ctxt, ps_rc_lap_out, pi4_tot_bits_estimated);
    i4_hevc_frame_qp += i4_deltaQP;

    /**clip to min qp which is user configurable*/
    i4_hevc_frame_qp = ihevce_clip_min_max_qp(
        ps_rc_ctxt, i4_hevc_frame_qp, rc_pic_type, ps_rc_lap_out->i4_rc_temporal_lyr_id);

    /*set estimate status for frame level error calculation*/
    if(i4_estimate_to_calc_frm_error > 0)
    {
        rc_set_estimate_status(
            ps_rc_ctxt->rc_hdl,
            i4_estimate_to_calc_frm_error - ps_rc_ctxt->i8_est_I_pic_header_bits,
            ps_rc_ctxt->i8_est_I_pic_header_bits,
            ps_rc_ctxt->i4_est_text_bits_ctr_get_qp);
    }
    else
    {
        rc_set_estimate_status(
            ps_rc_ctxt->rc_hdl,
            -1,
            ps_rc_ctxt->i8_est_I_pic_header_bits,
            ps_rc_ctxt->i4_est_text_bits_ctr_get_qp);
    }

    ps_rc_lap_out->i8_est_text_bits = *pi4_tot_bits_estimated;

    /*B pictures which are in fades will take the highest QP of either side of P pics*/
    if(ps_rc_lap_out->i4_rc_pic_type == IV_B_FRAME &&
       (ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_FADE_IN ||
        ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_FADE_OUT))
    {
        i4_hevc_frame_qp =
            MAX(ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[0], ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[1]);
    }

    /*saving the last two pics of layer 0*/
    if(0 == ps_rc_lap_out->i4_rc_temporal_lyr_id)
    {
        ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[1] = ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[0];
        ps_rc_ctxt->ai4_last_tw0_lyr0_pic_qp[0] = i4_hevc_frame_qp;
    }

    return i4_hevc_frame_qp;
}

/*##########################################################*/
/******* END OF ENC THRD QP QUERY FUNCTIONS ****************/
/*########################################################*/

/*####################################################*/
/******* START OF I2AVG RATIO FUNCTIONS **************/
/*##################################################*/

/**
******************************************************************************
*
*  @brief function to get i_to_avg_rest at scene cut frame based on data available from LAP
*
*  @par   Description
*
*  @param[in]   pv_rc_ctxt
*               void pointer to rc ctxt
*  @param[in]   ps_rc_lap_out : pointer to lap out structure
*  @param[in]   i4_update_delay : The Delay in the update. This can happen for dist. case!
*               All decision should consider this delay for updation!
*  @return      WORD32 i_to_rest bit ratio
*
******************************************************************************
*/
float ihevce_get_i_to_avg_ratio(
    void *pv_rc_ctxt,
    rc_lap_out_params_t *ps_rc_lap_out,
    WORD32 i_to_p_qp_offset,
    WORD32 i4_offset_flag,
    WORD32 i4_call_type,
    WORD32 ai4_qp_offsets[4],
    WORD32 i4_update_delay)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    WORD32 i = 0, k = 0, num_frames_in_lap[MAX_PIC_TYPE] = { 0 }, ai4_pic_dist[MAX_PIC_TYPE],
           ai4_pic_dist_in_cur_gop[MAX_PIC_TYPE] = { 0 };
    WORD32 i4_num_b, i4_num_frms_traversed_in_lap = 0, total_frms_considered = 0,
                     i4_flag_i_frame_exit = 0, u4_rc_scene_number;
    rc_lap_out_params_t *ps_cur_rc_lap_out = ps_rc_lap_out;

    rc_lap_out_params_t *ps_cur_rc_lap_out_I = ps_rc_lap_out;
    double complexity[MAX_PIC_TYPE] = { 0 }, d_first_i_complexity = 0, d_first_p_complexity = 0.0f,
           cur_lambda_modifer, den = 0, average_intra_complexity = 0;
    double i_frm_lambda_modifier;
    float i_to_rest_bit_ratio = 8.00;
    picture_type_e curr_rc_pic_type;
    LWORD64 i8_l1_analysis_lap_comp = 0;
    WORD32 i4_intra_frame_interval = rc_get_intra_frame_interval(ps_rc_ctxt->rc_hdl);
    UWORD32 u4_L1_based_lap_complexity_q7 = 0;
    WORD32 i4_frame_qp = 0, i4_I_frame_qp = 0;

    WORD32 ai4_lambda_offsets[5] = { -3, -2, 2, 6, 7 };
    /* The window for which your update is guaranteed */
    WORD32 updated_window = ps_rc_ctxt->i4_num_frame_in_lap_window - i4_update_delay;

    ASSERT(ps_rc_ctxt->i4_rc_pass != 2);
    rc_get_pic_distribution(ps_rc_ctxt->rc_hdl, &ai4_pic_dist[0]);

    if(ps_rc_ctxt->i4_max_temporal_lyr)
    {
        i4_num_b = ((WORD32)pow((float)2, ps_rc_ctxt->i4_max_temporal_lyr)) - 1;
    }
    else
    {
        i4_num_b = 0;
    }
    i_frm_lambda_modifier = ihevce_get_frame_lambda_modifier((WORD8)I_PIC, 0, 1, 1, i4_num_b);
    /* check should be wrt inter frame interval*/
    /*If lap frames are not sufficient return default ratio*/
    u4_rc_scene_number = ps_cur_rc_lap_out_I->u4_rc_scene_num;

    if(updated_window < 4)
    {
        return i_to_rest_bit_ratio;
    }

    k = 0;
    if(ps_cur_rc_lap_out != NULL)
    {
        WORD32 i4_temp_frame_qp;

        if(ps_cur_rc_lap_out->i4_L0_qp == -1)
        {
            i4_frame_qp = ps_cur_rc_lap_out->i4_L1_qp;
            i4_I_frame_qp = ps_cur_rc_lap_out->i4_L1_qp - 3;
        }
        else
        {
            i4_frame_qp = ps_cur_rc_lap_out->i4_L0_qp;
            i4_I_frame_qp = ps_cur_rc_lap_out->i4_L0_qp - 3;
        }

        do
        {
            curr_rc_pic_type = ihevce_rc_conv_pic_type(
                (IV_PICTURE_CODING_TYPE_T)ps_cur_rc_lap_out->i4_rc_pic_type,
                ps_rc_ctxt->i4_field_pic,
                ps_cur_rc_lap_out->i4_rc_temporal_lyr_id,
                ps_cur_rc_lap_out->i4_is_bottom_field,
                ps_rc_ctxt->i4_top_field_first);
            cur_lambda_modifer = ihevce_get_frame_lambda_modifier(
                (WORD8)curr_rc_pic_type,
                ps_cur_rc_lap_out->i4_rc_temporal_lyr_id,
                1,
                ps_cur_rc_lap_out->i4_rc_is_ref_pic,
                i4_num_b);
            if(curr_rc_pic_type == I_PIC)
            {
                i4_temp_frame_qp = i4_frame_qp + ai4_lambda_offsets[curr_rc_pic_type];
            }
            else
            {
                i4_temp_frame_qp =
                    i4_frame_qp + ai4_lambda_offsets[ps_cur_rc_lap_out->i4_rc_temporal_lyr_id + 1];
                i4_temp_frame_qp =
                    i4_temp_frame_qp +
                    ps_cur_rc_lap_out->ai4_offsets[ps_cur_rc_lap_out->i4_rc_temporal_lyr_id + 1];
            }

            i4_temp_frame_qp = CLIP3(i4_temp_frame_qp, 1, 51);
            i4_I_frame_qp = CLIP3(i4_I_frame_qp, 1, 51);

            if(curr_rc_pic_type == I_PIC)
            {
                complexity[I_PIC] += (double)ps_cur_rc_lap_out->ai8_pre_intra_sad[i4_I_frame_qp];
                if(total_frms_considered == 0)
                    d_first_i_complexity =
                        (double)ps_cur_rc_lap_out->ai8_pre_intra_sad[i4_I_frame_qp];

                num_frames_in_lap[I_PIC]++;
                i8_l1_analysis_lap_comp +=
                    (LWORD64)(1.17 * ps_cur_rc_lap_out->i8_raw_pre_intra_sad);
            }
            else
            {
                if((num_frames_in_lap[P_PIC] == 0) && (curr_rc_pic_type == P_PIC))
                    d_first_p_complexity =
                        (double)ps_cur_rc_lap_out->ai8_pre_intra_sad[i4_I_frame_qp];

                if(total_frms_considered == 0)
                {
                    num_frames_in_lap[I_PIC]++;
                    {
                        complexity[I_PIC] +=
                            (double)ps_cur_rc_lap_out->ai8_pre_intra_sad[i4_I_frame_qp];
                        d_first_i_complexity =
                            (double)ps_cur_rc_lap_out->ai8_pre_intra_sad[i4_I_frame_qp];
                    }
                }
                else
                {
                    /*SAD is scaled according the lambda parametrs use to make it proportional to bits consumed in the end*/
#if !USE_SQRT
                    //complexity[curr_rc_pic_type] += (double)(MIN(ps_cur_rc_lap_out->ai8_frame_acc_coarse_me_sad[i4_temp_frame_qp],ps_cur_rc_lap_out->i8_pre_intra_sad)/(/*(cur_lambda_modifer/i_frm_lambda_modifier) * */pow(1.125,(ps_rc_lap_out->i4_rc_temporal_lyr_id + 1/*i_to_p_qp_offset*/))));
                    if((curr_rc_pic_type > P_PIC) &&
                       (ps_rc_lap_out->i4_rc_quality_preset == IHEVCE_QUALITY_P6))
                        complexity[curr_rc_pic_type] +=
                            (double)(ps_cur_rc_lap_out->ai8_frame_acc_coarse_me_sad
                                         [i4_temp_frame_qp]);  // /(/*(cur_lambda_modifer/i_frm_lambda_modifier) * */pow(1.125,(ps_rc_lap_out->i4_rc_temporal_lyr_id + 1/*i_to_p_qp_offset*/))));
                    else
                        complexity[curr_rc_pic_type] += (double)(MIN(
                            ps_cur_rc_lap_out->ai8_frame_acc_coarse_me_sad[i4_temp_frame_qp],
                            ps_cur_rc_lap_out->ai8_pre_intra_sad
                                [i4_temp_frame_qp]));  ///(/*(cur_lambda_modifer/i_frm_lambda_modifier) * */pow(1.125,(ps_rc_lap_out->i4_rc_temporal_lyr_id + 1/*i_to_p_qp_offset*/))));

#else
                    complexity[curr_rc_pic_type] +=
                        MIN(ps_cur_rc_lap_out->ai8_frame_acc_coarse_me_sad[i4_temp_frame_qp],
                            ps_cur_rc_lap_out->i8_pre_intra_sad) /
                        (sqrt(cur_lambda_modifer / i_frm_lambda_modifier) *
                         pow(1.125, (ps_rc_lap_out->i4_rc_temporal_lyr_id + 1)));
#endif
                    num_frames_in_lap[curr_rc_pic_type]++;
                }
                i8_l1_analysis_lap_comp += (LWORD64)(
                    (float)ps_cur_rc_lap_out->i8_raw_l1_coarse_me_sad /
                    pow(1.125, curr_rc_pic_type));
            }

            if(ps_rc_lap_out->i4_rc_quality_preset == IHEVCE_QUALITY_P6)
            {
                if(curr_rc_pic_type < B_PIC)
                {
                    /*accumulate average intra sad*/
                    average_intra_complexity +=
                        ps_cur_rc_lap_out
                            ->ai8_pre_intra_sad[i4_I_frame_qp] /*/i_frm_lambda_modifier*/;
                    i4_num_frms_traversed_in_lap++;
                }
            }
            else
            {
                /*accumulate average intra sad*/
                average_intra_complexity +=
                    ps_cur_rc_lap_out->ai8_pre_intra_sad[i4_I_frame_qp] /*/i_frm_lambda_modifier*/;
                i4_num_frms_traversed_in_lap++;
            }

            ai4_pic_dist_in_cur_gop[curr_rc_pic_type]++;
            i++;
            total_frms_considered++;
            i4_num_frms_traversed_in_lap++;
            ps_cur_rc_lap_out = (rc_lap_out_params_t *)ps_cur_rc_lap_out->ps_rc_lap_out_next_encode;

            if((ps_cur_rc_lap_out == NULL) ||
               ((total_frms_considered + k) == i4_intra_frame_interval) || (i >= updated_window))
            {
                break;
            }

            if((i >= (ps_rc_ctxt->i4_next_sc_i_in_rc_look_ahead - k) ||
                (ps_cur_rc_lap_out->i4_rc_pic_type == IV_I_FRAME) ||
                (ps_cur_rc_lap_out->i4_rc_pic_type == IV_IDR_FRAME)) &&
               (i4_offset_flag == 1))
            {
                break;
            }
            /*If an I frame enters the lookahead it can cause bit allocation to go bad
            if corresponding p/b frames are absent*/
            if(((total_frms_considered + k) > (WORD32)(0.75f * i4_intra_frame_interval)) &&
               ((ps_cur_rc_lap_out->i4_rc_pic_type == IV_I_FRAME) ||
                (ps_cur_rc_lap_out->i4_rc_pic_type == IV_IDR_FRAME)))
            {
                i4_flag_i_frame_exit = 1;
                break;
            }

        } while(1);

        if(total_frms_considered > 0)
        {
            float lap_L1_comp =
                (float)i8_l1_analysis_lap_comp /
                (total_frms_considered * ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width);

            lap_L1_comp = rc_get_offline_normalized_complexity(
                ps_rc_ctxt->u4_intra_frame_interval,
                ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width,
                lap_L1_comp,
                ps_rc_ctxt->i4_rc_pass);

            u4_L1_based_lap_complexity_q7 = (WORD32)((lap_L1_comp * (1 << 7)) + .05f);
        }
        else
        {
            u4_L1_based_lap_complexity_q7 = 25;
        }

        if(i4_call_type == 1)
        {
            if(num_frames_in_lap[0] > 0)
            {
                float f_curr_i_to_sum = (float)(d_first_i_complexity / complexity[0]);
                f_curr_i_to_sum = CLIP3(f_curr_i_to_sum, 0.1f, 100.0f);
                rc_set_i_to_sum_api_ba(ps_rc_ctxt->rc_hdl, f_curr_i_to_sum);
            }
        }

        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            if(num_frames_in_lap[i] > 0)
            {
                complexity[i] = complexity[i] / num_frames_in_lap[i];
            }
        }
        /*for non - I scd case it is possible that entire LAP window might not have intra picture. Consider average intra sad when
        atleast one I pic is not available*/
        if(num_frames_in_lap[I_PIC] == 0)
        {
            ASSERT(i4_num_frms_traversed_in_lap);
            complexity[I_PIC] = average_intra_complexity / i4_num_frms_traversed_in_lap;
        }
        /*get picture type distribution in LAP*/
        if(num_frames_in_lap[I_PIC] == 0)
        {
            rc_get_pic_distribution(ps_rc_ctxt->rc_hdl, &ai4_pic_dist[0]);
        }
        else
        {
            memmove(ai4_pic_dist, num_frames_in_lap, sizeof(WORD32) * MAX_PIC_TYPE);
        }

        {
            WORD32 num_inter_pic = 0;
            for(i = 1; i < MAX_PIC_TYPE; i++)
            {
                den += complexity[i] * ai4_pic_dist[i];
            }

            for(i = 1; i < MAX_PIC_TYPE; i++)
            {
                num_inter_pic += ai4_pic_dist[i];
            }
            if(num_inter_pic > 0)
                den = den / num_inter_pic;
            else
                den = 0.0;
        }

        if(den > 0)
            i_to_rest_bit_ratio = (float)((complexity[I_PIC]) / den);
        else
            i_to_rest_bit_ratio = 15;

        if((total_frms_considered < (WORD32)(0.75f * i4_intra_frame_interval)) &&
           (total_frms_considered < (updated_window - 1)) &&
           ((UWORD32)total_frms_considered < ((ps_rc_ctxt->u4_max_frame_rate / 1000))))
        {
            /*This GOP will only sustain for few frames hence have strict restriction for I to rest ratio*/
            if(i_to_rest_bit_ratio > 12)
                i_to_rest_bit_ratio = 12;

            if(i_to_rest_bit_ratio > 8 &&
               total_frms_considered < (ps_rc_ctxt->i4_max_inter_frm_int * 2))
                i_to_rest_bit_ratio = 8;
        }
    }

    if((i4_call_type == 1) && (i_to_rest_bit_ratio < I_TO_REST_VVFAST) && (i4_offset_flag == 1))
    {
        float f_p_to_i_ratio = (float)(d_first_p_complexity / d_first_i_complexity);
        if(ps_rc_lap_out->i8_frame_satd_act_accum <
           (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width * 1.5f))
            rc_set_p_to_i_complexity_ratio(ps_rc_ctxt->rc_hdl, f_p_to_i_ratio);
    }

    /*Reset the pic distribution if I frame exit was encountered*/

    if(ps_rc_ctxt->e_rate_control_type != CONST_QP)
    {
        rc_get_pic_distribution(ps_rc_ctxt->rc_hdl, &ai4_pic_dist[0]);
        if((ai4_pic_dist_in_cur_gop[I_PIC] > 1) && (ai4_pic_dist[0] == 1))
        {
            i4_flag_i_frame_exit = 1;
        }
        if(i4_flag_i_frame_exit && (i4_call_type == 1))
        {
            if(ai4_pic_dist_in_cur_gop[I_PIC] == 0)
                memmove(ai4_pic_dist_in_cur_gop, num_frames_in_lap, sizeof(WORD32) * MAX_PIC_TYPE);

            rc_update_pic_distn_lap_to_rc(ps_rc_ctxt->rc_hdl, ai4_pic_dist_in_cur_gop);
            rc_set_bits_based_on_complexity(
                ps_rc_ctxt->rc_hdl, u4_L1_based_lap_complexity_q7, total_frms_considered);
        }
    }

    return i_to_rest_bit_ratio;
}

/*##################################################*/
/******* END OF I2AVG RATIO FUNCTIONS **************/
/*################################################*/

/*#########################################################*/
/******* START OF QSCALE CONVERSION FUNCTIONS *************/
/*########################################################*/

/**
******************************************************************************
*
*  @brief function to convert from qscale to qp
*
*  @par   Description
*  @param[in]   i4_frame_qs_q3 : QP value in qscale
*   return      frame qp
******************************************************************************
*/

WORD32 ihevce_rc_get_scaled_hevc_qp_from_qs_q3(WORD32 i4_frame_qs_q3, rc_quant_t *ps_rc_quant_ctxt)
{
    if(i4_frame_qs_q3 > ps_rc_quant_ctxt->i2_max_qscale)
    {
        i4_frame_qs_q3 = ps_rc_quant_ctxt->i2_max_qscale;
    }
    else if(i4_frame_qs_q3 < ps_rc_quant_ctxt->i2_min_qscale)
    {
        i4_frame_qs_q3 = ps_rc_quant_ctxt->i2_min_qscale;
    }

    return (ps_rc_quant_ctxt->pi4_qscale_to_qp[i4_frame_qs_q3]);
}

/**
******************************************************************************
*
*  @brief function to convert from qp to qscale
*
*  @par   Description
*  @param[in]   i4_frame_qp : QP value
*   return      value in qscale
******************************************************************************
*/
WORD32 ihevce_rc_get_scaled_mpeg2_qp(WORD32 i4_frame_qp, rc_quant_t *ps_rc_quant_ctxt)
{
    //i4_frame_qp = i4_frame_qp >> 3; // Q3 format is mantained for accuarate calc at lower qp
    WORD32 i4_qscale;
    if(i4_frame_qp > ps_rc_quant_ctxt->i2_max_qp)
    {
        i4_frame_qp = ps_rc_quant_ctxt->i2_max_qp;
    }
    else if(i4_frame_qp < ps_rc_quant_ctxt->i2_min_qp)
    {
        i4_frame_qp = ps_rc_quant_ctxt->i2_min_qp;
    }

    i4_qscale = (ps_rc_quant_ctxt->pi4_qp_to_qscale[i4_frame_qp + ps_rc_quant_ctxt->i1_qp_offset] +
                 (1 << (QSCALE_Q_FAC_3 - 1))) >>
                QSCALE_Q_FAC_3;
    return i4_qscale;
}

/**
******************************************************************************
*
*  @brief function to convert from qp to qscale
*
*  @par Description : This function maps logarithmic QP values to linear QP
*   values. The linear values are represented in Q6 format.
*
*  @param[in]   i4_frame_qp : QP value (log scale)
*
*  @return value in QP (linear scale)
*
******************************************************************************
*/
WORD32 ihevce_rc_get_scaled_mpeg2_qp_q6(WORD32 i4_frame_qp, UWORD8 u1_bit_depth)
{
    WORD32 i4_frame_qp_q6;
    number_t s_frame_qp;
    float f_qp;

    (void)u1_bit_depth;
    ASSERT(i4_frame_qp >= 0);
    ASSERT(i4_frame_qp <= 51 + ((u1_bit_depth - 8) * 6));
    f_qp = (float)pow((float)2, ((float)(i4_frame_qp - 4) / 6));
    convert_float_to_fix(f_qp, &s_frame_qp);
    convert_varq_to_fixq(s_frame_qp, &i4_frame_qp_q6, QSCALE_Q_FAC);

    if(i4_frame_qp_q6 < (1 << QSCALE_Q_FAC))
        i4_frame_qp_q6 = 1 << QSCALE_Q_FAC;

    return i4_frame_qp_q6;
}

/**
******************************************************************************
*
*  @brief function to convert from qscale to qp
*
*  @par   Description
*  @param[in]   i4_frame_qp_q6 : QP value in qscale. the input is assumed to be in q6 format
*   return      frame qp
******************************************************************************
*/
WORD32 ihevce_rc_get_scaled_hevce_qp_q6(WORD32 i4_frame_qp_q6, UWORD8 u1_bit_depth)
{
    WORD32 i4_hevce_qp;
    number_t s_hevce_qp, s_temp;
    float f_mpeg2_qp, f_hevce_qp;
    f_mpeg2_qp = (float)i4_frame_qp_q6 / (1 << QSCALE_Q_FAC);
    f_hevce_qp = (6 * ((float)log(f_mpeg2_qp) / (float)log((float)2))) + 4;
    convert_float_to_fix(f_hevce_qp, &s_hevce_qp);

    /*rounf off to nearest integer*/
    s_temp.sm = 1;
    s_temp.e = 1;
    add32_var_q(s_hevce_qp, s_temp, &s_hevce_qp);
    number_t_to_word32(s_hevce_qp, &i4_hevce_qp);
    if(i4_frame_qp_q6 == 0)
    {
        i4_hevce_qp = 0;
    }

    i4_hevce_qp -= ((u1_bit_depth - 8) * 6);

    return i4_hevce_qp;
}

/**
******************************************************************************
*
*  @brief function to convert from qp scale to qp
*
*  @par Description : This function maps linear QP values to logarithimic QP
*   values. The linear values are represented in Q3 format.
*
*  @param[in]   i4_frame_qp : QP value (linear scale, Q3 mode)
*
*  @return value in QP (log scale)
*
******************************************************************************
*/
WORD32 ihevce_rc_get_scaled_hevce_qp_q3(WORD32 i4_frame_qp, UWORD8 u1_bit_depth)
{
    WORD32 i4_hevce_qp;
    number_t s_hevce_qp, s_temp;

    if(i4_frame_qp == 0)
    {
        i4_hevce_qp = 0;
    }
    else
    {
        float f_mpeg2_qp, f_hevce_qp;

        f_mpeg2_qp = (float)i4_frame_qp;
        f_hevce_qp = (6 * ((float)log(f_mpeg2_qp) / (float)log((float)2) - 3)) + 4;
        convert_float_to_fix(f_hevce_qp, &s_hevce_qp);

        /*rounf off to nearest integer*/
        s_temp.sm = 1;
        s_temp.e = 1;
        add32_var_q(s_hevce_qp, s_temp, &s_hevce_qp);
        number_t_to_word32(s_hevce_qp, &i4_hevce_qp);
    }
    i4_hevce_qp -= ((u1_bit_depth - 8) * 6);

    return i4_hevce_qp;
}

/*#######################################################*/
/******* END OF QSCALE CONVERSION FUNCTIONS *************/
/*######################################################*/

/*###############################################*/
/******* START OF SET,GET FUNCTIONS *************/
/*#############################################*/

/**
******************************************************************************
*
*  @brief Convert pic type to rc pic type
*
*  @par   Description
*
*
*  @param[in]      pic_type
*  Pic type
*
*  @return      rc_pic_type
*
******************************************************************************
*/
picture_type_e ihevce_rc_conv_pic_type(
    IV_PICTURE_CODING_TYPE_T pic_type,
    WORD32 i4_field_pic,
    WORD32 i4_temporal_layer_id,
    WORD32 i4_is_bottom_field,
    WORD32 i4_top_field_first)
{
    picture_type_e rc_pic_type = pic_type;
    /*interlaced pictype are not supported*/
    if(pic_type > 9 && i4_temporal_layer_id > 3) /**/
    {
        DBG_PRINTF("unsupported picture type or temporal id\n");
        exit(0);
    }

    if(i4_field_pic == 0) /*Progressive Source*/
    {
        if(pic_type == IV_IDR_FRAME)
        {
            rc_pic_type = I_PIC;
        }
        else
        {
            rc_pic_type = (picture_type_e)pic_type;

            /*return different picture type based on temporal layer*/
            if(i4_temporal_layer_id > 1)
            {
                rc_pic_type = (picture_type_e)(pic_type + (i4_temporal_layer_id - 1));
            }
        }
    }

    else if(i4_field_pic == 1)
    {
        if(pic_type == IV_IDR_FRAME || pic_type == IV_I_FRAME)
        {
            rc_pic_type = I_PIC;
        }

        else if(i4_top_field_first == 1)
        {
            rc_pic_type = (picture_type_e)pic_type;

            if(i4_temporal_layer_id <= 1)

            {
                if(i4_is_bottom_field == 1)
                    rc_pic_type = (picture_type_e)(pic_type + 4);
            }
            /*return different picture type based on temporal layer*/
            if(i4_temporal_layer_id > 1)
            {
                if(i4_is_bottom_field == 0)
                    rc_pic_type = (picture_type_e)(pic_type + (i4_temporal_layer_id - 1));
                else
                    rc_pic_type = (picture_type_e)(
                        pic_type + (i4_temporal_layer_id - 1) +
                        4); /*Offset of 4 for the bottomfield*/
            }
        }
        else if(i4_top_field_first == 0)
        {
            rc_pic_type = (picture_type_e)pic_type;

            if(i4_temporal_layer_id <= 1)
            {
                if(i4_is_bottom_field == 1)
                    rc_pic_type = (picture_type_e)(pic_type + 4);
            }
            /*return different picture type based on temporal layer*/
            if(i4_temporal_layer_id > 1)
            {
                if(i4_is_bottom_field == 0)
                    rc_pic_type = (picture_type_e)(pic_type + (i4_temporal_layer_id - 1));
                else
                    rc_pic_type = (picture_type_e)(
                        pic_type + (i4_temporal_layer_id - 1) + 4); /*Offset of 4 for the topfield*/
            }
        }
    }

    return rc_pic_type;
}

/**
******************************************************************************
*
*  @brief function to update current frame intra cost
*
*  @par   Description
*  @param[inout] ps_rc_ctxt
*  @param[in]    i8_cur_frm_intra_cost
******************************************************************************
*/
void ihevce_rc_update_cur_frm_intra_satd(
    void *pv_ctxt, LWORD64 i8_cur_frm_intra_cost, WORD32 i4_enc_frm_id)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    ps_rc_ctxt->ai8_cur_frm_intra_cost[i4_enc_frm_id] = i8_cur_frm_intra_cost;
}
/**
******************************************************************************
*
*  @brief function to return scene type
*
*  @par   Description
*  @param[inout] ps_rc_lap_out
*  @return       i4_rc_scene_type
******************************************************************************
*/
/* Functions dependent on lap input*/
WORD32 ihevce_rc_lap_get_scene_type(rc_lap_out_params_t *ps_rc_lap_out)
{
    return (WORD32)ps_rc_lap_out->i4_rc_scene_type;
}

/**
******************************************************************************
*
*  @name  ihevce_rc_get_pic_param
*
*  @par   Description
*
*  @param[in]   rc_pic_type
*
*  @return      void
*
******************************************************************************
*/
static void ihevce_rc_get_pic_param(
    picture_type_e rc_pic_type, WORD32 *pi4_tem_lyr, WORD32 *pi4_is_bottom_field)
{
    /*bottom field determination*/
    if(rc_pic_type >= P1_PIC)
        *pi4_is_bottom_field = 1;
    else
        *pi4_is_bottom_field = 0;

    /*temporal lyr id determination*/
    if(rc_pic_type == I_PIC || rc_pic_type == P_PIC || rc_pic_type == P1_PIC)
    {
        *pi4_tem_lyr = 0;
    }
    else if(rc_pic_type == B_PIC || rc_pic_type == BB_PIC)
    {
        *pi4_tem_lyr = 1;
    }
    else if(rc_pic_type == B1_PIC || rc_pic_type == B11_PIC)
    {
        *pi4_tem_lyr = 2;
    }
    else if(rc_pic_type == B2_PIC || rc_pic_type == B22_PIC)
    {
        *pi4_tem_lyr = 3;
    }
    else
    {
        ASSERT(0);
    }
}
/**
******************************************************************************
*
*  @name  ihevce_get_offline_index
*
*  @par   Description
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*
*  @return      index
*
******************************************************************************
*/
static WORD32 ihevce_get_offline_index(rc_context_t *ps_rc_ctxt, WORD32 i4_num_pels_in_frame)
{
    WORD32 i4_rc_quality_preset = ps_rc_ctxt->i4_quality_preset;
    WORD32 base = 1;
    if(i4_num_pels_in_frame > 5000000) /*ultra HD*/
    {
        base = 0;
    }
    else if(i4_num_pels_in_frame > 1500000) /*Full HD*/
    {
        base = 5;
    }
    else if(i4_num_pels_in_frame > 600000) /*720p*/
    {
        base = 10;
    }
    else /*SD*/
    {
        base = 15;
    }
    /*based on preset choose coeff*/
    if(i4_rc_quality_preset == IHEVCE_QUALITY_P0) /*Pristine quality*/
    {
        return base;
    }
    else if(i4_rc_quality_preset == IHEVCE_QUALITY_P2) /*High quality*/
    {
        return base + 1;
    }
    else if(
        (i4_rc_quality_preset == IHEVCE_QUALITY_P5) ||
        (i4_rc_quality_preset == IHEVCE_QUALITY_P6)) /*Extreme speed */
    {
        return base + 4;
    }
    else if(i4_rc_quality_preset == IHEVCE_QUALITY_P4) /*High speed */
    {
        return base + 3;
    }
    else if(i4_rc_quality_preset == IHEVCE_QUALITY_P3) /*default assume Medium speed*/
    {
        return base + 2;
    }
    else
    {
        ASSERT(0);
    }
    return base + 2;
}

/**
******************************************************************************
*
*  @name  ihevce_get_frame_lambda_modifier
*
*  @par   Description
*
*  @param[in]   pic_type
*               i4_rc_temporal_lyr_id
*  @param[in]   i4_first_field
*  @param[in]   i4_rc_is_ref_pic
*  @return      lambda_modifier
*
******************************************************************************
*/
static double ihevce_get_frame_lambda_modifier(
    WORD8 pic_type,
    WORD32 i4_rc_temporal_lyr_id,
    WORD32 i4_first_field,
    WORD32 i4_rc_is_ref_pic,
    WORD32 i4_num_b_frms)
{
    double lambda_modifier;
    WORD32 num_b_frms = i4_num_b_frms, first_field = i4_first_field;

    if(I_PIC == pic_type)
    {
        double temporal_correction_islice = 1.0 - 0.05 * num_b_frms;
        temporal_correction_islice = MAX(0.5, temporal_correction_islice);

        lambda_modifier = 0.57 * temporal_correction_islice;
    }
    else if(P_PIC == pic_type)
    {
        if(first_field)
            lambda_modifier = 0.442;  //0.442*0.8;
        else
            lambda_modifier = 0.442;

        //lambda_modifier *= pow(2.00,(double)(1.00/3.00));
    }
    else
    {
        /* BSLICE */
        if(1 == i4_rc_is_ref_pic)
        {
            lambda_modifier = 0.3536;
        }
        else if(2 == i4_rc_is_ref_pic)
        {
            lambda_modifier = 0.45;
        }
        else
        {
            lambda_modifier = 0.68;
        }

        /* TODO: Disable lambda modification for interlace encode to match HM runs */
        //if(0 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
        {
            /* modify b lambda further based on temporal id */
            if(i4_rc_temporal_lyr_id)
            {
                lambda_modifier *= 3.00;
            }
        }
        //lambda_modifier *= pow(2.00,(double)((1.00/3.00) * (i4_rc_temporal_lyr_id + 1)));
    }

    /* modify the base lambda according to lambda modifier */
    lambda_modifier = sqrt(lambda_modifier);
    return lambda_modifier;
}

/*!
******************************************************************************
* \if Function name : get_avg_bitrate_bufsize
*
* \brief
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void get_avg_bitrate_bufsize(void *pv_ctxt, LWORD64 *pi8_bitrate, LWORD64 *pi8_ebf)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    *pi8_bitrate = rc_get_bit_rate(ps_rc_ctxt->rc_hdl);
    *pi8_ebf = rc_get_vbv_buf_size(ps_rc_ctxt->rc_hdl);
}

/**
******************************************************************************
*
*  @name  ihevce_get_dbf_buffer_size
*
*  @par   Description
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*
*  @return      qp
*
******************************************************************************
*/
void ihevce_get_dbf_buffer_size(
    void *pv_rc_ctxt, UWORD32 *pi4_buffer_size, UWORD32 *pi4_dbf, UWORD32 *pi4_bit_rate)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;

    pi4_buffer_size[0] = (WORD32)ps_rc_ctxt->s_vbv_compliance.f_buffer_size;
    pi4_dbf[0] = (WORD32)(ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level);
    ASSERT(
        ps_rc_ctxt->s_vbv_compliance.f_buffer_size >=
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level);

    pi4_bit_rate[0] = (WORD32)ps_rc_ctxt->s_vbv_compliance.f_bit_rate;
}

/*!
******************************************************************************
* \if Function name : ihevce_set_L0_scd_qp
*
* \brief
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_set_L0_scd_qp(void *pv_rc_ctxt, WORD32 i4_scd_qp)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;

    ps_rc_ctxt->i4_L0_frame_qp = i4_scd_qp;
}

/**
******************************************************************************
*
*  @name  rc_get_buffer_level_unclip
*
*  @par   Description
*
*  @param[in]      pv_rc_ctxt
*
*
*  @return      void
*
******************************************************************************
*/
float rc_get_buffer_level_unclip(void *pv_rc_ctxt)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    return (ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip);
}

/**
******************************************************************************
*
*  @brief Clip QP based on min and max frame qp
*
*  @par   Description
*
*  @param[inout]   ps_rc_ctxt
*  pointer to rc context
*
*  @param[in]      rc_pic_type
*  Pic type
*
*  @return      i4_hevc_frame_qp
*
******************************************************************************
*/
static WORD32 ihevce_clip_min_max_qp(
    rc_context_t *ps_rc_ctxt,
    WORD32 i4_hevc_frame_qp,
    picture_type_e rc_pic_type,
    WORD32 i4_rc_temporal_lyr_id)
{
    ASSERT(i4_rc_temporal_lyr_id >= 0);
    /**clip to min qp which is user configurable*/
    if(rc_pic_type == I_PIC && i4_hevc_frame_qp < ps_rc_ctxt->i4_min_frame_qp)
    {
        i4_hevc_frame_qp = ps_rc_ctxt->i4_min_frame_qp;
    }
    else if(rc_pic_type == P_PIC && i4_hevc_frame_qp < (ps_rc_ctxt->i4_min_frame_qp + 1))
    {
        i4_hevc_frame_qp = ps_rc_ctxt->i4_min_frame_qp + 1;
    }
    else if(i4_hevc_frame_qp < (ps_rc_ctxt->i4_min_frame_qp + i4_rc_temporal_lyr_id + 1))
    {
        /** For B frame max qp is set based on temporal reference*/
        i4_hevc_frame_qp = ps_rc_ctxt->i4_min_frame_qp + i4_rc_temporal_lyr_id + 1;
    }
    /* clip the Qp to MAX QP */
    if(i4_hevc_frame_qp < ps_rc_ctxt->ps_rc_quant_ctxt->i2_min_qp)
    {
        i4_hevc_frame_qp = ps_rc_ctxt->ps_rc_quant_ctxt->i2_min_qp;
    }
    /**clip to max qp based on pic type*/
    if(rc_pic_type == I_PIC && i4_hevc_frame_qp > ps_rc_ctxt->i4_max_frame_qp)
    {
        i4_hevc_frame_qp = ps_rc_ctxt->i4_max_frame_qp;
    }
    else if(rc_pic_type == P_PIC && i4_hevc_frame_qp > (ps_rc_ctxt->i4_max_frame_qp + 1))
    {
        i4_hevc_frame_qp = ps_rc_ctxt->i4_max_frame_qp + 1;
    }
    else if(i4_hevc_frame_qp > (ps_rc_ctxt->i4_max_frame_qp + i4_rc_temporal_lyr_id + 1))
    {
        /** For B frame max qp is set based on temporal reference*/
        i4_hevc_frame_qp = ps_rc_ctxt->i4_max_frame_qp + i4_rc_temporal_lyr_id + 1;
    }
    /* clip the Qp to MAX QP */
    if(i4_hevc_frame_qp > ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp)
    {
        i4_hevc_frame_qp = ps_rc_ctxt->ps_rc_quant_ctxt->i2_max_qp;
    }
    return i4_hevc_frame_qp;
}

/*#############################################*/
/******* END OF SET,GET FUNCTIONS *************/
/*###########################################*/

/*#################################################*/
/******* START OF RC UPDATE FUNCTIONS **************/
/*#################################################*/

/**
******************************************************************************
*
*  @brief updates the picture level information like bits consumed and
*
*  @par   Description
*
*  @param[inout]   ps_mem_tab
*  pointer to memory descriptors table
*
*  @param[in]      ps_init_prms
*  Create time static parameters
*
*  @return      void
*
******************************************************************************
*/

void ihevce_rc_update_pic_info(
    void *pv_ctxt,
    UWORD32 u4_total_bits_consumed,
    UWORD32 u4_total_header_bits,
    UWORD32 u4_frame_sad,
    UWORD32 u4_frame_intra_sad,
    IV_PICTURE_CODING_TYPE_T pic_type,
    WORD32 i4_avg_frame_hevc_qp,
    WORD32 i4_suppress_bpic_update,
    WORD32 *pi4_qp_normalized_8x8_cu_sum,
    WORD32 *pi4_8x8_cu_sum,
    LWORD64 *pi8_sad_by_qscale,
    ihevce_lap_output_params_t *ps_lap_out,
    rc_lap_out_params_t *ps_rc_lap_out,
    WORD32 i4_buf_id,
    UWORD32 u4_open_loop_intra_sad,
    LWORD64 i8_total_ssd_frame,
    WORD32 i4_enc_frm_id)
{
    LWORD64 a_mb_type_sad[2];
    WORD32 a_mb_type_tex_bits[2];
    /*dummy variables not used*/
    WORD32 a_mb_in_type[2] = { 0, 0 };
    LWORD64 a_mb_type_qp_q6[2] = { 0, 0 };
    /*qp accumulation at */
    WORD32 i4_avg_activity = 250;  //hardcoding to usual value
    WORD32 i4_intra_cost, i4_avg_frame_qp_q6, i;
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    WORD32 i4_frame_complexity, i4_bits_to_be_stuffed = 0, i4_is_last_frm_period = 0;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);
    frame_info_t s_frame_info;
    WORD32 i4_ctr = -1, i4_i, i4_j;
    WORD32 i4_scene_num = ps_rc_lap_out->u4_rc_scene_num % MAX_SCENE_NUM;

    /*update bit consumption. used only in rdopt*/
    //ASSERT(ps_rc_ctxt->ai4_rdopt_bit_consumption_estimate[ps_rc_ctxt->i4_rdopt_bit_count] == -1);
    //ASSERT(i4_buf_id>=0);
    ps_rc_ctxt->ai4_rdopt_bit_consumption_estimate[ps_rc_ctxt->i4_rdopt_bit_count] =
        u4_total_bits_consumed;
    ps_rc_ctxt->ai4_rdopt_bit_consumption_buf_id[ps_rc_ctxt->i4_rdopt_bit_count] = i4_buf_id;
    ps_rc_ctxt->i4_rdopt_bit_count =
        (ps_rc_ctxt->i4_rdopt_bit_count + 1) % NUM_BUF_RDOPT_ENT_CORRECT;

    {
        LWORD64 i8_texture_bits = u4_total_bits_consumed - u4_total_header_bits;
        ps_rc_lap_out->i4_use_offline_model_2pass = 0;

        /*flag to guide whether 2nd pass can use offline model or not*/
        if((abs(ps_rc_lap_out->i4_orig_rc_qp - i4_avg_frame_hevc_qp) < 2) &&
           (i8_texture_bits <= (ps_rc_lap_out->i8_est_text_bits * 2.0f)) &&
           (i8_texture_bits >= (ps_rc_lap_out->i8_est_text_bits * 0.5f)))

        {
            ps_rc_lap_out->i4_use_offline_model_2pass = 1;
        }
    }
    /*Counter of number of bit alloction periods*/
    if(rc_pic_type == I_PIC)
        ps_rc_ctxt
            ->i8_num_bit_alloc_period++;  //Currently only I frame periods are considerd as bit allocation period (Ignoring non- I scd and complexity reset flag
    /*initialze frame info*/
    init_frame_info(&s_frame_info);
    s_frame_info.i4_rc_hevc_qp = i4_avg_frame_hevc_qp;
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_L1_me_sad = ps_rc_lap_out->i8_raw_l1_coarse_me_sad;
    s_frame_info.i8_L1_ipe_raw_sad = ps_rc_lap_out->i8_raw_pre_intra_sad;
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_L0_open_cost = (LWORD64)u4_open_loop_intra_sad;
    s_frame_info.i4_num_entries++;

    if(rc_pic_type == I_PIC)
        s_frame_info.i8_L1_me_or_ipe_raw_sad = ps_rc_lap_out->i8_raw_pre_intra_sad;
    else
        s_frame_info.i8_L1_me_or_ipe_raw_sad = ps_rc_lap_out->i8_raw_l1_coarse_me_sad;
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_poc = ps_rc_lap_out->i4_rc_poc;
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_scene_type = ps_rc_lap_out->i4_rc_scene_type;
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_non_i_scd = ps_rc_lap_out->i4_is_non_I_scd || ps_rc_lap_out->i4_is_I_only_scd;
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_cl_sad = u4_frame_sad;
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_header_bits = u4_total_header_bits;
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_tex_bits = u4_total_bits_consumed - u4_total_header_bits;
    s_frame_info.i4_num_entries++;
    s_frame_info.e_pic_type = rc_pic_type;
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_est_texture_bits = ps_rc_lap_out->i8_est_text_bits;
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_lap_complexity_q7 = ps_rc_ctxt->ai4_lap_complexity_q7[i4_enc_frm_id];
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_lap_f_sim = ps_rc_ctxt->ai4_lap_f_sim[i4_enc_frm_id];
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_frame_acc_coarse_me_cost = ps_rc_lap_out->i8_frame_acc_coarse_me_cost;
    s_frame_info.i4_num_entries++;
    s_frame_info.i_to_avg_bit_ratio = ps_rc_ctxt->ai_to_avg_bit_ratio[i4_enc_frm_id];
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_num_scd_in_lap_window = ps_rc_ctxt->ai4_num_scd_in_lap_window[i4_enc_frm_id];
    s_frame_info.i4_num_entries++;
    s_frame_info.i4_num_frames_b4_scd = ps_rc_ctxt->ai4_num_frames_b4_scd[i4_enc_frm_id];
    s_frame_info.i4_num_entries++;
    s_frame_info.i8_num_bit_alloc_period = ps_rc_ctxt->i8_num_bit_alloc_period;
    s_frame_info.i4_num_entries++;
    s_frame_info.i1_is_complexity_based_bits_reset =
        (WORD8)ps_rc_lap_out->i4_is_cmplx_change_reset_bits;
    s_frame_info.i4_num_entries++;
    /*For the complexity based movement in 2nd pass*/
    memmove(
        (void *)s_frame_info.af_sum_weigh,
        ps_rc_lap_out->ps_frame_info->af_sum_weigh,
        sizeof(float) * MAX_PIC_TYPE * 3);
    s_frame_info.i4_num_entries++;

    /*store frame qp to clip qp accordingly*/
    if(ps_rc_lap_out->i4_is_rc_model_needs_to_be_updated)
    {
        ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type] = i4_avg_frame_hevc_qp;
    }

    for(i4_i = 0; i4_i < MAX_NON_REF_B_PICS_IN_QUEUE_SGI; i4_i++)
    {
        if(ps_rc_lap_out->u4_rc_scene_num == ps_rc_ctxt->au4_prev_scene_num_multi_scene[i4_i])
        {
            i4_ctr = i4_i;
            break;
        }
    }
    if(-1 == i4_ctr)
    {
        ps_rc_ctxt->i4_prev_qp_ctr++;
        ps_rc_ctxt->i4_prev_qp_ctr = ps_rc_ctxt->i4_prev_qp_ctr % MAX_NON_REF_B_PICS_IN_QUEUE_SGI;
        i4_ctr = ps_rc_ctxt->i4_prev_qp_ctr;
        ps_rc_ctxt->au4_prev_scene_num_multi_scene[i4_ctr] = ps_rc_lap_out->u4_rc_scene_num;
        for(i4_j = 0; i4_j < MAX_PIC_TYPE; i4_j++)
        {
            ps_rc_ctxt->ai4_qp_for_previous_scene_multi_scene[i4_ctr][i4_j] = 0;
        }
    }

    {
        ps_rc_ctxt->ai4_qp_for_previous_scene_multi_scene[i4_ctr][rc_pic_type] =
            i4_avg_frame_hevc_qp;
    }
    if(i4_scene_num < HALF_MAX_SCENE_ARRAY_QP)
    {
        WORD32 i4_i;
        ps_rc_ctxt->ai4_scene_numbers[i4_scene_num + HALF_MAX_SCENE_ARRAY_QP] = 0;
        for(i4_i = 0; i4_i < MAX_PIC_TYPE; i4_i++)
            ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num + HALF_MAX_SCENE_ARRAY_QP][i4_i] =
                INIT_HEVCE_QP_RC;
    }
    else
    {
        WORD32 i4_i;
        ps_rc_ctxt->ai4_scene_numbers[i4_scene_num - HALF_MAX_SCENE_ARRAY_QP] = 0;
        for(i4_i = 0; i4_i < MAX_PIC_TYPE; i4_i++)
            ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num - HALF_MAX_SCENE_ARRAY_QP][i4_i] =
                INIT_HEVCE_QP_RC;
    }

    /*update will have HEVC qp, convert it back to mpeg2 range qp for all internal calculations of RC*/

    i4_avg_frame_qp_q6 = ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale_q_factor
                             [i4_avg_frame_hevc_qp + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset];

    if(pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME)
    {
        /*TODO : Take care of precision of a_mb_type_sad*/
        a_mb_type_sad[0] =
            (((pi8_sad_by_qscale[1] * i4_avg_frame_qp_q6) +
              (((LWORD64)1) << (SAD_BY_QSCALE_Q + QSCALE_Q_FAC - 1))) >>
             (SAD_BY_QSCALE_Q + QSCALE_Q_FAC));  //u4_frame_sad;

        a_mb_type_sad[1] =
            (((pi8_sad_by_qscale[0] * i4_avg_frame_qp_q6) +
              (((LWORD64)1) << (SAD_BY_QSCALE_Q + QSCALE_Q_FAC - 1))) >>
             (SAD_BY_QSCALE_Q + QSCALE_Q_FAC));
        a_mb_type_tex_bits[0] =
            u4_total_bits_consumed - u4_total_header_bits;  //(u4_total_bits_consumed >> 3);
        a_mb_type_tex_bits[1] = 0;
        a_mb_in_type[0] = (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) >> 8;
        a_mb_in_type[1] = 0;
    }
    else
    {
        /*TODO : Take care of precision of a_mb_type_sad*/
        a_mb_type_sad[1] =
            (((pi8_sad_by_qscale[0] * i4_avg_frame_qp_q6) +
              (((LWORD64)1) << (SAD_BY_QSCALE_Q + QSCALE_Q_FAC - 1))) >>
             (SAD_BY_QSCALE_Q + QSCALE_Q_FAC));

        a_mb_type_tex_bits[0] =
            u4_total_bits_consumed - u4_total_header_bits;  //(u4_total_bits_consumed >> 3);
        a_mb_type_sad[0] =
            (((pi8_sad_by_qscale[1] * i4_avg_frame_qp_q6) +
              (((LWORD64)1) << (SAD_BY_QSCALE_Q + QSCALE_Q_FAC - 1))) >>
             (SAD_BY_QSCALE_Q + QSCALE_Q_FAC));  //u4_frame_sad;
        a_mb_type_tex_bits[1] =
            u4_total_bits_consumed - u4_total_header_bits;  //(u4_total_bits_consumed >> 3);
        a_mb_type_tex_bits[0] = 0;
        a_mb_in_type[1] = (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) >> 8;
        a_mb_in_type[0] = 0;
    }
    ASSERT(a_mb_type_sad[0] >= 0);
    ASSERT(a_mb_type_sad[1] >= 0);
    /*THis calclates sum of Qps of all MBs as per the corresponding mb type*/
    /*THis is different from a_mb_in_type,a_mb_type_sad and a_mb_type_tex_bits*/
    a_mb_type_qp_q6[0] = ((LWORD64)i4_avg_frame_qp_q6) * a_mb_in_type[0];
    a_mb_type_qp_q6[1] = ((LWORD64)i4_avg_frame_qp_q6) * a_mb_in_type[1];
    {
        WORD32 i4_avg_qp_q6_without_offset = 0, i4_hevc_qp_rc = i4_avg_frame_hevc_qp;
        WORD32 i4_rc_pic_type_rc_for_offset = rc_pic_type;
        if(i4_rc_pic_type_rc_for_offset > B2_PIC)
            i4_rc_pic_type_rc_for_offset = i4_rc_pic_type_rc_for_offset - B2_PIC;
        i4_hevc_qp_rc = i4_hevc_qp_rc - ps_rc_lap_out->ai4_offsets[i4_rc_pic_type_rc_for_offset] +
                        ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

        i4_hevc_qp_rc =
            CLIP3(i4_hevc_qp_rc, 1, MAX_HEVC_QP + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset);
        i4_avg_qp_q6_without_offset =
            ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale_q_factor[i4_hevc_qp_rc];

        /*Store the HBD qscale with and without accounting for offset*/
        s_frame_info.f_hbd_q_scale_without_offset =
            (float)i4_avg_qp_q6_without_offset / (1 << QSCALE_Q_FAC);
        s_frame_info.f_hbd_q_scale = (float)i4_avg_frame_qp_q6 / (1 << QSCALE_Q_FAC);
        s_frame_info.i4_num_entries++;
        s_frame_info.i4_num_entries++;

        /*Store the 8 bit qscale with and without accounting for offset*/
        /*Can be useful for pre-enc stage*/
        if(ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset != 0)
        {
            s_frame_info.f_8bit_q_scale_without_offset =
                s_frame_info.f_hbd_q_scale_without_offset / (1 << (ps_rc_ctxt->u1_bit_depth - 8));
            s_frame_info.f_8bit_q_scale =
                s_frame_info.f_hbd_q_scale / (1 << (ps_rc_ctxt->u1_bit_depth - 8));
        }
        else
        {
            s_frame_info.f_8bit_q_scale_without_offset = s_frame_info.f_hbd_q_scale_without_offset;
            s_frame_info.f_8bit_q_scale = s_frame_info.f_hbd_q_scale;
        }
        s_frame_info.i4_num_entries++;
        s_frame_info.i4_num_entries++;
    }

    /*making intra cost same as ssd as of now*/
    i4_intra_cost = u4_frame_intra_sad;

    /* Handling bits stuffing and skips */
    {
        WORD32 i4_num_bits_to_prevent_vbv_underflow;
        vbv_buf_status_e vbv_buffer_status;
        vbv_buffer_status = get_buffer_status(
            ps_rc_ctxt->rc_hdl,
            u4_total_bits_consumed,
            rc_pic_type,  //the picture type convention is different in buffer handling
            &i4_num_bits_to_prevent_vbv_underflow);

        if(vbv_buffer_status == VBV_UNDERFLOW)
        {
        }
        if(vbv_buffer_status == VBV_OVERFLOW)
        {
            i4_bits_to_be_stuffed =
                get_bits_to_stuff(ps_rc_ctxt->rc_hdl, u4_total_bits_consumed, rc_pic_type);
            //i4_bits_to_be_stuffed = 0;/*STORAGE_RC*/
        }
    }
    {
        WORD32 ai4_sad[MAX_PIC_TYPE], i4_valid_sad_entry = 0;
        UWORD32 u4_avg_sad = 0;

        /*calculate frame complexity. Given same content frame complexity should not vary across I,P and Bpic. Hence frame complexity is calculated
        based on average of all pic types SAD*/
        if(rc_pic_type == I_PIC)
        {
            ai4_sad[I_PIC] = u4_frame_intra_sad;
        }
        else
        {
            /*call to get previous I-PIC sad*/
            rc_get_sad(ps_rc_ctxt->rc_hdl, &ai4_sad[0]);
        }

        /*since intra sad is not available for every frame use previous I pic intra frame SAD*/
        rc_put_sad(ps_rc_ctxt->rc_hdl, ai4_sad[I_PIC], u4_frame_sad, rc_pic_type);
        rc_get_sad(ps_rc_ctxt->rc_hdl, &ai4_sad[0]);
        /*for first few frame valid SAD is not available. This will make sure invalid data is not used*/
        if(ps_rc_ctxt->i4_field_pic == 0)
        {
            for(i = 0; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
            {
                if(ai4_sad[i] >= 0)
                {
                    u4_avg_sad += ai4_sad[i];
                    i4_valid_sad_entry++;
                }
            }
        }
        else /*for field case*/
        {
            if(ai4_sad[0] >= 0)
            {
                u4_avg_sad += ai4_sad[0];
                i4_valid_sad_entry++;
            }

            for(i = 1; i < ps_rc_ctxt->i4_num_active_pic_type; i++)
            {
                if(ai4_sad[i] >= 0)
                {
                    u4_avg_sad += ai4_sad[i];
                    i4_valid_sad_entry++;
                }

                if(ai4_sad[i + FIELD_OFFSET] >= 0)
                {
                    u4_avg_sad += ai4_sad[i + FIELD_OFFSET];
                    i4_valid_sad_entry++;
                }
            }
        }

        if(i4_valid_sad_entry > 0)
        {
            i4_frame_complexity =
                (u4_avg_sad) /
                (i4_valid_sad_entry * (ps_rc_ctxt->i4_frame_width * ps_rc_ctxt->i4_frame_height));
        }
        else
        {
            i4_frame_complexity = 1;
        }
    }
    ASSERT(i4_frame_complexity >= 0);
    /*I_model only reset In case of fade-in and fade-out*/
    if(ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id])
    {
        ASSERT(rc_pic_type == I_PIC);
        rc_reset_pic_model(ps_rc_ctxt->rc_hdl, I_PIC);
        ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id] = 0;
    }

    /*check if next picture is I frame, both scene cuts and I pictures are treated as end of period*/
    {
        if(ps_rc_lap_out->i4_rc_pic_type != -1 && ps_rc_lap_out->i4_rc_scene_type != -1)
        {
            if(ps_rc_ctxt->u4_intra_frame_interval != 1)
            {
                /*TBD: For second pass this should be only criteria, While merging to latest verison make sure non - I SCD is not considered as one of the condition*/
                i4_is_last_frm_period = (WORD32)(
                    ps_rc_lap_out->i4_next_pic_type == IV_IDR_FRAME ||
                    ps_rc_lap_out->i4_next_pic_type == IV_I_FRAME);
            }
            else
            {
                i4_is_last_frm_period =
                    (WORD32)(ps_rc_lap_out->i4_next_scene_type == SCENE_TYPE_SCENE_CUT);
            }
        }

        /*In two pass only I frame ending should be considered end of period, otherwise complexity changes should be allowed to reset model in CBR and VBR modes*/
        if(ps_rc_ctxt->i4_rc_pass != 2)
            i4_is_last_frm_period = i4_is_last_frm_period ||
                                    ps_rc_ctxt->ai4_is_cmplx_change_reset_bits[i4_enc_frm_id];
    }

#if 1  //FRAME_PARALLEL_LVL //ELP_RC
    ps_rc_ctxt->i4_est_text_bits_ctr_update_qp++;
    ps_rc_ctxt->i4_est_text_bits_ctr_update_qp =
        (ps_rc_ctxt->i4_est_text_bits_ctr_update_qp % (ps_rc_ctxt->i4_num_frame_parallel));
#endif

    update_frame_level_info(
        ps_rc_ctxt->rc_hdl,
        rc_pic_type,
        a_mb_type_sad,
        u4_total_bits_consumed, /*total bits consumed by frame*/
        u4_total_header_bits,
        a_mb_type_tex_bits,
        a_mb_type_qp_q6, /*sum of qp of all mb in frame, since no ctb level modulation*/
        a_mb_in_type,
        i4_avg_activity,
        ps_rc_ctxt->ai4_is_frame_scd[i4_enc_frm_id], /*currenlty SCD is not enabled*/
        0, /*not a pre encode skip*/
        i4_intra_cost,
        0,
        ps_rc_lap_out
            ->i4_ignore_for_rc_update, /*HEVC_hierarchy: do not supress update for non-ref B pic*/
        i4_bits_to_be_stuffed,
        (ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] ||
         ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] ||
         ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i4_enc_frm_id]),
        ps_rc_ctxt->ai4_lap_complexity_q7[i4_enc_frm_id],
        i4_is_last_frm_period,
        ps_rc_ctxt->ai4_is_cmplx_change_reset_bits[i4_enc_frm_id],
        &s_frame_info,
        ps_rc_lap_out->i4_is_rc_model_needs_to_be_updated,
        ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset,
        i4_scene_num,
        ps_rc_ctxt->ai4_scene_numbers[i4_scene_num],
        ps_rc_ctxt->i4_est_text_bits_ctr_update_qp);
    /** reset flags valid for only one frame*/
    ps_rc_ctxt->ai4_is_frame_scd[i4_enc_frm_id] = 0;
    ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] = 0;
    ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] = 0;
    ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i4_enc_frm_id] = 0;
    ps_rc_ctxt->ai4_is_cmplx_change_reset_bits[i4_enc_frm_id] = 0;

    ps_rc_ctxt->i4_is_first_frame_encoded = 1;

    /** update the scene num for current frame*/
    ps_rc_ctxt->au4_scene_num_temp_id[ps_rc_lap_out->i4_rc_temporal_lyr_id] =
        ps_rc_lap_out->u4_rc_scene_num;

    if(ps_rc_ctxt->ai4_is_frame_scd[i4_enc_frm_id])
    {
        /*reset pre-enc SAD whenever SCD is detected so that it does not detect scene cut for other pictures*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[i] = -1;
        }
    }

    /*remember i frame's cost metric to scale SAD of next of I frame*/
    if(pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME)
    {
        ps_rc_ctxt->i8_prev_i_frm_cost = ps_rc_ctxt->ai8_cur_frm_intra_cost[i4_enc_frm_id];
        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[rc_pic_type] =
            ps_rc_ctxt->ai8_cur_frm_intra_cost[i4_enc_frm_id];
    }
    /*for other picture types update hme cost*/
    else
    {
        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[rc_pic_type] =
            ps_rc_ctxt->ai8_cur_frame_coarse_ME_cost[i4_enc_frm_id];
    }
}
/*!
******************************************************************************
* \if Function name : ihevce_rc_interface_update \endif
*
* \brief
*   Updating rate control interface parameters after the query call.
*
* \param[in] Rate control interface context,
*            Picture Type
*            Lap out structure pointer
*
*
* \return
*    None
*
* \author Ittiam
*  Ittiam
*
*****************************************************************************
*/
void ihevce_rc_interface_update(
    void *pv_ctxt,
    IV_PICTURE_CODING_TYPE_T pic_type,
    rc_lap_out_params_t *ps_rc_lap_out,
    WORD32 i4_avg_frame_hevc_qp,
    WORD32 i4_enc_frm_id)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);
    WORD32 i;
    WORD32 i4_avg_frame_qp_q6, i4_ctr = -1, i4_i, i4_j;
    WORD32 i4_scene_num = ps_rc_lap_out->u4_rc_scene_num % MAX_SCENE_NUM;

    /*store frame qp to clip qp accordingly*/
    if(ps_rc_lap_out->i4_is_rc_model_needs_to_be_updated)
    {
        WORD32 i4_i, i4_temp_i_qp, i4_temp_qp;
        ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][rc_pic_type] = i4_avg_frame_hevc_qp;
        ps_rc_ctxt->ai4_scene_numbers[i4_scene_num]++;

        if(rc_pic_type < P1_PIC)
            i4_temp_i_qp = i4_avg_frame_hevc_qp - rc_pic_type;
        else
            i4_temp_i_qp = i4_avg_frame_hevc_qp - rc_pic_type + 4;

        i4_temp_i_qp = ihevce_clip_min_max_qp(ps_rc_ctxt, i4_temp_i_qp, I_PIC, 0);

        if(ps_rc_ctxt->ai4_scene_numbers[i4_scene_num] == 1)
        {
            for(i4_i = 0; i4_i < 5; i4_i++)
            {
                if(ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i4_i] == INIT_HEVCE_QP_RC)
                {
                    i4_temp_qp = i4_temp_i_qp + i4_i;
                    i4_temp_qp = ihevce_clip_min_max_qp(
                        ps_rc_ctxt, i4_temp_qp, (picture_type_e)i4_i, MAX(i4_i - 1, 0));
                    ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i4_i] = i4_temp_qp;

                    if(i4_i > 0)
                        ps_rc_ctxt->ai4_prev_pic_hevc_qp[i4_scene_num][i4_i + 4] = i4_temp_qp;
                }
            }
        }
    }

    for(i4_i = 0; i4_i < MAX_NON_REF_B_PICS_IN_QUEUE_SGI; i4_i++)
    {
        if(ps_rc_lap_out->u4_rc_scene_num == ps_rc_ctxt->au4_prev_scene_num_multi_scene[i4_i])
        {
            i4_ctr = i4_i;
            break;
        }
    }
    if(-1 == i4_ctr)
    {
        ps_rc_ctxt->i4_prev_qp_ctr++;
        ps_rc_ctxt->i4_prev_qp_ctr = ps_rc_ctxt->i4_prev_qp_ctr % MAX_NON_REF_B_PICS_IN_QUEUE_SGI;
        i4_ctr = ps_rc_ctxt->i4_prev_qp_ctr;
        ps_rc_ctxt->au4_prev_scene_num_multi_scene[i4_ctr] = ps_rc_lap_out->u4_rc_scene_num;
        for(i4_j = 0; i4_j < MAX_PIC_TYPE; i4_j++)
        {
            ps_rc_ctxt->ai4_qp_for_previous_scene_multi_scene[i4_ctr][i4_j] = 0;
        }
    }

    {
        ps_rc_ctxt->ai4_qp_for_previous_scene_multi_scene[i4_ctr][rc_pic_type] =
            i4_avg_frame_hevc_qp;
    }

    /*I_model only reset In case of fade-in and fade-out*/
    if(ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id])
    {
        ASSERT(rc_pic_type == I_PIC);
        rc_reset_pic_model(ps_rc_ctxt->rc_hdl, I_PIC);
        ps_rc_ctxt->ai4_I_model_only_reset[i4_enc_frm_id] = 0;
    }

    i4_avg_frame_qp_q6 = ps_rc_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale_q_factor
                             [i4_avg_frame_hevc_qp + ps_rc_ctxt->ps_rc_quant_ctxt->i1_qp_offset];

    update_frame_rc_get_frame_qp_info(
        ps_rc_ctxt->rc_hdl,
        rc_pic_type,
        ps_rc_ctxt->ai4_is_frame_scd[i4_enc_frm_id],
        (ps_rc_ctxt->ai4_is_pause_to_resume[i4_enc_frm_id] ||
         ps_rc_ctxt->ai4_is_non_I_scd_pic[i4_enc_frm_id] ||
         ps_rc_ctxt->ai4_is_cmplx_change_reset_model[i4_enc_frm_id]),
        i4_avg_frame_qp_q6,
        ps_rc_lap_out->i4_ignore_for_rc_update,
        i4_scene_num,
        ps_rc_ctxt->ai4_scene_numbers[i4_scene_num]);

    /** update the scene num for current frame*/
    ps_rc_ctxt->au4_scene_num_temp_id[ps_rc_lap_out->i4_rc_temporal_lyr_id] =
        ps_rc_lap_out->u4_rc_scene_num;

    if(ps_rc_ctxt->ai4_is_frame_scd[i4_enc_frm_id])
    {
        /*reset pre-enc SAD whenever SCD is detected so that it does not detect scene cut for other pictures*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[i] = -1;
        }
    }

    /*remember i frame's cost metric to scale SAD of next of I frame*/
    if(pic_type == IV_I_FRAME || pic_type == IV_IDR_FRAME)
    {
        ps_rc_ctxt->i8_prev_i_frm_cost = ps_rc_ctxt->ai8_cur_frm_intra_cost[i4_enc_frm_id];
        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[rc_pic_type] =
            ps_rc_ctxt->ai8_cur_frm_intra_cost[i4_enc_frm_id];
    }
    /*for other picture types update hme cost*/
    else
    {
        ps_rc_ctxt->ai8_prev_frm_pre_enc_cost[rc_pic_type] =
            ps_rc_ctxt->ai8_cur_frame_coarse_ME_cost[i4_enc_frm_id];
    }

    ps_rc_ctxt->i4_is_first_frame_encoded = 1;
}

/****************************************************************************
Function Name : ihevce_rc_store_retrive_update_info
Description   : for storing and retrieving the data in case of the Enc Loop Parallelism.
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/

void ihevce_rc_store_retrive_update_info(
    void *pv_ctxt,
    rc_bits_sad_t *ps_rc_frame_stat,
    WORD32 i4_enc_frm_id_rc,
    WORD32 bit_rate_id,
    WORD32 i4_store_retrive,
    WORD32 *pout_buf_id,
    WORD32 *pi4_rc_pic_type,
    WORD32 *pcur_qp,
    void *ps_lap_out,
    void *ps_rc_lap_out)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    if(1 == i4_store_retrive)
    {
        memcpy(
            &ps_rc_ctxt->as_rc_frame_stat_store[i4_enc_frm_id_rc][bit_rate_id],
            ps_rc_frame_stat,
            sizeof(rc_bits_sad_t));
        memcpy(&ps_rc_ctxt->out_buf_id[i4_enc_frm_id_rc][bit_rate_id], pout_buf_id, sizeof(WORD32));
        memcpy(&ps_rc_ctxt->i4_pic_type[i4_enc_frm_id_rc], pi4_rc_pic_type, sizeof(WORD32));
        memcpy(&ps_rc_ctxt->cur_qp[i4_enc_frm_id_rc][bit_rate_id], pcur_qp, sizeof(WORD32));
        memcpy(
            &ps_rc_ctxt->as_lap_out[i4_enc_frm_id_rc],
            ps_lap_out,
            sizeof(ihevce_lap_output_params_t));
        memcpy(
            &ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc],
            ps_rc_lap_out,
            sizeof(rc_lap_out_params_t));
        //BUG_FIX related to the releasing of the next lap out buffers and retrieving of the data for the delayed update.

        {
            rc_lap_out_params_t *ps_rc_lap_out_next_encode;
            ps_rc_lap_out_next_encode =
                (rc_lap_out_params_t *)((rc_lap_out_params_t *)ps_rc_lap_out)
                    ->ps_rc_lap_out_next_encode;

            if(NULL != ps_rc_lap_out_next_encode)
            {
                ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc].i4_next_pic_type =
                    ps_rc_lap_out_next_encode->i4_rc_pic_type;
                ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc].i4_next_scene_type =
                    ps_rc_lap_out_next_encode->i4_rc_scene_type;
            }
            else
            {
                ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc].i4_next_pic_type = -1;
                ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc].i4_next_scene_type = -1;
            }

            ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc].ps_rc_lap_out_next_encode =
                NULL;  //RC_BUG_FIX
        }
    }
    else if(2 == i4_store_retrive)
    {
        memcpy(
            ps_rc_frame_stat,
            &ps_rc_ctxt->as_rc_frame_stat_store[i4_enc_frm_id_rc][bit_rate_id],
            sizeof(rc_bits_sad_t));
        memcpy(pout_buf_id, &ps_rc_ctxt->out_buf_id[i4_enc_frm_id_rc][bit_rate_id], sizeof(WORD32));
        memcpy(pi4_rc_pic_type, &ps_rc_ctxt->i4_pic_type[i4_enc_frm_id_rc], sizeof(WORD32));
        memcpy(pcur_qp, &ps_rc_ctxt->cur_qp[i4_enc_frm_id_rc][bit_rate_id], sizeof(WORD32));
        memcpy(
            ps_lap_out,
            &ps_rc_ctxt->as_lap_out[i4_enc_frm_id_rc],
            sizeof(ihevce_lap_output_params_t));
        memcpy(
            ps_rc_lap_out,
            &ps_rc_ctxt->as_rc_lap_out[i4_enc_frm_id_rc],
            sizeof(rc_lap_out_params_t));
    }
    else
    {
        ASSERT(0);
    }
}

/*###############################################*/
/******* END OF RC UPDATE FUNCTIONS **************/
/*###############################################*/

/*#################################################*/
/******* START OF RC UTILS FUNCTIONS **************/
/*#################################################*/

/**
******************************************************************************
*
*  @brief function to account for error correction between bits rdopt estimate
*           and actual entropy bit generation
*
*  @par   Description
*
*  @param[in]   pv_rc_ctxt
*               void pointer to rc ctxt
*  @param[in]   i4_rdopt_bits_gen_error
*               WODd32 variable with error correction between rdopt and entropy bytes gen
*
*  @return      void
*
******************************************************************************
*/

void ihevce_rc_rdopt_entropy_bit_correct(
    void *pv_rc_ctxt, WORD32 i4_cur_entropy_consumption, WORD32 i4_buf_id)
{
    rc_context_t *ps_ctxt = (rc_context_t *)pv_rc_ctxt;
    WORD32 i4_error;
    WORD32 i, count = 0;
    ASSERT(i4_buf_id >= 0);
    ps_ctxt->ai4_entropy_bit_consumption[ps_ctxt->i4_entropy_bit_count] =
        i4_cur_entropy_consumption;
    ps_ctxt->ai4_entropy_bit_consumption_buf_id[ps_ctxt->i4_entropy_bit_count] = i4_buf_id;
    ps_ctxt->i4_entropy_bit_count = (ps_ctxt->i4_entropy_bit_count + 1) % NUM_BUF_RDOPT_ENT_CORRECT;

    for(i = 0; i < NUM_BUF_RDOPT_ENT_CORRECT; i++)
    {
        if(ps_ctxt->ai4_rdopt_bit_consumption_buf_id[i] >= 0 &&
           (ps_ctxt->ai4_rdopt_bit_consumption_buf_id[i] ==
            ps_ctxt->ai4_entropy_bit_consumption_buf_id[i]))
        {
            i4_error = ps_ctxt->ai4_rdopt_bit_consumption_estimate[i] -
                       ps_ctxt->ai4_entropy_bit_consumption[i];
            //DBG_PRINTF("entropy mismatch error = %d\n",i4_error/ps_ctxt->ai4_rdopt_bit_consumption_estimate[i]);
            ps_ctxt->ai4_rdopt_bit_consumption_estimate[i] = -1;
            ps_ctxt->ai4_rdopt_bit_consumption_buf_id[i] = -1;
            ps_ctxt->ai4_entropy_bit_consumption[i] = -1;
            ps_ctxt->ai4_entropy_bit_consumption_buf_id[i] = -1;
            /*accumulate mismatch along with gop level bit error that is propogated to next frame*/
            /*error = rdopt - entropy so it is expected to be negative*/
            rc_update_mismatch_error(ps_ctxt->rc_hdl, i4_error);
            count++;
        }
    }
}

/**
******************************************************************************
*
*  @name  ihevce_rc_check_non_lap_scd
*
*  @par   Description Detects SCD frames as I_only_scds or non_I_scds based
                      on intrasatd & ME costs. Updates scd flags
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*               ps_rc_lap_out
*  @return      void
*
******************************************************************************
*/
void ihevce_rc_check_non_lap_scd(void *pv_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    picture_type_e rc_pic_type = ihevce_rc_conv_pic_type(
        (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_rc_lap_out->i4_rc_temporal_lyr_id,
        ps_rc_lap_out->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);

    /*Init to normal frames*/
    ps_rc_lap_out->i4_is_I_only_scd = 0;
    ps_rc_lap_out->i4_is_non_I_scd = 0;

    /*None of the above check is valid if marked as scene cut*/
    if(ps_rc_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
    {
        WORD32 i;
        /*reset all older data*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[i] = -1;
            ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_cost[i] = -1;
            ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_sad[i] = -1;
        }
    }
    else
    {
        /*Check if it is I only reset case, lap_out is assumed to have latest data which is used to set the corresponding flags*/
        /*For I pic check for I only reset case and for other pictures check for non-I scd case*/
        if(rc_pic_type == I_PIC)
        {
            if(ps_rc_lap_out->i8_pre_intra_satd <
                   (ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[rc_pic_type] >> 1) ||
               ps_rc_lap_out->i8_pre_intra_satd >
                   (ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[rc_pic_type] << 1))
            {
                /*Check if atleast one frame data is available*/
                if(ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[rc_pic_type] >= 0)
                    ps_rc_lap_out->i4_is_I_only_scd = 1;
            }
        }
        else if(
            ((rc_pic_type == P_PIC) &&
             (ps_rc_lap_out->i4_rc_quality_preset == IHEVCE_QUALITY_P6)) ||
            (ps_rc_lap_out->i4_rc_quality_preset < IHEVCE_QUALITY_P6))
        {
#define SAD_THREASHOLD_30FPS (2.5)
            /*Choose threshold as 2.5 for 30 fps content and 1.75 for 60 fps. Scale accordingly for intermediate framerate*/
            WORD32 i4_non_simple_repeat_prev_frame_detect = 0;
            float sad_change_threshold =
                (float)(-0.8f * ((float)ps_rc_ctxt->u4_max_frame_rate / 30000) + 3.05f); /*Change of SAD threshold for 30 fps content, this should be lowered for 60 fps*/
            if(sad_change_threshold < 1.5f)
                sad_change_threshold = 1.5f;
            if(sad_change_threshold > 3.0f)
                sad_change_threshold = 3.0f;
            ASSERT(ps_rc_lap_out->i8_raw_l1_coarse_me_sad >= 0);

            /*block variance computed at 4x4 level in w/4*h/4,
                percent dc blks is how many block's variance are less than or equal to 16*/
            if(ps_rc_lap_out->i4_perc_dc_blks < 85)
            {
                /*me sad is expected to be zero for repeat frames*/
                if((ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_sad[rc_pic_type] ==
                    0) &&
                   (ps_rc_lap_out->i4_rc_temporal_lyr_id == ps_rc_ctxt->i4_max_temporal_lyr))
                {
                    i4_non_simple_repeat_prev_frame_detect = 1;
                }
            }
            if(ps_rc_lap_out->i8_frame_acc_coarse_me_cost >
                   (sad_change_threshold *
                    ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_cost[rc_pic_type]) &&
               (ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_cost[rc_pic_type] >= 0) &&
               (!i4_non_simple_repeat_prev_frame_detect))
            {
                WORD32 one_per_pixel_sad_L1;
                /*per pixel sad has to be greater than 1 to avoid repeat frames influence non-I scd detection*/
                if((ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) < 4000000)
                {
                    /*1080*/
                    one_per_pixel_sad_L1 =
                        (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) >> 2;
                }
                else
                {
                    /*4k*/
                    one_per_pixel_sad_L1 =
                        (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width) >> 4;
                }
                if(ps_rc_lap_out->i8_frame_acc_coarse_me_cost > one_per_pixel_sad_L1)
                {
                    {
                        ps_rc_lap_out->i4_is_non_I_scd = 1;
                    }
                }
            }

            if(rc_pic_type == P_PIC)
            {
                if(ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_cost[rc_pic_type] < 0)
                {
                    if(ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[I_PIC] > 0)
                    {
                        if(ps_rc_lap_out->i8_pre_intra_satd >
                           ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[I_PIC] << 1)
                        {
                            ps_rc_lap_out->i4_is_non_I_scd = 1;
                        }
                    }
                }
            }
        }
    }

    /*remember the previous frame stats*/
    ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_I_intra_raw_satd[rc_pic_type] =
        ps_rc_lap_out->i8_pre_intra_satd;  //ps_rc_lap_out->i8_pre_intra_satd;
    ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_cost[rc_pic_type] =
        ps_rc_lap_out->i8_frame_acc_coarse_me_cost;  //ps_rc_lap_out->i8_frame_acc_coarse_me_sad;
    ps_rc_ctxt->s_l1_state_metric.ai8_L1_prev_pic_coarse_me_sad[rc_pic_type] =
        ps_rc_lap_out->i8_raw_l1_coarse_me_sad;
}

/**
******************************************************************************
*
*  @name  ihevce_rc_check_is_pre_enc_qp_valid
*
*  @par   Description  checking whether enc thread has updated qp in reverse queue
*
*  @param[in]   ps_rc_ctxt  - pointer to rc context
*
*  @return      zero on success
*
******************************************************************************
*/
/**only function accessed by encoder without using mutex lock*/
WORD32 ihevce_rc_check_is_pre_enc_qp_valid(void *pv_rc_ctxt, volatile WORD32 *pi4_force_end_flag)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;

    volatile WORD32 i4_is_qp_valid;
    volatile WORD32 *pi4_is_qp_valid;

    pi4_is_qp_valid =
        (volatile WORD32 *)&ps_rc_ctxt->as_pre_enc_qp_queue[ps_rc_ctxt->i4_pre_enc_qp_read_index]
            .i4_is_qp_valid;
    i4_is_qp_valid = *pi4_is_qp_valid;

    /*Due to stagger between L1 IPE and L0 IPE, towards the end (when encoder is in flush mode) L0 IPE can race ahead of enc
    since it will suddenly get stagger between L1 and L0 worth of free buffers. It could try to start L0 even before enc has
    populated qp for such frames. qp = -1 is returned in such case which implies encoder should wait for qp to be pop*/

    while(i4_is_qp_valid == -1)
    {
        /*this rate control call is outside mutex lock to avoid deadlock. If this acquires mutex lock enc will not be able to
        populate qp*/
        i4_is_qp_valid = *pi4_is_qp_valid;

        if(1 == (*pi4_force_end_flag))
        {
            *pi4_is_qp_valid = 1;
            i4_is_qp_valid = 1;
        }
    }
    return 0;
}

/*!
******************************************************************************
* \if Function name : ihevce_compute_temporal_complexity_reset_Kp_Kb
*
* \brief
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_compute_temporal_complexity_reset_Kp_Kb(
    rc_lap_out_params_t *ps_rc_lap_out, void *pv_rc_ctxt, WORD32 i4_Kp_Kb_reset_flag)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    rc_lap_out_params_t *ps_cur_rc_lap_out_temporal_offset,
        *ps_cur_rc_lap_out_temporal_offset_scd_detect;
    picture_type_e curr_rc_pic_type;
    LWORD64 i8_total_acc_coarse_me_sad = 0, i8_avg_acc_coarse_me_sad = 0;
    WORD8 i1_num_frames_in_Sub_GOP = 0, i = 0, i1_no_reset = 0;
    WORD32 i4_inter_frame_interval = rc_get_inter_frame_interval(ps_rc_ctxt->rc_hdl);
    WORD32 i4_frame_qp = 0, i4_temp_frame_qp = 0;
    WORD32 ai4_offsets[5] = { -3, -2, 2, 6, 7 };
    ps_cur_rc_lap_out_temporal_offset = ps_rc_lap_out;
    ps_cur_rc_lap_out_temporal_offset_scd_detect = ps_rc_lap_out;

    curr_rc_pic_type = ihevce_rc_conv_pic_type(
        (IV_PICTURE_CODING_TYPE_T)ps_cur_rc_lap_out_temporal_offset->i4_rc_pic_type,
        ps_rc_ctxt->i4_field_pic,
        ps_cur_rc_lap_out_temporal_offset->i4_rc_temporal_lyr_id,
        ps_cur_rc_lap_out_temporal_offset->i4_is_bottom_field,
        ps_rc_ctxt->i4_top_field_first);

    if(curr_rc_pic_type == I_PIC)
    {
        ps_cur_rc_lap_out_temporal_offset_scd_detect =
            (rc_lap_out_params_t *)ps_cur_rc_lap_out_temporal_offset->ps_rc_lap_out_next_encode;
        ps_cur_rc_lap_out_temporal_offset =
            (rc_lap_out_params_t *)ps_cur_rc_lap_out_temporal_offset->ps_rc_lap_out_next_encode;

        if(NULL != ps_cur_rc_lap_out_temporal_offset)
        {
            curr_rc_pic_type = ihevce_rc_conv_pic_type(
                (IV_PICTURE_CODING_TYPE_T)ps_cur_rc_lap_out_temporal_offset->i4_rc_pic_type,
                ps_rc_ctxt->i4_field_pic,
                ps_cur_rc_lap_out_temporal_offset->i4_rc_temporal_lyr_id,
                ps_cur_rc_lap_out_temporal_offset->i4_is_bottom_field,
                ps_rc_ctxt->i4_top_field_first);
        }
        else
            return;
    }

    if(ps_cur_rc_lap_out_temporal_offset->i4_L1_qp == -1)
        return;

    if(ps_cur_rc_lap_out_temporal_offset->i4_L0_qp == -1)
        i4_frame_qp = ps_cur_rc_lap_out_temporal_offset->i4_L1_qp;
    else
        i4_frame_qp = ps_cur_rc_lap_out_temporal_offset->i4_L0_qp;

    i1_num_frames_in_Sub_GOP = 0;
    i = 0;

    i1_no_reset = 0;
    do
    {
        if(ps_cur_rc_lap_out_temporal_offset != NULL)
        {
            if(curr_rc_pic_type != I_PIC)
                i4_temp_frame_qp =
                    i4_frame_qp + ps_cur_rc_lap_out_temporal_offset->i4_rc_temporal_lyr_id + 1;

            i4_temp_frame_qp += ai4_offsets[curr_rc_pic_type];
            i4_temp_frame_qp = CLIP3(i4_temp_frame_qp, 1, 51);

            {
                if(curr_rc_pic_type != I_PIC)
                {
                    i8_total_acc_coarse_me_sad +=
                        ps_cur_rc_lap_out_temporal_offset
                            ->ai8_frame_acc_coarse_me_sad[i4_temp_frame_qp];
                    i1_num_frames_in_Sub_GOP++;
                    i++;
                }
                else
                {
                    break;
                }
            }

            ps_cur_rc_lap_out_temporal_offset =
                (rc_lap_out_params_t *)ps_cur_rc_lap_out_temporal_offset->ps_rc_lap_out_next_encode;

            if(ps_cur_rc_lap_out_temporal_offset == NULL)
            {
                break;
            }
            curr_rc_pic_type = ihevce_rc_conv_pic_type(
                (IV_PICTURE_CODING_TYPE_T)ps_cur_rc_lap_out_temporal_offset->i4_rc_pic_type,
                ps_rc_ctxt->i4_field_pic,
                ps_cur_rc_lap_out_temporal_offset->i4_rc_temporal_lyr_id,
                ps_cur_rc_lap_out_temporal_offset->i4_is_bottom_field,
                ps_rc_ctxt->i4_top_field_first);
        }
        else
        {
            i1_num_frames_in_Sub_GOP = 0;
            break;
        }
    } while(
        ((((curr_rc_pic_type != P_PIC) && ((curr_rc_pic_type != I_PIC))) ||
          (curr_rc_pic_type == P_PIC)) &&
         (i1_num_frames_in_Sub_GOP < i4_inter_frame_interval)));

    if((i1_num_frames_in_Sub_GOP) && (i1_no_reset == 0))
    {
        float f_hme_sad_per_pixel;
        i8_avg_acc_coarse_me_sad = (i8_total_acc_coarse_me_sad / i1_num_frames_in_Sub_GOP);
        f_hme_sad_per_pixel =
            ((float)i8_avg_acc_coarse_me_sad /
             (ps_rc_ctxt->i4_frame_height * ps_rc_ctxt->i4_frame_width));
        f_hme_sad_per_pixel = CLIP3(f_hme_sad_per_pixel, 0.01f, 5.0f);
        /*reset the QP offsets for the next sub GOP depending on the offline model based on the temporal complexity */
        if(i4_Kp_Kb_reset_flag)
        {
            WORD32 i4_bin;

            rc_reset_Kp_Kb(
                ps_rc_ctxt->rc_hdl,
                8.00,
                ps_rc_ctxt->i4_num_active_pic_type,
                f_hme_sad_per_pixel,
                &i4_bin,
                ps_rc_ctxt->i4_rc_pass);
        }
        else
        {
            rc_ba_get_qp_offset_offline_data(
                ps_rc_ctxt->rc_hdl,
                ps_rc_lap_out->ai4_offsets,
                f_hme_sad_per_pixel,
                ps_rc_ctxt->i4_num_active_pic_type,
                &ps_rc_lap_out->i4_complexity_bin);

            ps_cur_rc_lap_out_temporal_offset = ps_rc_lap_out;
            ps_cur_rc_lap_out_temporal_offset->i4_offsets_set_flag = 1;

            curr_rc_pic_type = ihevce_rc_conv_pic_type(
                (IV_PICTURE_CODING_TYPE_T)ps_rc_lap_out->i4_rc_pic_type,
                ps_rc_ctxt->i4_field_pic,
                ps_rc_lap_out->i4_rc_temporal_lyr_id,
                ps_rc_lap_out->i4_is_bottom_field,
                ps_rc_ctxt->i4_top_field_first);

            if((curr_rc_pic_type == I_PIC) &&
               ((rc_lap_out_params_t *)ps_cur_rc_lap_out_temporal_offset->ps_rc_lap_out_next_encode)
                       ->i4_rc_pic_type == P_PIC)
                i1_num_frames_in_Sub_GOP++;

            for(i = 1; i < i1_num_frames_in_Sub_GOP; i++)
            {
                ps_cur_rc_lap_out_temporal_offset =
                    (rc_lap_out_params_t *)
                        ps_cur_rc_lap_out_temporal_offset->ps_rc_lap_out_next_encode;
                memmove(
                    ps_cur_rc_lap_out_temporal_offset->ai4_offsets,
                    ps_rc_lap_out->ai4_offsets,
                    sizeof(WORD32) * 5);
                ps_cur_rc_lap_out_temporal_offset->i4_complexity_bin =
                    ps_rc_lap_out->i4_complexity_bin;
                ps_cur_rc_lap_out_temporal_offset->i4_offsets_set_flag = 1;
            }
        }
    }
}

/**
******************************************************************************
*
*  @brief function to get delta QP or In frame RC bits estimate to avoid buffer underflow
*
*  @par   Description
*  @param[in]
******************************************************************************
*/

WORD32 ihevce_ebf_based_rc_correction_to_avoid_overflow(
    rc_context_t *ps_rc_ctxt, rc_lap_out_params_t *ps_rc_lap_out, WORD32 *pi4_tot_bits_estimated)
{
    WORD32 i4_modelQP, i4_clipQP, i4_maxEbfQP, i4_diffQP, i4_is_model_valid, i4_deltaQP = 0;
    LWORD64 i8_bitsClipQP, i8_grwEbf;  // i8_bitsComp;
    WORD32 i4_is_offline_model_used;
    WORD32 i4_vbv_buffer_size, i4_drain_rate, i4_currEbf, i4_maxEbf;
    WORD32 i4_case = -1;
    float f_thrsh_i_pic_delta_qp_1, f_thrsh_i_pic_delta_qp_2, f_thrsh_p_pic_delta_qp_1,
        f_thrsh_p_pic_delta_qp_2;
    float f_thrsh_br_pic_delta_qp_1, f_thrsh_br_pic_delta_qp_2, f_thrsh_bnr_pic_delta_qp_1,
        f_thrsh_bnr_pic_delta_qp_2;
    float f_vbv_thrsh_delta_qp;

    /*initialization of all the variables*/
    rc_init_buffer_info(
        ps_rc_ctxt->rc_hdl, &i4_vbv_buffer_size, &i4_currEbf, &i4_maxEbf, &i4_drain_rate);

    i4_is_model_valid = ps_rc_lap_out->i4_is_model_valid;
    i4_modelQP = ps_rc_ctxt->s_rc_high_lvl_stat.i4_modelQP;
    i4_clipQP = ps_rc_ctxt->s_rc_high_lvl_stat.i4_finalQP;
    i4_maxEbfQP = ps_rc_ctxt->s_rc_high_lvl_stat.i4_maxEbfQP;
    i8_bitsClipQP = ps_rc_ctxt->s_rc_high_lvl_stat.i8_bits_from_finalQP;
    i4_is_offline_model_used = ps_rc_ctxt->s_rc_high_lvl_stat.i4_is_offline_model_used;
    ASSERT(i4_clipQP != INVALID_QP);

    if(ps_rc_ctxt->i4_num_frame_parallel > 1)
    {
        f_thrsh_i_pic_delta_qp_1 = (float)VBV_THRSH_FRM_PRLL_I_PIC_DELTA_QP_1;
        f_thrsh_i_pic_delta_qp_2 = (float)VBV_THRSH_FRM_PRLL_I_PIC_DELTA_QP_2;
        f_thrsh_p_pic_delta_qp_1 = (float)VBV_THRSH_FRM_PRLL_P_PIC_DELTA_QP_1;
        f_thrsh_p_pic_delta_qp_2 = (float)VBV_THRSH_FRM_PRLL_P_PIC_DELTA_QP_2;
        f_thrsh_br_pic_delta_qp_1 = (float)VBV_THRSH_FRM_PRLL_BR_PIC_DELTA_QP_1;
        f_thrsh_br_pic_delta_qp_2 = (float)VBV_THRSH_FRM_PRLL_BR_PIC_DELTA_QP_2;
        f_thrsh_bnr_pic_delta_qp_1 = (float)VBV_THRSH_FRM_PRLL_BNR_PIC_DELTA_QP_1;
        f_thrsh_bnr_pic_delta_qp_2 = (float)VBV_THRSH_FRM_PRLL_BNR_PIC_DELTA_QP_2;
        f_vbv_thrsh_delta_qp = (float)VBV_THRSH_FRM_PRLL_DELTA_QP;
    }
    else
    {
        f_thrsh_i_pic_delta_qp_1 = (float)VBV_THRSH_I_PIC_DELTA_QP_1;
        f_thrsh_i_pic_delta_qp_2 = (float)VBV_THRSH_I_PIC_DELTA_QP_2;
        f_thrsh_p_pic_delta_qp_1 = (float)VBV_THRSH_P_PIC_DELTA_QP_1;
        f_thrsh_p_pic_delta_qp_2 = (float)VBV_THRSH_P_PIC_DELTA_QP_2;
        f_thrsh_br_pic_delta_qp_1 = (float)VBV_THRSH_BR_PIC_DELTA_QP_1;
        f_thrsh_br_pic_delta_qp_2 = (float)VBV_THRSH_BR_PIC_DELTA_QP_2;
        f_thrsh_bnr_pic_delta_qp_1 = (float)VBV_THRSH_BNR_PIC_DELTA_QP_1;
        f_thrsh_bnr_pic_delta_qp_2 = (float)VBV_THRSH_BNR_PIC_DELTA_QP_2;
        f_vbv_thrsh_delta_qp = (float)VBV_THRSH_DELTA_QP;
    }

    /* function logic starts */
    if(i4_is_model_valid)
    {
        ASSERT(i4_modelQP != INVALID_QP);
        i8_grwEbf = i8_bitsClipQP - (LWORD64)i4_drain_rate;
        if(((i4_currEbf + i8_grwEbf) > (0.6*i4_vbv_buffer_size)) /*&&
                 i4_modelQP >= i4_clipQP*/)
        {
            /* part of existing scene (i.e. no new scene)
            In which case this is not first I/P/Bref/Bnref etc
            The models for I/P/Bref/Bnref are all valid*/
            if(((i4_currEbf + i8_grwEbf) <
                i4_maxEbf)) /* does not matter whether this is 2pass, 1 pass, VBR, CBR etc*/
            {
                /* clipQP has been determined keeping in view certain other quality constraints like pusling etc.
                So better to honour it if possible*/
                //if (i8_bitsClipQP > i8_drain_rate)
                {
                    LWORD64 i8_thrsh_for_deltaQP_2 = i4_vbv_buffer_size,
                            i8_thrsh_for_deltaQP_1 = i4_vbv_buffer_size;
                    /*even when (modelQP - clipQP) = 0, we intend to QP increase as expected ebf is above 60%*/
                    i4_diffQP = MAX(i4_modelQP - i4_clipQP, 1);
                    switch(ps_rc_lap_out->i4_rc_pic_type)
                    {
                    case IV_I_FRAME:
                    case IV_IDR_FRAME:
                    {
                        i8_thrsh_for_deltaQP_1 =
                            (LWORD64)(f_thrsh_i_pic_delta_qp_1 * i4_vbv_buffer_size);
                        i8_thrsh_for_deltaQP_2 =
                            (LWORD64)(f_thrsh_i_pic_delta_qp_2 * i4_vbv_buffer_size);
                        break;
                    }
                    case IV_P_FRAME:
                    {
                        i8_thrsh_for_deltaQP_1 =
                            (LWORD64)(f_thrsh_p_pic_delta_qp_1 * i4_vbv_buffer_size);
                        i8_thrsh_for_deltaQP_2 =
                            (LWORD64)(f_thrsh_p_pic_delta_qp_2 * i4_vbv_buffer_size);
                        break;
                    }
                    case IV_B_FRAME:
                    {
                        if(ps_rc_lap_out->i4_rc_is_ref_pic)
                        {
                            i8_thrsh_for_deltaQP_1 =
                                (LWORD64)(f_thrsh_br_pic_delta_qp_1 * i4_vbv_buffer_size);
                            i8_thrsh_for_deltaQP_2 =
                                (LWORD64)(f_thrsh_br_pic_delta_qp_2 * i4_vbv_buffer_size);
                        }
                        else
                        {
                            /*as of now using the same thresholds as B reference, later may have to tune if required*/
                            i8_thrsh_for_deltaQP_1 =
                                (LWORD64)(f_thrsh_bnr_pic_delta_qp_1 * i4_vbv_buffer_size);
                            i8_thrsh_for_deltaQP_2 =
                                (LWORD64)(f_thrsh_bnr_pic_delta_qp_2 * i4_vbv_buffer_size);
                        }
                        break;
                    }
                    default:
                        break;
                    }

                    if((i4_currEbf + i8_grwEbf) > i8_thrsh_for_deltaQP_1)
                    {
                        /*For more than 2 QP chnage this means a larger scale issue and probably needs to be handled elsewhere ?*/
                        i4_deltaQP =
                            MIN(2, i4_diffQP); /* we dont intend to change QP by more than 2 */
                        i4_case = 0;
                    }
                    else if((i4_currEbf + i8_grwEbf) > i8_thrsh_for_deltaQP_2)
                    {
                        i4_deltaQP = MIN(1, i4_diffQP);
                        i4_case = 1;
                    }
                }
                /* else  if (i8_bitsClipQP > i8_drain_rate)
        {
                we have no correection, buffer will be healthy after this.
                However, there could be one problem if the currEbf is already close to say 80% of EBF.
                This means we have not reacted well early - needs to be handled?

                This could be the case where it is a simple scene immediately following a complex scene
                and is the I picture (not the first I since model is valid).
                Is this possible - maybe, what to do - dont know?
        }
                */
            }
            else /*(i4_clipQP < i4_maxEbfQP)*/
            {
                i4_deltaQP = 2;
                i4_case = 2;
            }
        }
        if((i4_currEbf + i8_grwEbf) < (0.6 * i4_vbv_buffer_size))
        {
            *pi4_tot_bits_estimated = i8_bitsClipQP;
        }
    }
    else
    {
        if(i4_is_offline_model_used)
        {
            /* this can be only for non-I SCD, where we reset RC */
            WORD32 i4_bits_est_for_in_frm_rc = *pi4_tot_bits_estimated;
            i8_grwEbf = i4_bits_est_for_in_frm_rc - i4_drain_rate;
            if((i4_currEbf + i8_grwEbf) > (f_vbv_thrsh_delta_qp * i4_vbv_buffer_size))
            {
                i4_bits_est_for_in_frm_rc =
                    i4_drain_rate + (WORD32)(0.85 * i4_vbv_buffer_size) - i4_currEbf;
                /* if pi4_tot_bits_estimated becomes less than zero or less than drain rate this indiactes that we are near or above 85% of the buffer */
                /* this needs a reaction */
                if(i4_bits_est_for_in_frm_rc < i4_drain_rate)
                {
                    *pi4_tot_bits_estimated =
                        MAX((i4_drain_rate + (WORD32)(0.95 * i4_vbv_buffer_size) - i4_currEbf),
                            i4_drain_rate);
                    i4_deltaQP = 2; /* this needs some review, needs to be handled well */
                }
            }
            i4_case = 3;
        }
        else
        {
            i8_bitsClipQP = *pi4_tot_bits_estimated;
            i8_grwEbf = i8_bitsClipQP - i4_drain_rate;

            if(((i4_currEbf + i8_grwEbf) <
                i4_maxEbf)) /* does not matter whether this is 2pass, 1 pass, VBR, CBR etc*/
            {
                /* clipQP has been determined keeping in view certain other quality constraints like pusling etc.
                    So better to honour it if possible*/
                //if (i8_bitsClipQP > i8_drain_rate)
                {
                    LWORD64 i8_thrsh_for_deltaQP_2 = i4_vbv_buffer_size,
                            i8_thrsh_for_deltaQP_1 = i4_vbv_buffer_size;

                    switch(ps_rc_lap_out->i4_rc_pic_type)
                    {
                    case IV_I_FRAME:
                    case IV_IDR_FRAME:
                    {
                        i8_thrsh_for_deltaQP_1 =
                            (LWORD64)(f_thrsh_i_pic_delta_qp_1 * i4_vbv_buffer_size);
                        i8_thrsh_for_deltaQP_2 =
                            (LWORD64)(f_thrsh_i_pic_delta_qp_2 * i4_vbv_buffer_size);
                        break;
                    }
                    case IV_P_FRAME:
                    {
                        i8_thrsh_for_deltaQP_1 =
                            (LWORD64)(f_thrsh_p_pic_delta_qp_1 * i4_vbv_buffer_size);
                        i8_thrsh_for_deltaQP_2 =
                            (LWORD64)(f_thrsh_p_pic_delta_qp_2 * i4_vbv_buffer_size);
                        break;
                    }
                    case IV_B_FRAME:
                    {
                        if(ps_rc_lap_out->i4_rc_is_ref_pic)
                        {
                            i8_thrsh_for_deltaQP_1 =
                                (LWORD64)(f_thrsh_br_pic_delta_qp_1 * i4_vbv_buffer_size);
                            i8_thrsh_for_deltaQP_2 =
                                (LWORD64)(f_thrsh_br_pic_delta_qp_2 * i4_vbv_buffer_size);
                        }
                        else
                        {
                            /*as of now using the same thresholds as B reference, later may have to tune if required*/
                            i8_thrsh_for_deltaQP_1 =
                                (LWORD64)(f_thrsh_bnr_pic_delta_qp_1 * i4_vbv_buffer_size);
                            i8_thrsh_for_deltaQP_2 =
                                (LWORD64)(f_thrsh_bnr_pic_delta_qp_2 * i4_vbv_buffer_size);
                        }
                        break;
                    }
                    default:
                        break;
                    }

                    if((i4_currEbf + i8_grwEbf) > i8_thrsh_for_deltaQP_1)
                    {
                        /*For more than 2 QP chnage this means a larger scale issue and probably needs to be handled elsewhere ?*/
                        i4_deltaQP = 2; /* we dont intend to change QP by more than 2 */
                        i4_case = 5;
                    }
                    else if((i4_currEbf + i8_grwEbf) > i8_thrsh_for_deltaQP_2)
                    {
                        i4_deltaQP = 1;
                        i4_case = 6;
                    }
                }
            }
            else
            {
                i4_deltaQP = 2;
                i4_case = 7;
            }
        }
    }
    return i4_deltaQP;
}

/*###############################################*/
/******* END OF RC UTILS FUNCTIONS ***************/
/*###############################################*/

/*########################################################*/
/******* START OF VBV COMPLIANCE FUNCTIONS ***************/
/*#######################################################*/

/*!
******************************************************************************
* \if Function name : ihevce_vbv_compliance_frame_level_update
*
* \brief
*    this function initializes the hrd buffer level to be used for vbv compliance testing using the parameters feeded in VUI parameters
*
* \param[in] *pv_ctxt -> rc context
*           i4_bits_generated -> bits generated from entropy
*           i4_resolution_id -> info needed for log Dump
*           i4_appln_bitrate_inst -> info needed for log Dump
*           u4_cur_cpb_removal_delay_minus1 -> cbp removal delay of present frame
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/

void ihevce_vbv_compliance_frame_level_update(
    void *pv_rc_ctxt,
    WORD32 i4_bits_generated,
    WORD32 i4_resolution_id,
    WORD32 i4_appln_bitrate_inst,
    UWORD32 u4_cur_cpb_removal_delay_minus1)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_rc_ctxt;
    float f_max_vbv_buff_size = (float)ps_rc_ctxt->s_vbv_compliance.f_buffer_size;
    WORD32 i4_cbp_removal_delay_diff = 1;

    if((ps_rc_ctxt->s_vbv_compliance.u4_prev_cpb_removal_delay_minus1 > 0) &&
       (u4_cur_cpb_removal_delay_minus1 >
        ps_rc_ctxt->s_vbv_compliance.u4_prev_cpb_removal_delay_minus1))
        i4_cbp_removal_delay_diff =
            (u4_cur_cpb_removal_delay_minus1 -
             ps_rc_ctxt->s_vbv_compliance.u4_prev_cpb_removal_delay_minus1);

    ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level =
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level - (float)i4_bits_generated +
        (i4_cbp_removal_delay_diff * ps_rc_ctxt->s_vbv_compliance.f_drain_rate);

    ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip =
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level;

    if(ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level < 0)
    {
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level = 0;
    }

    if(ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level >
       ps_rc_ctxt->s_vbv_compliance.f_buffer_size)
    {
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level =
            ps_rc_ctxt->s_vbv_compliance.f_buffer_size;
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip -=
            ps_rc_ctxt->s_vbv_compliance.f_buffer_size;
    }
    else if(ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip > 0)
    {
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip = 0;
    }

    if(ps_rc_ctxt->e_rate_control_type == VBR_STREAMING)
    {
        if(ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip > 0)
            ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level_unclip = 0;
    }
    ps_rc_ctxt->s_vbv_compliance.u4_prev_cpb_removal_delay_minus1 = u4_cur_cpb_removal_delay_minus1;
}

/*!
******************************************************************************
* \if Function name : ihevce_vbv_complaince_init_level
*
* \brief
*    this function initializes the hrd buffer level to be used for vbv compliance testing using the parameters feeded in VUI parameters
*
* \param[in] *pv_ctxt -> rc context
*            *ps_vui      -> VUI parameters
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/

void ihevce_vbv_complaince_init_level(void *pv_ctxt, vui_t *ps_vui)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;

    ps_rc_ctxt->s_vbv_compliance.f_frame_rate =
        (float)((float)ps_vui->u4_vui_time_scale / ps_vui->u4_vui_num_units_in_tick);  //rc_get_frame_rate(ps_rc_ctxt->rc_hdl);

    if(1 == ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
    {
        ASSERT(1 == ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag);

        ps_rc_ctxt->s_vbv_compliance.f_bit_rate = (float)((
            (ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[0].au4_bit_rate_du_value_minus1[0] +
             1)
            << (6 + ps_vui->s_vui_hrd_parameters
                        .u4_bit_rate_scale)));  //rc_get_bit_rate(ps_rc_ctxt->rc_hdl);

        ps_rc_ctxt->s_vbv_compliance.f_buffer_size = (float)((
            (ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[0].au4_cpb_size_du_value_minus1[0] +
             1)
            << (4 + ps_vui->s_vui_hrd_parameters
                        .u4_cpb_size_du_scale)));  //ps_rc_ctxt->u4_max_vbv_buff_size;
    }
    else
    {
        ps_rc_ctxt->s_vbv_compliance.f_bit_rate = (float)((
            (ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[0].au4_bit_rate_value_minus1[0] +
             1)
            << (6 + ps_vui->s_vui_hrd_parameters
                        .u4_bit_rate_scale)));  //rc_get_bit_rate(ps_rc_ctxt->rc_hdl);

        ps_rc_ctxt->s_vbv_compliance.f_buffer_size = (float)((
            (ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[0].au4_cpb_size_value_minus1[0] +
             1)
            << (4 + ps_vui->s_vui_hrd_parameters
                        .u4_cpb_size_scale)));  //ps_rc_ctxt->u4_max_vbv_buff_size;
    }
    ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level =
        (float)ps_rc_ctxt->s_vbv_compliance.f_buffer_size;  //ps_rc_ctxt->u4_max_vbv_buff_size;

    ps_rc_ctxt->s_vbv_compliance.f_drain_rate =
        ((ps_rc_ctxt->s_vbv_compliance.f_bit_rate) / ps_rc_ctxt->s_vbv_compliance.f_frame_rate);

    ps_rc_ctxt->s_vbv_compliance.u4_prev_cpb_removal_delay_minus1 = 0;
}

/*########################################################*/
/******* END OF VBV COMPLIANCE FUNCTIONS *****************/
/*#######################################################*/

/*################################################################*/
/******* START OF DYN CHANGE iN BITRATE FUNCTIONS *****************/
/*################################################################*/
/*!
******************************************************************************
* \if Function name : change_bitrate_vbv_complaince
*
* \brief
*    this function updates the new bitrate and re calculates the drain rate
*
* \param[in] *pv_ctxt -> rc context
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void change_bitrate_vbv_complaince(void *pv_ctxt, LWORD64 i8_new_bitrate, LWORD64 i8_buffer_size)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    ps_rc_ctxt->s_vbv_compliance.f_buffer_size = (float)i8_buffer_size;
    ps_rc_ctxt->s_vbv_compliance.f_bit_rate = (float)i8_new_bitrate;
    if(ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level > i8_buffer_size)
        ps_rc_ctxt->s_vbv_compliance.f_curr_buffer_level = (float)i8_buffer_size;
    ps_rc_ctxt->s_vbv_compliance.f_drain_rate =
        ps_rc_ctxt->s_vbv_compliance.f_bit_rate / ps_rc_ctxt->s_vbv_compliance.f_frame_rate;
}
/*!
******************************************************************************
* \if Function name : ihevce_rc_register_dyn_change_bitrate
*
* \brief
*    this function registers call to change bitrate dynamically.
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/

void ihevce_rc_register_dyn_change_bitrate(
    void *pv_ctxt, LWORD64 i8_new_bitrate, LWORD64 i8_new_peak_bitrate)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    ps_rc_ctxt->i8_new_bitrate = i8_new_bitrate;
    ps_rc_ctxt->i8_new_peak_bitrate = i8_new_peak_bitrate;
    ps_rc_ctxt->i4_bitrate_changed = 1;
    ASSERT(ps_rc_ctxt->i8_new_bitrate > 0);
    ASSERT(ps_rc_ctxt->i8_new_peak_bitrate > 0);
}

/*!
******************************************************************************
* \if Function name : ihevce_rc_get_new_bitrate
*
* \brief
*    get new bitrate
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_rc_get_new_bitrate(void *pv_ctxt)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    return ps_rc_ctxt->i8_new_bitrate;
}
/*!
******************************************************************************
* \if Function name : ihevce_rc_get_new_peak_bitrate
*
* \brief
*    get new peak rate
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_rc_get_new_peak_bitrate(void *pv_ctxt)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    return ps_rc_ctxt->i8_new_peak_bitrate;
}

/*!
******************************************************************************
* \if Function name : ihevce_rc_change_avg_bitrate
*
* \brief
*    change average bitrate configured based on new bitrate
*
* \param[in] *pv_ctxt -> rc context
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_rc_change_avg_bitrate(void *pv_ctxt)
{
    rc_context_t *ps_rc_ctxt = (rc_context_t *)pv_ctxt;
    LWORD64 vbv_buffer_level_b4_change;

    ASSERT(ps_rc_ctxt->i8_new_bitrate != -1);
    ASSERT(ps_rc_ctxt->i8_new_peak_bitrate != -1);
    /*Get the VBV buffer level just before forcing bitrate change*/
    vbv_buffer_level_b4_change = (LWORD64)rc_get_ebf(ps_rc_ctxt->rc_hdl);

    change_avg_bit_rate(
        ps_rc_ctxt->rc_hdl,
        (UWORD32)ps_rc_ctxt->i8_new_bitrate,
        (UWORD32)ps_rc_ctxt->i8_new_peak_bitrate);
    /*Once the request is serviced set new bitrate to -1*/
    ps_rc_ctxt->i8_new_bitrate = -1;
    ps_rc_ctxt->i8_new_peak_bitrate = -1;
    return vbv_buffer_level_b4_change;
}

/*##############################################################*/
/******* END OF DYN CHNAGE iN BITRATE FUNCTIONS *****************/
/*##############################################################*/
