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
* \file ihevce_frame_process.c
*
* \brief
*    This file contains top level functions related Frame processing
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
*
* List of Functions
*
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
#include <time.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

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
#include "ihevc_cabac_tables.h"
#include "ihevc_common_tables.h"

#include "ihevce_defs.h"
#include "ihevce_buffer_que_interface.h"
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_lap_interface.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_checks.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_inter_pred.h"
#include "ihevce_common_utils.h"
#include "ihevce_sub_pic_rc.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_enc_loop_pass.h"
#include "ihevce_trace.h"
#include "ihevce_encode_header.h"
#include "ihevce_encode_header_sei_vui.h"
#include "ihevce_ipe_structs.h"
#include "ihevce_ipe_pass.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_rc_enc_structs.h"
#include "hme_globals.h"
#include "ihevce_me_pass.h"
#include "ihevce_coarse_me_pass.h"
#include "ihevce_frame_process.h"
#include "ihevce_rc_interface.h"
#include "ihevce_profile.h"
#include "ihevce_decomp_pre_intra_structs.h"
#include "ihevce_decomp_pre_intra_pass.h"
#include "ihevce_frame_process_utils.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define REF_MOD_STRENGTH 1.0
#define REF_MAX_STRENGTH 1.4f

/*****************************************************************************/
/* Extern variables                                                          */
/*****************************************************************************/

/**
* @var   QP2QUANT_MD[]
*
* @brief Direct Cost Comoparision Table
*
* @param Comments: Direct cost is compared with 16 * QP2QUANT_MD[Qp]
*                  If direct cost is less  than 16 * QP2QUANT_MD[Qp]
*                  than direct cost is assumed to be zero
*/
const WORD16 QP2QUANT_MD[52] = { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
                                 1,  1,  1,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,
                                 5,  6,  6,  7,  8,  9,  10, 11, 13, 14, 16, 18, 20,
                                 23, 25, 29, 32, 36, 40, 45, 51, 57, 64, 72, 81, 91 };

/*
Gaussian 11x11 window with a sigma of 1.5 - values multiplied by 2048
Window made into 9x9 window as most entries were zero
The center weight has been reduced by 1 after dropping first row/col and last row/col
*/
UWORD8 g_u1_win_size = 9;
UWORD8 g_u1_win_q_shift = 11;
UWORD8 au1_g_win[81] = { 0,  1,  2, 3,  4,  3,   2,   1,   0,  1,  3, 8,  16, 20, 16,  8,   3,
                         1,  2,  8, 24, 48, 60,  48,  24,  8,  2,  3, 16, 48, 93, 116, 93,  48,
                         16, 3,  4, 20, 60, 116, 144, 116, 60, 20, 4, 3,  16, 48, 93,  116, 93,
                         48, 16, 3, 2,  8,  24,  48,  60,  48, 24, 8, 2,  1,  3,  8,   16,  20,
                         16, 8,  3, 1,  0,  1,   2,   3,   4,  3,  2, 1,  0 };

/* lagrange params */
const double lamda_modifier_for_I_pic[8] = { 0.85,   0.7471, 0.6646, 0.5913,
                                             0.5261, 0.4680, 0.4164, 0.3705 };

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_mbr_quality_tool_set_configuration \endif
*
* \brief
*   tool set selection for auxilary bitrate. currently only num intra and inter
*       candidates for auxilary bitrates are controlled
*
* \param[in] ps_enc_loop_thrd_ctxt : enc ctxt
* \param[in] ps_stat_prms: static parameters
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_mbr_quality_tool_set_configuration(
    ihevce_enc_loop_ctxt_t *ps_enc_loop_thrd_ctxt, ihevce_static_cfg_params_t *ps_stat_prms)
{
    /* for single bitrate encoder*/
    switch(ps_stat_prms->s_tgt_lyr_prms.i4_mbr_quality_setting)
    {
    case IHEVCE_MBR_HIGH_QUALITY:
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_intra = 3;
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_inter = 4;
        break;

    case IHEVCE_MBR_MEDIUM_SPEED:
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_intra = 3;
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_inter = 3;
        break;

    case IHEVCE_MBR_HIGH_SPEED:
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_intra = 2;
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_inter = 2;
        break;

    case IHEVCE_MBR_EXTREME_SPEED:
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_intra = 1;
        ps_enc_loop_thrd_ctxt->i4_num_modes_to_evaluate_inter = 1;
        break;

    default:
        assert(0);
        break;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_find_free_indx \endif
*
* \brief
*    Pre encode Frame processing slave thread entry point function
*
* \param[in] Frame processing thread context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_find_free_indx(recon_pic_buf_t **pps_recon_buf_q, WORD32 i4_num_buf)
{
    WORD32 i4_ctr;
    WORD32 i4_is_full = 1;
    WORD32 i4_least_POC = 0x7FFFFFFF;
    WORD32 i4_least_POC_idx = -1;
    WORD32 i4_least_GOP_num = 0x7FFFFFFF;

    for(i4_ctr = 0; i4_ctr < i4_num_buf; i4_ctr++)
    {
        if(pps_recon_buf_q[i4_ctr]->i4_is_free == 1)
        {
            i4_is_full = 0;
            break;
        }
    }
    if(i4_is_full)
    {
        /* remove if any non-reference pictures are present */
        for(i4_ctr = 0; i4_ctr < i4_num_buf; i4_ctr++)
        {
            if(!pps_recon_buf_q[i4_ctr]->i4_is_reference &&
               pps_recon_buf_q[i4_ctr]->i4_non_ref_free_flag)
            {
                i4_least_POC_idx = i4_ctr;
                break;
            }
        }
        /* if all non reference pictures are removed, then find the least poc
        in the least gop number*/
        if(i4_least_POC_idx == -1)
        {
            for(i4_ctr = 0; i4_ctr < i4_num_buf; i4_ctr++)
            {
                if(i4_least_GOP_num > pps_recon_buf_q[i4_ctr]->i4_idr_gop_num)
                {
                    i4_least_GOP_num = pps_recon_buf_q[i4_ctr]->i4_idr_gop_num;
                }
            }
            for(i4_ctr = 0; i4_ctr < i4_num_buf; i4_ctr++)
            {
                if(i4_least_POC > pps_recon_buf_q[i4_ctr]->i4_poc &&
                   i4_least_GOP_num == pps_recon_buf_q[i4_ctr]->i4_idr_gop_num)
                {
                    i4_least_POC = pps_recon_buf_q[i4_ctr]->i4_poc;
                    i4_least_POC_idx = i4_ctr;
                }
            }
        }
    }
    return i4_least_POC_idx;
}

/*!
******************************************************************************
* \if Function name : complexity_RC_reset_marking \endif
*
* \brief
*   this function the complexity variation and set the complexity change flag for
*   rate control to reset the model
*
* \param[in] ps_enc_loop_thrd_ctxt : enc ctxt
* \param[in] ps_stat_prms: static parameters
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void complexity_RC_reset_marking(enc_ctxt_t *ps_enc_ctxt, WORD32 i4_cur_ipe_idx, WORD32 i4_end_flag)
{
    rc_lap_out_params_t *ps_cur_ipe_lap_out;
    rc_lap_out_params_t *ps_lap_out_temp;
    WORD32 i4_max_temporal_layers;

    ps_cur_ipe_lap_out =
        &ps_enc_ctxt->s_multi_thrd.aps_curr_inp_pre_enc[i4_cur_ipe_idx]->s_rc_lap_out;
    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_model = 0;
    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_bits = 0;

    i4_max_temporal_layers = ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_temporal_layers;

    /*reset the RC_reset counter at reset points*/
    if(ps_cur_ipe_lap_out->i4_is_I_only_scd || ps_cur_ipe_lap_out->i4_is_non_I_scd ||
       ps_cur_ipe_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
    {
        ps_enc_ctxt->i4_past_RC_reset_count = 0;
    }

    if(ps_cur_ipe_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
    {
        ps_enc_ctxt->i4_past_RC_scd_reset_count = 0;
    }
    ps_enc_ctxt->i4_past_RC_reset_count++;
    ps_enc_ctxt->i4_past_RC_scd_reset_count++;

    /*complexity based rate control reset */

    if((ps_cur_ipe_lap_out->i4_rc_pic_type == IV_P_FRAME ||
        ps_cur_ipe_lap_out->i4_rc_pic_type == IV_I_FRAME) &&
       (i4_max_temporal_layers > 1) && (!i4_end_flag) &&
       (ps_enc_ctxt->s_multi_thrd.i4_delay_pre_me_btw_l0_ipe > (2 * (1 << i4_max_temporal_layers))))
    {
        WORD32 i4_is_cur_pic_high_complex_region =
            ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[i4_cur_ipe_idx]
                ->i4_is_high_complex_region;
        WORD32 i4_next_ipe_idx;
        WORD32 i4_next_next_ipe_idx;
        WORD32 i4_temp_ipe_idx;
        WORD32 i;

        ps_enc_ctxt->i4_future_RC_reset = 0;
        ps_enc_ctxt->i4_future_RC_scd_reset = 0;
        ASSERT(i4_is_cur_pic_high_complex_region != -1);

        /*get the next idx of p/i picture */
        i4_next_ipe_idx = (i4_cur_ipe_idx + 1);
        if(i4_next_ipe_idx >= ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe)
        {
            i4_next_ipe_idx = 0;
        }
        i4_temp_ipe_idx = i4_next_ipe_idx;
        for(i = 0; i < (1 << i4_max_temporal_layers); i++)
        {
            ps_lap_out_temp =
                &ps_enc_ctxt->s_multi_thrd.aps_curr_inp_pre_enc[i4_next_ipe_idx]->s_rc_lap_out;

            if(ps_lap_out_temp->i4_rc_pic_type == IV_P_FRAME ||
               ps_lap_out_temp->i4_rc_pic_type == IV_I_FRAME)
            {
                break;
            }
            i4_next_ipe_idx++;
            if(i4_next_ipe_idx >= ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe)
            {
                i4_next_ipe_idx = 0;
            }
        }
        /* get the next idx of next p/i picture*/
        i4_next_next_ipe_idx = (i4_next_ipe_idx + 1);
        if(i4_next_next_ipe_idx >= ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe)
        {
            i4_next_next_ipe_idx = 0;
        }
        for(i = 0; i < (1 << i4_max_temporal_layers); i++)
        {
            ps_lap_out_temp =
                &ps_enc_ctxt->s_multi_thrd.aps_curr_inp_pre_enc[i4_next_next_ipe_idx]->s_rc_lap_out;

            if(ps_lap_out_temp->i4_rc_pic_type == IV_P_FRAME ||
               ps_lap_out_temp->i4_rc_pic_type == IV_I_FRAME)
            {
                break;
            }
            i4_next_next_ipe_idx++;
            if(i4_next_next_ipe_idx >= ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe)
            {
                i4_next_next_ipe_idx = 0;
            }
        }

        /*check for any possible RC reset in the future 8 frames*/
        for(i = 0; i < 8; i++)
        {
            ps_lap_out_temp =
                &ps_enc_ctxt->s_multi_thrd.aps_curr_inp_pre_enc[i4_temp_ipe_idx]->s_rc_lap_out;

            if(ps_lap_out_temp->i4_is_I_only_scd || ps_lap_out_temp->i4_is_non_I_scd ||
               ps_lap_out_temp->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
            {
                ps_enc_ctxt->i4_future_RC_reset = 1;
            }
            if(ps_cur_ipe_lap_out->i4_rc_scene_type == SCENE_TYPE_SCENE_CUT)
            {
                ps_enc_ctxt->i4_future_RC_scd_reset = 1;
            }
            i4_temp_ipe_idx++;
            if(i4_temp_ipe_idx >= ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe)
            {
                i4_temp_ipe_idx = 0;
            }
        }

        if((!ps_enc_ctxt->i4_future_RC_reset) && (ps_enc_ctxt->i4_past_RC_reset_count > 8))
        {
            /*if the prev two P/I pic is not in high complex region
            then enable reset RC flag*/
            if((!ps_enc_ctxt->ai4_is_past_pic_complex[0]) &&
               (!ps_enc_ctxt->ai4_is_past_pic_complex[1]))
            {
                if(i4_is_cur_pic_high_complex_region)
                {
                    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_model = 1;
                    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_bits = 1;
                    ps_enc_ctxt->i4_is_I_reset_done = 0;
                }
            }

            /*if the next two P/I pic is not in high complex region
            then enable reset RC flag*/
            if((!ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[i4_next_ipe_idx]
                     ->i4_is_high_complex_region) &&
               (!ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[i4_next_next_ipe_idx]
                     ->i4_is_high_complex_region))
            {
                if(i4_is_cur_pic_high_complex_region)
                {
                    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_model = 1;
                    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_bits = 1;
                    ps_enc_ctxt->i4_is_I_reset_done = 0;
                }
            }
        }
        else if((!ps_enc_ctxt->i4_future_RC_scd_reset) && (ps_enc_ctxt->i4_past_RC_scd_reset_count > 8))
        {
            /*if the prev two P/I pic is not in high complex region
            then enable reset RC flag*/
            if((!ps_enc_ctxt->ai4_is_past_pic_complex[0]) &&
               (!ps_enc_ctxt->ai4_is_past_pic_complex[1]))
            {
                if(i4_is_cur_pic_high_complex_region)
                {
                    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_bits = 1;
                }
            }

            /*if the next two P/I pic is not in high complex region
            then enable reset RC flag*/
            if((!ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[i4_next_ipe_idx]
                     ->i4_is_high_complex_region) &&
               (!ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[i4_next_next_ipe_idx]
                     ->i4_is_high_complex_region))
            {
                if(i4_is_cur_pic_high_complex_region)
                {
                    ps_cur_ipe_lap_out->i4_is_cmplx_change_reset_bits = 1;
                }
            }
        }

        /* forcing I frame reset after complexity change is disable as it gives gain, could be due to that
        required i reset is already happening on pre Intra SAD*/
        /*if(!ps_enc_ctxt->i4_is_I_reset_done && (ps_cur_ipe_lap_out->i4_pic_type
        == IV_I_FRAME))
        {
            ps_cur_ipe_lap_out->i4_is_I_only_scd = 1;
            ps_enc_ctxt->i4_is_I_reset_done = 1;
        }*/

        ps_enc_ctxt->ai4_is_past_pic_complex[0] = i4_is_cur_pic_high_complex_region;

        ps_enc_ctxt->ai4_is_past_pic_complex[1] = ps_enc_ctxt->ai4_is_past_pic_complex[0];
    }
    return;
}
/*!
******************************************************************************
* \if Function name : ihevce_manage_ref_pics \endif
*
* \brief
*    Reference picture management based on delta poc array given by LAP
*    Populates the reference list after removing non used reference pictures
*    populates the delta poc of reference pics to be signalled in slice header
*
* \param[in] encoder context pointer
* \param[in] current LAP Encoder buffer pointer
* \param[in] current frame process and entropy buffer pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_pre_enc_manage_ref_pics(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    pre_enc_me_ctxt_t *ps_curr_out,
    WORD32 i4_ping_pong)
{
    /* local variables */
    WORD32 ctr;
    WORD32 ref_pics;
    WORD32 ai4_buf_status[HEVCE_MAX_DPB_PICS] = { 0 };
    WORD32 curr_poc;
    WORD32 wp_flag = 0;
    WORD32 num_ref_pics_list0 = 0;
    WORD32 num_ref_pics_list1 = 0;
    WORD32 cra_poc = ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc;
    WORD32 slice_type = ps_curr_out->s_slice_hdr.i1_slice_type;
    recon_pic_buf_t *(*aps_pre_enc_ref_pic_list)[HEVCE_MAX_REF_PICS * 2];
    WORD32 i4_inc_L1_active_ref_pic = 0;
    WORD32 i4_inc_L0_active_ref_pic = 0;

    (void)ps_curr_out;
    curr_poc = ps_curr_inp->s_lap_out.i4_poc;

    /* Number of reference pics given by LAP should not be greater than max */
    ASSERT(HEVCE_MAX_REF_PICS >= ps_curr_inp->s_lap_out.i4_num_ref_pics);

    /*derive ref_pic_list based on ping_pong instance */
    aps_pre_enc_ref_pic_list = ps_enc_ctxt->aps_pre_enc_ref_lists[i4_ping_pong];

    /* derive the weighted prediction enable flag based on slice type */
    if(BSLICE == slice_type)
    {
        wp_flag = ps_curr_inp->s_lap_out.i1_weighted_bipred_flag;
    }
    else if(PSLICE == slice_type)
    {
        wp_flag = ps_curr_inp->s_lap_out.i1_weighted_pred_flag;
    }
    else
    {
        wp_flag = 0;
    }

    /*to support diplicate pics*/
    {
        WORD32 i, j;
        for(i = 0; i < 2; i++)
        {
            for(j = 0; j < HEVCE_MAX_REF_PICS * 2; j++)
            {
                aps_pre_enc_ref_pic_list[i][j] =
                    &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][i][j];
            }
        }
    }

    /* run a loop over the number of reference pics given by LAP */
    for(ref_pics = 0; ref_pics < ps_curr_inp->s_lap_out.i4_num_ref_pics; ref_pics++)
    {
        WORD32 ref_poc;
        WORD32 i4_loop = 1;
        WORD32 i4_temp_list;

        ref_poc = curr_poc + ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_ref_pic_delta_poc;

        /* run a loop to check the poc based on delta poc array */
        for(ctr = 0; ctr < ps_enc_ctxt->i4_pre_enc_num_buf_recon_q; ctr++)
        {
            /* if the POC is matching with current ref picture*/
            if((ref_poc == ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr]->i4_poc) &&
               (0 == ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr]->i4_is_free))
            {
                /* mark the buf status as used */
                ai4_buf_status[ctr] = 1;

                /* populate the reference lists based on delta poc array */
                if((ref_poc < curr_poc) || (0 == curr_poc))
                {
                    /* list 0 */
                    memcpy(
                        &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_0][num_ref_pics_list0],
                        ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr],
                        sizeof(recon_pic_buf_t));
                    i4_temp_list = num_ref_pics_list0;

                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        /* list 0 */
                        i4_temp_list++;
                        memcpy(
                            &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_0][i4_temp_list],
                            ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr],
                            sizeof(recon_pic_buf_t));
                        i4_loop++;
                    }

                    /* populate weights and offsets corresponding to this ref pic */
                    memcpy(
                        &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_0][num_ref_pics_list0]
                             .s_weight_offset,
                        &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[0],
                        sizeof(ihevce_wght_offst_t));

                    /* Store the used as ref for current pic flag  */
                    ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_0][num_ref_pics_list0]
                        .i4_used_by_cur_pic_flag =
                        ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                    num_ref_pics_list0++;
                    i4_loop = 1;
                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        /* populate weights and offsets corresponding to this ref pic */
                        memcpy(
                            &ps_enc_ctxt
                                 ->as_pre_enc_ref_lists[i4_ping_pong][LIST_0][num_ref_pics_list0]
                                 .s_weight_offset,
                            &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[i4_loop],
                            sizeof(ihevce_wght_offst_t));

                        /* Store the used as ref for current pic flag  */
                        ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_0][num_ref_pics_list0]
                            .i4_used_by_cur_pic_flag =
                            ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                        num_ref_pics_list0++;
                        i4_loop++;
                    }
                }
                else
                {
                    /* list 1 */
                    memcpy(
                        &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_1][num_ref_pics_list1],
                        ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr],
                        sizeof(recon_pic_buf_t));

                    i4_temp_list = num_ref_pics_list1;
                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        /* list 1 */
                        i4_temp_list++;
                        memcpy(
                            &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_1][i4_temp_list],
                            ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr],
                            sizeof(recon_pic_buf_t));
                        i4_loop++;
                    }

                    /* populate weights and offsets corresponding to this ref pic */
                    memcpy(
                        &ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_1][num_ref_pics_list1]
                             .s_weight_offset,
                        &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[0],
                        sizeof(ihevce_wght_offst_t));

                    /* Store the used as ref for current pic flag  */
                    ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_1][num_ref_pics_list1]
                        .i4_used_by_cur_pic_flag =
                        ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                    num_ref_pics_list1++;
                    i4_loop = 1;
                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        /* populate weights and offsets corresponding to this ref pic */
                        memcpy(
                            &ps_enc_ctxt
                                 ->as_pre_enc_ref_lists[i4_ping_pong][LIST_1][num_ref_pics_list1]
                                 .s_weight_offset,
                            &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[i4_loop],
                            sizeof(ihevce_wght_offst_t));

                        /* Store the used as ref for current pic flag  */
                        ps_enc_ctxt->as_pre_enc_ref_lists[i4_ping_pong][LIST_1][num_ref_pics_list1]
                            .i4_used_by_cur_pic_flag =
                            ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                        num_ref_pics_list1++;
                        i4_loop++;
                    }
                }
                break;
            }
        }

        /* if the reference picture is not found then error */
        ASSERT(ctr != ps_enc_ctxt->i4_pre_enc_num_buf_recon_q);
    }
    /* sort the reference pics in List0 in descending order POC */
    if(num_ref_pics_list0 > 1)
    {
        /* run a loop for num ref pics -1 */
        for(ctr = 0; ctr < num_ref_pics_list0 - 1; ctr++)
        {
            WORD32 max_idx = ctr;
            recon_pic_buf_t *ps_temp;
            WORD32 i;

            for(i = (ctr + 1); i < num_ref_pics_list0; i++)
            {
                /* check for poc greater than current ref poc */
                if(aps_pre_enc_ref_pic_list[LIST_0][i]->i4_poc >
                   aps_pre_enc_ref_pic_list[LIST_0][max_idx]->i4_poc)
                {
                    max_idx = i;
                }
            }

            /* if max of remaining is not current, swap the pointers */
            if(max_idx != ctr)
            {
                ps_temp = aps_pre_enc_ref_pic_list[LIST_0][max_idx];
                aps_pre_enc_ref_pic_list[LIST_0][max_idx] = aps_pre_enc_ref_pic_list[LIST_0][ctr];
                aps_pre_enc_ref_pic_list[LIST_0][ctr] = ps_temp;
            }
        }
    }

    /* sort the reference pics in List1 in ascending order POC */
    if(num_ref_pics_list1 > 1)
    {
        /* run a loop for num ref pics -1 */
        for(ctr = 0; ctr < num_ref_pics_list1 - 1; ctr++)
        {
            WORD32 min_idx = ctr;
            recon_pic_buf_t *ps_temp;
            WORD32 i;

            for(i = (ctr + 1); i < num_ref_pics_list1; i++)
            {
                /* check for p[oc less than current ref poc */
                if(aps_pre_enc_ref_pic_list[LIST_1][i]->i4_poc <
                   aps_pre_enc_ref_pic_list[LIST_1][min_idx]->i4_poc)
                {
                    min_idx = i;
                }
            }

            /* if min of remaining is not current, swap the pointers */
            if(min_idx != ctr)
            {
                ps_temp = aps_pre_enc_ref_pic_list[LIST_1][min_idx];
                aps_pre_enc_ref_pic_list[LIST_1][min_idx] = aps_pre_enc_ref_pic_list[LIST_1][ctr];
                aps_pre_enc_ref_pic_list[LIST_1][ctr] = ps_temp;
            }
        }
    }

    /* call the ME API to update the DPB of HME pyramids coarse layers */
    ihevce_coarse_me_frame_dpb_update(
        ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
        num_ref_pics_list0,
        num_ref_pics_list1,
        &aps_pre_enc_ref_pic_list[LIST_0][0],
        &aps_pre_enc_ref_pic_list[LIST_1][0]);

    /* Default list creation based on uses as ref pic for current pic flag */
    {
        WORD32 num_ref_pics_list_final = 0;
        WORD32 list_idx = 0;

        /* LIST 0 */
        /* run a loop for num ref pics in list 0 */
        for(ctr = 0; ctr < num_ref_pics_list0; ctr++)
        {
            /* check for used as reference flag */
            if(1 == aps_pre_enc_ref_pic_list[LIST_0][ctr]->i4_used_by_cur_pic_flag)
            {
                /* copy the pointer to the actual valid list idx */
                aps_pre_enc_ref_pic_list[LIST_0][list_idx] = aps_pre_enc_ref_pic_list[LIST_0][ctr];

                /* increment the valid pic counters and idx */
                list_idx++;
                num_ref_pics_list_final++;
            }
        }

        /* finally store the number of pictures in List0 */
        num_ref_pics_list0 = num_ref_pics_list_final;
        /* LIST 1 */
        num_ref_pics_list_final = 0;
        list_idx = 0;

        /* run a loop for num ref pics in list 1 */
        for(ctr = 0; ctr < num_ref_pics_list1; ctr++)
        {
            /* check for used as reference flag */
            if(1 == aps_pre_enc_ref_pic_list[LIST_1][ctr]->i4_used_by_cur_pic_flag)
            {
                /* copy the pointer to the actual valid list idx */
                aps_pre_enc_ref_pic_list[LIST_1][list_idx] = aps_pre_enc_ref_pic_list[LIST_1][ctr];

                /* increment the valid pic counters and idx */
                list_idx++;
                num_ref_pics_list_final++;
            }
        }

        /* finally store the number of pictures in List1 */
        num_ref_pics_list1 = num_ref_pics_list_final;
    }
    /*in case of single active ref picture on L0 and L1, then consider one of them weighted
    and another non-weighted*/
    if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
    {
        if(num_ref_pics_list0 > 2)
        {
            if(aps_pre_enc_ref_pic_list[LIST_0][0]->i4_poc ==
               aps_pre_enc_ref_pic_list[LIST_0][1]->i4_poc)
            {
                i4_inc_L0_active_ref_pic = 1;
            }
        }
    }
    else
    {
        if(num_ref_pics_list0 >= 2 && num_ref_pics_list1 >= 2)
        {
            if(aps_pre_enc_ref_pic_list[LIST_0][0]->i4_poc ==
               aps_pre_enc_ref_pic_list[LIST_0][1]->i4_poc)
            {
                i4_inc_L0_active_ref_pic = 1;
            }
            if(aps_pre_enc_ref_pic_list[LIST_1][0]->i4_poc ==
               aps_pre_enc_ref_pic_list[LIST_1][1]->i4_poc)
            {
                i4_inc_L1_active_ref_pic = 1;
            }
        }
    }

    /* append the reference pics in List1 and end of list0 */
    for(ctr = 0; ctr < num_ref_pics_list1; ctr++)
    {
        aps_pre_enc_ref_pic_list[LIST_0][num_ref_pics_list0 + ctr] =
            aps_pre_enc_ref_pic_list[LIST_1][ctr];
    }

    /* append the reference pics in List0 and end of list1 */
    for(ctr = 0; ctr < num_ref_pics_list0; ctr++)
    {
        aps_pre_enc_ref_pic_list[LIST_1][num_ref_pics_list1 + ctr] =
            aps_pre_enc_ref_pic_list[LIST_0][ctr];
    }

    /* reference list modification for adding duplicate reference */
    {

    }

    /* popluate the default weights and offsets for disabled cases */
    {
        WORD32 i;

        /* populate the weights and offsets for all pics in L0 + L1 */
        for(i = 0; i < (num_ref_pics_list0 + num_ref_pics_list1); i++)
        {
            /* populate the weights and offsets if weighted prediction is disabled */
            if(1 == wp_flag)
            {
                /* if weights are disabled then populate default values */
                if(0 ==
                   aps_pre_enc_ref_pic_list[LIST_0][i]->s_weight_offset.u1_luma_weight_enable_flag)
                {
                    /* set to default values */
                    aps_pre_enc_ref_pic_list[LIST_0][i]->s_weight_offset.i2_luma_weight =
                        (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                    aps_pre_enc_ref_pic_list[LIST_0][i]->s_weight_offset.i2_luma_offset = 0;
                }
            }
        }

        for(i = 0; i < (num_ref_pics_list0 + num_ref_pics_list1); i++)
        {
            /* populate the weights and offsets if weighted prediction is enabled */
            if(1 == wp_flag)
            {
                /* if weights are disabled then populate default values */
                if(0 ==
                   aps_pre_enc_ref_pic_list[LIST_1][i]->s_weight_offset.u1_luma_weight_enable_flag)
                {
                    /* set to default values */
                    aps_pre_enc_ref_pic_list[LIST_1][i]->s_weight_offset.i2_luma_weight =
                        (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                    aps_pre_enc_ref_pic_list[LIST_1][i]->s_weight_offset.i2_luma_offset = 0;
                }
            }
        }
    }

    /* run a loop to free the non used reference pics */
    for(ctr = 0; ctr < ps_enc_ctxt->i4_pre_enc_num_buf_recon_q; ctr++)
    {
        /* if not used as reference */
        if(0 == ai4_buf_status[ctr])
        {
            ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr]->i4_is_free = 1;
            ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr]->i4_poc = -1;
        }
    }

    /* store the number of reference pics in the list for ME/MC etc */
    ps_enc_ctxt->i4_pre_enc_num_ref_l0 = num_ref_pics_list0;
    ps_enc_ctxt->i4_pre_enc_num_ref_l1 = num_ref_pics_list1;

#define HME_USE_ONLY_2REF
#ifndef HME_USE_ONLY_2REF
    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = num_ref_pics_list0;
    ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = num_ref_pics_list1;
#else
#if MULTI_REF_ENABLE == 1
    if(ps_curr_inp->s_lap_out.i4_quality_preset >= IHEVCE_QUALITY_P3)
    {
        if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
        {
            if(IHEVCE_QUALITY_P6 == ps_curr_inp->s_lap_out.i4_quality_preset)
            {
                if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                {
                    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active =
                        MIN(MAX_NUM_REFS_IN_PPICS_IN_XS25 + 1, num_ref_pics_list0);
                }
                else
                {
                    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active =
                        MIN(MAX_NUM_REFS_IN_PPICS_IN_XS25, num_ref_pics_list0);
                    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active += i4_inc_L0_active_ref_pic;
                }

                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = 0;
            }
            else
            {
                if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                {
                    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(3, num_ref_pics_list0);
                }
                else
                {
                    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(2, num_ref_pics_list0);
                    ps_enc_ctxt->i4_pre_enc_num_ref_l0_active += i4_inc_L0_active_ref_pic;
                }

                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = 0;
            }
        }
        else
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
            {
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(2, num_ref_pics_list0);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = MIN(1, num_ref_pics_list1);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active += i4_inc_L1_active_ref_pic;
            }
            else
            {
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(1, num_ref_pics_list0);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = MIN(1, num_ref_pics_list1);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active += i4_inc_L1_active_ref_pic;
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active += i4_inc_L0_active_ref_pic;
            }
        }
    }
    else
    {
        if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(4, num_ref_pics_list0);
            else
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(4, num_ref_pics_list0);

            ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = 0;
        }
        else
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
            {
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(4, num_ref_pics_list0);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = MIN(4, num_ref_pics_list1);
            }
            else
            {
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(4, num_ref_pics_list0);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = MIN(4, num_ref_pics_list1);
            }
        }
    }
#else
    {
        if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(3, num_ref_pics_list0);
            else
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(2, num_ref_pics_list0);

            ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = 0;
        }
        else
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
            {
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(2, num_ref_pics_list0);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = MIN(1, num_ref_pics_list1);
            }
            else
            {
                ps_enc_ctxt->i4_pre_enc_num_ref_l0_active = MIN(1, num_ref_pics_list0);
                ps_enc_ctxt->i4_pre_enc_num_ref_l1_active = MIN(1, num_ref_pics_list1);
            }
        }
    }
