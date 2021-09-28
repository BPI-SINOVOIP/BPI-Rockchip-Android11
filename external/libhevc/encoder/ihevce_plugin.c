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
* \file ihevce_plugin.c
*
* \brief
*    This file contains wrapper utilities to use hevc encoder library
*
* \date
*    15/04/2014
*
* \author
*    Ittiam
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

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_plugin.h"
#include "ihevce_plugin_priv.h"
#include "ihevce_hle_interface.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_error_codes.h"
#include "ihevce_error_checks.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define CREATE_TIME_ALLOCATION_INPUT 1
#define CREATE_TIME_ALLOCATION_OUTPUT 0

#define MAX_NUM_FRM_IN_GOP 600

/*****************************************************************************/
/* Extern variables                                                          */
/*****************************************************************************/

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : mem_mngr_alloc \endif
*
* \brief
*    Memory manager specific alloc function
*
* \param[in] pv_handle : handle to memory manager
*                        (currently not required can be set to null)
* \param[in] ps_memtab : memory descriptor pointer
*
* \return
*    Memory pointer
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void mem_mngr_alloc(void *pv_handle, ihevce_sys_api_t *ps_sys_api, iv_mem_rec_t *ps_memtab)
{
#ifndef X86_MINGW
    WORD32 error, mem_alignment;
#endif

    (void)pv_handle;

#ifdef X86_MINGW
    ps_memtab->pv_base = _aligned_malloc(ps_memtab->i4_mem_size, ps_memtab->i4_mem_alignment);
#else
    mem_alignment = ps_memtab->i4_mem_alignment;
    mem_alignment = (mem_alignment >> 3) << 3;
    if(mem_alignment == 0)
    {
        error = posix_memalign(&ps_memtab->pv_base, sizeof(void *), ps_memtab->i4_mem_size);
    }
    else
    {
        error = posix_memalign(&ps_memtab->pv_base, mem_alignment, ps_memtab->i4_mem_size);
    }
    if(error != 0)
    {
        ps_sys_api->ihevce_printf(ps_sys_api->pv_cb_handle, "posix_memalign error %d\n", error);
    }
#endif

    if(ps_memtab->pv_base == NULL)
    {
        ps_sys_api->ihevce_printf(
            ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Unable to allocate memory\n");
        ASSERT(0);
    }
    return;
}

/*!
******************************************************************************
* \if Function name : memory_alloc \endif
*
* \brief
*    common memory allocate function should be used across all threads
*
* \param[in] pv_handle : handle to memory manager
*                        (currently not required can be set to null)
* \param[in] u4_size : size of memory required
*
* \return
*    Memory pointer
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *memory_alloc(void *pv_handle, UWORD32 u4_size)
{
    (void)pv_handle;
    return (malloc(u4_size));
}

/*!
******************************************************************************
* \if Function name : mem_mngr_free \endif
*
* \brief
*    Memory manager specific free function
*
* \param[in] pv_handle : handle to memory manager
*                        (currently not required can be set to null)
* \param[in] ps_memtab : memory descriptor pointer
*
* \return
*    Memory pointer
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void mem_mngr_free(void *pv_handle, iv_mem_rec_t *ps_memtab)
{
    (void)pv_handle;
#ifdef X86_MINGW
    _aligned_free(ps_memtab->pv_base);
#else
    free(ps_memtab->pv_base);
#endif
    return;
}

/*!
******************************************************************************
* \if Function name : memory_free \endif
*
* \brief
*    common memory free function should be used across all threads
*
* \param[in] pv_handle : handle to memory manager
*                        (currently not required can be set to null)
* \param[in] pv_mem : memory to be freed
*
* \return
*    Memory pointer
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void memory_free(void *pv_handle, void *pv_mem)
{
    (void)pv_handle;
    free(pv_mem);
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_set_def_params \endif
*
* \brief
*    Set default values
*
* \param[in] Static params pointer
*
* \return
*    status
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T ihevce_set_def_params(ihevce_static_cfg_params_t *ps_params)
{
    WORD32 i, j;
    /* sanity checks */
    if(NULL == ps_params)
        return (IHEVCE_EFAIL);

    memset(ps_params, 0, sizeof(*ps_params));

    /* initialsie all the parameters to default values */
    ps_params->i4_size = sizeof(ihevce_static_cfg_params_t);
    ps_params->i4_save_recon = 0;
    ps_params->i4_log_dump_level = 0;
    ps_params->i4_enable_logo = 0;
    ps_params->i4_enable_csv_dump = 0;

    /* Control to free the entropy output buffers   */
    /* 1  for non_blocking mode */
    /* and 0 for blocking mode */
    ps_params->i4_outbuf_buf_free_control = 1;

    /* coding tools parameters */
    ps_params->s_coding_tools_prms.i4_size = sizeof(ihevce_coding_params_t);
    ps_params->s_coding_tools_prms.i4_cropping_mode = 1;
    ps_params->s_coding_tools_prms.i4_deblocking_type = 0;
    ps_params->s_coding_tools_prms.i4_enable_entropy_sync = 0;
    // New IDR/CDR Params
    ps_params->s_coding_tools_prms.i4_max_closed_gop_period = 0;
    ps_params->s_coding_tools_prms.i4_min_closed_gop_period = 0;
    ps_params->s_coding_tools_prms.i4_max_cra_open_gop_period = 60;
    ps_params->s_coding_tools_prms.i4_max_i_open_gop_period = 0;
    ps_params->s_coding_tools_prms.i4_max_reference_frames = -1;
    ps_params->s_coding_tools_prms.i4_max_temporal_layers = 0;
    ps_params->s_coding_tools_prms.i4_slice_type = 0;
    ps_params->s_coding_tools_prms.i4_use_default_sc_mtx = 0;
    ps_params->s_coding_tools_prms.i4_weighted_pred_enable = 0;
    ps_params->s_coding_tools_prms.i4_vqet = 0;

    ps_params->e_arch_type = ARCH_NA;

    /* config parameters */
    ps_params->s_config_prms.i4_size = sizeof(ihevce_config_prms_t);
    ps_params->s_config_prms.i4_cu_level_rc = 1;
    ps_params->s_config_prms.i4_init_vbv_fullness = 0;
    ps_params->s_config_prms.i4_max_frame_qp = 51;
    ps_params->s_config_prms.i4_max_log2_cu_size = 6;
    ps_params->s_config_prms.i4_max_log2_tu_size = 5;
    ps_params->s_config_prms.i4_max_search_range_horz = 512;
    ps_params->s_config_prms.i4_max_search_range_vert = 256;
    ps_params->s_config_prms.i4_max_tr_tree_depth_I = 1;
    ps_params->s_config_prms.i4_max_tr_tree_depth_nI = 3;
    ps_params->s_config_prms.i4_min_frame_qp = 1;
    ps_params->s_config_prms.i4_min_log2_cu_size = 3;
    ps_params->s_config_prms.i4_min_log2_tu_size = 2;
    ps_params->s_config_prms.i4_num_frms_to_encode = -1;
    ps_params->s_config_prms.i4_rate_control_mode = 2;
    ps_params->s_config_prms.i4_stuffing_enable = 0;
    ps_params->s_config_prms.i4_vbr_max_peak_rate_dur = 2000;

    /* LAP parameters */
    ps_params->s_lap_prms.i4_size = sizeof(ihevce_lap_params_t);
    ps_params->s_lap_prms.i4_deinterlacer_enable = 0;
    ps_params->s_lap_prms.i4_denoise_enable = 0;
    ps_params->s_lap_prms.i4_enable_wts_ofsts = 1;
    ps_params->s_lap_prms.i4_rc_look_ahead_pics = 0;

    /* Multi Thread parameters */
    ps_params->s_multi_thrd_prms.i4_size = sizeof(ihevce_static_multi_thread_params_t);
    ps_params->s_multi_thrd_prms.i4_max_num_cores = 1;
    ps_params->s_multi_thrd_prms.i4_memory_alloc_ctrl_flag = 0;
    ps_params->s_multi_thrd_prms.i4_num_proc_groups = 1;
    ps_params->s_multi_thrd_prms.ai4_num_cores_per_grp[0] = -1;
    ps_params->s_multi_thrd_prms.i4_use_thrd_affinity = -1;  //0;
    memset(&ps_params->s_multi_thrd_prms.au8_core_aff_mask[0], 0, sizeof(ULWORD64) * MAX_NUM_CORES);

    /* Output Streams parameters */
    ps_params->s_out_strm_prms.i4_size = sizeof(ihevce_out_strm_params_t);
    ps_params->s_out_strm_prms.i4_aud_enable_flags = 0;
    ps_params->s_out_strm_prms.i4_eos_enable_flags = 0;
    ps_params->s_out_strm_prms.i4_codec_profile = 1;
    ps_params->s_out_strm_prms.i4_codec_tier = 0;
    ps_params->s_out_strm_prms.i4_codec_type = 0;
    ps_params->s_out_strm_prms.i4_sei_buffer_period_flags = 0;
    ps_params->s_out_strm_prms.i4_sei_enable_flag = 0;
    ps_params->s_out_strm_prms.i4_sei_payload_enable_flag = 0;
    ps_params->s_out_strm_prms.i4_sei_pic_timing_flags = 0;
    ps_params->s_out_strm_prms.i4_sei_cll_enable = 0;
    ps_params->s_out_strm_prms.u2_sei_avg_cll = 0;
    ps_params->s_out_strm_prms.u2_sei_max_cll = 0;
    ps_params->s_out_strm_prms.i4_sei_recovery_point_flags = 0;
    ps_params->s_out_strm_prms.i4_sei_mastering_disp_colour_vol_flags = 0;
    ps_params->s_out_strm_prms.i4_decoded_pic_hash_sei_flag = 0;
    ps_params->s_out_strm_prms.i4_sps_at_cdr_enable = 1;
    ps_params->s_out_strm_prms.i4_vui_enable = 0;
    /*Set the interoperability flag to 0*/
    ps_params->s_out_strm_prms.i4_interop_flags = 0;

    /* Source parameters */
    ps_params->s_src_prms.i4_size = sizeof(ihevce_src_params_t);
    ps_params->s_src_prms.inp_chr_format = 1;
    ps_params->s_src_prms.i4_chr_format = 11;
    ps_params->s_src_prms.i4_field_pic = 0;
    ps_params->s_src_prms.i4_frm_rate_denom = 1000;
    ps_params->s_src_prms.i4_frm_rate_num = 30000;
    ps_params->s_src_prms.i4_height = 0;  //1080;
    ps_params->s_src_prms.i4_input_bit_depth = 8;
    ps_params->s_src_prms.i4_topfield_first = 1;
    ps_params->s_src_prms.i4_width = 0;  //1920;
    ps_params->s_src_prms.i4_orig_width = 0;
    ps_params->s_src_prms.i4_orig_height = 0;

    /* Target layer parameters */
    ps_params->s_tgt_lyr_prms.i4_size = sizeof(ihevce_tgt_layer_params_t);
    ps_params->s_tgt_lyr_prms.i4_enable_temporal_scalability = 0;
    ps_params->s_tgt_lyr_prms.i4_internal_bit_depth = 8;
    ps_params->s_tgt_lyr_prms.i4_mbr_quality_setting = IHEVCE_MBR_HIGH_QUALITY;
    ps_params->s_tgt_lyr_prms.i4_multi_res_layer_reuse = 0;
    ps_params->s_tgt_lyr_prms.i4_num_res_layers = 1;
    ps_params->s_tgt_lyr_prms.i4_mres_single_out = 0;
    ps_params->s_tgt_lyr_prms.i4_start_res_id = 0;
    ps_params->s_tgt_lyr_prms.pf_scale_chroma = NULL;
    ps_params->s_tgt_lyr_prms.pf_scale_luma = NULL;
    ps_params->s_tgt_lyr_prms.pv_scaler_handle = NULL;

    /* target parameters */
    for(i = 0; i < IHEVCE_MAX_NUM_RESOLUTIONS; i++)
    {
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_size = sizeof(ihevce_tgt_params_t);
        for(j = 0; j < IHEVCE_MAX_NUM_BITRATES; j++)
        {
            ps_params->s_tgt_lyr_prms.as_tgt_params[i].ai4_frame_qp[j] = 32;
            ps_params->s_tgt_lyr_prms.as_tgt_params[i].ai4_tgt_bitrate[j] = 5000000;
            ps_params->s_tgt_lyr_prms.as_tgt_params[i].ai4_peak_bitrate[j] = 10000000;
            ps_params->s_tgt_lyr_prms.as_tgt_params[i].ai4_max_vbv_buffer_size[j] = -1;
        }
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_codec_level = 156;
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_frm_rate_scale_factor = 1;
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_height = 0;
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_num_bitrate_instances = 1;
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_quality_preset = IHEVCE_QUALITY_P5;
        ps_params->s_tgt_lyr_prms.as_tgt_params[i].i4_width = 0;
    }

    /* SEI VUI parameters */
    ps_params->s_vui_sei_prms.u1_aspect_ratio_info_present_flag = 0;
    ps_params->s_vui_sei_prms.au1_aspect_ratio_idc[0] = 255;
    ps_params->s_vui_sei_prms.au2_sar_width[0] = 4;
    ps_params->s_vui_sei_prms.au2_sar_height[0] = 3;
    ps_params->s_vui_sei_prms.u1_overscan_info_present_flag = 0;
    ps_params->s_vui_sei_prms.u1_overscan_appropriate_flag = 0;
    ps_params->s_vui_sei_prms.u1_video_signal_type_present_flag = 1;
    ps_params->s_vui_sei_prms.u1_video_format = 5;
    ps_params->s_vui_sei_prms.u1_video_full_range_flag = 1;
    ps_params->s_vui_sei_prms.u1_colour_description_present_flag = 0;
    ps_params->s_vui_sei_prms.u1_colour_primaries = 2;
    ps_params->s_vui_sei_prms.u1_transfer_characteristics = 2;
    ps_params->s_vui_sei_prms.u1_matrix_coefficients = 2;
    ps_params->s_vui_sei_prms.u1_chroma_loc_info_present_flag = 0;
    ps_params->s_vui_sei_prms.u1_chroma_sample_loc_type_top_field = 0;
    ps_params->s_vui_sei_prms.u1_chroma_sample_loc_type_bottom_field = 0;
    ps_params->s_vui_sei_prms.u1_vui_hrd_parameters_present_flag = 0;
    ps_params->s_vui_sei_prms.u1_timing_info_present_flag = 0;
    ps_params->s_vui_sei_prms.u1_nal_hrd_parameters_present_flag = 0;

    /* Setting sysAPIs to NULL */
    memset(&ps_params->s_sys_api, 0, sizeof(ihevce_sys_api_t));

    /* Multi pass parameters */
    memset(&ps_params->s_pass_prms, 0, sizeof(ihevce_pass_prms_t));
    ps_params->s_pass_prms.i4_size = sizeof(ihevce_pass_prms_t);

    /* Tile parameters */
    ps_params->s_app_tile_params.i4_size = sizeof(ihevce_app_tile_params_t);
    ps_params->s_app_tile_params.i4_tiles_enabled_flag = 0;
    ps_params->s_app_tile_params.i4_uniform_spacing_flag = 1;
    ps_params->s_app_tile_params.i4_num_tile_cols = 1;
    ps_params->s_app_tile_params.i4_num_tile_rows = 1;

    ps_params->s_slice_params.i4_slice_segment_mode = 0;
    ps_params->s_slice_params.i4_slice_segment_argument = 1300;

    return (IHEVCE_EOK);
}

