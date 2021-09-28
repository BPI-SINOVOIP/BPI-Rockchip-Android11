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
* \file ihevce_hle_interface.c
*
* \brief
*    This file contains all the functions related High level enocder
*    Interface layer
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* List of Functions
*    <TODO: TO BE ADDED>
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
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
#include "ihevce_buffer_que_interface.h"
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
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_memory_init.h"
#include "ihevce_lap_interface.h"
#include "ihevce_entropy_cod.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_frame_process_utils.h"
#include "ihevce_frame_process.h"
#include "ihevce_profile.h"
#include "ihevce_global_tables.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_common_utils.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_coarse_me_pass.h"
#include "ihevce_me_pass.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_enc_loop_pass.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
/*!
******************************************************************************
* \if Function name : ihevce_context_reset \endif
*
* \brief
*    Encoder reset function
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
void ihevce_context_reset(enc_ctxt_t *ps_enc_ctxt)
{
    ps_enc_ctxt->i4_end_flag = 0;

    /* set the queue related pointer and buffer to default value */
    ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl = NULL;

    /* Reset the i/o queues created status to 0 */
    ps_enc_ctxt->i4_io_queues_created = 0;

    /* reset the frame limit flag to 0 */
    ps_enc_ctxt->i4_frame_limit_reached = 0;

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_hle_interface_create \endif
*
* \brief
*    High level Encoder create function
*
* \param[in]  High level enocder interface context pointer
*
* \return
*    success or fail
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_hle_interface_create(ihevce_hle_ctxt_t *ps_hle_ctxt)
{
    /* local variables */
    enc_ctxt_t *ps_enc_ctxt;
    iv_mem_rec_t s_memtab;
    ihevce_static_cfg_params_t *ps_enc_static_cfg_params;
    WORD32 i4_num_resolutions = ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;
    WORD32 i4_look_ahead_frames_in_first_pass = -1;
    WORD32 i4_total_cores = 0, ctr, i4_mres_flag = 0;
    ihevce_sys_api_t *ps_sys_api = &ps_hle_ctxt->ps_static_cfg_prms->s_sys_api;

    WORD32 status = 0;
    WORD32 i;
    WORD32 *pi4_active_res_id = NULL;

    /* OSAL Init */
    status = ihevce_osal_init((void *)ps_hle_ctxt);

    if(status != 0)
        return (IV_FAIL);

    /* --------------------------------------------------------------------- */
    /*              High Level Encoder Init                                  */
    /* --------------------------------------------------------------------- */

    if(i4_num_resolutions > 1)
        i4_mres_flag = 1;
    /* set no error in the output */
    ps_hle_ctxt->i4_error_code = 0;

    /* Error checks on the static parameters passed */
    ps_hle_ctxt->i4_error_code = ihevce_hle_validate_static_params(ps_hle_ctxt->ps_static_cfg_prms);

    /*memory for static cfg params for encoder, which can be overwritten if encoder wants
        encoder should use this for all its usage*/
    s_memtab.i4_size = sizeof(iv_mem_rec_t);
    s_memtab.i4_mem_alignment = 4;
    s_memtab.i4_mem_size = sizeof(ihevce_static_cfg_params_t);
    s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

    ps_hle_ctxt->ihevce_mem_alloc(
        ps_hle_ctxt->pv_mem_mgr_hdl, &ps_hle_ctxt->ps_static_cfg_prms->s_sys_api, &s_memtab);
    if(s_memtab.pv_base == NULL)
    {
        return (IV_FAIL);
    }
    ps_enc_static_cfg_params = (ihevce_static_cfg_params_t *)s_memtab.pv_base;
    memcpy(
        ps_enc_static_cfg_params,
        ps_hle_ctxt->ps_static_cfg_prms,
        (sizeof(ihevce_static_cfg_params_t)));

    i4_total_cores = ps_enc_static_cfg_params->s_multi_thrd_prms.i4_max_num_cores;

    /* check for validity of memory control flag (only 0,1,2 modes are allowed) */
    if((ps_enc_static_cfg_params->s_multi_thrd_prms.i4_memory_alloc_ctrl_flag > 2) ||
       (ps_enc_static_cfg_params->s_multi_thrd_prms.i4_memory_alloc_ctrl_flag < 0))
    {
        ps_hle_ctxt->i4_error_code = IHEVCE_INVALID_MEM_CTRL_FLAG;
    }

    if((i4_mres_flag == 1) &&
       (ps_enc_static_cfg_params->s_multi_thrd_prms.i4_use_thrd_affinity == 1))
    {
        ps_sys_api->ihevce_printf(
            ps_sys_api->pv_cb_handle,
            "\nIHEVCE WARNING: Enabling thread affinity in multiresolution encoding will affect "
            "performance\n");
    }
    if((ps_enc_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[0].i4_quality_preset ==
        IHEVCE_QUALITY_P6) &&
       (ps_enc_static_cfg_params->s_config_prms.i4_cu_level_rc))
    {
        ps_sys_api->ihevce_printf(
            ps_sys_api->pv_cb_handle,
            "\nIHEVCE WARNING: Disabling CU level QP modulation for P6 preset\n");
        ps_enc_static_cfg_params->s_config_prms.i4_cu_level_rc = 0;
    }
    if((ps_enc_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[0].i4_quality_preset ==
        IHEVCE_QUALITY_P7) &&
       (ps_enc_static_cfg_params->s_config_prms.i4_cu_level_rc))
    {
        ps_sys_api->ihevce_printf(
            ps_sys_api->pv_cb_handle,
            "\nIHEVCE WARNING: Disabling CU level QP modulation for P7 preset\n");
        ps_enc_static_cfg_params->s_config_prms.i4_cu_level_rc = 0;
    }

    if(0 != ps_hle_ctxt->i4_error_code)
    {
        ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
        return (IV_FAIL);
    }
    ps_hle_ctxt->ai4_num_core_per_res[0] = i4_total_cores;

    if(1 == ps_enc_static_cfg_params->s_tgt_lyr_prms.i4_mres_single_out)
    {
        /*    Memory Allocation of pi4_active_res_id */
        s_memtab.i4_size = sizeof(iv_mem_rec_t);
        s_memtab.i4_mem_alignment = 4;
        s_memtab.i4_mem_size = sizeof(WORD32) * (IHEVCE_MAX_NUM_RESOLUTIONS + 1);
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_hle_ctxt->ihevce_mem_alloc(
            ps_hle_ctxt->pv_mem_mgr_hdl, &ps_enc_static_cfg_params->s_sys_api, &s_memtab);
        if(s_memtab.pv_base == NULL)
        {
            return (IV_FAIL);
        }

        pi4_active_res_id = (WORD32 *)s_memtab.pv_base;
    }
    /* --------------------------------------------------------------------- */
    /*    Context and Memory Initialization of Encoder ctxt                  */
    /* --------------------------------------------------------------------- */
    for(ctr = 0; ctr < i4_num_resolutions; ctr++)
    {
        WORD32 i4_br_id;
        s_memtab.i4_size = sizeof(iv_mem_rec_t);
        s_memtab.i4_mem_alignment = 4;
        s_memtab.i4_mem_size = sizeof(enc_ctxt_t);
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_hle_ctxt->ihevce_mem_alloc(
            ps_hle_ctxt->pv_mem_mgr_hdl, &ps_enc_static_cfg_params->s_sys_api, &s_memtab);
        if(s_memtab.pv_base == NULL)
        {
            return (IV_FAIL);
        }

        ps_enc_ctxt = (enc_ctxt_t *)s_memtab.pv_base;

        ps_enc_ctxt->ps_stat_prms = ps_enc_static_cfg_params;

        /* check of number of cores to decide the num threads active */
        ps_enc_ctxt->s_multi_thrd.i4_all_thrds_active_flag = 1;

        if(1 == ps_enc_static_cfg_params->s_tgt_lyr_prms.i4_mres_single_out)
        {
            pi4_active_res_id[ctr] = 0;
            ps_enc_ctxt->s_multi_thrd.pi4_active_res_id = pi4_active_res_id;
        }

        /*store num bit-rate instances in the encoder context */
        ps_enc_ctxt->i4_num_bitrates =
            ps_enc_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[ctr].i4_num_bitrate_instances;
        if(BLU_RAY_SUPPORT == ps_enc_static_cfg_params->s_out_strm_prms.i4_interop_flags)
        {
            ps_enc_ctxt->i4_blu_ray_spec = 1;
        }
        else
        {
            ps_enc_ctxt->i4_blu_ray_spec = 0;
        }

        /* if all threads are required to be active */
        if(1 == ps_enc_ctxt->s_multi_thrd.i4_all_thrds_active_flag)
        {
            /* store the number of threads to be created as passed by app with HT flag */
            ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds =
                ps_hle_ctxt->ai4_num_core_per_res[ctr];

            /* pre enc threads are doubled if HT is ON */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds =
                ps_hle_ctxt->ai4_num_core_per_res[ctr];
        }
        else
        {
            // TODO: distribute threads across stages
        }

        /*Keep track of resolution id, this is used to differentiate from other encoder instance*/
        ps_enc_ctxt->i4_resolution_id = ctr;
        /* store hle ctxt in enc ctxt */
        ps_enc_ctxt->pv_hle_ctxt = (void *)ps_hle_ctxt;
        ps_enc_ctxt->pv_rc_mutex_lock_hdl = NULL;
        ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_mutex_lock_hdl = NULL;
        ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_for_qp_update_mutex_lock_hdl = NULL;
        ps_enc_ctxt->i4_look_ahead_frames_in_first_pass = i4_look_ahead_frames_in_first_pass;

        ps_enc_ctxt->ai4_is_past_pic_complex[0] = 0;
        ps_enc_ctxt->ai4_is_past_pic_complex[1] = 0;
        ps_enc_ctxt->i4_is_I_reset_done = 1;
        ps_enc_ctxt->i4_past_RC_reset_count = 0;
        ps_enc_ctxt->i4_future_RC_reset = 0;
        ps_enc_ctxt->i4_past_RC_scd_reset_count = 0;
        ps_enc_ctxt->i4_future_RC_scd_reset = 0;
        ps_enc_ctxt->i4_active_scene_num = -1;
        for(i = 0; i < IHEVCE_MAX_NUM_BITRATES; i++)
        {
            ps_enc_ctxt->ai4_rc_query[i] = 0;
        }
        ps_enc_ctxt->i4_active_enc_frame_id = 0;
        ps_enc_ctxt->u1_is_popcnt_available = 1;

#ifndef ARM
        ps_enc_ctxt->e_arch_type = ARCH_X86_GENERIC;
        ps_enc_ctxt->u1_is_popcnt_available = 0;
#else
        if(ps_enc_static_cfg_params->e_arch_type == ARCH_NA)
            ps_enc_ctxt->e_arch_type = ihevce_default_arch();
        else
            ps_enc_ctxt->e_arch_type = ps_enc_static_cfg_params->e_arch_type;
        ps_enc_ctxt->u1_is_popcnt_available = 0;
#endif

        {
            ps_enc_static_cfg_params->e_arch_type = ps_enc_ctxt->e_arch_type;

            ihevce_init_function_ptr(ps_enc_ctxt, ps_enc_ctxt->e_arch_type);
        }

        ihevce_mem_manager_init(ps_enc_ctxt, ps_hle_ctxt);

        if(0 != ps_hle_ctxt->i4_error_code)
        {
            return (IV_FAIL);
        }

        /* mutex lock for RC calls */
        ps_enc_ctxt->pv_rc_mutex_lock_hdl = osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
        if(NULL == ps_enc_ctxt->pv_rc_mutex_lock_hdl)
        {
            return IV_FAIL;
        }

        /* mutex lock for Sub pic RC calls */
        ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_mutex_lock_hdl =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
        if(NULL == ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_mutex_lock_hdl)
        {
            return IV_FAIL;
        }

        ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_for_qp_update_mutex_lock_hdl =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
        if(NULL == ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_for_qp_update_mutex_lock_hdl)
        {
            return IV_FAIL;
        }

        /* reset the encoder context */
        ihevce_context_reset(ps_enc_ctxt);

        /* register the Encoder context in HLE interface ctxt */
        ps_hle_ctxt->apv_enc_hdl[ctr] = ps_enc_ctxt;
    }
    /* init profile */
    PROFILE_INIT(&ps_hle_ctxt->profile_hle);
    for(ctr = 0; ctr < i4_num_resolutions; ctr++)
    {
        WORD32 i4_br_id;

        PROFILE_INIT(&ps_hle_ctxt->profile_enc_me[ctr]);
        PROFILE_INIT(&ps_hle_ctxt->profile_pre_enc_l1l2[ctr]);
        PROFILE_INIT(&ps_hle_ctxt->profile_pre_enc_l0ipe[ctr]);
        for(i4_br_id = 0; i4_br_id < ps_enc_ctxt->i4_num_bitrates; i4_br_id++)
        {
            PROFILE_INIT(&ps_hle_ctxt->profile_enc[ctr][i4_br_id]);
            PROFILE_INIT(&ps_hle_ctxt->profile_entropy[ctr][i4_br_id]);
        }
    }
    if(1 == ps_enc_static_cfg_params->s_tgt_lyr_prms.i4_mres_single_out)
        pi4_active_res_id[i4_num_resolutions] = 0;

    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_query_io_buf_req \endif
*
* \brief
*    High level Encoder IO buffers query function
*
* \param[in] High level encoder interface context pointer
* \param[out] Input buffer requirment stucture pointer.
* \param[out] Output buffer requirment stucture pointer.
*
* \return
*    success or fail
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_query_io_buf_req(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    iv_input_bufs_req_t *ps_input_bufs_req,
    iv_res_layer_output_bufs_req_t *ps_res_layer_output_bufs_req,
    iv_res_layer_recon_bufs_req_t *ps_res_layer_recon_bufs_req)
{
    /* local variables */
    enc_ctxt_t *ps_enc_ctxt;
    ihevce_src_params_t *ps_src_prms;
    WORD32 ctb_align_pic_wd;
    WORD32 ctb_align_pic_ht, i4_resolution_id = 0, i4_num_resolutions, i4_num_bitrate_instances;
    WORD32 i4_resolution_id_ctr, br_ctr;

    ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[i4_resolution_id];
    ps_src_prms = &ps_hle_ctxt->ps_static_cfg_prms->s_src_prms;
    i4_num_resolutions = ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;
    /* set no error in the output */
    ps_hle_ctxt->i4_error_code = 0;

    /* ------- populate the Input buffer requirements -------- */
    /* get the number of buffers required for LAP */
    ps_input_bufs_req->i4_min_num_yuv_bufs =
        ihevce_lap_get_num_ip_bufs(&ps_enc_ctxt->s_lap_stat_prms);

    ps_input_bufs_req->i4_min_num_synch_ctrl_bufs = ps_input_bufs_req->i4_min_num_yuv_bufs;

    ps_input_bufs_req->i4_min_num_asynch_ctrl_bufs = NUM_AYSNC_CMD_BUFS;

    /* buffer sizes are populated based on create time parameters */
    ctb_align_pic_wd =
        ps_src_prms->i4_width +
        SET_CTB_ALIGN(ps_src_prms->i4_width, ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

    ctb_align_pic_ht =
        ps_src_prms->i4_height +
        SET_CTB_ALIGN(ps_src_prms->i4_height, ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

    if(ps_src_prms->i4_input_bit_depth > 8)
    {
        ps_input_bufs_req->i4_min_size_y_buf = ctb_align_pic_wd * ctb_align_pic_ht * 2;

        ps_input_bufs_req->i4_min_size_uv_buf = ps_input_bufs_req->i4_min_size_y_buf >> 1;
    }
    else
    {
        ps_input_bufs_req->i4_min_size_y_buf = ctb_align_pic_wd * ctb_align_pic_ht;

        ps_input_bufs_req->i4_min_size_uv_buf = (ctb_align_pic_wd * ctb_align_pic_ht) >> 1;
    }

    ps_input_bufs_req->i4_min_size_uv_buf <<=
        ((ps_src_prms->i4_chr_format == IV_YUV_422SP_UV) ? 1 : 0);

    ps_input_bufs_req->i4_yuv_format = ps_src_prms->i4_chr_format;

    ps_input_bufs_req->i4_min_size_synch_ctrl_bufs =
        ((MAX_SEI_PAYLOAD_PER_TLV + 16) * MAX_NUMBER_OF_SEI_PAYLOAD) + 16;

    ps_input_bufs_req->i4_min_size_asynch_ctrl_bufs =
        ((MAX_SEI_PAYLOAD_PER_TLV + 16) * (MAX_NUMBER_OF_SEI_PAYLOAD - 6)) + 16;

    for(i4_resolution_id_ctr = 0; i4_resolution_id_ctr < i4_num_resolutions; i4_resolution_id_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[i4_resolution_id_ctr];

        i4_num_bitrate_instances = ps_enc_ctxt->s_runtime_tgt_params.i4_num_bitrate_instances;

        /* buffer sizes are populated based on create time parameters */
        ctb_align_pic_wd = ps_enc_ctxt->s_runtime_tgt_params.i4_width +
                           SET_CTB_ALIGN(
                               ps_enc_ctxt->s_runtime_tgt_params.i4_width,
                               ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

        ctb_align_pic_ht = ps_enc_ctxt->s_runtime_tgt_params.i4_height +
                           SET_CTB_ALIGN(
                               ps_enc_ctxt->s_runtime_tgt_params.i4_height,
                               ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

        for(br_ctr = 0; br_ctr < i4_num_bitrate_instances; br_ctr++)
        {
            /* ------- populate the Output buffer requirements -------- */
            ps_res_layer_output_bufs_req->s_output_buf_req[i4_resolution_id_ctr][br_ctr]
                .i4_min_num_out_bufs = NUM_OUTPUT_BUFS;

            ps_res_layer_output_bufs_req->s_output_buf_req[i4_resolution_id_ctr][br_ctr]
                .i4_min_size_bitstream_buf = (ctb_align_pic_wd * ctb_align_pic_ht);

            if((ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth == 12) ||
               ((ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) &&
                (ps_src_prms->i4_chr_format == IV_YUV_422SP_UV)))
            {
                ps_res_layer_output_bufs_req->s_output_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_bitstream_buf *= 2;
            }

            if((ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth == 10) &&
               (ps_src_prms->i4_chr_format == IV_YUV_420SP_UV))
            {
                ps_res_layer_output_bufs_req->s_output_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_bitstream_buf *= 3;
                ps_res_layer_output_bufs_req->s_output_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_bitstream_buf >>= 1;
            }

            //recon_dump
            /* ------- populate the Recon buffer requirements -------- */
            if(ps_enc_ctxt->ps_stat_prms->i4_save_recon == 0)
            {
                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_num_recon_bufs = 0;

                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_y_buf = 0;

                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_uv_buf = 0;
            }
            else
            {
                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_num_recon_bufs = 2 * HEVCE_MAX_REF_PICS + 1;

                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_y_buf =
                    ctb_align_pic_wd * ctb_align_pic_ht *
                    ((ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8)
                         ? 2
                         : 1);

                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_uv_buf =
                    (ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                         .i4_min_size_y_buf >>
                     1);
                ps_res_layer_recon_bufs_req->s_recon_buf_req[i4_resolution_id_ctr][br_ctr]
                    .i4_min_size_uv_buf <<=
                    ((ps_src_prms->i4_chr_format == IV_YUV_422SP_UV) ? 1 : 0);
            }
        }
    }

    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_create_ports \endif
*
* \brief
*    High level Encoder IO ports Create function
*
* \param[in] High level encoder interface context pointer
* \param[in] Input data buffer descriptor
* \param[in] Input control buffer descriptor
* \param[in] Output data buffer descriptor
* \param[in] Output control status buffer descriptor
* \param[out] Pointer to store the ID for Input data Que
* \param[out] Pointer to store the ID for Input control Que
* \param[out] Pointer to store the ID for Output data Que
* \param[out] Pointer to store the ID for Output control status Que
*
* \return
*  success or fail
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_create_ports(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    iv_input_data_ctrl_buffs_desc_t *ps_input_data_ctrl_buffs_desc,
    iv_input_asynch_ctrl_buffs_desc_t *ps_input_asynch_ctrl_buffs_desc,
    iv_res_layer_output_data_buffs_desc_t *ps_mres_output_data_buffs_desc,
    iv_res_layer_recon_data_buffs_desc_t *ps_mres_recon_data_buffs_desc)
{
    /* local varaibles */
    enc_ctxt_t *ps_enc_ctxt;
    WORD32 res_ctr,
        i4_num_resolutions = ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;
    void *pv_q_mutex_hdl = NULL;

    /* set no error in the output */
    ps_hle_ctxt->i4_error_code = 0;

    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];
        /* check on buffer sizes provided by applciation needs to be checked */

        /* call the memory manager que init function , pass the op data , status, recon for the first bitrate, internally we will increment*/
        ihevce_mem_manager_que_init(
            ps_enc_ctxt,
            ps_hle_ctxt,
            ps_input_data_ctrl_buffs_desc,
            ps_input_asynch_ctrl_buffs_desc,
            &ps_mres_output_data_buffs_desc->s_output_data_buffs[res_ctr][0],
            &ps_mres_recon_data_buffs_desc->s_recon_data_buffs[res_ctr][0]);

        /* set the number of Queues */
        ps_enc_ctxt->s_enc_ques.i4_num_queues = IHEVCE_MAX_NUM_QUEUES;

        /* allocate a mutex to take care of handling multiple threads accesing Queues */
        /*my understanding, this is common semaphore for all the queue. Since main input is still
        common across all instance fo encoder. Hence common semaphore is a must*/
        if(0 == res_ctr)
        {
            ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl = osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
            /* store it in local variable for allocating it to other instances */
            pv_q_mutex_hdl = ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl;
            if(NULL == pv_q_mutex_hdl)
            {
                return IV_FAIL;
            }
        }
        else
        {
            ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl = pv_q_mutex_hdl;
        }

        /* Set the i/o queues created status to 1 */
        ps_enc_ctxt->i4_io_queues_created = 1;
    }
    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_hle_interface_thrd \endif
*
* \brief
*    High level encoder thread interface function
*
* \param[in] High level interface context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_hle_interface_thrd(void *pv_proc_intf_ctxt)
{
    /* local variables */
    WORD32 ctr, res_ctr;
    ihevce_hle_ctxt_t *ps_hle_ctxt;
    enc_ctxt_t *ps_enc_ctxt;
    /* enc ctxt to store 0th instance's params which are required by all instances */
    enc_ctxt_t *ps_enc_ctxt_base;
    void *pv_lap_sem_hdl;
    void *pv_enc_frame_process_sem_hdl;
    void *pv_pre_enc_frame_process_sem_hdl;
    void *apv_ent_coding_sem_hdl[IHEVCE_MAX_NUM_BITRATES];
    void *pv_ent_common_mres_sem_hdl = NULL;
    void *pv_out_common_mres_sem_hdl = NULL;

    void *pv_inp_data_sem_hdl;
    void *pv_lap_inp_data_sem_hdl;
    void *pv_preenc_inp_data_sem_hdl;
    void *pv_inp_ctrl_sem_hdl;
    void *apv_out_stream_sem_hdl[IHEVCE_MAX_NUM_BITRATES];
    void *apv_out_recon_sem_hdl[IHEVCE_MAX_NUM_BITRATES];
    void *pv_out_ctrl_sts_sem_hdl;

    lap_intface_t *ps_lap_interface_ctxt;
    iv_mem_rec_t s_memtab;
    WORD32 i4_num_bit_rate_instances[IHEVCE_MAX_NUM_RESOLUTIONS], i4_num_resolutions;
    WORD32 i;  //loop variable
    WORD32 ai4_proc_count[MAX_NUMBER_PROC_GRPS] = { 0 }, i4_proc_grp_count;
    WORD32 i4_acc_proc_num = 0;

    /* Frame Encode processing threads & semaphores */
    void *apv_enc_frm_proc_hdls[IHEVCE_MAX_NUM_RESOLUTIONS][MAX_NUM_FRM_PROC_THRDS_ENC];
    frm_proc_thrd_ctxt_t
        *aps_enc_frm_proc_thrd_ctxt[IHEVCE_MAX_NUM_RESOLUTIONS][MAX_NUM_FRM_PROC_THRDS_ENC];

    /* Pre Frame Encode processing threads & semaphores */
    void *apv_pre_enc_frm_proc_hdls[IHEVCE_MAX_NUM_RESOLUTIONS][MAX_NUM_FRM_PROC_THRDS_PRE_ENC];
    frm_proc_thrd_ctxt_t
        *aps_pre_enc_frm_proc_thrd_ctxt[IHEVCE_MAX_NUM_RESOLUTIONS][MAX_NUM_FRM_PROC_THRDS_PRE_ENC];

    void *apv_entropy_thrd_hdls[IHEVCE_MAX_NUM_RESOLUTIONS][NUM_ENTROPY_THREADS];
    frm_proc_thrd_ctxt_t *aps_entropy_thrd_ctxt[IHEVCE_MAX_NUM_RESOLUTIONS][NUM_ENTROPY_THREADS];

    ps_hle_ctxt = (ihevce_hle_ctxt_t *)pv_proc_intf_ctxt;
    ps_enc_ctxt_base = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[0];
    /* profile start */
    PROFILE_START(&ps_hle_ctxt->profile_hle);
    /* store default values of mem tab */
    s_memtab.i4_size = sizeof(iv_mem_rec_t);
    s_memtab.i4_mem_alignment = 4;

    i4_num_resolutions = ps_enc_ctxt_base->ps_stat_prms->s_tgt_lyr_prms.i4_num_res_layers;
    memset(
        apv_entropy_thrd_hdls,
        0,
        IHEVCE_MAX_NUM_RESOLUTIONS * NUM_ENTROPY_THREADS * sizeof(void *));
    memset(
        apv_entropy_thrd_hdls,
        0,
        IHEVCE_MAX_NUM_RESOLUTIONS * NUM_ENTROPY_THREADS * sizeof(void *));
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        i4_num_bit_rate_instances[res_ctr] =
            ps_enc_ctxt_base->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[res_ctr]
                .i4_num_bitrate_instances;
    }
    /* --------------------------------------------------------------------- */
    /*        Init number of threads for each stage                          */
    /* --------------------------------------------------------------------- */

    {
        for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
        {
            ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];
            /* all the threads created will be made active */
            ps_enc_ctxt->s_multi_thrd.i4_num_active_enc_thrds =
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds;

            ps_enc_ctxt->s_multi_thrd.i4_num_active_pre_enc_thrds =
                ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds;
        }
    }

    /* --------------------------------------------------------------------- */
    /*            Multiple processing Threads Semaphores init                */
    /* --------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        osal_sem_attr_t attr = OSAL_DEFAULT_SEM_ATTR;

        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        attr.value = SEM_START_VALUE;

        /* Create Semaphore handle for LAP thread   */
        if(0 == ps_enc_ctxt->i4_resolution_id)
        {
            pv_lap_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
            if(NULL == pv_lap_sem_hdl)
            {
                return IV_FAIL;
            }
        }
        else
        {
            /*NOTE: Tile workspace assigned this to null. Confirm this*/
            pv_lap_sem_hdl = ps_enc_ctxt_base->s_thrd_sem_ctxt.pv_lap_sem_handle;
        }
        /* Create Semaphore for encode frame process thread */
        pv_enc_frame_process_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
        if(NULL == pv_enc_frame_process_sem_hdl)
        {
            return IV_FAIL;
        }

        /* Create Semaphore for pre_encode frame process thread */
        pv_pre_enc_frame_process_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
        if(NULL == pv_pre_enc_frame_process_sem_hdl)
        {
            return IV_FAIL;
        }

        /* Create Semaphore for input frame data q function */
        if(0 == ps_enc_ctxt->i4_resolution_id)
        {
            pv_inp_data_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
            if(NULL == pv_inp_data_sem_hdl)
            {
                return IV_FAIL;
            }
        }
        else
        {
            pv_inp_data_sem_hdl = ps_enc_ctxt_base->s_thrd_sem_ctxt.pv_inp_data_sem_handle;
        }

        /*creating new input queue owned by encoder*/
        /* Create Semaphore for input frame data q function */
        pv_lap_inp_data_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
        if(NULL == pv_lap_inp_data_sem_hdl)
        {
            return IV_FAIL;
        }

        /* Create Semaphore for input frame data q function */
        pv_preenc_inp_data_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
        if(NULL == pv_preenc_inp_data_sem_hdl)
        {
            return IV_FAIL;
        }

        /* Create Semaphore for input conrol data q function */
        if(0 == ps_enc_ctxt->i4_resolution_id)
        {
            pv_inp_ctrl_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
            if(NULL == pv_inp_ctrl_sem_hdl)
            {
                return IV_FAIL;
            }
        }
        else
        { /*Inp ctrl queue is same for all resolutions between app and lap*/
            pv_inp_ctrl_sem_hdl = ps_enc_ctxt_base->s_thrd_sem_ctxt.pv_inp_ctrl_sem_handle;
        }

        /* Create Semaphore for output control status data q function */
        pv_out_ctrl_sts_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
        if(NULL == pv_out_ctrl_sts_sem_hdl)
        {
            return IV_FAIL;
        }

        /* Multi res single output case singel output queue is used for all output resolutions */
        if(1 == ps_enc_ctxt_base->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out)
        {
            ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_OUTPUT_DATA_Q] =
                ps_enc_ctxt_base->s_enc_ques.apv_q_hdl[IHEVCE_OUTPUT_DATA_Q];
            if(0 == ps_enc_ctxt->i4_resolution_id)
            {
                /* Create Semaphore for enropy coding thread   */
                pv_ent_common_mres_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
                if(NULL == pv_ent_common_mres_sem_hdl)
                {
                    return IV_FAIL;
                }

                /* Create Semaphore for output stream data q function */
                pv_out_common_mres_sem_hdl = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
                if(NULL == pv_out_common_mres_sem_hdl)
                {
                    return IV_FAIL;
                }
            }
            ps_enc_ctxt->s_thrd_sem_ctxt.pv_ent_common_mres_sem_hdl = pv_ent_common_mres_sem_hdl;
            ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_common_mres_sem_hdl = pv_out_common_mres_sem_hdl;
        }

        /*create entropy and output semaphores for each thread.
        Each thread will correspond to each bit-rate instance running */
        for(i = 0; i < i4_num_bit_rate_instances[res_ctr]; i++)
        {
            /* Create Semaphore for enropy coding thread   */
            apv_ent_coding_sem_hdl[i] = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
            if(NULL == apv_ent_coding_sem_hdl[i])
            {
                return IV_FAIL;
            }

            /* Create Semaphore for output stream data q function */
            apv_out_stream_sem_hdl[i] = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
            if(NULL == apv_out_stream_sem_hdl[i])
            {
                return IV_FAIL;
            }

            /* Create Semaphore for output recon data q function */
            apv_out_recon_sem_hdl[i] = osal_sem_create(ps_hle_ctxt->pv_osal_handle, &attr);
            if(NULL == apv_out_recon_sem_hdl[i])
            {
                return IV_FAIL;
            }
        }

        /* update the semaphore handles and the thread creates status */

        ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle = pv_enc_frame_process_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle =
            pv_pre_enc_frame_process_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle = pv_lap_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_data_sem_handle = pv_inp_data_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_inp_data_sem_hdl = pv_lap_inp_data_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_preenc_inp_data_sem_hdl = pv_preenc_inp_data_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_ctrl_sem_handle = pv_inp_ctrl_sem_hdl;
        ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_ctrl_sem_handle = pv_out_ctrl_sts_sem_hdl;
        for(i = 0; i < i4_num_bit_rate_instances[res_ctr]; i++)
        {
            ps_enc_ctxt->s_thrd_sem_ctxt.apv_ent_cod_sem_handle[i] = apv_ent_coding_sem_hdl[i];
            ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_strm_sem_handle[i] = apv_out_stream_sem_hdl[i];
            ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_recon_sem_handle[i] = apv_out_recon_sem_hdl[i];
        }
    }

    /* --------------------------------------------------------------------- */
    /*            Multiple processing Threads Mutex init                     */
    /* --------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        /* create a mutex lock for Job Queue access accross slave threads of encode frame processing */
        ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_enc_grp_me =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
        if(NULL == ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_enc_grp_me)
        {
            return IV_FAIL;
        }

        /* create a mutex lock for Job Queue access accross slave threads of encode frame processing */
        ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_enc_grp_enc_loop =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
        if(NULL == ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_enc_grp_enc_loop)
        {
            return IV_FAIL;
        }

        /* create mutex for enc thread group */
        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i] =
                osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
            if(NULL == ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i])
            {
                return IV_FAIL;
            }

            ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_me_end[i] =
                osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
            if(NULL == ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_me_end[i])
            {
                return IV_FAIL;
            }
        }

        for(i = 0; i < MAX_NUM_ENC_LOOP_PARALLEL; i++)
        {
            ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i] =
                osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
            if(NULL == ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i])
            {
                return IV_FAIL;
            }

            ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_frame_init[i] =
                osal_mutex_create(ps_hle_ctxt->pv_osal_handle);
            if(NULL == ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_frame_init[i])
            {
                return IV_FAIL;
            }
        }

        /*initialize mutex for pre-enc group */
        ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_init =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_decomp_deinit =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_init =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_deinit =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_deinit =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_l0_ipe_init =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_decomp =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_hme =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_l0ipe =
            osal_mutex_create(ps_hle_ctxt->pv_osal_handle);

        if(NULL == ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_init ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_decomp_deinit ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_init ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_deinit ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_deinit ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_l0_ipe_init ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_decomp ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_hme ||
           NULL == ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_l0ipe)
        {
            return IV_FAIL;
        }
    }

    /* --------------------------------------------------------------------- */
    /*            Multiple processing Threads Context init                   */
    /* --------------------------------------------------------------------- */

    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];
        ps_enc_ctxt_base = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[0];

        /*initialize multi-thread context for enc group*/
        ps_enc_ctxt->s_multi_thrd.i4_is_recon_free_done = 0;
        ps_enc_ctxt->s_multi_thrd.i4_idx_dvsr_p = 0;
        ps_enc_ctxt->s_multi_thrd.i4_last_inp_buf = 0;

        {
            /* For all the ME frames in Parallel */
            WORD32 i4_frm_idx;

            for(i4_frm_idx = 0; i4_frm_idx < MAX_NUM_ME_PARALLEL; i4_frm_idx++)
            {
                ps_enc_ctxt->s_multi_thrd.me_num_thrds_exited[i4_frm_idx] = 0;
                ps_enc_ctxt->s_multi_thrd.ai4_me_master_done_flag[i4_frm_idx] = 0;
                ps_enc_ctxt->s_multi_thrd.ai4_me_enc_buff_prod_flag[i4_frm_idx] = 0;
            }
        }

        {
            WORD32 i4_frm_idx;
            ps_enc_ctxt->s_multi_thrd.num_thrds_done = 0;
            ps_enc_ctxt->s_multi_thrd.num_thrds_exited_for_reenc = 0;
            for(i4_frm_idx = 0; i4_frm_idx < MAX_NUM_ENC_LOOP_PARALLEL; i4_frm_idx++)
            {
                ps_enc_ctxt->s_multi_thrd.num_thrds_exited[i4_frm_idx] = 0;

                ps_enc_ctxt->s_multi_thrd.enc_master_done_frame_init[i4_frm_idx] = 0;

                for(i = 0; i < i4_num_bit_rate_instances[res_ctr]; i++)
                {
                    /*reset the entropy buffer produced status */
                    ps_enc_ctxt->s_multi_thrd.ai4_produce_outbuf[i4_frm_idx][i] = 1;
                    ps_enc_ctxt->s_multi_thrd.ps_frm_recon[i4_frm_idx][i] = NULL;

                    ps_enc_ctxt->s_multi_thrd.ps_curr_out_enc_grp[i4_frm_idx][i] = NULL;
                }
            }
        }
        ps_enc_ctxt->s_multi_thrd.i4_seq_mode_enabled_flag = 0;

        /* Set prev_frame_done = 1 to indicate that all the threads are in same frame*/
        for(i = 0; i < ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel; i++)
        {
            ihevce_dmgr_set_done_frm_frm_sync(
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_done[i]);
        }
        /* Set prev_frame_done = 1 to indicate that all the threads are in same frame*/
        ihevce_dmgr_set_done_frm_frm_sync(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_enc_done_for_reenc);
        /*to enable the dependency manager to wait when first reached*/
        ihevce_dmgr_set_prev_done_frm_frm_sync(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_enc_done_for_reenc);
        for(i = 0; i < ps_enc_ctxt->s_multi_thrd.i4_num_me_frm_pllel; i++)
        {
            ihevce_dmgr_set_done_frm_frm_sync(
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_me_done[i]);
        }

        /* initialize multi-thread context for pre enc group */

        ps_enc_ctxt->s_multi_thrd.i4_ctrl_blocking_mode = BUFF_QUE_BLOCKING_MODE;

        for(ctr = 0; ctr < MAX_PRE_ENC_STAGGER + NUM_BUFS_DECOMP_HME; ctr++)
        {
            ps_enc_ctxt->s_multi_thrd.ai4_pre_enc_init_done[ctr] = 0;
            ps_enc_ctxt->s_multi_thrd.ai4_pre_enc_hme_init_done[ctr] = 0;
            ps_enc_ctxt->s_multi_thrd.ai4_pre_enc_deinit_done[ctr] = 1;
            ps_enc_ctxt->s_multi_thrd.ai4_num_thrds_processed_decomp[ctr] = 0;
            ps_enc_ctxt->s_multi_thrd.ai4_num_thrds_processed_coarse_me[ctr] = 0;
            ps_enc_ctxt->s_multi_thrd.ai4_num_thrds_processed_pre_enc[ctr] = 0;

            ps_enc_ctxt->s_multi_thrd.ai4_num_thrds_processed_L0_ipe_qp_init[ctr] = 0;
            ps_enc_ctxt->s_multi_thrd.ai4_decomp_coarse_me_complete_flag[ctr] = 1;
            ps_enc_ctxt->s_multi_thrd.ai4_end_flag_pre_enc[ctr] = 0;
        }

        /* Set prev_frame_done = 1 to indicate that all the threads are in same frame*/
        ihevce_dmgr_set_done_frm_frm_sync(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l1);

        ihevce_dmgr_set_done_frm_frm_sync(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_coarse_me);

        ihevce_dmgr_set_done_frm_frm_sync(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l0);

        {
            /**init idx for handling delay between pre-me and l0-ipe*/
            ps_enc_ctxt->s_multi_thrd.i4_delay_pre_me_btw_l0_ipe = 0;
            ps_enc_ctxt->s_multi_thrd.i4_max_delay_pre_me_btw_l0_ipe =
                MIN_L1_L0_STAGGER_NON_SEQ +
                ps_enc_ctxt->s_lap_stat_prms.s_lap_params.i4_rc_look_ahead_pics + 1;
            if(ps_enc_ctxt->s_lap_stat_prms.s_lap_params.i4_rc_look_ahead_pics)
            {
                ps_enc_ctxt->s_multi_thrd.i4_delay_pre_me_btw_l0_ipe =
                    MIN_L1_L0_STAGGER_NON_SEQ +
                    ps_enc_ctxt->s_lap_stat_prms.s_lap_params.i4_rc_look_ahead_pics;
            }
            ps_enc_ctxt->s_multi_thrd.i4_qp_update_l0_ipe = -1;
        }
    }

    /** Get Number of Processor Groups **/
    i4_proc_grp_count = ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups;
    /*** Enc threads are allocated based on the assumption that there can be only 2 processor groups **/
    ASSERT(i4_proc_grp_count <= MAX_NUMBER_PROC_GRPS);
    /** Get Number of logical processors in Each Group **/
    for(ctr = 0; ctr < i4_proc_grp_count; ctr++)
    {
        ai4_proc_count[ctr] =
            ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.ai4_num_cores_per_grp[ctr];
    }

    /* --------------------------------------------------------------------- */
    /*            Create a LAP thread                                        */
    /* --------------------------------------------------------------------- */
    /* LAP thread will run on  0th resolution instance context */
    {
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
        s_memtab.i4_mem_size = sizeof(lap_intface_t);

        /* initialise the interface strucure parameters */
        ps_hle_ctxt->ihevce_mem_alloc(
            ps_hle_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt_base->ps_stat_prms->s_sys_api, &s_memtab);
        if(s_memtab.pv_base == NULL)
        {
            return (IV_FAIL);
        }

        ps_lap_interface_ctxt = (lap_intface_t *)s_memtab.pv_base;

        /* populate the params */
        ps_lap_interface_ctxt->pv_hle_ctxt = ps_hle_ctxt;
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[0];
        ps_lap_interface_ctxt->pv_lap_module_ctxt = ps_enc_ctxt->s_module_ctxt.pv_lap_ctxt;
        ps_lap_interface_ctxt->i4_ctrl_in_que_id = IHEVCE_INPUT_ASYNCH_CTRL_Q;
        ps_lap_interface_ctxt->i4_ctrl_out_que_id = IHEVCE_OUTPUT_STATUS_Q;
        ps_lap_interface_ctxt->i4_ctrl_cmd_buf_size = ENC_COMMAND_BUFF_SIZE;
        ps_lap_interface_ctxt->i4_ctrl_in_que_blocking_mode = BUFF_QUE_BLOCKING_MODE;
        ps_lap_interface_ctxt->ps_sys_api = &ps_enc_ctxt_base->ps_stat_prms->s_sys_api;
        ps_enc_ctxt_base->pv_lap_interface_ctxt = (void *)ps_lap_interface_ctxt;
        ps_lap_interface_ctxt->ihevce_dyn_bitrate_cb = ihevce_dyn_bitrate;
    }

    /* --------------------------------------------------------------------- */
    /*          Create Entropy Coding threads                             */
    /* --------------------------------------------------------------------- */
    /*Create entropy thread for each encoder instance*/
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        osal_thread_attr_t s_thread_attr = OSAL_DEFAULT_THREAD_ATTR;
        WORD32 i4_num_entropy_threads;

        /* derive encoder ctxt from hle handle */
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        i4_num_entropy_threads =
            ps_enc_ctxt_base->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[res_ctr]
                .i4_num_bitrate_instances;

        /* initialise the interface strucure parameters */
        for(ctr = 0; ctr < i4_num_entropy_threads; ctr++)
        {
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.i4_mem_size = sizeof(frm_proc_thrd_ctxt_t);

            ps_hle_ctxt->ihevce_mem_alloc(
                ps_hle_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt_base->ps_stat_prms->s_sys_api, &s_memtab);
            if(s_memtab.pv_base == NULL)
            {
                return (IV_FAIL);
            }

            aps_entropy_thrd_ctxt[res_ctr][ctr] = (frm_proc_thrd_ctxt_t *)s_memtab.pv_base;

            /* initialise the interface strucure parameters */
            aps_entropy_thrd_ctxt[res_ctr][ctr]->i4_thrd_id = ctr;
            aps_entropy_thrd_ctxt[res_ctr][ctr]->ps_hle_ctxt = ps_hle_ctxt;
            aps_entropy_thrd_ctxt[res_ctr][ctr]->pv_enc_ctxt = (void *)ps_enc_ctxt;

            /* Initialize application thread attributes */
            s_thread_attr.exit_code = 0;
            s_thread_attr.name = 0;
            s_thread_attr.priority_map_flag = 1;
            s_thread_attr.priority = OSAL_PRIORITY_DEFAULT;
            s_thread_attr.stack_addr = 0;
            s_thread_attr.stack_size = THREAD_STACK_SIZE;
            s_thread_attr.thread_func = ihevce_ent_coding_thrd;
            s_thread_attr.thread_param =
                (void *)(aps_entropy_thrd_ctxt[res_ctr]
                                              [ctr]);  //encioder and hle context are derived from this
            s_thread_attr.core_affinity_mask = 0;
            if(ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1)
            {
                /* Run ENTROPY thread on last group if there are more than one processor group */
                s_thread_attr.group_num =
                    ps_hle_ctxt->ps_static_cfg_prms->s_multi_thrd_prms.i4_num_proc_groups - 1;
            }
            else
            {
                s_thread_attr.group_num = 0;
            }

            /* Create entropy coding thread */
            apv_entropy_thrd_hdls[res_ctr][ctr] =
                osal_thread_create(ps_hle_ctxt->pv_osal_handle, &s_thread_attr);
            if(NULL == apv_entropy_thrd_hdls[res_ctr][ctr])
            {
                return IV_FAIL;
            }
        }
    }

    /* --------------------------------------------------------------------- */
    /*     Create all Slave Encode Frame processing threads                  */
    /* - -------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        WORD32 enc_ctr = 0;
        WORD32 i4_loop_count;
        WORD32 i4_curr_grp_num = 0;
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        i4_acc_proc_num = 0;
        /* Calculate the start core number of enc threads for current resolution */
        for(i4_loop_count = 0; i4_loop_count < res_ctr; i4_loop_count++)
        {
            /* Add number of cores taken by each resolution till the curr resolution */
            enc_ctr += ps_hle_ctxt->ai4_num_core_per_res[i4_loop_count];
        }
        if(ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1)
        {
            /* Select the group number for each res based on processors present in each group */
            for(i4_loop_count = 0;
                i4_loop_count <
                ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups;
                i4_loop_count++)
            {
                i4_acc_proc_num += ai4_proc_count[i4_loop_count];
                if(enc_ctr >= i4_acc_proc_num)
                {
                    /* if enc_ctr is greater than proc count for first group,
                    then increment group count.This group number will be starting grp num for
                    that resolution */
                    i4_curr_grp_num++;
                }
                else
                    break;
            }
        }

        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds; ctr++)
        {
            osal_thread_attr_t s_thread_attr = OSAL_DEFAULT_THREAD_ATTR;

            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.i4_mem_size = sizeof(frm_proc_thrd_ctxt_t);

            ps_hle_ctxt->ihevce_mem_alloc(
                ps_hle_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt_base->ps_stat_prms->s_sys_api, &s_memtab);
            if(s_memtab.pv_base == NULL)
            {
                return (IV_FAIL);
            }

            aps_enc_frm_proc_thrd_ctxt[res_ctr][ctr] = (frm_proc_thrd_ctxt_t *)s_memtab.pv_base;

            /* initialise the interface strucure parameters */
            aps_enc_frm_proc_thrd_ctxt[res_ctr][ctr]->i4_thrd_id = ctr;

            aps_enc_frm_proc_thrd_ctxt[res_ctr][ctr]->ps_hle_ctxt = ps_hle_ctxt;

            ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

            aps_enc_frm_proc_thrd_ctxt[res_ctr][ctr]->pv_enc_ctxt = (void *)ps_enc_ctxt;

            /* Initialize application thread attributes */
            s_thread_attr.exit_code = 0;
            s_thread_attr.name = 0;
            s_thread_attr.priority_map_flag = 1;
            s_thread_attr.priority = OSAL_PRIORITY_DEFAULT;
            s_thread_attr.stack_addr = 0;
            s_thread_attr.stack_size = THREAD_STACK_SIZE;
            s_thread_attr.thread_func = ihevce_enc_frm_proc_slave_thrd;
            s_thread_attr.thread_param = (void *)(aps_enc_frm_proc_thrd_ctxt[res_ctr][ctr]);
            s_thread_attr.group_num = i4_curr_grp_num;
            if(1 == ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_use_thrd_affinity)
            {
                ihevce_static_multi_thread_params_t *ps_multi_thrd_prms =
                    &ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms;

                s_thread_attr.core_affinity_mask = ps_multi_thrd_prms->au8_core_aff_mask[enc_ctr];
                if((enc_ctr >= i4_acc_proc_num) &&
                   (ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1))
                {
                    /*** When the cores in the Group0 is exhausted start enc threads in the next Processor Group ***/
                    s_thread_attr.group_num++;
                    i4_curr_grp_num++;
                    /* This takes care of the condition that differnt proc groups can have diff number of cores */
                    i4_acc_proc_num += ai4_proc_count[i4_curr_grp_num];
                }
            }
            else
            {
                s_thread_attr.core_affinity_mask = 0;
                if((enc_ctr >= i4_acc_proc_num) &&
                   (ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1))
                {
                    /*** When the cores in the Group0 is exhausted start enc threads in the next Processor Group ***/
                    s_thread_attr.group_num++;
                    i4_curr_grp_num++;
                    /* This takes care of the condition that differnt proc groups can have diff number of cores */
                    i4_acc_proc_num += ai4_proc_count[i4_curr_grp_num];
                }
            }

            /* Create frame processing thread */
            apv_enc_frm_proc_hdls[res_ctr][ctr] =
                osal_thread_create(ps_hle_ctxt->pv_osal_handle, &s_thread_attr);
            if(NULL == apv_enc_frm_proc_hdls[res_ctr][ctr])
            {
                return IV_FAIL;
            }
            enc_ctr++;
        }
    }

    /* --------------------------------------------------------------------- */
    /*     Create all Pre - Encode Frame processing threads                  */
    /* --------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        WORD32 pre_enc_ctr = 0;
        WORD32 i4_loop_count;
        WORD32 i4_curr_grp_num = 0;
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        i4_acc_proc_num = 0;

        for(i4_loop_count = 0; i4_loop_count < res_ctr; i4_loop_count++)
            pre_enc_ctr += ps_hle_ctxt->ai4_num_core_per_res[i4_loop_count];
        if(ps_enc_ctxt->s_multi_thrd.i4_all_thrds_active_flag)
        {
            /* If its sequential mode of operation enc and pre-enc threads to be given same core affinity mask */
            pre_enc_ctr -= ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds;
        }

        if(ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1)
        {
            /* Select the group number for each res based on processors present in each group */
            for(i4_loop_count = 0;
                i4_loop_count <
                ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups;
                i4_loop_count++)
            {
                i4_acc_proc_num += ai4_proc_count[i4_loop_count];
                if((pre_enc_ctr + ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds) >=
                   i4_acc_proc_num)
                {
                    /* if pre_enc_ctr is greater than proc count for first group,
                    then increment group count.This group number will be starting grp num for
                    that resolution */
                    i4_curr_grp_num++;
                }
                else
                    break;
            }
        }

        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds; ctr++)
        {
            osal_thread_attr_t s_thread_attr = OSAL_DEFAULT_THREAD_ATTR;

            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.i4_mem_size = sizeof(frm_proc_thrd_ctxt_t);

            ps_hle_ctxt->ihevce_mem_alloc(
                ps_hle_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt_base->ps_stat_prms->s_sys_api, &s_memtab);
            if(s_memtab.pv_base == NULL)
            {
                return (IV_FAIL);
            }

            aps_pre_enc_frm_proc_thrd_ctxt[res_ctr][ctr] = (frm_proc_thrd_ctxt_t *)s_memtab.pv_base;

            /* initialise the interface strucure parameters */
            aps_pre_enc_frm_proc_thrd_ctxt[res_ctr][ctr]->i4_thrd_id = ctr;

            aps_pre_enc_frm_proc_thrd_ctxt[res_ctr][ctr]->ps_hle_ctxt = ps_hle_ctxt;
            ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];
            aps_pre_enc_frm_proc_thrd_ctxt[res_ctr][ctr]->pv_enc_ctxt = (void *)ps_enc_ctxt;

            /* Initialize application thread attributes */
            s_thread_attr.exit_code = 0;
            s_thread_attr.name = 0;
            s_thread_attr.priority_map_flag = 1;
            s_thread_attr.priority = OSAL_PRIORITY_DEFAULT;
            s_thread_attr.stack_addr = 0;
            s_thread_attr.stack_size = THREAD_STACK_SIZE;
            s_thread_attr.thread_func = ihevce_pre_enc_process_frame_thrd;
            s_thread_attr.thread_param = (void *)(aps_pre_enc_frm_proc_thrd_ctxt[res_ctr][ctr]);
            s_thread_attr.group_num = i4_curr_grp_num;

            if(1 == ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_use_thrd_affinity)
            {
                ihevce_static_multi_thread_params_t *ps_multi_thrd_prms =
                    &ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms;

                s_thread_attr.core_affinity_mask =
                    ps_multi_thrd_prms->au8_core_aff_mask
                        [pre_enc_ctr + ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds];
                if(((pre_enc_ctr + ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds) >=
                    i4_acc_proc_num) &&
                   (ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1))
                {
                    /*** When the cores in the Group0 is exhausted start enc threads in the next Processor Group ***/
                    s_thread_attr.group_num++;
                    i4_curr_grp_num++;
                    /* This takes care of the condition that differnt proc groups can have diff number of cores */
                    i4_acc_proc_num += ai4_proc_count[i4_curr_grp_num];
                }
            }
            else
            {
                s_thread_attr.core_affinity_mask = 0;

                if(((pre_enc_ctr + ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds) >=
                    i4_acc_proc_num) &&
                   (ps_enc_ctxt_base->ps_stat_prms->s_multi_thrd_prms.i4_num_proc_groups > 1))
                {
                    /*** When the cores in the Group0 is exhausted start enc threads in the next Processor Group ***/
                    s_thread_attr.group_num++;
                    i4_curr_grp_num++;
                    /* This takes care of the condition that differnt proc groups can have diff number of cores */
                    i4_acc_proc_num += ai4_proc_count[i4_curr_grp_num];
                }
            }

            /* Create frame processing thread */
            apv_pre_enc_frm_proc_hdls[res_ctr][ctr] =
                osal_thread_create(ps_hle_ctxt->pv_osal_handle, &s_thread_attr);
            if(NULL == apv_pre_enc_frm_proc_hdls[res_ctr][ctr])
            {
                return IV_FAIL;
            }
            pre_enc_ctr++;
        }
    }

    /* Set the threads init done Flag */
    ps_hle_ctxt->i4_hle_init_done = 1;

    /* --------------------------------------------------------------------- */
    /*            Wait and destroy Processing threads                        */
    /* --------------------------------------------------------------------- */

    /* --------------------------------------------------------------------- */
    /*           Frame process Pre - Encode threads destroy                  */
    /* --------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds; ctr++)
        {
            /* Wait for thread to complete */
            osal_thread_wait(apv_pre_enc_frm_proc_hdls[res_ctr][ctr]);

            /* Destroy thread */
            osal_thread_destroy(apv_pre_enc_frm_proc_hdls[res_ctr][ctr]);

            s_memtab.i4_mem_size = sizeof(frm_proc_thrd_ctxt_t);
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.pv_base = (void *)aps_pre_enc_frm_proc_thrd_ctxt[res_ctr][ctr];

            /* free the ctxt memory */
            ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
        }
    }

    /* --------------------------------------------------------------------- */
    /*           Frame process Encode slave threads destroy                  */
    /* --------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds; ctr++)
        {
            /* Wait for thread to complete */
            osal_thread_wait(apv_enc_frm_proc_hdls[res_ctr][ctr]);

            /* Destroy thread */
            osal_thread_destroy(apv_enc_frm_proc_hdls[res_ctr][ctr]);

            s_memtab.i4_mem_size = sizeof(frm_proc_thrd_ctxt_t);
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.pv_base = (void *)aps_enc_frm_proc_thrd_ctxt[res_ctr][ctr];

            /* free the ctxt memory */
            ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
        }
    }

    /* --------------------------------------------------------------------- */
    /*           Entropy threads destroy                                     */
    /* --------------------------------------------------------------------- */
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        WORD32 i4_num_bitrates =
            ps_enc_ctxt_base->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[res_ctr]
                .i4_num_bitrate_instances;

        for(ctr = 0; ctr < i4_num_bitrates; ctr++)
        {
            /* Wait for Entropy Coding thread to complete */
            osal_thread_wait(apv_entropy_thrd_hdls[res_ctr][ctr]);

            /* Destroy Entropy Coding thread */
            osal_thread_destroy(apv_entropy_thrd_hdls[res_ctr][ctr]);

            //semaphore will come here

            s_memtab.i4_mem_size = sizeof(frm_proc_thrd_ctxt_t);
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.pv_base = (void *)aps_entropy_thrd_ctxt[res_ctr][ctr];

            /* free the ctxt memory */
            ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
        }
    }

    s_memtab.i4_mem_size = sizeof(lap_intface_t);
    s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
    s_memtab.pv_base = (void *)ps_lap_interface_ctxt;
    ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
    /* profile stop */
    PROFILE_STOP(&ps_hle_ctxt->profile_hle, NULL);
    return (0);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_get_free_inp_data_buff \endif