#endif
#endif

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_manage_ref_pics \endif
*
* \brief
*    Reference picture management based on delta poc array given by LAP
*    Populates the reference list after removing non used reference pictures
*    populates the delta poc of reference pics to be signalled in slice header
*
* \param[in] encoder context pointer
* \param[in] current LAP Encoder buffer pointer
* \param[in] current frame process and entropy buffer pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_manage_ref_pics(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    slice_header_t *ps_slice_header,
    WORD32 i4_me_frm_id,
    WORD32 i4_thrd_id,
    WORD32 i4_bitrate_instance_id)
{
    WORD32 ctr;
    WORD32 ref_pics;
    WORD32 curr_poc, curr_idr_gop_num;
    WORD32 wp_flag;
    WORD32 num_ref_pics_list0 = 0;
    WORD32 num_ref_pics_list1 = 0;
    WORD32 cra_poc = ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc;
    WORD32 slice_type = ps_slice_header->i1_slice_type;
    recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2];
    recon_pic_buf_t(*aps_ref_list_temp)[HEVCE_MAX_REF_PICS * 2];
    WORD32 i4_num_rpics_l0_excl_dup;
    WORD32 i4_num_rpics_l1_excl_dup;
    WORD32 i4_inc_L1_active_ref_pic = 0;
    WORD32 i4_inc_L0_active_ref_pic = 0;
    WORD32 i4_bridx = i4_bitrate_instance_id;  //bitrate instance index
    WORD32 i4_resolution_id = ps_enc_ctxt->i4_resolution_id;
    me_enc_rdopt_ctxt_t *ps_cur_out_me_prms;
    recon_pic_buf_t ***ppps_recon_bufs = ps_enc_ctxt->pps_recon_buf_q;
    WORD32 i4_num_recon_bufs = ps_enc_ctxt->ai4_num_buf_recon_q[i4_bridx];

    ps_cur_out_me_prms = ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id];

    /*to support diplicate pics*/
    {
        WORD32 i, j;
        for(i = 0; i < NUM_REF_LISTS; i++)
        {
            for(j = 0; j < HEVCE_MAX_REF_PICS * 2; j++)
            {
                ps_cur_out_me_prms->aps_ref_list[i4_bridx][i][j] =
                    &ps_cur_out_me_prms->as_ref_list[i4_bridx][i][j];
            }
        }
    }

    aps_ref_list = ps_cur_out_me_prms->aps_ref_list[i4_bridx];
    aps_ref_list_temp = ps_cur_out_me_prms->as_ref_list[i4_bridx];

    curr_poc = ps_curr_inp->s_lap_out.i4_poc;
    curr_idr_gop_num = ps_curr_inp->s_lap_out.i4_idr_gop_num;

    /* Number of reference pics given by LAP should not be greater than max */
    ASSERT(HEVCE_MAX_REF_PICS >= ps_curr_inp->s_lap_out.i4_num_ref_pics);

    /* derive the weighted prediction enable flag based on slice type */
    if(BSLICE == slice_type)
    {
        wp_flag = ps_curr_inp->s_lap_out.i1_weighted_bipred_flag;
    }
    else if(PSLICE == slice_type)
    {
        wp_flag = ps_curr_inp->s_lap_out.i1_weighted_pred_flag;
    }
    else
    {
        wp_flag = 0;
    }

    ps_slice_header->s_rplm.i1_ref_pic_list_modification_flag_l0 = 0;
    ps_slice_header->s_rplm.i1_ref_pic_list_modification_flag_l1 = 0;
    ASSERT(curr_poc != INVALID_POC);

    /* run a loop over the number of reference pics given by LAP */
    for(ref_pics = 0; ref_pics < ps_curr_inp->s_lap_out.i4_num_ref_pics; ref_pics++)
    {
        WORD32 ref_poc;
        WORD32 i4_loop = 1;
        WORD32 i4_temp_list;

        ref_poc = curr_poc + ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_ref_pic_delta_poc;
        if((0 == curr_poc) && curr_idr_gop_num)
        {
            curr_idr_gop_num -= 1;
        }
        ASSERT(ref_poc != INVALID_POC);
        /* run a loop to check the poc based on delta poc array */
        for(ctr = 0; ctr < i4_num_recon_bufs; ctr++)
        {
            /* if the POC is matching with current ref picture*/
            if((ref_poc == ppps_recon_bufs[i4_bridx][ctr]->i4_poc) &&
               (0 == ppps_recon_bufs[i4_bridx][ctr]->i4_is_free) &&
               (curr_idr_gop_num == ppps_recon_bufs[i4_bridx][ctr]->i4_idr_gop_num))
            {
                /* populate the reference lists based on delta poc array */
                if((ref_poc < curr_poc) || (0 == curr_poc))
                {
                    /* list 0 */
                    memcpy(
                        &aps_ref_list_temp[LIST_0][num_ref_pics_list0],
                        ppps_recon_bufs[i4_bridx][ctr],
                        sizeof(recon_pic_buf_t));

                    i4_temp_list = num_ref_pics_list0;

                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        i4_temp_list++;
                        /* list 0 */
                        memcpy(
                            &aps_ref_list_temp[LIST_0][i4_temp_list],
                            ppps_recon_bufs[i4_bridx][ctr],
                            sizeof(recon_pic_buf_t));
                        i4_loop++;
                    }

                    /* populate weights and offsets corresponding to this ref pic */
                    memcpy(
                        &aps_ref_list_temp[LIST_0][num_ref_pics_list0].s_weight_offset,
                        &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[0],
                        sizeof(ihevce_wght_offst_t));

                    /* Store the used as ref for current pic flag  */
                    aps_ref_list_temp[LIST_0][num_ref_pics_list0].i4_used_by_cur_pic_flag =
                        ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                    if(wp_flag)
                    {
                        WORD16 i2_luma_weight = (aps_ref_list[LIST_0][num_ref_pics_list0]
                                                     ->s_weight_offset.i2_luma_weight);

                        aps_ref_list[LIST_0][num_ref_pics_list0]->i4_inv_luma_wt =
                            ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                        aps_ref_list[LIST_0][num_ref_pics_list0]->i4_log2_wt_denom =
                            ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                    }
                    else
                    {
                        WORD16 i2_luma_weight =
                            (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                        aps_ref_list[LIST_0][num_ref_pics_list0]->s_weight_offset.i2_luma_weight =
                            i2_luma_weight;

                        aps_ref_list[LIST_0][num_ref_pics_list0]->i4_inv_luma_wt =
                            ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                        aps_ref_list[LIST_0][num_ref_pics_list0]->i4_log2_wt_denom =
                            ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                    }

                    num_ref_pics_list0++;
                    i4_loop = 1;

                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        /* populate weights and offsets corresponding to this ref pic */
                        memcpy(
                            &aps_ref_list_temp[LIST_0][num_ref_pics_list0].s_weight_offset,
                            &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[i4_loop],
                            sizeof(ihevce_wght_offst_t));

                        /* Store the used as ref for current pic flag  */
                        aps_ref_list_temp[LIST_0][num_ref_pics_list0].i4_used_by_cur_pic_flag =
                            ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                        if(wp_flag)
                        {
                            WORD16 i2_luma_weight = (aps_ref_list[LIST_0][num_ref_pics_list0]
                                                         ->s_weight_offset.i2_luma_weight);

                            aps_ref_list[LIST_0][num_ref_pics_list0]->i4_inv_luma_wt =
                                ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                            aps_ref_list[LIST_0][num_ref_pics_list0]->i4_log2_wt_denom =
                                ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                        }
                        else
                        {
                            WORD16 i2_luma_weight =
                                (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                            aps_ref_list[LIST_0][num_ref_pics_list0]
                                ->s_weight_offset.i2_luma_weight = i2_luma_weight;

                            aps_ref_list[LIST_0][num_ref_pics_list0]->i4_inv_luma_wt =
                                ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                            aps_ref_list[LIST_0][num_ref_pics_list0]->i4_log2_wt_denom =
                                ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                        }

                        num_ref_pics_list0++;
                        i4_loop++;
                        ps_slice_header->s_rplm.i1_ref_pic_list_modification_flag_l0 = 1;
                        ps_slice_header->s_rplm.i1_ref_pic_list_modification_flag_l1 = 1;
                    }
                }
                else
                {
                    /* list 1 */
                    memcpy(
                        &aps_ref_list_temp[LIST_1][num_ref_pics_list1],
                        ppps_recon_bufs[i4_bridx][ctr],
                        sizeof(recon_pic_buf_t));
                    i4_temp_list = num_ref_pics_list1;
                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        i4_temp_list++;
                        /* list 1 */
                        memcpy(
                            &aps_ref_list_temp[LIST_1][i4_temp_list],
                            ppps_recon_bufs[i4_bridx][ctr],
                            sizeof(recon_pic_buf_t));
                        i4_loop++;
                    }

                    /* populate weights and offsets corresponding to this ref pic */
                    memcpy(
                        &aps_ref_list_temp[LIST_1][num_ref_pics_list1].s_weight_offset,
                        &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[0],
                        sizeof(ihevce_wght_offst_t));

                    /* Store the used as ref for current pic flag  */
                    aps_ref_list_temp[LIST_1][num_ref_pics_list1].i4_used_by_cur_pic_flag =
                        ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                    if(wp_flag)
                    {
                        WORD16 i2_luma_weight = (aps_ref_list[LIST_1][num_ref_pics_list1]
                                                     ->s_weight_offset.i2_luma_weight);

                        aps_ref_list[LIST_1][num_ref_pics_list1]->i4_inv_luma_wt =
                            ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                        aps_ref_list[LIST_1][num_ref_pics_list1]->i4_log2_wt_denom =
                            ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                    }
                    else
                    {
                        WORD16 i2_luma_weight =
                            (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                        aps_ref_list[LIST_1][num_ref_pics_list1]->s_weight_offset.i2_luma_weight =
                            i2_luma_weight;

                        aps_ref_list[LIST_1][num_ref_pics_list1]->i4_inv_luma_wt =
                            ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                        aps_ref_list[LIST_1][num_ref_pics_list1]->i4_log2_wt_denom =
                            ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                    }

                    num_ref_pics_list1++;
                    i4_loop = 1;
                    /*duplicate pics added to the list*/
                    while(i4_loop != ps_curr_inp->s_lap_out.as_ref_pics[ref_pics]
                                         .i4_num_duplicate_entries_in_ref_list)
                    {
                        /* populate weights and offsets corresponding to this ref pic */
                        memcpy(
                            &aps_ref_list_temp[LIST_1][num_ref_pics_list1].s_weight_offset,
                            &ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].as_wght_off[i4_loop],
                            sizeof(ihevce_wght_offst_t));

                        /* Store the used as ref for current pic flag  */
                        aps_ref_list_temp[LIST_1][num_ref_pics_list1].i4_used_by_cur_pic_flag =
                            ps_curr_inp->s_lap_out.as_ref_pics[ref_pics].i4_used_by_cur_pic_flag;

                        if(wp_flag)
                        {
                            WORD16 i2_luma_weight = (aps_ref_list[LIST_1][num_ref_pics_list1]
                                                         ->s_weight_offset.i2_luma_weight);

                            aps_ref_list[LIST_1][num_ref_pics_list1]->i4_inv_luma_wt =
                                ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                            aps_ref_list[LIST_1][num_ref_pics_list1]->i4_log2_wt_denom =
                                ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                        }
                        else
                        {
                            WORD16 i2_luma_weight =
                                (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                            aps_ref_list[LIST_1][num_ref_pics_list1]
                                ->s_weight_offset.i2_luma_weight = i2_luma_weight;

                            aps_ref_list[LIST_1][num_ref_pics_list1]->i4_inv_luma_wt =
                                ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                            aps_ref_list[LIST_1][num_ref_pics_list1]->i4_log2_wt_denom =
                                ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                        }

                        num_ref_pics_list1++;
                        i4_loop++;
                        ps_slice_header->s_rplm.i1_ref_pic_list_modification_flag_l1 = 1;
                        ps_slice_header->s_rplm.i1_ref_pic_list_modification_flag_l0 = 1;
                    }
                }
                break;
            }
        }

        /* if the reference picture is not found then error */
        ASSERT(ctr != i4_num_recon_bufs);
    }

    i4_num_rpics_l0_excl_dup = num_ref_pics_list0;
    i4_num_rpics_l1_excl_dup = num_ref_pics_list1;

    /* sort the reference pics in List0 in descending order POC */
    if(num_ref_pics_list0 > 1)
    {
        /* run a loop for num ref pics -1 */
        for(ctr = 0; ctr < num_ref_pics_list0 - 1; ctr++)
        {
            WORD32 max_idx = ctr;
            recon_pic_buf_t *ps_temp;
            WORD32 i;

            for(i = (ctr + 1); i < num_ref_pics_list0; i++)
            {
                /* check for poc greater than current ref poc */
                if(aps_ref_list[LIST_0][i]->i4_poc > aps_ref_list[LIST_0][max_idx]->i4_poc)
                {
                    max_idx = i;
                }
            }

            /* if max of remaining is not current, swap the pointers */
            if(max_idx != ctr)
            {
                ps_temp = aps_ref_list[LIST_0][max_idx];
                aps_ref_list[LIST_0][max_idx] = aps_ref_list[LIST_0][ctr];
                aps_ref_list[LIST_0][ctr] = ps_temp;
            }
        }
    }

    /* sort the reference pics in List1 in ascending order POC */
    if(num_ref_pics_list1 > 1)
    {
        /* run a loop for num ref pics -1 */
        for(ctr = 0; ctr < num_ref_pics_list1 - 1; ctr++)
        {
            WORD32 min_idx = ctr;
            recon_pic_buf_t *ps_temp;
            WORD32 i;

            for(i = (ctr + 1); i < num_ref_pics_list1; i++)
            {
                /* check for p[oc less than current ref poc */
                if(aps_ref_list[LIST_1][i]->i4_poc < aps_ref_list[LIST_1][min_idx]->i4_poc)
                {
                    min_idx = i;
                }
            }

            /* if min of remaining is not current, swap the pointers */
            if(min_idx != ctr)
            {
                ps_temp = aps_ref_list[LIST_1][min_idx];
                aps_ref_list[LIST_1][min_idx] = aps_ref_list[LIST_1][ctr];
                aps_ref_list[LIST_1][ctr] = ps_temp;
            }
        }
    }

    /* popluate the slice header parameters to signal delta POCs and use flags */
    {
        WORD32 i;
        WORD32 prev_poc = curr_poc;

        ps_slice_header->s_stref_picset.i1_inter_ref_pic_set_prediction_flag = 0;

        ps_slice_header->s_stref_picset.i1_num_neg_pics = num_ref_pics_list0;

        ps_slice_header->s_stref_picset.i1_num_pos_pics = num_ref_pics_list1;

        ps_slice_header->s_stref_picset.i1_num_ref_idc = -1;

        /* populate the delta POCs of reference pics */
        i = 0;

        for(ctr = 0; ctr < i4_num_rpics_l0_excl_dup; ctr++)
        {
            WORD32 ref_poc_l0 = aps_ref_list[LIST_0][i]->i4_poc;

            ps_slice_header->s_stref_picset.ai2_delta_poc[ctr] = prev_poc - ref_poc_l0;
            ps_slice_header->s_stref_picset.ai1_used[ctr] =
                aps_ref_list[LIST_0][i]->i4_used_by_cur_pic_flag;

            /* check if this picture has to be used as reference */
            if(1 == ps_slice_header->s_stref_picset.ai1_used[ctr])
            {
                /* check for CRA poc related use flag signalling */
                ps_slice_header->s_stref_picset.ai1_used[ctr] =
                    (curr_poc > cra_poc) ? (ref_poc_l0 >= cra_poc) : (slice_type != ISLICE);
            }
            if(!(prev_poc - ref_poc_l0))
            {
                ctr -= 1;
                i4_num_rpics_l0_excl_dup -= 1;
            }
            prev_poc = ref_poc_l0;

            i++;
        }

        i = 0;
        prev_poc = curr_poc;
        for(; ctr < (i4_num_rpics_l0_excl_dup + i4_num_rpics_l1_excl_dup); ctr++)
        {
            WORD32 ref_poc_l1 = aps_ref_list[LIST_1][i]->i4_poc;

            ps_slice_header->s_stref_picset.ai2_delta_poc[ctr] = ref_poc_l1 - prev_poc;

            ps_slice_header->s_stref_picset.ai1_used[ctr] =
                aps_ref_list[LIST_1][i]->i4_used_by_cur_pic_flag;

            /* check if this picture has to be used as reference */
            if(1 == ps_slice_header->s_stref_picset.ai1_used[ctr])
            {
                /* check for CRA poc related use flag signalling */
                ps_slice_header->s_stref_picset.ai1_used[ctr] =
                    (curr_poc > cra_poc) ? (ref_poc_l1 >= cra_poc) : (slice_type != ISLICE);
                /* (slice_type != ISLICE); */
            }
            if(!(ref_poc_l1 - prev_poc))
            {
                ctr -= 1;
                i4_num_rpics_l1_excl_dup -= 1;
            }
            prev_poc = ref_poc_l1;
            i++;
        }
        ps_slice_header->s_stref_picset.i1_num_neg_pics = i4_num_rpics_l0_excl_dup;

        ps_slice_header->s_stref_picset.i1_num_pos_pics = i4_num_rpics_l1_excl_dup;

        if(IV_IDR_FRAME == ps_curr_inp->s_lap_out.i4_pic_type)
        {
            ps_slice_header->s_stref_picset.i1_num_neg_pics = 0;
            ps_slice_header->s_stref_picset.i1_num_pos_pics = 0;
        }

        /* not used so set to -1 */
        memset(&ps_slice_header->s_stref_picset.ai1_ref_idc[0], -1, MAX_DPB_SIZE);
    }
    /* call the ME API to update the DPB of HME pyramids
    Upadate list for reference bit-rate only */
    if(0 == i4_bridx)
    {
        ihevce_me_frame_dpb_update(
            ps_enc_ctxt->s_module_ctxt.pv_me_ctxt,
            num_ref_pics_list0,
            num_ref_pics_list1,
            &aps_ref_list[LIST_0][0],
            &aps_ref_list[LIST_1][0],
            i4_thrd_id);
    }

    /* Default list creation based on uses as ref pic for current pic flag */
    {
        WORD32 num_ref_pics_list_final = 0;
        WORD32 list_idx = 0;

        /* LIST 0 */
        /* run a loop for num ref pics in list 0 */
        for(ctr = 0; ctr < num_ref_pics_list0; ctr++)
        {
            /* check for used as reference flag */
            if(1 == aps_ref_list[LIST_0][ctr]->i4_used_by_cur_pic_flag)
            {
                /* copy the pointer to the actual valid list idx */
                aps_ref_list[LIST_0][list_idx] = aps_ref_list[LIST_0][ctr];

                /* increment the valid pic counters and idx */
                list_idx++;
                num_ref_pics_list_final++;
            }
        }

        /* finally store the number of pictures in List0 */
        num_ref_pics_list0 = num_ref_pics_list_final;

        /* LIST 1 */
        num_ref_pics_list_final = 0;
        list_idx = 0;

        /* run a loop for num ref pics in list 1 */
        for(ctr = 0; ctr < num_ref_pics_list1; ctr++)
        {
            /* check for used as reference flag */
            if(1 == aps_ref_list[LIST_1][ctr]->i4_used_by_cur_pic_flag)
            {
                /* copy the pointer to the actual valid list idx */
                aps_ref_list[LIST_1][list_idx] = aps_ref_list[LIST_1][ctr];

                /* increment the valid pic counters and idx */
                list_idx++;
                num_ref_pics_list_final++;
            }
        }

        /* finally store the number of pictures in List1 */
        num_ref_pics_list1 = num_ref_pics_list_final;
    }
    /*in case of single active ref picture on L0 and L1, then consider one of them weighted
    and another non-weighted*/
    if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
    {
        if(num_ref_pics_list0 > 2)
        {
            if(aps_ref_list[LIST_0][0]->i4_poc == aps_ref_list[LIST_0][1]->i4_poc)
            {
                i4_inc_L0_active_ref_pic = 1;
            }
        }
    }
    else
    {
        if(num_ref_pics_list0 >= 2 && num_ref_pics_list1 >= 2)
        {
            if(aps_ref_list[LIST_0][0]->i4_poc == aps_ref_list[LIST_0][1]->i4_poc)
            {
                i4_inc_L0_active_ref_pic = 1;
            }

            if(aps_ref_list[LIST_1][0]->i4_poc == aps_ref_list[LIST_1][1]->i4_poc)
            {
                i4_inc_L1_active_ref_pic = 1;
            }
        }
    }
    /* append the reference pics in List1 and end of list0 */
    for(ctr = 0; ctr < num_ref_pics_list1; ctr++)
    {
        aps_ref_list[LIST_0][num_ref_pics_list0 + ctr] = aps_ref_list[LIST_1][ctr];
    }

    /* append the reference pics in List0 and end of list1 */
    for(ctr = 0; ctr < num_ref_pics_list0; ctr++)
    {
        aps_ref_list[LIST_1][num_ref_pics_list1 + ctr] = aps_ref_list[LIST_0][ctr];
    }

    /* reference list modification for adding duplicate reference */
    {
        WORD32 i4_latest_idx = 0;
        recon_pic_buf_t *ps_ref_list_cur;
        recon_pic_buf_t *ps_ref_list_prev;
        /*List 0*/
        ps_ref_list_cur = aps_ref_list[LIST_0][0];
        ps_ref_list_prev = ps_ref_list_cur;
        for(ctr = 0; ctr < (num_ref_pics_list0 + num_ref_pics_list1); ctr++)
        {
            if(ps_ref_list_cur->i4_poc != ps_ref_list_prev->i4_poc)
            {
                i4_latest_idx++;
            }
            ps_ref_list_prev = ps_ref_list_cur;
            ps_slice_header->s_rplm.i4_ref_poc_l0[ctr] = ps_ref_list_cur->i4_poc;
            ps_slice_header->s_rplm.i1_list_entry_l0[ctr] = i4_latest_idx;
            if((ctr + 1) < (num_ref_pics_list0 + num_ref_pics_list1))
            {
                ps_ref_list_cur = aps_ref_list[LIST_0][ctr + 1];
            }
        } /*end for*/

        /*LIST 1*/
        i4_latest_idx = 0;
        ps_ref_list_cur = aps_ref_list[LIST_1][0];
        ps_ref_list_prev = ps_ref_list_cur;
        for(ctr = 0; ctr < (num_ref_pics_list0 + num_ref_pics_list1); ctr++)
        {
            if(ps_ref_list_cur->i4_poc != ps_ref_list_prev->i4_poc)
            {
                i4_latest_idx++;
            }
            ps_ref_list_prev = ps_ref_list_cur;
            ps_slice_header->s_rplm.i4_ref_poc_l1[ctr] = ps_ref_list_cur->i4_poc;
            ps_slice_header->s_rplm.i1_list_entry_l1[ctr] = i4_latest_idx;
            if((ctr + 1) < (num_ref_pics_list0 + num_ref_pics_list1))
            {
                ps_ref_list_cur = aps_ref_list[LIST_1][ctr + 1];
            }
        } /*end for*/
    }

    /* set number of active references used for l0 and l1 in slice hdr */
    ps_slice_header->i1_num_ref_idx_active_override_flag = 1;
    ps_slice_header->i1_num_ref_idx_l0_active = num_ref_pics_list0 + num_ref_pics_list1;
    if(BSLICE == slice_type)
    {
        /* i1_num_ref_idx_l1_active applicable only for B pics */
        ps_slice_header->i1_num_ref_idx_l1_active = num_ref_pics_list0 + num_ref_pics_list1;
    }
    /* popluate the slice header parameters with weights and offsets */
    {
        WORD32 i;

        /* populate the log 2 weight denom if weighted prediction is enabled */
        if(1 == wp_flag)
        {
            ps_slice_header->s_wt_ofst.i1_chroma_log2_weight_denom =
                ps_curr_inp->s_lap_out.i4_log2_chroma_wght_denom;
            ps_slice_header->s_wt_ofst.i1_luma_log2_weight_denom =
                ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
        }

        /* populate the weights and offsets for all pics in L0 + L1 */
        for(i = 0; i < (num_ref_pics_list0 + num_ref_pics_list1); i++)
        {
            /* populate the weights and offsets if weighted prediction is enabled */
            if(1 == wp_flag)
            {
                ps_slice_header->s_wt_ofst.i1_luma_weight_l0_flag[i] =
                    aps_ref_list[LIST_0][i]->s_weight_offset.u1_luma_weight_enable_flag;

                /* if weights are enabled then copy to slice header */
                if(1 == ps_slice_header->s_wt_ofst.i1_luma_weight_l0_flag[i])
                {
                    ps_slice_header->s_wt_ofst.i2_luma_weight_l0[i] =
                        aps_ref_list[LIST_0][i]->s_weight_offset.i2_luma_weight;
                    ps_slice_header->s_wt_ofst.i2_luma_offset_l0[i] =
                        aps_ref_list[LIST_0][i]->s_weight_offset.i2_luma_offset;

                    {
                        WORD16 i2_luma_weight =
                            (aps_ref_list[LIST_0][i]->s_weight_offset.i2_luma_weight);

                        aps_ref_list[LIST_0][i]->i4_inv_luma_wt =
                            ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                        aps_ref_list[LIST_0][i]->i4_log2_wt_denom =
                            ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                    }
                }
                else
                {
                    WORD16 i2_luma_weight = (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                    /* set to default values */
                    aps_ref_list[LIST_0][i]->s_weight_offset.i2_luma_weight = (i2_luma_weight);

                    aps_ref_list[LIST_0][i]->s_weight_offset.i2_luma_offset = 0;

                    aps_ref_list[LIST_0][i]->i4_inv_luma_wt =
                        ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                    aps_ref_list[LIST_0][i]->i4_log2_wt_denom =
                        ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                }

                ps_slice_header->s_wt_ofst.i1_chroma_weight_l0_flag[i] =
                    aps_ref_list[LIST_0][i]->s_weight_offset.u1_chroma_weight_enable_flag;

                /* if weights are enabled then copy to slice header */
                if(1 == ps_slice_header->s_wt_ofst.i1_chroma_weight_l0_flag[i])
                {
                    ps_slice_header->s_wt_ofst.i2_chroma_weight_l0_cb[i] =
                        aps_ref_list[LIST_0][i]->s_weight_offset.i2_cb_weight;
                    ps_slice_header->s_wt_ofst.i2_chroma_offset_l0_cb[i] =
                        aps_ref_list[LIST_0][i]->s_weight_offset.i2_cb_offset;

                    ps_slice_header->s_wt_ofst.i2_chroma_weight_l0_cr[i] =
                        aps_ref_list[LIST_0][i]->s_weight_offset.i2_cr_weight;
                    ps_slice_header->s_wt_ofst.i2_chroma_offset_l0_cr[i] =
                        aps_ref_list[LIST_0][i]->s_weight_offset.i2_cr_offset;
                }
                else
                {
                    /* set to default values */
                    aps_ref_list[LIST_0][i]->s_weight_offset.i2_cb_weight =
                        (1 << ps_curr_inp->s_lap_out.i4_log2_chroma_wght_denom);
                    aps_ref_list[LIST_0][i]->s_weight_offset.i2_cr_weight =
                        (1 << ps_curr_inp->s_lap_out.i4_log2_chroma_wght_denom);
                    aps_ref_list[LIST_0][i]->s_weight_offset.i2_cb_offset = 0;
                    aps_ref_list[LIST_0][i]->s_weight_offset.i2_cr_offset = 0;
                }
            }
        }

        for(i = 0; i < (num_ref_pics_list0 + num_ref_pics_list1); i++)
        {
            /* populate the weights and offsets if weighted prediction is enabled */
            if(1 == wp_flag)
            {
                ps_slice_header->s_wt_ofst.i1_luma_weight_l1_flag[i] =
                    aps_ref_list[LIST_1][i]->s_weight_offset.u1_luma_weight_enable_flag;

                /* if weights are enabled then copy to slice header */
                if(1 == ps_slice_header->s_wt_ofst.i1_luma_weight_l1_flag[i])
                {
                    ps_slice_header->s_wt_ofst.i2_luma_weight_l1[i] =
                        aps_ref_list[LIST_1][i]->s_weight_offset.i2_luma_weight;
                    ps_slice_header->s_wt_ofst.i2_luma_offset_l1[i] =
                        aps_ref_list[LIST_1][i]->s_weight_offset.i2_luma_offset;

                    {
                        WORD16 i2_luma_weight =
                            (aps_ref_list[LIST_1][i]->s_weight_offset.i2_luma_weight);

                        aps_ref_list[LIST_1][i]->i4_inv_luma_wt =
                            ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                        aps_ref_list[LIST_1][i]->i4_log2_wt_denom =
                            ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                    }
                }
                else
                {
                    WORD16 i2_luma_weight = (1 << ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom);

                    /* set to default values */
                    aps_ref_list[LIST_1][i]->s_weight_offset.i2_luma_weight = (i2_luma_weight);

                    aps_ref_list[LIST_1][i]->s_weight_offset.i2_luma_offset = 0;

                    aps_ref_list[LIST_1][i]->i4_inv_luma_wt =
                        ((1 << 15) + (i2_luma_weight >> 1)) / i2_luma_weight;

                    aps_ref_list[LIST_1][i]->i4_log2_wt_denom =
                        ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom;
                }

                ps_slice_header->s_wt_ofst.i1_chroma_weight_l1_flag[i] =
                    aps_ref_list[LIST_1][i]->s_weight_offset.u1_chroma_weight_enable_flag;

                /* if weights are enabled then copy to slice header */
                if(1 == ps_slice_header->s_wt_ofst.i1_chroma_weight_l1_flag[i])
                {
                    ps_slice_header->s_wt_ofst.i2_chroma_weight_l1_cb[i] =
                        aps_ref_list[LIST_1][i]->s_weight_offset.i2_cb_weight;
                    ps_slice_header->s_wt_ofst.i2_chroma_offset_l1_cb[i] =
                        aps_ref_list[LIST_1][i]->s_weight_offset.i2_cb_offset;

                    ps_slice_header->s_wt_ofst.i2_chroma_weight_l1_cr[i] =
                        aps_ref_list[LIST_1][i]->s_weight_offset.i2_cr_weight;
                    ps_slice_header->s_wt_ofst.i2_chroma_offset_l1_cr[i] =
                        aps_ref_list[LIST_1][i]->s_weight_offset.i2_cr_offset;
                }
                else
                {
                    /* set to default values */
                    aps_ref_list[LIST_1][i]->s_weight_offset.i2_cb_weight =
                        (1 << ps_curr_inp->s_lap_out.i4_log2_chroma_wght_denom);
                    aps_ref_list[LIST_1][i]->s_weight_offset.i2_cr_weight =
                        (1 << ps_curr_inp->s_lap_out.i4_log2_chroma_wght_denom);
                    aps_ref_list[LIST_1][i]->s_weight_offset.i2_cb_offset = 0;
                    aps_ref_list[LIST_1][i]->s_weight_offset.i2_cr_offset = 0;
                }
            }
        }
    }

    /* store the number of reference pics in the list for ME/MC etc */
    ps_enc_ctxt->i4_num_ref_l0 = num_ref_pics_list0;
    ps_enc_ctxt->i4_num_ref_l1 = num_ref_pics_list1;

#define HME_USE_ONLY_2REF
#ifndef HME_USE_ONLY_2REF
    ps_enc_ctxt->i4_num_ref_l0_active = num_ref_pics_list0;
    ps_enc_ctxt->i4_num_ref_l1_active = num_ref_pics_list1;
#else
#if MULTI_REF_ENABLE == 1
    if(ps_curr_inp->s_lap_out.i4_quality_preset >= IHEVCE_QUALITY_P3)
    {
        if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
        {
            if(ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6)
            {
                if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                {
                    ps_enc_ctxt->i4_num_ref_l0_active =
                        MIN(MAX_NUM_REFS_IN_PPICS_IN_XS25 + 1, num_ref_pics_list0);
                }
                else
                {
                    ps_enc_ctxt->i4_num_ref_l0_active =
                        MIN(MAX_NUM_REFS_IN_PPICS_IN_XS25, num_ref_pics_list0);

                    ps_enc_ctxt->i4_num_ref_l0_active += i4_inc_L0_active_ref_pic;
                }
            }
            else
            {
                if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                {
                    ps_enc_ctxt->i4_num_ref_l0_active = MIN(3, num_ref_pics_list0);
                }
                else
                {
                    ps_enc_ctxt->i4_num_ref_l0_active = MIN(2, num_ref_pics_list0);
                    ps_enc_ctxt->i4_num_ref_l0_active += i4_inc_L0_active_ref_pic;
                }
            }

            ps_enc_ctxt->i4_num_ref_l1_active = 0;
        }
        else
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
            {
                ps_enc_ctxt->i4_num_ref_l0_active = MIN(2, num_ref_pics_list0);
                ps_enc_ctxt->i4_num_ref_l1_active = MIN(1, num_ref_pics_list1);
                ps_enc_ctxt->i4_num_ref_l1_active += i4_inc_L1_active_ref_pic;
            }
            else
            {
                ps_enc_ctxt->i4_num_ref_l0_active = MIN(1, num_ref_pics_list0);
                ps_enc_ctxt->i4_num_ref_l1_active = MIN(1, num_ref_pics_list1);

                ps_enc_ctxt->i4_num_ref_l1_active += i4_inc_L1_active_ref_pic;
                ps_enc_ctxt->i4_num_ref_l0_active += i4_inc_L0_active_ref_pic;
            }
        }
    }
    else
    {
        if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
                ps_enc_ctxt->i4_num_ref_l0_active = MIN(4, num_ref_pics_list0);
            else
                ps_enc_ctxt->i4_num_ref_l0_active = MIN(4, num_ref_pics_list0);

            ps_enc_ctxt->i4_num_ref_l1_active = 0;
        }
        else
        {
            if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
            {
                ps_enc_ctxt->i4_num_ref_l0_active = MIN(4, num_ref_pics_list0);
                ps_enc_ctxt->i4_num_ref_l1_active = MIN(4, num_ref_pics_list1);
            }
            else
            {
                ps_enc_ctxt->i4_num_ref_l0_active = MIN(4, num_ref_pics_list0);
                ps_enc_ctxt->i4_num_ref_l1_active = MIN(4, num_ref_pics_list1);
            }
        }
    }