/*!
******************************************************************************
* \if Function name : ihevce_cmds_error_report \endif
*
* \brief
*    Call back from encoder to report errors
*
* \param[in]    pv_error_handling_cb_handle
* \param[in]    i4_error_code
* \param[in]    i4_cmd_type
* \param[in]    i4_id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_cmds_error_report(
    void *pv_cb_handle, WORD32 i4_error_code, WORD32 i4_cmd_type, WORD32 i4_buf_id)
{
    /*local variables*/
    plugin_ctxt_t *plugin_ctxt = (plugin_ctxt_t *)pv_cb_handle;
    ihevce_static_cfg_params_t *ps_static_cfg_params =
        ((ihevce_hle_ctxt_t *)plugin_ctxt->pv_hle_interface_ctxt)->ps_static_cfg_prms;

    if(i4_cmd_type == 0)
        ps_static_cfg_params->s_sys_api.ihevce_printf(
            ps_static_cfg_params->s_sys_api.pv_cb_handle,
            "PLUGIN ERROR: Asynchronous Buffer Error %d in Buffer Id %d",
            i4_error_code,
            i4_buf_id);
    else
        ps_static_cfg_params->s_sys_api.ihevce_printf(
            ps_static_cfg_params->s_sys_api.pv_cb_handle,
            "PLUGIN ERROR: Synchronous Buffer Error %d in Buffer Id %d",
            i4_error_code,
            i4_buf_id);

    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_strm_fill_done \endif
*
* \brief
*    Call back from encoder when Bitstream is ready to consume
*
* \param[in]
* \param[in]
* \param[in]
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T
    ihevce_strm_fill_done(void *pv_ctxt, void *pv_curr_out, WORD32 i4_br_id, WORD32 i4_res_id)
{
    /* local variables */
    plugin_ctxt_t *ps_ctxt = (plugin_ctxt_t *)pv_ctxt;
    app_ctxt_t *ps_app_ctxt = &ps_ctxt->s_app_ctxt;
    out_strm_prms_t *ps_out_strm_prms = &ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id];
    void *pv_app_out_strm_buf_mutex_hdl = ps_out_strm_prms->pv_app_out_strm_buf_mutex_hdl;
    void *pv_app_out_strm_buf_cond_var_hdl = ps_out_strm_prms->pv_app_out_strm_buf_cond_var_hdl;
    iv_output_data_buffs_t *ps_curr_out = (iv_output_data_buffs_t *)pv_curr_out;
    WORD32 end_flag = ps_curr_out->i4_end_flag;
    WORD32 osal_result;

    /* ------  output dump stream  -- */
    if((WORD32)IV_FAIL != ps_curr_out->i4_process_ret_sts)
    {
        if(0 != ps_curr_out->i4_bytes_generated)
        {
            /* accumulate the total bits generated */
            (ps_out_strm_prms->u8_total_bits) += ps_curr_out->i4_bytes_generated * 8;
            (ps_out_strm_prms->u4_num_frms_enc)++;
        }
    }

    /****** Lock the critical section ******/
    osal_result = osal_mutex_lock(pv_app_out_strm_buf_mutex_hdl);
    if(OSAL_SUCCESS != osal_result)
        return (IV_FAIL);

    /* Update the end flag to communicate with the o/p thread */
    ps_app_ctxt->ai4_out_strm_end_flag[i4_res_id][i4_br_id] = end_flag;

    /* set the produced status of the buffer */
    {
        WORD32 idx = ps_curr_out->i4_cb_buf_id;

        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_timestamp_low =
            ps_curr_out->i4_out_timestamp_low;
        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_timestamp_high =
            ps_curr_out->i4_out_timestamp_high;
        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_bytes_gen =
            ps_curr_out->i4_bytes_generated;
        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_is_key_frame = 0;
        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_end_flag = end_flag;

        if((IV_IDR_FRAME == ps_curr_out->i4_encoded_frame_type) ||
           (IV_I_FRAME == ps_curr_out->i4_encoded_frame_type))
        {
            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_is_key_frame = 1;
        }

        /* set the buffer as produced */
        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][idx].i4_is_prod = 1;
    }

    /****** Wake ******/
    osal_cond_var_signal(pv_app_out_strm_buf_cond_var_hdl);

    /****** Unlock the critical section ******/
    osal_result = osal_mutex_unlock(pv_app_out_strm_buf_mutex_hdl);
    if(OSAL_SUCCESS != osal_result)
        return (IV_FAIL);

    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_plugin_init \endif
