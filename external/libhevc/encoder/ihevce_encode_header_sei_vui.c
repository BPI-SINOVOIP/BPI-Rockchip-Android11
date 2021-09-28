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
* @file ihevce_encode_header_sei_vui.c
*
* @brief
*   This file contains function definitions related to header vui/sei encoding
*
* @author
*   ittiam
*
* List of Functions
*   ihevce_generate_sub_layer_hrd_params()
*   ihevce_generate_hrd_params()
*   ihevce_generate_vui()
*   ihevce_put_buf_period_sei_params()
*   ihevce_put_active_parameter_set_sei_params()
*   ihevce_put_mastering_disp_col_vol_sei_params()
*   ihevce_put_mastering_disp_col_vol_sei_params()
*   ihevce_put_sei_params()
*   ihevce_put_cll_info_sei_params()
*   ihevce_put_recovery_point_sei_params()
*   ihevce_put_pic_timing_sei_params()
*   ihevce_put_hash_sei_params()
*   ihevce_put_sei_msg()
*   ihevce_generate_sei()
*   ihevce_populate_mastering_disp_col_vol_sei()
*   ihevce_populate_recovery_point_sei()
*   ihevce_populate_picture_timing_sei()
*   ihevce_populate_buffering_period_sei()
*   ihevce_populate_active_parameter_set_sei()
*   ihevce_calc_CRC()
*   ihevce_calc_checksum()
*   ihevce_populate_hash_sei()
*   ihevce_populate_vui()
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