#else
    if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
    {
        if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
            ps_enc_ctxt->i4_num_ref_l0_active = MIN(3, num_ref_pics_list0);
        else
            ps_enc_ctxt->i4_num_ref_l0_active = MIN(2, num_ref_pics_list0);

        ps_enc_ctxt->i4_num_ref_l1_active = 0;
    }
    else
    {
        if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
        {
            ps_enc_ctxt->i4_num_ref_l0_active = MIN(2, num_ref_pics_list0);
            ps_enc_ctxt->i4_num_ref_l1_active = MIN(1, num_ref_pics_list1);
        }
        else
        {
            ps_enc_ctxt->i4_num_ref_l0_active = MIN(1, num_ref_pics_list0);
            ps_enc_ctxt->i4_num_ref_l1_active = MIN(1, num_ref_pics_list1);
        }
    }
#endif

#endif

    ps_slice_header->i1_num_ref_idx_l0_active = MAX(1, ps_enc_ctxt->i4_num_ref_l0_active);
    if(BSLICE == slice_type)
    {
        /* i1_num_ref_idx_l1_active applicable only for B pics */
        ps_slice_header->i1_num_ref_idx_l1_active = MAX(1, ps_enc_ctxt->i4_num_ref_l1_active);
    }
    if(1 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
    {
        /* If Interlace field is enabled,  p field following an cra I field should have only one ref frame */
        WORD32 cra_second_poc = cra_poc + 1;

        if(curr_poc == cra_second_poc)
        {
            /*   set number of active references used for l0 and l1 for me  */
            ps_enc_ctxt->i4_num_ref_l0_active = 1;
            ps_enc_ctxt->i4_num_ref_l1_active = 0;

            /*   set number of active references used for l0 and l1 in slice hdr */
            ps_slice_header->i1_num_ref_idx_active_override_flag = 1;
            ps_slice_header->i1_num_ref_idx_l0_active =
                ps_enc_ctxt->i4_num_ref_l0 + ps_enc_ctxt->i4_num_ref_l1;
        }
    }
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_frame_lambda_prms \endif
*
* \brief
*    Function whihc calculates the Lambda params for current picture
*
* \param[in] ps_enc_ctxt : encoder ctxt pointer
* \param[in] ps_cur_pic_ctxt : current pic ctxt
* \param[in] i4_cur_frame_qp : current pic QP
* \param[in] first_field : is first field flag
* \param[in] i4_temporal_lyr_id : Current picture layer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_get_frame_lambda_prms(
    enc_ctxt_t *ps_enc_ctxt,
    pre_enc_me_ctxt_t *ps_cur_pic_ctxt,
    WORD32 i4_cur_frame_qp,
    WORD32 first_field,
    WORD32 i4_is_ref_pic,
    WORD32 i4_temporal_lyr_id,
    double f_i_pic_lamda_modifier,
    WORD32 i4_inst_id,
    WORD32 i4_lambda_type)
{
    double lambda_modifier = CONST_LAMDA_MOD_VAL;
    double lambda_uv_modifier = CONST_LAMDA_MOD_VAL;
    double lambda = 0;
    double lambda_uv;
    WORD32 i4_use_const_lamda_modifier;

    /* initialize lambda based on frm qp, slice type, num b and temporal id */
    /* This lamba calculation mimics the jctvc doc (TODO add doc number     */

    WORD32 num_b_frms =
        (1 << ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_temporal_layers) - 1;
    WORD32 chroma_qp = (ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV)
                           ? MIN(i4_cur_frame_qp, 51)
                           : gai1_ihevc_chroma_qp_scale[i4_cur_frame_qp + MAX_QP_BD_OFFSET];

    WORD32 i4_qp_bdoffset =
        6 * (ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_internal_bit_depth - 8);
    WORD32 slice_type = ps_cur_pic_ctxt->s_slice_hdr.i1_slice_type;

    (void)first_field;
    (void)i4_is_ref_pic;
    (void)i4_temporal_lyr_id;
    i4_use_const_lamda_modifier = USE_CONSTANT_LAMBDA_MODIFIER;
    i4_use_const_lamda_modifier = i4_use_const_lamda_modifier ||
                                  ((ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
                                    (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER)) &&
                                   ((ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
                                     (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)) ||
                                    (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
                                     (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_1)) ||
                                    (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
                                     (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_2)) ||
                                    (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
                                     (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_3))));

    /* lambda modifier is the dependent on slice type and temporal id  */
    if(ISLICE == slice_type)
    {
        double temporal_correction_islice = 1.0 - 0.05 * num_b_frms;
        temporal_correction_islice = MAX(0.5, temporal_correction_islice);

        lambda_modifier = 0.57 * temporal_correction_islice;
        lambda_uv_modifier = lambda_modifier;
        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_modifier = f_i_pic_lamda_modifier;
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_uv_modifier = f_i_pic_lamda_modifier;
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_modifier = lambda_modifier;
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_uv_modifier = lambda_uv_modifier;
        }
    }
    else if(PSLICE == slice_type)
    {
        if(first_field)
            lambda_modifier = 0.442;  //0.442*0.8;
        else
            lambda_modifier = 0.442;
        lambda_uv_modifier = lambda_modifier;
        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_modifier = CONST_LAMDA_MOD_VAL;
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_uv_modifier = CONST_LAMDA_MOD_VAL;
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_modifier = lambda_modifier;
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_uv_modifier = lambda_uv_modifier;
        }
    }
    else
    {
        /* BSLICE */
        if(1 == i4_is_ref_pic)
        {
            lambda_modifier = 0.3536;
        }
        else if(2 == i4_is_ref_pic)
        {
            lambda_modifier = 0.45;
        }
        else
        {
            lambda_modifier = 0.68;
        }
        lambda_uv_modifier = lambda_modifier;
        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_modifier = CONST_LAMDA_MOD_VAL;
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_uv_modifier = CONST_LAMDA_MOD_VAL;
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_modifier = lambda_modifier;
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].lambda_uv_modifier = lambda_uv_modifier;
        }
        /* TODO: Disable lambda modification for interlace encode to match HM runs */
        //if(0 == ps_enc_ctxt->s_runtime_src_prms.i4_field_pic)
        {
            /* modify b lambda further based on temporal id */
            if(i4_temporal_lyr_id)
            {
                lambda_modifier *= CLIP3((((double)(i4_cur_frame_qp - 12)) / 6.0), 2.00, 4.00);
                lambda_uv_modifier *= CLIP3((((double)(chroma_qp - 12)) / 6.0), 2.00, 4.00);
            }
        }
    }
    if(i4_use_const_lamda_modifier)
    {
        if(ISLICE == slice_type)
        {
            lambda_modifier = f_i_pic_lamda_modifier;
            lambda_uv_modifier = f_i_pic_lamda_modifier;
        }
        else
        {
            lambda_modifier = CONST_LAMDA_MOD_VAL;
            lambda_uv_modifier = CONST_LAMDA_MOD_VAL;
        }
    }

    switch(i4_lambda_type)
    {
    case 0:
    {
        i4_qp_bdoffset = 0;

        lambda = pow(2.0, (((double)(i4_cur_frame_qp + i4_qp_bdoffset - 12)) / 3.0));
        lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

        /* modify the base lambda according to lambda modifier */
        lambda *= lambda_modifier;
        lambda_uv *= lambda_uv_modifier;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].u4_chroma_cost_weighing_factor =
            (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_qf =
            (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_chroma_qf =
            (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_lambda_qf =
            (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf =
                (WORD32)((sqrt(lambda) / 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
                (WORD32)(sqrt(lambda * 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
                (WORD32)((sqrt(lambda * 1.5)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_type2_lambda_chroma_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_chroma_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf;

        break;
    }
    case 1:
    {
        lambda = pow(2.0, (((double)(i4_cur_frame_qp + i4_qp_bdoffset - 12)) / 3.0));
        lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

        /* modify the base lambda according to lambda modifier */
        lambda *= lambda_modifier;
        lambda_uv *= lambda_uv_modifier;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].u4_chroma_cost_weighing_factor =
            (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_qf =
            (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_chroma_qf =
            (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_lambda_qf =
            (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf =
                (WORD32)((sqrt(lambda) / 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
                (WORD32)(sqrt(lambda * 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
                (WORD32)((sqrt(lambda * 1.5)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_type2_lambda_chroma_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_chroma_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_type2_lambda_qf =
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf;

        break;
    }
    case 2:
    {
        lambda = pow(2.0, (((double)(i4_cur_frame_qp + i4_qp_bdoffset - 12)) / 3.0));
        lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

        /* modify the base lambda according to lambda modifier */
        lambda *= lambda_modifier;
        lambda_uv *= lambda_uv_modifier;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].u4_chroma_cost_weighing_factor =
            (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_qf =
            (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_lambda_chroma_qf =
            (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_lambda_qf =
            (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_lambda_qf =
                (WORD32)((sqrt(lambda) / 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
                (WORD32)(sqrt(lambda * 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
                (WORD32)((sqrt(lambda * 1.5)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        /* lambda corresponding to 8- bit, for metrics based on 8- bit ( Example 8bit SAD in encloop)*/

        lambda = pow(2.0, (((double)(i4_cur_frame_qp - 12)) / 3.0));
        lambda_uv = pow(2.0, (((double)(chroma_qp - 12)) / 3.0));

        /* modify the base lambda according to lambda modifier */
        lambda *= lambda_modifier;
        lambda_uv *= lambda_uv_modifier;

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].u4_chroma_cost_weighing_factor =
            (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_type2_lambda_qf =
            (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i8_cl_ssd_type2_lambda_chroma_qf =
            (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_sad_type2_lambda_qf =
            (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
        if(i4_use_const_lamda_modifier)
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_type2_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_type2_lambda_qf =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_type2_lambda_qf =
                (WORD32)((sqrt(lambda)) * (1 << (LAMBDA_Q_SHIFT)));
        }
        else
        {
            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_sad_type2_lambda_qf =
                (WORD32)((sqrt(lambda) / 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_type2_lambda_qf =
                (WORD32)(sqrt(lambda * 1.5) * (1 << LAMBDA_Q_SHIFT));

            ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_type2_lambda_qf =
                (WORD32)((sqrt(lambda * 1.5)) * (1 << (LAMBDA_Q_SHIFT)));
        }

        break;
    }
    default:
    {
        /* Intended to be a barren wasteland! */
        ASSERT(0);
    }
    }

    /* Assign the final lambdas after up shifting to its q format */

    /* closed loop ssd lambda is same as final lambda */

    /* --- Initialized the lambda for SATD computations --- */
    if(i4_use_const_lamda_modifier)
    {
        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
            (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
            (WORD32)((sqrt(lambda)) * (1 << (LAMBDA_Q_SHIFT)));
    }
    else
    {
        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_cl_satd_lambda_qf =
            (WORD32)(sqrt(lambda * 1.5) * (1 << LAMBDA_Q_SHIFT));

        ps_cur_pic_ctxt->as_lambda_prms[i4_inst_id].i4_ol_satd_lambda_qf =
            (WORD32)((sqrt(lambda * 1.5)) * (1 << (LAMBDA_Q_SHIFT)));
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_update_qp_L1_sad_based \endif
*
* \brief
*    Function which recalculates qp in case of scene cut based on L1 satd/act
*
* \param[in] ps_enc_ctxt : encoder ctxt pointer
* \param[in] ps_cur_pic_ctxt : current pic ctxt
* \param[in] i4_cur_frame_qp : current pic QP
* \param[in] first_field : is first field flag
* \param[in] i4_temporal_lyr_id : Current picture layer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_update_qp_L1_sad_based(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    ihevce_lap_enc_buf_t *ps_prev_inp,
    pre_enc_me_ctxt_t *ps_curr_out,
    WORD32 i4_is_last_thread)
{
    WORD32 i4_l1_ht, i4_l1_wd;
    ihevce_ed_blk_t *ps_ed_4x4 = ps_curr_out->ps_layer1_buf;
    WORD32 best_satd_16x16;
    //LWORD64 acc_satd = 0;
    LWORD64 acc_sad = 0; /*SAD accumulated to compare with coarse me sad*/
    WORD32 i4_tot_4x4block_l1_x, i4_tot_4x4block_l1_y;
    WORD32 i4_tot_ctb_l1_x, i4_tot_ctb_l1_y;
    WORD32 i;
    WORD32 i4_act_factor;
    UWORD8 u1_cu_possible_qp;
    WORD32 i4_q_scale_mod;
    LWORD64 i8_best_satd_16x16;
    LWORD64 i8_frame_satd_by_act_L1_accum;
    LWORD64 i8_frame_acc_sadt_L1, i8_frame_acc_sadt_L1_squared;
    WORD32 i4_new_frame_qp = 0, i4_qp_for_I_pic = 0;
    LWORD64 pre_intra_satd_act_evaluated = 0;
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1 = ps_curr_out->ps_ed_ctb_l1;
    WORD32 i4_j;
    double scale_factor_cmplx_change_detection;
    WORD32 i4_cmplx_change_detection_thrsh;
    long double ld_frame_avg_satd_L1;

    if(i4_is_last_thread)
    {
        ihevce_decomp_pre_intra_master_ctxt_t *ps_master_ctxt =
            (ihevce_decomp_pre_intra_master_ctxt_t *)
                ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt;
        ihevce_decomp_pre_intra_ctxt_t *ps_ctxt = ps_master_ctxt->aps_decomp_pre_intra_thrd_ctxt[0];

        i4_l1_wd = ps_ctxt->as_layers[1].i4_actual_wd;
        i4_l1_ht = ps_ctxt->as_layers[1].i4_actual_ht;

        if((ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6) &&
           (ps_curr_inp->s_lap_out.i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
        {
            i8_frame_acc_sadt_L1 = -1;
        }
        else
        {
            /*the accumulation of intra satd and calculation of new qp happens for all thread
    It must be made sure every thread returns same value of intra satd and qp*/
            i8_frame_acc_sadt_L1 = ihevce_decomp_pre_intra_get_frame_satd(
                ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt, &i4_l1_wd, &i4_l1_ht);
        }

#if USE_SQRT_AVG_OF_SATD_SQR
        if((ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6) &&
           (ps_curr_inp->s_lap_out.i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
        {
            i8_frame_acc_sadt_L1_squared = 0x7fffffff;
        }
        else
        {
            i8_frame_acc_sadt_L1_squared = ihevce_decomp_pre_intra_get_frame_satd_squared(
                ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt, &i4_l1_wd, &i4_l1_ht);
        }
#else
        i8_frame_acc_sadt_L1_squared = i8_frame_acc_sadt_L1;
#endif
        if((i4_l1_wd * i4_l1_ht) > (245760 /*640 * 384*/))
        {
            scale_factor_cmplx_change_detection =
                (double)0.12 * ((i4_l1_wd * i4_l1_ht) / (640.0 * 384.0));
            i4_cmplx_change_detection_thrsh =
                (WORD32)(HME_HIGH_SAD_BLK_THRESH * (1 - scale_factor_cmplx_change_detection));
        }
        else
        {
            scale_factor_cmplx_change_detection =
                (double)0.12 * ((640.0 * 384.0) / (i4_l1_wd * i4_l1_ht));
            i4_cmplx_change_detection_thrsh =
                (WORD32)(HME_HIGH_SAD_BLK_THRESH * (1 + scale_factor_cmplx_change_detection));
        }
        i4_tot_4x4block_l1_x =
            ((i4_l1_wd + ((MAX_CTB_SIZE >> 1) - 1)) & 0xFFFFFFE0) /
            4;  //((i4_l1_wd + 31) & 0xFFFFFFE0)/4;//(i4_l1_wd + (i4_l1_wd % 32 )) / 4;
        i4_tot_4x4block_l1_y =
            ((i4_l1_ht + ((MAX_CTB_SIZE >> 1) - 1)) & 0xFFFFFFE0) /
            4;  //((i4_l1_ht + 31) & 0xFFFFFFE0)/4;//(i4_l1_ht + (i4_l1_ht % 32 )) / 4;
        ld_frame_avg_satd_L1 =
            (WORD32)log(
                1 + (long double)i8_frame_acc_sadt_L1_squared /
                        ((long double)((i4_tot_4x4block_l1_x * i4_tot_4x4block_l1_y) >> 2))) /
            log(2.0);
        /* L1 satd accumalated for computing qp */
        i8_frame_satd_by_act_L1_accum = 0;
        i4_tot_ctb_l1_x =
            ((i4_l1_wd + ((MAX_CTB_SIZE >> 1) - 1)) & 0xFFFFFFE0) / (MAX_CTB_SIZE >> 1);
        i4_tot_ctb_l1_y =
            ((i4_l1_ht + ((MAX_CTB_SIZE >> 1) - 1)) & 0xFFFFFFE0) / (MAX_CTB_SIZE >> 1);

        for(i = 0; i < (i4_tot_ctb_l1_x * i4_tot_ctb_l1_y); i += 1)
        {
            for(i4_j = 0; i4_j < 16; i4_j++)
            {
                if(ps_ed_ctb_l1->i4_best_satd_8x8[i4_j] != -1)
                {
                    ASSERT(ps_ed_ctb_l1->i4_best_satd_8x8[i4_j] >= 0);
                    ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_j] >= 0);

                    if((ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6) &&
                       (ps_curr_inp->s_lap_out.i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
                    {
                        best_satd_16x16 = 0;
                    }
                    else
                    {
                        best_satd_16x16 = ps_ed_ctb_l1->i4_best_satd_8x8[i4_j];
                    }

                    acc_sad += (WORD32)ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_j];
                    //acc_satd += (WORD32)best_satd_16x16;
                    u1_cu_possible_qp = ihevce_cu_level_qp_mod(
                        32,
                        best_satd_16x16,
                        ld_frame_avg_satd_L1,
                        REF_MOD_STRENGTH,  // To be changed later
                        &i4_act_factor,
                        &i4_q_scale_mod,
                        &ps_enc_ctxt->s_rc_quant);
                    i8_best_satd_16x16 = best_satd_16x16 << QP_LEVEL_MOD_ACT_FACTOR;

                    if((ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6) &&
                       (ps_curr_inp->s_lap_out.i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
                    {
                        i4_act_factor = (1 << QP_LEVEL_MOD_ACT_FACTOR);
                    }

                    if(0 != i4_act_factor)
                    {
                        i8_frame_satd_by_act_L1_accum +=
                            ((WORD32)(i8_best_satd_16x16 / i4_act_factor));
                        /*Accumulate SAD for those regions which will undergo evaluation in L0 stage*/
                        if(ps_ed_4x4->intra_or_inter != 2)
                            pre_intra_satd_act_evaluated +=
                                ((WORD32)(i8_best_satd_16x16 / i4_act_factor));
                    }
                }
                ps_ed_4x4 += 4;
            }
            ps_ed_ctb_l1 += 1;
        }
        /** store the L1 satd in context struct
    Note: this variable is common across all thread. it must be made sure all threads write same value*/
        if((ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6) &&
           (ps_curr_inp->s_lap_out.i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
        {
            i8_frame_satd_by_act_L1_accum = ps_prev_inp->s_rc_lap_out.i8_frame_satd_by_act_L1_accum;
            ps_curr_inp->s_rc_lap_out.i8_frame_satd_by_act_L1_accum = i8_frame_satd_by_act_L1_accum;
            ps_curr_inp->s_rc_lap_out.i8_satd_by_act_L1_accum_evaluated = -1;
        }
        else
        {
            ps_curr_inp->s_rc_lap_out.i8_frame_satd_by_act_L1_accum = i8_frame_satd_by_act_L1_accum;
            ps_curr_inp->s_rc_lap_out.i8_satd_by_act_L1_accum_evaluated =
                pre_intra_satd_act_evaluated;
        }

        ps_curr_inp->s_rc_lap_out.i8_pre_intra_satd = i8_frame_acc_sadt_L1;
        /*accumulate raw intra sad without subtracting non coded sad*/
        ps_curr_inp->s_rc_lap_out.i8_raw_pre_intra_sad = acc_sad;
    }
    /*update pre-enc qp using data from L1 to use better qp in L0 in case of cbr mode*/
    if(i4_is_last_thread)
    {
        /* acquire mutex lock for rate control calls */
        osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
        {
            LWORD64 i8_est_L0_satd_by_act;
            WORD32 i4_cur_q_scale;
            if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != CONST_QP)
            {
                /*RCTODO :This needs to be reviewed in the context of 10/12 bit encoding as the Qp seems to be sub-optimal*/
                if(ps_enc_ctxt->ps_stat_prms->s_pass_prms.i4_pass != 2)
                    i4_cur_q_scale =
                        ps_enc_ctxt->s_rc_quant.pi4_qp_to_qscale
                            [ps_curr_out->i4_curr_frm_qp];  // + ps_enc_ctxt->s_rc_quant.i1_qp_offset];
                else
                    i4_cur_q_scale = ps_enc_ctxt->s_rc_quant
                                         .pi4_qp_to_qscale[MAX(ps_curr_out->i4_curr_frm_qp, 0)];
            }
            else
                i4_cur_q_scale =
                    ps_enc_ctxt->s_rc_quant.pi4_qp_to_qscale
                        [ps_curr_out->i4_curr_frm_qp + ps_enc_ctxt->s_rc_quant.i1_qp_offset];

            i4_cur_q_scale = (i4_cur_q_scale + (1 << (QSCALE_Q_FAC_3 - 1))) >> QSCALE_Q_FAC_3;

            i8_est_L0_satd_by_act = ihevce_get_L0_satd_based_on_L1(
                i8_frame_satd_by_act_L1_accum,
                ps_curr_inp->s_rc_lap_out.i4_num_pels_in_frame_considered,
                i4_cur_q_scale);
            /*HEVC_RC query rate control for qp*/
            if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
            {
                i4_new_frame_qp = ihevce_get_L0_est_satd_based_scd_qp(
                    ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                    &ps_curr_inp->s_rc_lap_out,
                    i8_est_L0_satd_by_act,
                    8.00);
            }
            else
                i4_new_frame_qp = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms
                                      .as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                                      .ai4_frame_qp[0];
            i4_new_frame_qp = CLIP3(i4_new_frame_qp, 1, 51);
            i4_qp_for_I_pic = CLIP3(i4_qp_for_I_pic, 1, 51);
            ps_curr_inp->s_rc_lap_out.i4_L1_qp = i4_new_frame_qp;
            /*I frame qp = qp-3 due to effect of lambda modifier*/
            i4_qp_for_I_pic = i4_new_frame_qp - 3;

            /*use new qp get possible qp even for inter pictures assuming default offset*/
            if(ps_curr_inp->s_lap_out.i4_pic_type != IV_IDR_FRAME &&
               ps_curr_inp->s_lap_out.i4_pic_type != IV_I_FRAME)
            {
                i4_new_frame_qp += ps_curr_inp->s_lap_out.i4_temporal_lyr_id + 1;
            }

            /*accumulate the L1 ME sad using skip sad value based on qp*/
            /*accumulate this only for last thread as it ll be guranteed that L1 ME sad is completely populated*/
            /*The lambda modifier in encoder is tuned in such a way that the qp offsets according to lambda modifer are as follows
                Note: These qp offset only account for lambda modifier, Hence this should be applied over qp offset that is already there due to picture type
                relative lambda scale(these lambda diff are mapped into qp difference which is applied over and obove the qp offset)
                Qi =  Iqp                         1
                Qp =  Iqp                         1
                Qb =  Iqp + 1.55                  1.48
                Qb1 = Iqp + 3.1                   2.05
                Qb2 = Iqp + 3.1                   2.05*/

            /*ihevce_compute_offsets_from_rc(ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],ai4_offsets,&ps_curr_inp->s_lap_out);*/

            if(ps_curr_inp->s_lap_out.i4_pic_type == IV_I_FRAME ||
               ps_curr_inp->s_lap_out.i4_pic_type == IV_IDR_FRAME)
            {
                i4_new_frame_qp = i4_new_frame_qp - 3;
            }
            else if(ps_curr_inp->s_lap_out.i4_pic_type == IV_P_FRAME)
            {
                i4_new_frame_qp = i4_new_frame_qp - 2;
            }
            if(ps_curr_inp->s_lap_out.i4_pic_type == IV_B_FRAME &&
               ps_curr_inp->s_lap_out.i4_temporal_lyr_id == 1)
            {
                i4_new_frame_qp = i4_new_frame_qp + 2;
            }
            else if(
                ps_curr_inp->s_lap_out.i4_pic_type == IV_B_FRAME &&
                ps_curr_inp->s_lap_out.i4_temporal_lyr_id == 2)
            {
                i4_new_frame_qp = i4_new_frame_qp + 6;
            }
            else if(
                ps_curr_inp->s_lap_out.i4_pic_type == IV_B_FRAME &&
                ps_curr_inp->s_lap_out.i4_temporal_lyr_id == 3)
            {
                i4_new_frame_qp = i4_new_frame_qp + 7;
            }

            i4_new_frame_qp = CLIP3(i4_new_frame_qp, 1, 51);
            i4_qp_for_I_pic = CLIP3(i4_qp_for_I_pic, 1, 51);

            {
                calc_l1_level_hme_intra_sad_different_qp(
                    ps_enc_ctxt, ps_curr_out, ps_curr_inp, i4_tot_ctb_l1_x, i4_tot_ctb_l1_y);

                /** frame accumulated SAD over entire frame after accounting for dead zone SAD, this is least of intra or inter*/
                /*ihevce_accum_hme_sad_subgop_rc(ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],&ps_curr_inp->s_lap_out);    */
                ihevce_rc_register_L1_analysis_data(
                    ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                    &ps_curr_inp->s_rc_lap_out,
                    i8_est_L0_satd_by_act,
                    ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad
                        [i4_new_frame_qp],  //since the sad passed will be used to calc complexity it should be non coded sad subtracted sad
                    ps_curr_inp->s_rc_lap_out.ai8_frame_acc_coarse_me_sad[i4_new_frame_qp]);

                ihevce_coarse_me_get_rc_param(
                    ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
                    &ps_curr_out->i8_acc_frame_coarse_me_cost,
                    &ps_curr_out->i8_acc_frame_coarse_me_sad,
                    &ps_curr_out->i8_acc_num_blks_high_sad,
                    &ps_curr_out->i8_total_blks,
                    ps_curr_inp->s_lap_out.i4_is_prev_pic_in_Tid0_same_scene);

                if(ps_curr_out->i8_total_blks)
                {
                    ps_curr_out->i4_complexity_percentage = (WORD32)(
                        (ps_curr_out->i8_acc_num_blks_high_sad * 100) /
                        (ps_curr_out->i8_total_blks));
                }
                /*not for Const QP mode*/
                if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
                {
                    if(ps_curr_inp->s_lap_out.i4_is_prev_pic_in_Tid0_same_scene &&
                       ps_curr_out->i8_total_blks &&
                       (((float)(ps_curr_out->i8_acc_num_blks_high_sad * 100) /
                         (ps_curr_out->i8_total_blks)) > (i4_cmplx_change_detection_thrsh)))
                    {
                        ps_curr_out->i4_is_high_complex_region = 1;
                    }
                    else
                    {
                        ps_curr_out->i4_is_high_complex_region = 0;
                    }
                }
                ps_curr_inp->s_rc_lap_out.i8_frame_acc_coarse_me_cost =
                    ps_curr_out->i8_acc_frame_coarse_me_cost;
                /*check for I only reset case and Non I SCD*/
                ihevce_rc_check_non_lap_scd(
                    ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0], &ps_curr_inp->s_rc_lap_out);
            }
        }
        /* release mutex lock after rate control calls */
        osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_frame_init \endif
*
* \brief
*    Pre encode Frame processing slave thread entry point function
*
* \param[in] Frame processing thread context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_frame_init(
    enc_ctxt_t *ps_enc_ctxt,
    pre_enc_me_ctxt_t *ps_curr_inp_prms,
    me_enc_rdopt_ctxt_t *ps_cur_out_me_prms,
    WORD32 i4_cur_frame_qp,
    WORD32 i4_me_frm_id,
    WORD32 i4_thrd_id)
{
    ihevce_lap_enc_buf_t *ps_curr_inp;
    WORD32 first_field = 1;
    me_master_ctxt_t *ps_master_ctxt;

    (void)i4_thrd_id;
    (void)ps_cur_out_me_prms;
    ps_curr_inp = ps_curr_inp_prms->ps_curr_inp;

    ps_master_ctxt = (me_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_me_ctxt;

    /* get frame level lambda params */
    ihevce_get_frame_lambda_prms(
        ps_enc_ctxt,
        ps_curr_inp_prms,
        i4_cur_frame_qp,
        first_field,
        ps_curr_inp->s_lap_out.i4_is_ref_pic,
        ps_curr_inp->s_lap_out.i4_temporal_lyr_id,
        ps_curr_inp->s_lap_out.f_i_pic_lamda_modifier,
        0,
        ENC_LAMBDA_TYPE);

    if(1 == ps_curr_inp_prms->i4_frm_proc_valid_flag)
    {
        UWORD8 i1_cu_qp_delta_enabled_flag =
            ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_cu_level_rc;

        /* picture level init of ME */
        ihevce_me_frame_init(
            ps_enc_ctxt->s_module_ctxt.pv_me_ctxt,
            ps_cur_out_me_prms,
            ps_enc_ctxt->ps_stat_prms,
            &ps_enc_ctxt->s_frm_ctb_prms,
            &ps_curr_inp_prms->as_lambda_prms[0],
            ps_enc_ctxt->i4_num_ref_l0,
            ps_enc_ctxt->i4_num_ref_l1,
            ps_enc_ctxt->i4_num_ref_l0_active,
            ps_enc_ctxt->i4_num_ref_l1_active,
            &ps_cur_out_me_prms->aps_ref_list[0][LIST_0][0],
            &ps_cur_out_me_prms->aps_ref_list[0][LIST_1][0],
            ps_cur_out_me_prms->aps_ref_list[0],
            &ps_enc_ctxt->s_func_selector,
            ps_curr_inp,
            ps_curr_inp_prms->pv_me_lyr_ctxt,
            i4_me_frm_id,
            i4_thrd_id,
            i4_cur_frame_qp,
            ps_curr_inp->s_lap_out.i4_temporal_lyr_id,
            i1_cu_qp_delta_enabled_flag,
            ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id]->pv_dep_mngr_encloop_dep_me);

        /* -------------------------------------------------------- */
        /* Preparing Job Queue for ME and each instance of enc_loop */
        /* -------------------------------------------------------- */
        ihevce_prepare_job_queue(ps_enc_ctxt, ps_curr_inp, i4_me_frm_id);

        /* Dep. Mngr : Reset the num ctb processed in every row  for ENC sync */
        ihevce_dmgr_rst_row_row_sync(
            ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id]->pv_dep_mngr_encloop_dep_me);
    }
}

/****************************************************************************
Function Name : ihevce_rc_close
Description   : closing the Rate control by passing the stored data in to the stat file for 2 pass encoding.
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/

void ihevce_rc_close(
    enc_ctxt_t *ps_enc_ctxt,
    WORD32 i4_enc_frm_id_rc,
    WORD32 i4_store_retrive,
    WORD32 i4_update_cnt,
    WORD32 i4_bit_rate_idx)
{
    rc_bits_sad_t s_rc_frame_stat;
    WORD32 out_buf_id;
    WORD32 i4_pic_type, k;
    WORD32 cur_qp;
    ihevce_lap_output_params_t s_lap_out;
    rc_lap_out_params_t s_rc_lap_out;

    for(k = 0; k < i4_update_cnt; k++)  //ELP_RC
    {
        ihevce_rc_store_retrive_update_info(
            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bit_rate_idx],
            &s_rc_frame_stat,
            i4_enc_frm_id_rc,
            i4_bit_rate_idx,
            2,
            &out_buf_id,
            &i4_pic_type,
            &cur_qp,
            (void *)&s_lap_out,
            (void *)&s_rc_lap_out);

        ihevce_rc_update_pic_info(
            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bit_rate_idx],
            (s_rc_frame_stat.u4_total_texture_bits +
             s_rc_frame_stat.u4_total_header_bits),  //pass total bits
            s_rc_frame_stat.u4_total_header_bits,
            s_rc_frame_stat.u4_total_sad,
            s_rc_frame_stat.u4_total_intra_sad,
            (IV_PICTURE_CODING_TYPE_T)i4_pic_type,
            cur_qp,
            0,
            s_rc_frame_stat.i4_qp_normalized_8x8_cu_sum,
            s_rc_frame_stat.i4_8x8_cu_sum,
            s_rc_frame_stat.i8_sad_by_qscale,
            &s_lap_out,
            &s_rc_lap_out,
            out_buf_id,
            s_rc_frame_stat.u4_open_loop_intra_sad,
            s_rc_frame_stat.i8_total_ssd_frame,
            i4_enc_frm_id_rc);  //ps_curr_out->i4_inp_timestamp_low)
        i4_enc_frm_id_rc++;
        i4_enc_frm_id_rc = (i4_enc_frm_id_rc % ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_frm_proc_slave_thrd \endif
*
* \brief
*    Enocde Frame processing slave thread entry point function
*
* \param[in] Frame processing thread context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_enc_frm_proc_slave_thrd(void *pv_frm_proc_thrd_ctxt)
{
    frm_proc_thrd_ctxt_t *ps_thrd_ctxt;
    enc_ctxt_t *ps_enc_ctxt;
    WORD32 i4_me_end_flag, i4_enc_end_flag;
    WORD32 i4_thrd_id;
    ihevce_hle_ctxt_t *ps_hle_ctxt;
    WORD32 i4_num_bitrates;  //number of bit-rates instances running
    WORD32 i;  //ctr
    void *pv_dep_mngr_prev_frame_me_done;
    void *pv_dep_mngr_prev_frame_done;
    WORD32 i4_resolution_id;
    WORD32 i4_enc_frm_id_rc = 0;
    WORD32 i4_enc_frm_id = 0;
    WORD32 i4_me_frm_id = 0;

    ps_thrd_ctxt = (frm_proc_thrd_ctxt_t *)pv_frm_proc_thrd_ctxt;
    ps_hle_ctxt = ps_thrd_ctxt->ps_hle_ctxt;
    ps_enc_ctxt = (enc_ctxt_t *)ps_thrd_ctxt->pv_enc_ctxt; /*Changed for mres*/
    i4_thrd_id = ps_thrd_ctxt->i4_thrd_id;
    i4_me_end_flag = 0;
    i4_enc_end_flag = 0;
    i4_num_bitrates = ps_enc_ctxt->i4_num_bitrates;
    i4_resolution_id = ps_enc_ctxt->i4_resolution_id;

    /*pv_dep_mngr_prev_frame_me_done  =
        ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_me_done;*/

    while((0 == i4_me_end_flag) && (0 == i4_enc_end_flag))
    {
        WORD32 result;
        WORD32 ai4_in_buf_id[MAX_NUM_ME_PARALLEL];
        me_enc_rdopt_ctxt_t *ps_curr_out_me;

        if(1 == ps_enc_ctxt->s_multi_thrd.i4_num_me_frm_pllel)
        {
            pv_dep_mngr_prev_frame_me_done =
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_me_done[0];
        }
        else
        {
            pv_dep_mngr_prev_frame_me_done =
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_me_done[i4_me_frm_id];
        }

        /* Wait till the previous frame ME is completly done*/
        {
            ihevce_dmgr_chk_frm_frm_sync(pv_dep_mngr_prev_frame_me_done, ps_thrd_ctxt->i4_thrd_id);
        }

        /****** Lock the critical section ******/
        if(NULL != ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i4_me_frm_id])
        {
            result = osal_mutex_lock(ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i4_me_frm_id]);

            if(OSAL_SUCCESS != result)
                return 0;
        }

        {
            /************************************/
            /****** ENTER CRITICAL SECTION ******/
            /************************************/

            /* First slave getting the mutex lock will act as master and does ME init
            * of current frame and other slaves skip it
            */
            if(ps_enc_ctxt->s_multi_thrd.ai4_me_master_done_flag[i4_me_frm_id] == 0)
            {
                WORD32 i4_ref_cur_qp;  //current frame Qp for reference bit-rate instance
                ihevce_lap_enc_buf_t *ps_curr_inp = NULL;

                if(0 == i4_me_end_flag)
                {
                    /* ------- get the input prms buffer from pre encode que ------------ */
                    ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id] =
                        (pre_enc_me_ctxt_t *)ihevce_q_get_filled_buff(
                            (void *)ps_enc_ctxt,
                            IHEVCE_PRE_ENC_ME_Q,
                            &ai4_in_buf_id[i4_me_frm_id],
                            BUFF_QUE_BLOCKING_MODE);
                    /*always buffer must be available*/
                    ASSERT(ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id] != NULL);

                    ps_enc_ctxt->s_multi_thrd.is_in_buf_freed[i4_enc_frm_id] = 0;

                    /* ------- get the input prms buffer from L0 IPE queue ------------ */
                    ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id] =
                        (pre_enc_L0_ipe_encloop_ctxt_t *)ihevce_q_get_filled_buff(
                            (void *)ps_enc_ctxt,
                            IHEVCE_L0_IPE_ENC_Q,
                            &ps_enc_ctxt->s_multi_thrd.ai4_in_frm_l0_ipe_id[i4_me_frm_id],
                            BUFF_QUE_BLOCKING_MODE);

                    /*always buffer must be available*/
                    ASSERT(ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id] != NULL);

                    /* ------- get the free buffer from me_enc que ------------ */
                    ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id] =
                        (me_enc_rdopt_ctxt_t *)ihevce_q_get_free_buff(
                            ps_enc_ctxt,
                            IHEVCE_ME_ENC_RDOPT_Q,
                            &ps_enc_ctxt->s_multi_thrd.ai4_me_out_buf_id[i4_me_frm_id],
                            BUFF_QUE_BLOCKING_MODE);

                    /*always buffer must be available*/
                    ASSERT(ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id] != NULL);
                }
                if(NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id] &&
                   NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id] &&
                   NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id])
                {
                    ps_curr_inp =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->ps_curr_inp;

                    ps_curr_out_me = ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id];

                    ps_curr_out_me->ps_curr_inp_from_l0_ipe_prms =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id];

                    /*initialization of curr out me*/
                    ps_curr_out_me->ps_curr_inp_from_me_prms =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id];

                    ps_curr_out_me->curr_inp_from_me_buf_id = ai4_in_buf_id[i4_me_frm_id];

                    ps_curr_out_me->i4_buf_id =
                        ps_enc_ctxt->s_multi_thrd.ai4_me_out_buf_id[i4_me_frm_id];

                    ps_curr_out_me->ps_curr_inp =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->ps_curr_inp;

                    ps_curr_out_me->curr_inp_buf_id =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->curr_inp_buf_id;

                    ps_curr_out_me->curr_inp_from_l0_ipe_buf_id =
                        ps_enc_ctxt->s_multi_thrd.ai4_in_frm_l0_ipe_id[i4_me_frm_id];

                    ps_curr_out_me->i4_frm_proc_valid_flag =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                            ->i4_frm_proc_valid_flag;

                    ps_curr_out_me->i4_end_flag =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->i4_end_flag;

                    /* do the processing if input frm data is valid */
                    if(1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag)
                    {
                        /* slice header will be populated in pre-enocde stage */
                        memcpy(
                            &ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id]
                                 ->s_slice_hdr,
                            &ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                                 ->s_slice_hdr,
                            sizeof(slice_header_t));

                        if(ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                               ->i4_frm_proc_valid_flag)
                        {
                            WORD32 ctr;
                            recon_pic_buf_t *ps_frm_recon;
                            for(i = 0; i < i4_num_bitrates; i++)
                            {
                                /* run a loop to free the non used reference pics */
                                /* This is done here because its assured that recon buf
                                * between app and encode loop is set as produced
                                */
                                {
                                    WORD32 i4_free_id;
                                    i4_free_id = ihevce_find_free_indx(
                                        ps_enc_ctxt->pps_recon_buf_q[i],
                                        ps_enc_ctxt->ai4_num_buf_recon_q[i]);

                                    if(i4_free_id != -1)
                                    {
                                        ps_enc_ctxt->pps_recon_buf_q[i][i4_free_id]->i4_is_free = 1;
                                        ps_enc_ctxt->pps_recon_buf_q[i][i4_free_id]->i4_poc = -1;
                                    }
                                }

                                ps_frm_recon = NULL;
                                for(ctr = 0; ctr < ps_enc_ctxt->ai4_num_buf_recon_q[i]; ctr++)
                                {
                                    if(ps_enc_ctxt->pps_recon_buf_q[i][ctr]->i4_is_free)
                                    {
                                        ps_frm_recon = ps_enc_ctxt->pps_recon_buf_q[i][ctr];
                                        break;
                                    }
                                }
                                ASSERT(ps_frm_recon != NULL);

                                ps_frm_recon->i4_is_free = 0;
                                ps_frm_recon->i4_non_ref_free_flag = 0;
                                ps_frm_recon->i4_topfield_first =
                                    ps_curr_inp->s_input_buf.i4_topfield_first;
                                ps_frm_recon->i4_poc = ps_curr_inp->s_lap_out.i4_poc;
                                ps_frm_recon->i4_pic_type = ps_curr_inp->s_lap_out.i4_pic_type;
                                ps_frm_recon->i4_display_num =
                                    ps_curr_inp->s_lap_out.i4_display_num;
                                ps_frm_recon->i4_idr_gop_num =
                                    ps_curr_inp->s_lap_out.i4_idr_gop_num;
                                ps_frm_recon->i4_bottom_field =
                                    ps_curr_inp->s_input_buf.i4_bottom_field;
                                ps_frm_recon->i4_is_reference =
                                    ps_curr_inp->s_lap_out.i4_is_ref_pic;

                                {
                                    WORD32 sei_hash_enabled =
                                        (ps_enc_ctxt->ps_stat_prms->s_out_strm_prms
                                             .i4_sei_enable_flag == 1) &&
                                        (ps_enc_ctxt->ps_stat_prms->s_out_strm_prms
                                             .i4_decoded_pic_hash_sei_flag != 0);

                                    /* Deblock a picture for all reference frames unconditionally. */
                                    /* Deblock non ref if psnr compute or save recon is enabled    */
                                    ps_frm_recon->i4_deblk_pad_hpel_cur_pic =
                                        ps_frm_recon->i4_is_reference ||
                                        (ps_enc_ctxt->ps_stat_prms->i4_save_recon) ||
                                        (1 == sei_hash_enabled);
                                }

                                ps_frm_recon->s_yuv_buf_desc.i4_y_ht =
                                    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht;
                                ps_frm_recon->s_yuv_buf_desc.i4_uv_ht =
                                    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht >>
                                    ((ps_enc_ctxt->s_runtime_src_prms.i4_chr_format ==
                                      IV_YUV_422SP_UV)
                                         ? 0
                                         : 1);
                                ps_frm_recon->s_yuv_buf_desc.i4_y_wd =
                                    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd;
                                ps_frm_recon->s_yuv_buf_desc.i4_uv_wd =
                                    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd;
                                ps_frm_recon->s_yuv_buf_desc.i4_y_strd =
                                    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd +
                                    (PAD_HORZ << 1);
                                ps_frm_recon->s_yuv_buf_desc.i4_uv_strd =
                                    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd +
                                    (PAD_HORZ << 1);

                                /* reset the row_frm dep mngr for ME reverse sync for reference bitrate */
                                if(i == 0)
                                {
                                    ihevce_dmgr_map_rst_sync(ps_frm_recon->pv_dep_mngr_recon);
                                }

                                ps_enc_ctxt->s_multi_thrd.ps_frm_recon[i4_enc_frm_id][i] =
                                    ps_frm_recon;
                            }
                        }
                        /* Reference buffer management and reference list creation */
                        /* This needs to be created for each bit-rate since the reconstructed output is
                        different for all bit-rates. ME uses only 0th instnace ref list */
                        for(i = i4_num_bitrates - 1; i >= 0; i--)
                        {
                            ihevce_manage_ref_pics(
                                ps_enc_ctxt,
                                ps_curr_inp,
                                &ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id]
                                     ->s_slice_hdr,
                                i4_me_frm_id,
                                i4_thrd_id,
                                i); /* bitrate instance ID */
                        }
                        /*query of qp to be moved just before encoding starts*/
                        i4_ref_cur_qp = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                                            ->i4_curr_frm_qp;
                        /* The Qp populated in Pre enc stage needs to overwritten with Qp
                        queried from rate control*/
                    }
                    else
                    {
                        i4_ref_cur_qp = 0;
                    }

                    /* call the core encoding loop */
                    ihevce_frame_init(
                        ps_enc_ctxt,
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id],
                        ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id],
                        i4_ref_cur_qp,
                        i4_me_frm_id,
                        i4_thrd_id);
                }

                ps_enc_ctxt->s_multi_thrd.ai4_me_master_done_flag[i4_me_frm_id] = 1;
            }
        }

        /************************************/
        /******  EXIT CRITICAL SECTION ******/
        /************************************/

        /****** Unlock the critical section ******/
        if(NULL != ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i4_me_frm_id])
        {
            result = osal_mutex_unlock(ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i4_me_frm_id]);
            if(OSAL_SUCCESS != result)
                return 0;
        }

        if((1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out) &&
           (1 == ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                     ->ps_curr_inp->s_lap_out.i4_first_frm_new_res))
        {
            /* Reset the enc frame rc id whenver change in resolution happens */
            i4_enc_frm_id_rc = 0;
        }

        /*update end flag for each thread */
        i4_me_end_flag = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->i4_end_flag;
        if(NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id] &&
           NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id] &&
           NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id])
        {
            pre_enc_me_ctxt_t *ps_curr_inp_prms;
            pre_enc_L0_ipe_encloop_ctxt_t *ps_curr_L0_IPE_inp_prms;
            ihevce_lap_enc_buf_t *ps_curr_inp;

            /* get the current  buffer pointer  */
            ps_curr_inp_prms = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id];
            ps_curr_L0_IPE_inp_prms =
                ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id];
            ps_curr_inp = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->ps_curr_inp;
            if(i4_thrd_id == 0)
            {
                PROFILE_START(&ps_hle_ctxt->profile_enc_me[ps_enc_ctxt->i4_resolution_id]);
            }

            /* -------------------------------------------------- */
            /*    Motion estimation (enc layer) of entire frame   */
            /* -------------------------------------------------- */
            if((i4_me_end_flag == 0) &&
               (1 ==
                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->i4_frm_proc_valid_flag))
            {
                /* Init i4_is_prev_frame_reference for the next P-frame */
                me_master_ctxt_t *ps_master_ctxt =
                    (me_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_me_ctxt;

                /* get the current thread ctxt pointer */
                me_ctxt_t *ps_ctxt = ps_master_ctxt->aps_me_ctxt[i4_thrd_id];

                me_frm_ctxt_t *ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i4_me_frm_id];

                if(ISLICE != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                                 ->s_slice_hdr.i1_slice_type)
                {
                    ihevce_me_process(
                        ps_enc_ctxt->s_module_ctxt.pv_me_ctxt,
                        ps_curr_inp,
                        ps_curr_inp_prms->ps_ctb_analyse,
                        ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id],
                        ps_curr_inp_prms->plf_intra_8x8_cost,
                        ps_curr_L0_IPE_inp_prms->ps_ipe_analyse_ctb,
                        ps_curr_L0_IPE_inp_prms,
                        ps_curr_inp_prms->pv_me_lyr_ctxt,
                        &ps_enc_ctxt->s_multi_thrd,
                        ((ps_enc_ctxt->s_multi_thrd.i4_num_me_frm_pllel == 1) ? 0 : 1),
                        i4_thrd_id,
                        i4_me_frm_id);
                }
                else
                {
                    /* Init i4_is_prev_frame_reference for the next P-frame */
                    me_master_ctxt_t *ps_master_ctxt =
                        (me_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_me_ctxt;

                    /* get the current thread ctxt pointer */
                    me_ctxt_t *ps_ctxt = ps_master_ctxt->aps_me_ctxt[i4_thrd_id];

                    me_frm_ctxt_t *ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i4_me_frm_id];

                    multi_thrd_ctxt_t *ps_multi_thrd_ctxt = &ps_enc_ctxt->s_multi_thrd;

                    if(ps_enc_ctxt->s_multi_thrd.i4_num_me_frm_pllel != 1)
                    {
                        ps_frm_ctxt->i4_is_prev_frame_reference = 0;
                    }
                    else
                    {
                        ps_frm_ctxt->i4_is_prev_frame_reference =
                            ps_multi_thrd_ctxt->aps_cur_inp_me_prms[i4_me_frm_id]
                                ->ps_curr_inp->s_lap_out.i4_is_ref_pic;
                    }
                }
            }
            if(i4_thrd_id == 0)
            {
                PROFILE_STOP(&ps_hle_ctxt->profile_enc_me[ps_enc_ctxt->i4_resolution_id], NULL);
            }
        }
        /************************************/
        /******  ENTER CRITICAL SECTION *****/
        /************************************/
        {
            WORD32 result_frame_init;
            void *pv_mutex_handle_frame_init;

            /* Create mutex for locking non-reentrant sections      */
            pv_mutex_handle_frame_init =
                ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_me_end[i4_me_frm_id];

            /****** Lock the critical section ******/
            if(NULL != pv_mutex_handle_frame_init)
            {
                result_frame_init = osal_mutex_lock(pv_mutex_handle_frame_init);

                if(OSAL_SUCCESS != result_frame_init)
                    return 0;
            }
        }

        if(0 == ps_enc_ctxt->s_multi_thrd.ai4_me_enc_buff_prod_flag[i4_me_frm_id])
        {
            /* ------- set buffer produced from me_enc que ------------ */
            ihevce_q_set_buff_prod(
                ps_enc_ctxt,
                IHEVCE_ME_ENC_RDOPT_Q,
                ps_enc_ctxt->s_multi_thrd.ai4_me_out_buf_id[i4_me_frm_id]);

            ps_enc_ctxt->s_multi_thrd.ai4_me_enc_buff_prod_flag[i4_me_frm_id] = 1;
        }
        if(NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id] &&
           NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id])
        {
            ihevce_lap_enc_buf_t *ps_curr_inp;

            WORD32 first_field = 1;

            /* Increment the counter to keep track of no of threads exiting the current mutex*/
            ps_enc_ctxt->s_multi_thrd.me_num_thrds_exited[i4_me_frm_id]++;

            ps_curr_inp = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]->ps_curr_inp;
            /* Last slave thread will reset the master done frame init flag and set the prev
            * frame me done flag for curr frame
            */
            if(ps_enc_ctxt->s_multi_thrd.me_num_thrds_exited[i4_me_frm_id] ==
               ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds)
            {
                ps_enc_ctxt->s_multi_thrd.me_num_thrds_exited[i4_me_frm_id] = 0;

                ps_enc_ctxt->s_multi_thrd.ai4_me_master_done_flag[i4_me_frm_id] = 0;

                /* Update Dyn. Vert. Search prms for P Pic. */
                if(IV_P_FRAME == ps_curr_inp->s_lap_out.i4_pic_type)
                {
                    WORD32 i4_idx_dvsr_p = ps_enc_ctxt->s_multi_thrd.i4_idx_dvsr_p;
                    /* Sanity Check */
                    ASSERT(ps_curr_inp->s_lap_out.i4_pic_type < IV_IP_FRAME);

                    /*  Frame END processing for Dynamic Vertival Search    */
                    ihevce_l0_me_frame_end(
                        ps_enc_ctxt->s_module_ctxt.pv_me_ctxt,
                        i4_idx_dvsr_p,
                        ps_curr_inp->s_lap_out.i4_display_num,
                        i4_me_frm_id);

                    ps_enc_ctxt->s_multi_thrd.i4_idx_dvsr_p++;
                    if(ps_enc_ctxt->s_multi_thrd.i4_idx_dvsr_p == NUM_SG_INTERLEAVED)
                    {
                        ps_enc_ctxt->s_multi_thrd.i4_idx_dvsr_p = 0;
                    }
                }
                if(1 == ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id]
                            ->i4_frm_proc_valid_flag)
                {
                    /* Init i4_is_prev_frame_reference for the next P-frame */
                    me_master_ctxt_t *ps_master_ctxt =
                        (me_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_me_ctxt;

                    /* get the current thread ctxt pointer */
                    me_ctxt_t *ps_ctxt = ps_master_ctxt->aps_me_ctxt[i4_thrd_id];

                    me_frm_ctxt_t *ps_frm_ctxt = ps_ctxt->aps_me_frm_prms[i4_me_frm_id];

                    ps_frm_ctxt->ps_curr_descr->aps_layers[0]->i4_non_ref_free = 1;
                }
                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_me_prms[i4_me_frm_id] = NULL;
                ps_enc_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_me_frm_id] = NULL;
                ps_enc_ctxt->s_multi_thrd.aps_cur_L0_ipe_inp_prms[i4_me_frm_id] = NULL;
                ps_enc_ctxt->s_multi_thrd.ai4_me_enc_buff_prod_flag[i4_me_frm_id] = 0;
                ps_enc_ctxt->s_multi_thrd.ai4_me_master_done_flag[i4_me_frm_id] = 0;

                /* Set me processing done for curr frame in the dependency manager */
                ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_me_done);
            }
        }
        /************************************/
        /******  EXIT CRITICAL SECTION ******/
        /************************************/

        {
            void *pv_mutex_handle_frame_init;

            /* Create mutex for locking non-reentrant sections      */
            pv_mutex_handle_frame_init =
                ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_me_end[i4_me_frm_id];
            /****** Unlock the critical section ******/
            if(NULL != pv_mutex_handle_frame_init)
            {
                result = osal_mutex_unlock(pv_mutex_handle_frame_init);
                if(OSAL_SUCCESS != result)
                    return 0;
            }
        }
        /* -------------------------------------------- */
        /*        Encode Loop of entire frame           */
        /* -------------------------------------------- */
        ASSERT(ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel <= MAX_NUM_ENC_LOOP_PARALLEL);

        if(1 == ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel)
        {
            pv_dep_mngr_prev_frame_done = ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_done[0];
        }
        else
        {
            pv_dep_mngr_prev_frame_done =
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_done[i4_enc_frm_id];
        }
        /* Wait till the prev frame enc loop is completed*/
        {
            ihevce_dmgr_chk_frm_frm_sync(pv_dep_mngr_prev_frame_done, ps_thrd_ctxt->i4_thrd_id);
        }

        /************************************/
        /****** ENTER CRITICAL SECTION ******/
        /************************************/
        {
            WORD32 result_frame_init;
            void *pv_mutex_handle_frame_init;

            /* Create mutex for locking non-reentrant sections      */
            pv_mutex_handle_frame_init =
                ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_frame_init[i4_enc_frm_id];

            /****** Lock the critical section ******/
            if(NULL != pv_mutex_handle_frame_init)
            {
                result_frame_init = osal_mutex_lock(pv_mutex_handle_frame_init);

                if(OSAL_SUCCESS != result_frame_init)
                    return 0;
            }
        }

        {
            ihevce_lap_enc_buf_t *ps_curr_inp = NULL;
            pre_enc_me_ctxt_t *ps_curr_inp_from_me = NULL;
            me_enc_rdopt_ctxt_t *ps_curr_inp_enc = NULL;
            pre_enc_L0_ipe_encloop_ctxt_t *ps_curr_L0_IPE_inp_prms = NULL;
            recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2];
            WORD32 ai4_cur_qp[IHEVCE_MAX_NUM_BITRATES] = { 0 };
            WORD32 i4_field_pic = ps_enc_ctxt->s_runtime_src_prms.i4_field_pic;
            WORD32 first_field = 1;
            WORD32 result_frame_init;
            void *pv_mutex_handle_frame_init;

            /* Create mutex for locking non-reentrant sections      */
            pv_mutex_handle_frame_init =
                ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_frame_init[i4_enc_frm_id];

            //aquire and initialize -> output and recon buffers
            if(ps_enc_ctxt->s_multi_thrd.enc_master_done_frame_init[i4_enc_frm_id] == 0)
            {
                WORD32
                    i4_bitrate_ctr;  //bit-rate instance counter (for loop variable) [0->reference bit-rate, 1,2->auxiliarty bit-rates]
                /* ------- get the input prms buffer from me que ------------ */
                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] =
                    (me_enc_rdopt_ctxt_t *)ihevce_q_get_filled_buff(
                        ps_enc_ctxt,
                        IHEVCE_ME_ENC_RDOPT_Q,
                        &ps_enc_ctxt->s_multi_thrd.i4_enc_in_buf_id[i4_enc_frm_id],
                        BUFF_QUE_BLOCKING_MODE);
                i4_enc_end_flag =
                    ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]->i4_end_flag;

                ASSERT(ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] != NULL);

                if(ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] != NULL)
                {
                    ps_curr_inp =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]->ps_curr_inp;
                    ps_curr_inp_from_me =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                            ->ps_curr_inp_from_me_prms;
                    ps_curr_inp_enc = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id];
                    ps_curr_L0_IPE_inp_prms =
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                            ->ps_curr_inp_from_l0_ipe_prms;

                    for(i4_bitrate_ctr = 0; i4_bitrate_ctr < i4_num_bitrates; i4_bitrate_ctr++)
                    {
                        iv_enc_recon_data_buffs_t
                            *ps_recon_out[MAX_NUM_ENC_LOOP_PARALLEL][IHEVCE_MAX_NUM_BITRATES] = {
                                { NULL }
                            };
                        frm_proc_ent_cod_ctxt_t *ps_curr_out[MAX_NUM_ENC_LOOP_PARALLEL]
                                                            [IHEVCE_MAX_NUM_BITRATES] = { { NULL } };

                        /* ------- get free output buffer from Frame buffer que ---------- */
                        /* There is a separate queue for each bit-rate instnace. The output
                    buffer is acquired from the corresponding queue based on the
                    bitrate instnace */
                        ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr] =
                            (frm_proc_ent_cod_ctxt_t *)ihevce_q_get_free_buff(
                                (void *)ps_enc_ctxt,
                                IHEVCE_FRM_PRS_ENT_COD_Q +
                                    i4_bitrate_ctr, /*decides the buffer queue */
                                &ps_enc_ctxt->s_multi_thrd.out_buf_id[i4_enc_frm_id][i4_bitrate_ctr],
                                BUFF_QUE_BLOCKING_MODE);
                        ps_enc_ctxt->s_multi_thrd.is_out_buf_freed[i4_enc_frm_id][i4_bitrate_ctr] =
                            0;
                        ps_enc_ctxt->s_multi_thrd
                            .ps_curr_out_enc_grp[i4_enc_frm_id][i4_bitrate_ctr] =
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr];
                        //ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_enc_order_num = ps_curr_inp->s_lap_out.i4_enc_order_num;
                        /*registered User Data Call*/
                        if(ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_payload_enable_flag)
                        {
                            ihevce_fill_sei_payload(
                                ps_enc_ctxt,
                                ps_curr_inp,
                                ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]);
                        }

                        /*derive end flag and input valid flag in output buffer */
                        if(NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id])
                        {
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_end_flag =
                                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                    ->i4_end_flag;
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_frm_proc_valid_flag =
                                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                    ->i4_frm_proc_valid_flag;

                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_out_flush_flag =
                                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                    ->ps_curr_inp->s_lap_out.i4_out_flush_flag;
                        }

                        /*derive other parameters in output buffer */
                        if(NULL != ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr] &&
                           (NULL != ps_curr_inp_from_me) &&
                           (1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag) &&
                           (i4_enc_end_flag == 0))
                        {
                            /* copy the time stamps from inp to entropy inp */
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_inp_timestamp_low =
                                ps_curr_inp_from_me->i4_inp_timestamp_low;
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_inp_timestamp_high =
                                ps_curr_inp_from_me->i4_inp_timestamp_high;
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->pv_app_frm_ctxt =
                                ps_curr_inp_from_me->pv_app_frm_ctxt;

                            /*copy slice header params from temp structure to output buffer */
                            memcpy(
                                &ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->s_slice_hdr,
                                &ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                     ->s_slice_hdr,
                                sizeof(slice_header_t));

                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]
                                ->s_slice_hdr.pu4_entry_point_offset =
                                &ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]
                                     ->ai4_entry_point_offset[0];

                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_slice_nal_type =
                                ps_curr_inp_from_me->i4_slice_nal_type;

                            /* populate sps, vps and pps pointers for the entropy input params */
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->ps_pps =
                                &ps_enc_ctxt->as_pps[i4_bitrate_ctr];
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->ps_sps =
                                &ps_enc_ctxt->as_sps[i4_bitrate_ctr];
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->ps_vps =
                                &ps_enc_ctxt->as_vps[i4_bitrate_ctr];

                            /* SEI header will be populated in pre-enocde stage */
                            memcpy(
                                &ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->s_sei,
                                &ps_curr_inp_from_me->s_sei,
                                sizeof(sei_params_t));

                            /*AUD and EOS presnt flags are populated*/
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i1_aud_present_flag =
                                ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_aud_enable_flags;

                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i1_eos_present_flag =
                                ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_eos_enable_flags;

                            /* Information required for SEI Picture timing info */
                            {
                                ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_display_num =
                                    ps_curr_inp->s_lap_out.i4_display_num;
                            }

                            /* The Qp populated in Pre enc stage needs to overwritten with Qp
                        queried from rate control*/
                            ps_curr_out[i4_enc_frm_id][i4_bitrate_ctr]
                                ->s_slice_hdr.i1_slice_qp_delta =
                                (WORD8)ps_curr_inp_from_me->i4_curr_frm_qp -
                                ps_enc_ctxt->as_pps[i4_bitrate_ctr].i1_pic_init_qp;
                        }

                        /* ------- get a filled descriptor from output Que ------------ */
                        if(/*(1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag) &&*/
                           (ps_enc_ctxt->ps_stat_prms->i4_save_recon != 0))
                        {
                            /*swaping of buf_id for 0th and reference bitrate location, as encoder
                        assumes always 0th loc for reference bitrate and app must receive in
                        the configured order*/
                            WORD32 i4_recon_buf_id = i4_bitrate_ctr;
                            if(i4_bitrate_ctr == 0)
                            {
                                i4_recon_buf_id = ps_enc_ctxt->i4_ref_mbr_id;
                            }
                            else if(i4_bitrate_ctr == ps_enc_ctxt->i4_ref_mbr_id)
                            {
                                i4_recon_buf_id = 0;
                            }

                            /* ------- get free Recon buffer from Frame buffer que ---------- */
                            /* There is a separate queue for each bit-rate instnace. The recon
                        buffer is acquired from the corresponding queue based on the
                        bitrate instnace */
                            ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr] =
                                (iv_enc_recon_data_buffs_t *)ihevce_q_get_filled_buff(
                                    (void *)ps_enc_ctxt,
                                    IHEVCE_RECON_DATA_Q +
                                        i4_recon_buf_id, /*decides the buffer queue */
                                    &ps_enc_ctxt->s_multi_thrd
                                         .recon_buf_id[i4_enc_frm_id][i4_bitrate_ctr],
                                    BUFF_QUE_BLOCKING_MODE);

                            ps_enc_ctxt->s_multi_thrd
                                .is_recon_dumped[i4_enc_frm_id][i4_bitrate_ctr] = 0;
                            ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr] =
                                ps_enc_ctxt->s_multi_thrd
                                    .ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr];

                            ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_end_flag =
                                ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                    ->i4_end_flag;
                        }

                    }  //bitrate ctr
                }
            }
            if(ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] != NULL)
            {
                ps_curr_inp =
                    ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]->ps_curr_inp;
                ps_curr_inp_from_me = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                          ->ps_curr_inp_from_me_prms;
                ps_curr_inp_enc = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id];
                ps_curr_L0_IPE_inp_prms =
                    ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                        ->ps_curr_inp_from_l0_ipe_prms;
            }
            if((NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]) &&
               ((1 == ps_curr_inp_enc->i4_frm_proc_valid_flag) &&
                (ps_enc_ctxt->s_multi_thrd.enc_master_done_frame_init[i4_enc_frm_id] == 0)))
            {
                for(i = 0; i < i4_num_bitrates; i++)
                {
                    aps_ref_list = ps_curr_inp_enc->aps_ref_list[i];
                    /* acquire mutex lock for rate control calls */
                    osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

                    /*utlize the satd data from pre enc stage to get more accurate estimate SAD for I pic*/
                    if(ps_curr_inp->s_lap_out.i4_pic_type == IV_I_FRAME ||
                       ps_curr_inp->s_lap_out.i4_pic_type == IV_IDR_FRAME)
                    {
                        ihevce_rc_update_cur_frm_intra_satd(
                            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                            ps_curr_inp_from_me->i8_frame_acc_satd_cost,
                            ps_enc_ctxt->i4_active_enc_frame_id);
                    }

                    /*pels assuming satd/act is obtained for entire frame*/
                    ps_curr_inp->s_rc_lap_out.i4_num_pels_in_frame_considered =
                        ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht *
                        ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd;

                    /*Service pending request to change average bitrate if any*/
                    {
                        LWORD64 i8_new_bitrate =
                            ihevce_rc_get_new_bitrate(ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0]);
                        LWORD64 i8_new_peak_bitrate = ihevce_rc_get_new_peak_bitrate(
                            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0]);
                        ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                            ->i8_buf_level_bitrate_change = -1;
                        if((i8_new_bitrate != -1) &&
                           (i8_new_peak_bitrate != -1)) /*-1 indicates no pending request*/
                        {
                            LWORD64 buffer_level = ihevce_rc_change_avg_bitrate(
                                ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0]);
                            ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                ->i8_buf_level_bitrate_change = buffer_level;
                        }
                    }

                    if((1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out) &&
                       (1 == ps_curr_inp->s_lap_out.i4_first_frm_new_res))
                    {
                        /* Whenver change in resolution happens change the buffer level */
                        ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                            ->i8_buf_level_bitrate_change = 0;
                    }