*
* \brief
*    Initialises the enocder context and threads
*
* \param[in] Static params pointer
*
* \return
*    status
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T ihevce_init(ihevce_static_cfg_params_t *ps_params, void **ppv_ihevce_hdl)
{
    /* local variables */
    plugin_ctxt_t *ps_ctxt;
    app_ctxt_t *ps_app_ctxt;
    ihevce_hle_ctxt_t *ps_interface_ctxt;
    ihevce_sys_api_t *ps_sys_api;
    osal_cb_funcs_t s_cb_funcs;
    WORD32 status = 0;

    /* sanity checks */
    if(NULL == ps_params)
        return (IHEVCE_EFAIL);

    if(NULL == ppv_ihevce_hdl)
        return (IHEVCE_EFAIL);

    /* set the handle to null by default */
    *ppv_ihevce_hdl = NULL;

    /* Initiallizing system apis */
    ps_sys_api = &ps_params->s_sys_api;
    ihevce_init_sys_api(NULL, ps_sys_api);

    /* --------------------------------------------------------------------- */
    /*                   Query and print Encoder version                     */
    /* --------------------------------------------------------------------- */
    ps_sys_api->ihevce_printf(
        ps_sys_api->pv_cb_handle, "Encoder version %s\n\n", ihevce_get_encoder_version());

    /* --------------------------------------------------------------------- */
    /*                    Plugin Handle create                               */
    /* --------------------------------------------------------------------- */
    ps_ctxt = (plugin_ctxt_t *)memory_alloc(NULL, sizeof(plugin_ctxt_t));
    if(NULL == ps_ctxt)
    {
        ps_sys_api->ihevce_printf(
            ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in Plugin initialization\n");
        return (IHEVCE_EFAIL);
    }
    memset(ps_ctxt, 0, sizeof(plugin_ctxt_t));

    /* initialise memory call backs */
    ps_ctxt->ihevce_mem_alloc = memory_alloc;
    ps_ctxt->ihevce_mem_free = memory_free;

    ps_ctxt->u8_num_frames_encoded = 0;

    if((0 == ps_params->i4_res_id) && (0 == ps_params->i4_br_id))
    {
        /* --------------------------------------------------------------------- */
        /*                      OSAL Handle create                               */
        /* --------------------------------------------------------------------- */
        ps_ctxt->pv_osal_handle = memory_alloc(NULL, OSAL_HANDLE_SIZE);

        /* Initialize OSAL call back functions */
        s_cb_funcs.mmr_handle = NULL;
        s_cb_funcs.osal_alloc = memory_alloc;
        s_cb_funcs.osal_free = memory_free;

        status = osal_init(ps_ctxt->pv_osal_handle);
        if(OSAL_SUCCESS != status)
        {
            ps_sys_api->ihevce_printf(
                ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in OSAL initialization\n");
            return (IHEVCE_EFAIL);
        }

        status = osal_register_callbacks(ps_ctxt->pv_osal_handle, &s_cb_funcs);
        if(OSAL_SUCCESS != status)
        {
            ps_sys_api->ihevce_printf(
                ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in OSAL call back registration\n");
            return (IHEVCE_EFAIL);
        }

        /* --------------------------------------------------------------------- */
        /*                      Thread affinity  Initialization                  */
        /* --------------------------------------------------------------------- */
        if(ps_params->s_multi_thrd_prms.i4_use_thrd_affinity)
        {
            WORD32 i4_ctr;

            /* loop over all the cores */
            for(i4_ctr = 0; i4_ctr < ps_params->s_multi_thrd_prms.i4_max_num_cores; i4_ctr++)
            {
                /* All cores are logical cores  */
                ps_params->s_multi_thrd_prms.au8_core_aff_mask[i4_ctr] = ((ULWORD64)1 << i4_ctr);
            }
        }

        /* --------------------------------------------------------------------- */
        /*             Context Initialization                                    */
        /* --------------------------------------------------------------------- */
        ps_app_ctxt = &ps_ctxt->s_app_ctxt;

        ps_ctxt->ps_static_cfg_prms = (ihevce_static_cfg_params_t *)ps_ctxt->ihevce_mem_alloc(
            NULL, sizeof(ihevce_static_cfg_params_t));
        if(NULL == ps_ctxt->ps_static_cfg_prms)
        {
            ps_sys_api->ihevce_printf(
                ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in Plugin memory initialization\n");
            return (IHEVCE_EFAIL);
        }

        ps_params->apF_csv_file[0][0] = NULL;

        /* set the memory manager handle to NULL */
        ps_app_ctxt->pv_mem_mngr_handle = NULL;

        /* --------------------------------------------------------------------- */
        /*            Back up the static params passed by caller                 */
        /* --------------------------------------------------------------------- */
        memcpy(ps_ctxt->ps_static_cfg_prms, ps_params, sizeof(ihevce_static_cfg_params_t));

        ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_orig_width =
            ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width;
        if(HEVCE_MIN_WIDTH > ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width)
        {
            ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width = HEVCE_MIN_WIDTH;
        }

        ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_orig_height =
            ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_height;
        if(HEVCE_MIN_HEIGHT > ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_height)
        {
            ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_height = HEVCE_MIN_HEIGHT;
        }

        /* setting tgt width and height same as src width and height */
        ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[0].i4_width =
            ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width;
        ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[0].i4_height =
            ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_height;

        /* setting key frame interval */
        ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period =
            MIN(MAX_NUM_FRM_IN_GOP,
                ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_closed_gop_period);
        ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period =
            MIN(MAX_NUM_FRM_IN_GOP,
                ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_cra_open_gop_period);
        ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period =
            MIN(MAX_NUM_FRM_IN_GOP,
                ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_i_open_gop_period);

        /* --------------------------------------------------------------------- */
        /*            High Level Encoder context init                            */
        /* --------------------------------------------------------------------- */
        ps_interface_ctxt =
            (ihevce_hle_ctxt_t *)ps_ctxt->ihevce_mem_alloc(NULL, sizeof(ihevce_hle_ctxt_t));
        if(NULL == ps_interface_ctxt)
        {
            ps_sys_api->ihevce_printf(
                ps_sys_api->pv_cb_handle,
                "IHEVCE ERROR: Error in Plugin HLE memory initialization\n");
            return (IHEVCE_EFAIL);
        }
        memset(ps_interface_ctxt, 0, sizeof(ihevce_hle_ctxt_t));
        ps_interface_ctxt->i4_size = sizeof(ihevce_hle_ctxt_t);

        ps_ctxt->pv_hle_interface_ctxt = ps_interface_ctxt;

        /* store the static config parameters pointer */
        ps_interface_ctxt->ps_static_cfg_prms = ps_ctxt->ps_static_cfg_prms;

        /* initialise the interface strucure parameters */
        ps_interface_ctxt->pv_inp_cb_handle = (void *)ps_ctxt;
        ps_interface_ctxt->pv_out_cb_handle = (void *)ps_ctxt;
        ps_interface_ctxt->pv_recon_cb_handle = (void *)ps_ctxt;

        ps_interface_ctxt->pv_osal_handle = ps_ctxt->pv_osal_handle;
        ps_interface_ctxt->ihevce_mem_alloc = mem_mngr_alloc;
        ps_interface_ctxt->ihevce_mem_free = mem_mngr_free;
        ps_interface_ctxt->i4_hle_init_done = 0;
        ps_interface_ctxt->pv_mem_mgr_hdl = ps_app_ctxt->pv_mem_mngr_handle;

        /* reigter the callbacks */
        ps_interface_ctxt->ihevce_output_strm_fill_done = ihevce_strm_fill_done;
        ps_interface_ctxt->ihevce_output_recon_fill_done = NULL;
        ps_interface_ctxt->ihevce_set_free_input_buff = NULL;

        /*Added for run time or create time creation*/
        ps_interface_ctxt->i4_create_time_input_allocation = (WORD32)CREATE_TIME_ALLOCATION_INPUT;
        ps_interface_ctxt->i4_create_time_output_allocation = (WORD32)CREATE_TIME_ALLOCATION_OUTPUT;

        ps_interface_ctxt->ihevce_cmds_error_report = ihevce_cmds_error_report;
        ps_interface_ctxt->pv_cmd_err_cb_handle = (void *)ps_ctxt;

        /* --------------------------------------------------------------------- */
        /*           High Level Encoder Instance Creation                        */
        /* --------------------------------------------------------------------- */
        status = ihevce_hle_interface_create(ps_interface_ctxt);
        if((WORD32)IV_FAIL == status)
        {
            ihevce_hle_interface_delete(ps_interface_ctxt);

            memory_free(NULL, ps_interface_ctxt);

            /* free static config memory */
            ps_ctxt->ihevce_mem_free(NULL, ps_ctxt->ps_static_cfg_prms);

            /* free osal handle */
            memory_free(NULL, ps_ctxt->pv_osal_handle);

            /* free plugin ctxt memory */
            memory_free(NULL, ps_ctxt);

            ps_sys_api->ihevce_printf(
                ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in Plugin HLE create failed\n");
            return (IHEVCE_EFAIL);
        }

        /* --------------------------------------------------------------------- */
        /*            Input Output and Command buffer allocation                 */
        /* --------------------------------------------------------------------- */
        {
            WORD32 ctr;
            WORD32 buf_size;
            UWORD8 *pu1_tmp_buf;
            WORD32 i4_res_id;
            WORD32 i4_br_id;
            WORD32 i4_num_resolutions;
            WORD32 ai4_num_bitrate_instances[IHEVCE_MAX_NUM_RESOLUTIONS] = { 1 };
            iv_input_bufs_req_t s_input_bufs_req;
            iv_res_layer_output_bufs_req_t s_res_layer_output_bufs_req;
            iv_res_layer_recon_bufs_req_t s_res_layer_recon_bufs_req;

            /* local array of pointers */
            void *apv_inp_luma_bufs[MAX_NUM_INP_DATA_BUFS];
            void *apv_inp_cb_bufs[MAX_NUM_INP_DATA_BUFS];
            void *apv_inp_cr_bufs[MAX_NUM_INP_DATA_BUFS];
            void *apv_inp_sync_bufs[MAX_NUM_INP_CTRL_SYNC_BUFS];
            void *apv_inp_async_bufs[MAX_NUM_INP_CTRL_ASYNC_BUFS];
            void *apv_out_data_bufs[IHEVCE_MAX_NUM_RESOLUTIONS][IHEVCE_MAX_NUM_BITRATES]
                                   [MAX_NUM_OUT_DATA_BUFS];

            /* get the number of resolutions */
            i4_num_resolutions = ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;

            /* set the size of the structure */
            s_input_bufs_req.i4_size = sizeof(iv_input_bufs_req_t);
            s_res_layer_output_bufs_req.i4_size = sizeof(iv_res_layer_output_bufs_req_t);
            s_res_layer_recon_bufs_req.i4_size = sizeof(iv_res_layer_recon_bufs_req_t);

            /* loop over num resolutions */
            for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
            {
                /* store the number of bitrates */
                ai4_num_bitrate_instances[i4_res_id] =
                    ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_res_id]
                        .i4_num_bitrate_instances;

                /* loop over num bitrates */
                for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                {
                    s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id].i4_size =
                        sizeof(iv_output_bufs_req_t);
                }
            }

            /* call Query I/O buffer */
            status = ihevce_query_io_buf_req(
                ps_interface_ctxt,
                &s_input_bufs_req,
                &s_res_layer_output_bufs_req,
                &s_res_layer_recon_bufs_req);

            /* check on the requirements against the MAX of application */
            /* should be present only for debug purpose                 */

            /* ---------------  Input data buffers init ---------------------- */
            /* allocate memory for input buffers  */
            if(ps_interface_ctxt->i4_create_time_input_allocation == 1)
            {
                buf_size = s_input_bufs_req.i4_min_size_uv_buf + s_input_bufs_req.i4_min_size_y_buf;
                ps_ctxt->s_memtab_inp_data_buf.i4_size = sizeof(iv_mem_rec_t);
                ps_ctxt->s_memtab_inp_data_buf.i4_mem_alignment = 4;
                ps_ctxt->s_memtab_inp_data_buf.i4_mem_size =
                    (s_input_bufs_req.i4_min_num_yuv_bufs + XTRA_INP_DATA_BUFS) * buf_size;
                ps_ctxt->s_memtab_inp_data_buf.e_mem_type = IV_EXT_CACHEABLE_NUMA_NODE0_MEM;

                mem_mngr_alloc(
                    ps_app_ctxt->pv_mem_mngr_handle, ps_sys_api, &ps_ctxt->s_memtab_inp_data_buf);

                pu1_tmp_buf = (UWORD8 *)ps_ctxt->s_memtab_inp_data_buf.pv_base;

                if(NULL == pu1_tmp_buf)
                {
                    ps_sys_api->ihevce_printf(
                        ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in allocate memory\n");
                    return (IHEVCE_EFAIL);
                }

                /* loop to initialise the buffer pointer */
                for(ctr = 0; ctr < s_input_bufs_req.i4_min_num_yuv_bufs + XTRA_INP_DATA_BUFS; ctr++)
                {
                    apv_inp_luma_bufs[ctr] = pu1_tmp_buf;
                    apv_inp_cb_bufs[ctr] = pu1_tmp_buf + s_input_bufs_req.i4_min_size_y_buf;
                    apv_inp_cr_bufs[ctr] = NULL; /* 420SP case */

                    /* increment the input buffer pointer to next buffer */
                    pu1_tmp_buf += buf_size;
                }
            }

            /* ---------------  Output data buffers init ---------------------- */

            /* loop over num resolutions */
            for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
            {
                for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                {
                    buf_size = s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                   .i4_min_size_bitstream_buf;

                    ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id].i4_size =
                        sizeof(iv_mem_rec_t);
                    ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id].i4_mem_alignment = 4;

                    if(!ps_interface_ctxt->i4_create_time_output_allocation)
                    {
                        ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id].i4_mem_size =
                            (s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                 .i4_min_num_out_bufs +
                             XTRA_OUT_DATA_BUFS) *
                            buf_size;
                    }
                    else
                    {
                        ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id].i4_mem_size =
                            (s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                 .i4_min_num_out_bufs) *
                            buf_size;
                    }
                    ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id].e_mem_type =
                        IV_EXT_CACHEABLE_NUMA_NODE1_MEM;

                    mem_mngr_alloc(
                        ps_app_ctxt->pv_mem_mngr_handle,
                        ps_sys_api,
                        &ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id]);

                    pu1_tmp_buf =
                        (UWORD8 *)ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id].pv_base;
                    if(NULL == pu1_tmp_buf)
                    {
                        ps_sys_api->ihevce_printf(
                            ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in allocate memory\n");
                        return (IHEVCE_EFAIL);
                    }

                    if(ps_interface_ctxt->i4_create_time_output_allocation == 1)
                    {
                        /* loop to initialise the buffer pointer */
                        for(ctr = 0;
                            ctr < s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                      .i4_min_num_out_bufs;
                            ctr++)
                        {
                            apv_out_data_bufs[i4_res_id][i4_br_id][ctr] = pu1_tmp_buf;
                            pu1_tmp_buf += buf_size;
                        }
                    }
                    else
                    {
                        WORD32 i4_num_out_bufs =
                            s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                .i4_min_num_out_bufs +
                            XTRA_OUT_DATA_BUFS;
                        ps_ctxt->i4_num_out_bufs = i4_num_out_bufs;
                        ps_ctxt->ai4_free_out_buf_idx[i4_res_id][i4_br_id] = 0;
                        ps_ctxt->i4_prod_out_buf_idx = 0;

                        /* Assert to make sure ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][] array
                        has more bufs than ps_ctxt->i4_num_out_bufs. Needed to identify
                        wrap-around case */
                        ASSERT(ps_ctxt->i4_num_out_bufs <= MAX_NUM_OUT_DATA_BUFS);

                        /* loop to initialise the buffer pointer */
                        for(ctr = 0; ctr < i4_num_out_bufs; ctr++)
                        {
                            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ctr].i4_idx = ctr;
                            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ctr].i4_is_free = 1;
                            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ctr].i4_is_prod = 0;
                            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ctr].i4_bytes_gen = 0;
                            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ctr].pu1_buf = pu1_tmp_buf;
                            ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ctr].i4_buf_size = buf_size;
                            pu1_tmp_buf += buf_size;
                        }
                    }

                    /* create mutex for controlling the out strm buf b/w appln and encoder */
                    ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                        .pv_app_out_strm_buf_mutex_hdl = osal_mutex_create(ps_ctxt->pv_osal_handle);
                    if(NULL == ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                   .pv_app_out_strm_buf_mutex_hdl)
                    {
                        ps_sys_api->ihevce_printf(
                            ps_sys_api->pv_cb_handle,
                            "IHEVCE ERROR: Error in Plugin initialization\n");
                        return (IHEVCE_EFAIL);
                    }

                    /* create mutex for controlling the out strm buf b/w appln and encoder */
                    ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                        .pv_app_out_strm_buf_cond_var_hdl =
                        osal_cond_var_create(ps_ctxt->pv_osal_handle);
                    if(NULL == ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                   .pv_app_out_strm_buf_cond_var_hdl)
                    {
                        ps_sys_api->ihevce_printf(
                            ps_sys_api->pv_cb_handle,
                            "IHEVCE ERROR: Error in Plugin initialization\n");
                        return (IHEVCE_EFAIL);
                    }
                }
            }

            if(ps_interface_ctxt->i4_create_time_input_allocation == 1)
            {
                /* ------------- Input sync command buffers init -------------------- */
                buf_size = s_input_bufs_req.i4_min_size_synch_ctrl_bufs;

                ps_ctxt->s_memtab_inp_sync_ctrl_buf.i4_size = sizeof(iv_mem_rec_t);
                ps_ctxt->s_memtab_inp_sync_ctrl_buf.i4_mem_alignment = 4;
                ps_ctxt->s_memtab_inp_sync_ctrl_buf.i4_mem_size =
                    (s_input_bufs_req.i4_min_num_yuv_bufs + XTRA_INP_DATA_BUFS) * buf_size;
                ps_ctxt->s_memtab_inp_sync_ctrl_buf.e_mem_type = IV_EXT_CACHEABLE_NUMA_NODE0_MEM;

                mem_mngr_alloc(
                    ps_app_ctxt->pv_mem_mngr_handle,
                    ps_sys_api,
                    &ps_ctxt->s_memtab_inp_sync_ctrl_buf);

                pu1_tmp_buf = (UWORD8 *)ps_ctxt->s_memtab_inp_sync_ctrl_buf.pv_base;

                if(NULL == pu1_tmp_buf)
                {
                    ps_sys_api->ihevce_printf(
                        ps_sys_api->pv_cb_handle, "IHEVCE ERROR: Error in allocate memory\n");
                    return (IHEVCE_EFAIL);
                }

                /* loop to initialise the buffer pointer */
                for(ctr = 0; ctr < s_input_bufs_req.i4_min_num_yuv_bufs + XTRA_INP_DATA_BUFS; ctr++)
                {
                    apv_inp_sync_bufs[ctr] = pu1_tmp_buf;
                    pu1_tmp_buf += buf_size;
                }
            }

            /* ------------- Input async command buffers init -------------------- */
            buf_size = s_input_bufs_req.i4_min_size_asynch_ctrl_bufs;

            /* allocate memory for output status buffer */
            ps_ctxt->pu1_inp_async_ctrl_buf = (UWORD8 *)ps_ctxt->ihevce_mem_alloc(
                NULL, s_input_bufs_req.i4_min_num_asynch_ctrl_bufs * buf_size);
            if(ps_ctxt->pu1_inp_async_ctrl_buf == NULL)
            {
                ps_sys_api->ihevce_printf(
                    ps_sys_api->pv_cb_handle,
                    "IHEVCE ERROR: Error in Plugin memory initialization\n");
                return (IHEVCE_EFAIL);
            }

            pu1_tmp_buf = ps_ctxt->pu1_inp_async_ctrl_buf;

            /* loop to initialise the buffer pointer */
            for(ctr = 0; ctr < s_input_bufs_req.i4_min_num_asynch_ctrl_bufs; ctr++)
            {
                apv_inp_async_bufs[ctr] = pu1_tmp_buf;
                pu1_tmp_buf += buf_size;
            }

            /* Create IO ports for the buffer allocated */
            {
                iv_input_data_ctrl_buffs_desc_t s_inp_desc;
                iv_input_asynch_ctrl_buffs_desc_t s_inp_ctrl_desc;
                iv_res_layer_output_data_buffs_desc_t s_mres_out_desc;
                iv_res_layer_recon_data_buffs_desc_t s_mres_recon_desc;

                /* set the parameters of the input data control desc */
                s_inp_desc.i4_size = sizeof(iv_input_data_ctrl_buffs_desc_t);
                s_inp_desc.i4_num_synch_ctrl_bufs = s_input_bufs_req.i4_min_num_synch_ctrl_bufs;
                s_inp_desc.i4_num_yuv_bufs =
                    s_input_bufs_req.i4_min_num_yuv_bufs + XTRA_INP_DATA_BUFS;
                s_inp_desc.i4_size_y_buf = s_input_bufs_req.i4_min_size_y_buf;
                s_inp_desc.i4_size_uv_buf = s_input_bufs_req.i4_min_size_uv_buf;
                s_inp_desc.i4_size_synch_ctrl_bufs = s_input_bufs_req.i4_min_size_synch_ctrl_bufs;
                s_inp_desc.ppv_synch_ctrl_bufs = &apv_inp_sync_bufs[0];
                s_inp_desc.ppv_y_buf = &apv_inp_luma_bufs[0];
                s_inp_desc.ppv_u_buf = &apv_inp_cb_bufs[0];
                s_inp_desc.ppv_v_buf = &apv_inp_cr_bufs[0];

                /* set the parameters of the input async control desc */
                s_inp_ctrl_desc.i4_size = sizeof(iv_input_asynch_ctrl_buffs_desc_t);
                s_inp_ctrl_desc.i4_num_asynch_ctrl_bufs =
                    s_input_bufs_req.i4_min_num_asynch_ctrl_bufs;
                s_inp_ctrl_desc.i4_size_asynch_ctrl_bufs =
                    s_input_bufs_req.i4_min_size_asynch_ctrl_bufs;
                s_inp_ctrl_desc.ppv_asynch_ctrl_bufs = &apv_inp_async_bufs[0];

                for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
                {
                    /* set the parameters of the output data desc */
                    for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                    {
                        s_mres_out_desc.s_output_data_buffs[i4_res_id][i4_br_id].i4_size =
                            sizeof(iv_output_data_buffs_desc_t);

                        if(!ps_interface_ctxt->i4_create_time_output_allocation)
                        {
                            s_mres_out_desc.s_output_data_buffs[i4_res_id][i4_br_id]
                                .i4_num_bitstream_bufs =
                                s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                    .i4_min_num_out_bufs +
                                XTRA_OUT_DATA_BUFS;
                        }
                        else
                        {
                            s_mres_out_desc.s_output_data_buffs[i4_res_id][i4_br_id]
                                .i4_num_bitstream_bufs =
                                s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                    .i4_min_num_out_bufs;
                        }

                        s_mres_out_desc.s_output_data_buffs[i4_res_id][i4_br_id]
                            .i4_size_bitstream_buf =
                            s_res_layer_output_bufs_req.s_output_buf_req[i4_res_id][i4_br_id]
                                .i4_min_size_bitstream_buf;
                        s_mres_out_desc.s_output_data_buffs[i4_res_id][i4_br_id].ppv_bitstream_bufs =
                            &apv_out_data_bufs[i4_res_id][i4_br_id][0];
                    }
                }

                s_mres_recon_desc.i4_size = sizeof(iv_res_layer_recon_data_buffs_desc_t);
                /* call create I/O ports */
                status = ihevce_create_ports(
                    ps_interface_ctxt,
                    &s_inp_desc,
                    &s_inp_ctrl_desc,
                    &s_mres_out_desc,
                    &s_mres_recon_desc);
            }
        }

        /* --------------------------------------------------------------------- */
        /*            Create a High level encoder  thread                        */
        /* --------------------------------------------------------------------- */
        {
            osal_thread_attr_t s_thread_attr = OSAL_DEFAULT_THREAD_ATTR;

            /* Initialize application thread attributes */
            s_thread_attr.exit_code = 0;
            s_thread_attr.name = 0;
            s_thread_attr.priority_map_flag = 1;
            s_thread_attr.priority = OSAL_PRIORITY_DEFAULT;
            s_thread_attr.stack_addr = 0;
            s_thread_attr.stack_size = THREAD_STACK_SIZE;
            s_thread_attr.thread_func = ihevce_hle_interface_thrd;
            s_thread_attr.thread_param = (void *)(ps_interface_ctxt);
            s_thread_attr.core_affinity_mask = 0;
            s_thread_attr.group_num = 0;

            /* Create High level encoder thread */
            ps_ctxt->pv_hle_thread_hdl =
                osal_thread_create(ps_ctxt->pv_osal_handle, &s_thread_attr);
            if(NULL == ps_ctxt->pv_hle_thread_hdl)
            {
                return IHEVCE_EFAIL;
            }
        }

        /* --------------------------------------------------------------------- */
        /*                 Wait until HLE init is done                           */
        /* --------------------------------------------------------------------- */
        {
            volatile WORD32 hle_init_done;
            volatile WORD32 *pi4_hle_init_done;

            pi4_hle_init_done = (volatile WORD32 *)&ps_interface_ctxt->i4_hle_init_done;

            do
            {
                hle_init_done = *pi4_hle_init_done;

            } while(0 == hle_init_done);
        }

        /* reset flush mode */
        ps_ctxt->i4_flush_mode_on = 0;

        {
            WORD32 i4_res_id;
            WORD32 i4_br_id;
            for(i4_res_id = 0; i4_res_id < IHEVCE_MAX_NUM_RESOLUTIONS; i4_res_id++)
            {
                for(i4_br_id = 0; i4_br_id < IHEVCE_MAX_NUM_BITRATES; i4_br_id++)
                {
                    /* reset out end flag */
                    ps_ctxt->ai4_out_end_flag[i4_res_id][i4_br_id] = 0;
                }
            }
        }

        /* reset the field id */
        ps_ctxt->i4_field_id = 0;

        /* based on number of B pics set the DTS value */
        ps_ctxt->i8_dts = -1;

        if(0 != ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers)
        {
            ps_ctxt->i8_dts =
                (-1) *
                (1 << ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers);
        }

        /* initialsie the buffer stride */
        {
            WORD32 max_cu_size;

            max_cu_size = (1 << ps_ctxt->ps_static_cfg_prms->s_config_prms.i4_max_log2_cu_size);
            ps_ctxt->i4_frm_stride =
                ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width +
                SET_CTB_ALIGN(ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width, max_cu_size);
        }
    }
    else
    {
        /* free plugin ctxt memory */
        memory_free(NULL, ps_ctxt);

        return (IHEVCE_EFAIL);
    }

    /* reset the place holders of old bitrate */
    memset(&ps_ctxt->ai4_old_bitrate[0][0], 0, sizeof(ps_ctxt->ai4_old_bitrate));

    ps_ctxt->ai4_old_bitrate[0][0] = ps_params->s_tgt_lyr_prms.as_tgt_params[0].ai4_tgt_bitrate[0];

    /* store the plugin handle before returning */
    *ppv_ihevce_hdl = (void *)ps_ctxt;

    return (IHEVCE_EOK);
}