#include "ihevc_defs.h"
#include "ihevc_macros.h"
#include "ihevc_debug.h"
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
#include "ihevc_trans_tables.h"
#include "ihevc_trans_macros.h"

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_error_checks.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_encode_header.h"
#include "ihevce_encode_header_sei_vui.h"
#include "ihevce_trace.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Generates sub-layer  hrd parameters of VUI (Video Usability Info  Set)
*
*  @par   Description
*  Parse Video Usability Info as per ANNEX E.2
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_hrd_params
*  pointer to structure containing sub-layer hrd params of VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_sub_layer_hrd_params(
    bitstrm_t *ps_bitstrm,
    sub_lyr_hrd_params_t *ps_sub_lyr_hrd_params,
    hrd_params_t *ps_hrd_params,
    WORD32 cpb_cnt_minus1)
{
    WORD32 j;
    WORD32 return_status = IHEVCE_SUCCESS;

    for(j = 0; j <= cpb_cnt_minus1; j++)
    {
        /* au4_bit_rate_value_minus1 */
        PUT_BITS_UEV(
            ps_bitstrm, ps_sub_lyr_hrd_params->au4_bit_rate_value_minus1[j], return_status);
        ENTROPY_TRACE("bit_rate_value_minus1", ps_sub_lyr_hrd_params->au4_bit_rate_value_minus1[j]);

        /* au4_cpb_size_value_minus1 */
        PUT_BITS_UEV(
            ps_bitstrm, ps_sub_lyr_hrd_params->au4_cpb_size_value_minus1[j], return_status);
        ENTROPY_TRACE("cpb_size_value_minus1", ps_sub_lyr_hrd_params->au4_cpb_size_value_minus1[j]);

        if(ps_hrd_params->u1_sub_pic_cpb_params_present_flag)
        {
            /* au4_cpb_size_du_value_minus1 */
            PUT_BITS_UEV(
                ps_bitstrm, ps_sub_lyr_hrd_params->au4_cpb_size_du_value_minus1[j], return_status);
            ENTROPY_TRACE(
                "cpb_size_du_value_minus1", ps_sub_lyr_hrd_params->au4_cpb_size_du_value_minus1[j]);

            /* au4_bit_rate_du_value_minus1 */
            PUT_BITS_UEV(
                ps_bitstrm, ps_sub_lyr_hrd_params->au4_bit_rate_du_value_minus1[j], return_status);
            ENTROPY_TRACE(
                "bit_rate_du_value_minus1", ps_sub_lyr_hrd_params->au4_bit_rate_du_value_minus1[j]);
        }

        /* au1_cbr_flag */
        PUT_BITS(ps_bitstrm, ps_sub_lyr_hrd_params->au1_cbr_flag[j], 1, return_status);
        ENTROPY_TRACE("cbr_flag", ps_sub_lyr_hrd_params->au1_cbr_flag[j]);
    }
    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates hrd parameters of VUI (Video Usability Info  Set)
*
*  @par   Description
*  Parse Video Usability Info as per ANNEX E.2
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sps
*  pointer to structure containing SPS data
*
*  @param[in]   ps_hrd_params
*  pointer to structure containing hrd params of VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_hrd_params(bitstrm_t *ps_bitstrm, hrd_params_t *ps_hrd_params, sps_t *ps_sps)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_common_info_present_flag = 1;

    if(u1_common_info_present_flag)
    {
        /* u1_nal_hrd_parameters_present_flag */
        PUT_BITS(ps_bitstrm, ps_hrd_params->u1_nal_hrd_parameters_present_flag, 1, return_status);
        ENTROPY_TRACE(
            "nal_hrd_parameters_present_flag", ps_hrd_params->u1_nal_hrd_parameters_present_flag);

        /* u1_vcl_hrd_parameters_present_flag */
        PUT_BITS(ps_bitstrm, ps_hrd_params->u1_vcl_hrd_parameters_present_flag, 1, return_status);
        ENTROPY_TRACE(
            "vcl_hrd_parameters_present_flag", ps_hrd_params->u1_vcl_hrd_parameters_present_flag);

        if(ps_hrd_params->u1_vcl_hrd_parameters_present_flag ||
           ps_hrd_params->u1_nal_hrd_parameters_present_flag)
        {
            /* u1_sub_pic_cpb_params_present_flag */
            PUT_BITS(
                ps_bitstrm, ps_hrd_params->u1_sub_pic_cpb_params_present_flag, 1, return_status);
            ENTROPY_TRACE(
                "sub_pic_Cpb_params_present_flag",
                ps_hrd_params->u1_sub_pic_cpb_params_present_flag);

            if(ps_hrd_params->u1_sub_pic_cpb_params_present_flag)
            {
                /* u1_tick_divisor_minus2 */
                PUT_BITS(ps_bitstrm, ps_hrd_params->u1_tick_divisor_minus2, 8, return_status);
                ENTROPY_TRACE("tick_divisor_minus2", ps_hrd_params->u1_tick_divisor_minus2);

                /* u1_du_cpb_removal_delay_increment_length_minus1 */
                PUT_BITS(
                    ps_bitstrm,
                    ps_hrd_params->u1_du_cpb_removal_delay_increment_length_minus1,
                    5,
                    return_status);
                ENTROPY_TRACE(
                    "du_cpb_removal_delay_increment_length_minus1",
                    ps_hrd_params->u1_du_cpb_removal_delay_increment_length_minus1);

                /* u1_sub_pic_cpb_params_in_pic_timing_sei_flag */
                PUT_BITS(
                    ps_bitstrm,
                    ps_hrd_params->u1_sub_pic_cpb_params_in_pic_timing_sei_flag,
                    1,
                    return_status);
                ENTROPY_TRACE(
                    "sub_pic_cpb_params_in_pic_timing_sei_flag",
                    ps_hrd_params->u1_sub_pic_cpb_params_in_pic_timing_sei_flag);

                /* u1_dpb_output_delay_du_length_minus1 */
                PUT_BITS(
                    ps_bitstrm,
                    ps_hrd_params->u1_dpb_output_delay_du_length_minus1,
                    5,
                    return_status);
                ENTROPY_TRACE(
                    "dpb_output_delay_du_length_minus1",
                    ps_hrd_params->u1_dpb_output_delay_du_length_minus1);
            }

            /* u4_bit_rate_scale */
            PUT_BITS(ps_bitstrm, ps_hrd_params->u4_bit_rate_scale, 4, return_status);
            ENTROPY_TRACE("bit_rate_scale", ps_hrd_params->u4_bit_rate_scale);

            /* u4_cpb_size_scale */
            PUT_BITS(ps_bitstrm, ps_hrd_params->u4_cpb_size_scale, 4, return_status);
            ENTROPY_TRACE("cpb_size_scale", ps_hrd_params->u4_cpb_size_scale);

            if(ps_hrd_params->u1_sub_pic_cpb_params_present_flag)
            {
                /* u4_cpb_size_du_scale */
                PUT_BITS(ps_bitstrm, ps_hrd_params->u4_cpb_size_du_scale, 4, return_status);
                ENTROPY_TRACE("cpb_size_du_scale", ps_hrd_params->u4_cpb_size_du_scale);
            }

            /* u1_initial_cpb_removal_delay_length_minus1 */
            PUT_BITS(
                ps_bitstrm,
                ps_hrd_params->u1_initial_cpb_removal_delay_length_minus1,
                5,
                return_status);
            ENTROPY_TRACE(
                "initial_cpb_removal_delay_length_minus1",
                ps_hrd_params->u1_initial_cpb_removal_delay_length_minus1);

            /* u1_au_cpb_removal_delay_length_minus1 */
            PUT_BITS(
                ps_bitstrm, ps_hrd_params->u1_au_cpb_removal_delay_length_minus1, 5, return_status);
            ENTROPY_TRACE(
                "cpb_removal_delay_length_minus1",
                ps_hrd_params->u1_au_cpb_removal_delay_length_minus1);

            /* u1_dpb_output_delay_length_minus1 */
            PUT_BITS(
                ps_bitstrm, ps_hrd_params->u1_dpb_output_delay_length_minus1, 5, return_status);
            ENTROPY_TRACE(
                "dpb_output_delay_length_minus1", ps_hrd_params->u1_dpb_output_delay_length_minus1);
        }
    }

    for(i = 0; i < (ps_sps->i1_sps_max_sub_layers); i++)
    {
        /* au1_fixed_pic_rate_general_flag */
        PUT_BITS(ps_bitstrm, ps_hrd_params->au1_fixed_pic_rate_general_flag[i], 1, return_status);
        ENTROPY_TRACE(
            "fixed_pic_rate_general_flag", ps_hrd_params->au1_fixed_pic_rate_general_flag[i]);

        if(!ps_hrd_params->au1_fixed_pic_rate_general_flag[i])
        {
            /* au1_fixed_pic_rate_within_cvs_flag */
            PUT_BITS(
                ps_bitstrm, ps_hrd_params->au1_fixed_pic_rate_within_cvs_flag[i], 1, return_status);
            ENTROPY_TRACE(
                "fixed_pic_rate_within_cvs_flag",
                ps_hrd_params->au1_fixed_pic_rate_within_cvs_flag[i]);
        }

        if(ps_hrd_params->au1_fixed_pic_rate_within_cvs_flag[i])
        {
            /* au2_elemental_duration_in_tc_minus1 */
            PUT_BITS_UEV(
                ps_bitstrm, ps_hrd_params->au2_elemental_duration_in_tc_minus1[i], return_status);
            ENTROPY_TRACE(
                "elemental_duration_in_tc_minus1",
                ps_hrd_params->au2_elemental_duration_in_tc_minus1[i]);
        }
        else
        {
            /* au1_low_delay_hrd_flag */
            PUT_BITS(ps_bitstrm, ps_hrd_params->au1_low_delay_hrd_flag[i], 1, return_status);
            ENTROPY_TRACE("low_delay_hrd_flag", ps_hrd_params->au1_low_delay_hrd_flag[i]);
        }

        if(!ps_hrd_params->au1_low_delay_hrd_flag[i])
        {
            /* au1_cpb_cnt_minus1 */
            PUT_BITS_UEV(ps_bitstrm, ps_hrd_params->au1_cpb_cnt_minus1[i], return_status);
            ENTROPY_TRACE("cpb_cnt_minus1", ps_hrd_params->au1_cpb_cnt_minus1[i]);
        }

        if(ps_hrd_params->u1_nal_hrd_parameters_present_flag)
        {
            sub_lyr_hrd_params_t *ps_sub_lyr_hrd_params =
                &ps_hrd_params->as_sub_layer_hrd_params[i];
            return_status |= ihevce_generate_sub_layer_hrd_params(
                ps_bitstrm,
                ps_sub_lyr_hrd_params,
                ps_hrd_params,
                ps_hrd_params->au1_cpb_cnt_minus1[i]);
        }

        if(ps_hrd_params->u1_vcl_hrd_parameters_present_flag)
        {
            sub_lyr_hrd_params_t *ps_sub_lyr_hrd_params =
                &ps_hrd_params->as_sub_layer_hrd_params[i];
            return_status |= ihevce_generate_sub_layer_hrd_params(
                ps_bitstrm,
                ps_sub_lyr_hrd_params,
                ps_hrd_params,
                ps_hrd_params->au1_cpb_cnt_minus1[i]);
        }
    }

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates VUI (Video Usability Info  Set)
*
*  @par   Description
*  Parse Video Usability Info as per ANNEX E.2
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sps
*  pointer to structure containing SPS data
*
*  @param[in]   ps_vui
*  pointer to structure containing VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_vui(bitstrm_t *ps_bitstrm, sps_t *ps_sps, vui_t s_vui)
{
    WORD32 return_status = IHEVCE_SUCCESS;

    /* aspect_ratio_info_present_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_aspect_ratio_info_present_flag, 1, return_status);
    ENTROPY_TRACE("aspect_ratio_info_present_flag", s_vui.u1_aspect_ratio_info_present_flag);

    if(s_vui.u1_aspect_ratio_info_present_flag)
    {
        /* aspect_ratio_idc */
        PUT_BITS(ps_bitstrm, s_vui.u1_aspect_ratio_idc, 8, return_status);
        ENTROPY_TRACE("aspect_ratio_idc", s_vui.u1_aspect_ratio_idc);
        if(s_vui.u1_aspect_ratio_idc == IHEVCE_EXTENDED_SAR)
        {
            /* SAR_width */
            PUT_BITS(ps_bitstrm, s_vui.u2_sar_width, 16, return_status);
            ENTROPY_TRACE("sar_width", s_vui.u2_sar_width);

            /* SAR_hieght */
            PUT_BITS(ps_bitstrm, s_vui.u2_sar_height, 16, return_status);
            ENTROPY_TRACE("sar_height", s_vui.u2_sar_height);
        }
    }

    /* overscan_info_present_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_overscan_info_present_flag, 1, return_status);
    ENTROPY_TRACE("overscan_info_present_flag", s_vui.u1_overscan_info_present_flag);

    if(s_vui.u1_overscan_info_present_flag)
    {
        /* u1_overscan_appropriate_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_overscan_appropriate_flag, 1, return_status);
        ENTROPY_TRACE("overscan_appropriate_flag", s_vui.u1_overscan_appropriate_flag);
    }

    /* video_signal_type_present_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_video_signal_type_present_flag, 1, return_status);
    ENTROPY_TRACE("video_signal_type_present_flag", s_vui.u1_video_signal_type_present_flag);

    if(s_vui.u1_video_signal_type_present_flag)
    {
        /* u1_video_format */
        PUT_BITS(ps_bitstrm, s_vui.u1_video_format, 3, return_status);
        ENTROPY_TRACE("video_format", s_vui.u1_video_format);

        /* u1_video_full_range_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_video_full_range_flag, 1, return_status);
        ENTROPY_TRACE("video_full_range_flag", s_vui.u1_video_full_range_flag);

        /* u1_colour_description_present_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_colour_description_present_flag, 1, return_status);
        ENTROPY_TRACE("colour_description_present_flag", s_vui.u1_colour_description_present_flag);

        if(s_vui.u1_colour_description_present_flag)
        {
            /* u1_colour_primaries */
            PUT_BITS(ps_bitstrm, s_vui.u1_colour_primaries, 8, return_status);
            ENTROPY_TRACE("colour_primaries", s_vui.u1_colour_primaries);

            /* u1_transfer_characteristics */
            PUT_BITS(ps_bitstrm, s_vui.u1_transfer_characteristics, 8, return_status);
            ENTROPY_TRACE("transfer_characteristics", s_vui.u1_transfer_characteristics);

            /* u1_matrix_coefficients */
            PUT_BITS(ps_bitstrm, s_vui.u1_matrix_coefficients, 8, return_status);
            ENTROPY_TRACE("matrix_coefficients", s_vui.u1_matrix_coefficients);
        }
    }

    /* u1_chroma_loc_info_present_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_chroma_loc_info_present_flag, 1, return_status);
    ENTROPY_TRACE("chroma_loc_info_present_flag", s_vui.u1_chroma_loc_info_present_flag);

    if(s_vui.u1_chroma_loc_info_present_flag)
    {
        /* u1_chroma_sample_loc_type_top_field */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u1_chroma_sample_loc_type_top_field, return_status);
        ENTROPY_TRACE(
            "chroma_sample_loc_type_top_field", s_vui.u1_chroma_sample_loc_type_top_field);

        /* u1_chroma_sample_loc_type_bottom_field */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u1_chroma_sample_loc_type_bottom_field, return_status);
        ENTROPY_TRACE(
            "chroma_sample_loc_type_bottom_field", s_vui.u1_chroma_sample_loc_type_bottom_field);
    }

    /* u1_neutral_chroma_indication_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_neutral_chroma_indication_flag, 1, return_status);
    ENTROPY_TRACE("neutral_chroma_indication_flag", s_vui.u1_neutral_chroma_indication_flag);

    /* u1_field_seq_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_field_seq_flag, 1, return_status);
    ENTROPY_TRACE("field_seq_flag", s_vui.u1_field_seq_flag);

    /* HM CURRENTLY NOT SUPPOSTED */
    /* u1_frame_field_info_present_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_frame_field_info_present_flag, 1, return_status);
    ENTROPY_TRACE("frame_field_info_present_flag", s_vui.u1_frame_field_info_present_flag);

    /* u1_default_display_window_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_default_display_window_flag, 1, return_status);
    ENTROPY_TRACE("default_display_window_flag", s_vui.u1_default_display_window_flag);

    if(s_vui.u1_default_display_window_flag)
    {
        /* u4_def_disp_win_left_offset */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u4_def_disp_win_left_offset, return_status);
        ENTROPY_TRACE("def_disp_win_left_offset", s_vui.u4_def_disp_win_left_offset);

        /* u4_def_disp_win_right_offset */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u4_def_disp_win_right_offset, return_status);
        ENTROPY_TRACE("def_disp_win_right_offset", s_vui.u4_def_disp_win_right_offset);

        /* u4_def_disp_win_top_offset */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u4_def_disp_win_top_offset, return_status);
        ENTROPY_TRACE("def_disp_win_top_offset", s_vui.u4_def_disp_win_top_offset);

        /* u4_def_disp_win_bottom_offset */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u4_def_disp_win_bottom_offset, return_status);
        ENTROPY_TRACE("def_disp_win_bottom_offset", s_vui.u4_def_disp_win_bottom_offset);
    }

    /* u1_vui_timing_info_present_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_vui_timing_info_present_flag, 1, return_status);
    ENTROPY_TRACE("vui_timing_info_present_flag", s_vui.u1_vui_timing_info_present_flag);

    if(s_vui.u1_vui_timing_info_present_flag)
    {
        /* u4_num_units_in_tick */
        PUT_BITS(ps_bitstrm, s_vui.u4_vui_num_units_in_tick, 32, return_status);
        ENTROPY_TRACE("num_units_in_tick", s_vui.u4_vui_num_units_in_tick);

        /* u4_time_scale */
        PUT_BITS(ps_bitstrm, s_vui.u4_vui_time_scale, 32, return_status);
        ENTROPY_TRACE("time_scale", s_vui.u4_vui_time_scale);

        /* u1_poc_proportional_to_timing_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_poc_proportional_to_timing_flag, 1, return_status);
        ENTROPY_TRACE("poc_proportional_to_timing_flag", s_vui.u1_poc_proportional_to_timing_flag);

        /* u1_num_ticks_poc_diff_one_minus1 */
        if(s_vui.u1_poc_proportional_to_timing_flag)
        {
            PUT_BITS_UEV(ps_bitstrm, s_vui.u4_num_ticks_poc_diff_one_minus1, return_status);
            ENTROPY_TRACE("num_ticks_poc_diff_one_minus1", s_vui.u4_num_ticks_poc_diff_one_minus1);
        }

        /* u1_vui_hrd_parameters_present_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_vui_hrd_parameters_present_flag, 1, return_status);
        ENTROPY_TRACE("vui_hrd_parameters_present_flag", s_vui.u1_vui_hrd_parameters_present_flag);

        if(s_vui.u1_vui_hrd_parameters_present_flag)
        {
            return_status |=
                ihevce_generate_hrd_params(ps_bitstrm, &(s_vui.s_vui_hrd_parameters), ps_sps);
        }
    }

    /* u1_bitstream_restriction_flag */
    PUT_BITS(ps_bitstrm, s_vui.u1_bitstream_restriction_flag, 1, return_status);
    ENTROPY_TRACE("bitstream_restriction_flag", s_vui.u1_bitstream_restriction_flag);

    if(s_vui.u1_bitstream_restriction_flag)
    {
        /* u1_tiles_fixed_structure_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_tiles_fixed_structure_flag, 1, return_status);
        ENTROPY_TRACE("tiles_fixed_structure_flag", s_vui.u1_tiles_fixed_structure_flag);

        /* u1_motion_vectors_over_pic_boundaries_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_motion_vectors_over_pic_boundaries_flag, 1, return_status);
        ENTROPY_TRACE(
            "motion_vectors_over_pic_boundaries_flag",
            s_vui.u1_motion_vectors_over_pic_boundaries_flag);

        /* u1_restricted_ref_pic_lists_flag */
        PUT_BITS(ps_bitstrm, s_vui.u1_restricted_ref_pic_lists_flag, 1, return_status);
        ENTROPY_TRACE("restricted_ref_pic_lists_flag", s_vui.u1_restricted_ref_pic_lists_flag);

        /* u4_min_spatial_segmentation_idc */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u4_min_spatial_segmentation_idc, return_status);
        ENTROPY_TRACE("min_spatial_segmentation_idc", s_vui.u4_min_spatial_segmentation_idc);

        /* u1_max_bytes_per_pic_denom */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u1_max_bytes_per_pic_denom, return_status);
        ENTROPY_TRACE("max_bytes_per_pic_denom", s_vui.u1_max_bytes_per_pic_denom);

        /* u1_max_bits_per_mincu_denom */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u1_max_bits_per_mincu_denom, return_status);
        ENTROPY_TRACE("max_bits_per_mincu_denom", s_vui.u1_max_bits_per_mincu_denom);

        /* u1_log2_max_mv_length_horizontal */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u1_log2_max_mv_length_horizontal, return_status);
        ENTROPY_TRACE("log2_max_mv_length_horizontal", s_vui.u1_log2_max_mv_length_horizontal);

        /* u1_log2_max_mv_length_vertical */
        PUT_BITS_UEV(ps_bitstrm, s_vui.u1_log2_max_mv_length_vertical, return_status);
        ENTROPY_TRACE("log2_max_mv_length_vertical", s_vui.u1_log2_max_mv_length_vertical);
    }
    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates Buffering Period (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_pt_sei
*  pointer to structure containing buffering period SEI data
*
*  @param[in]   ps_vui_params
*  pointer to structure containing VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_buf_period_sei_params(
    buf_period_sei_params_t *ps_bp_sei, vui_t *ps_vui_params, bitstrm_t *ps_bitstrm)
{
    UWORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_payload_size = 0;
    UWORD8 u1_sub_pic_cpb_params_present_flag =
        ps_vui_params->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag;

    {
        //UWORD32 range;
        //UWORD32 sps_id = ps_bp_sei->u1_bp_seq_parameter_set_id;

        //range = 0;
        //GETRANGE(range, sps_id);
        u1_payload_size += 1;  //(((range - 1) << 1) + 1);

        if(!u1_sub_pic_cpb_params_present_flag)
            u1_payload_size += 1;
        u1_payload_size += 1;
        u1_payload_size +=
            ps_vui_params->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 + 1;
        if(1 == ps_vui_params->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag)
        {
            for(i = 0; i <= ps_bp_sei->u4_cpb_cnt; i++)
            {
                u1_payload_size += ps_bp_sei->u4_initial_cpb_removal_delay_length << 1;
                if(u1_sub_pic_cpb_params_present_flag || ps_bp_sei->u1_rap_cpb_params_present_flag)
                    u1_payload_size += ps_bp_sei->u4_initial_cpb_removal_delay_length << 1;
            }
        }
        if(1 == ps_vui_params->s_vui_hrd_parameters.u1_vcl_hrd_parameters_present_flag)
        {
            for(i = 0; i <= ps_bp_sei->u4_cpb_cnt; i++)
            {
                u1_payload_size += ps_bp_sei->u4_initial_cpb_removal_delay_length << 1;
                if(u1_sub_pic_cpb_params_present_flag || ps_bp_sei->u1_rap_cpb_params_present_flag)
                    u1_payload_size += ps_bp_sei->u4_initial_cpb_removal_delay_length << 1;
            }
        }
    }

    u1_payload_size = (u1_payload_size + 7) >> 3;

    /************************************************************************************************/
    /* Calculating the cbp removal delay and cbp removal delay offset based on the                  */
    /* buffer level information from Rate control                                                   */
    /* NOTE : Buffer fullness level for Rate control is updated using Approximate                   */
    /* number of bits from RDOPT stage rather than from accurate number of bits from ENTROPY coding */
    /************************************************************************************************/

    {
        ULWORD64 u8_temp;
        UWORD32 u4_buffer_size, u4_dbf;

        u4_buffer_size = ps_bp_sei->u4_buffer_size_sei;
        u4_dbf = ps_bp_sei->u4_dbf_sei;
        for(i = 0; i <= ps_bp_sei->u4_cpb_cnt; i++)
        {
            u8_temp = ((ULWORD64)(u4_dbf)*90000);
            u8_temp = u8_temp / ps_bp_sei->u4_target_bit_rate_sei;
            ps_bp_sei->au4_nal_initial_cpb_removal_delay[i] = (UWORD32)u8_temp;

            u8_temp = ((ULWORD64)(u4_buffer_size - u4_dbf) * 90000);
            u8_temp = u8_temp / ps_bp_sei->u4_target_bit_rate_sei;
            ps_bp_sei->au4_nal_initial_cpb_removal_delay_offset[i] = (UWORD32)u8_temp;

            if(ps_bp_sei->au4_nal_initial_cpb_removal_delay[i] < 1)
                ps_bp_sei->au4_nal_initial_cpb_removal_delay[i] =
                    1; /* ps_bp_sei->au4_nal_initial_cpb_removal_delay[i] should be always greater than 0 */
        }
    }

    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("payload_size", u1_payload_size);

    /************************************************************************/
    /* Put the buffering period SEI parameters into the bitstream. For      */
    /* details refer to section D.1.1 of the standard                       */
    /************************************************************************/

    /* seq_parameter_set_id */
    PUT_BITS_UEV(ps_bitstrm, ps_bp_sei->u1_bp_seq_parameter_set_id, return_status);
    ENTROPY_TRACE("seq_parameter_set_id", ps_bp_sei->u1_bp_seq_parameter_set_id);

    //PUT_BITS(ps_bitstrm, u1_sub_pic_cpb_params_present_flag, 1, return_status);
    //ENTROPY_TRACE("sub_pic_cpb_params_present_flag", u1_sub_pic_cpb_params_present_flag);

    if(!u1_sub_pic_cpb_params_present_flag)
    {
        /* u1_rap_cpb_params_present_flag */
        PUT_BITS(ps_bitstrm, ps_bp_sei->u1_rap_cpb_params_present_flag, 1, return_status);
        ENTROPY_TRACE("rap_cpb_params_present_flag", ps_bp_sei->u1_rap_cpb_params_present_flag);
    }

    if(ps_bp_sei->u1_rap_cpb_params_present_flag)
    {
        PUT_BITS(
            ps_bitstrm,
            ps_bp_sei->u4_cpb_delay_offset,
            (ps_vui_params->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 + 1),
            return_status);
        ENTROPY_TRACE("cpb_delay_offset", ps_bp_sei->cpb_delay_offset);

        PUT_BITS(
            ps_bitstrm,
            ps_bp_sei->u4_dpb_delay_offset,
            (ps_vui_params->s_vui_hrd_parameters.u1_dpb_output_delay_length_minus1 + 1),
            return_status);
        ENTROPY_TRACE("dpb_delay_offset", ps_bp_sei->dpb_delay_offset);
    }

    PUT_BITS(ps_bitstrm, ps_bp_sei->u1_concatenation_flag, 1, return_status);
    ENTROPY_TRACE("concatenation_flag", ps_bp_sei->concatenation_flag);

    PUT_BITS(
        ps_bitstrm,
        ps_bp_sei->u4_au_cpb_removal_delay_delta_minus1,
        (ps_vui_params->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 + 1),
        return_status);

    ENTROPY_TRACE(
        "au_cpb_removal_delay_delta_minus1", ps_bp_sei->au_cpb_removal_delay_delta_minus1);

    if(1 == ps_vui_params->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag)
    {
        for(i = 0; i <= ps_bp_sei->u4_cpb_cnt; i++)
        {
            PUT_BITS(
                ps_bitstrm,
                ps_bp_sei->au4_nal_initial_cpb_removal_delay[i],
                ps_bp_sei->u4_initial_cpb_removal_delay_length,
                return_status);
            ENTROPY_TRACE(
                "nal_initial_cpb_removal_delay", ps_bp_sei->au4_nal_initial_cpb_removal_delay[i]);

            PUT_BITS(
                ps_bitstrm,
                ps_bp_sei->au4_nal_initial_cpb_removal_delay_offset[i],
                ps_bp_sei->u4_initial_cpb_removal_delay_length,
                return_status);
            ENTROPY_TRACE(
                "nal_initial_cpb_removal_delay_offset",
                ps_bp_sei->au4_nal_initial_cpb_removal_delay_offset[i]);

            if(u1_sub_pic_cpb_params_present_flag || ps_bp_sei->u1_rap_cpb_params_present_flag)
            {
                PUT_BITS(
                    ps_bitstrm,
                    ps_bp_sei->au4_nal_initial_alt_cpb_removal_delay[i],
                    ps_bp_sei->u4_initial_cpb_removal_delay_length,
                    return_status);
                ENTROPY_TRACE(
                    "nal_initial_alt_cpb_removal_delay",
                    ps_bp_sei->au4_nal_initial_alt_cpb_removal_delay[i]);

                PUT_BITS(
                    ps_bitstrm,
                    ps_bp_sei->au4_nal_initial_alt_cpb_removal_delay_offset[i],
                    ps_bp_sei->u4_initial_cpb_removal_delay_length,
                    return_status);
                ENTROPY_TRACE(
                    "nal_initial_alt_cpb_removal_delay_offset",
                    ps_bp_sei->au4_nal_initial_alt_cpb_removal_delay_offset[i]);
            }
        }
    }

    if(1 == ps_vui_params->s_vui_hrd_parameters.u1_vcl_hrd_parameters_present_flag)
    {
        for(i = 0; i <= ps_bp_sei->u4_cpb_cnt; i++)
        {
            PUT_BITS(
                ps_bitstrm,
                ps_bp_sei->au4_vcl_initial_cpb_removal_delay[i],
                ps_bp_sei->u4_initial_cpb_removal_delay_length,
                return_status);
            ENTROPY_TRACE(
                "vcl_initial_cpb_removal_delay", ps_bp_sei->au4_vcl_initial_cpb_removal_delay[i]);

            PUT_BITS(
                ps_bitstrm,
                ps_bp_sei->au4_vcl_initial_cpb_removal_delay_offset[i],
                ps_bp_sei->u4_initial_cpb_removal_delay_length,
                return_status);
            ENTROPY_TRACE(
                "vcl_initial_cpb_removal_delay_offset",
                ps_bp_sei->au4_vcl_initial_cpb_removal_delay_offset[i]);

            if(u1_sub_pic_cpb_params_present_flag || ps_bp_sei->u1_rap_cpb_params_present_flag)
            {
                PUT_BITS(
                    ps_bitstrm,
                    ps_bp_sei->au4_vcl_initial_alt_cpb_removal_delay[i],
                    ps_bp_sei->u4_initial_cpb_removal_delay_length,
                    return_status);
                ENTROPY_TRACE(
                    "vcl_initial_alt_cpb_removal_delay",
                    ps_bp_sei->au4_vcl_initial_alt_cpb_removal_delay[i]);

                PUT_BITS(
                    ps_bitstrm,
                    ps_bp_sei->au4_vcl_initial_alt_cpb_removal_delay_offset[i],
                    ps_bp_sei->u4_initial_cpb_removal_delay_length,
                    return_status);
                ENTROPY_TRACE(
                    "vcl_initial_alt_cpb_removal_delay_offset",
                    ps_bp_sei->au4_vcl_initial_alt_cpb_removal_delay_offset[i]);
            }
        }
    }

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates active parameter set  (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_act_sei
*  pointer to structure containing active parameter set SEI data
*
*  @return      success or failure error code
*
*****************************************************************************
*/
WORD32 ihevce_put_active_parameter_set_sei_params(
    active_parameter_set_sei_param_t *ps_act_sei, bitstrm_t *ps_bitstrm)

{
    UWORD8 u1_payload_size = 0, i;
    UWORD8 u1_range = 0, u1_act_sps_id;
    WORD32 return_status = IHEVCE_SUCCESS;

    u1_payload_size += 4;
    u1_payload_size += 1;
    u1_payload_size += 1;
    u1_payload_size += 1;  // num_sps_ids_minus1  ahould be zero as per the standard

    u1_act_sps_id = ps_act_sei->u1_active_video_parameter_set_id;

    GETRANGE(u1_range, u1_act_sps_id);

    u1_payload_size += (((u1_range - 1) << 1) + 1);

    u1_payload_size = (u1_payload_size + 7) >> 3;

    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("payload_size", u1_payload_size);

    PUT_BITS(ps_bitstrm, ps_act_sei->u1_active_video_parameter_set_id, 4, return_status);
    ENTROPY_TRACE("active_video_parameter_set_id", ps_act_sei->u1_active_video_parameter_set_id);

    PUT_BITS(ps_bitstrm, ps_act_sei->u1_self_contained_cvs_flag, 1, return_status);
    ENTROPY_TRACE("self_contained_cvs_flag", ps_act_sei->u1_self_contained_cvs_flag);

    PUT_BITS(ps_bitstrm, ps_act_sei->u1_no_parameter_set_update_flag, 1, return_status);
    ENTROPY_TRACE("no_parameter_set_update_flag", ps_act_sei->u1_no_parameter_set_update_flag);

    PUT_BITS_UEV(ps_bitstrm, ps_act_sei->u1_num_sps_ids_minus1, return_status);

    ENTROPY_TRACE("num_sps_ids_minus1", ps_act_sei->u1_num_sps_ids_minus1);

    for(i = 0; i <= ps_act_sei->u1_num_sps_ids_minus1; i++)
    {
        PUT_BITS_UEV(ps_bitstrm, ps_act_sei->au1_active_seq_parameter_set_id[i], return_status);
        ENTROPY_TRACE(
            "active_video_parameter_set_id", ps_act_sei->au1_active_seq_parameter_set_id[i]);
    }
    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates Mastering Display Colour Volume (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_rp_sei
*  pointer to structure containing recovery point SEI data
*
*  @param[in]   ps_vui_params
*  pointer to structure containing VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_mastering_disp_col_vol_sei_params(
    mastering_dis_col_vol_sei_params_t *ps_mdcl_sei, bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_payload_size = 0;
    WORD32 c;

    u1_payload_size += 6; /* display primaries x */
    u1_payload_size += 6; /* display primaries y */
    u1_payload_size += 2; /* white point x */
    u1_payload_size += 2; /* white point y */
    u1_payload_size += 4; /* max display mastering luminance */
    u1_payload_size += 4; /* min display mastering luminance */

    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("u1_payload_size", u1_payload_size);

    ASSERT(ps_mdcl_sei->u2_white_point_x <= 50000);

    ASSERT(ps_mdcl_sei->u2_white_point_y <= 50000);

    ASSERT(
        ps_mdcl_sei->u4_max_display_mastering_luminance >
        ps_mdcl_sei->u4_min_display_mastering_luminance);

    /*******************************************************************************/
    /* Put the mastering display colour volume SEI parameters into the bitstream.  */
    /* For details refer to section D.1.1 of the standard                          */
    /*******************************************************************************/

    /* display primaries x */
    for(c = 0; c < 3; c++)
    {
        ASSERT(ps_mdcl_sei->au2_display_primaries_x[c] <= 50000);

        PUT_BITS(ps_bitstrm, ps_mdcl_sei->au2_display_primaries_x[c], 16, return_status);
        ENTROPY_TRACE("u2_display_primaries_x", ps_mdcl_sei->au2_display_primaries_x[c]);

        ASSERT(ps_mdcl_sei->au2_display_primaries_y[c] <= 50000);

        PUT_BITS(ps_bitstrm, ps_mdcl_sei->au2_display_primaries_y[c], 16, return_status);
        ENTROPY_TRACE("u2_display_primaries_y", ps_mdcl_sei->au2_display_primaries_y[c]);
    }

    /* white point x */
    PUT_BITS(ps_bitstrm, ps_mdcl_sei->u2_white_point_x, 16, return_status);
    ENTROPY_TRACE("u2_white point x", ps_mdcl_sei->u2_white_point_x);

    /* white point y */
    PUT_BITS(ps_bitstrm, ps_mdcl_sei->u2_white_point_y, 16, return_status);
    ENTROPY_TRACE("u2_white point y", ps_mdcl_sei->u2_white_point_y);

    /* max display mastering luminance */
    PUT_BITS(ps_bitstrm, ps_mdcl_sei->u4_max_display_mastering_luminance, 32, return_status);
    ENTROPY_TRACE(
        "u4_max_display_mastering_luminance", ps_mdcl_sei->u4_max_display_mastering_luminance);

    /* min display mastering luminance */
    PUT_BITS(ps_bitstrm, ps_mdcl_sei->u4_min_display_mastering_luminance, 32, return_status);
    ENTROPY_TRACE(
        "u4_max_display_mastering_luminance", ps_mdcl_sei->u4_min_display_mastering_luminance);

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Stores user data in bitstream
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   u4_sei_payload_length
*  SEI Payload Length
*
*  @param[in]   pu1_sei_payload
*  pointer to SEI Payload
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_sei_params(
    UWORD32 u4_sei_payload_length, UWORD8 *pu1_sei_payload, bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD32 i, u4_length = 0;

    u4_length = u4_sei_payload_length;
    while(u4_sei_payload_length >= 0xFF)
    {
        PUT_BITS(ps_bitstrm, (UWORD32)0xFF, 8, return_status);
        u4_sei_payload_length -= 0xFF;
    }
    PUT_BITS(ps_bitstrm, (UWORD32)u4_sei_payload_length, 8, return_status);

    for(i = 0; i < u4_length; i++)
    {
        PUT_BITS(ps_bitstrm, (UWORD32)*pu1_sei_payload, 8, return_status);
        pu1_sei_payload++;
    }

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Stores content light level info in bitstream
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]
*
*
*  @param[in]
*
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_cll_info_sei_params(UWORD16 u2_avg_cll, UWORD16 u2_max_cll, bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_payload_size;

    u1_payload_size = 4;
    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("u1_payload_size", u1_payload_size);

    PUT_BITS(ps_bitstrm, (UWORD32)u2_avg_cll, 16, return_status);
    PUT_BITS(ps_bitstrm, (UWORD32)u2_max_cll, 16, return_status);

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates Recovery Point (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_rp_sei
*  pointer to structure containing recovery point SEI data
*
*  @param[in]   ps_vui_params
*  pointer to structure containing VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_recovery_point_sei_params(
    recovery_point_sei_params_t *ps_rp_sei, bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_payload_size = 0;

    {
        UWORD32 range, val;
        WORD32 recov_point = ps_rp_sei->i4_recovery_poc_cnt;
        val = 0;
        range = 0;

        if(recov_point <= 0)
            val = ((-recov_point) << 1);
        else
            val = (recov_point << 1) - 1;

        GETRANGE(range, val);

        u1_payload_size += (((range - 1) << 1) + 1);

        u1_payload_size += 1;
        u1_payload_size += 1;
    }

    u1_payload_size = (u1_payload_size + 7) >> 3;
    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("u1_payload_size", u1_payload_size);
    /************************************************************************/
    /* Put the recovery point SEI parameters into the bitstream. For      */
    /* details refer to section D.1.1 of the standard                       */
    /************************************************************************/

    /* i4_recovery_poc_cnt */
    PUT_BITS_SEV(ps_bitstrm, ps_rp_sei->i4_recovery_poc_cnt, return_status);
    ENTROPY_TRACE("i4_recovery_poc_cnt", ps_rp_sei->i4_recovery_poc_cnt);

    /* u1_exact_match_flag */
    PUT_BITS(ps_bitstrm, ps_rp_sei->u1_exact_match_flag, 1, return_status);
    ENTROPY_TRACE("exact_match_flag", ps_rp_sei->u1_exact_match_flag);

    /* u1_broken_link_flag */
    PUT_BITS(ps_bitstrm, ps_rp_sei->u1_broken_link_flag, 1, return_status);
    ENTROPY_TRACE("broken_link_flag", ps_rp_sei->u1_broken_link_flag);

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates Picture timing (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_pt_sei
*  pointer to structure containing oicture timing SEI data
*
*  @param[in]   ps_vui_params
*  pointer to structure containing VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_pic_timing_sei_params(
    pic_timing_sei_params_t *ps_pt_sei, vui_t *ps_vui_params, bitstrm_t *ps_bitstrm)
{
    UWORD32 i;
    UWORD8 u1_payload_size = 0;
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_au_cpb_removal_delay_length =
        ps_vui_params->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 + 1;

    UWORD8 u1_dpb_output_delay_length =
        ps_vui_params->s_vui_hrd_parameters.u1_dpb_output_delay_length_minus1 + 1;

    UWORD8 u1_du_cpb_removal_delay_increment_length =
        ps_vui_params->s_vui_hrd_parameters.u1_du_cpb_removal_delay_increment_length_minus1 + 1;

    UWORD8 u1_sub_pic_cpb_params_present_flag =
        ps_vui_params->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag;

    UWORD8 u1_sub_pic_cpb_params_in_pt_sei_flag =
        ps_vui_params->s_vui_hrd_parameters.u1_sub_pic_cpb_params_in_pic_timing_sei_flag;

    {
        if(1 == ps_vui_params->u1_frame_field_info_present_flag)
        {
            u1_payload_size += 4;
            u1_payload_size += 2;
            u1_payload_size += 1;
        }

        if(ps_vui_params->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag ||
           ps_vui_params->s_vui_hrd_parameters.u1_vcl_hrd_parameters_present_flag)
        {
            u1_payload_size += u1_au_cpb_removal_delay_length;
            u1_payload_size += u1_dpb_output_delay_length;
        }

        if(u1_sub_pic_cpb_params_in_pt_sei_flag && u1_sub_pic_cpb_params_present_flag)
        {
            UWORD32 range, val = ps_pt_sei->u4_num_decoding_units_minus1;
            range = 0;

            GETRANGE(range, val);
            u1_payload_size += (((range - 1) << 1) + 1);

            u1_payload_size += 1;
            if(1 == ps_pt_sei->u1_du_common_cpb_removal_delay_flag)
            {
                u1_payload_size += u1_du_cpb_removal_delay_increment_length;

                for(i = 0; i <= ps_pt_sei->u4_num_decoding_units_minus1; i++)
                {
                    val = ps_pt_sei->au4_num_nalus_in_du_minus1[0];
                    range = 0;

                    GETRANGE(range, val);
                    u1_payload_size += (((range - 1) << 1) + 1);

                    if((1 != ps_pt_sei->u1_du_common_cpb_removal_delay_flag) &&
                       (i < ps_pt_sei->u4_num_decoding_units_minus1))
                    {
                        u1_payload_size += u1_du_cpb_removal_delay_increment_length;
                    }
                }
            }
        }
    }

    ASSERT((ps_pt_sei->u4_au_cpb_removal_delay_minus1 < (1 << u1_au_cpb_removal_delay_length)));

    u1_payload_size = (u1_payload_size + 7) >> 3;
    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("u1_payload_size", u1_payload_size);

    /************************************************************************/
    /* Put the picture timing SEI parameters into the bitstream. For        */
    /* details refer to section D.1.2 of the standard                       */
    /************************************************************************/

    if(1 == ps_vui_params->u1_frame_field_info_present_flag)
    {
        PUT_BITS(ps_bitstrm, ps_pt_sei->u4_pic_struct, 4, return_status);
        ENTROPY_TRACE("pic_struct", ps_pt_sei->u4_pic_struct);

        PUT_BITS(ps_bitstrm, ps_pt_sei->u4_source_scan_type, 2, return_status);
        ENTROPY_TRACE("source_scan_type", ps_pt_sei->u4_source_scan_type);

        PUT_BITS(ps_bitstrm, ps_pt_sei->u1_duplicate_flag, 1, return_status);
        ENTROPY_TRACE("duplicate_flag", ps_pt_sei->u1_duplicate_flag);
    }

    if(ps_vui_params->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag ||
       ps_vui_params->s_vui_hrd_parameters
           .u1_vcl_hrd_parameters_present_flag)  // condition from standard when CpbDpbDelaysPresentFlag flag will be present
    {
        PUT_BITS(
            ps_bitstrm,
            ps_pt_sei->u4_au_cpb_removal_delay_minus1,
            u1_au_cpb_removal_delay_length,
            return_status);
        ENTROPY_TRACE("cpb_removal_delay_minus1", ps_pt_sei->u4_au_cpb_removal_delay_minus1);

        PUT_BITS(
            ps_bitstrm,
            ps_pt_sei->u4_pic_dpb_output_delay,
            u1_dpb_output_delay_length,
            return_status);
        ENTROPY_TRACE("pic_dpb_output_delay", ps_pt_sei->u4_pic_dpb_output_delay);

        if(u1_sub_pic_cpb_params_present_flag)
        {
            PUT_BITS(
                ps_bitstrm,
                ps_pt_sei->u4_pic_dpb_output_du_delay,
                ps_vui_params->s_vui_hrd_parameters.u1_dpb_output_delay_du_length_minus1,
                return_status);
            ENTROPY_TRACE("pic_dpb_output_du_delay", ps_pt_sei->u4_pic_dpb_output_delay);
        }

        if(u1_sub_pic_cpb_params_in_pt_sei_flag && u1_sub_pic_cpb_params_present_flag)
        {
            PUT_BITS_UEV(ps_bitstrm, ps_pt_sei->u4_num_decoding_units_minus1, return_status);
            ENTROPY_TRACE("num_decoding_units_minus1", ps_pt_sei->u4_num_decoding_units_minus1);

            PUT_BITS(ps_bitstrm, ps_pt_sei->u1_du_common_cpb_removal_delay_flag, 1, return_status);
            ENTROPY_TRACE(
                "du_common_cpb_removal_delay_flag", ps_pt_sei->u4_du_common_cpb_removal_delay_flag);

            if(1 == ps_pt_sei->u1_du_common_cpb_removal_delay_flag)
            {
                PUT_BITS(
                    ps_bitstrm,
                    ps_pt_sei->u4_du_common_cpb_removal_delay_increment_minus1,
                    u1_du_cpb_removal_delay_increment_length,
                    return_status);
                ENTROPY_TRACE(
                    "du_common_cpb_removal_delay_increment_minus1",
                    ps_pt_sei->u4_du_common_cpb_removal_delay_increment_minus1);
            }

            for(i = 0; i <= ps_pt_sei->u4_num_decoding_units_minus1; i++)
            {
                PUT_BITS_UEV(ps_bitstrm, ps_pt_sei->au4_num_nalus_in_du_minus1[0], return_status);
                ENTROPY_TRACE("num_nalus_in_du_minus1", ps_pt_sei->u4_num_nalus_in_du_minus1);

                if((1 != ps_pt_sei->u1_du_common_cpb_removal_delay_flag) &&
                   (i < ps_pt_sei->u4_num_decoding_units_minus1))
                {
                    PUT_BITS(
                        ps_bitstrm,
                        ps_pt_sei->au4_du_cpb_removal_delay_increment_minus1[0],
                        u1_du_cpb_removal_delay_increment_length,
                        return_status);
                    ENTROPY_TRACE(
                        "du_cpb_removal_delay_increment_minus1",
                        ps_pt_sei->u4_du_cpb_removal_delay_increment_minus1);
                }
            }
        }
    }

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates Hash (Supplemental Enhancement Information )
*
*  @par   Description
*  Put hash sei params
*
*  @param[in]   ps_hash_sei_params
*  pointer to structure containing hash SEI data
*
*  @param[in]   i1_decoded_pic_hash_sei_flag
*  flag saying the hash SEI type
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_hash_sei_params(
    hash_sei_param_t *ps_hash_sei_params, WORD8 i1_decoded_pic_hash_sei_flag, bitstrm_t *ps_bitstrm)
{
    UWORD32 i, c_idx, val;
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD8 u1_payload_size = 0;

    u1_payload_size += 1; /* hash_type */

    if(1 == i1_decoded_pic_hash_sei_flag)
    {
        /* MD5 : 3 color planes * 16 byte values */
        u1_payload_size += (16 * 3);
    }
    else if(2 == i1_decoded_pic_hash_sei_flag)
    {
        /* CRC : 3 color planes * 2 byte values */
        u1_payload_size += (2 * 3);
    }
    else if(3 == i1_decoded_pic_hash_sei_flag)
    {
        /* Checksum : 3 color planes * 4 byte values */
        u1_payload_size += (4 * 3);
    }
    else
    {
        ASSERT(0);
    }

    /************************************************************************/
    /* PayloadSize : This is the size of the payload in bytes               */
    /************************************************************************/
    PUT_BITS(ps_bitstrm, u1_payload_size, 8, return_status);
    ENTROPY_TRACE("payload_size", u1_payload_size);

    /************************************************************************/
    /* Put the hash SEI parameters into the bitstream. For                  */
    /* details refer to section D.2.19 of the standard                      */
    /************************************************************************/

    PUT_BITS(ps_bitstrm, (i1_decoded_pic_hash_sei_flag - 1), 8, return_status);
    ENTROPY_TRACE("hash_type", (i1_decoded_pic_hash_sei_flag - 1));

    if(1 == i1_decoded_pic_hash_sei_flag)
    {
        for(c_idx = 0; c_idx < 3; c_idx++)
        {
            for(i = 0; i < 16; i++)
            {
                PUT_BITS(ps_bitstrm, ps_hash_sei_params->au1_sei_hash[c_idx][i], 8, return_status);
                ENTROPY_TRACE("picture_md5", ps_hash_sei_params->au1_sei_hash[c_idx][i]);
            }
        }
    }
    else if(2 == i1_decoded_pic_hash_sei_flag)
    {
        for(c_idx = 0; c_idx < 3; c_idx++)
        {
            val = (ps_hash_sei_params->au1_sei_hash[c_idx][0] << 8) +
                  ps_hash_sei_params->au1_sei_hash[c_idx][1];
            PUT_BITS(ps_bitstrm, val, 16, return_status);
            ENTROPY_TRACE("picture_crc", val);
        }
    }
    else if(3 == i1_decoded_pic_hash_sei_flag)
    {
        for(c_idx = 0; c_idx < 3; c_idx++)
        {
            val = (ps_hash_sei_params->au1_sei_hash[c_idx][0] << 24) +
                  (ps_hash_sei_params->au1_sei_hash[c_idx][1] << 16) +
                  (ps_hash_sei_params->au1_sei_hash[c_idx][2] << 8) +
                  (ps_hash_sei_params->au1_sei_hash[c_idx][3]);

            PUT_BITS(ps_bitstrm, val, 32, return_status);
            ENTROPY_TRACE("picture_checksum", val);
        }
    }
    else
    {
        ASSERT(0);
    }

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates SEI (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   e_payload_type
*  Determines the type of SEI msg
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_params
*  pointer to structure containing SEI data
*               buffer period, recovery point, picture timing
*
*  @param[in]   ps_vui_params
*  pointer to structure containing VUI data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_put_sei_msg(
    IHEVCE_SEI_TYPE e_payload_type,
    sei_params_t *ps_sei_params,
    vui_t *ps_vui_params,
    bitstrm_t *ps_bitstrm,
    UWORD32 i4_sei_payload_length,
    UWORD8 *pu1_sei_payload)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    /************************************************************************/
    /* PayloadType : Send in the SEI type in the stream                     */
    /************************************************************************/
    UWORD32 u4_payload_type = e_payload_type;
    while(u4_payload_type > 0xFF)
    {
        PUT_BITS(ps_bitstrm, 0xFF, 8, return_status);
        u4_payload_type -= 0xFF;
    }
    PUT_BITS(ps_bitstrm, (UWORD32)u4_payload_type, 8, return_status);
    ENTROPY_TRACE("e_payload_type", e_payload_type);

    /************************************************************************/
    /* PayloadSize : This is sent from within the type specific functions   */
    /************************************************************************/

    switch(e_payload_type)
    {
    case IHEVCE_SEI_BUF_PERIOD_T:
        return_status |= ihevce_put_buf_period_sei_params(
            &(ps_sei_params->s_buf_period_sei_params), ps_vui_params, ps_bitstrm);
        break;

    case IHEVCE_SEI_PIC_TIMING_T:
        return_status |= ihevce_put_pic_timing_sei_params(
            &(ps_sei_params->s_pic_timing_sei_params), ps_vui_params, ps_bitstrm);
        break;

    case IHEVCE_SEI_RECOVERY_POINT_T:
        return_status |= ihevce_put_recovery_point_sei_params(
            &(ps_sei_params->s_recovery_point_params), ps_bitstrm);
        break;
    case IHEVCE_SEI_ACTIVE_PARAMETER_SETS_T:
        return_status |= ihevce_put_active_parameter_set_sei_params(
            &(ps_sei_params->s_active_parameter_set_sei_params), ps_bitstrm);
        break;
    case IHEVCE_SEI_DECODED_PICTURE_HASH_T:
        return_status |= ihevce_put_hash_sei_params(
            &(ps_sei_params->s_hash_sei_params),
            ps_sei_params->i1_decoded_pic_hash_sei_flag,
            ps_bitstrm);
        break;
    case IHEVCE_SEI_MASTERING_DISP_COL_VOL_T:
        return_status |= ihevce_put_mastering_disp_col_vol_sei_params(
            &(ps_sei_params->s_mastering_dis_col_vol_sei_params), ps_bitstrm);
        break;
    case IHEVCE_SEI_CONTENT_LIGHT_LEVEL_DATA_T:
        return_status |= ihevce_put_cll_info_sei_params(
            ps_sei_params->s_cll_info_sei_params.u2_sei_avg_cll,
            ps_sei_params->s_cll_info_sei_params.u2_sei_max_cll,
            ps_bitstrm);
        break;
    //case IHEVCE_SEI_USER_DATA_REGISTERED_ITU_T_T35_T:
    //return_status |= ihevce_put_sei_params(i4_sei_payload_length, pu1_sei_payload, ps_bitstrm);
    //break;
    default:
        //Any Payload type other than the above cases will entred hereand be added to the bitstream
        return_status |= ihevce_put_sei_params(i4_sei_payload_length, pu1_sei_payload, ps_bitstrm);
        //return_status = IHEVCE_FAIL;
    }

    ASSERT(IHEVCE_SUCCESS == return_status);

    /* rbsp trailing bits */
    if((IHEVCE_SUCCESS == return_status) && (ps_bitstrm->i4_bits_left_in_cw & 0x7))
        ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates SEI (Supplemental Enhancement Information )
*
*  @par   Description
*  Parse Supplemental Enhancement Information as per Section 7.3.2.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sei_params
*  pointer to structure containing SEI data
*               buffer period, recovery point, picture timing
*
*  @param[in]   ps_vui_params
*  pointer to structure containing VUI data
*
*  @param[in]   nal_unit_header
*  NAL_PREFIX_SEI / NAL_SUFFIX_SEI
*
*  @param[in]   u4_num_sei_payloads
*  Number of SEI payloads
*
*  @param[in]   ps_sei_payload
*  pointer to structure containing SEI payload data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_sei(
    bitstrm_t *ps_bitstrm,
    sei_params_t *ps_sei_params,
    vui_t *ps_vui_params,
    WORD32 insert_per_cra,
    WORD32 nal_unit_header,
    UWORD32 u4_num_sei_payloads,
    sei_payload_t *ps_sei_payload)
{
    UWORD32 u4_i;
    WORD32 return_status = IHEVCE_SUCCESS;

    (void)insert_per_cra;
    /* Insert Start Code */
    return_status = ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    ASSERT((NAL_PREFIX_SEI == nal_unit_header) || (NAL_SUFFIX_SEI == nal_unit_header));
    /* Insert Nal Unit Header */
    return_status |= ihevce_generate_nal_unit_header(ps_bitstrm, nal_unit_header, 0);

    if(NAL_PREFIX_SEI == nal_unit_header)
    {
        /* Active Parameter and Buffering period insertion */

        if(ps_sei_params->i1_buf_period_params_present_flag)
        {
            /* insert active_parameter_set SEI required if buffering period SEI messages are inserted */
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_ACTIVE_PARAMETER_SETS_T,
                ps_sei_params,
                ps_vui_params,
                ps_bitstrm,
                0,
                NULL);

            /*************************************************************************************************/
            /* NOTE: Need to terminate and start new SEI message after active parameter set SEI              */
            /* Buffering period/pic timing SEI refering to active SPS cannot be embedded in same SEI message */
            /* This is because SPS is activated in HM deocder after completely parsing full SEI message.     */
            /*************************************************************************************************/
            if(1) /* Insert New SEI for buffering period after active parameter set SEI */
            {
                ihevce_put_rbsp_trailing_bits(ps_bitstrm);

                /* Insert Next SEI Start Code */
                return_status = ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

                /* Insert Next SEI Nal Unit Header */
                return_status |= ihevce_generate_nal_unit_header(ps_bitstrm, nal_unit_header, 0);
            }

            /* Buffering Period SEI meassage for all IDR, CRA pics */
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_BUF_PERIOD_T, ps_sei_params, ps_vui_params, ps_bitstrm, 0, NULL);
        }

        /* Pic timing SEI meassage for non IDR, non CRA pics */
        if(ps_sei_params->i1_pic_timing_params_present_flag)
        {
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_PIC_TIMING_T, ps_sei_params, ps_vui_params, ps_bitstrm, 0, NULL);
        }

        /* Recovery point SEI meassage for all IDR, CRA pics */
        if(ps_sei_params->i1_recovery_point_params_present_flag)
        {
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_RECOVERY_POINT_T, ps_sei_params, ps_vui_params, ps_bitstrm, 0, NULL);
        }

        /* Mastering Display Colour SEI for all IDR, CRA pics */
        if(ps_sei_params->i4_sei_mastering_disp_colour_vol_params_present_flags)
        {
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_MASTERING_DISP_COL_VOL_T,
                ps_sei_params,
                ps_vui_params,
                ps_bitstrm,
                0,
                NULL);
        }
        /*Registered User Data*/
        for(u4_i = 0; u4_i < u4_num_sei_payloads; u4_i++)
        {
            return_status |= ihevce_put_sei_msg(
                (IHEVCE_SEI_TYPE)(ps_sei_payload[u4_i].u4_payload_type),
                ps_sei_params,
                ps_vui_params,
                ps_bitstrm,
                ps_sei_payload[u4_i].u4_payload_length,
                ps_sei_payload[u4_i].pu1_sei_payload);
        }
        /* Content Light Level Information*/
        if(ps_sei_params->i1_sei_cll_enable)
        {
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_CONTENT_LIGHT_LEVEL_DATA_T,
                ps_sei_params,
                ps_vui_params,
                ps_bitstrm,
                0,
                NULL);
        }
    }
    else if(NAL_SUFFIX_SEI == nal_unit_header)
    {
        /* Insert hash SEI */
        if(ps_sei_params->i1_decoded_pic_hash_sei_flag)
        {
            return_status |= ihevce_put_sei_msg(
                IHEVCE_SEI_DECODED_PICTURE_HASH_T,
                ps_sei_params,
                ps_vui_params,
                ps_bitstrm,
                0,
                NULL);
        }
    }

    /*put trailing bits to indicate end of sei*/
    ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Populates ihevce_populate_mastering_disp_col_vol_sei of SEI  structure
*
*  @par   Description
*  Populates mastering display colour volume sei structure
*
*  @param[in]  ps_sei
*  pointer to sei params that needs to be populated
*
*  @param[in]  ps_out_strm_prms
*  pointer to output stream params
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_mastering_disp_col_vol_sei(
    sei_params_t *ps_sei, ihevce_out_strm_params_t *ps_out_strm_prms)
{
    WORD32 i;

    mastering_dis_col_vol_sei_params_t *ps_mastering_dis_col_vol_sei_params =
        &ps_sei->s_mastering_dis_col_vol_sei_params;

    for(i = 0; i < 3; i++)
    {
        ps_mastering_dis_col_vol_sei_params->au2_display_primaries_x[i] =
            ps_out_strm_prms->au2_display_primaries_x[i];
        ps_mastering_dis_col_vol_sei_params->au2_display_primaries_y[i] =
            ps_out_strm_prms->au2_display_primaries_y[i];
    }

    ps_mastering_dis_col_vol_sei_params->u2_white_point_x = ps_out_strm_prms->u2_white_point_x;
    ps_mastering_dis_col_vol_sei_params->u2_white_point_y = ps_out_strm_prms->u2_white_point_y;

    ps_mastering_dis_col_vol_sei_params->u4_max_display_mastering_luminance =
        ps_out_strm_prms->u4_max_display_mastering_luminance;
    ps_mastering_dis_col_vol_sei_params->u4_min_display_mastering_luminance =
        ps_out_strm_prms->u4_min_display_mastering_luminance;

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates ihevce_populate_recovery_point_sei of SEI  structure
*
*  @par   Description
*  Populates vui structure for its use in header generation
*
*  @param[out]  ps_sei
*  pointer to sei params that needs to be populated
*
*  @param[out]  ps_vui
*  pointer to vui params referred by sei
*
*  @param[out]  ps_sps
*  pointer to sps params referred by sei
*
*  @param[in]   ps_src_params
*  pointer to source config params; resolution, frame rate etc
*
*  @param[in]   ps_config_prms
*  pointer to configuration params like bitrate, HRD buffer sizes, cu, tu sizes
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_recovery_point_sei(
    sei_params_t *ps_sei, ihevce_vui_sei_params_t *ps_vui_sei_prms)
{
    recovery_point_sei_params_t *ps_rec_point_params = &ps_sei->s_recovery_point_params;

    (void)ps_vui_sei_prms;
    ps_rec_point_params->i4_recovery_poc_cnt = 0;
    ps_rec_point_params->u1_broken_link_flag = 0;
    ps_rec_point_params->u1_exact_match_flag = 1;

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates picture timing of SEI  structure
*
*  @par   Description
*  Populates vui structure for its use in header generation
*
*  @param[out]  ps_sei
*  pointer to sei params that needs to be populated
*
*  @param[out]  ps_vui
*  pointer to vui params referred by sei
*
*  @param[in]   ps_src_params
*  pointer to source config params; resolution, frame rate etc
*
*  @param[in]   u4_bottom_field_flag
*  Used only for interlaced field coding. 0:top field, 1:bottom field
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_picture_timing_sei(
    sei_params_t *ps_sei,
    vui_t *ps_vui,
    ihevce_src_params_t *ps_src_params,
    WORD32 u4_bottom_field_flag)
{
    pic_timing_sei_params_t *ps_pic_timing_params = &ps_sei->s_pic_timing_sei_params;
    UWORD8 u1_prog_seq = !ps_src_params->i4_field_pic;
    UWORD8 u1_top_field_first = 1;  //ps_curr_inp->s_input_buf.i4_topfield_first;

    UWORD8 u1_repeat_first_field = 0;
    WORD32 field_seq_flag = ps_vui->u1_field_seq_flag;

    if(ps_vui->u1_frame_field_info_present_flag)
    {
        /**************************************************************************/
        /* Refer Table D-1                                                        */
        /**************************************************************************/
        if(0 == u1_prog_seq)
        {
            if(field_seq_flag)
            {
                ASSERT((u4_bottom_field_flag == 0) || (u4_bottom_field_flag == 1));

                /* 1 => top field pic */
                /* 2 => bottom field pic */
                ps_pic_timing_params->u4_pic_struct = 1 + u4_bottom_field_flag;
            }
            else if(0 == u1_repeat_first_field)
            {
                /******************************************************************/
                /* [PROGRESSIVE SEQ]    = 0;                                      */
                /* [MPEG2 PIC STRUCT]   = FIELD_PICTURE                           */
                /* [REPEAT_FIRST_FIELD] = 0                                       */
                /* u1_pic_struct = 3 => top    - bottom field pic                 */
                /* u1_pic_struct = 4 => bottom - top                              */
                /******************************************************************/
                ps_pic_timing_params->u4_pic_struct = 4 - u1_top_field_first;
            }
            else
            {
                /******************************************************************/
                /* [PROGRESSIVE SEQ]    = 0;                                      */
                /* [MPEG2 PIC STRUCT]   = FIELD_PICTURE                           */
                /* [REPEAT_FIRST_FIELD] = 1                                       */
                /* u1_pic_struct = 5 => top    - bottom - top                     */
                /* u1_pic_struct = 6 => bottom - top    - bottom                  */
                /******************************************************************/
                ps_pic_timing_params->u4_pic_struct = 6 - u1_top_field_first;
            }
        }
        else
        {
            if(0 == u1_repeat_first_field)
            {
                /******************************************************************/
                /* [PROGRESSIVE SEQ]    = 1;                                      */
                /* [MPEG2 PIC STRUCT]   = FRAME_PICTURE                           */
                /* u1_pic_struct = 0 => frame picture (no repeat)                 */
                /******************************************************************/
                ps_pic_timing_params->u4_pic_struct = 0;
            }
            else
            {
                /******************************************************************/
                /* [PROGRESSIVE SEQ]    = 1;                                      */
                /* [MPEG2 PIC STRUCT]   = FRAME_PICTURE                           */
                /* u1_pic_struct = 7 => frame picture (repeat once)               */
                /* u1_pic_struct = 8 => frame picture (repeat twice)              */
                /******************************************************************/
                ps_pic_timing_params->u4_pic_struct = 7 + u1_top_field_first;
            }
        }
        /* Porogressive frame - 1 ; Interlace - 0 */
        ps_pic_timing_params->u4_source_scan_type = !ps_src_params->i4_field_pic;

        ps_pic_timing_params->u1_duplicate_flag = 0;
    }
    ps_pic_timing_params->u4_pic_dpb_output_du_delay = 0;

    ps_pic_timing_params->u4_num_decoding_units_minus1 = 1;

    ps_pic_timing_params->u1_du_common_cpb_removal_delay_flag = 1;

    ps_pic_timing_params->u4_du_common_cpb_removal_delay_increment_minus1 = 1;

    ps_pic_timing_params->au4_num_nalus_in_du_minus1[0] = 1;

    ps_pic_timing_params->au4_du_cpb_removal_delay_increment_minus1[0] = 1;

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates buffer period of sei structure
*
*  @par   Description
*  Populates vui structure for its use in header generation
*
*  @param[out]  ps_sei
*  pointer to sei params that needs to be populated
*
*  @param[out]  ps_vui
*  pointer to vui params referred by sei
*
*  @param[out]  ps_sps
*  pointer to sps params referred by sei
*
*  @param[out]  ps_vui_sei_prms
*  pointer to sps params referred by application
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_buffering_period_sei(
    sei_params_t *ps_sei, vui_t *ps_vui, sps_t *ps_sps, ihevce_vui_sei_params_t *ps_vui_sei_prms)
{
    WORD32 i;

    buf_period_sei_params_t *ps_bp_sei = &ps_sei->s_buf_period_sei_params;

    WORD32 i1_sps_max_sub_layersminus1 = ps_sps->i1_sps_max_sub_layers - 1;

    hrd_params_t *ps_vui_hrd_parameters = &ps_vui->s_vui_hrd_parameters;

    sub_lyr_hrd_params_t *ps_sub_layer_hrd_params =
        &ps_vui_hrd_parameters->as_sub_layer_hrd_params[i1_sps_max_sub_layersminus1];

    WORD32 cpb_cnt = ps_vui_hrd_parameters->au1_cpb_cnt_minus1[i1_sps_max_sub_layersminus1];

    WORD32 cpb_size, bit_rate;

    (void)ps_vui_sei_prms;
    ps_bp_sei->u1_bp_seq_parameter_set_id = ps_sps->i1_sps_id;

    ps_bp_sei->u4_initial_cpb_removal_delay_length =
        ps_vui->s_vui_hrd_parameters.u1_initial_cpb_removal_delay_length_minus1 + 1;

    ps_bp_sei->u1_sub_pic_cpb_params_present_flag =
        ps_vui_hrd_parameters->u1_sub_pic_cpb_params_present_flag;

    ps_bp_sei->u1_rap_cpb_params_present_flag = 0;  //DEFAULT value

    ps_bp_sei->u4_cpb_delay_offset = 0;  //DEFAULT value
    ps_bp_sei->u4_dpb_delay_offset = 0;  //DEFAULT value

    ps_bp_sei->u1_concatenation_flag = 0;  //DEFAULT value ???
    ps_bp_sei->u4_au_cpb_removal_delay_delta_minus1 = 0;  //DEFAULT value  ???

    ps_bp_sei->u4_cpb_cnt = cpb_cnt;

    if(ps_vui->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag)
    {
        for(i = 0; i <= cpb_cnt; i++)
        {
            ULWORD64 u8_temp;
            if(1 == ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
            {
                ASSERT(1 == ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag);

                cpb_size = (ps_sub_layer_hrd_params->au4_cpb_size_du_value_minus1[i] + 1) *
                           (1 << (4 + ps_vui_hrd_parameters->u4_cpb_size_du_scale));

                bit_rate = (ps_sub_layer_hrd_params->au4_bit_rate_du_value_minus1[i] + 1) *
                           (1 << (6 + ps_vui_hrd_parameters->u4_bit_rate_scale));
            }
            else
            {
                cpb_size = (ps_sub_layer_hrd_params->au4_cpb_size_value_minus1[i] + 1) *
                           (1 << (4 + ps_vui_hrd_parameters->u4_cpb_size_scale));

                bit_rate = (ps_sub_layer_hrd_params->au4_bit_rate_value_minus1[i] + 1) *
                           (1 << (6 + ps_vui_hrd_parameters->u4_bit_rate_scale));
            }

            u8_temp = (ULWORD64)(90000 * (ULWORD64)cpb_size);
            ps_bp_sei->au4_nal_initial_cpb_removal_delay[i] = (UWORD32)(u8_temp / bit_rate);

            ps_bp_sei->au4_nal_initial_cpb_removal_delay_offset[i] = 0;

            if(ps_bp_sei->u1_rap_cpb_params_present_flag ||
               ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
            {
                ps_bp_sei->au4_nal_initial_alt_cpb_removal_delay[i] = (UWORD32)(u8_temp / bit_rate);
                ps_bp_sei->au4_nal_initial_cpb_removal_delay_offset[i] = 0;
            }
        }
    }

    if(ps_vui->s_vui_hrd_parameters.u1_vcl_hrd_parameters_present_flag)
    {
        for(i = 0; i <= cpb_cnt; i++)
        {
            if(1 == ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
            {
                ASSERT(1 == ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag);
                cpb_size = (ps_sub_layer_hrd_params->au4_cpb_size_du_value_minus1[i] + 1) *
                           (1 << (4 + ps_vui_hrd_parameters->u4_cpb_size_du_scale));

                bit_rate = (ps_sub_layer_hrd_params->au4_bit_rate_du_value_minus1[i] + 1) *
                           (1 << (6 + ps_vui_hrd_parameters->u4_bit_rate_scale));
            }
            else
            {
                cpb_size = (ps_sub_layer_hrd_params->au4_cpb_size_value_minus1[i] + 1) *
                           (1 << (4 + ps_vui_hrd_parameters->u4_cpb_size_scale));

                bit_rate = (ps_sub_layer_hrd_params->au4_bit_rate_value_minus1[i] + 1) *
                           (1 << (6 + ps_vui_hrd_parameters->u4_bit_rate_scale));
            }

            ps_bp_sei->au4_vcl_initial_cpb_removal_delay[i] = 90000 * (cpb_size / bit_rate);

            ps_bp_sei->au4_vcl_initial_cpb_removal_delay_offset[i] = 0;

            if(ps_bp_sei->u1_rap_cpb_params_present_flag ||
               ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
            {
                ps_bp_sei->au4_vcl_initial_alt_cpb_removal_delay[i] = 90000 * (cpb_size / bit_rate);
                ps_bp_sei->au4_vcl_initial_cpb_removal_delay_offset[i] = 0;
            }
        }
    }

    /* Reset picture timing cbp removal delay at every insertion Buffering period SEI */
    //ps_sei->s_pic_timing_sei_params.u4_au_cpb_removal_delay_minus1 = 0;

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates active parameter set sei structure
*
*  @par   Description
*
*  @param[out]  ps_sei
*  pointer to sei params that needs to be populated
*
*  @param[in]   ps_vps
*  pointer to configuration vps_t.
*
*  @return      success or failure error code
*
*****************************************************************************
*/
WORD32 ihevce_populate_active_parameter_set_sei(sei_params_t *ps_sei, vps_t *ps_vps, sps_t *ps_sps)
{
    UWORD8 i;
    active_parameter_set_sei_param_t *ps_act_sei = &ps_sei->s_active_parameter_set_sei_params;

    (void)ps_sps;
    ps_act_sei->u1_active_video_parameter_set_id = ps_vps->i1_vps_id;
    ps_act_sei->u1_self_contained_cvs_flag = 0;
    ps_act_sei->u1_no_parameter_set_update_flag = 1;
    ps_act_sei->u1_num_sps_ids_minus1 = 0;
    for(i = 0; i <= ps_act_sei->u1_num_sps_ids_minus1; i++)
    {
        ps_act_sei->au1_active_seq_parameter_set_id[i] = 0;
    }

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates Hash SEI values for CRC Hash
*
*  @par   Description
*
*  @param[out]  ps_hash_sei_params
*  pointer to hash sei params that needs to be populated
*
*  @param[in]   bit_depth
*  i4_internal_bit_depth value. Assume same for Luma & Chroma
*
*  @param[in]   pv_y_buf
*  Pointer to decoded/recon Luma buffer
*
*  @param[in]   y_wd
*  pic width in luma samples
*
*  @param[in]   y_ht
*  pic height in luma samples
*
*  @param[in]   y_strd
*  Stride of luma buffer
*
*  @param[in]   pv_u_buf
*  Pointer to decoded/recon Chroma buffer
*
*  @param[in]   uv_wd
*  pic width in luma samples / SubWidthC
*
*  @param[in]   uv_ht
*  pic height in luma samples / SubHeightC
*
*  @param[in]   uv_strd
*  Stride of chroma buffer
*
*  @return  None
*
*****************************************************************************
*/
static void ihevce_calc_CRC(
    hash_sei_param_t *ps_hash_sei_params,
    WORD32 bit_depth,
    void *pv_y_buf,
    WORD32 y_wd,
    WORD32 y_ht,
    WORD32 y_strd,
    void *pv_u_buf,
    WORD32 uv_wd,
    WORD32 uv_ht,
    WORD32 uv_strd)
{
    WORD32 x, y, bit_idx, is_gt8bit = 0, gt8bit_mul;
    UWORD8 *pu_buf;

    /* For this to work, assumes little endian in case of HBD */
    if(bit_depth > 8)
        is_gt8bit = 1;
    gt8bit_mul = 1 + is_gt8bit;

    /* Luma CRC */
    {
        UWORD32 u4_crc_val = 0xffff;

        pu_buf = (UWORD8 *)pv_y_buf;

        for(y = 0; y < y_ht; y++)
        {
            for(x = 0; x < y_wd; x++)
            {
                // take CRC of first pictureData byte
                for(bit_idx = 0; bit_idx < 8; bit_idx++)
                {
                    CALC_CRC_BIT_LEVEL(
                        u4_crc_val, pu_buf[y * (y_strd * gt8bit_mul) + (x * gt8bit_mul)], bit_idx);
                }
                // take CRC of second pictureData byte if bit depth is greater than 8-bits
                if(bit_depth > 8)
                {
                    for(bit_idx = 0; bit_idx < 8; bit_idx++)
                    {
                        CALC_CRC_BIT_LEVEL(
                            u4_crc_val,
                            pu_buf[y * (y_strd * gt8bit_mul) + (x * gt8bit_mul + 1)],
                            bit_idx);
                    }
                }
            }
        }

        for(bit_idx = 0; bit_idx < 16; bit_idx++)
        {
            UWORD32 u4_crc_msb = (u4_crc_val >> 15) & 1;
            u4_crc_val = ((u4_crc_val << 1) & 0xffff) ^ (u4_crc_msb * 0x1021);
        }

        ps_hash_sei_params->au1_sei_hash[0][0] = (u4_crc_val >> 8) & 0xff;
        ps_hash_sei_params->au1_sei_hash[0][1] = u4_crc_val & 0xff;
    }

    /* Cb & Cr CRC */
    {
        UWORD32 u4_crc_val_u = 0xffff, u4_crc_val_v = 0xffff;

        pu_buf = (UWORD8 *)pv_u_buf;

        for(y = 0; y < uv_ht; y++)
        {
            for(x = 0; x < uv_wd; x += 2)
            {
                // take CRC of first pictureData byte
                for(bit_idx = 0; bit_idx < 8; bit_idx++)
                {
                    CALC_CRC_BIT_LEVEL(
                        u4_crc_val_u,
                        pu_buf[y * (uv_strd * gt8bit_mul) + (x * gt8bit_mul)],
                        bit_idx);
                    CALC_CRC_BIT_LEVEL(
                        u4_crc_val_v,
                        pu_buf[y * (uv_strd * gt8bit_mul) + ((x + 1) * gt8bit_mul)],
                        bit_idx);
                }
                // take CRC of second pictureData byte if bit depth is greater than 8-bits
                if(bit_depth > 8)
                {
                    for(bit_idx = 0; bit_idx < 8; bit_idx++)
                    {
                        CALC_CRC_BIT_LEVEL(
                            u4_crc_val_u,
                            pu_buf[y * (uv_strd * gt8bit_mul) + (x * gt8bit_mul) + 1],
                            bit_idx);
                        CALC_CRC_BIT_LEVEL(
                            u4_crc_val_v,
                            pu_buf[y * (uv_strd * gt8bit_mul) + ((x + 1) * gt8bit_mul) + 1],
                            bit_idx);
                    }
                }
            }
        }

        for(bit_idx = 0; bit_idx < 16; bit_idx++)
        {
            UWORD32 u4_crc_msb = (u4_crc_val_u >> 15) & 1;
            u4_crc_val_u = ((u4_crc_val_u << 1) & 0xffff) ^ (u4_crc_msb * 0x1021);

            u4_crc_msb = (u4_crc_val_v >> 15) & 1;
            u4_crc_val_v = ((u4_crc_val_v << 1) & 0xffff) ^ (u4_crc_msb * 0x1021);
        }

        ps_hash_sei_params->au1_sei_hash[1][0] = (u4_crc_val_u >> 8) & 0xff;
        ps_hash_sei_params->au1_sei_hash[1][1] = u4_crc_val_u & 0xff;

        ps_hash_sei_params->au1_sei_hash[2][0] = (u4_crc_val_v >> 8) & 0xff;
        ps_hash_sei_params->au1_sei_hash[2][1] = u4_crc_val_v & 0xff;
    }
}

/**
******************************************************************************
*
*  @brief Populates Hash SEI values for Checksum Hash
*
*  @par   Description
*
*  @param[out]  ps_hash_sei_params
*  pointer to hash sei params that needs to be populated
*
*  @param[in]   bit_depth
*  i4_internal_bit_depth value. Assume same for Luma & Chroma
*
*  @param[in]   pv_y_buf
*  Pointer to decoded/recon Luma buffer
*
*  @param[in]   y_wd
*  pic width in luma samples
*
*  @param[in]   y_ht
*  pic height in luma samples
*
*  @param[in]   y_strd
*  Stride of luma buffer
*
*  @param[in]   pv_u_buf
*  Pointer to decoded/recon Chroma buffer
*
*  @param[in]   uv_wd
*  pic width in luma samples / SubWidthC
*
*  @param[in]   uv_ht
*  pic height in luma samples / SubHeightC
*
*  @param[in]   uv_strd
*  Stride of chroma buffer
*
*  @return  None
*
*****************************************************************************
*/
static void ihevce_calc_checksum(
    hash_sei_param_t *ps_hash_sei_params,
    WORD32 bit_depth,
    void *pv_y_buf,
    WORD32 y_wd,
    WORD32 y_ht,
    WORD32 y_strd,
    void *pv_u_buf,
    WORD32 uv_wd,
    WORD32 uv_ht,
    WORD32 uv_strd,
    WORD32 i4_frame_pos_x,
    WORD32 i4_frame_pos_y)
{
    WORD32 x, y;
    UWORD8 *pu_buf;
    WORD32 row, col;
    UWORD32 u4_xor_mask;
    UWORD32 gt8bit_mul = 1;

    if(bit_depth > 8)
        gt8bit_mul++;

    /* Luma Checksum */
    {
        UWORD32 u4_sum_luma = 0;

        pu_buf = (UWORD8 *)pv_y_buf;

        for(y = i4_frame_pos_y, row = 0; row < y_ht; y++, row++)
        {
            for(x = i4_frame_pos_x, col = 0; col < y_wd; x++, col++)
            {
                // take checksum of first pictureData byte
                u4_xor_mask = (x & 0xFF) ^ (y & 0xFF) ^ (x >> 8) ^ (y >> 8);
                u4_sum_luma =
                    (u4_sum_luma + ((pu_buf[(row * y_strd + col) * gt8bit_mul]) ^ u4_xor_mask)) &
                    0xFFFFFFFF;

                // take checksum of second pictureData byte if bit depth is greater than 8-bits
                if(bit_depth > 8)
                {
                    u4_sum_luma = (u4_sum_luma + ((pu_buf[(row * y_strd + col) * gt8bit_mul + 1]) ^
                                                  u4_xor_mask)) &
                                  0xFFFFFFFF;
                }
            }
        }

        ps_hash_sei_params->au1_sei_hash[0][0] = (u4_sum_luma >> 24) & 0xff;
        ps_hash_sei_params->au1_sei_hash[0][1] = (u4_sum_luma >> 16) & 0xff;
        ps_hash_sei_params->au1_sei_hash[0][2] = (u4_sum_luma >> 8) & 0xff;
        ps_hash_sei_params->au1_sei_hash[0][3] = (u4_sum_luma)&0xff;
    }

    /* Cb & Cr checksum */
    {
        UWORD32 u4_sum_cb = 0, u4_sum_cr = 0;
        pu_buf = (UWORD8 *)pv_u_buf;

        for(y = (i4_frame_pos_y / 2), row = 0; row < uv_ht; y++, row++)
        {
            for(x = (i4_frame_pos_x / 2), col = 0; col < uv_wd; x++, col += 2)
            {
                // take checksum of first pictureData byte
                u4_xor_mask = (x & 0xFF) ^ (y & 0xFF) ^ (x >> 8) ^ (y >> 8);
                u4_sum_cb =
                    (u4_sum_cb + ((pu_buf[(row * uv_strd + (col)) * gt8bit_mul]) ^ u4_xor_mask)) &
                    0xFFFFFFFF;
                u4_sum_cr = (u4_sum_cr +
                             ((pu_buf[(row * uv_strd + (col + 1)) * gt8bit_mul]) ^ u4_xor_mask)) &
                            0xFFFFFFFF;

                // take checksum of second pictureData byte if bit depth is greater than 8-bits
                if(bit_depth > 8)
                {
                    u4_sum_cb = (u4_sum_cb + ((pu_buf[(row * uv_strd + (col)) * gt8bit_mul + 1]) ^
                                              u4_xor_mask)) &
                                0xFFFFFFFF;
                    u4_sum_cr =
                        (u4_sum_cr +
                         ((pu_buf[(row * uv_strd + (col + 1)) * gt8bit_mul + 1]) ^ u4_xor_mask)) &
                        0xFFFFFFFF;
                }
            }
        }

        ps_hash_sei_params->au1_sei_hash[1][0] = (u4_sum_cb >> 24) & 0xff;
        ps_hash_sei_params->au1_sei_hash[1][1] = (u4_sum_cb >> 16) & 0xff;
        ps_hash_sei_params->au1_sei_hash[1][2] = (u4_sum_cb >> 8) & 0xff;
        ps_hash_sei_params->au1_sei_hash[1][3] = (u4_sum_cb)&0xff;

        ps_hash_sei_params->au1_sei_hash[2][0] = (u4_sum_cr >> 24) & 0xff;
        ps_hash_sei_params->au1_sei_hash[2][1] = (u4_sum_cr >> 16) & 0xff;
        ps_hash_sei_params->au1_sei_hash[2][2] = (u4_sum_cr >> 8) & 0xff;
        ps_hash_sei_params->au1_sei_hash[2][3] = (u4_sum_cr)&0xff;
    }
}

/**
******************************************************************************
*
*  @brief Populates Hash SEI values
*
*  @par   Description
*
*  @param[out]  ps_sei
*  pointer to sei params that needs to be populated
*
*  @param[in]   bit_depth
*  i4_internal_bit_depth value. Assume same for Luma & Chroma
*
*  @param[in]   pv_y_buf
*  Pointer to decoded/recon Luma buffer
*
*  @param[in]   y_wd
*  pic width in luma samples
*
*  @param[in]   y_ht
*  pic height in luma samples
*
*  @param[in]   y_strd
*  Stride of luma buffer
*
*  @param[in]   pv_u_buf
*  Pointer to decoded/recon Chroma buffer
*
*  @param[in]   uv_wd
*  pic width in luma samples / SubWidthC
*
*  @param[in]   uv_ht
*  pic height in luma samples / SubHeightC
*
*  @param[in]   uv_strd
*  Stride of chroma buffer
*
*  @return      success or failure error code
*
*****************************************************************************
*/
WORD32 ihevce_populate_hash_sei(
    sei_params_t *ps_sei,
    WORD32 bit_depth,
    void *pv_y_buf,
    WORD32 y_wd,
    WORD32 y_ht,
    WORD32 y_strd,
    void *pv_u_buf,
    WORD32 uv_wd,
    WORD32 uv_ht,
    WORD32 uv_strd,
    WORD32 i4_frame_pos_x,
    WORD32 i4_frame_pos_y)
{
    hash_sei_param_t *ps_hash_sei_params = &ps_sei->s_hash_sei_params;

    if(1 == ps_sei->i1_decoded_pic_hash_sei_flag)
    {
        ASSERT(0);  // Not supported now!
    }
    else if(2 == ps_sei->i1_decoded_pic_hash_sei_flag)
    {
        /* calculate CRC for entire reconstructed picture */
        ihevce_calc_CRC(
            ps_hash_sei_params,
            bit_depth,
            pv_y_buf,
            y_wd,
            y_ht,
            y_strd,
            pv_u_buf,
            uv_wd,
            uv_ht,
            uv_strd);
    }
    else if(3 == ps_sei->i1_decoded_pic_hash_sei_flag)
    {
        /* calculate Checksum for entire reconstructed picture */
        ihevce_calc_checksum(
            ps_hash_sei_params,
            bit_depth,
            pv_y_buf,
            y_wd,
            y_ht,
            y_strd,
            pv_u_buf,
            uv_wd,
            uv_ht,
            uv_strd,
            i4_frame_pos_x,
            i4_frame_pos_y);
    }
    else
    {
        ASSERT(0);
    }

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates vui structure
*
*  @par   Description
*  Populates vui structure for its use in header generation
*
*  @param[out]  ps_vui
*  pointer to vui params that needs to be populated
*
*  @param[out]  ps_sps
*  pointer to sps params referred by vui
*
*  @param[in]   ps_src_params
*  pointer to source config params; resolution, frame rate etc
*
*  @param[out]  ps_vui_sei_prms
*  pointer to sps params referred by application
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_vui(
    vui_t *ps_vui,
    sps_t *ps_sps,
    ihevce_src_params_t *ps_src_params,
    ihevce_vui_sei_params_t *ps_vui_sei_prms,
    WORD32 i4_resolution_id,
    ihevce_tgt_params_t *ps_tgt_params,
    ihevce_static_cfg_params_t *ps_stat_prms,
    WORD32 i4_bitrate_instance_id)
{
    WORD32 i, j, i4_range_idr, i4_range_cdr;
    ULWORD64 max_vbv_size;

    ps_vui->u1_aspect_ratio_info_present_flag = ps_vui_sei_prms->u1_aspect_ratio_info_present_flag;

    ps_vui->u1_aspect_ratio_idc = ps_vui_sei_prms->au1_aspect_ratio_idc[i4_resolution_id];

    ps_vui->u2_sar_height = ps_vui_sei_prms->au2_sar_height[i4_resolution_id];

    ps_vui->u2_sar_width = ps_vui_sei_prms->au2_sar_width[i4_resolution_id];

    ps_vui->u1_overscan_info_present_flag = ps_vui_sei_prms->u1_overscan_info_present_flag;

    ps_vui->u1_overscan_appropriate_flag = ps_vui_sei_prms->u1_overscan_appropriate_flag;

    ps_vui->u1_video_signal_type_present_flag = ps_vui_sei_prms->u1_video_signal_type_present_flag;

    ps_vui->u1_video_format = ps_vui_sei_prms->u1_video_format;

    ps_vui->u1_video_full_range_flag = ps_vui_sei_prms->u1_video_full_range_flag;

    ps_vui->u1_colour_description_present_flag =
        ps_vui_sei_prms->u1_colour_description_present_flag;

    ps_vui->u1_colour_primaries = ps_vui_sei_prms->u1_colour_primaries;

    ps_vui->u1_transfer_characteristics = ps_vui_sei_prms->u1_transfer_characteristics;

    ps_vui->u1_matrix_coefficients = ps_vui_sei_prms->u1_matrix_coefficients;

    ps_vui->u1_chroma_loc_info_present_flag = ps_vui_sei_prms->u1_chroma_loc_info_present_flag;

    ps_vui->u1_chroma_sample_loc_type_top_field =
        ps_vui_sei_prms->u1_chroma_sample_loc_type_top_field;

    ps_vui->u1_chroma_sample_loc_type_bottom_field =
        ps_vui_sei_prms->u1_chroma_sample_loc_type_bottom_field;

    ps_vui->u1_neutral_chroma_indication_flag = 0;

    ps_vui->u1_default_display_window_flag = 0;

    /* Default Values for display offset added */
    if(ps_vui->u1_default_display_window_flag)
    {
        ps_vui->u4_def_disp_win_bottom_offset = 0;

        ps_vui->u4_def_disp_win_left_offset = 0;

        ps_vui->u4_def_disp_win_right_offset = 0;

        ps_vui->u4_def_disp_win_top_offset = 0;
    }

    ps_vui->u1_vui_hrd_parameters_present_flag =
        ps_vui_sei_prms->u1_vui_hrd_parameters_present_flag;

    ps_vui->u1_field_seq_flag = ps_src_params->i4_field_pic;

    ps_vui->u1_frame_field_info_present_flag = 1;

    ps_vui->u1_vui_timing_info_present_flag = ps_vui_sei_prms->u1_timing_info_present_flag;

    //if(ps_vui->u1_vui_timing_info_present_flag)
    {
        /* NumUnits in tick is same as the frame rate denominator assuming delta poc as 1 */
        ps_vui->u4_vui_num_units_in_tick = ps_src_params->i4_frm_rate_denom;

        /* TimeScale is the same as the frame rate numerator assuming delta poc as 1      */
        ps_vui->u4_vui_time_scale =
            (ps_src_params->i4_frm_rate_num / ps_tgt_params->i4_frm_rate_scale_factor);
    }

    ps_vui->u1_poc_proportional_to_timing_flag = 1;

    if(ps_vui->u1_poc_proportional_to_timing_flag && ps_vui->u1_vui_timing_info_present_flag)
        ps_vui->u4_num_ticks_poc_diff_one_minus1 = 0;

    //if (ps_vui->u1_vui_hrd_parameters_present_flag)
    {
        ps_vui->s_vui_hrd_parameters.u1_initial_cpb_removal_delay_length_minus1 = 23;
        ps_vui->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 = 23; /* Default value */

        ps_vui->s_vui_hrd_parameters.u1_dpb_output_delay_length_minus1 =
            4;  // max num of B pics are 7. So the max delay can go up to 5 and a maximun 10 is allowed for initial removal dalay.

        ps_vui->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag =
            ps_vui_sei_prms->u1_nal_hrd_parameters_present_flag;

        ps_vui->s_vui_hrd_parameters.u1_vcl_hrd_parameters_present_flag =
            0;  //ps_vui_sei_prms->u1_vcl_hrd_parameters_present_flag;
        ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag = 0;

        if(ps_vui->s_vui_hrd_parameters.u1_nal_hrd_parameters_present_flag ||
           ps_vui->s_vui_hrd_parameters.u1_vcl_hrd_parameters_present_flag)
        {
            /* Initialize  u1_au_cpb_removal_delay_length_minus1 based on configured intra periods */
            ps_vui->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 =
                8; /* Default value when HRD params are enabled */
            if(ps_stat_prms->s_coding_tools_prms.i4_max_cra_open_gop_period ||
               ps_stat_prms->s_coding_tools_prms.i4_max_closed_gop_period)
            {
                GETRANGE(
                    i4_range_cdr, ps_stat_prms->s_coding_tools_prms.i4_max_cra_open_gop_period);

                GETRANGE(i4_range_idr, ps_stat_prms->s_coding_tools_prms.i4_max_closed_gop_period);

                ps_vui->s_vui_hrd_parameters.u1_au_cpb_removal_delay_length_minus1 =
                    MAX(i4_range_cdr, i4_range_idr);
            }
            /*BLU_RAY Default set to 0 */
            ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag = 0;
            if(ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
            {
                ps_vui->s_vui_hrd_parameters.u1_tick_divisor_minus2 = 1;
                ps_vui->s_vui_hrd_parameters.u1_du_cpb_removal_delay_increment_length_minus1 = 23;
                ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_in_pic_timing_sei_flag = 1;
                ps_vui->s_vui_hrd_parameters.u1_dpb_output_delay_du_length_minus1 = 0;
            }
        }

        ps_vui->s_vui_hrd_parameters.u4_bit_rate_scale = VUI_BIT_RATE_SCALE;
        ps_vui->s_vui_hrd_parameters.u4_cpb_size_scale = VUI_CPB_SIZE_SCALE;
        if(ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
        {
            ps_vui->s_vui_hrd_parameters.u4_cpb_size_du_scale = 0;
        }

        for(i = 0; i <= (ps_sps->i1_sps_max_sub_layers - 1); i++)
        {
            ps_vui->s_vui_hrd_parameters.au1_fixed_pic_rate_general_flag[i] =
                1; /*BLU_RAY specific change already done */
            ps_vui->s_vui_hrd_parameters.au1_fixed_pic_rate_within_cvs_flag[i] = 1;
            ps_vui->s_vui_hrd_parameters.au2_elemental_duration_in_tc_minus1[i] = 0;

            /*BLU_RAY low_delay_hrd_flag is always set to 0*/
            ps_vui->s_vui_hrd_parameters.au1_low_delay_hrd_flag[i] = 0;

            /************************************************************************/
            /* cpb_cnt_minus1 is set to zero because we assume that the decoder     */
            /* can work with just one CPB specification                             */
            /************************************************************************/
            ps_vui->s_vui_hrd_parameters.au1_cpb_cnt_minus1[i] = 0;

            max_vbv_size = ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                               .ai4_max_vbv_buffer_size[i4_bitrate_instance_id];
            for(j = 0; j <= ps_vui->s_vui_hrd_parameters.au1_cpb_cnt_minus1[i]; j++)
            {
                ULWORD64 u8_bit_rate_val =
                    (ULWORD64)ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                        .ai4_tgt_bitrate[i4_bitrate_instance_id];
                ULWORD64 u8_max_cpb_size;
                if((ps_stat_prms->s_config_prms.i4_rate_control_mode == 2) ||
                   (ps_stat_prms->s_config_prms.i4_rate_control_mode ==
                    1))  // VBR/Capped VBR rate control mode
                    u8_bit_rate_val =
                        (ULWORD64)(ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                                       .ai4_peak_bitrate[i4_bitrate_instance_id]);
                u8_max_cpb_size =
                    max_vbv_size;  //((ULWORD64)(max_vbv_size * u8_bit_rate_val)/1000);

                if(3 == ps_stat_prms->s_config_prms.i4_rate_control_mode)
                {
                    /* For CQP mode, assume Level specified max rate and buffer size */
                    WORD32 codec_level_index = ihevce_get_level_index(
                        ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level);
                    WORD32 codec_tier = ps_stat_prms->s_out_strm_prms.i4_codec_tier;

                    /* Bitrate as per level and tier limits */
                    u8_bit_rate_val =
                        g_as_level_data[codec_level_index].i4_max_bit_rate[codec_tier] *
                        CBP_VCL_FACTOR;
                    u8_max_cpb_size =
                        g_as_level_data[codec_level_index].i4_max_cpb[codec_tier] * CBP_VCL_FACTOR;
                }

                u8_bit_rate_val >>= (6 + ps_vui->s_vui_hrd_parameters.u4_bit_rate_scale);
                u8_max_cpb_size >>= (4 + ps_vui->s_vui_hrd_parameters.u4_cpb_size_scale);

                ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[i]
                    .au4_bit_rate_value_minus1[j] = (UWORD32)(u8_bit_rate_val - 1);
                ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[i]
                    .au4_cpb_size_value_minus1[j] = (UWORD32)(u8_max_cpb_size - 1);

                if(ps_vui->s_vui_hrd_parameters.u1_sub_pic_cpb_params_present_flag)
                {
                    ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[i]
                        .au4_cpb_size_value_minus1[j] = 0;
                }

                /************************************************************************/
                /* CBR flag is set as per the RATE_CONTROL macro                        */
                /************************************************************************/

                /* Default cbr falg setting. will discard Decoder buffer overflows ( No stuffing required)*/

                ps_vui->s_vui_hrd_parameters.as_sub_layer_hrd_params[i].au1_cbr_flag[j] = 0;
            }
        }
    }

    ps_vui->u1_bitstream_restriction_flag = 0;

    if(ps_vui->u1_bitstream_restriction_flag)
    {
        ps_vui->u1_tiles_fixed_structure_flag = 1;

        ps_vui->u1_motion_vectors_over_pic_boundaries_flag = 1;

        ps_vui->u4_min_spatial_segmentation_idc = 0;

        ps_vui->u1_restricted_ref_pic_lists_flag = 0;

        ps_vui->u1_max_bytes_per_pic_denom = 2;

        ps_vui->u1_max_bits_per_mincu_denom = 1;

        ps_vui->u1_log2_max_mv_length_horizontal = 15;

        ps_vui->u1_log2_max_mv_length_vertical = 15;
    }

    return IHEVCE_SUCCESS;
}