#if 1  //KISH ELP
                    {
                        rc_bits_sad_t as_rc_frame_stat[IHEVCE_MAX_NUM_BITRATES];

                        if(ps_enc_ctxt->ai4_rc_query[i] ==
                           ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc)  //KISH
                        {
                            WORD32 out_buf_id[IHEVCE_MAX_NUM_BITRATES];
                            WORD32 i4_pic_type;
                            WORD32 cur_qp[IHEVCE_MAX_NUM_BITRATES];
                            ihevce_lap_output_params_t s_lap_out;

                            rc_lap_out_params_t s_rc_lap_out;
                            WORD32 i4_suppress_bpic_update;

                            ihevce_rc_store_retrive_update_info(
                                (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                                &as_rc_frame_stat[i],
                                ps_enc_ctxt->i4_active_enc_frame_id,
                                i,
                                2,
                                &out_buf_id[i],
                                &i4_pic_type,
                                &cur_qp[i],
                                (void *)&s_lap_out,
                                (void *)&s_rc_lap_out);

                            i4_suppress_bpic_update =
                                (WORD32)(s_rc_lap_out.i4_rc_temporal_lyr_id > 1);
                            /*RC inter face update before update to happen only for ELP disabled */
                            if(1 == ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc)
                            {
                                /* SGI & Enc Loop Parallelism related changes*/
                                ihevce_rc_interface_update(
                                    (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                                    (IV_PICTURE_CODING_TYPE_T)s_rc_lap_out.i4_rc_pic_type,
                                    &s_rc_lap_out,
                                    cur_qp[i],
                                    i4_enc_frm_id_rc);
                            }

                            ihevce_rc_update_pic_info(
                                (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                                (as_rc_frame_stat[i].u4_total_texture_bits +
                                 as_rc_frame_stat[i].u4_total_header_bits),  //pass total bits
                                as_rc_frame_stat[i].u4_total_header_bits,
                                as_rc_frame_stat[i].u4_total_sad,
                                as_rc_frame_stat[i].u4_total_intra_sad,
                                (IV_PICTURE_CODING_TYPE_T)i4_pic_type,
                                cur_qp[i],
                                i4_suppress_bpic_update,
                                as_rc_frame_stat[i].i4_qp_normalized_8x8_cu_sum,
                                as_rc_frame_stat[i].i4_8x8_cu_sum,
                                as_rc_frame_stat[i].i8_sad_by_qscale,
                                &s_lap_out,
                                &s_rc_lap_out,
                                out_buf_id[i],
                                as_rc_frame_stat[i].u4_open_loop_intra_sad,
                                as_rc_frame_stat[i].i8_total_ssd_frame,
                                ps_enc_ctxt
                                    ->i4_active_enc_frame_id);  //ps_curr_out->i4_inp_timestamp_low)

                            //DBG_PRINTF("\n Sad = %d \t total bits = %d ", s_rc_frame_stat.u4_total_sad, (s_rc_frame_stat.u4_total_texture_bits + s_rc_frame_stat.u4_total_header_bits));
                            /*populate qp for pre enc*/

                            //g_count--;
                            ps_enc_ctxt->ai4_rc_query[i]--;

                            if(i == (i4_num_bitrates - 1))
                            {
                                ihevce_rc_cal_pre_enc_qp(
                                    (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0]);

                                ps_enc_ctxt->i4_active_enc_frame_id++;
                                ps_enc_ctxt->i4_active_enc_frame_id =
                                    (ps_enc_ctxt->i4_active_enc_frame_id %
                                     ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc);
                            }
                        }
                    }
#endif
                    if(ps_enc_ctxt->ai4_rc_query[i] < ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc)
                    {
                        /*HEVC_RC query rate control for qp*/
                        ai4_cur_qp[i] = ihevce_rc_get_pic_quant(
                            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                            &ps_curr_inp->s_rc_lap_out,
                            ENC_GET_QP,
                            i4_enc_frm_id_rc,
                            0,
                            &ps_curr_inp->s_lap_out.ai4_frame_bits_estimated[i]);

                        ps_curr_inp->s_rc_lap_out.i4_orig_rc_qp = ai4_cur_qp[i];

                        ps_enc_ctxt->s_multi_thrd.i4_in_frame_rc_enabled = 0;
                        ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                            ->i4_sub_pic_level_rc = 0;
                        ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                            ->ai4_frame_bits_estimated =
                            ps_curr_inp->s_lap_out.ai4_frame_bits_estimated[i];

                        {
                            ps_enc_ctxt->ai4_rc_query[i]++;
                        }
                    }

                    /* SGI & Enc Loop Parallelism related changes*/
                    ihevce_rc_interface_update(
                        (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                        (IV_PICTURE_CODING_TYPE_T)ps_curr_inp->s_lap_out.i4_pic_type,
                        &ps_curr_inp->s_rc_lap_out,
                        ai4_cur_qp[i],
                        i4_enc_frm_id_rc);

                    //DBG_PRINTF("HEVC_QP = %d  MPEG2_QP = %d\n",cur_qp,gu1_HEVCToMpeg2Quant[cur_qp]);//i_model_print

                    /* release mutex lock after rate control calls */
                    osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                        ->s_slice_hdr.i1_slice_qp_delta =
                        (WORD8)ai4_cur_qp[i] - ps_enc_ctxt->as_pps[i].i1_pic_init_qp;

                    ps_enc_ctxt->s_multi_thrd.cur_qp[i4_enc_frm_id][i] = ai4_cur_qp[i];

                    /* For interlace pictures, first_field depends on topfield_first and bottom field */
                    if(i4_field_pic)
                    {
                        first_field =
                            (ps_curr_inp->s_input_buf.i4_topfield_first ^
                             ps_curr_inp->s_input_buf.i4_bottom_field);
                    }
                    /* get frame level lambda params */
                    ihevce_get_frame_lambda_prms(
                        ps_enc_ctxt,
                        ps_curr_inp_from_me,
                        ai4_cur_qp[i],
                        first_field,
                        ps_curr_inp->s_lap_out.i4_is_ref_pic,
                        ps_curr_inp->s_lap_out.i4_temporal_lyr_id,
                        ps_curr_inp->s_lap_out.f_i_pic_lamda_modifier,
                        i,
                        ENC_LOOP_LAMBDA_TYPE);

#if ADAPT_COLOCATED_FROM_L0_FLAG
                    ps_enc_ctxt->s_multi_thrd.ps_frm_recon[i4_enc_frm_id][i]->i4_frame_qp =
                        ai4_cur_qp[i];
#endif
                }  //bitrate counter ends

                /* Reset the Dependency Mngrs local to EncLoop., ie CU_TopRight and Dblk */
                ihevce_enc_loop_dep_mngr_frame_reset(
                    ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt, i4_enc_frm_id);
            }

            {
                /*Set the master done flag for frame init so that other
                * threads can skip it
                */
                ps_enc_ctxt->s_multi_thrd.enc_master_done_frame_init[i4_enc_frm_id] = 1;
            }

            /************************************/
            /******  EXIT CRITICAL SECTION ******/
            /************************************/

            /****** Unlock the critical section ******/
            if(NULL != pv_mutex_handle_frame_init)
            {
                result_frame_init = osal_mutex_unlock(pv_mutex_handle_frame_init);
                if(OSAL_SUCCESS != result_frame_init)
                    return 0;
            }
            ps_enc_ctxt->s_multi_thrd.i4_encode = 1;
            ps_enc_ctxt->s_multi_thrd.i4_num_re_enc = 0;
            /************************************/
            /******  Do Enc loop process   ******/
            /************************************/
            /* Each thread will run the enc-loop.
            Each thread will initialize it's own enc_loop context and do the processing.
            Each thread will run all the bit-rate instances one after another */
            if((i4_enc_end_flag == 0) &&
               (NULL != ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]) &&
               (1 == ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                         ->i4_frm_proc_valid_flag))
            {
                while(1)
                {
                    ctb_enc_loop_out_t *ps_ctb_enc_loop_frm[IHEVCE_MAX_NUM_BITRATES];
                    cu_enc_loop_out_t *ps_cu_enc_loop_frm[IHEVCE_MAX_NUM_BITRATES];
                    tu_enc_loop_out_t *ps_tu_frm[IHEVCE_MAX_NUM_BITRATES];
                    pu_t *ps_pu_frm[IHEVCE_MAX_NUM_BITRATES];
                    UWORD8 *pu1_frm_coeffs[IHEVCE_MAX_NUM_BITRATES];
                    me_master_ctxt_t *ps_master_me_ctxt =
                        (me_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_me_ctxt;
                    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt =
                        (ihevce_enc_loop_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt;

                    for(i = 0; i < i4_num_bitrates; i++)
                    {
                        if(i4_thrd_id == 0)
                        {
                            PROFILE_START(
                                &ps_hle_ctxt->profile_enc[ps_enc_ctxt->i4_resolution_id][i]);
                        }
                        if(NULL != ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id])
                        {
                            ps_ctb_enc_loop_frm[i] =
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->ps_frm_ctb_data;
                            ps_cu_enc_loop_frm[i] =
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->ps_frm_cu_data;
                            ps_tu_frm[i] =
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->ps_frm_tu_data;
                            ps_pu_frm[i] =
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->ps_frm_pu_data;
                            pu1_frm_coeffs[i] = (UWORD8 *)ps_enc_ctxt->s_multi_thrd
                                                    .ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                                    ->pv_coeff_data;
                        }
                        /*derive reference picture list based on ping or pong instnace */
                        aps_ref_list = ps_curr_inp_enc->aps_ref_list[i];

                        /* Always consider chroma cost when computing cost for derived instance */
                        ps_master_ctxt->aps_enc_loop_thrd_ctxt[i4_thrd_id]->i4_consider_chroma_cost =
                            1;

                        /*************************
                        * MULTI BITRATE CODE START
                        **************************/
                        if(i4_num_bitrates > 1)
                        {
                            ihevce_mbr_quality_tool_set_configuration(
                                ps_master_ctxt->aps_enc_loop_thrd_ctxt[i4_thrd_id],
                                ps_enc_ctxt->ps_stat_prms);
                        }
                        /************************
                        * MULTI BITRATE CODE END
                        *************************/
                        /* picture level init of Encode loop module */
                        ihevce_enc_loop_frame_init(
                            ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt,
                            ps_enc_ctxt->s_multi_thrd.cur_qp[i4_enc_frm_id][i],
                            aps_ref_list,
                            ps_enc_ctxt->s_multi_thrd.ps_frm_recon[i4_enc_frm_id][i],
                            &ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                 ->s_slice_hdr,
                            ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]->ps_pps,
                            ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]->ps_sps,
                            ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]->ps_vps,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i1_weighted_pred_flag,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i1_weighted_bipred_flag,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i4_log2_luma_wght_denom,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i4_log2_chroma_wght_denom,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i4_poc,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i4_display_num,
                            ps_enc_ctxt,
                            ps_curr_inp_enc,
                            i,
                            i4_thrd_id,
                            i4_enc_frm_id,  // update this to enc_loop_ctxt struct
                            i4_num_bitrates,
                            ps_curr_inp_enc->ps_curr_inp->s_lap_out.i4_quality_preset,
                            ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                ->pv_dep_mngr_encloop_dep_me);

                        ihevce_enc_loop_process(
                            ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt,
                            ps_curr_inp,
                            ps_curr_inp_from_me->ps_ctb_analyse,
                            ps_curr_L0_IPE_inp_prms->ps_ipe_analyse_ctb,
                            ps_enc_ctxt->s_multi_thrd.ps_frm_recon[i4_enc_frm_id][i],
                            ps_curr_inp_enc->ps_cur_ctb_cu_tree,
                            ps_ctb_enc_loop_frm[i],
                            ps_cu_enc_loop_frm[i],
                            ps_tu_frm[i],
                            ps_pu_frm[i],
                            pu1_frm_coeffs[i],
                            &ps_enc_ctxt->s_frm_ctb_prms,
                            &ps_curr_inp_from_me->as_lambda_prms[i],
                            &ps_enc_ctxt->s_multi_thrd,
                            i4_thrd_id,
                            i4_enc_frm_id,
                            ps_enc_ctxt->ps_stat_prms->s_pass_prms.i4_pass);
                        if(i4_thrd_id == 0)
                        {
                            PROFILE_STOP(
                                &ps_hle_ctxt->profile_enc[ps_enc_ctxt->i4_resolution_id][i], NULL);
                        }
                    }  //loop over bitrate ends
                    {
                        break;
                    }
                } /*end of while(ps_enc_ctxt->s_multi_thrd.ai4_encode[i4_enc_frm_id] == 1)*/
            }

            /************************************/
            /****** ENTER CRITICAL SECTION ******/
            /************************************/

            /****** Lock the critical section ******/
            if(NULL != ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id])
            {
                result = osal_mutex_lock(
                    ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id]);

                if(OSAL_SUCCESS != result)
                    return 0;
            }
            if(ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] != NULL)
            {
                /* Increment the counter to keep track of no of threads exiting the current mutex*/
                ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id]++;

                /* If the end frame is reached force the last slave to enter the next critical section*/
                if(i4_enc_end_flag == 1)
                {
                    if(ps_enc_ctxt->s_multi_thrd.num_thrds_done ==
                       ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds - 1)
                    {
                        ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id] =
                            ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds;
                    }
                }

                {
                    /*Last slave thread comming out of enc loop will execute next critical section*/
                    if(ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id] ==
                       ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds)
                    {
                        iv_enc_recon_data_buffs_t *ps_recon_out_temp = NULL;
                        recon_pic_buf_t *ps_frm_recon_temp = NULL;
                        ihevce_lap_enc_buf_t *ps_curr_inp;
                        rc_lap_out_params_t *ps_rc_lap_out_next_encode;

                        WORD32 ai4_act_qp[IHEVCE_MAX_NUM_BITRATES];
                        ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id] = 0;

                        ps_curr_inp = ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                          ->ps_curr_inp;

                        for(i = 0; i < i4_num_bitrates; i++)
                        {
                            {
                                WORD32 j, i4_avg_QP;
                                ihevce_enc_loop_master_ctxt_t *ps_master_ctxt =
                                    (ihevce_enc_loop_master_ctxt_t *)
                                        ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt;
                                ihevce_enc_loop_ctxt_t *ps_ctxt, *ps_ctxt_temp;
                                ihevce_enc_loop_ctxt_t *ps_ctxt_last_thrd;
                                LWORD64 i8_total_cu_bits_into_qscale = 0, i8_total_cu_bits = 0;
                                UWORD32 total_frame_intra_sad = 0;
                                UWORD32 total_frame_inter_sad = 0;
                                UWORD32 total_frame_sad = 0;

                                LWORD64 total_frame_intra_cost = 0;
                                LWORD64 total_frame_inter_cost = 0;
                                LWORD64 total_frame_cost = 0;

                                ps_ctxt_last_thrd =
                                    ps_master_ctxt->aps_enc_loop_thrd_ctxt[i4_thrd_id];
                                if(ps_enc_ctxt->s_multi_thrd.i4_in_frame_rc_enabled)
                                {
                                    WORD32 i4_total_ctb =
                                        ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz *
                                        ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert;

                                    ai4_act_qp[i] =
                                        ps_enc_ctxt->s_multi_thrd
                                            .ai4_curr_qp_acc[ps_ctxt_last_thrd->i4_enc_frm_id][i] /
                                        i4_total_ctb;
                                }
                                else
                                {
                                    ai4_act_qp[i] =
                                        ps_enc_ctxt->s_multi_thrd.cur_qp[i4_enc_frm_id][i];
                                }

                                ps_enc_ctxt->s_multi_thrd
                                    .ai4_curr_qp_acc[ps_ctxt_last_thrd->i4_enc_frm_id][i] = 0;

                                /*Reset all the values of sub pic rc to default after the frame is completed */
                                {
                                    ps_enc_ctxt->s_multi_thrd
                                        .ai4_acc_ctb_ctr[ps_ctxt_last_thrd->i4_enc_frm_id][i] = 0;
                                    ps_enc_ctxt->s_multi_thrd
                                        .ai4_ctb_ctr[ps_ctxt_last_thrd->i4_enc_frm_id][i] = 0;

                                    ps_enc_ctxt->s_multi_thrd
                                        .ai4_threshold_reached[ps_ctxt_last_thrd->i4_enc_frm_id][i] =
                                        0;

                                    ps_enc_ctxt->s_multi_thrd
                                        .ai4_curr_qp_estimated[ps_ctxt_last_thrd->i4_enc_frm_id][i] =
                                        (1 << QP_LEVEL_MOD_ACT_FACTOR);

                                    ps_enc_ctxt->s_multi_thrd
                                        .af_acc_hdr_bits_scale_err[ps_ctxt_last_thrd->i4_enc_frm_id]
                                                                  [i] = 0;
                                }
                                for(j = 0; j < ps_master_ctxt->i4_num_proc_thrds; j++)
                                {
                                    /* ENC_LOOP state structure */
                                    ps_ctxt = ps_master_ctxt->aps_enc_loop_thrd_ctxt[j];

                                    total_frame_intra_sad +=
                                        ps_ctxt
                                            ->aaps_enc_loop_rc_params[ps_ctxt_last_thrd
                                                                          ->i4_enc_frm_id][i]
                                            ->u4_frame_intra_sad_acc;
                                    total_frame_inter_sad +=
                                        ps_ctxt
                                            ->aaps_enc_loop_rc_params[ps_ctxt_last_thrd
                                                                          ->i4_enc_frm_id][i]
                                            ->u4_frame_inter_sad_acc;
                                    total_frame_sad +=
                                        ps_ctxt
                                            ->aaps_enc_loop_rc_params[ps_ctxt_last_thrd
                                                                          ->i4_enc_frm_id][i]
                                            ->u4_frame_sad_acc;

                                    total_frame_intra_cost +=
                                        ps_ctxt
                                            ->aaps_enc_loop_rc_params[ps_ctxt_last_thrd
                                                                          ->i4_enc_frm_id][i]
                                            ->i8_frame_intra_cost_acc;
                                    total_frame_inter_cost +=
                                        ps_ctxt
                                            ->aaps_enc_loop_rc_params[ps_ctxt_last_thrd
                                                                          ->i4_enc_frm_id][i]
                                            ->i8_frame_inter_cost_acc;
                                    total_frame_cost +=
                                        ps_ctxt
                                            ->aaps_enc_loop_rc_params[ps_ctxt_last_thrd
                                                                          ->i4_enc_frm_id][i]
                                            ->i8_frame_cost_acc;
                                    /*Reset thrd id flag once the frame is completed */
                                    ps_enc_ctxt->s_multi_thrd
                                        .ai4_thrd_id_valid_flag[ps_ctxt_last_thrd->i4_enc_frm_id][i]
                                                               [j] = -1;
                                }
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->s_pic_level_info.u4_frame_sad = total_frame_sad;
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->s_pic_level_info.u4_frame_intra_sad = total_frame_intra_sad;
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->s_pic_level_info.u4_frame_inter_sad = total_frame_inter_sad;

                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->s_pic_level_info.i8_frame_cost = total_frame_cost;
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->s_pic_level_info.i8_frame_intra_cost = total_frame_intra_cost;
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->s_pic_level_info.i8_frame_inter_cost = total_frame_inter_cost;
                            }
                            ps_enc_ctxt->s_multi_thrd.ai4_produce_outbuf[i4_enc_frm_id][i] = 1;
                            ps_recon_out_temp =
                                ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i];
                            ps_frm_recon_temp =
                                ps_enc_ctxt->s_multi_thrd.ps_frm_recon[i4_enc_frm_id][i];

                            /* end of frame processing only if current input is valid */
                            if(1 == ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id]
                                        ->i4_frm_proc_valid_flag)
                            {
                                /* Calculate the SEI Hash if enabled */
                                if(0 !=
                                   ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                       ->s_sei.i1_decoded_pic_hash_sei_flag)
                                {
                                    void *pv_y_buf;
                                    void *pv_u_buf;

                                    {
                                        pv_y_buf = ps_frm_recon_temp->s_yuv_buf_desc.pv_y_buf;
                                        pv_u_buf = ps_frm_recon_temp->s_yuv_buf_desc.pv_u_buf;
                                    }

                                    ihevce_populate_hash_sei(
                                        &ps_enc_ctxt->s_multi_thrd
                                             .ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                             ->s_sei,
                                        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms
                                            .i4_internal_bit_depth,
                                        pv_y_buf,
                                        ps_frm_recon_temp->s_yuv_buf_desc.i4_y_wd,
                                        ps_frm_recon_temp->s_yuv_buf_desc.i4_y_ht,
                                        ps_frm_recon_temp->s_yuv_buf_desc.i4_y_strd,
                                        pv_u_buf,
                                        ps_frm_recon_temp->s_yuv_buf_desc.i4_uv_wd,
                                        ps_frm_recon_temp->s_yuv_buf_desc.i4_uv_ht,
                                        ps_frm_recon_temp->s_yuv_buf_desc.i4_uv_strd,
                                        0,
                                        0);
                                }
                                /* Sending qp, poc and pic-type to entropy thread for printing on console */
                                if(ps_enc_ctxt->ps_stat_prms->i4_log_dump_level != 0)
                                {
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_qp =
                                        ps_enc_ctxt->s_multi_thrd.cur_qp[i4_enc_frm_id][i];
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_poc = ps_curr_inp->s_lap_out.i4_poc;
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_pic_type = ps_curr_inp->s_lap_out.i4_pic_type;
                                }

                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->i4_is_I_scenecut =
                                    ((ps_curr_inp->s_lap_out.i4_scene_type == 1) &&
                                     (ps_curr_inp->s_lap_out.i4_pic_type == IV_IDR_FRAME ||
                                      ps_curr_inp->s_lap_out.i4_pic_type == IV_I_FRAME));

                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                    ->i4_is_non_I_scenecut =
                                    ((ps_curr_inp->s_lap_out.i4_scene_type ==
                                      SCENE_TYPE_SCENE_CUT) &&
                                     (ps_enc_ctxt->s_multi_thrd
                                          .ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                          ->i4_is_I_scenecut == 0));

                                /*ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]->i4_is_I_only_scd   = ps_curr_inp->s_lap_out.i4_is_I_only_scd;
                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]->i4_is_non_I_scd    = ps_curr_inp->s_lap_out.i4_is_non_I_scd;

                                ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]->i4_is_model_valid    = ps_curr_inp->s_lap_out.i4_is_model_valid;*/

                                /* -------------------------------------------- */
                                /*        MSE Computation for PSNR              */
                                /* -------------------------------------------- */
                                if(ps_enc_ctxt->ps_stat_prms->i4_log_dump_level != 0)
                                {
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_qp =
                                        ps_enc_ctxt->s_multi_thrd.cur_qp[i4_enc_frm_id][i];
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_poc = ps_curr_inp->s_lap_out.i4_poc;
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_pic_type = ps_curr_inp->s_lap_out.i4_pic_type;
                                }

                                /* if non reference B picture */
                                if(0 == ps_frm_recon_temp->i4_is_reference)
                                {
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i]
                                        ->i4_pic_type += 2;
                                }