static IHEVCE_PLUGIN_STATUS_T
    ihevce_receive_out_buffer(plugin_ctxt_t *ps_ctxt, ihevce_out_buf_t *ps_out)
{
    app_ctxt_t *ps_app_ctxt = &ps_ctxt->s_app_ctxt;
    WORD32 i4_res_id, i4_br_id;
    WORD32 i4_num_resolutions;
    WORD32 ai4_num_bitrate_instances[IHEVCE_MAX_NUM_RESOLUTIONS] = { 1 };

    i4_num_resolutions = ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;
    for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
    {
        ai4_num_bitrate_instances[i4_res_id] =
            ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_res_id]
                .i4_num_bitrate_instances;
    }
    /* default init */
    ps_out->pu1_output_buf = NULL;
    ps_out->i4_bytes_generated = 0;

    /* ---------------- if any output buffer is available return the buffer back ------------- */
    while(1)
    {
        WORD32 osal_result;
        WORD32 buf_present = 0;
        WORD32 i4_is_prod = 1;
        WORD32 i4_atleast_one_br_prod = 0;
        /****** Lock the critical section ******/
        osal_result =
            osal_mutex_lock(ps_app_ctxt->as_out_strm_prms[0][0].pv_app_out_strm_buf_mutex_hdl);

        if(OSAL_SUCCESS != osal_result)
            return IHEVCE_EFAIL;

        /* wait until entropy sends an output */
        while(1)
        {
            i4_is_prod = 1;
            for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
            {
                for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                {
                    i4_is_prod &=
                        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ps_ctxt->i4_prod_out_buf_idx]
                            .i4_is_prod;
                    i4_atleast_one_br_prod |=
                        ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ps_ctxt->i4_prod_out_buf_idx]
                            .i4_is_prod;
                }
            }
            if(!i4_is_prod)
            {
                for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
                {
                    for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                    {
                        osal_cond_var_wait(
                            ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                .pv_app_out_strm_buf_cond_var_hdl,
                            ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                .pv_app_out_strm_buf_mutex_hdl);
                    }
                }
            }
            else
            {
                break;
            }
        }

        ASSERT(i4_is_prod == 1);

        /* check if the current buffer for all bitrates and resolutions have been produced */
        if(1 == i4_is_prod)
        {
            buf_present = 1;

            for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
            {
                for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                {
                    /* set the buffer to free status */
                    ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][ps_ctxt->i4_prod_out_buf_idx]
                        .i4_is_free = 1;
                    if((0 == i4_res_id) && (0 == i4_br_id))
                    {
                        ps_out->i4_bytes_generated =
                            ps_ctxt->aaas_out_bufs[0][0][ps_ctxt->i4_prod_out_buf_idx].i4_bytes_gen;
                        ps_out->pu1_output_buf =
                            ps_ctxt->aaas_out_bufs[0][0][ps_ctxt->i4_prod_out_buf_idx].pu1_buf;
                    }
                }
            }

            /* copy the contents to output buffer */
            ps_out->i4_is_key_frame =
                ps_ctxt->aaas_out_bufs[0][0][ps_ctxt->i4_prod_out_buf_idx].i4_is_key_frame;
            ps_out->u8_pts =
                ps_ctxt->aaas_out_bufs[0][0][ps_ctxt->i4_prod_out_buf_idx].i4_timestamp_low;
            ps_out->u8_pts =
                ps_out->u8_pts |
                ((ULWORD64)(
                     ps_ctxt->aaas_out_bufs[0][0][ps_ctxt->i4_prod_out_buf_idx].i4_timestamp_high)
                 << 32);
            ps_out->i4_end_flag =
                ps_ctxt->aaas_out_bufs[0][0][ps_ctxt->i4_prod_out_buf_idx].i4_end_flag;
            ps_out->i8_dts = ps_ctxt->i8_dts;

            /* increment the DTS */
            ps_ctxt->i8_dts++;
        }

        /* check for buffer present */
        if(1 == buf_present)
        {
            ps_ctxt->i4_prod_out_buf_idx++;

            /* wrap around case */
            if(ps_ctxt->i4_prod_out_buf_idx == ps_ctxt->i4_num_out_bufs)
            {
                ps_ctxt->i4_prod_out_buf_idx = 0;
            }

            /****** Unlock the critical section ******/
            osal_result = osal_mutex_unlock(
                ps_app_ctxt->as_out_strm_prms[0][0].pv_app_out_strm_buf_mutex_hdl);
            if(OSAL_SUCCESS != osal_result)
                return IHEVCE_EFAIL;

            /* break while 1 loop */
            break;
        }
        else
        {
            /* in steady state*/
            if(0 == ps_ctxt->i4_flush_mode_on)
            {
                /****** Unlock the critical section ******/
                osal_result = osal_mutex_unlock(
                    ps_app_ctxt->as_out_strm_prms[0][0].pv_app_out_strm_buf_mutex_hdl);
                if(OSAL_SUCCESS != osal_result)
                    return IHEVCE_EFAIL;
                if(!i4_atleast_one_br_prod) /*** If atleast one bitrate is produced do not break from loop **/
                { /*** Continue in while loop and Wait for next bitrate ***/
                    /* break while 1 loop */
                    break;
                }
            }
            else
            {
                /* In flush mode is ON then this function must return output
                buffers. Otherwise assume that encoding is over and return fail */
                /****** Unlock the critical section ******/
                osal_result = osal_mutex_unlock(
                    ps_app_ctxt->as_out_strm_prms[0][0].pv_app_out_strm_buf_mutex_hdl);
                if(OSAL_SUCCESS != osal_result)
                    return IHEVCE_EFAIL;
            }
        }
    }

    return IHEVCE_EOK;
}