*
* \brief
*    Gets a free buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] pointer to return the buffer id
* \param[in] blocking mode / non blocking mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_free_inp_data_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode)
{
    void *pv_ptr;
    enc_ctxt_t *ps_enc_ctxt;
    WORD32 i4_resolution_id = 0;

    ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[i4_resolution_id];
    if(ps_enc_ctxt->i4_frame_limit_reached == 1)
    {
        return (NULL);
    }
    /*Input buffer is same for all enc handles*/
    pv_ptr = ihevce_q_get_free_buff(
        ps_hle_ctxt->apv_enc_hdl[0], IHEVCE_INPUT_DATA_CTRL_Q, pi4_buff_id, i4_blocking_mode);

    return (pv_ptr);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_get_free_inp_ctrl_buff \endif
*
* \brief
*    Gets a free buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] pointer to return the buffer id
* \param[in] blocking mode / non blocking mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_free_inp_ctrl_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode)
{
    void *pv_ptr;

    /*Input buffer is same for all enc handles*/
    pv_ptr = ihevce_q_get_free_buff(
        ps_hle_ctxt->apv_enc_hdl[0], IHEVCE_INPUT_ASYNCH_CTRL_Q, pi4_buff_id, i4_blocking_mode);

    return (pv_ptr);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_get_free_out_strm_buff \endif
*
* \brief
*    Gets a free buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] pointer to return the buffer id
* \param[in] blocking mode / non blocking mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_free_out_strm_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_buff_id,
    WORD32 i4_blocking_mode,
    WORD32 i4_bitrate_instance,
    WORD32 i4_res_instance)
{
    void *pv_ptr;

    pv_ptr = ihevce_q_get_free_buff(
        ps_hle_ctxt->apv_enc_hdl[i4_res_instance],
        (IHEVCE_OUTPUT_DATA_Q + i4_bitrate_instance),
        pi4_buff_id,
        i4_blocking_mode);
    return (pv_ptr);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_get_free_out_recon_buff \endif
*
* \brief
*    Gets a free buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] pointer to return the buffer id
* \param[in] blocking mode / non blocking mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_free_out_recon_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_buff_id,
    WORD32 i4_blocking_mode,
    WORD32 i4_bitrate_instance,
    WORD32 i4_res_instance)
{
    void *pv_ptr;

    pv_ptr = ihevce_q_get_free_buff(
        ps_hle_ctxt->apv_enc_hdl[i4_res_instance],
        (IHEVCE_RECON_DATA_Q + i4_bitrate_instance),
        pi4_buff_id,
        i4_blocking_mode);
    return (pv_ptr);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_set_inp_data_buff_prod \endif
*
* \brief
*    Sets the input data buffer as produced in the que requested
*
* \param[in] high level encoder context pointer
* \param[in] buffer id which needs to be set as produced
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
    ihevce_q_set_inp_data_buff_prod(ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_buff_id)
{
    IV_API_CALL_STATUS_T ret_status;

    ret_status =
        ihevce_q_set_buff_prod(ps_hle_ctxt->apv_enc_hdl[0], IHEVCE_INPUT_DATA_CTRL_Q, i4_buff_id);

    return (ret_status);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_set_inp_ctrl_buff_prod \endif
*
* \brief
*    Sets the input data buffer as produced in the que requested
*
* \param[in] high level encoder context pointer
* \param[in] buffer id which needs to be set as produced
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
    ihevce_q_set_inp_ctrl_buff_prod(ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_buff_id)

{
    IV_API_CALL_STATUS_T ret_status;

    ret_status =
        ihevce_q_set_buff_prod(ps_hle_ctxt->apv_enc_hdl[0], IHEVCE_INPUT_ASYNCH_CTRL_Q, i4_buff_id);

    return (ret_status);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_set_out_strm_buff_prod \endif
*
* \brief
*    Sets the Output stream buffer as produced in the que requested
*
* \param[in] high level encoder context pointer
* \param[in] buffer id which needs to be set as produced
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_q_set_out_strm_buff_prod(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 i4_buff_id,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id)
{
    IV_API_CALL_STATUS_T ret_status;

    ret_status = ihevce_q_set_buff_prod(
        ps_hle_ctxt->apv_enc_hdl[i4_resolution_id],
        (IHEVCE_OUTPUT_DATA_Q + i4_bitrate_instance_id),
        i4_buff_id);

    return (ret_status);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_set_out_recon_buff_prod \endif
*
* \brief
*    Sets the Output recon buffer as produced in the que requested
*
* \param[in] high level encoder context pointer
* \param[in] buffer id which needs to be set as produced
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_q_set_out_recon_buff_prod(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 i4_buff_id,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id)
{
    IV_API_CALL_STATUS_T ret_status;

    ret_status = ihevce_q_set_buff_prod(
        ps_hle_ctxt->apv_enc_hdl[i4_resolution_id],
        (IHEVCE_RECON_DATA_Q + i4_bitrate_instance_id),
        i4_buff_id);

    return (ret_status);
}

//recon_dump
/*!
******************************************************************************
* \if Function name : ihevce_q_get_filled_recon_buff \endif
*
* \brief
*    Gets a next filled recon buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] pointer to return the buffer id
* \param[in] blocking mode / non blocking mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_filled_recon_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 *pi4_buff_id,
    WORD32 i4_blocking_mode,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id)
{
    void *pv_ptr;

    pv_ptr = ihevce_q_get_filled_buff(
        ps_hle_ctxt->apv_enc_hdl[i4_resolution_id],
        IHEVCE_RECON_DATA_Q + i4_bitrate_instance_id,
        pi4_buff_id,
        i4_blocking_mode);

    return (pv_ptr);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_get_filled_ctrl_sts_buff \endif
*
* \brief
*    Gets a next filled control status buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] pointer to return the buffer id
* \param[in] blocking mode / non blocking mode
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_filled_ctrl_sts_buff(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode)
{
    void *pv_ptr;
    pv_ptr = ihevce_q_get_filled_buff(
        ps_hle_ctxt->apv_enc_hdl[0], IHEVCE_OUTPUT_STATUS_Q, pi4_buff_id, i4_blocking_mode);

    return (pv_ptr);
}

//recon_dump
/*!
******************************************************************************
* \if Function name : ihevce_q_rel_recon_buf \endif
*
* \brief
*    Frees the recon buffer in the recon buffer que
*
* \param[in] high level encoder context pointer
* \param[in] buffer id which needs to be freed
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_q_rel_recon_buf(
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    WORD32 i4_buff_id,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id)
{
    IV_API_CALL_STATUS_T ret_status;

    ret_status = ihevce_q_rel_buf(
        ps_hle_ctxt->apv_enc_hdl[i4_resolution_id],
        IHEVCE_RECON_DATA_Q + i4_bitrate_instance_id,
        i4_buff_id);

    return (ret_status);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_rel_ctrl_sts_buf \endif
*
* \brief
*    Frees the output control sttus buffer in buffer que
*
* \param[in] high level encoder context pointer
* \param[in] buffer id which needs to be freed
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_q_rel_ctrl_sts_buf(ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_buff_id)
{
    IV_API_CALL_STATUS_T ret_status;

    ret_status = ihevce_q_rel_buf(ps_hle_ctxt->apv_enc_hdl[0], IHEVCE_OUTPUT_STATUS_Q, i4_buff_id);

    return (ret_status);
}

/*!
******************************************************************************
* \if Function name : ihevce_hle_interface_delete \endif
*
* \brief
*    High leve encoder delete interface
*
* \param[in] high level encoder interface context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_hle_interface_delete(ihevce_hle_ctxt_t *ps_hle_ctxt)
{
    /* local varaibles */
    enc_ctxt_t *ps_enc_ctxt;
    iv_mem_rec_t s_memtab;
    WORD32 ctr = 0, i, res_ctr, i4_num_resolutions;
    WORD32 ai4_num_bitrate_instances[IHEVCE_MAX_NUM_RESOLUTIONS] = { 1 };

    i4_num_resolutions = ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_num_res_layers;
    for(ctr = 0; ctr < i4_num_resolutions; ctr++)
    {
        ai4_num_bitrate_instances[ctr] =
            ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[ctr]
                .i4_num_bitrate_instances;
    }

    for(res_ctr = 0; res_ctr < i4_num_resolutions && ps_hle_ctxt->apv_enc_hdl[res_ctr]; res_ctr++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[res_ctr];

        if(res_ctr == 0)
        {
            osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle);
            osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_data_sem_handle);
            osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_ctrl_sem_handle);
            if(1 == ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out)
            {
                osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_ent_common_mres_sem_hdl);
                osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_common_mres_sem_hdl);
            }
        }

        osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_inp_data_sem_hdl);
        osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_preenc_inp_data_sem_hdl);

        osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
        osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle);

        osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_ctrl_sem_handle);

        for(i = 0; i < ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[res_ctr]
                           .i4_num_bitrate_instances;
            i++)
        {
            osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.apv_ent_cod_sem_handle[i]);
            osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_strm_sem_handle[i]);
            osal_sem_destroy(ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_recon_sem_handle[i]);
        }

        /* destroy the mutex  allocated for job queue usage encode group */
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_enc_grp_me);

        /* destroy the mutex  allocated for job queue usage encode group */
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_enc_grp_enc_loop);

        /* destroy the mutexes allocated for enc thread group */
        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.apv_mutex_handle[i]);

            osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_me_end[i]);
        }

        for(i = 0; i < MAX_NUM_ENC_LOOP_PARALLEL; i++)
        {
            osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.apv_mutex_handle_frame_init[i]);

            osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.apv_post_enc_mutex_handle[i]);
        }

        /* destroy the mutex  allocated for job queue, init and de-init
        usage pre enocde group */
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_decomp);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_hme);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_job_q_mutex_hdl_pre_enc_l0ipe);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_init);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_decomp_deinit);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_init);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_hme_deinit);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_l0_ipe_init);
        osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_mutex_hdl_pre_enc_deinit);

        /* destroy the EncLoop Module */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        ihevce_enc_loop_delete(ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt);

        /* destroy the Coarse ME Module */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        ihevce_coarse_me_delete(
            ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
            ps_hle_ctxt->ps_static_cfg_prms,
            ps_enc_ctxt->i4_resolution_id);
        /* destroy semaphores for all the threads in pre-enc and enc */
        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds; ctr++)
        {
            osal_sem_destroy(ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle[ctr]);
        }

        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds; ctr++)
        {
            osal_sem_destroy(ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle[ctr]);
        }

        /* destroy the ME-EncLoop Dep Mngr */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        for(ctr = 0; ctr < NUM_ME_ENC_BUFS; ctr++)
        {
            ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_encloop_dep_me[ctr]);
        }
        /* destroy the Prev. frame EncLoop Done Dep Mngr */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        for(i = 0; i < ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel; i++)
        {
            ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_done[i]);
        }
        /* destroy the Prev. frame EncLoop Done for re-encode Dep Mngr */
        ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_enc_done_for_reenc);

        /* destroy the Prev. frame ME Done Dep Mngr */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        for(i = 0; i < ps_enc_ctxt->s_multi_thrd.i4_num_me_frm_pllel; i++)
        {
            ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_me_done[i]);
        }

        /* destroy the Prev. frame PreEnc L1 Done Dep Mngr */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l1);

        /* destroy the Prev. frame PreEnc HME Done Dep Mngr */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_coarse_me);

        /* destroy the Prev. frame PreEnc L0 Done Dep Mngr */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        ihevce_dmgr_del(ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l0);

        /* destroy the ME-Prev Recon  Dep Mngr    */
        /* Note : Only Destroys the resources allocated in the module like */
        /* semaphore,etc. Memory free is done separately using memtabs     */
        for(ctr = 0; ctr < ps_enc_ctxt->ai4_num_buf_recon_q[0]; ctr++)
        {
            ihevce_dmgr_del(ps_enc_ctxt->pps_recon_buf_q[0][ctr]->pv_dep_mngr_recon);
        }

        /* destroy all the mutex created */
        if(res_ctr == 0)
        {
            if(NULL != ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl)
            {
                osal_mutex_destroy(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);
            }
        }

        if(NULL != ps_enc_ctxt->pv_rc_mutex_lock_hdl)
        {
            osal_mutex_destroy(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
        }

        if(NULL != ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_mutex_lock_hdl)
        {
            osal_mutex_destroy(ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_mutex_lock_hdl);
        }

        if(NULL != ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_for_qp_update_mutex_lock_hdl)
        {
            osal_mutex_destroy(
                ps_enc_ctxt->s_multi_thrd.pv_sub_pic_rc_for_qp_update_mutex_lock_hdl);
        }

        /* call the memrory free function */
        ihevce_mem_manager_free(ps_enc_ctxt, ps_hle_ctxt);
        if((1 == ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out) &&
           (res_ctr == 0))
        {
            s_memtab.i4_mem_size = sizeof(WORD32) * IHEVCE_MAX_NUM_RESOLUTIONS;
            s_memtab.i4_mem_alignment = 4;
            s_memtab.i4_size = sizeof(iv_mem_rec_t);
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.pv_base = ps_enc_ctxt->s_multi_thrd.pi4_active_res_id;
            /* free active_res_id memory */
            ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
        }
        if(res_ctr == (i4_num_resolutions - 1))
        {
            s_memtab.i4_mem_size = sizeof(ihevce_static_cfg_params_t);
            s_memtab.i4_mem_alignment = 4;
            s_memtab.i4_size = sizeof(iv_mem_rec_t);
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.pv_base = ps_enc_ctxt->ps_stat_prms;

            /* free the encoder context pointer */
            ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);
        }
        s_memtab.i4_mem_size = sizeof(enc_ctxt_t);
        s_memtab.i4_mem_alignment = 4;
        s_memtab.i4_size = sizeof(iv_mem_rec_t);
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
        s_memtab.pv_base = ps_enc_ctxt;

        /* free the encoder context pointer */
        ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_memtab);

        /* reset the encoder handle to NULL */
        ps_hle_ctxt->apv_enc_hdl[res_ctr] = NULL;
    }
    /* profile end */
    PROFILE_END(&ps_hle_ctxt->profile_hle, "hle interface thread active time");
    for(res_ctr = 0; res_ctr < i4_num_resolutions; res_ctr++)
    {
        WORD32 i4_br_id;

        PROFILE_END(&ps_hle_ctxt->profile_pre_enc_l1l2[res_ctr], "pre enc l1l2 process");
        PROFILE_END(&ps_hle_ctxt->profile_pre_enc_l0ipe[res_ctr], "pre enc l0 ipe process");
        PROFILE_END(&ps_hle_ctxt->profile_enc_me[res_ctr], "enc me process");
        for(i4_br_id = 0; i4_br_id < ai4_num_bitrate_instances[res_ctr]; i4_br_id++)
        {
            PROFILE_END(&ps_hle_ctxt->profile_enc[res_ctr][i4_br_id], "enc loop process");
            PROFILE_END(&ps_hle_ctxt->profile_entropy[res_ctr][i4_br_id], "entropy process");
        }
    }

    /* OSAL Delete */
    ihevce_osal_delete((void *)ps_hle_ctxt);

    return (IV_SUCCESS);
}