#define FORCE_EXT_REF_PIC 0

                                /* -------------------------------------------- */
                                /*        Dumping of recon to App Queue         */
                                /* -------------------------------------------- */
                                if(1 == ps_enc_ctxt->ps_stat_prms->i4_save_recon)
                                {
                                    {
                                        WORD32 i, j;
                                        UWORD8 *pu1_recon;
                                        UWORD8 *pu1_chrm_buf_u;
                                        UWORD8 *pu1_chrm_buf_v;
                                        UWORD8 *pu1_curr_recon;

                                        pu1_recon =
                                            (UWORD8 *)ps_frm_recon_temp->s_yuv_buf_desc.pv_y_buf;

                                        /** Copying Luma into recon buffer  **/
                                        pu1_curr_recon = (UWORD8 *)ps_recon_out_temp->pv_y_buf;

                                        for(j = 0; j < ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht;
                                            j++)
                                        {
                                            memcpy(
                                                pu1_curr_recon,
                                                pu1_recon,
                                                ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd);

                                            pu1_recon +=
                                                ps_frm_recon_temp->s_yuv_buf_desc.i4_y_strd;
                                            pu1_curr_recon +=
                                                ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd;
                                        }

                                        /* recon chroma is converted from Semiplanar to Planar for dumping */
                                        pu1_recon =
                                            (UWORD8 *)ps_frm_recon_temp->s_yuv_buf_desc.pv_u_buf;
                                        pu1_chrm_buf_u = (UWORD8 *)ps_recon_out_temp->pv_cb_buf;
                                        pu1_chrm_buf_v =
                                            pu1_chrm_buf_u +
                                            ((ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd >> 1) *
                                             ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht);

                                        for(j = 0; j < ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht;
                                            j++)
                                        {
                                            for(i = 0;
                                                i<ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd>> 1;
                                                i++)
                                            {
                                                *pu1_chrm_buf_u++ = *pu1_recon++;
                                                *pu1_chrm_buf_v++ = *pu1_recon++;
                                            }

                                            pu1_recon -=
                                                ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd;
                                            pu1_recon +=
                                                ps_frm_recon_temp->s_yuv_buf_desc.i4_uv_strd;
                                        }

                                        /* set the POC and number of bytes in Y & UV buf */
                                        ps_recon_out_temp->i4_poc = ps_frm_recon_temp->i4_poc;
                                        ps_recon_out_temp->i4_y_pixels =
                                            ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht *
                                            ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd;
                                        ps_recon_out_temp->i4_uv_pixels =
                                            ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd *
                                            ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht;
                                    }
                                }
                                ps_frm_recon_temp->i4_non_ref_free_flag = 1;
                                /* -------------------------------------------- */
                                /*        End of picture updates                */
                                /* -------------------------------------------- */
                            }

                            /* After the MSE (or PSNR) computation is done we will update
                    these data in output buffer structure and then signal entropy
                    thread that the buffer is produced. */
                            if(ps_enc_ctxt->s_multi_thrd.ai4_produce_outbuf[i4_enc_frm_id][i] == 1)
                            {
                                /* set the output buffer as produced */
                                ihevce_q_set_buff_prod(
                                    (void *)ps_enc_ctxt,
                                    IHEVCE_FRM_PRS_ENT_COD_Q + i,
                                    ps_enc_ctxt->s_multi_thrd.out_buf_id[i4_enc_frm_id][i]);

                                ps_enc_ctxt->s_multi_thrd.is_out_buf_freed[i4_enc_frm_id][i] = 1;
                                ps_enc_ctxt->s_multi_thrd.ai4_produce_outbuf[i4_enc_frm_id][i] = 0;
                            }

                        }  //bit-rate counter ends
                        /* -------------------------------------------- */
                        /*        Frame level RC update                 */
                        /* -------------------------------------------- */
                        /* Query enc_loop to get the Parameters for Rate control */
                        if(1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag)
                        {
                            frm_proc_ent_cod_ctxt_t *ps_curr_out = NULL;
                            /*HEVC_RC*/
                            rc_bits_sad_t as_rc_frame_stat[IHEVCE_MAX_NUM_BITRATES];
                            osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

                            for(i = 0; i < i4_num_bitrates; i++)
                            {
                                /*each bit-rate RC params are collated by master thread */
                                ihevce_enc_loop_get_frame_rc_prms(
                                    ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt,
                                    &as_rc_frame_stat[i],
                                    i,
                                    i4_enc_frm_id);

                                /*update bits estimate on rd opt thread so that mismatch between rdopt and entropy can be taken care of*/
                                ps_curr_out =
                                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i];

                                ps_rc_lap_out_next_encode =
                                    (rc_lap_out_params_t *)
                                        ps_curr_inp->s_rc_lap_out.ps_rc_lap_out_next_encode;

                                ps_curr_out->i4_is_end_of_idr_gop = 0;

                                if(NULL != ps_rc_lap_out_next_encode)
                                {
                                    if(ps_rc_lap_out_next_encode->i4_rc_pic_type == IV_IDR_FRAME)
                                    {
                                        /*If the next pic is IDR, then signal end of gopf for current frame*/
                                        ps_curr_out->i4_is_end_of_idr_gop = 1;
                                    }
                                }
                                else if(NULL == ps_rc_lap_out_next_encode)
                                {
                                    /*If the lap out next is NULL, then end of sequence reached*/
                                    ps_curr_out->i4_is_end_of_idr_gop = 1;
                                }

                                if(NULL == ps_curr_out)
                                {
                                    DBG_PRINTF("error in getting curr out in encode loop\n");
                                }

                                //DBG_PRINTF("\nRDOPT head = %d RDOPT text = %d\n",s_rc_frame_stat.u4_total_header_bits,s_rc_frame_stat.u4_total_texture_bits);
                                /* acquire mutex lock for rate control calls */

                                /* Note : u4_total_intra_sad coming out of enc_loop */
                                /* will not be accurate becos of intra gating       */
                                /* need to access the importance of this sad in RC  */

                                //Store the rc update parameters for deterministic Enc loop parallelism

                                {
                                    ihevce_rc_store_retrive_update_info(
                                        (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
                                        &as_rc_frame_stat[i],
                                        i4_enc_frm_id_rc,
                                        i,
                                        1,
                                        &ps_enc_ctxt->s_multi_thrd.out_buf_id[i4_enc_frm_id][i],
                                        &ps_curr_inp->s_lap_out.i4_pic_type,
                                        &ai4_act_qp[i],
                                        (void *)&ps_curr_inp->s_lap_out,
                                        (void *)&ps_curr_inp->s_rc_lap_out);  // STORE
                                }
                            }

                            /* release mutex lock after rate control calls */
                            osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
                        }
                        if((ps_enc_ctxt->ps_stat_prms->i4_save_recon != 0) /*&&
                                                                   (1 == ps_curr_inp->s_input_buf.s_input_buf.i4_inp_frm_data_valid_flag)*/)
                        {
                            WORD32 i4_bitrate_ctr;
                            for(i4_bitrate_ctr = 0; i4_bitrate_ctr < i4_num_bitrates;
                                i4_bitrate_ctr++)
                            {
                                /*swaping of buf_id for 0th and reference bitrate location, as encoder
                        assumes always 0th loc for reference bitrate and app must receive in
                        the configured order*/
                                WORD32 i4_recon_buf_id = i4_bitrate_ctr;
                                if(i4_bitrate_ctr == 0)
                                {
                                    i4_recon_buf_id = ps_enc_ctxt->i4_ref_mbr_id;
                                }
                                else if(i4_bitrate_ctr == ps_enc_ctxt->i4_ref_mbr_id)
                                {
                                    i4_recon_buf_id = 0;
                                }

                                /* Call back to Apln. saying recon buffer is produced */
                                ps_hle_ctxt->ihevce_output_recon_fill_done(
                                    ps_hle_ctxt->pv_recon_cb_handle,
                                    ps_enc_ctxt->s_multi_thrd
                                        .ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr],
                                    i4_recon_buf_id, /* br instance */
                                    i4_resolution_id /* res_intance */);

                                /* --- release the current recon buffer ---- */
                                ihevce_q_rel_buf(
                                    (void *)ps_enc_ctxt,
                                    (IHEVCE_RECON_DATA_Q + i4_recon_buf_id),
                                    ps_enc_ctxt->s_multi_thrd
                                        .recon_buf_id[i4_enc_frm_id][i4_bitrate_ctr]);

                                ps_enc_ctxt->s_multi_thrd
                                    .is_recon_dumped[i4_enc_frm_id][i4_bitrate_ctr] = 1;
                            }
                        }

                        if(i4_enc_end_flag == 1)
                        {
                            if(ps_enc_ctxt->s_multi_thrd.is_in_buf_freed[i4_enc_frm_id] == 0)
                            {
                                /* release the pre_enc/enc queue buffer */
                                ihevce_q_rel_buf(
                                    (void *)ps_enc_ctxt,
                                    IHEVCE_PRE_ENC_ME_Q,
                                    ps_curr_inp_enc->curr_inp_from_me_buf_id);

                                ps_enc_ctxt->s_multi_thrd.is_in_buf_freed[i4_enc_frm_id] = 1;
                            }
                        }
                        /* release encoder owned input buffer*/
                        ihevce_q_rel_buf(
                            (void *)ps_enc_ctxt,
                            IHEVCE_INPUT_DATA_CTRL_Q,
                            ps_curr_inp_enc->curr_inp_buf_id);
                        /* release the pre_enc/enc queue buffer */
                        ihevce_q_rel_buf(
                            ps_enc_ctxt,
                            IHEVCE_PRE_ENC_ME_Q,
                            ps_curr_inp_enc->curr_inp_from_me_buf_id);

                        ps_enc_ctxt->s_multi_thrd.is_in_buf_freed[i4_enc_frm_id] = 1;

                        /* release the pre_enc/enc queue buffer */
                        ihevce_q_rel_buf(
                            ps_enc_ctxt,
                            IHEVCE_L0_IPE_ENC_Q,
                            ps_curr_inp_enc->curr_inp_from_l0_ipe_buf_id);

                        ps_enc_ctxt->s_multi_thrd.is_L0_ipe_in_buf_freed[i4_enc_frm_id] = 1;
                        /* release the me/enc queue buffer */
                        ihevce_q_rel_buf(
                            ps_enc_ctxt,
                            IHEVCE_ME_ENC_RDOPT_Q,
                            ps_enc_ctxt->s_multi_thrd.i4_enc_in_buf_id[i4_enc_frm_id]);

                        /* reset the pointers to NULL */
                        ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] = NULL;
                        ps_enc_ctxt->s_multi_thrd.enc_master_done_frame_init[i4_enc_frm_id] = 0;
                        for(i = 0; i < i4_num_bitrates; i++)
                            ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i] = NULL;

                        /* Set the prev_frame_done variable to 1 to indicate that
                *prev frame is done */
                        ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_done);
                    }
                }
            }
            else
            {
                /* Increment the counter to keep track of no of threads exiting the current mutex*/
                ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id]++;
                /*Last slave thread comming out of enc loop will execute next critical section*/
                if(ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id] ==
                   ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds)
                {
                    ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_enc_frm_id] = 0;

                    /* reset the pointers to NULL */
                    ps_enc_ctxt->s_multi_thrd.aps_cur_inp_enc_prms[i4_enc_frm_id] = NULL;

                    ps_enc_ctxt->s_multi_thrd.enc_master_done_frame_init[i4_enc_frm_id] = 0;

                    for(i = 0; i < i4_num_bitrates; i++)
                        ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_enc_frm_id][i] = NULL;

                    /* Set the prev_frame_done variable to 1 to indicate that
                        *prev frame is done
                        */
                    ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_done);
                }
            }

            /* Toggle the ping pong flag of the thread exiting curr frame*/
            /*ps_enc_ctxt->s_multi_thrd.ping_pong[ps_thrd_ctxt->i4_thrd_id] =
                !ps_enc_ctxt->s_multi_thrd.ping_pong[ps_thrd_ctxt->i4_thrd_id];*/
        }

        /************************************/
        /******  EXIT CRITICAL SECTION ******/
        /************************************/
        /****** Unlock the critical section ******/
        if(NULL != ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id])
        {
            result = osal_mutex_unlock(
                ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id]);
            if(OSAL_SUCCESS != result)
                return 0;
        }

        if((0 == i4_me_end_flag) && (0 == i4_enc_end_flag))
        {
            i4_enc_frm_id++;
            i4_enc_frm_id_rc++;

            if(i4_enc_frm_id == NUM_ME_ENC_BUFS)
            {
                i4_enc_frm_id = 0;
            }

            if(i4_enc_frm_id_rc == ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc)
            {
                i4_enc_frm_id_rc = 0;
            }
            i4_me_frm_id++;

            if(i4_me_frm_id == NUM_ME_ENC_BUFS)
                i4_me_frm_id = 0;
        }
        if(1 == ps_enc_ctxt->s_multi_thrd.i4_force_end_flag)
        {
            i4_me_end_flag = 1;
            i4_enc_end_flag = 1;
        }
    }

    /****** Lock the critical section ******/

    if(NULL != ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id])
    {
        WORD32 result;

        result =
            osal_mutex_lock(ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id]);

        if(OSAL_SUCCESS != result)
            return 0;
    }

    if(ps_enc_ctxt->s_multi_thrd.num_thrds_done ==
       (ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds - 1))
    {
        if(1 != ps_enc_ctxt->s_multi_thrd.i4_force_end_flag)
        {
            osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
            for(i = 0; i < ps_enc_ctxt->i4_num_bitrates; i++)
            {
                ihevce_rc_close(
                    ps_enc_ctxt,
                    ps_enc_ctxt->i4_active_enc_frame_id,
                    2,
                    MIN(ps_enc_ctxt->ai4_rc_query[i], ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc),
                    i);
            }
            osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
        }
    }

    ps_enc_ctxt->s_multi_thrd.num_thrds_done++;

    /****** UnLock the critical section ******/
    if(NULL != ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id])
    {
        WORD32 result;

        result =
            osal_mutex_unlock(ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id]);

        if(OSAL_SUCCESS != result)
            return 0;
    }

    /****** Lock the critical section ******/
    if(NULL != ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id])
    {
        WORD32 result;
        result =
            osal_mutex_lock(ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id]);

        if(OSAL_SUCCESS != result)
            return 0;
    }
    if((ps_enc_ctxt->s_multi_thrd.num_thrds_done ==
        ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds) &&
       (ps_enc_ctxt->s_multi_thrd.i4_force_end_flag))
    {
        WORD32 num_bufs_preenc_me_que, num_bufs_L0_ipe_enc;
        WORD32 buf_id_ctr, frm_id_ctr;
        frm_proc_ent_cod_ctxt_t *ps_curr_out_enc_ent[IHEVCE_MAX_NUM_BITRATES];
        WORD32 out_buf_id_enc_ent[IHEVCE_MAX_NUM_BITRATES];

        if(ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel > 1)
        {
            num_bufs_preenc_me_que = (MAX_L0_IPE_ENC_STAGGER - 1) + MIN_L1_L0_STAGGER_NON_SEQ +
                                     NUM_BUFS_DECOMP_HME +
                                     ps_enc_ctxt->ps_stat_prms->s_lap_prms.i4_rc_look_ahead_pics;

            num_bufs_L0_ipe_enc = MAX_L0_IPE_ENC_STAGGER;
        }
        else
        {
            num_bufs_preenc_me_que = (MIN_L0_IPE_ENC_STAGGER - 1) + MIN_L1_L0_STAGGER_NON_SEQ +
                                     NUM_BUFS_DECOMP_HME +
                                     ps_enc_ctxt->ps_stat_prms->s_lap_prms.i4_rc_look_ahead_pics;

            num_bufs_L0_ipe_enc = MIN_L0_IPE_ENC_STAGGER;
        }
        for(buf_id_ctr = 0; buf_id_ctr < num_bufs_preenc_me_que; buf_id_ctr++)
        {
            /* release encoder owned input buffer*/
            ihevce_q_rel_buf((void *)ps_enc_ctxt, IHEVCE_PRE_ENC_ME_Q, buf_id_ctr);
        }
        for(buf_id_ctr = 0; buf_id_ctr < num_bufs_L0_ipe_enc; buf_id_ctr++)
        {
            /* release encoder owned input buffer*/
            ihevce_q_rel_buf((void *)ps_enc_ctxt, IHEVCE_L0_IPE_ENC_Q, buf_id_ctr);
        }
        for(frm_id_ctr = 0; frm_id_ctr < NUM_ME_ENC_BUFS; frm_id_ctr++)
        {
            for(i = 0; i < ps_enc_ctxt->i4_num_bitrates; i++)
            {
                if(NULL != ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[frm_id_ctr][i])
                {
                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[frm_id_ctr][i]
                        ->i4_frm_proc_valid_flag = 0;
                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[frm_id_ctr][i]->i4_end_flag = 1;
                    /* set the output buffer as produced */
                    ihevce_q_set_buff_prod(
                        (void *)ps_enc_ctxt,
                        IHEVCE_FRM_PRS_ENT_COD_Q + i,
                        ps_enc_ctxt->s_multi_thrd.out_buf_id[frm_id_ctr][i]);
                }
            }
        }
        for(buf_id_ctr = 0; buf_id_ctr < NUM_FRMPROC_ENTCOD_BUFS;
            buf_id_ctr++) /*** Set buffer produced for NUM_FRMPROC_ENTCOD_BUFS buffers for entropy to exit ***/
        {
            for(i = 0; i < ps_enc_ctxt->i4_num_bitrates; i++)
            {
                ps_curr_out_enc_ent[i] = (frm_proc_ent_cod_ctxt_t *)ihevce_q_get_free_buff(
                    (void *)ps_enc_ctxt,
                    IHEVCE_FRM_PRS_ENT_COD_Q + i, /*decides the buffer queue */
                    &out_buf_id_enc_ent[i],
                    BUFF_QUE_NON_BLOCKING_MODE);
                if(NULL != ps_curr_out_enc_ent[i])
                {
                    ps_curr_out_enc_ent[i]->i4_frm_proc_valid_flag = 0;
                    ps_curr_out_enc_ent[i]->i4_end_flag = 1;
                    /* set the output buffer as produced */
                    ihevce_q_set_buff_prod(
                        (void *)ps_enc_ctxt, IHEVCE_FRM_PRS_ENT_COD_Q + i, out_buf_id_enc_ent[i]);
                }
            }
        }
    }

    /* The last thread coming out of Enc. Proc. */
    /* Release all the Recon buffers the application might have queued in */
    if((ps_enc_ctxt->s_multi_thrd.num_thrds_done ==
        ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds) &&
       (ps_enc_ctxt->ps_stat_prms->i4_save_recon != 0) &&
       (ps_enc_ctxt->s_multi_thrd.i4_is_recon_free_done == 0))
    {
        WORD32 i4_bitrate_ctr;

        for(i4_bitrate_ctr = 0; i4_bitrate_ctr < i4_num_bitrates; i4_bitrate_ctr++)
        {
            WORD32 end_flag = 0;
            while(0 == end_flag)
            {
                /*swaping of buf_id for 0th and reference bitrate location, as encoder
                assumes always 0th loc for reference bitrate and app must receive in
                the configured order*/
                WORD32 i4_recon_buf_id = i4_bitrate_ctr;
                if(i4_bitrate_ctr == 0)
                {
                    i4_recon_buf_id = ps_enc_ctxt->i4_ref_mbr_id;
                }
                else if(i4_bitrate_ctr == ps_enc_ctxt->i4_ref_mbr_id)
                {
                    i4_recon_buf_id = 0;
                }

                /* ------- get free Recon buffer from Frame buffer que ---------- */
                /* There is a separate queue for each bit-rate instnace. The recon
                buffer is acquired from the corresponding queue based on the
                bitrate instnace */
                ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr] =
                    (iv_enc_recon_data_buffs_t *)ihevce_q_get_filled_buff(
                        (void *)ps_enc_ctxt,
                        IHEVCE_RECON_DATA_Q + i4_recon_buf_id, /*decides the buffer queue */
                        &ps_enc_ctxt->s_multi_thrd.recon_buf_id[i4_enc_frm_id][i4_bitrate_ctr],
                        BUFF_QUE_BLOCKING_MODE);

                /* Update the end_flag from application */
                end_flag = ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr]
                               ->i4_is_last_buf;

                ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_end_flag =
                    1;
                ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_y_pixels =
                    0;
                ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr]->i4_uv_pixels =
                    0;

                /* Call back to Apln. saying recon buffer is produced */
                ps_hle_ctxt->ihevce_output_recon_fill_done(
                    ps_hle_ctxt->pv_recon_cb_handle,
                    ps_enc_ctxt->s_multi_thrd.ps_recon_out[i4_enc_frm_id][i4_bitrate_ctr],
                    i4_recon_buf_id, /* br instance */
                    i4_resolution_id /* res_intance */);

                /* --- release the current recon buffer ---- */
                ihevce_q_rel_buf(
                    (void *)ps_enc_ctxt,
                    (IHEVCE_RECON_DATA_Q + i4_recon_buf_id),
                    ps_enc_ctxt->s_multi_thrd.recon_buf_id[i4_enc_frm_id][i4_bitrate_ctr]);
            }
        }
        /* Set the recon free done flag */
        ps_enc_ctxt->s_multi_thrd.i4_is_recon_free_done = 1;
    }

    /****** UnLock the critical section ******/
    if(NULL != ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id])
    {
        WORD32 result;
        result =
            osal_mutex_unlock(ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i4_enc_frm_id]);

        if(OSAL_SUCCESS != result)
            return 0;
    }

    return (0);
}

