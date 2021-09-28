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
* \file ihevce_error_checks.c
*
* \brief
*    This file contains all the functions which checks the validity of the
*    parameters passed to the encoder.
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* List of Functions
*    ihevce_get_level_index()
*    ihevce_hle_validate_static_params()
*    ihevce_validate_tile_config_params()
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
#include "ihevce_hle_interface.h"
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
#include "ihevce_trace.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_validate_tile_config_params \endif
*
* \brief
*    This function validates the static parameters related to tiles
*
* \param[in] Encoder static config prms pointer
*
* \return
*    None
*
* \author
*    Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_validate_tile_config_params(ihevce_static_cfg_params_t *ps_static_cfg_prms)
{
    WORD32 error_code = IHEVCE_SUCCESS;
    ihevce_sys_api_t *ps_sys_api = &ps_static_cfg_prms->s_sys_api;
    void *pv_cb_handle = ps_sys_api->pv_cb_handle;

    /* As of now tiles are not supported */
    if(ps_static_cfg_prms->s_app_tile_params.i4_tiles_enabled_flag != 0)
    {
        error_code = IHEVCE_BAD_TILE_CONFIGURATION;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_tiles_enabled_flag should be set to 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    return error_code;
}

/*!
******************************************************************************
* \if Function name : ihevce_hle_validate_static_params \endif
*
* \brief
*    This function validates the static parameters before creating the encoder
*    instance.
*
* \param[in] Encoder context pointer
*
* \return
*    Error code
*
* \author
*    Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_hle_validate_static_params(ihevce_static_cfg_params_t *ps_static_cfg_prms)
{
    WORD32 error_code;
    WORD32 i4_resolution_id;
    WORD32 ai4_num_bitrate_instances[IHEVCE_MAX_NUM_RESOLUTIONS] = { 1 };
    WORD32 i4_num_resolutions;
    ihevce_sys_api_t *ps_sys_api = &ps_static_cfg_prms->s_sys_api;
    void *pv_cb_handle = ps_sys_api->pv_cb_handle;

    /* derive local variables */
    i4_num_resolutions = ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;
    for(i4_resolution_id = 0; i4_resolution_id < i4_num_resolutions; i4_resolution_id++)
    {
        ai4_num_bitrate_instances[i4_resolution_id] =
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                .i4_num_bitrate_instances;
    }
    // clang-format off
    if(0 != ps_static_cfg_prms->i4_log_dump_level)
    {
        /* Print all the config params */
        if((0 == ps_static_cfg_prms->i4_res_id) && (0 == ps_static_cfg_prms->i4_br_id))
        {
            WORD32 i4_resolution_id_loop, i4_i;
            WORD32 i4_num_res_layers = ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "**********************************************\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "*********** STATIC PARAMS CONFIG *************\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "**********************************************\n");

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : ps_static_cfg_prms->s_src_prms \n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_width %d                    \n", ps_static_cfg_prms->s_src_prms.i4_width);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_height %d                   \n", ps_static_cfg_prms->s_src_prms.i4_height);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_frm_rate_num %d             \n", ps_static_cfg_prms->s_src_prms.i4_frm_rate_num);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_frm_rate_denom %d           \n", ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_field_pic %d                \n", ps_static_cfg_prms->s_src_prms.i4_field_pic);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_chr_format %d               \n", ps_static_cfg_prms->s_src_prms.i4_chr_format);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_input_bit_depth %d          \n", ps_static_cfg_prms->s_src_prms.i4_input_bit_depth);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_topfield_first %d           \n\n", ps_static_cfg_prms->s_src_prms.i4_topfield_first);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : ps_static_cfg_prms->s_tgt_lyr_prms \n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_num_res_layers %d               \n", i4_num_res_layers);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_multi_res_layer_reuse %d        \n", ps_static_cfg_prms->s_tgt_lyr_prms.i4_multi_res_layer_reuse);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_mbr_quality_setting %d          \n", ps_static_cfg_prms->s_tgt_lyr_prms.i4_mbr_quality_setting);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : For Each resolution,");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : i4_target_width ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop, ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_width);
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : i4_target_width ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop, ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_height);
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : i4_frm_rate_scale_factor ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop,
                    ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_frm_rate_scale_factor);
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : i4_codec_level ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop, ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_codec_level);
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : i4_num_bitrate_instances ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d", i4_resolution_id_loop,
                    ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_num_bitrate_instances);
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                WORD32 i4_num_bitrate_instances, i4_br_loop;
                i4_num_bitrate_instances = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_num_bitrate_instances;
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_tgt_bitrate res_id %d ", i4_resolution_id_loop);
                for(i4_br_loop = 0; i4_br_loop < i4_num_bitrate_instances; i4_br_loop++)
                {
                    PRINTF(
                        ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "br_id %d %d ", i4_br_loop, ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].ai4_tgt_bitrate[i4_br_loop]);
                }
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_peak_bitrate res_id %d ", i4_resolution_id_loop);
                for(i4_br_loop = 0; i4_br_loop < i4_num_bitrate_instances; i4_br_loop++)
                {
                    PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "br_id %d %d ", i4_br_loop,
                        ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].ai4_peak_bitrate[i4_br_loop]);
                }
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : vbv_buffer_size res_id %d ", i4_resolution_id_loop);
                for(i4_br_loop = 0; i4_br_loop < i4_num_bitrate_instances; i4_br_loop++)
                {
                    PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "br_id %d %d ", i4_br_loop,
                        ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].ai4_max_vbv_buffer_size[i4_br_loop]);
                }
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");
            }

            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                WORD32 i4_num_bitrate_instances, i4_br_loop;

                i4_num_bitrate_instances = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_num_bitrate_instances;
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_frame_qp res_id %d ", i4_resolution_id_loop);
                for(i4_br_loop = 0; i4_br_loop < i4_num_bitrate_instances; i4_br_loop++)
                {
                    PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "br_id %d %d ", i4_br_loop, ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].ai4_frame_qp[i4_br_loop]);
                }
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_internal_bit_depth %d               \n", ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_enable_temporal_scalability %d               \n", ps_static_cfg_prms->s_tgt_lyr_prms.i4_enable_temporal_scalability);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_quality_preset ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d", i4_resolution_id_loop, ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id_loop].i4_quality_preset);
            }
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_coding_tools_prms \n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_idr_period %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_min_idr_period %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_min_closed_gop_period);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_cra_period %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_i_cra_period %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_temporal_layers %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_reference_frames %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_max_reference_frames);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_deblocking_type %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_deblocking_type);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_use_default_sc_mtx %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_use_default_sc_mtx);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_enable_entropy_sync %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_enable_entropy_sync);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_cropping_mode %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_cropping_mode);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_vqet %d \n", ps_static_cfg_prms->s_coding_tools_prms.i4_vqet);

            switch(ps_static_cfg_prms->e_arch_type)
            {
            case ARCH_NA:
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : archType %d \n", 0);
                break;
#ifdef ARM
            case ARCH_ARM_NONEON:
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : archType %d \n", 4);
                break;
#endif
            default:
                break;
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_config_prms \n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_num_frms_to_encode %d \n", ps_static_cfg_prms->s_config_prms.i4_num_frms_to_encode);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_log2_cu_size %d \n", ps_static_cfg_prms->s_config_prms.i4_max_log2_cu_size);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_log2_cu_size %d \n", ps_static_cfg_prms->s_config_prms.i4_min_log2_cu_size);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_log2_cu_size %d \n", ps_static_cfg_prms->s_config_prms.i4_max_log2_tu_size);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_log2_cu_size %d \n", ps_static_cfg_prms->s_config_prms.i4_min_log2_cu_size);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_tr_tree_depth_I %d \n", ps_static_cfg_prms->s_config_prms.i4_max_tr_tree_depth_I);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_tr_tree_depth_nI %d \n", ps_static_cfg_prms->s_config_prms.i4_max_tr_tree_depth_nI);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_search_range_horz %d \n", ps_static_cfg_prms->s_config_prms.i4_max_search_range_horz);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_search_range_vert %d \n", ps_static_cfg_prms->s_config_prms.i4_max_search_range_vert);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_multi_thrd_prms \n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_num_cores %d \n", ps_static_cfg_prms->s_multi_thrd_prms.i4_max_num_cores);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_use_thrd_affinity %d \n", ps_static_cfg_prms->s_multi_thrd_prms.i4_use_thrd_affinity);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : rate control params \n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_rate_control_mode %d \n", ps_static_cfg_prms->s_config_prms.i4_rate_control_mode);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_cu_level_rc %d \n", ps_static_cfg_prms->s_config_prms.i4_cu_level_rc);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_pass %d \n", ps_static_cfg_prms->s_pass_prms.i4_pass);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_vbr_max_peak_rate_dur %d \n", ps_static_cfg_prms->s_config_prms.i4_vbr_max_peak_rate_dur);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_init_vbv_fullness %d \n", ps_static_cfg_prms->s_config_prms.i4_init_vbv_fullness);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_stuffing_enable %d \n", ps_static_cfg_prms->s_config_prms.i4_stuffing_enable);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_max_frame_qp %d \n", ps_static_cfg_prms->s_config_prms.i4_max_frame_qp);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_min_frame_qp %d \n", ps_static_cfg_prms->s_config_prms.i4_min_frame_qp);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\n");

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_lap_prms\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_rc_look_ahead_pics %d \n", ps_static_cfg_prms->s_lap_prms.i4_rc_look_ahead_pics);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_enable_wts_ofsts %d \n", ps_static_cfg_prms->s_lap_prms.i4_enable_wts_ofsts);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_out_strm_prms\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_codec_type %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_codec_type);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_codec_profile %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_codec_profile);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_codec_tier %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_aud_enable_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_aud_enable_flags);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_interop_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_interop_flags);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sps_at_cdr_enable %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sps_at_cdr_enable);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_vui_enable %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_vui_enable);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_enable_flag %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_enable_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_payload_enable_flag %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_payload_enable_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_buffer_period_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_buffer_period_flags);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_pic_timing_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_pic_timing_flags);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_cll_enable %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_cll_enable);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u2_sei_avg_cll %d \n", ps_static_cfg_prms->s_out_strm_prms.u2_sei_avg_cll);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u2_sei_max_cll %d \n", ps_static_cfg_prms->s_out_strm_prms.u2_sei_max_cll);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_recovery_point_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_recovery_point_flags);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_mastering_disp_colour_vol_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags);
            for(i4_i = 0; i4_i < 3; i4_i++)
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u2_display_primaries_x[i4_i] %d \n", ps_static_cfg_prms->s_out_strm_prms.au2_display_primaries_x[i4_i]);
            for(i4_i = 0; i4_i < 3; i4_i++)
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u2_display_primaries_y[i4_i] %d \n", ps_static_cfg_prms->s_out_strm_prms.au2_display_primaries_y[i4_i]);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u2_white_point_x %d \n", ps_static_cfg_prms->s_out_strm_prms.u2_white_point_x);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u2_white_point_y %d \n", ps_static_cfg_prms->s_out_strm_prms.u2_white_point_y);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u4_max_display_mastering_luminance %d \n", ps_static_cfg_prms->s_out_strm_prms.u4_max_display_mastering_luminance);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u4_min_display_mastering_luminance %d \n", ps_static_cfg_prms->s_out_strm_prms.u4_min_display_mastering_luminance);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_sei_hash_flags %d \n", ps_static_cfg_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_app_tile_params\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_tiles_enabled_flag %d \n", ps_static_cfg_prms->s_app_tile_params.i4_tiles_enabled_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_uniform_spacing_flag %d \n", ps_static_cfg_prms->s_app_tile_params.i4_uniform_spacing_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_num_tile_cols %d \n", ps_static_cfg_prms->s_app_tile_params.i4_num_tile_cols);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_num_tile_rows %d \n", ps_static_cfg_prms->s_app_tile_params.i4_num_tile_rows);

            for(i4_i = 0; i4_i < ps_static_cfg_prms->s_app_tile_params.i4_num_tile_cols; i4_i++)
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_column_width[i4_i] %d \n", ps_static_cfg_prms->s_app_tile_params.ai4_column_width[i4_i]);

            for(i4_i = 0; i4_i < ps_static_cfg_prms->s_app_tile_params.i4_num_tile_rows; i4_i++)
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_row_height[i4_i] %d \n", ps_static_cfg_prms->s_app_tile_params.ai4_row_height[i4_i]);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_slice_params\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_slice_segment_mode %d \n", ps_static_cfg_prms->s_slice_params.i4_slice_segment_mode);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_slice_segment_argument %d \n", ps_static_cfg_prms->s_slice_params.i4_slice_segment_argument);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms->s_vui_sei_prms\n");
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_aspect_ratio_info_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_aspect_ratio_info_present_flag);

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_aspect_ratio_idc ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop, ps_static_cfg_prms->s_vui_sei_prms.au1_aspect_ratio_idc[i4_resolution_id_loop]);
            }

            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : au2_sar_width ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop, ps_static_cfg_prms->s_vui_sei_prms.au2_sar_width[i4_resolution_id_loop]);
            }
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : au2_sar_width ");
            for(i4_resolution_id_loop = 0; i4_resolution_id_loop < i4_num_res_layers; i4_resolution_id_loop++)
            {
                PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "res_id %d %d ", i4_resolution_id_loop, ps_static_cfg_prms->s_vui_sei_prms.au2_sar_height[i4_resolution_id_loop]);
            }
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : u1_overscan_info_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_overscan_info_present_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_overscan_appropriate_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_overscan_appropriate_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_video_signal_type_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_video_signal_type_present_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_video_format %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_video_format);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_video_full_range_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_video_full_range_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_colour_description_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_colour_description_present_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_colour_primaries %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_colour_primaries);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_transfer_characteristics %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_transfer_characteristics);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_matrix_coefficients %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_matrix_coefficients);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_chroma_loc_info_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_chroma_loc_info_present_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_chroma_sample_loc_type_top_field %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_chroma_sample_loc_type_top_field);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_chroma_sample_loc_type_bottom_field %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_chroma_sample_loc_type_bottom_field);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_timing_info_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_timing_info_present_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_vui_hrd_parameters_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_vui_hrd_parameters_present_flag);
            PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : u1_nal_hrd_parameters_present_flag %d \n", ps_static_cfg_prms->s_vui_sei_prms.u1_nal_hrd_parameters_present_flag);
        }

        PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "\nIHEVCE : ps_static_cfg_prms \n");
        PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_enable_logo %d                   \n", ps_static_cfg_prms->i4_enable_logo);
        PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_log_dump_level %d                \n", ps_static_cfg_prms->i4_log_dump_level);
        PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "IHEVCE : i4_save_recon %d                    \n", ps_static_cfg_prms->i4_save_recon);

        PRINTF(ps_sys_api->pv_cb_handle, i4_res_id, i4_br_id, "**********************************************\n");
    }
    // clang-format on

    if(ps_static_cfg_prms->s_multi_thrd_prms.i4_num_proc_groups > MAX_NUMBER_PROC_GRPS)
    {
        error_code = IHEVCE_UNSUPPORTED_PROC_CONFIG;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR:  Number of Processor Groups not supported \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* Error check for system-api callback functions */
    if(NULL == ps_sys_api->ihevce_printf)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fopen)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fopen callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fclose)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fclose callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fflush)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fflush callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fseek)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fseek callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fread)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fread callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fscanf)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fscanf callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->ihevce_sscanf)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_sscanf callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fprintf)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fprintf callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->s_file_io_api.ihevce_fwrite)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_fwrite callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(NULL == ps_sys_api->ihevce_sprintf)
    {
        error_code = IHEVCE_SYSTEM_APIS_NOT_INITIALLIZED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: ihevce_sprintf callback function not initiallized\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* Error check for static source parameters */
    if((ps_static_cfg_prms->s_src_prms.i4_orig_width > HEVCE_MAX_WIDTH) ||
       (ps_static_cfg_prms->s_src_prms.i4_orig_width < 2))
    {
        error_code = IHEVCE_WIDTH_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR:  i4_src_width out of range \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    if((ps_static_cfg_prms->s_src_prms.i4_orig_height > HEVCE_MAX_HEIGHT) ||
       (ps_static_cfg_prms->s_src_prms.i4_orig_height < 2))
    {
        error_code = IHEVCE_HEIGHT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR:  i4_src_height out of range \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    /*check for odd resolution*/
    if(0 != (ps_static_cfg_prms->s_src_prms.i4_width & 1))
    {
        error_code = IHEVCE_WIDTH_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR:  i4_src_width not supported \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(0 != (ps_static_cfg_prms->s_src_prms.i4_height & 1))
    {
        error_code = IHEVCE_HEIGHT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR:  i4_src_height not supported \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    if((ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom != 1000) &&
       (ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom != 1001))
    {
        error_code = IHEVCE_FRAME_RATE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: frame rate denom not supported \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if((((ps_static_cfg_prms->s_src_prms.i4_frm_rate_num * 1.0) /
          ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom) > MAX_FRAME_RATE) ||
       (((ps_static_cfg_prms->s_src_prms.i4_frm_rate_num * 1.0) /
          ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom) < MIN_FRAME_RATE))
    {
        error_code = IHEVCE_FRAME_RATE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Frame rate (%d / %d) is out of range [%.1f - %.1f]\n",
            ps_static_cfg_prms->s_src_prms.i4_frm_rate_num,
            ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom,
            MIN_FRAME_RATE, MAX_FRAME_RATE);
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_src_prms.i4_field_pic != 0)
    {
        error_code = IHEVCE_CONTENT_TYPE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: Field encoding not supported \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_src_prms.inp_chr_format != IV_YUV_420SP_UV &&
       ps_static_cfg_prms->s_src_prms.inp_chr_format != IV_YUV_420P)
    {
        error_code = IHEVCE_CHROMA_FORMAT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_input_chroma_format Invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_src_prms.i4_chr_format != IV_YUV_420SP_UV)
    {
        error_code = IHEVCE_CHROMA_FORMAT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_internal_chroma_format Invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    /* Check error for interoperability flags */
    if(ps_static_cfg_prms->s_out_strm_prms.i4_interop_flags != 0)
    {
        error_code = IHEVCE_INTEROPERABILITY_FLAG_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_interop_flags out of range, to be set to 0\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* Error check for static output stream parameters  */
    if(ps_static_cfg_prms->s_out_strm_prms.i4_codec_type != 0)
    {
        error_code = IHEVCE_CODEC_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_codec_type should be set to 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_out_strm_prms.i4_codec_profile != 1)
    {
        error_code = IHEVCE_CODEC_PROFILE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_codec_profile should be set to 1 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth != 8)
    {
        error_code = IHEVCE_OUTPUT_BIT_DEPTH_OUT_OF_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: (output_bit_depth = %d) not supported \n",
            ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth);
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_src_prms.i4_input_bit_depth != 8)
    {
        error_code = IHEVCE_INPUT_BIT_DEPTH_OUT_OF_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_input_bit_depth value not supported \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if((ps_static_cfg_prms->s_out_strm_prms.i4_vui_enable > 1) ||
       (ps_static_cfg_prms->s_out_strm_prms.i4_vui_enable < 0))
    {
        error_code = IHEVCE_VUI_ENABLE_OUT_OF_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_vui_enable should be set to 1 or 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if((ps_static_cfg_prms->s_out_strm_prms.i4_sei_enable_flag > 1) ||
       (ps_static_cfg_prms->s_out_strm_prms.i4_sei_enable_flag < 0))
    {
        error_code = IHEVCE_SEI_ENABLE_OUT_OF_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_sei_enable_flags should be set to 1 or 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if((ps_static_cfg_prms->s_out_strm_prms.i4_sei_payload_enable_flag > 1) ||
       (ps_static_cfg_prms->s_out_strm_prms.i4_sei_payload_enable_flag < 0))
    {
        error_code = IHEVCE_SEI_PAYLOAD_ENABLE_OUT_OF_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_sei_payload_enable_flag should be set to 1 or 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }
    if((ps_static_cfg_prms->s_multi_thrd_prms.i4_max_num_cores > MAX_NUM_CORES) ||
       (ps_static_cfg_prms->s_multi_thrd_prms.i4_max_num_cores < 1))
    {
        error_code = IHEVCE_INVALID_CORE_CONFIG;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: Invalid Number of Cores configured\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if((ps_static_cfg_prms->e_arch_type != ARCH_NA) &&
       (ps_static_cfg_prms->e_arch_type != ARCH_ARM_NONEON))
    {
        error_code = IHEVCE_ARCHITECTURE_TYPE_UNSUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: unsupported archType \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_vqet != 0)
    {
        error_code = IHEVCE_VISUAL_QUALITY_ENHANCEMENTS_TOGGLER_VALUE_UNSUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: visual_quality_enhancements_toggler should be set to 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers < 0 ||
       ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers > 3)
    {
        error_code = IHEVCE_TEMPORAL_LAYERS_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_max_temporal_layers out of range \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if((ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period < 0) ||
       (ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period < 0) ||
       (ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period < 0))
    {
        error_code = IHEVCE_INVALID_GOP_PERIOD;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: gop period is not valid for the configured temporal layers\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    {
        WORD32 sub_gop_size = (1 << ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers)
                              << ps_static_cfg_prms->s_src_prms.i4_field_pic;
        WORD32 i4_max_idr_period, i4_min_idr_period, i4_max_cra_period, i4_max_i_period;
        WORD32 i4_max_i_distance;
        WORD32 i4_min_i_distance = 0, i4_non_zero_idr_period = 0x7FFFFFFF,
               i4_non_zero_cra_period = 0x7FFFFFFF, i4_non_zero_i_period = 0x7FFFFFFF;
        i4_max_idr_period = ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period;
        i4_min_idr_period = ps_static_cfg_prms->s_coding_tools_prms.i4_min_closed_gop_period;
        i4_max_cra_period = ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period;
        i4_max_i_period = ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period;
        i4_max_i_distance = MAX(MAX(i4_max_idr_period, i4_max_cra_period), i4_max_i_period);

        if(sub_gop_size > 1)
        {
            switch(sub_gop_size)
            {
            case 2:
                ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
                    ALIGN2(i4_max_idr_period);

                if(i4_max_idr_period > 1)
                    ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
                        ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period + 1;

                ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period =
                    ALIGN2(i4_max_cra_period);
                ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period =
                    ALIGN2(i4_max_i_period);
                break;
            case 4:
                ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
                    ALIGN4(i4_max_idr_period);

                if(i4_max_idr_period > 1)
                    ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
                        ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period + 1;

                ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period =
                    ALIGN4(i4_max_cra_period);
                ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period =
                    ALIGN4(i4_max_i_period);
                break;
            case 8:
                ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
                    ALIGN8(i4_max_idr_period);

                if(i4_max_idr_period > 1)
                    ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
                        ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period + 1;

                ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period =
                    ALIGN8(i4_max_cra_period);
                ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period =
                    ALIGN8(i4_max_i_period);
                break;
            }
        }

        if(0 != i4_max_idr_period)
        {
            i4_non_zero_idr_period = i4_max_idr_period;
        }
        if(0 != i4_max_cra_period)
        {
            i4_non_zero_cra_period = i4_max_cra_period;
        }
        if(0 != i4_max_i_period)
        {
            i4_non_zero_i_period = i4_max_i_period;
        }
        i4_min_i_distance =
            MIN(MIN(i4_non_zero_idr_period, i4_non_zero_cra_period), i4_non_zero_i_period);
        if(i4_min_i_distance < sub_gop_size && i4_min_i_distance)
        {
            error_code = IHEVCE_INVALID_GOP_PERIOD;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: gop period is not valid for the configured temporal layers\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        if((i4_min_idr_period > i4_max_idr_period) || (i4_min_idr_period < 0))
        {
            error_code = IHEVCE_INVALID_GOP_PERIOD;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: gop period is not valid => min closed gop > max closed gop\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }
        if(ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers && i4_max_i_distance == 1)
        {
            error_code = IHEVCE_TEMPORAL_LAYERS_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: Invalid max temporal layer for I only encoding\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }
        if((i4_max_idr_period < i4_max_cra_period || i4_max_idr_period < i4_max_i_period) &&
           i4_max_idr_period)
        {
            error_code = IHEVCE_INVALID_GOP_PERIOD;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: MAX IDR period can't be less than Max CRA or I period\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }
        if((i4_max_cra_period < i4_max_i_period) && i4_max_cra_period)
        {
            error_code = IHEVCE_INVALID_GOP_PERIOD;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: MAX CRA period can't be less than Max I period\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }
    }
    if(0 != ps_static_cfg_prms->s_tgt_lyr_prms.i4_enable_temporal_scalability)
    {
        error_code = IHEVCE_INVALID_TEMPORAL_SCALABILITY;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: Temporal scalability is not supported \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_max_reference_frames != -1)
    {
        error_code = IHEVCE_REF_FRAMES_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: only supported value for i4_max_reference_frames is -1\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_weighted_pred_enable != 0 &&
       ps_static_cfg_prms->s_coding_tools_prms.i4_weighted_pred_enable != 1)
    {
        error_code = IHEVCE_INVALID_WEIGHTED_PREDICTION_INPUT;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_weighted_pred_enable invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_deblocking_type != 0 &&
       ps_static_cfg_prms->s_coding_tools_prms.i4_deblocking_type != 1 &&
       ps_static_cfg_prms->s_coding_tools_prms.i4_deblocking_type != 2)
    {
        error_code = IHEVCE_INVALID_DEBLOCKING_TYPE_INPUT;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_deblocking_type invalid\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_use_default_sc_mtx != 0 &&
       ps_static_cfg_prms->s_coding_tools_prms.i4_use_default_sc_mtx != 1)
    {
        error_code = IHEVCE_INVALID_DEFAULT_SC_MATRIX_ENABLE_INPUT;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_use_default_sc_mtx invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_coding_tools_prms.i4_cropping_mode != 0 &&
       ps_static_cfg_prms->s_coding_tools_prms.i4_cropping_mode != 1)
    {
        error_code = IHEVCE_INVALID_CROPPING_MODE;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_cropping_mode invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    /* Error checks for Static Config Parameters */
    if(ps_static_cfg_prms->s_config_prms.i4_min_log2_cu_size != 3)
    {
        error_code = IHEVCE_MIN_CU_SIZE_INPUT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_min_log2_cu_size invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_min_log2_tu_size != 2)
    {
        error_code = IHEVCE_MIN_TU_SIZE_INPUT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_min_log2_tu_size invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_max_log2_cu_size < 4 ||
       ps_static_cfg_prms->s_config_prms.i4_max_log2_cu_size > 6)
    {
        error_code = IHEVCE_MAX_CU_SIZE_INPUT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_max_log2_cu_size invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_max_log2_tu_size < 2 ||
       ps_static_cfg_prms->s_config_prms.i4_max_log2_tu_size > 5)
    {
        error_code = IHEVCE_MAX_TU_SIZE_INPUT_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_max_log2_tu_size invalid \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_min_log2_cu_size == 4 &&
       ps_static_cfg_prms->s_config_prms.i4_max_log2_tu_size == 5)
    {
        /* Because tu size should always be lesser than the cu size */
        error_code = IHEVCE_INVALID_MAX_TU_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Invalid combination of i4_min_log2_cu_size and i4_max_log2_tu_size\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_max_tr_tree_depth_I < 1 ||
       ps_static_cfg_prms->s_config_prms.i4_max_tr_tree_depth_I > 3)
    {
        error_code = IHEVCE_INVALID_TR_TREE_DEPTH_FOR_I_FRAME;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_max_tr_tree_depth_I out of range\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_max_tr_tree_depth_nI < 1 ||
       ps_static_cfg_prms->s_config_prms.i4_max_tr_tree_depth_nI > 4)
    {
        error_code = IHEVCE_INVALID_TR_TREE_DEPTH;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_max_tr_tree_depth_nI out of range\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_max_search_range_horz < 64 ||
       ps_static_cfg_prms->s_config_prms.i4_max_search_range_horz > 512)
    {
        error_code = IHEVCE_UNSUPPORTED_HORIZONTAL_SEARCH_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_max_search_range_horz out of range\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_config_prms.i4_max_search_range_vert < 32 ||
       ps_static_cfg_prms->s_config_prms.i4_max_search_range_vert > 256)
    {
        error_code = IHEVCE_UNSUPPORTED_VERTICAL_SEARCH_RANGE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_max_search_range_vert out of range\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    if(ps_static_cfg_prms->s_lap_prms.i4_rc_look_ahead_pics > NUM_LAP2_LOOK_AHEAD ||
       ps_static_cfg_prms->s_lap_prms.i4_rc_look_ahead_pics < 0)
    {
        error_code = IHEVCE_UNSUPPORTED_LOOK_AHEAD;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: rc look ahead pc must be in range of 0 to NUM_LAP2_LOOK_AHEAD\n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    /* Num res instances should be less than equal to IHEVCE_MAX_NUM_RESOLUTIONS */
    if((i4_num_resolutions < 1) || (i4_num_resolutions > IHEVCE_MAX_NUM_RESOLUTIONS))
    {
        error_code = IHEVCE_NUM_MAX_RESOLUTIONS_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: invalid i4_num_resolutions \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    if((ps_static_cfg_prms->i4_res_id < 0) || (ps_static_cfg_prms->i4_res_id >= i4_num_resolutions))
    {
        error_code = IHEVCE_NUM_MAX_RESOLUTIONS_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: invalid i4_num_resolutions \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    if((ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out < 0) ||
       (ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out > 1))
    {
        error_code = IHEVCE_INVALID_MRES_SINGLE_OUT;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: invalid i4_mres_single_out value \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    if((ps_static_cfg_prms->i4_save_recon) &&
       (1 == ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out))
    {
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE WARNING: i4_save_recon not supported for mres single out case \n");
        ps_static_cfg_prms->i4_save_recon = 0;
    }

    if((1 == i4_num_resolutions) && (1 == ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out))
    {
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE WARNING: i4_mres_single_out value changed to 0 for single resolution case \n");
        ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out = 0;
    }

    if(ps_static_cfg_prms->s_tgt_lyr_prms.i4_mbr_quality_setting < 0 ||
       ps_static_cfg_prms->s_tgt_lyr_prms.i4_mbr_quality_setting > 3)
    {
        error_code = IHEVCE_INVALID_MBR_QUALITY_SETTING;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: invalid mbr quality setting\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    if(ps_static_cfg_prms->s_tgt_lyr_prms.i4_multi_res_layer_reuse != 0)
    {
        error_code = IHEVCE_MULTI_RES_LAYER_REUSE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: reuse of info across resolution is not currently supported \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    for(i4_resolution_id = 0; i4_resolution_id < i4_num_resolutions; i4_resolution_id++)
    {
        WORD32 codec_level_index, quality_preset, height, width, frm_rate_scale_factor;
        WORD32 br_ctr;
        UWORD32 u4_luma_sample_rate;
        WORD32 max_dpb_size;
        WORD32 i4_field_pic = ps_static_cfg_prms->s_src_prms.i4_field_pic;

        codec_level_index = ihevce_get_level_index(
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level);
        quality_preset =
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;
        height = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
        width = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
        frm_rate_scale_factor = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                                    .i4_frm_rate_scale_factor;
        /* Check error for max picture size(luma) for the given level */
        if((width * height) > g_as_level_data[codec_level_index].i4_max_luma_picture_size)
        {
            error_code = IHEVCE_PIC_SIZE_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: (i4_tgt_width * i4_tgt_height) out of range for resolution number "
                "'%d' codec level %d "
                "\n",
                i4_resolution_id,
                codec_level_index);
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if((width * height) <= (g_as_level_data[codec_level_index].i4_max_luma_picture_size >> 2))
        {
            max_dpb_size = 16;
        }
        else if((width * height) <= (g_as_level_data[codec_level_index].i4_max_luma_picture_size >> 1))
        {
            max_dpb_size = 12;
        }
        else if(
            (width * height) <=
            (3 * g_as_level_data[codec_level_index].i4_max_luma_picture_size >> 2))
        {
            max_dpb_size = 8;
        }
        else
        {
            max_dpb_size = 6;
        }

        /* DPB check */
        if((((DEFAULT_MAX_REFERENCE_PICS - i4_field_pic) /*max reference*/ + 2) << i4_field_pic) >
           max_dpb_size)
        {
            error_code = IHEVCE_CODEC_LEVEL_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: i4_codec_level should be set correct \n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        if((quality_preset > IHEVCE_QUALITY_P7) || (quality_preset < 0) || (quality_preset == 1))
        {
            error_code = IHEVCE_INVALID_QUALITY_PRESET_INPUT;
            ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_quality_preset invalid \n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        /* Error checks for target width and height */
        if((height > HEVCE_MAX_HEIGHT) || (height < HEVCE_MIN_HEIGHT) ||
           (height != ps_static_cfg_prms->s_src_prms.i4_height))
        {
            error_code = IHEVCE_TGT_HEIGHT_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: Target height not supported\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        if((width > HEVCE_MAX_WIDTH) || (width < HEVCE_MIN_WIDTH) ||
           (width != ps_static_cfg_prms->s_src_prms.i4_width))
        {
            error_code = IHEVCE_TGT_WIDTH_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: Target width not supported\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        /* Error checks for the codec level */
        if(ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level >
           LEVEL6)
        {
            error_code = IHEVCE_CODEC_LEVEL_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: i4_codec_level should be set to a max value of 153 \n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        if(frm_rate_scale_factor != 1)
        {
            error_code = IHEVCE_TGT_FRAME_RATE_SCALING_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR:  Target frame rate scaler should be 1 \n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        u4_luma_sample_rate = (UWORD32)(width * height);
        u4_luma_sample_rate *= (UWORD32)(
            ps_static_cfg_prms->s_src_prms.i4_frm_rate_num /
            (ps_static_cfg_prms->s_src_prms.i4_frm_rate_denom * frm_rate_scale_factor));
        /* Check error for max samples rate (frame rate * luma picture size) for the given level */
        if(u4_luma_sample_rate > g_as_level_data[codec_level_index].u4_max_luma_sample_rate)
        {
            error_code = IHEVCE_LUMA_SAMPLE_RATE_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: input sample rate (i4_src_width * i4_src_height * i4_frm_rate_num / "
                "i4_frm_rate_denom ) "
                "exceeds u4_max_luma_sample_rate\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        /* Num instances should be less than equal to IHEVCE_MAX_NUM_BITRATES */
        if((ai4_num_bitrate_instances[i4_resolution_id] < 1) ||
           (ai4_num_bitrate_instances[i4_resolution_id] > IHEVCE_MAX_NUM_BITRATES))
        {
            error_code = IHEVCE_INVALID_NUM_BR_INSTANCES;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid i4_num_bitrate_instances \n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        /* check for codec tier */
        if((ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier > HIGH_TIER) ||
           (ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier < MAIN_TIER))
        {
            error_code = IHEVC_CODEC_TIER_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: Codec tier  out of range\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        if((ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level <
            120) &&
           (ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier == HIGH_TIER))
        {
            error_code = IHEVC_CODEC_TIER_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: Codec tier = HIGH TIER Not supported below Level 4\n");
            return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        }

        /* Check error for max bitrate for the given level */
        for(br_ctr = 0; br_ctr < ai4_num_bitrate_instances[i4_resolution_id]; br_ctr++)
        {
            WORD32 frame_qp = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                                  .ai4_frame_qp[br_ctr];
            WORD32 tgt_bitrate = ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                                     .ai4_tgt_bitrate[br_ctr];
            WORD32 peak_bitrate;

            if(frame_qp > 51 || frame_qp <= 0)
            {
                error_code = IHEVCE_UNSUPPORTED_FRAME_QP;
                ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_frame_qp out of range\n");
                return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
            }
            if((frame_qp < ps_static_cfg_prms->s_config_prms.i4_min_frame_qp) ||
               ((frame_qp + ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers + 1) >
                ps_static_cfg_prms->s_config_prms.i4_max_frame_qp))
            {
                error_code = IHEVCE_UNSUPPORTED_FRAME_QP;
                ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_frame_qp out of range\n");
                return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
            }

            if(tgt_bitrate >
                   g_as_level_data[codec_level_index]
                           .i4_max_bit_rate[ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier] *
                       CBP_VCL_FACTOR ||
               tgt_bitrate < 4000)
            {
                error_code = IHEVCE_BITRATE_NOT_SUPPORTED;
                ps_sys_api->ihevce_printf(
                    pv_cb_handle,
                    "IHEVCE ERROR: i4_tgt_bitrate out of range for resoltuion number %d bitrate "
                    "number %d\n",
                    i4_resolution_id,
                    br_ctr);
                return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
            }

            peak_bitrate = tgt_bitrate << 1;
            peak_bitrate =
                MIN(peak_bitrate,
                    g_as_level_data[codec_level_index]
                            .i4_max_bit_rate[ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier] *
                        1000);
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                .ai4_peak_bitrate[br_ctr] = peak_bitrate;
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                .ai4_max_vbv_buffer_size[br_ctr] = peak_bitrate;
        }
    }

    if((ps_static_cfg_prms->i4_br_id < 0) ||
       (ps_static_cfg_prms->i4_br_id >= ai4_num_bitrate_instances[ps_static_cfg_prms->i4_res_id]))
    {
        error_code = IHEVCE_INVALID_NUM_BR_INSTANCES;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: invalid i4_num_bitrate_instances \n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* Check error for rate control mode for the given level */
    if(ps_static_cfg_prms->s_config_prms.i4_rate_control_mode != 2 &&
       ps_static_cfg_prms->s_config_prms.i4_rate_control_mode != 3 &&
       ps_static_cfg_prms->s_config_prms.i4_rate_control_mode != 5)
    {
        error_code = IHEVCE_RATE_CONTROL_MDOE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_rate_control_mode out of range\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* Check error for pass number */
    if(ps_static_cfg_prms->s_pass_prms.i4_pass != 0)
    {
        error_code = IHEVCE_RATE_CONTROL_PASS_INVALID;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_pass out of range\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* Check error for cu level qp modultion for the given level */
    if(ps_static_cfg_prms->s_config_prms.i4_cu_level_rc != 0 &&
       ps_static_cfg_prms->s_config_prms.i4_cu_level_rc != 1)
    {
        error_code = IHEVCE_RATE_CONTROL_MDOE_NOT_SUPPORTED;
        ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: i4_cu_level_rc out of range\n");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    /* size error checks for the api structures */
    if(ps_static_cfg_prms->i4_size != sizeof(ihevce_static_cfg_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_static_cfg_params_t is not matching with actual "
            "size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(ps_static_cfg_prms->s_src_prms.i4_size != sizeof(ihevce_src_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_src_params_t is not matching with actual size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(ps_static_cfg_prms->s_tgt_lyr_prms.i4_size != sizeof(ihevce_tgt_layer_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_tgt_layer_params_t is not matching with actual "
            "size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(ps_static_cfg_prms->s_out_strm_prms.i4_size != sizeof(ihevce_out_strm_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_out_strm_params_t is not matching with actual "
            "size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(ps_static_cfg_prms->s_coding_tools_prms.i4_size != sizeof(ihevce_coding_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_coding_params_t is not matching with actual "
            "size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(ps_static_cfg_prms->s_config_prms.i4_size != sizeof(ihevce_config_prms_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_config_prms_t is not matching with actual size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    if(ps_static_cfg_prms->s_multi_thrd_prms.i4_size != sizeof(ihevce_static_multi_thread_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_static_multi_thread_params_t is not matching "
            "with actual size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }
    for(i4_resolution_id = 0; i4_resolution_id < i4_num_resolutions; i4_resolution_id++)
    {
        if(ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_size !=
           sizeof(ihevce_tgt_params_t))
        {
            error_code = IHEVCE_INVALID_SIZE;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: Size element of ihevce_tgt_params_t is not matching with actual "
                "size");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }
    }

    if(ps_static_cfg_prms->s_lap_prms.i4_size != sizeof(ihevce_lap_params_t))
    {
        error_code = IHEVCE_INVALID_SIZE;
        ps_sys_api->ihevce_printf(
            pv_cb_handle,
            "IHEVCE ERROR: Size element of ihevce_lap_params_t is not matching with actual size");
        return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
    }

    for(i4_resolution_id = 0; i4_resolution_id < i4_num_resolutions; i4_resolution_id++)
    {
        if(ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_size !=
           sizeof(ihevce_tgt_params_t))
        {
            error_code = IHEVCE_INVALID_SIZE;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: Size element of ihevce_tgt_params_t is not matching with actual "
                "size");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }
    }

    /* Check SEI related error checks */
    if(1 == ps_static_cfg_prms->s_out_strm_prms.i4_sei_enable_flag)
    {
        WORD32 i;
        /* Check values for i4_sei_hash_flags */
        if(!((ps_static_cfg_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag == 2) ||
             (ps_static_cfg_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag == 3) ||
             (ps_static_cfg_prms->s_out_strm_prms.i4_decoded_pic_hash_sei_flag == 0)))
        {
            error_code = IHEVCE_SEI_HASH_VALUE_NOT_SUPPORTED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: i4_sei_hash_flags out of range\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        /* Content Light Level Info error check */
        if((ps_static_cfg_prms->s_out_strm_prms.i4_sei_cll_enable > 1) ||
           (ps_static_cfg_prms->s_out_strm_prms.i4_sei_cll_enable < 0))
        {
            error_code = IHEVCE_SEI_CLL_ENABLE_OUT_OF_RANGE;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: i4_sei_cll_enable out of range\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if((ps_static_cfg_prms->s_out_strm_prms.i4_sei_buffer_period_flags ||
            ps_static_cfg_prms->s_out_strm_prms.i4_sei_pic_timing_flags) &&
           (!ps_static_cfg_prms->s_out_strm_prms.i4_vui_enable))
        {
            error_code = IHEVCE_SEI_ENABLED_VUI_DISABLED;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: Both SEI and VUI ought to be enabled when either "
                "'i4_sei_buffer_period_flags' or "
                "'i4_sei_pic_timing_flags' are enabled\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if((1 == ps_static_cfg_prms->s_out_strm_prms.i4_sei_buffer_period_flags) &&
           (3 == ps_static_cfg_prms->s_config_prms.i4_rate_control_mode))
        {
            error_code = IHEVCE_SEI_MESSAGES_DEPENDENCY;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: i4_sei_buffer_period_flags should be disabled for CQP mode of Rate "
                "control \n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        /* Check values for i4_sei_mastering_disp_colour_vol_flags */
        if((ps_static_cfg_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags != 0) &&
           (ps_static_cfg_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags != 1))
        {
            error_code = IHEVCE_MASTERING_DISP_COL_VOL_OUT_OF_RANGE;
            ps_sys_api->ihevce_printf(
                pv_cb_handle,
                "IHEVCE ERROR: i4_sei_mastering_disp_colour_vol_flags out of range\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if(1 == ps_static_cfg_prms->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags)
        {
            /* Check values for u2_display_primaries_x and u2_display_primaries_y */
            for(i = 0; i < 3; i++)
            {
                if((ps_static_cfg_prms->s_out_strm_prms.au2_display_primaries_x[i] > 50000))
                {
                    error_code = IHEVCE_DISPLAY_PRIMARY_X_OUT_OF_RANGE;
                    ps_sys_api->ihevce_printf(
                        pv_cb_handle, "IHEVCE ERROR: au2_display_primaries_x out of range\n");
                    return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
                }

                if((ps_static_cfg_prms->s_out_strm_prms.au2_display_primaries_y[i] > 50000))
                {
                    error_code = IHEVCE_DISPLAY_PRIMARY_Y_OUT_OF_RANGE;
                    ps_sys_api->ihevce_printf(
                        pv_cb_handle, "IHEVCE ERROR: au2_display_primaries_y out of range\n");
                    return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
                }
            }

            if((ps_static_cfg_prms->s_out_strm_prms.u2_white_point_x > 50000))
            {
                error_code = IHEVCE_WHITE_POINT_X_OUT_OF_RANGE;
                ps_sys_api->ihevce_printf(
                    pv_cb_handle, "IHEVCE ERROR: u2_white_point_x out of range\n");
                return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
            }

            if((ps_static_cfg_prms->s_out_strm_prms.u2_white_point_y > 50000))
            {
                error_code = IHEVCE_WHITE_POINT_Y_OUT_OF_RANGE;
                ps_sys_api->ihevce_printf(
                    pv_cb_handle, "IHEVCE ERROR: u2_white_point_y out of range\n");
                return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
            }

            if(ps_static_cfg_prms->s_out_strm_prms.u4_max_display_mastering_luminance <=
               ps_static_cfg_prms->s_out_strm_prms.u4_min_display_mastering_luminance)
            {
                error_code = IHEVCE_MAX_DISP_MATERING_LUM_OUT_OF_RANGE;
                ps_sys_api->ihevce_printf(
                    pv_cb_handle,
                    "IHEVCE ERROR: u4_max_display_mastering_luminance should be greater then "
                    "u4_min_display_mastering_luminance \n");
                return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
            }
        }
    }

    if(1 == ps_static_cfg_prms->s_out_strm_prms.i4_vui_enable)
    {
        /* validate static vui parameters */
        if(((ps_static_cfg_prms->s_vui_sei_prms.u1_aspect_ratio_info_present_flag & 0xFE) > 0))
        {
            error_code = IHEVC_INVALID_ASPECT_RATIO_PARAMS;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid aspect ratio parameters\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if(((ps_static_cfg_prms->s_vui_sei_prms.u1_overscan_info_present_flag & 0xFE) > 0) ||
           ((ps_static_cfg_prms->s_vui_sei_prms.u1_overscan_appropriate_flag & 0xFE) > 0))
        {
            error_code = IHEVC_INVALID_OVERSCAN_PARAMS;
            ps_sys_api->ihevce_printf(pv_cb_handle, "IHEVCE ERROR: invalid overscan parameters\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if(((ps_static_cfg_prms->s_vui_sei_prms.u1_video_signal_type_present_flag & 0xFE) > 0) ||
           (ps_static_cfg_prms->s_vui_sei_prms.u1_video_format > 5) ||
           ((ps_static_cfg_prms->s_vui_sei_prms.u1_video_full_range_flag & 0xFE) > 0))
        {
            error_code = IHEVC_INVALID_VIDEO_PARAMS;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid video signal type parameters\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if(((ps_static_cfg_prms->s_vui_sei_prms.u1_colour_description_present_flag & 0xFE) > 0))
        {
            error_code = IHEVC_INVALID_COLOUR_PARAMS;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid colour description parameters\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if(((ps_static_cfg_prms->s_vui_sei_prms.u1_chroma_loc_info_present_flag & 0xFE) > 0) ||
           (ps_static_cfg_prms->s_vui_sei_prms.u1_chroma_sample_loc_type_top_field > 5) ||
           (ps_static_cfg_prms->s_vui_sei_prms.u1_chroma_sample_loc_type_bottom_field > 5))
        {
            error_code = IHEVC_INVALID_CHROMA_PARAMS;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid chroma info parameters\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if((ps_static_cfg_prms->s_vui_sei_prms.u1_timing_info_present_flag & 0xFE) > 0)
        {
            error_code = IHEVC_INVALID_TIMING_INFO_PARAM;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid timing info present flag\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }

        if(((ps_static_cfg_prms->s_vui_sei_prms.u1_vui_hrd_parameters_present_flag & 0xFE) > 0) ||
           ((ps_static_cfg_prms->s_vui_sei_prms.u1_nal_hrd_parameters_present_flag & 0xFE) > 0))
        {
            error_code = IHEVC_INVALID_HRD_PRESENT_PARAMS;
            ps_sys_api->ihevce_printf(
                pv_cb_handle, "IHEVCE ERROR: invalid vui or vcl hrd parameters present flag\n");
            return (IHEVCE_SETUNSUPPORTEDINPUT(error_code));
        }
    }

    error_code = ihevce_validate_tile_config_params(ps_static_cfg_prms);
    if(IHEVCE_SUCCESS != error_code)
    {
        return error_code;
    }

    if(ps_static_cfg_prms->s_slice_params.i4_slice_segment_mode != 0)
    {
        error_code = IHEVCE_BAD_SLICE_PARAMS;
        ps_sys_api->ihevce_printf(
            pv_cb_handle, "IHEVCE ERROR: i4_slice_segment_mode should be 0 \n");
        return IHEVCE_SETUNSUPPORTEDINPUT(error_code);
    }

    return IHEVCE_SUCCESS;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_level_index \endif
*
* \brief
*    This function returns the index of level based on codec_level value
*    Please see the LEVEL_T enum def
*
* \param[in] Codec Level
*
* \return
*    Index of Codec level
*
* \author
*    Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_get_level_index(WORD32 i4_codec_level)
{
    switch(i4_codec_level)
    {
    case LEVEL1:
        return 0;
    case LEVEL2:
        return 1;
    case LEVEL2_1:
        return 2;
    case LEVEL3:
        return 3;
    case LEVEL3_1:
        return 4;
    case LEVEL4:
        return 5;
    case LEVEL4_1:
        return 6;
    case LEVEL5:
        return 7;
    case LEVEL5_1:
        return 8;
    case LEVEL5_2:
        return 9;
    case LEVEL6:
        return 10;
    case LEVEL6_1:
        return 11;
    case LEVEL6_2:
        return 12;
    default:
        return 0;
    }
}