static IHEVCE_PLUGIN_STATUS_T
    ihevce_queue_out_buffer(plugin_ctxt_t *ps_ctxt, WORD32 i4_res_id, WORD32 i4_br_id)
{
    app_ctxt_t *ps_app_ctxt = &ps_ctxt->s_app_ctxt;
    ihevce_hle_ctxt_t *ps_interface_ctxt = (ihevce_hle_ctxt_t *)ps_ctxt->pv_hle_interface_ctxt;

    /* --------------------------------------------------------------------- */
    /*           Free Output buffer Queuing                                  */
    /* --------------------------------------------------------------------- */
    /* ------- Que in free output buffer if end flag is not set ------ */
    if(0 == ps_ctxt->ai4_out_end_flag[i4_res_id][i4_br_id])
    {
        WORD32 osal_result;
        iv_output_data_buffs_t *ps_curr_out;
        WORD32 buf_id_strm;
        WORD32 free_idx;

        free_idx = ps_ctxt->ai4_free_out_buf_idx[i4_res_id][i4_br_id];

        if(1 == ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_is_free)
        {
            /* ---------- get a free desc. from output Q ------ */
            ps_curr_out = (iv_output_data_buffs_t *)ihevce_q_get_free_out_strm_buff(
                ps_interface_ctxt, &buf_id_strm, BUFF_QUE_NON_BLOCKING_MODE, i4_br_id, i4_res_id);

            /* if a free buffer is available */
            if(NULL != ps_curr_out)
            {
                /****** Lock the critical section ******/
                osal_result = osal_mutex_lock(ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                                  .pv_app_out_strm_buf_mutex_hdl);

                if(OSAL_SUCCESS != osal_result)
                    return IHEVCE_EFAIL;

                if(1 == ps_app_ctxt->ai4_out_strm_end_flag[i4_res_id][i4_br_id])
                {
                    ps_curr_out->i4_is_last_buf = 1;
                    ps_ctxt->ai4_out_end_flag[i4_res_id][i4_br_id] = 1;
                }
                else
                {
                    ps_curr_out->i4_is_last_buf = 0;
                }
                ASSERT(ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_is_free == 1);
                ASSERT(free_idx == ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_idx);

                ps_curr_out->pv_bitstream_bufs =
                    (void *)ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].pu1_buf;
                ps_curr_out->i4_cb_buf_id =
                    ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_idx;
                ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_is_free = 0;
                ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_is_prod = 0;
                ps_ctxt->aaas_out_bufs[i4_res_id][i4_br_id][free_idx].i4_bytes_gen = 0;

                ps_ctxt->ai4_free_out_buf_idx[i4_res_id][i4_br_id]++;

                /* wrap around case */
                if(ps_ctxt->ai4_free_out_buf_idx[i4_res_id][i4_br_id] == ps_ctxt->i4_num_out_bufs)
                {
                    ps_ctxt->ai4_free_out_buf_idx[i4_res_id][i4_br_id] = 0;
                }

                /****** Unlock the critical section ******/
                osal_result = osal_mutex_unlock(ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                                    .pv_app_out_strm_buf_mutex_hdl);
                if(OSAL_SUCCESS != osal_result)
                    return IHEVCE_EFAIL;

                /* ---------- set the buffer as produced ---------- */
                ihevce_q_set_out_strm_buff_prod(
                    ps_interface_ctxt, buf_id_strm, i4_br_id, i4_res_id);
            }
        }
    }
    return IHEVCE_EOK;
}