/*!
******************************************************************************
* \if Function name : ihevce_set_pre_enc_prms \endif
*
* \brief
*    Set CTB parameters
*    Set ME params
*    Set pps, sps, vps, vui params
*    Do RC init
*
* \param[in] Encoder context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_set_pre_enc_prms(enc_ctxt_t *ps_enc_ctxt)
{
    WORD32 i;
    WORD32 i4_num_instance,
        i4_resolution_id = ps_enc_ctxt->i4_resolution_id;  //number of bit-rate instances

    i4_num_instance = ps_enc_ctxt->i4_num_bitrates;

#if PIC_ALIGN_CTB_SIZE

    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width,
            ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

    ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz =
        ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd / ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size;

    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height,
            ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

    ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert =
        ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht / ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size;
#else  // PIC_ALIGN_CTB_SIZE
    /* Allign the frame width to min CU size */
    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width,
            ps_enc_ctxt->s_frm_ctb_prms.i4_min_cu_size);

    ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz =
        ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd / ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size;

    if((ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd %
        ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size) != 0)
        ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz =
            ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz + 1;

    /* Allign the frame hieght to min CU size */
    ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height,
            ps_enc_ctxt->s_frm_ctb_prms.i4_min_cu_size);

    ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert =
        ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht / ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size;

    if((ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht %
        ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size) != 0)
        ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert =
            ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert + 1;

#endif  // PIC_ALIGN_CTB_SIZE

    ps_enc_ctxt->s_frm_ctb_prms.i4_max_cus_in_row = ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz *
                                                    ps_enc_ctxt->s_frm_ctb_prms.i4_num_cus_in_ctb;

    ps_enc_ctxt->s_frm_ctb_prms.i4_max_pus_in_row = ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz *
                                                    ps_enc_ctxt->s_frm_ctb_prms.i4_num_pus_in_ctb;

    ps_enc_ctxt->s_frm_ctb_prms.i4_max_tus_in_row = ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz *
                                                    ps_enc_ctxt->s_frm_ctb_prms.i4_num_tus_in_ctb;
    ihevce_coarse_me_set_resolution(
        ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
        1,
        &ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd,
        &ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht);

    /*if Resolution need to be changed dynamically then needs to go to encode group */
    ihevce_me_set_resolution(
        ps_enc_ctxt->s_module_ctxt.pv_me_ctxt,
        1,
        &ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd,
        &ps_enc_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht);
    i4_num_instance = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                          .i4_num_bitrate_instances;
    for(i = 0; i < i4_num_instance; i++)
    {
        WORD32 i4_id;
        /*swaping of buf_id for 0th and reference bitrate location, as encoder
        assumes always 0th loc for reference bitrate and app must receive in
        the configured order*/
        if(i == 0)
        {
            i4_id = ps_enc_ctxt->i4_ref_mbr_id;
        }
        else if(i == ps_enc_ctxt->i4_ref_mbr_id)
        {
            i4_id = 0;
        }
        else
        {
            i4_id = i;
        }
        /* populate vps based on encoder configuration and tools */
        ihevce_populate_vps(
            ps_enc_ctxt,
            &ps_enc_ctxt->as_vps[i],
            &ps_enc_ctxt->s_runtime_src_prms,
            &ps_enc_ctxt->ps_stat_prms->s_out_strm_prms,
            &ps_enc_ctxt->s_runtime_coding_prms,
            &ps_enc_ctxt->ps_stat_prms->s_config_prms,
            ps_enc_ctxt->ps_stat_prms,
            i4_resolution_id);

        /* populate sps based on encoder configuration and tools */
        ihevce_populate_sps(
            ps_enc_ctxt,
            &ps_enc_ctxt->as_sps[i],
            &ps_enc_ctxt->as_vps[i],
            &ps_enc_ctxt->s_runtime_src_prms,
            &ps_enc_ctxt->ps_stat_prms->s_out_strm_prms,
            &ps_enc_ctxt->s_runtime_coding_prms,
            &ps_enc_ctxt->ps_stat_prms->s_config_prms,
            &ps_enc_ctxt->s_frm_ctb_prms,
            ps_enc_ctxt->ps_stat_prms,
            i4_resolution_id);

        /* populate pps based on encoder configuration and tools */
        ihevce_populate_pps(
            &ps_enc_ctxt->as_pps[i],
            &ps_enc_ctxt->as_sps[i],
            &ps_enc_ctxt->s_runtime_src_prms,
            &ps_enc_ctxt->ps_stat_prms->s_out_strm_prms,
            &ps_enc_ctxt->s_runtime_coding_prms,
            &ps_enc_ctxt->ps_stat_prms->s_config_prms,
            ps_enc_ctxt->ps_stat_prms,
            i4_id,
            i4_resolution_id,
            ps_enc_ctxt->ps_tile_params_base,
            &ps_enc_ctxt->ai4_column_width_array[0],
            &ps_enc_ctxt->ai4_row_height_array[0]);

        // if(ps_enc_ctxt->as_sps[i].i1_vui_parameters_present_flag == 1)
        {
            ihevce_populate_vui(
                &ps_enc_ctxt->as_sps[i].s_vui_parameters,
                &ps_enc_ctxt->as_sps[i],
                &ps_enc_ctxt->s_runtime_src_prms,
                &ps_enc_ctxt->ps_stat_prms->s_vui_sei_prms,
                i4_resolution_id,
                &ps_enc_ctxt->s_runtime_tgt_params,
                ps_enc_ctxt->ps_stat_prms,
                i4_id);
        }
    }

    osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
    /* run the loop over all bit-rate instnaces */
    for(i = 0; i < i4_num_instance; i++)
    {
        /*HEVC_RC Do one time initialization of rate control*/
        ihevce_rc_init(
            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
            &ps_enc_ctxt->s_runtime_src_prms,
            &ps_enc_ctxt->s_runtime_tgt_params,
            &ps_enc_ctxt->s_rc_quant,
            &ps_enc_ctxt->ps_stat_prms->s_sys_api,
            &ps_enc_ctxt->ps_stat_prms->s_lap_prms,
            ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc);

        ihevce_vbv_complaince_init_level(
            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i],
            &ps_enc_ctxt->as_sps[i].s_vui_parameters);
    }
    osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
}

/*!
******************************************************************************
* \if Function name : ihevce_pre_enc_init \endif
*
* \brief
*   set out_buf params
*   Calculate end_flag if flushmode on
*   Slice initialization
*   Populate SIE params
*   reference list creation
*
* \param[in] Encoder context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_pre_enc_init(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    pre_enc_me_ctxt_t *ps_curr_out,
    WORD32 *pi4_end_flag_ret,
    WORD32 *pi4_cur_qp_ret,
    WORD32 *pi4_decomp_lyr_idx,
    WORD32 i4_ping_pong)
{
    WORD32 end_flag = 0;
    WORD32 cur_qp;
    //recon_pic_buf_t *ps_frm_recon;
    WORD32 first_field = 1;
    WORD32 i4_field_pic = ps_enc_ctxt->s_runtime_src_prms.i4_field_pic;
    WORD32 i4_decomp_lyrs_idx = 0;
    WORD32 i4_resolution_id = ps_enc_ctxt->i4_resolution_id;
    WORD32 slice_type = ISLICE;
    WORD32 nal_type;
    WORD32 min_cu_size;

    WORD32 stasino_enabled;

    /* copy the time stamps from inp to entropy inp */
    ps_curr_out->i4_inp_timestamp_low = ps_curr_inp->s_input_buf.i4_inp_timestamp_low;
    ps_curr_out->i4_inp_timestamp_high = ps_curr_inp->s_input_buf.i4_inp_timestamp_high;
    ps_curr_out->pv_app_frm_ctxt = ps_curr_inp->s_input_buf.pv_app_frm_ctxt;

    /* get the min cu size from config params */
    min_cu_size = ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_min_log2_cu_size;

    min_cu_size = 1 << min_cu_size;

    ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd =
        ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd +
        SET_CTB_ALIGN(ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd, min_cu_size);

    ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht =
        ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht +
        SET_CTB_ALIGN(ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht, min_cu_size);

    ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd =
        ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd +
        SET_CTB_ALIGN(ps_curr_inp->s_lap_out.s_input_buf.i4_uv_wd, min_cu_size);

    if(IV_YUV_420SP_UV == ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format)
    {
        ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht =
            ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht +
            SET_CTB_ALIGN(ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht, (min_cu_size >> 1));
    }
    else if(IV_YUV_422SP_UV == ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format)
    {
        ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht =
            ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht +
            SET_CTB_ALIGN(ps_curr_inp->s_lap_out.s_input_buf.i4_uv_ht, min_cu_size);
    }

    /* update the END flag from LAP out */
    end_flag = ps_curr_inp->s_lap_out.i4_end_flag;
    ps_curr_out->i4_end_flag = end_flag;
    ps_enc_ctxt->s_multi_thrd.i4_last_pic_flag = end_flag;

    /* ----------------------------------------------------------------------*/
    /*  Slice initialization for current frame; Required for entropy context */
    /* ----------------------------------------------------------------------*/
    {
        WORD32 cur_poc = ps_curr_inp->s_lap_out.i4_poc;

        /* max merge candidates derived based on quality preset for now */
        WORD32 max_merge_candidates = 2;

        /* pocs less than random acess poc tagged for discard as they */
        /* could be refering to pics before the cra.                  */

        /* CRA case: as the leading pictures can refer the picture precedes the associated
        IRAP(CRA) in decoding order, hence make it Random access skipped leading pictures (RASL)*/

        if((1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_enable_temporal_scalability) &&
           (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_temporal_layers ==
            ps_curr_inp->s_lap_out.i4_temporal_lyr_id))  //TEMPORALA_SCALABILITY CHANGES
        {
            if(ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc)
            {
                nal_type = (cur_poc < ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc)
                               ? (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_RASL_R : NAL_RASL_N)
                               : (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_TSA_R : NAL_TSA_N);
            }
            /* IDR case: as the leading pictures can't refer the picture precedes the associated
            IRAP(IDR) in decoding order, hence make it Random access decodable leading pictures (RADL)*/
            else
            {
                nal_type = (cur_poc < ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc)
                               ? (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_RADL_R : NAL_RADL_N)
                               : (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_TSA_R : NAL_TSA_N);
            }
        }
        else
        {
            if(ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc)
            {
                nal_type = (cur_poc < ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc)
                               ? (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_RASL_R : NAL_RASL_N)
                               : (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_TRAIL_R : NAL_TRAIL_N);
            }
            /* IDR case: as the leading pictures can't refer the picture precedes the associated
            IRAP(IDR) in decoding order, hence make it Random access decodable leading pictures (RADL)*/
            else
            {
                nal_type = (cur_poc < ps_curr_inp->s_lap_out.i4_assoc_IRAP_poc)
                               ? (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_RADL_R : NAL_RADL_N)
                               : (ps_curr_inp->s_lap_out.i4_is_ref_pic ? NAL_TRAIL_R : NAL_TRAIL_N);
            }
        }

        switch(ps_curr_inp->s_lap_out.i4_pic_type)
        {
        case IV_IDR_FRAME:
            /*  IDR pic */
            slice_type = ISLICE;
            nal_type = NAL_IDR_W_LP;
            cur_poc = 0;
            ps_enc_ctxt->i4_cra_poc = cur_poc;
            break;

        case IV_I_FRAME:
            slice_type = ISLICE;

            if(ps_curr_inp->s_lap_out.i4_is_cra_pic)
            {
                nal_type = NAL_CRA;
            }

            ps_enc_ctxt->i4_cra_poc = cur_poc;
            break;

        case IV_P_FRAME:
            slice_type = PSLICE;
            break;

        case IV_B_FRAME:
            /* TODO : Mark the nal type as NAL_TRAIL_N for non ref pics */
            slice_type = BSLICE;
            break;

        default:
            /* This should never occur */
            ASSERT(0);
        }

        /* number of merge candidates and error metric chosen based on quality preset */
        switch(ps_curr_inp->s_lap_out.i4_quality_preset)
        {
        case IHEVCE_QUALITY_P0:
            max_merge_candidates = 5;
            break;

        case IHEVCE_QUALITY_P2:
            max_merge_candidates = 5;
            break;

        case IHEVCE_QUALITY_P3:
            max_merge_candidates = 3;
            break;

        case IHEVCE_QUALITY_P4:
        case IHEVCE_QUALITY_P5:
        case IHEVCE_QUALITY_P6:
            max_merge_candidates = 2;
            break;

        default:
            ASSERT(0);
        }

        /* acquire mutex lock for rate control calls */
        osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
        {
            ps_curr_inp->s_rc_lap_out.i4_num_pels_in_frame_considered =
                ps_curr_inp->s_lap_out.s_input_buf.i4_y_ht *
                ps_curr_inp->s_lap_out.s_input_buf.i4_y_wd;

            /*initialize the frame info stat inside LAP out, Data inside this will be populated in ihevce_rc_get_bpp_based_frame_qp call*/
            ps_curr_inp->s_rc_lap_out.ps_frame_info = &ps_curr_inp->s_frame_info;

            ps_curr_inp->s_rc_lap_out.i4_is_bottom_field = ps_curr_inp->s_input_buf.i4_bottom_field;
            if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode == 3)
            {
                /*for constant qp use same qp*/
                /*HEVC_RC query rate control for qp*/
                cur_qp = ihevce_rc_pre_enc_qp_query(
                    (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                    &ps_curr_inp->s_rc_lap_out,
                    0);
            }
            else
            {
                cur_qp = ihevce_rc_get_bpp_based_frame_qp(
                    (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0], &ps_curr_inp->s_rc_lap_out);
            }
        }
        /* release mutex lock after rate control calls */
        osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

        /* store the QP in output prms */
        /* The same qp is also used in enc thread only for ME*/
        ps_curr_out->i4_curr_frm_qp = cur_qp;

        /* slice header entropy syn memory is not valid in pre encode stage */
        ps_curr_out->s_slice_hdr.pu4_entry_point_offset = NULL;

        /* derive the flag which indicates if stasino is enabled */
        stasino_enabled = (ps_enc_ctxt->s_runtime_coding_prms.i4_vqet &
                           (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)) &&
                          (ps_enc_ctxt->s_runtime_coding_prms.i4_vqet &
                           (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER));

        /* initialize the slice header */
        ihevce_populate_slice_header(
            &ps_curr_out->s_slice_hdr,
            &ps_enc_ctxt->as_pps[0],
            &ps_enc_ctxt->as_sps[0],
            nal_type,
            slice_type,
            0,
            0,
            ps_curr_inp->s_lap_out.i4_poc,
            cur_qp,
            max_merge_candidates,
            ps_enc_ctxt->ps_stat_prms->s_pass_prms.i4_pass,
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                .i4_quality_preset,
            stasino_enabled);

        ps_curr_out->i4_slice_nal_type = nal_type;

        ps_curr_out->s_slice_hdr.u4_nuh_temporal_id = 0;

        if(1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_enable_temporal_scalability)
        {
            ps_curr_out->s_slice_hdr.u4_nuh_temporal_id =
                (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_temporal_layers ==
                 ps_curr_inp->s_lap_out.i4_temporal_lyr_id);  //TEMPORALA_SCALABILITY CHANGES
        }

        /* populate sps, vps and pps pointers for the entropy input params */
        ps_curr_out->ps_pps = &ps_enc_ctxt->as_pps[0];
        ps_curr_out->ps_sps = &ps_enc_ctxt->as_sps[0];
        ps_curr_out->ps_vps = &ps_enc_ctxt->as_vps[0];
    }

    /* By default, Sei messages are set to 0, to avoid unintialised memory access */
    memset(&ps_curr_out->s_sei, 0, sizeof(sei_params_t));

    /* VUI, SEI flags reset */
    ps_curr_out->s_sei.i1_sei_parameters_present_flag = 0;
    ps_curr_out->s_sei.i1_buf_period_params_present_flag = 0;
    ps_curr_out->s_sei.i1_pic_timing_params_present_flag = 0;
    ps_curr_out->s_sei.i1_recovery_point_params_present_flag = 0;
    ps_curr_out->s_sei.i1_decoded_pic_hash_sei_flag = 0;
    ps_curr_out->s_sei.i4_sei_mastering_disp_colour_vol_params_present_flags = 0;

    if(ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_enable_flag == 1)
    {
        /* insert buffering period, display volume, recovery point only at irap points */
        WORD32 insert_per_irap =
            ((slice_type == ISLICE) &&
             (((NAL_IDR_N_LP == nal_type) || (NAL_CRA == nal_type)) || (NAL_IDR_W_LP == nal_type)));

        ps_curr_out->s_sei.i1_sei_parameters_present_flag = 1;

        /* populate Sei buffering period based on encoder configuration and tools */
        if(ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_buffer_period_flags == 1)
        {
            ihevce_populate_buffering_period_sei(
                &ps_curr_out->s_sei,
                &ps_enc_ctxt->as_sps[0].s_vui_parameters,
                &ps_enc_ctxt->as_sps[0],
                &ps_enc_ctxt->ps_stat_prms->s_vui_sei_prms);

            ps_curr_out->s_sei.i1_buf_period_params_present_flag = insert_per_irap;

            ihevce_populate_active_parameter_set_sei(
                &ps_curr_out->s_sei, &ps_enc_ctxt->as_vps[0], &ps_enc_ctxt->as_sps[0]);
        }

        /* populate Sei picture timing based on encoder configuration and tools */
        if(ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_pic_timing_flags == 1)
        {
            ihevce_populate_picture_timing_sei(
                &ps_curr_out->s_sei,
                &ps_enc_ctxt->as_sps[0].s_vui_parameters,
                &ps_enc_ctxt->s_runtime_src_prms,
                ps_curr_inp->s_input_buf.i4_bottom_field);
            ps_curr_out->s_sei.i1_pic_timing_params_present_flag = 1;
        }

        /* populate Sei recovery point based on encoder configuration and tools */
        if(ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_recovery_point_flags == 1)
        {
            ihevce_populate_recovery_point_sei(
                &ps_curr_out->s_sei, &ps_enc_ctxt->ps_stat_prms->s_vui_sei_prms);
            ps_curr_out->s_sei.i1_recovery_point_params_present_flag = insert_per_irap;
        }

        /* populate mastering_display_colour_volume parameters */
        if(ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags == 1)
        {
            ihevce_populate_mastering_disp_col_vol_sei(
                &ps_curr_out->s_sei, &ps_enc_ctxt->ps_stat_prms->s_out_strm_prms);

            ps_curr_out->s_sei.i4_sei_mastering_disp_colour_vol_params_present_flags =
                insert_per_irap;
        }

        /* populate SEI Hash Flag based on encoder configuration */
        if(0 != ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag)
        {
            /* Sanity checks */
            ASSERT(0 != ps_enc_ctxt->as_sps[0].i1_chroma_format_idc);

            ASSERT(
                (0 < ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag) &&
                (4 > ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag));

            /* MD5 is not supported now! picture_md5[cIdx][i] pblm */
            ASSERT(1 != ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag);

            ps_curr_out->s_sei.i1_decoded_pic_hash_sei_flag =
                ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag;
        }
    }

    /* For interlace pictures, first_field depends on topfield_first and bottom field */
    if(i4_field_pic)
    {
        first_field =
            (ps_curr_inp->s_input_buf.i4_topfield_first ^ ps_curr_inp->s_input_buf.i4_bottom_field);
    }

    /* get frame level lambda params */
    ihevce_get_frame_lambda_prms(
        ps_enc_ctxt,
        ps_curr_out,
        cur_qp,
        first_field,
        ps_curr_inp->s_lap_out.i4_is_ref_pic,
        ps_curr_inp->s_lap_out.i4_temporal_lyr_id,
        lamda_modifier_for_I_pic[4] /*mean TRF*/,
        0,
        PRE_ENC_LAMBDA_TYPE);
    /* Coarse ME and Decomp buffers sharing */
    {
        UWORD8 *apu1_lyr_bufs[MAX_NUM_HME_LAYERS];
        WORD32 ai4_lyr_buf_strd[MAX_NUM_HME_LAYERS];

        /* get the Decomposition frame buffer from ME */
        i4_decomp_lyrs_idx = ihevce_coarse_me_get_lyr_buf_desc(
            ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt, &apu1_lyr_bufs[0], &ai4_lyr_buf_strd[0]);
        /* register the buffers with decomp module along with frame init */
        ihevce_decomp_pre_intra_frame_init(
            ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt,
            &apu1_lyr_bufs[0],
            &ai4_lyr_buf_strd[0],
            ps_curr_out->ps_layer1_buf,
            ps_curr_out->ps_layer2_buf,
            ps_curr_out->ps_ed_ctb_l1,
            ps_curr_out->as_lambda_prms[0].i4_ol_sad_lambda_qf,
            ps_curr_out->ps_ctb_analyse);
    }

    /* -------------------------------------------------------- */
    /*   Preparing Pre encode Passes Job Queue                  */
    /* -------------------------------------------------------- */
    ihevce_prepare_pre_enc_job_queue(ps_enc_ctxt, ps_curr_inp, i4_ping_pong);

    /*assign return variables */
    *pi4_end_flag_ret = end_flag;
    *pi4_cur_qp_ret = cur_qp;
    *pi4_decomp_lyr_idx = i4_decomp_lyrs_idx;
    //*pps_frm_recon_ret = ps_frm_recon;
}

/*!
******************************************************************************
* \if Function name : ihevce_pre_enc_coarse_me_init \endif
*
* \brief
*   set out_buf params
*   Calculate end_flag if flushmode on
*   Slice initialization
*   Populate SIE params
*   reference list creation
*
* \param[in] Encoder context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_pre_enc_coarse_me_init(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    pre_enc_me_ctxt_t *ps_curr_out,
    recon_pic_buf_t **pps_frm_recon_ret,
    WORD32 i4_decomp_lyrs_idx,
    WORD32 i4_cur_qp,
    WORD32 i4_ping_pong)

{
    /* local variables */
    recon_pic_buf_t *ps_frm_recon;
    coarse_me_master_ctxt_t *ps_ctxt = NULL;
    ps_ctxt = (coarse_me_master_ctxt_t *)ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt;
    /* Reference buffer management and reference list creation for pre enc group */
    ihevce_pre_enc_manage_ref_pics(ps_enc_ctxt, ps_curr_inp, ps_curr_out, i4_ping_pong);

    /* get a free recon buffer for current picture  */
    {
        WORD32 ctr;

        ps_frm_recon = NULL;
        for(ctr = 0; ctr < ps_enc_ctxt->i4_pre_enc_num_buf_recon_q; ctr++)
        {
            if(1 == ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr]->i4_is_free)
            {
                ps_frm_recon = ps_enc_ctxt->pps_pre_enc_recon_buf_q[ctr];
                break;
            }
        }
    }
    /* should not be NULL */
    ASSERT(ps_frm_recon != NULL);

    /* populate reference /recon params based on LAP output */
    ps_frm_recon->i4_is_free = 0;
    /* top first field is set to 1 by application */
    ps_frm_recon->i4_topfield_first = ps_curr_inp->s_input_buf.i4_topfield_first;
    ps_frm_recon->i4_poc = ps_curr_inp->s_lap_out.i4_poc;
    ps_frm_recon->i4_pic_type = ps_curr_inp->s_lap_out.i4_pic_type;
    ps_frm_recon->i4_display_num = ps_curr_inp->s_lap_out.i4_display_num;
    /* bottom field is toggled for every field by application */
    ps_frm_recon->i4_bottom_field = ps_curr_inp->s_input_buf.i4_bottom_field;

    /* Reference picture property is given by LAP */
    ps_frm_recon->i4_is_reference = ps_curr_inp->s_lap_out.i4_is_ref_pic;

    /* Deblock a picture for all reference frames unconditionally. */
    /* Deblock non ref if psnr compute or save recon is enabled    */
    ps_frm_recon->i4_deblk_pad_hpel_cur_pic = ps_frm_recon->i4_is_reference ||
                                              (ps_enc_ctxt->ps_stat_prms->i4_save_recon);

    /* set the width, height and stride to defalut values */
    ps_frm_recon->s_yuv_buf_desc.i4_y_ht = 0;
    ps_frm_recon->s_yuv_buf_desc.i4_uv_ht = 0;
    ps_frm_recon->s_yuv_buf_desc.i4_y_wd = 0;
    ps_frm_recon->s_yuv_buf_desc.i4_uv_wd = 0;
    ps_frm_recon->s_yuv_buf_desc.i4_y_strd = 0;
    ps_frm_recon->s_yuv_buf_desc.i4_uv_strd = 0;

    /* register the Layer1 MV bank pointer with ME module */
    ihevce_coarse_me_set_lyr1_mv_bank(
        ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
        ps_curr_inp,
        ps_curr_out->pv_me_mv_bank,
        ps_curr_out->pv_me_ref_idx,
        i4_decomp_lyrs_idx);

    /* Coarse picture level init of ME */
    ihevce_coarse_me_frame_init(
        ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
        ps_enc_ctxt->ps_stat_prms,
        &ps_enc_ctxt->s_frm_ctb_prms,
        &ps_curr_out->as_lambda_prms[0],
        ps_enc_ctxt->i4_pre_enc_num_ref_l0,
        ps_enc_ctxt->i4_pre_enc_num_ref_l1,
        ps_enc_ctxt->i4_pre_enc_num_ref_l0_active,
        ps_enc_ctxt->i4_pre_enc_num_ref_l1_active,
        &ps_enc_ctxt->aps_pre_enc_ref_lists[i4_ping_pong][LIST_0][0],
        &ps_enc_ctxt->aps_pre_enc_ref_lists[i4_ping_pong][LIST_1][0],
        ps_curr_inp,
        i4_cur_qp,
        ps_curr_out->ps_layer1_buf,
        ps_curr_out->ps_ed_ctb_l1,
        ps_curr_out->pu1_me_reverse_map_info,
        ps_curr_inp->s_lap_out.i4_temporal_lyr_id);

    /*assign return variables */
    *pps_frm_recon_ret = ps_frm_recon;
}

/*!
******************************************************************************
* \brief
*  Function to calculate modulation based on spatial variance across lap period
*
*****************************************************************************
*/
void ihevce_variance_calc_acc_activity(enc_ctxt_t *ps_enc_ctxt, WORD32 i4_cur_ipe_idx)
{
    pre_enc_me_ctxt_t *ps_curr_out = ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[i4_cur_ipe_idx];
    WORD32 is_curr_bslice = (ps_curr_out->s_slice_hdr.i1_slice_type == BSLICE);
#if MODULATION_OVER_LAP
    WORD32 loop_lap2 = MAX(1, ps_enc_ctxt->s_multi_thrd.i4_delay_pre_me_btw_l0_ipe - 1);
#else
    WORD32 loop_lap2 = 1;
#endif
    WORD32 i4_delay_loop = ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe;
    WORD32 i, j;

    ps_curr_out->i8_acc_frame_8x8_sum_act_sqr = 0;
    ps_curr_out->i8_acc_frame_8x8_sum_act_for_strength = 0;
    for(i = 0; i < 2; i++)
    {
        ps_curr_out->i8_acc_frame_8x8_sum_act[i] = 0;
        ps_curr_out->i4_acc_frame_8x8_num_blks[i] = 0;
        ps_curr_out->i8_acc_frame_16x16_sum_act[i] = 0;
        ps_curr_out->i4_acc_frame_16x16_num_blks[i] = 0;
        ps_curr_out->i8_acc_frame_32x32_sum_act[i] = 0;
        ps_curr_out->i4_acc_frame_32x32_num_blks[i] = 0;
    }
    ps_curr_out->i8_acc_frame_16x16_sum_act[i] = 0;
    ps_curr_out->i4_acc_frame_16x16_num_blks[i] = 0;
    ps_curr_out->i8_acc_frame_32x32_sum_act[i] = 0;
    ps_curr_out->i4_acc_frame_32x32_num_blks[i] = 0;

    if(!is_curr_bslice)
    {
        for(i = 0; i < loop_lap2; i++)
        {
            WORD32 ipe_idx_tmp = (i4_cur_ipe_idx + i) % i4_delay_loop;
            ihevce_lap_enc_buf_t *ps_in = ps_enc_ctxt->s_multi_thrd.aps_curr_inp_pre_enc[ipe_idx_tmp];
            pre_enc_me_ctxt_t *ps_out = ps_enc_ctxt->s_multi_thrd.aps_curr_out_pre_enc[ipe_idx_tmp];
            UWORD8 is_bslice = (ps_out->s_slice_hdr.i1_slice_type == BSLICE);

            if(!is_bslice)
            {
                ps_curr_out->i8_acc_frame_8x8_sum_act_sqr += ps_out->u8_curr_frame_8x8_sum_act_sqr;
                ps_curr_out->i8_acc_frame_8x8_sum_act_for_strength += ps_out->i4_curr_frame_8x8_sum_act_for_strength[0];
                for(j = 0; j < 2; j++)
                {
                    ps_curr_out->i8_acc_frame_8x8_sum_act[j] += ps_out->i8_curr_frame_8x8_sum_act[j];
                    ps_curr_out->i4_acc_frame_8x8_num_blks[j] += ps_out->i4_curr_frame_8x8_num_blks[j];
                    ps_curr_out->i8_acc_frame_16x16_sum_act[j] += ps_out->i8_curr_frame_16x16_sum_act[j];
                    ps_curr_out->i4_acc_frame_16x16_num_blks[j] += ps_out->i4_curr_frame_16x16_num_blks[j];
                    ps_curr_out->i8_acc_frame_32x32_sum_act[j] += ps_out->i8_curr_frame_32x32_sum_act[j];
                    ps_curr_out->i4_acc_frame_32x32_num_blks[j] += ps_out->i4_curr_frame_32x32_num_blks[j];
                }
                ps_curr_out->i8_acc_frame_16x16_sum_act[j] += ps_out->i8_curr_frame_16x16_sum_act[j];
                ps_curr_out->i4_acc_frame_16x16_num_blks[j] += ps_out->i4_curr_frame_16x16_num_blks[j];
                ps_curr_out->i8_acc_frame_32x32_sum_act[j] += ps_out->i8_curr_frame_32x32_sum_act[j];
                ps_curr_out->i4_acc_frame_32x32_num_blks[j] += ps_out->i4_curr_frame_32x32_num_blks[j];
            }
            if(NULL == ps_in->s_rc_lap_out.ps_rc_lap_out_next_encode)
                break;
        }

        for(j = 0; j < 3; j++)
        {
            if(j < 2)
                ASSERT(0 != ps_curr_out->i4_acc_frame_8x8_num_blks[j]);
            ASSERT(0 != ps_curr_out->i4_acc_frame_16x16_num_blks[j]);
            ASSERT(0 != ps_curr_out->i4_acc_frame_32x32_num_blks[j]);

#define AVG_ACTIVITY(a, b, c) a = ((b + (c >> 1)) / c)

            if(j < 2)
            {
                if(0 == ps_curr_out->i4_acc_frame_8x8_num_blks[j])
                {
                    ps_curr_out->i8_curr_frame_8x8_avg_act[j] = 0;
                }
                else
                {
                    AVG_ACTIVITY(ps_curr_out->i8_curr_frame_8x8_sum_act_for_strength,
                                 ps_curr_out->i8_acc_frame_8x8_sum_act_for_strength,
                                 ps_curr_out->i4_acc_frame_8x8_num_blks[j]);
                    AVG_ACTIVITY(ps_curr_out->i8_curr_frame_8x8_avg_act[j],
                                 ps_curr_out->i8_acc_frame_8x8_sum_act[j],
                                 ps_curr_out->i4_acc_frame_8x8_num_blks[j]);
                    ps_curr_out->ld_curr_frame_8x8_log_avg[j] =
                        fast_log2(1 + ps_curr_out->i8_curr_frame_8x8_avg_act[j]);
                }
            }

            if(0 == ps_curr_out->i4_acc_frame_16x16_num_blks[j])
            {
                ps_curr_out->i8_curr_frame_16x16_avg_act[j] = 0;
            }
            else
            {
                AVG_ACTIVITY(ps_curr_out->i8_curr_frame_16x16_avg_act[j],
                             ps_curr_out->i8_acc_frame_16x16_sum_act[j],
                             ps_curr_out->i4_acc_frame_16x16_num_blks[j]);
                ps_curr_out->ld_curr_frame_16x16_log_avg[j] =
                    fast_log2(1 + ps_curr_out->i8_curr_frame_16x16_avg_act[j]);
            }

            if(0 == ps_curr_out->i4_acc_frame_32x32_num_blks[j])
            {
                ps_curr_out->i8_curr_frame_32x32_avg_act[j] = 0;
            }
            else
            {
                AVG_ACTIVITY(ps_curr_out->i8_curr_frame_32x32_avg_act[j],
                             ps_curr_out->i8_acc_frame_32x32_sum_act[j],
                             ps_curr_out->i4_acc_frame_32x32_num_blks[j]);
                ps_curr_out->ld_curr_frame_32x32_log_avg[j] =
                    fast_log2(1 + ps_curr_out->i8_curr_frame_32x32_avg_act[j]);
            }
        }

        /* store the avg activity for B pictures */
#if POW_OPT
        ps_enc_ctxt->ald_lap2_8x8_log_avg_act_from_T0[0] = ps_curr_out->ld_curr_frame_8x8_log_avg[0];
        ps_enc_ctxt->ald_lap2_8x8_log_avg_act_from_T0[1] = ps_curr_out->ld_curr_frame_8x8_log_avg[1];
        ps_enc_ctxt->ald_lap2_16x16_log_avg_act_from_T0[0] = ps_curr_out->ld_curr_frame_16x16_log_avg[0];
        ps_enc_ctxt->ald_lap2_16x16_log_avg_act_from_T0[1] = ps_curr_out->ld_curr_frame_16x16_log_avg[1];
        ps_enc_ctxt->ald_lap2_16x16_log_avg_act_from_T0[2] = ps_curr_out->ld_curr_frame_16x16_log_avg[2];
        ps_enc_ctxt->ald_lap2_32x32_log_avg_act_from_T0[0] = ps_curr_out->ld_curr_frame_32x32_log_avg[0];
        ps_enc_ctxt->ald_lap2_32x32_log_avg_act_from_T0[1] = ps_curr_out->ld_curr_frame_32x32_log_avg[1];
        ps_enc_ctxt->ald_lap2_32x32_log_avg_act_from_T0[2] = ps_curr_out->ld_curr_frame_32x32_log_avg[2];
#else
        ps_enc_ctxt->ai8_lap2_8x8_avg_act_from_T0[0] = ps_curr_out->i8_curr_frame_8x8_avg_act[0];
        ps_enc_ctxt->ai8_lap2_8x8_avg_act_from_T0[1] = ps_curr_out->i8_curr_frame_8x8_avg_act[1];
        ps_enc_ctxt->ai8_lap2_16x16_avg_act_from_T0[0] = ps_curr_out->i8_curr_frame_16x16_avg_act[0];
        ps_enc_ctxt->ai8_lap2_16x16_avg_act_from_T0[1] = ps_curr_out->i8_curr_frame_16x16_avg_act[1];
        ps_enc_ctxt->ai8_lap2_16x16_avg_act_from_T0[2] = ps_curr_out->i8_curr_frame_16x16_avg_act[2];
        ps_enc_ctxt->ai8_lap2_32x32_avg_act_from_T0[0] = ps_curr_out->i8_curr_frame_32x32_avg_act[0];
        ps_enc_ctxt->ai8_lap2_32x32_avg_act_from_T0[1] = ps_curr_out->i8_curr_frame_32x32_avg_act[1];
        ps_enc_ctxt->ai8_lap2_32x32_avg_act_from_T0[2] = ps_curr_out->i8_curr_frame_32x32_avg_act[2];
#endif

        /* calculate modulation index */
        {
            LWORD64 i8_mean, i8_mean_sqr, i8_variance;
            LWORD64 i8_deviation;
            WORD32 i4_mod_factor;
            float f_strength;

            if(ps_curr_out->i4_acc_frame_8x8_num_blks[0] > 0)
            {
#if STRENGTH_BASED_ON_CURR_FRM
                AVG_ACTIVITY(i8_mean_sqr, ps_curr_out->i8_curr_frame_8x8_sum_act_sqr,
                             ps_curr_out->i4_curr_frame_8x8_num_blks[0]);
#else
                AVG_ACTIVITY(i8_mean_sqr, ps_curr_out->i8_acc_frame_8x8_sum_act_sqr,
                             ps_curr_out->i4_acc_frame_8x8_num_blks[0]);
#endif
                i8_mean = ps_curr_out->i8_curr_frame_8x8_sum_act_for_strength;
                i8_variance = i8_mean_sqr - (i8_mean * i8_mean);
                i8_deviation = sqrt(i8_variance);

#if STRENGTH_BASED_ON_DEVIATION
                if(i8_deviation <= REF_MOD_DEVIATION)
                {
                    f_strength = ((i8_deviation - BELOW_REF_DEVIATION) * REF_MOD_STRENGTH) / (REF_MOD_DEVIATION - BELOW_REF_DEVIATION);
                }
                else
                {
                    f_strength = ((i8_deviation - ABOVE_REF_DEVIATION) * REF_MOD_STRENGTH) / (REF_MOD_DEVIATION - ABOVE_REF_DEVIATION);
                }
#else
                f_strength = ((i8_mean_sqr / (float)(i8_mean * i8_mean)) - 1.0) * REF_MOD_STRENGTH / REF_MOD_VARIANCE;
#endif
                i4_mod_factor = (WORD32)(i8_deviation / 60);
                f_strength = CLIP3(f_strength, 0.0, REF_MAX_STRENGTH);
            }
            else
            {
                /* If not sufficient blocks are present, turn modulation index to 1  */
                i4_mod_factor = 1;
                f_strength = 0;
            }
            ps_curr_out->ai4_mod_factor_derived_by_variance[0] = i4_mod_factor;
            ps_curr_out->ai4_mod_factor_derived_by_variance[1] = i4_mod_factor;
            ps_curr_out->f_strength = f_strength;

            ps_enc_ctxt->ai4_mod_factor_derived_by_variance[0] = i4_mod_factor;
            ps_enc_ctxt->ai4_mod_factor_derived_by_variance[1] = i4_mod_factor;
            ps_enc_ctxt->f_strength = f_strength;
        }
    }
    else
    {
        ps_curr_out->ai4_mod_factor_derived_by_variance[0] = ps_enc_ctxt->ai4_mod_factor_derived_by_variance[0];
        ps_curr_out->ai4_mod_factor_derived_by_variance[1] = ps_enc_ctxt->ai4_mod_factor_derived_by_variance[1];
        ps_curr_out->f_strength = ps_enc_ctxt->f_strength;

        /* copy the prev avg activity from Tid 0 for B pictures*/
#if POW_OPT
        ps_curr_out->ld_curr_frame_8x8_log_avg[0] = ps_enc_ctxt->ald_lap2_8x8_log_avg_act_from_T0[0];
        ps_curr_out->ld_curr_frame_8x8_log_avg[1] = ps_enc_ctxt->ald_lap2_8x8_log_avg_act_from_T0[1];
        ps_curr_out->ld_curr_frame_16x16_log_avg[0] = ps_enc_ctxt->ald_lap2_16x16_log_avg_act_from_T0[0];
        ps_curr_out->ld_curr_frame_16x16_log_avg[1] = ps_enc_ctxt->ald_lap2_16x16_log_avg_act_from_T0[1];
        ps_curr_out->ld_curr_frame_16x16_log_avg[2] = ps_enc_ctxt->ald_lap2_16x16_log_avg_act_from_T0[2];
        ps_curr_out->ld_curr_frame_32x32_log_avg[0] = ps_enc_ctxt->ald_lap2_32x32_log_avg_act_from_T0[0];
        ps_curr_out->ld_curr_frame_32x32_log_avg[1] = ps_enc_ctxt->ald_lap2_32x32_log_avg_act_from_T0[1];
        ps_curr_out->ld_curr_frame_32x32_log_avg[2] = ps_enc_ctxt->ald_lap2_32x32_log_avg_act_from_T0[2];
#else
        ps_curr_out->i8_curr_frame_8x8_avg_act[0] = ps_enc_ctxt->ai8_lap2_8x8_avg_act_from_T0[0];
        ps_curr_out->i8_curr_frame_8x8_avg_act[1] = ps_enc_ctxt->ai8_lap2_8x8_avg_act_from_T0[1];
        ps_curr_out->i8_curr_frame_16x16_avg_act[0] = ps_enc_ctxt->ai8_lap2_16x16_avg_act_from_T0[0];
        ps_curr_out->i8_curr_frame_16x16_avg_act[1] = ps_enc_ctxt->ai8_lap2_16x16_avg_act_from_T0[1];
        ps_curr_out->i8_curr_frame_16x16_avg_act[2] = ps_enc_ctxt->ai8_lap2_16x16_avg_act_from_T0[2];
        ps_curr_out->i8_curr_frame_32x32_avg_act[0] = ps_enc_ctxt->ai8_lap2_32x32_avg_act_from_T0[0];
        ps_curr_out->i8_curr_frame_32x32_avg_act[1] = ps_enc_ctxt->ai8_lap2_32x32_avg_act_from_T0[1];
        ps_curr_out->i8_curr_frame_32x32_avg_act[2] = ps_enc_ctxt->ai8_lap2_32x32_avg_act_from_T0[2];
#endif
    }
#undef AVG_ACTIVITY
}