/*!
******************************************************************************
* \if Function name : ihevce_close \endif
*
* \brief
*    De-Initialises the enocder context and threads
*
* \param[in] Static params pointer
*
* \return
*    status
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T ihevce_close(void *pv_ihevce_hdl)
{
    /* local variables */
    plugin_ctxt_t *ps_ctxt;
    app_ctxt_t *ps_app_ctxt;
    ihevce_hle_ctxt_t *ps_interface_ctxt;
    WORD32 i4_num_resolutions;
    WORD32 i4_res_id;
    WORD32 i4_br_id;
    WORD32 ai4_num_bitrate_instances[IHEVCE_MAX_NUM_RESOLUTIONS] = { 1 };
    ihevce_sys_api_t *ps_sys_api;

    /* sanity checks */
    if(NULL == pv_ihevce_hdl)
        return (IHEVCE_EFAIL);

    /* derive local variables */
    ps_ctxt = (plugin_ctxt_t *)pv_ihevce_hdl;

    ps_sys_api = &ps_ctxt->ps_static_cfg_prms->s_sys_api;

    if((0 == ps_ctxt->ps_static_cfg_prms->i4_res_id) &&
       (0 == ps_ctxt->ps_static_cfg_prms->i4_br_id))
    {
        ps_interface_ctxt = (ihevce_hle_ctxt_t *)ps_ctxt->pv_hle_interface_ctxt;
        ps_app_ctxt = &ps_ctxt->s_app_ctxt;
        i4_num_resolutions = ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;

        if(1 != ps_ctxt->i4_flush_mode_on)
        {
            for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
            {
                ai4_num_bitrate_instances[i4_res_id] =
                    ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_res_id]
                        .i4_num_bitrate_instances;
                for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
                {
                    /* ------- Que in free output buffer if end flag is not set ------ */
                    ihevce_queue_out_buffer(ps_ctxt, i4_res_id, i4_br_id);
                }
            }
            /* --------------------------------------------------------------------- */
            /*            Input Processing                                           */
            /* --------------------------------------------------------------------- */
            {
                WORD32 buf_id;

                iv_input_data_ctrl_buffs_t *ps_curr_inp;
                WORD32 *pi4_ctrl_ptr;

                /* ---------- get a free buffer from input Q ------ */
                ps_curr_inp = (iv_input_data_ctrl_buffs_t *)ihevce_q_get_free_inp_data_buff(
                    ps_interface_ctxt, &buf_id, BUFF_QUE_BLOCKING_MODE);

                if(NULL != ps_curr_inp)
                {
                    /* flush mode command */

                    ps_curr_inp->i4_buf_id = buf_id;

                    /* set the input status to invalid flag */
                    ps_curr_inp->i4_inp_frm_data_valid_flag = 0;

                    pi4_ctrl_ptr = (WORD32 *)ps_curr_inp->pv_synch_ctrl_bufs;

                    *pi4_ctrl_ptr = IHEVCE_SYNCH_API_FLUSH_TAG;
                    *(pi4_ctrl_ptr + 1) = 0;
                    *(pi4_ctrl_ptr + 2) = IHEVCE_SYNCH_API_END_TAG;

                    ps_curr_inp->i4_cmd_buf_size = 4 * 3; /* 4 bytes */

                    /* ---------- set the buffer as produced ---------- */
                    ihevce_q_set_inp_data_buff_prod(ps_interface_ctxt, buf_id);
                }
                else
                {
                    /* Enable flush-mode and internal-flush once limit according to
                    Eval-version is reached */
                    ps_ctxt->i4_flush_mode_on = 1;
                }
            }
        }

        /* --------------------------------------------------------------------- */
        /*            Wait and destroy Processing threads                        */
        /* --------------------------------------------------------------------- */

        /* Wait for High level encoder thread to complete */
        osal_thread_wait(ps_ctxt->pv_hle_thread_hdl);

        /* Destroy Hle thread */
        osal_thread_destroy(ps_ctxt->pv_hle_thread_hdl);

        /* --------------------------------------------------------------------- */
        /*            Input Output and Command buffers free                      */
        /* --------------------------------------------------------------------- */

        /* free output data and control buffer */

        for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
        {
            ai4_num_bitrate_instances[i4_res_id] =
                ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_res_id]
                    .i4_num_bitrate_instances;

            for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
            {
                mem_mngr_free(
                    ps_app_ctxt->pv_mem_mngr_handle,
                    &ps_ctxt->as_memtab_out_data_buf[i4_res_id][i4_br_id]);

                /* free mutex of out strm buf b/w appln and encoder */
                osal_mutex_destroy(ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                       .pv_app_out_strm_buf_mutex_hdl);

                osal_cond_var_destroy(ps_app_ctxt->as_out_strm_prms[i4_res_id][i4_br_id]
                                          .pv_app_out_strm_buf_cond_var_hdl);
            }
        }

        ps_ctxt->ihevce_mem_free(NULL, ps_ctxt->pu1_out_ctrl_buf);
        ps_ctxt->ihevce_mem_free(NULL, ps_ctxt->pu1_inp_async_ctrl_buf);

        /* free input data and control buffer */
        if(ps_interface_ctxt->i4_create_time_input_allocation == 1)
        {
            mem_mngr_free(ps_app_ctxt->pv_mem_mngr_handle, &ps_ctxt->s_memtab_inp_data_buf);
            mem_mngr_free(ps_app_ctxt->pv_mem_mngr_handle, &ps_ctxt->s_memtab_inp_sync_ctrl_buf);
        }

        /* --------------------------------------------------------------------- */
        /*               Encoder Instance Deletion                               */
        /* --------------------------------------------------------------------- */
        ihevce_hle_interface_delete(ps_interface_ctxt);

        /* free the high level encoder context memory */
        ps_ctxt->ihevce_mem_free(NULL, ps_ctxt->pv_hle_interface_ctxt);

        if(ps_ctxt->ps_static_cfg_prms->i4_enable_csv_dump)
        {
            ps_sys_api->s_file_io_api.ihevce_fclose(
                (void *)ps_sys_api->pv_cb_handle, ps_ctxt->ps_static_cfg_prms->apF_csv_file[0][0]);
        }

        /* free static config memory */
        ps_ctxt->ihevce_mem_free(NULL, ps_ctxt->ps_static_cfg_prms);

        /* free osal handle */
        memory_free(NULL, ps_ctxt->pv_osal_handle);

        /* free plugin ctxt memory */
        memory_free(NULL, pv_ihevce_hdl);
    }
    else
    {
        return (IHEVCE_EFAIL);
    }

    return (IHEVCE_EOK);
}