/*!
******************************************************************************
* \if Function name : ihevce_pre_enc_process_frame_thrd \endif
*
* \brief
*    Pre-Encode Frame processing thread interface function
*
* \param[in] High level encoder context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_pre_enc_process_frame_thrd(void *pv_frm_proc_thrd_ctxt)
{
    frm_proc_thrd_ctxt_t *ps_thrd_ctxt = (frm_proc_thrd_ctxt_t *)pv_frm_proc_thrd_ctxt;
    ihevce_hle_ctxt_t *ps_hle_ctxt = ps_thrd_ctxt->ps_hle_ctxt;
    enc_ctxt_t *ps_enc_ctxt = (enc_ctxt_t *)ps_thrd_ctxt->pv_enc_ctxt;
    multi_thrd_ctxt_t *ps_multi_thrd = &ps_enc_ctxt->s_multi_thrd;
    WORD32 i4_thrd_id = ps_thrd_ctxt->i4_thrd_id;
    WORD32 i4_resolution_id = ps_enc_ctxt->i4_resolution_id;
    WORD32 i4_end_flag = 0;
    WORD32 i4_out_flush_flag = 0;
    WORD32 i4_cur_decomp_idx = 0;
    WORD32 i4_cur_coarse_me_idx = 0;
    WORD32 i4_cur_ipe_idx = 0;
    ihevce_lap_enc_buf_t *ps_lap_inp_buf = NULL;
    void *pv_dep_mngr_prev_frame_pre_enc_l1 = ps_multi_thrd->pv_dep_mngr_prev_frame_pre_enc_l1;
    void *pv_dep_mngr_prev_frame_pre_enc_l0 = ps_multi_thrd->pv_dep_mngr_prev_frame_pre_enc_l0;
    void *pv_dep_mngr_prev_frame_pre_enc_coarse_me =
        ps_multi_thrd->pv_dep_mngr_prev_frame_pre_enc_coarse_me;
    WORD32 i4_num_buf_prod_for_l0_ipe = 0;
    WORD32 i4_decomp_end_flag = 0;

    (void)ps_hle_ctxt;
    (void)i4_resolution_id;

    /* ---------- Processing Loop until Flush command is received --------- */
    while(0 == i4_end_flag)
    {
        /* Wait till previous frame(instance)'s decomp_intra is processed */
        {
            ihevce_dmgr_chk_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_l1, i4_thrd_id);
        }

        /* ----------------------------------------------------------- */
        /*     decomp pre_intra init                                   */
        /* ----------------------------------------------------------- */

        /****** Lock the critical section for decomp pre_intra init ******/
        {
            WORD32 i4_status;

            i4_status = osal_mutex_lock(ps_multi_thrd->pv_mutex_hdl_pre_enc_init);
            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        ps_multi_thrd->ai4_decomp_coarse_me_complete_flag[i4_cur_decomp_idx] = 0;

        /* init */
        if((ps_multi_thrd->ai4_pre_enc_init_done[i4_cur_decomp_idx] == 0) &&
           (0 == i4_decomp_end_flag))
        {
            ihevce_lap_enc_buf_t *ps_curr_inp = NULL;
            pre_enc_me_ctxt_t *ps_curr_out = NULL;
            WORD32 in_buf_id;
            WORD32 out_buf_id;

            do
            {
                ps_lap_inp_buf = NULL;
                if(0 == ps_multi_thrd->i4_last_inp_buf)
                {
                    /* ------- get input buffer input data que ---------- */
                    ps_lap_inp_buf = (ihevce_lap_enc_buf_t *)ihevce_q_get_filled_buff(
                        (void *)ps_enc_ctxt,
                        IHEVCE_INPUT_DATA_CTRL_Q,
                        &in_buf_id,
                        BUFF_QUE_BLOCKING_MODE);
                    ps_multi_thrd->i4_last_inp_buf = ihevce_check_last_inp_buf(
                        (WORD32 *)ps_lap_inp_buf->s_input_buf.pv_synch_ctrl_bufs);
                }

                ps_curr_inp =
                    ihevce_lap_process(ps_enc_ctxt->pv_lap_interface_ctxt, ps_lap_inp_buf);

            } while(NULL == ps_curr_inp);

            /* set the flag saying init is done so that other cores dont do it */
            ps_multi_thrd->ai4_pre_enc_init_done[i4_cur_decomp_idx] = 1;

            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_decomp_idx] = ps_curr_inp;
            ps_multi_thrd->ai4_in_buf_id_pre_enc[i4_cur_decomp_idx] =
                ps_curr_inp->s_input_buf.i4_buf_id;

            /* ------- get free output buffer from pre-enc/enc buffer que ---------- */
            ps_curr_out = (pre_enc_me_ctxt_t *)ihevce_q_get_free_buff(
                (void *)ps_enc_ctxt, IHEVCE_PRE_ENC_ME_Q, &out_buf_id, BUFF_QUE_BLOCKING_MODE);
            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_decomp_idx] = ps_curr_out;
            ps_multi_thrd->ai4_out_buf_id_pre_enc[i4_cur_decomp_idx] = out_buf_id;

            if((NULL != ps_curr_inp) && (NULL != ps_curr_out))
            {
                /* by default last picture to be encoded flag is set to 0      */
                /* this flag will be used by slave threads to exit at the end */
                ps_multi_thrd->i4_last_pic_flag = 0;

                /* store the buffer id */
                ps_curr_out->i4_buf_id = out_buf_id;

                ps_curr_out->i8_acc_num_blks_high_sad = 0;
                ps_curr_out->i8_total_blks = 0;
                ps_curr_out->i4_is_high_complex_region = -1;

                /* set the parameters for sync b/w pre-encode and encode threads */
                ps_curr_out->i4_end_flag = ps_curr_inp->s_lap_out.i4_end_flag;
                ps_curr_out->i4_frm_proc_valid_flag = 1;
                if(ps_curr_out->i4_end_flag)
                {
                    ps_curr_out->i4_frm_proc_valid_flag =
                        ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag;
                    ps_multi_thrd->i4_last_pic_flag = 1;
                    ps_multi_thrd->ai4_end_flag_pre_enc[i4_cur_decomp_idx] = 1;
                }
                if(ps_curr_inp->s_lap_out.i4_out_flush_flag)
                {
                    ps_curr_out->i4_frm_proc_valid_flag =
                        ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag;
                }

                /* do the init processing if input frm data is valid */
                if(1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag)
                {
                    WORD32 end_flag = ps_multi_thrd->ai4_end_flag_pre_enc[i4_cur_decomp_idx];
                    WORD32 cur_qp = 0, count;

                    ihevce_pre_enc_init(
                        ps_enc_ctxt,
                        ps_curr_inp,
                        ps_curr_out,
                        &end_flag,
                        &cur_qp,
                        &ps_multi_thrd->ai4_decomp_lyr_buf_idx[i4_cur_decomp_idx],
                        i4_cur_decomp_idx);

                    ps_multi_thrd->ai4_end_flag_pre_enc[i4_cur_decomp_idx] = end_flag;
                    ps_multi_thrd->ai4_cur_frame_qp_pre_enc[i4_cur_decomp_idx] = cur_qp;

                    for(count = 0; count < ((HEVCE_MAX_HEIGHT >> 1) / 8); count++)
                    {
                        ps_multi_thrd->aai4_l1_pre_intra_done[i4_cur_decomp_idx][count] = 0;
                    }
                }
            }
        }
        else if(1 == i4_decomp_end_flag)
        {
            /* Once end is reached all subsequent flags are set to 1 to indicate end */
            ps_multi_thrd->ai4_end_flag_pre_enc[i4_cur_decomp_idx] = 1;
        }

        /****** UnLock the critical section after decomp pre_intra init ******/
        {
            WORD32 i4_status;
            i4_status = osal_mutex_unlock(ps_multi_thrd->pv_mutex_hdl_pre_enc_init);

            if(OSAL_SUCCESS != i4_status)
                return 0;
        }
        if(i4_thrd_id == 0)
        {
            PROFILE_START(&ps_hle_ctxt->profile_pre_enc_l1l2[i4_resolution_id]);
        }
        /* ------------------------------------------------------------ */
        /*        Layer Decomp and Pre Intra Analysis                   */
        /* ------------------------------------------------------------ */
        if(0 == i4_decomp_end_flag)
        {
            pre_enc_me_ctxt_t *ps_curr_out = ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_decomp_idx];

            if(1 == ps_curr_out->i4_frm_proc_valid_flag)
            {
                ihevce_decomp_pre_intra_process(
                    ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt,
                    &ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_decomp_idx]->s_lap_out,
                    &ps_enc_ctxt->s_frm_ctb_prms,
                    ps_multi_thrd,
                    i4_thrd_id,
                    i4_cur_decomp_idx);
            }
        }

        /* ------------------------------------------------------------ */
        /*        Layer Decomp and Pre Intra Deinit                     */
        /* ------------------------------------------------------------ */

        /****** Lock the critical section for decomp deinit ******/
        {
            WORD32 i4_status;
            i4_status = osal_mutex_lock(ps_multi_thrd->pv_mutex_hdl_pre_enc_decomp_deinit);

            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        ps_multi_thrd->ai4_num_thrds_processed_decomp[i4_cur_decomp_idx]++;
        i4_decomp_end_flag = ps_multi_thrd->ai4_end_flag_pre_enc[i4_cur_decomp_idx];

        /* check for last thread condition */
        if(ps_multi_thrd->ai4_num_thrds_processed_decomp[i4_cur_decomp_idx] ==
           ps_multi_thrd->i4_num_pre_enc_proc_thrds)
        {
            ps_multi_thrd->ai4_num_thrds_processed_decomp[i4_cur_decomp_idx] = 0;

            /* reset the init flag so that init happens by the first thread for the next frame
               of same ping_pong instance */
            ps_multi_thrd->ai4_pre_enc_init_done[i4_cur_decomp_idx] = 0;

            /* update the pre enc l1 done in dep manager */
            ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_l1);
        }

        /* index increment */
        i4_cur_decomp_idx = i4_cur_decomp_idx + 1;

        /* wrap around case */
        if(i4_cur_decomp_idx >= ps_multi_thrd->i4_max_delay_pre_me_btw_l0_ipe)
        {
            i4_cur_decomp_idx = 0;
        }

        /****** UnLock the critical section after decomp pre_intra deinit ******/
        {
            WORD32 i4_status;
            i4_status = osal_mutex_unlock(ps_multi_thrd->pv_mutex_hdl_pre_enc_decomp_deinit);

            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        /* ------------------------------------------------------------ */
        /*                     HME Init                                 */
        /* ------------------------------------------------------------ */

        /* Wait till previous frame(instance)'s coarse_me is processed */
        {
            ihevce_dmgr_chk_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_coarse_me, i4_thrd_id);
        }

        /****** Lock the critical section for hme init ******/
        {
            WORD32 i4_status;

            i4_status = osal_mutex_lock(ps_multi_thrd->pv_mutex_hdl_pre_enc_hme_init);
            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        if(0 == ps_multi_thrd->ai4_pre_enc_hme_init_done[i4_cur_coarse_me_idx])
        {
            /* do the init processing if input frm data is valid */
            if(1 ==
               ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx]->i4_frm_proc_valid_flag)
            {
                recon_pic_buf_t *ps_frm_recon = NULL;

                /* DPB management for coarse me + HME init */
                ihevce_pre_enc_coarse_me_init(
                    ps_enc_ctxt,
                    ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_coarse_me_idx],
                    ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx],
                    &ps_frm_recon,
                    ps_multi_thrd->ai4_decomp_lyr_buf_idx[i4_cur_coarse_me_idx],
                    ps_multi_thrd->ai4_cur_frame_qp_pre_enc[i4_cur_coarse_me_idx],
                    i4_cur_coarse_me_idx);
            }

            ps_multi_thrd->ai4_pre_enc_hme_init_done[i4_cur_coarse_me_idx] = 1;
        }

        /****** Unlock the critical section for hme init ******/
        {
            WORD32 i4_status;

            i4_status = osal_mutex_unlock(ps_multi_thrd->pv_mutex_hdl_pre_enc_hme_init);
            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        /* ------------------------------------------------------------ */
        /*  Coarse Motion estimation and early intra-inter decision     */
        /* ------------------------------------------------------------ */
        if(1 == ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx]->i4_frm_proc_valid_flag)
        {
            ihevce_coarse_me_process(
                ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
                ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_coarse_me_idx],
                &ps_enc_ctxt->s_multi_thrd,
                i4_thrd_id,
                i4_cur_coarse_me_idx);
        }

        /* update the end flag */
        i4_end_flag = ps_multi_thrd->ai4_end_flag_pre_enc[i4_cur_coarse_me_idx];
        i4_out_flush_flag =
            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_coarse_me_idx]->s_lap_out.i4_out_flush_flag;

        /****** Lock the critical section for hme deinit ******/
        {
            WORD32 i4_status;
            i4_status = osal_mutex_lock(ps_multi_thrd->pv_mutex_hdl_pre_enc_hme_deinit);

            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        /* last thread finishing pre_enc_process will update the flag indicating
        decomp and coarse ME is done. So that the next frame (next ping_pong instance)
        can start immediately after finishing current frame's IPE */
        if(1 == ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx]->i4_frm_proc_valid_flag)
        {
            ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx]++;

            /* ------------------------------------------------------------ */
            /*  Update qp used in based in L1 satd/act in case of scene cut */
            /* ------------------------------------------------------------ */
            {
                ihevce_lap_enc_buf_t *ps_curr_inp =
                    ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_coarse_me_idx];

                if(1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag)
                {
                    WORD32 i4_prev_coarse_me_idx;

                    /* wrap around case */
                    if(i4_cur_coarse_me_idx == 0)
                    {
                        i4_prev_coarse_me_idx = ps_multi_thrd->i4_max_delay_pre_me_btw_l0_ipe - 1;
                    }
                    else
                    {
                        i4_prev_coarse_me_idx = i4_cur_coarse_me_idx - 1;
                    }

                    ihevce_update_qp_L1_sad_based(
                        ps_enc_ctxt,
                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_coarse_me_idx],
                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_prev_coarse_me_idx],
                        ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx],
                        ((ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx] ==
                          ps_multi_thrd->i4_num_pre_enc_proc_thrds)));
                }
            }
            /* check for last thread condition */
            if(ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx] ==
               ps_multi_thrd->i4_num_pre_enc_proc_thrds)
            {
                ihevce_lap_enc_buf_t *ps_curr_inp =
                    ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_coarse_me_idx];

                /*        Frame END processing                  */
                ihevce_coarse_me_frame_end(ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt);

                if(1 == ps_curr_inp->s_input_buf.i4_inp_frm_data_valid_flag)
                {
                    WORD32 i4_enable_noise_detection = 0;
                    WORD32 i4_vqet = ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet;

                    if(i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER))
                    {
                        if(i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION))
                        {
                            i4_enable_noise_detection = 1;
                        }
                    }

                    if(1 != ((ps_curr_inp->s_lap_out.i4_pic_type == IV_B_FRAME) &&
                             (ps_enc_ctxt->s_lap_stat_prms.ai4_quality_preset[i4_resolution_id] ==
                              IHEVCE_QUALITY_P6)))
                    {
                        ihevce_decomp_pre_intra_curr_frame_pre_intra_deinit(
                            ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx],
                            &ps_enc_ctxt->s_frm_ctb_prms);
                    }
                }

                ps_multi_thrd->ai4_decomp_coarse_me_complete_flag[i4_cur_coarse_me_idx] = 1;

                ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx] = 0;

                /* get the layer 1 ctxt to be passed on to encode group */
                ihevce_coarse_me_get_lyr1_ctxt(
                    ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
                    ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx]->pv_me_lyr_ctxt,
                    ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_coarse_me_idx]->pv_me_lyr_bnk_ctxt);

                /* reset the init flag so that init happens by the first thread for the next frame
                    of same ping_pong instance */
                ps_multi_thrd->ai4_pre_enc_hme_init_done[i4_cur_coarse_me_idx] = 0;

                /* update the pre enc l1 done in dep manager */
                ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_coarse_me);
            }

            i4_num_buf_prod_for_l0_ipe++;

            /* index increment */
            i4_cur_coarse_me_idx = i4_cur_coarse_me_idx + 1;

            /* wrap around case */
            if(i4_cur_coarse_me_idx >= ps_multi_thrd->i4_max_delay_pre_me_btw_l0_ipe)
            {
                i4_cur_coarse_me_idx = 0;
            }
        }
        else
        {
            /* for invalid frame set the processed flag to 1 for L0 IPE */
            ps_multi_thrd->ai4_decomp_coarse_me_complete_flag[i4_cur_coarse_me_idx] = 1;

            if(1 == i4_out_flush_flag)
            {
                /* update the num thrds who have finished pre-enc processing */
                ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx]++;

                if(ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx] ==
                   ps_multi_thrd->i4_num_pre_enc_proc_thrds)
                {
                    ps_multi_thrd->ai4_decomp_coarse_me_complete_flag[i4_cur_coarse_me_idx] = 1;

                    /* reset num thread finished counter */
                    ps_multi_thrd->ai4_num_thrds_processed_coarse_me[i4_cur_coarse_me_idx] = 0;

                    ps_multi_thrd->ai4_pre_enc_hme_init_done[i4_cur_coarse_me_idx] = 0;

                    /* update flag indicating coarse_me and decomp is done */
                    ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_coarse_me);
                }
            }

            i4_num_buf_prod_for_l0_ipe++;

            /* index increment */
            i4_cur_coarse_me_idx = i4_cur_coarse_me_idx + 1;

            /* wrap around case */
            if(i4_cur_coarse_me_idx >= ps_multi_thrd->i4_max_delay_pre_me_btw_l0_ipe)
            {
                i4_cur_coarse_me_idx = 0;
            }
        }

        /****** UnLock the critical section after hme deinit ******/
        {
            WORD32 i4_status;
            i4_status =
                osal_mutex_unlock(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_deinit);

            if(OSAL_SUCCESS != i4_status)
                return 0;
        }

        if(i4_thrd_id == 0)
        {
            PROFILE_STOP(&ps_hle_ctxt->profile_pre_enc_l1l2[i4_resolution_id], NULL);
        }

        /* ----------------------------------------------------------- */
        /*     IPE init and process                                    */
        /* ----------------------------------------------------------- */
        if(i4_thrd_id == 0)
        {
            PROFILE_START(&ps_hle_ctxt->profile_pre_enc_l0ipe[i4_resolution_id]);
        }
        if(i4_num_buf_prod_for_l0_ipe >= ps_multi_thrd->i4_delay_pre_me_btw_l0_ipe || i4_end_flag ||
           i4_out_flush_flag)
        {
            do
            {
                /* Wait till previous frame(instance)'s IPE is processed */
                {
                    ihevce_dmgr_chk_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_l0, i4_thrd_id);
                }

                /* Wait till current frame(instance)'s L1 and below layers are processed */
                {
                    volatile WORD32 *pi4_cur_l1_complete =
                        &ps_multi_thrd->ai4_decomp_coarse_me_complete_flag[i4_cur_ipe_idx];

                    while(1)
                    {
                        if(*pi4_cur_l1_complete)
                            break;
                    }
                }

                /* ----------------------------------------------------------- */
                /*     L0 IPE qp init                                          */
                /* ----------------------------------------------------------- */

                /****** Lock the critical section for init ******/
                {
                    WORD32 i4_status;
                    i4_status = osal_mutex_lock(ps_multi_thrd->pv_mutex_hdl_l0_ipe_init);

                    if(OSAL_SUCCESS != i4_status)
                        return 0;
                }

                /* first thread that enters will calculate qp and write that to shared variable
                   that will be accessed by other threads */
                if(ps_multi_thrd->ai4_num_thrds_processed_L0_ipe_qp_init[i4_cur_ipe_idx] == 0)
                {
                    volatile WORD32 i4_is_qp_valid = -1;
                    WORD32 i4_update_qp;
                    WORD32 i4_cur_q_scale;

                    i4_cur_q_scale =
                        ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->i4_curr_frm_qp;
                    i4_cur_q_scale = ps_enc_ctxt->s_rc_quant.pi4_qp_to_qscale[i4_cur_q_scale];
                    i4_cur_q_scale = (i4_cur_q_scale + (1 << (QSCALE_Q_FAC_3 - 1))) >>
                                     QSCALE_Q_FAC_3;
                    /* Get free buffer to store L0 IPE output to enc loop */
                    ps_multi_thrd->ps_L0_IPE_curr_out_pre_enc =
                        (pre_enc_L0_ipe_encloop_ctxt_t *)ihevce_q_get_free_buff(
                            (void *)ps_enc_ctxt,
                            IHEVCE_L0_IPE_ENC_Q,
                            &ps_multi_thrd->i4_L0_IPE_out_buf_id,
                            BUFF_QUE_BLOCKING_MODE);
                    if(ps_enc_ctxt->ps_stat_prms->s_pass_prms.i4_pass != 2 &&
                       ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
                    {
                        complexity_RC_reset_marking(
                            ps_enc_ctxt, i4_cur_ipe_idx, (i4_end_flag || i4_out_flush_flag));
                    }
                    if(1 == ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                ->s_input_buf.i4_inp_frm_data_valid_flag)
                    {
                        while(i4_is_qp_valid == -1)
                        {
                            /*this rate control call is outside mutex lock to avoid deadlock. If this acquires mutex lock enc will not be able to
                            populate qp*/
                            i4_is_qp_valid = ihevce_rc_check_is_pre_enc_qp_valid(
                                (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                                (volatile WORD32 *)&ps_enc_ctxt->s_multi_thrd.i4_force_end_flag);
                            if(1 == ps_enc_ctxt->s_multi_thrd.i4_force_end_flag)
                            {
                                /*** For force end condition break from this loop ***/
                                i4_is_qp_valid = 1;
                                break;
                            }
                        }

                        /*lock rate control context*/
                        osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

                        /* Qp query has to happen irrespective of using it or not since producer consumer logic will be disturbed */
                        i4_update_qp = ihevce_rc_pre_enc_qp_query(
                            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                            &ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]->s_rc_lap_out,
                            0);

                        if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
                        {
                            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                ->s_rc_lap_out.i8_frm_satd_act_accum_L0_frm_L1 =
                                ihevce_get_L0_satd_based_on_L1(
                                    ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                        ->s_rc_lap_out.i8_frame_satd_by_act_L1_accum,
                                    ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                        ->s_rc_lap_out.i4_num_pels_in_frame_considered,
                                    i4_cur_q_scale);

                            if(ps_enc_ctxt->ps_stat_prms->s_pass_prms.i4_pass != 2)
                            {
                                if(ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                           ->s_rc_lap_out.i4_rc_scene_type ==
                                       SCENE_TYPE_SCENE_CUT ||
                                   ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                       ->s_rc_lap_out.i4_is_I_only_scd ||
                                   ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                           ->s_rc_lap_out.i4_is_non_I_scd == 1)
                                {
                                    float i_to_avg_rest_ratio;
                                    WORD32 i4_count = 0;

                                    while(1)
                                    {
                                        i_to_avg_rest_ratio = ihevce_get_i_to_avg_ratio(
                                            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                                            &ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                 ->s_rc_lap_out,
                                            1,
                                            0,
                                            0,
                                            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                ->s_rc_lap_out.ai4_offsets,
                                            0);
                                        /* HEVC_RC query rate control for qp */
                                        i4_update_qp = ihevce_get_L0_est_satd_based_scd_qp(
                                            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                                            &ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                 ->s_rc_lap_out,
                                            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                ->s_rc_lap_out.i8_frm_satd_act_accum_L0_frm_L1,
                                            i_to_avg_rest_ratio);

                                        ihevce_set_L0_scd_qp(
                                            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[0],
                                            i4_update_qp);

                                        if(ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                   ->s_lap_out.i4_pic_type != IV_IDR_FRAME &&
                                           ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                   ->s_lap_out.i4_pic_type != IV_I_FRAME)
                                        {
                                            i4_update_qp +=
                                                ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                    ->s_lap_out.i4_temporal_lyr_id +
                                                1;

                                            i4_update_qp =
                                                CLIP3(i4_update_qp, MIN_HEVC_QP, MAX_HEVC_QP);
                                        }

                                        i4_count++;
                                        if((i4_update_qp ==
                                            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                ->s_rc_lap_out.i4_L0_qp) ||
                                           i4_count > 4)
                                            break;

                                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                            ->s_rc_lap_out.i4_L0_qp = i4_update_qp;
                                    }
                                }
                            }
                            else
                            {
                                //i4_update_qp = ihevce_get_first_pass_qp(ps_enc_ctxt->s_multi_thrd.aps_curr_inp_pre_enc[i4_cur_ipe_idx]->s_lap_out.pv_frame_info);
                                i4_update_qp = ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                                                   ->s_rc_lap_out.ps_frame_info->i4_rc_hevc_qp;
                            }
                        }

                        {
                            WORD32 i4_index = 0;
                            rc_lap_out_params_t *ps_rc_lap_temp =
                                &ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]->s_rc_lap_out;
                            WORD32 i4_offset;

                            if(ps_rc_lap_temp->i4_rc_pic_type != IV_IDR_FRAME &&
                               ps_rc_lap_temp->i4_rc_pic_type != IV_I_FRAME)
                            {
                                i4_index = ps_rc_lap_temp->i4_rc_temporal_lyr_id + 1;
                            }
                            i4_offset = ps_rc_lap_temp->ai4_offsets[i4_index];
                            ASSERT(i4_offset >= 0);
                            /* Map the current frame Qp to L0 Qp */
                            ps_rc_lap_temp->i4_L0_qp = i4_update_qp - i4_offset;
                        }
                        osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
                        ASSERT(ps_multi_thrd->i4_qp_update_l0_ipe == -1);
                        ps_multi_thrd->i4_qp_update_l0_ipe = i4_update_qp;
                        ps_multi_thrd->i4_rc_l0_qp = i4_update_qp;
                    }
                    ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                        ->s_lap_out.f_i_pic_lamda_modifier = CONST_LAMDA_MOD_VAL;
                }
                /* update qp only if it is not scene cut since it has already been
                   populated in L1 for scene cut frames */
                if(1 == ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_input_buf.i4_inp_frm_data_valid_flag &&
                   ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
                {
                    /*get relevant lambda params*/
                    ihevce_get_frame_lambda_prms(
                        ps_enc_ctxt,
                        ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx],
                        ps_multi_thrd->i4_qp_update_l0_ipe,
                        ps_enc_ctxt->s_runtime_src_prms.i4_field_pic,
                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]->s_lap_out.i4_is_ref_pic,
                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_lap_out.i4_temporal_lyr_id,
                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_lap_out.f_i_pic_lamda_modifier,
                        0,
                        PRE_ENC_LAMBDA_TYPE);

                    ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->i4_curr_frm_qp =
                        ps_multi_thrd->i4_qp_update_l0_ipe;
                }
                /* Compute accumulated activity and strength */
                if(1 == ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_input_buf.i4_inp_frm_data_valid_flag &&
                   ps_multi_thrd->ai4_num_thrds_processed_L0_ipe_qp_init[i4_cur_ipe_idx] == 0)
                {
                    ihevce_variance_calc_acc_activity(ps_enc_ctxt, i4_cur_ipe_idx);
                }

                /* Mark qp as read by last thread */
                ps_multi_thrd->ai4_num_thrds_processed_L0_ipe_qp_init[i4_cur_ipe_idx]++;
                if(ps_multi_thrd->ai4_num_thrds_processed_L0_ipe_qp_init[i4_cur_ipe_idx] ==
                   ps_multi_thrd->i4_num_pre_enc_proc_thrds)
                {
                    ps_multi_thrd->ai4_num_thrds_processed_L0_ipe_qp_init[i4_cur_ipe_idx] = 0;
                    ps_multi_thrd->i4_qp_update_l0_ipe = -1;
                }

                /****** UnLock the critical section after deinit ******/
                {
                    WORD32 i4_status;
                    i4_status = osal_mutex_unlock(ps_multi_thrd->pv_mutex_hdl_l0_ipe_init);

                    if(OSAL_SUCCESS != i4_status)
                        return 0;
                }

                if(1 == ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_input_buf.i4_inp_frm_data_valid_flag)
                {
                    WORD32 i4_slice_type =
                        (WORD32)ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]
                            ->s_slice_hdr.i1_slice_type;
                    WORD32 i4_quality_preset =
                        (WORD32)ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_lap_out.i4_quality_preset;
                    WORD32 i4_temporal_layer_id =
                        (WORD32)ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]
                            ->s_lap_out.i4_temporal_lyr_id;
#if DISABLE_L0_IPE_INTRA_IN_BPICS
                    if(1 != ((i4_quality_preset == IHEVCE_QUALITY_P6) &&
                             (i4_temporal_layer_id > TEMPORAL_LAYER_DISABLE)))
#endif
                    {
                        UWORD8 i1_cu_qp_delta_enabled_flag =
                            ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_cu_level_rc;

                        ihevce_populate_ipe_frame_init(
                            ps_enc_ctxt->s_module_ctxt.pv_ipe_ctxt,
                            ps_enc_ctxt->ps_stat_prms,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->i4_curr_frm_qp,
                            i4_slice_type,
                            i4_thrd_id,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx],
                            i1_cu_qp_delta_enabled_flag,
                            &ps_enc_ctxt->s_rc_quant,
                            i4_quality_preset,
                            i4_temporal_layer_id,
                            &ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx]->s_lap_out);

                        ihevce_ipe_process(
                            ps_enc_ctxt->s_module_ctxt.pv_ipe_ctxt,
                            &ps_enc_ctxt->s_frm_ctb_prms,
                            &ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->as_lambda_prms[0],
                            ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx],
                            ps_multi_thrd->ps_L0_IPE_curr_out_pre_enc,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->ps_ctb_analyse,
                            ps_multi_thrd->ps_L0_IPE_curr_out_pre_enc->ps_ipe_analyse_ctb,
                            &ps_enc_ctxt->s_multi_thrd,
                            i4_slice_type,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->ps_layer1_buf,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->ps_layer2_buf,
                            ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx]->ps_ed_ctb_l1,
                            i4_thrd_id,
                            i4_cur_ipe_idx);
                    }
                }

                /* ----------------------------------------------------------- */
                /*     pre-enc de-init                                         */
                /* ----------------------------------------------------------- */

                /****** Lock the critical section for deinit ******/
                {
                    WORD32 i4_status;
                    i4_status = osal_mutex_lock(ps_multi_thrd->pv_mutex_hdl_pre_enc_deinit);

                    if(OSAL_SUCCESS != i4_status)
                        return 0;
                }

                ps_multi_thrd->ai4_num_thrds_processed_pre_enc[i4_cur_ipe_idx]++;
                if(ps_multi_thrd->ai4_num_thrds_processed_pre_enc[i4_cur_ipe_idx] ==
                   ps_multi_thrd->i4_num_pre_enc_proc_thrds)
                {
                    ps_multi_thrd->ai4_pre_enc_deinit_done[i4_cur_ipe_idx] = 0;
                    ps_multi_thrd->ai4_num_thrds_processed_pre_enc[i4_cur_ipe_idx] = 0;

                    /* reset the init flag so that init happens by the first thread for the
                       next frame of same ping_pnog instnace */
                    ps_multi_thrd->ai4_pre_enc_init_done[i4_cur_ipe_idx] = 0;
                }

                /* de-init */
                if(0 == ps_multi_thrd->ai4_pre_enc_deinit_done[i4_cur_ipe_idx])
                {
                    ihevce_lap_enc_buf_t *ps_curr_inp =
                        ps_multi_thrd->aps_curr_inp_pre_enc[i4_cur_ipe_idx];
                    pre_enc_me_ctxt_t *ps_curr_out =
                        ps_multi_thrd->aps_curr_out_pre_enc[i4_cur_ipe_idx];

                    /* set the flag saying de init is done so that other cores dont do it */
                    ps_multi_thrd->ai4_pre_enc_deinit_done[i4_cur_ipe_idx] = 1;

                    if(1 == ps_curr_out->i4_frm_proc_valid_flag)
                    {
                        LWORD64 frame_acc_satd_by_modqp;
                        float L1_full_processed_ratio;

                        if(ps_curr_inp->s_rc_lap_out.i8_satd_by_act_L1_accum_evaluated)
                        {
                            L1_full_processed_ratio =
                                ((float)ps_curr_inp->s_rc_lap_out.i8_frame_satd_by_act_L1_accum /
                                 ps_curr_inp->s_rc_lap_out.i8_satd_by_act_L1_accum_evaluated);
                        }
                        else
                        {
                            L1_full_processed_ratio = 1.0;
                        }
                        /* Get frame-level satd cost and mode bit cost from IPE */
                        ps_curr_out->i8_frame_acc_satd_cost = ihevce_ipe_get_frame_intra_satd_cost(
                            ps_enc_ctxt->s_module_ctxt.pv_ipe_ctxt,
                            &frame_acc_satd_by_modqp,
                            &ps_curr_inp->s_rc_lap_out.i8_est_I_pic_header_bits,
                            &ps_curr_inp->s_lap_out.i8_frame_level_activity_fact,
                            &ps_curr_inp->s_lap_out.i8_frame_l0_acc_satd);

                        if((ps_curr_inp->s_lap_out.i4_quality_preset == IHEVCE_QUALITY_P6) &&
                           (ps_curr_inp->s_lap_out.i4_temporal_lyr_id > TEMPORAL_LAYER_DISABLE))
                        {
                            ps_curr_inp->s_rc_lap_out.i8_est_I_pic_header_bits = -1;
                        }

                        {
                            WORD32 i4_cur_q_scale = (ps_enc_ctxt->s_rc_quant.pi4_qp_to_qscale
                                                         [ps_enc_ctxt->s_multi_thrd.i4_rc_l0_qp +
                                                          ps_enc_ctxt->s_rc_quant.i1_qp_offset] +
                                                     (1 << (QSCALE_Q_FAC_3 - 1))) >>
                                                    QSCALE_Q_FAC_3;

                            /* calculate satd/act_fac = satd/qm * (qp_used_at_L0_analysis) */
                            ps_curr_inp->s_rc_lap_out.i8_frame_satd_act_accum =
                                frame_acc_satd_by_modqp * i4_cur_q_scale;
                        }

                        /* Because of early intra inter decision, L0 intra analysis might not happen for entire frame, correct the error
                           based on L1 data */
                        ps_curr_inp->s_rc_lap_out.i8_est_I_pic_header_bits = (LWORD64)(
                            ps_curr_inp->s_rc_lap_out.i8_est_I_pic_header_bits *
                            L1_full_processed_ratio);

                        if(L1_full_processed_ratio < 1.5)
                        {
                            ps_curr_inp->s_rc_lap_out.i8_frame_satd_act_accum = (LWORD64)(
                                ps_curr_inp->s_rc_lap_out.i8_frame_satd_act_accum *
                                L1_full_processed_ratio);
                        }
                        else
                        {
                            /* This is the case when too many candidates would not have gone through intra analysis, scaling based on L1 is found to be inappropriate,
                            Hence directly estimating L0 satd from L1 satd */
                            ps_curr_inp->s_rc_lap_out.i8_frame_satd_act_accum =
                                ps_curr_inp->s_rc_lap_out.i8_frm_satd_act_accum_L0_frm_L1;
                        }
                    }

                    /* register the current input buffer to be cnosumed by encode group threads */
                    ps_curr_out->curr_inp_buf_id =
                        ps_multi_thrd->ai4_in_buf_id_pre_enc[i4_cur_ipe_idx];
                    ps_curr_out->ps_curr_inp = ps_curr_inp;

                    /* set the output buffer as produced */
                    ihevce_q_set_buff_prod(
                        (void *)ps_enc_ctxt,
                        IHEVCE_PRE_ENC_ME_Q,
                        ps_multi_thrd->ai4_out_buf_id_pre_enc[i4_cur_ipe_idx]);

                    /* set the output buffer of L0 IPE as produced */
                    ihevce_q_set_buff_prod(
                        (void *)ps_enc_ctxt,
                        IHEVCE_L0_IPE_ENC_Q,
                        ps_multi_thrd->i4_L0_IPE_out_buf_id);

                    /* update flag indicating ipe is done */
                    ihevce_dmgr_update_frm_frm_sync(pv_dep_mngr_prev_frame_pre_enc_l0);
                }

                {
                    /* index increment */
                    i4_cur_ipe_idx = i4_cur_ipe_idx + 1;

                    /* wrap around case */
                    if(i4_cur_ipe_idx >= ps_multi_thrd->i4_max_delay_pre_me_btw_l0_ipe)
                    {
                        i4_cur_ipe_idx = 0;
                    }

                    i4_num_buf_prod_for_l0_ipe--;
                }
                /*NOTE: update of above indices should mark end if ipe.do not access below this*/

                /****** UnLock the critical section after deinit ******/
                {
                    WORD32 i4_status;
                    i4_status = osal_mutex_unlock(ps_multi_thrd->pv_mutex_hdl_pre_enc_deinit);

                    if(OSAL_SUCCESS != i4_status)
                        return 0;
                }

                if(1 == ps_multi_thrd->i4_force_end_flag)
                {
                    i4_end_flag = 1;
                    break;
                }
            } while((i4_end_flag || i4_out_flush_flag) && i4_num_buf_prod_for_l0_ipe);
        }
        if(i4_thrd_id == 0)
        {
            PROFILE_STOP(&ps_hle_ctxt->profile_pre_enc_l0ipe[i4_resolution_id], NULL);
        }
    }

    return 0;
}

void calc_l1_level_hme_intra_sad_different_qp(
    enc_ctxt_t *ps_enc_ctxt,
    pre_enc_me_ctxt_t *ps_curr_out,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    WORD32 i4_tot_ctb_l1_x,
    WORD32 i4_tot_ctb_l1_y)
{
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;
    WORD32 i4_qp_counter, i4_qp_start = 0, i4_qp_end = 0, i, i4_j, i4_new_frame_qp;
    LWORD64 i8_l1_intra_sad_nc_accounted = 0, cur_intra_sad, raw_hme_sad = 0;
    LWORD64 cur_hme_sad = 0, cur_hme_sad_for_offset = 0, acc_hme_l1_sad = 0,
            acc_hme_l1_sad_for_offset = 0;
    i4_qp_start = 1;
    i4_qp_end = 51;

    for(i4_qp_counter = i4_qp_start; i4_qp_counter <= i4_qp_end; i4_qp_counter = i4_qp_counter + 3)
    {
        i8_l1_intra_sad_nc_accounted = 0;
        cur_intra_sad = 0;
        raw_hme_sad = 0;
        cur_hme_sad = 0;
        cur_hme_sad_for_offset = 0;
        acc_hme_l1_sad = 0;
        ps_ed_ctb_l1 = ps_curr_out->ps_ed_ctb_l1;
        i4_new_frame_qp = i4_qp_counter;
        acc_hme_l1_sad = 0;

        for(i = 0; i < (i4_tot_ctb_l1_x * i4_tot_ctb_l1_y); i += 1)
        {
            for(i4_j = 0; i4_j < 16; i4_j++)
            {
                if(ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_j] != -1)
                {
                    ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_j] >= 0);
                    if(ps_curr_inp->s_rc_lap_out.i4_rc_pic_type != IV_I_FRAME &&
                       ps_curr_inp->s_rc_lap_out.i4_rc_pic_type != IV_IDR_FRAME)
                    {
                        /*When l1 is disabled for B pics i4_best_sad_8x8_l1_ipe is set to max value always,
                        so will enter this path even for incomplete ctb, hence the assert holdsto good only for P pic  */
                        if(ps_curr_inp->s_rc_lap_out.i4_rc_quality_preset == IHEVCE_QUALITY_P6)
                        {
                            if(ps_curr_inp->s_rc_lap_out.i4_rc_pic_type == IV_P_FRAME)
                            {
                                ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i4_j] >= 0);
                                ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_me_for_decide[i4_j] >= 0);
                            }
                        }
                        else
                        {
                            ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i4_j] >= 0);
                            ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_me_for_decide[i4_j] >= 0);
                        }

#if 1  //DISABLE_L1_L2_IPE_INTRA_IN_BPICS && RC_DEPENDENCY_FOR_BPIC
                        if((ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i4_j] != -1))
#endif
                        {
                            cur_hme_sad = ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i4_j] -
                                          (QP2QUANT_MD[i4_new_frame_qp] << 3);
                        }
                        raw_hme_sad += ps_ed_ctb_l1->i4_best_sad_8x8_l1_me[i4_j];

                        if(cur_hme_sad > 0)
                            acc_hme_l1_sad += cur_hme_sad;
                    }
                    if(cur_hme_sad_for_offset > 0)
                    {
                        acc_hme_l1_sad_for_offset += cur_hme_sad_for_offset;
                    }
                    ASSERT(ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_j] >= 0);
                    /*intra sad is scaled by 1.17 to be account for 1/3 vs 1/6th rounding*/
                    cur_intra_sad = (LWORD64)(
                        (ps_ed_ctb_l1->i4_best_sad_8x8_l1_ipe[i4_j] * 1.17) -
                        (QP2QUANT_MD[i4_new_frame_qp] << 3));

                    if(cur_intra_sad > 0)
                        i8_l1_intra_sad_nc_accounted += cur_intra_sad;
                }
            }
            ps_ed_ctb_l1 += 1;
        }
        if((ps_curr_inp->s_rc_lap_out.i4_rc_quality_preset == IHEVCE_QUALITY_P6) &&
           (ps_curr_inp->s_rc_lap_out.i4_rc_pic_type == IV_B_FRAME))
        {
            ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad[i4_qp_counter] = -1;
            ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad[i4_qp_counter + 1] = -1;
            ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad[i4_qp_counter + 2] = -1;
        }
        else
        {
            ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad[i4_qp_counter] =
                i8_l1_intra_sad_nc_accounted;
            ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad[i4_qp_counter + 1] =
                i8_l1_intra_sad_nc_accounted;
            ps_curr_inp->s_rc_lap_out.ai8_pre_intra_sad[i4_qp_counter + 2] =
                i8_l1_intra_sad_nc_accounted;
        }
        ps_curr_inp->s_rc_lap_out.ai8_frame_acc_coarse_me_sad[i4_qp_counter] = acc_hme_l1_sad;
        ps_curr_inp->s_rc_lap_out.ai8_frame_acc_coarse_me_sad[i4_qp_counter + 1] = acc_hme_l1_sad;
        ps_curr_inp->s_rc_lap_out.ai8_frame_acc_coarse_me_sad[i4_qp_counter + 2] = acc_hme_l1_sad;
        ps_curr_inp->s_rc_lap_out.i8_raw_l1_coarse_me_sad = raw_hme_sad;
    }
}