/*!
******************************************************************************
* \if Function name : ihevce_copy_inp_8bit \endif
*
* \brief
*    Input copy function for 8 bit input
*
* \param[in] Source and desdtination buffer descriptors
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_copy_inp_8bit(
    iv_input_data_ctrl_buffs_t *ps_curr_inp,
    ihevce_inp_buf_t *ps_inp,
    WORD32 chroma_format,
    WORD32 i4_orig_wd,
    WORD32 i4_orig_ht)
{
    UWORD8 *pu1_src, *pu1_dst;
    WORD32 src_strd, dst_strd;
    WORD32 frm_height = i4_orig_ht;
    WORD32 frm_width = i4_orig_wd;
    WORD32 buf_height = ps_curr_inp->s_input_buf.i4_y_ht;
    WORD32 buf_width = ps_curr_inp->s_input_buf.i4_y_wd;
    WORD32 rows, cols;

    pu1_src = (UWORD8 *)ps_inp->apv_inp_planes[0];
    src_strd = ps_inp->ai4_inp_strd[0];
    pu1_dst = (UWORD8 *)ps_curr_inp->s_input_buf.pv_y_buf;
    dst_strd = ps_curr_inp->s_input_buf.i4_y_strd;

    if((ps_inp->ai4_inp_size[0] < (src_strd * frm_height)) || (ps_inp->ai4_inp_size[0] <= 0) ||
       (ps_inp->apv_inp_planes[0] == NULL))
    {
        return (IV_FAIL);
    }
    /* copy the input luma data into internal buffer */
    for(rows = 0; rows < frm_height; rows++)
    {
        memcpy(pu1_dst, pu1_src, frm_width);
        if(buf_width > frm_width)
        {
            memset(pu1_dst + frm_width, 0x0, buf_width - frm_width);
        }
        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }
    while(rows < buf_height)
    {
        memset(pu1_dst, 0x0, buf_width);
        pu1_dst += dst_strd;
        rows++;
    }

    if(IV_YUV_420P == chroma_format)
    {
        UWORD8 *pu1_src_u, *pu1_src_v;
        WORD32 src_strd_u, src_strd_v;

        pu1_src_u = (UWORD8 *)ps_inp->apv_inp_planes[1];
        src_strd_u = ps_inp->ai4_inp_strd[1];
        pu1_src_v = (UWORD8 *)ps_inp->apv_inp_planes[2];
        src_strd_v = ps_inp->ai4_inp_strd[2];
        pu1_dst = (UWORD8 *)ps_curr_inp->s_input_buf.pv_u_buf;
        dst_strd = ps_curr_inp->s_input_buf.i4_uv_strd;

        frm_width = i4_orig_wd >> 1;
        frm_height = i4_orig_ht >> 1;
        buf_width = ps_curr_inp->s_input_buf.i4_uv_wd;
        buf_height = ps_curr_inp->s_input_buf.i4_uv_ht;

        if((ps_inp->ai4_inp_size[1] < (ps_inp->ai4_inp_strd[1] * frm_height)) ||
           (ps_inp->ai4_inp_size[1] <= 0) || (pu1_src_u == NULL))
        {
            return (IV_FAIL);
        }
        if((ps_inp->ai4_inp_size[2] < (ps_inp->ai4_inp_strd[2] * frm_height)) ||
           (ps_inp->ai4_inp_size[2] <= 0) || (pu1_src_v == NULL))
        {
            return (IV_FAIL);
        }

        /* copy the input chroma data in pixel interleaved format */
        for(rows = 0; rows < frm_height; rows++)
        {
            for(cols = 0; cols < frm_width; cols++)
            {
                /* U V alternate */
                pu1_dst[(cols << 1)] = pu1_src_u[cols];
                pu1_dst[(cols << 1) + 1] = pu1_src_v[cols];
            }
            if(buf_width > (cols << 1))
            {
                memset(&pu1_dst[(cols << 1)], 0x80, buf_width - (cols << 1));
            }

            pu1_src_u += src_strd_u;
            pu1_src_v += src_strd_v;
            pu1_dst += dst_strd;
        }
        while(rows < buf_height)
        {
            memset(pu1_dst, 0x80, buf_width);

            pu1_dst += dst_strd;
            rows++;
        }
    }
    else if(IV_YUV_420SP_UV == chroma_format)
    {
        pu1_src = (UWORD8 *)ps_inp->apv_inp_planes[1];
        src_strd = ps_inp->ai4_inp_strd[1];
        pu1_dst = (UWORD8 *)ps_curr_inp->s_input_buf.pv_u_buf;
        dst_strd = ps_curr_inp->s_input_buf.i4_uv_strd;

        frm_width = i4_orig_wd;
        frm_height = i4_orig_ht >> 1;
        buf_width = ps_curr_inp->s_input_buf.i4_uv_wd;
        buf_height = ps_curr_inp->s_input_buf.i4_uv_ht;

        if((ps_inp->ai4_inp_size[1] < (ps_inp->ai4_inp_strd[1] * frm_height)) ||
           (ps_inp->ai4_inp_size[1] <= 0) || (pu1_src == NULL))
        {
            return (IV_FAIL);
        }

        /* copy the input luma data into internal buffer */
        for(rows = 0; rows < frm_height; rows++)
        {
            memcpy(pu1_dst, pu1_src, frm_width);
            if(buf_width > frm_width)
            {
                memset(pu1_dst + frm_width, 0x80, buf_width - frm_width);
            }
            pu1_src += src_strd;
            pu1_dst += dst_strd;
        }
        while(rows < buf_height)
        {
            memset(pu1_dst, 0x80, buf_width);
            pu1_dst += dst_strd;
            rows++;
        }
    }
    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_encode_header \endif
*
* \brief
*    Receive sps, pps and vps of the encoded sequence
*
* \param[in] Plugin handle, Output buffer
*
* \return
*    Success or Failure
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T ihevce_encode_header(void *pv_ihevce_hdl, ihevce_out_buf_t *ps_out)
{
    plugin_ctxt_t *ps_ctxt = (plugin_ctxt_t *)pv_ihevce_hdl;
    ihevce_hle_ctxt_t *ps_interface_ctxt;

    /* sanity checks */
    if(NULL == pv_ihevce_hdl)
        return (IHEVCE_EFAIL);

    if(NULL == ps_out)
        return (IHEVCE_EFAIL);

    if((0 == ps_ctxt->ps_static_cfg_prms->i4_res_id) &&
       (0 == ps_ctxt->ps_static_cfg_prms->i4_br_id))
    {
        WORD32 status;

        /* ------- Que in free output buffer if end flag is not set ------ */
        ihevce_queue_out_buffer(ps_ctxt, 0, 0);

        /* ------- API call to encoder header ------- */
        ps_interface_ctxt = (ihevce_hle_ctxt_t *)ps_ctxt->pv_hle_interface_ctxt;
        status = ihevce_entropy_encode_header(ps_interface_ctxt, 0, 0);
        if(status)
            return IHEVCE_EFAIL;

        /* ------- receive header ------- */
        ihevce_receive_out_buffer(ps_ctxt, ps_out);
    }
    else
    {
        return (IHEVCE_EFAIL);
    }

    return (IHEVCE_EOK);
}

/*!
******************************************************************************
* \if Function name : ihevce_encode \endif
*
* \brief
*    Frame level processing function
*
* \param[in] Plugin handle, Input buffer, Output buffer
*
* \return
*    Success or Failure
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IHEVCE_PLUGIN_STATUS_T
    ihevce_encode(void *pv_ihevce_hdl, ihevce_inp_buf_t *ps_inp, ihevce_out_buf_t *ps_out)
{
    /* local variables */
    plugin_ctxt_t *ps_ctxt;
    app_ctxt_t *ps_app_ctxt;
    ihevce_hle_ctxt_t *ps_interface_ctxt;

    WORD32 i4_res_id, i4_br_id;
    WORD32 i4_num_resolutions;
    WORD32 ai4_num_bitrate_instances[IHEVCE_MAX_NUM_RESOLUTIONS] = { 1 };
    UWORD32 u4_latency = 0;

    /* sanity checks */
    if(NULL == pv_ihevce_hdl)
        return (IHEVCE_EFAIL);

    if(NULL == ps_out)
        return (IHEVCE_EFAIL);

    /* derive local variables */
    ps_ctxt = (plugin_ctxt_t *)pv_ihevce_hdl;
    if((0 == ps_ctxt->ps_static_cfg_prms->i4_res_id) &&
       (0 == ps_ctxt->ps_static_cfg_prms->i4_br_id))
    {
        ps_interface_ctxt = (ihevce_hle_ctxt_t *)ps_ctxt->pv_hle_interface_ctxt;
        ps_app_ctxt = &ps_ctxt->s_app_ctxt;
        i4_num_resolutions = ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;

        if(ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers)
        {
            u4_latency +=
                (1 << ps_ctxt->ps_static_cfg_prms->s_coding_tools_prms.i4_max_temporal_layers) - 1;
        }

        u4_latency += ps_ctxt->ps_static_cfg_prms->s_lap_prms.i4_rc_look_ahead_pics;

        /* Once the internal-flush-flag has been set and codec has issued
        end flag, exit encoding by returning IHEVCE_EFAIL */
        if(ps_ctxt->i4_internal_flush)
        {
            if(1 == ps_app_ctxt->ai4_out_strm_end_flag[0][0])
                return (IHEVCE_EFAIL);
        }

        for(i4_res_id = 0; i4_res_id < i4_num_resolutions; i4_res_id++)
        {
            ai4_num_bitrate_instances[i4_res_id] =
                ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_res_id]
                    .i4_num_bitrate_instances;
            for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[i4_res_id]; i4_br_id++)
            {
                /* ------- Que in free output buffer if end flag is not set ------ */
                ihevce_queue_out_buffer(ps_ctxt, i4_res_id, i4_br_id);
            }
        }

        /* --------------------------------------------------------------------- */
        /*            Input Processing                                           */
        /* --------------------------------------------------------------------- */
        if(0 == ps_ctxt->i4_flush_mode_on)
        {
            WORD32 frm_stride;
            WORD32 frm_width;
            WORD32 frm_height;
            WORD32 buf_id;

            iv_input_data_ctrl_buffs_t *ps_curr_inp;
            WORD32 *pi4_ctrl_ptr;

            frm_width = ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_width;
            frm_height = ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_height;
            frm_stride = ps_ctxt->i4_frm_stride;

            /* ---------- get a free buffer from input Q ------ */
            ps_curr_inp = (iv_input_data_ctrl_buffs_t *)ihevce_q_get_free_inp_data_buff(
                ps_interface_ctxt, &buf_id, BUFF_QUE_BLOCKING_MODE);

            if(NULL != ps_curr_inp)
            {
                /* if input buffer is not NULL */
                if(NULL != ps_inp)
                {
                    WORD32 result;

                    pi4_ctrl_ptr = (WORD32 *)ps_curr_inp->pv_synch_ctrl_bufs;

                    /* ---------- set ip params ---------- */
                    ps_curr_inp->s_input_buf.i4_size = sizeof(iv_yuv_buf_t);
                    ps_curr_inp->s_input_buf.i4_y_wd = frm_width;
                    ps_curr_inp->s_input_buf.i4_y_ht = frm_height;
                    ps_curr_inp->s_input_buf.i4_y_strd = frm_stride;
                    ps_curr_inp->s_input_buf.i4_uv_wd = frm_width;
                    ps_curr_inp->s_input_buf.i4_uv_ht =
                        frm_height >>
                        ((ps_ctxt->ps_static_cfg_prms->s_src_prms.inp_chr_format == 13) ? 0 : 1);
                    ps_curr_inp->s_input_buf.i4_uv_strd = frm_stride;

                    ps_curr_inp->i4_buf_id = buf_id;
                    ps_curr_inp->i4_inp_frm_data_valid_flag = 1;
                    ps_curr_inp->i4_topfield_first = 1; /* set to default */
                    ps_curr_inp->i4_bottom_field = ps_ctxt->i4_field_id;
                    ps_curr_inp->i4_inp_timestamp_low = (WORD32)(ps_inp->u8_pts & 0xFFFFFFFF);
                    ps_curr_inp->i4_inp_timestamp_high = (WORD32)(ps_inp->u8_pts >> 32);

                    /* toggle field id */
                    ps_ctxt->i4_field_id = !ps_ctxt->i4_field_id;

                    /* ---------- Introduction of Force IDR locs   ---------- */
                    if(ps_inp->i4_force_idr_flag)
                    {
                        *pi4_ctrl_ptr = IHEVCE_SYNCH_API_FORCE_IDR_TAG;
                        *(pi4_ctrl_ptr + 1) = 0;
                        pi4_ctrl_ptr += 2;

                        /* set the cmd to NA */
                        *pi4_ctrl_ptr = IHEVCE_SYNCH_API_END_TAG;

                        ps_curr_inp->i4_cmd_buf_size = 4 * 3; /* 12 bytes */
                    }
                    else
                    {
                        /* set the cmd to NA */
                        *pi4_ctrl_ptr = IHEVCE_SYNCH_API_END_TAG;

                        ps_curr_inp->i4_cmd_buf_size = 4; /* 4 bytes */
                    }
                    /* call the input copy function */
                    result = ihevce_copy_inp_8bit(
                        ps_curr_inp,
                        ps_inp,
                        ps_ctxt->ps_static_cfg_prms->s_src_prms.inp_chr_format,
                        ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_orig_width,
                        ps_ctxt->ps_static_cfg_prms->s_src_prms.i4_orig_height);

                    if(IV_SUCCESS != result)
                        return (IHEVCE_EFAIL);

                    if(3 != ps_ctxt->ps_static_cfg_prms->s_config_prms.i4_rate_control_mode)
                    {
                        /* Dynamic Change in bitrate not supported for multi res/MBR */
                        /*** Check for change in bitrate command ***/
                        if(ps_ctxt->ai4_old_bitrate[0][0] != ps_inp->i4_curr_bitrate)
                        {
                            WORD32 buf_id;
                            WORD32 *pi4_cmd_buf;
                            iv_input_ctrl_buffs_t *ps_ctrl_buf;
                            ihevce_dyn_config_prms_t *ps_dyn_br;
                            WORD32 codec_level_index = ihevce_get_level_index(
                                ps_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[0]
                                    .i4_codec_level);
                            WORD32 max_bitrate =
                                g_as_level_data[codec_level_index].i4_max_bit_rate
                                    [ps_ctxt->ps_static_cfg_prms->s_out_strm_prms.i4_codec_tier] *
                                1000;

                            /* ---------- get a free buffer from command Q ------ */
                            ps_ctrl_buf = (iv_input_ctrl_buffs_t *)ihevce_q_get_free_inp_ctrl_buff(
                                ps_interface_ctxt, &buf_id, BUFF_QUE_BLOCKING_MODE);
                            /* store the buffer id */
                            ps_ctrl_buf->i4_buf_id = buf_id;

                            /* get the buffer pointer */
                            pi4_cmd_buf = (WORD32 *)ps_ctrl_buf->pv_asynch_ctrl_bufs;

                            /* store the set default command, encoder should use create time prms */
                            *pi4_cmd_buf = IHEVCE_ASYNCH_API_SETBITRATE_TAG;
                            *(pi4_cmd_buf + 1) = sizeof(ihevce_dyn_config_prms_t);

                            ps_dyn_br = (ihevce_dyn_config_prms_t *)(pi4_cmd_buf + 2);
                            ps_dyn_br->i4_size = sizeof(ihevce_dyn_config_prms_t);
                            ps_dyn_br->i4_tgt_br_id = 0;
                            ps_dyn_br->i4_tgt_res_id = 0;
                            ps_dyn_br->i4_new_tgt_bitrate =
                                MIN(ps_inp->i4_curr_bitrate, max_bitrate);
                            ps_dyn_br->i4_new_peak_bitrate =
                                MIN((ps_dyn_br->i4_new_tgt_bitrate << 1), max_bitrate);
                            pi4_cmd_buf += 2;
                            pi4_cmd_buf += (sizeof(ihevce_dyn_config_prms_t) >> 2);

                            *(pi4_cmd_buf) = IHEVCE_ASYNCH_API_END_TAG;

                            ps_ctrl_buf->i4_cmd_buf_size = 12 + sizeof(ihevce_dyn_config_prms_t);

                            /* ---------- set the buffer as produced ---------- */
                            ihevce_q_set_inp_ctrl_buff_prod(ps_interface_ctxt, buf_id);

                            ps_ctxt->ai4_old_bitrate[0][0] = ps_inp->i4_curr_bitrate;
                        }
                    }

                    ps_ctxt->u8_num_frames_queued++;
                }
                else
                { /* flush mode command */

                    ps_curr_inp->i4_buf_id = buf_id;

                    /* set the input status to invalid flag */
                    ps_curr_inp->i4_inp_frm_data_valid_flag = 0;

                    pi4_ctrl_ptr = (WORD32 *)ps_curr_inp->pv_synch_ctrl_bufs;

                    *pi4_ctrl_ptr = IHEVCE_SYNCH_API_FLUSH_TAG;
                    *(pi4_ctrl_ptr + 1) = 0;
                    *(pi4_ctrl_ptr + 2) = IHEVCE_SYNCH_API_END_TAG;

                    ps_curr_inp->i4_cmd_buf_size = 4 * 3; /* 4 bytes */
                }

                /* ---------- set the buffer as produced ---------- */
                ihevce_q_set_inp_data_buff_prod(ps_interface_ctxt, buf_id);
                ps_ctxt->u8_num_frames_encoded++;
            }
            else
            {
                /* Enable flush-mode and internal-flush once limit according to
                Eval-version is reached */
                ps_ctxt->i4_flush_mode_on = 1;
                ps_ctxt->i4_internal_flush = 1;
            }
        }

        /* set encoder in flush mode if input buffer is NULL */
        if(0 == ps_ctxt->i4_flush_mode_on)
        {
            if(NULL == ps_inp)
            {
                ps_ctxt->i4_flush_mode_on = 1;
            }
        }

        if((u4_latency < ps_ctxt->u8_num_frames_queued) || (1 == ps_ctxt->i4_flush_mode_on))
        {
            /* --------------------------------------------------------------------- */
            /*            Output Processing                                          */
            /* --------------------------------------------------------------------- */
            ihevce_receive_out_buffer(ps_ctxt, ps_out);
        }
    }
    else  //Other bitrate and resolution instances
    {
        return IHEVCE_EFAIL;
    }
    return (IHEVCE_EOK);
}

