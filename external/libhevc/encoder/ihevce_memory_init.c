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
* \file ihevce_memory_init.c
*
* \brief
*    This file contains functions which perform memory requirement gathering
*    and freeing of memories of encoder at the end
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
#include "ihevc_common_tables.h"

#include "ihevce_defs.h"
#include "ihevce_hle_interface.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_lap_interface.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_decomp_pre_intra_structs.h"
#include "ihevce_decomp_pre_intra_pass.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_nbr_avail.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_sub_pic_rc.h"
#include "ihevce_global_tables.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_cabac_rdo.h"
#include "ihevce_deblk.h"
#include "ihevce_entropy_interface.h"
#include "ihevce_frame_process.h"
#include "ihevce_ipe_pass.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_rc_interface.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "ihevce_enc_subpel_gen.h"
#include "ihevce_inter_pred.h"
#include "ihevce_mv_pred.h"
#include "ihevce_mv_pred_merge.h"
#include "ihevce_enc_loop_inter_mode_sifter.h"
#include "ihevce_me_pass.h"
#include "ihevce_coarse_me_pass.h"
#include "ihevce_enc_cu_recursion.h"
#include "ihevce_enc_loop_pass.h"
#include "ihevce_common_utils.h"
#include "ihevce_buffer_que_interface.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_sao.h"
#include "ihevce_tile_interface.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_mem_manager_init \endif
*
* \brief
*    Encoder Memory init function
*
* \param[in] Processing interface context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
#define MAX_QUEUE 40
void ihevce_mem_manager_init(enc_ctxt_t *ps_enc_ctxt, ihevce_hle_ctxt_t *ps_intrf_ctxt)
{
    /* local variables */
    WORD32 total_memtabs_req = 0;
    WORD32 total_memtabs_used = 0;
    WORD32 total_system_memtabs = 0;
    WORD32 ctr;
    WORD32 buf_size;
    WORD32 num_ctb_horz;
    WORD32 num_ctb_vert;
    WORD32 num_cu_in_ctb;
    WORD32 num_pu_in_ctb;
    WORD32 num_tu_in_ctb;
    WORD32 ctb_size;
    WORD32 min_cu_size;
    WORD32 max_num_ref_pics;
    WORD32 mem_alloc_ctrl_flag;
    WORD32 space_for_mem_in_enc_grp = 0;
    WORD32 space_for_mem_in_pre_enc_grp = 0;
    WORD32 mv_bank_size;
    WORD32 ref_idx_bank_size;
    WORD32 a_wd[MAX_NUM_HME_LAYERS], a_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_wd[MAX_NUM_HME_LAYERS], a_disp_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_ctb_align_wd[MAX_NUM_HME_LAYERS], a_ctb_align_ht[MAX_NUM_HME_LAYERS];
    WORD32 n_enc_layers = 1, n_tot_layers;
    WORD32 num_bufs_preenc_me_que, num_bufs_L0_ipe_enc, max_delay_preenc_l0_que;
    WORD32 i, i4_resolution_id = ps_enc_ctxt->i4_resolution_id;  //counter
    WORD32 i4_num_bitrate_inst;
    iv_mem_rec_t *ps_memtab;
    WORD32 i4_field_pic, i4_total_queues = 0;

    recon_pic_buf_t **pps_pre_enc_pic_bufs;
    frm_proc_ent_cod_ctxt_t **pps_frm_proc_ent_cod_bufs[IHEVCE_MAX_NUM_BITRATES];
    pre_enc_me_ctxt_t **pps_pre_enc_bufs;
    me_enc_rdopt_ctxt_t **pps_me_enc_bufs;
    pre_enc_L0_ipe_encloop_ctxt_t **pps_L0_ipe_enc_bufs;
    /*get number of input buffer required based on requirement from each stage*/
    ihevce_lap_enc_buf_t **pps_lap_enc_input_bufs;
    WORD32 i4_num_enc_loop_frm_pllel;
    WORD32 i4_num_me_frm_pllel;
    /*msr: These are parameters required to allocate input buffer,
    encoder needs to be initilized before getting requirements hence filled once static params are initilized*/
    WORD32 num_input_buf_per_queue, i4_yuv_min_size, i4_luma_min_size;

    i4_num_bitrate_inst = ps_enc_ctxt->i4_num_bitrates;
    i4_field_pic = ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_field_pic;
    ps_intrf_ctxt->i4_gpu_mem_size = 0;

    /*Initialize the thrd id flag and all deafult values for sub pic rc */
    {
        WORD32 i, j, k;

        for(i = 0; i < MAX_NUM_ENC_LOOP_PARALLEL; i++)
        {
            for(j = 0; j < IHEVCE_MAX_NUM_BITRATES; j++)
            {
                ps_enc_ctxt->s_multi_thrd.ai4_acc_ctb_ctr[i][j] = 0;
                ps_enc_ctxt->s_multi_thrd.ai4_ctb_ctr[i][j] = 0;

                ps_enc_ctxt->s_multi_thrd.ai4_threshold_reached[i][j] = 0;

                ps_enc_ctxt->s_multi_thrd.ai4_curr_qp_acc[i][j] = 0;

                ps_enc_ctxt->s_multi_thrd.af_acc_hdr_bits_scale_err[i][j] = 0;

                for(k = 0; k < MAX_NUM_FRM_PROC_THRDS_ENC; k++)
                {
                    ps_enc_ctxt->s_multi_thrd.ai4_thrd_id_valid_flag[i][j][k] = -1;
                }
            }
        }
    }

#define ENABLE_FRM_PARALLEL
#ifdef ENABLE_FRM_PARALLEL
    i4_num_enc_loop_frm_pllel = MAX_NUM_ENC_LOOP_PARALLEL;
    i4_num_me_frm_pllel = MAX_NUM_ME_PARALLEL;
#else
    i4_num_enc_loop_frm_pllel = 1;
    i4_num_me_frm_pllel = 1;
#endif

    ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel = i4_num_enc_loop_frm_pllel;
    ps_enc_ctxt->i4_max_fr_enc_loop_parallel_rc = i4_num_enc_loop_frm_pllel;
    ps_enc_ctxt->s_multi_thrd.i4_num_me_frm_pllel = i4_num_me_frm_pllel;
    ps_enc_ctxt->s_multi_thrd.i4_force_end_flag = 0;

    ps_enc_ctxt->i4_ref_mbr_id = 0;
    /* get the ctb size from max cu size */
    ctb_size = ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_max_log2_cu_size;

    /* get the min cu size from config params */
    min_cu_size = ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_min_log2_cu_size;

    /* convert to actual width */
    ctb_size = 1 << ctb_size;
    min_cu_size = 1 << min_cu_size;

    /* Get the width and heights of different decomp layers */
    *a_wd =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
            .i4_width +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                .i4_width,
            min_cu_size);
    *a_ht =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
            .i4_height +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                .i4_height,
            min_cu_size);

    n_tot_layers = hme_derive_num_layers(n_enc_layers, a_wd, a_ht, a_disp_wd, a_disp_ht);
    hme_coarse_get_layer1_mv_bank_ref_idx_size(
        n_tot_layers,
        a_wd,
        a_ht,
        ((ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_reference_frames == -1)
             ? ((DEFAULT_MAX_REFERENCE_PICS) << i4_field_pic)
             : ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_reference_frames),
        (S32 *)(&mv_bank_size),
        (S32 *)(&ref_idx_bank_size));
    if(n_tot_layers < 3)
    {
        WORD32 error_code;
        error_code = IHEVCE_NUM_DECOMP_LYRS_NOT_SUPPORTED;
        ps_intrf_ctxt->i4_error_code = IHEVCE_SETUNSUPPORTEDINPUT(error_code);
        return;
    }

    /* calculate num cu,pu,tu in ctb */
    num_cu_in_ctb = ctb_size / MIN_CU_SIZE;
    num_cu_in_ctb *= num_cu_in_ctb;

    num_pu_in_ctb = ctb_size / MIN_PU_SIZE;
    num_pu_in_ctb *= num_pu_in_ctb;

    num_tu_in_ctb = ctb_size / MIN_PU_SIZE;
    num_tu_in_ctb *= num_tu_in_ctb;

    /* calcuate the number of ctb horizontally*/
    num_ctb_horz =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
            .i4_width +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                .i4_width,
            ctb_size);
    num_ctb_horz = num_ctb_horz / ctb_size;

    /* calcuate the number of ctb vertically*/
    num_ctb_vert =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
            .i4_height +
        SET_CTB_ALIGN(
            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                .i4_height,
            ctb_size);
    num_ctb_vert = num_ctb_vert / ctb_size;

    /* align all the decomp layer dimensions to CTB alignment */
    for(ctr = 0; ctr < n_tot_layers; ctr++)
    {
        a_ctb_align_wd[ctr] = a_wd[ctr] + SET_CTB_ALIGN(a_wd[ctr], ctb_size);

        a_ctb_align_ht[ctr] = a_ht[ctr] + SET_CTB_ALIGN(a_ht[ctr], ctb_size);
    }

    /* SEI related parametert initialization */

    ps_enc_ctxt->u4_cur_pic_encode_cnt = 0;

    /* store the frame level ctb parameters which will be constant for the session */
    ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size = ctb_size;
    ps_enc_ctxt->s_frm_ctb_prms.i4_min_cu_size = min_cu_size;
    ps_enc_ctxt->s_frm_ctb_prms.i4_num_cus_in_ctb = num_cu_in_ctb;
    ps_enc_ctxt->s_frm_ctb_prms.i4_num_pus_in_ctb = num_pu_in_ctb;
    ps_enc_ctxt->s_frm_ctb_prms.i4_num_tus_in_ctb = num_tu_in_ctb;

    /* intialize cra poc to default value */
    ps_enc_ctxt->i4_cra_poc = 0;

    /* initialise the memory alloc control flag */
    mem_alloc_ctrl_flag = ps_enc_ctxt->ps_stat_prms->s_multi_thrd_prms.i4_memory_alloc_ctrl_flag;

    /* decide the memory space for enc_grp and pre_enc_grp based on control flag */
    if(0 == mem_alloc_ctrl_flag)
    {
        /* normal memory */
        space_for_mem_in_enc_grp = IV_EXT_CACHEABLE_NORMAL_MEM;
        space_for_mem_in_pre_enc_grp = IV_EXT_CACHEABLE_NORMAL_MEM;
    }
    else if(1 == mem_alloc_ctrl_flag)
    {
        /* only NUMA Node 0 memory allocation */
        space_for_mem_in_enc_grp = IV_EXT_CACHEABLE_NUMA_NODE0_MEM;
        space_for_mem_in_pre_enc_grp = IV_EXT_CACHEABLE_NUMA_NODE0_MEM;
    }
    else if(2 == mem_alloc_ctrl_flag)
    {
        /* Both NUMA Node 0 & Node 1 memory allocation */
        space_for_mem_in_enc_grp = IV_EXT_CACHEABLE_NUMA_NODE0_MEM;
        space_for_mem_in_pre_enc_grp = IV_EXT_CACHEABLE_NUMA_NODE1_MEM;
    }
    else
    {
        /* should not enter here */
        ASSERT(0);
    }

    {
        if(ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel > 1)
        {
            num_bufs_preenc_me_que = MIN_L1_L0_STAGGER_NON_SEQ +
                                     ps_enc_ctxt->ps_stat_prms->s_lap_prms.i4_rc_look_ahead_pics +
                                     (MAX_L0_IPE_ENC_STAGGER - 1) + NUM_BUFS_DECOMP_HME;
        }
        else
        {
            num_bufs_preenc_me_que = MIN_L1_L0_STAGGER_NON_SEQ +
                                     ps_enc_ctxt->ps_stat_prms->s_lap_prms.i4_rc_look_ahead_pics +
                                     (MIN_L0_IPE_ENC_STAGGER - 1) + NUM_BUFS_DECOMP_HME;
        }

        /*The number of buffers to support stagger between L0 IPE, ME and enc loop. This is a separate queue to store L0 IPE
        output to save memory since this is not used in L1 stage*/
        if(ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel > 1)
        {
            num_bufs_L0_ipe_enc = MAX_L0_IPE_ENC_STAGGER;
        }
        else
        {
            num_bufs_L0_ipe_enc = MIN_L0_IPE_ENC_STAGGER;
        }

        max_delay_preenc_l0_que = MIN_L1_L0_STAGGER_NON_SEQ +
                                  ps_enc_ctxt->ps_stat_prms->s_lap_prms.i4_rc_look_ahead_pics + 1;
    }

    /* ------------ popluate the lap static parameters ------------- */
    ps_enc_ctxt->s_lap_stat_prms.i4_max_closed_gop_period =
        ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_closed_gop_period;

    ps_enc_ctxt->s_lap_stat_prms.i4_min_closed_gop_period =
        ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_min_closed_gop_period;

    ps_enc_ctxt->s_lap_stat_prms.i4_max_cra_open_gop_period =
        ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_cra_open_gop_period;

    ps_enc_ctxt->s_lap_stat_prms.i4_max_i_open_gop_period =
        ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_i_open_gop_period;

    ps_enc_ctxt->s_lap_stat_prms.i4_max_reference_frames =
        ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_reference_frames;

    ps_enc_ctxt->s_lap_stat_prms.i4_max_temporal_layers =
        ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_temporal_layers;

    ps_enc_ctxt->s_lap_stat_prms.i4_width = ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_width;

    ps_enc_ctxt->s_lap_stat_prms.i4_height = ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_height;

    ps_enc_ctxt->s_lap_stat_prms.i4_enable_logo = ps_enc_ctxt->ps_stat_prms->i4_enable_logo;

    ps_enc_ctxt->s_lap_stat_prms.i4_src_interlace_field =
        ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_field_pic;
    ps_enc_ctxt->s_lap_stat_prms.i4_frame_rate =
        ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_frm_rate_num /
        ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_frm_rate_denom;

    ps_enc_ctxt->s_lap_stat_prms.i4_blu_ray_spec = ps_enc_ctxt->i4_blu_ray_spec;

    ps_enc_ctxt->s_lap_stat_prms.i4_internal_bit_depth =
        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_internal_bit_depth;

    ps_enc_ctxt->s_lap_stat_prms.i4_input_bit_depth =
        ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_input_bit_depth;

    ps_enc_ctxt->s_lap_stat_prms.u1_chroma_array_type =
        (ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV) ? 2 : 1;

    ps_enc_ctxt->s_lap_stat_prms.i4_rc_pass_num = ps_enc_ctxt->ps_stat_prms->s_pass_prms.i4_pass;

    if(0 == i4_resolution_id)
    {
        for(ctr = 0; ctr < ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_num_res_layers; ctr++)
        {
            ps_enc_ctxt->s_lap_stat_prms.ai4_quality_preset[ctr] =
                ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[ctr].i4_quality_preset;

            if(ps_enc_ctxt->s_lap_stat_prms.ai4_quality_preset[ctr] == IHEVCE_QUALITY_P7)
            {
                ps_enc_ctxt->s_lap_stat_prms.ai4_quality_preset[ctr] = IHEVCE_QUALITY_P6;
            }
        }
    }
    memcpy(
        &ps_enc_ctxt->s_lap_stat_prms.s_lap_params,
        &ps_enc_ctxt->ps_stat_prms->s_lap_prms,
        sizeof(ihevce_lap_params_t));

    /* copy the create prms as runtime prms */
    memcpy(
        &ps_enc_ctxt->s_runtime_src_prms,
        &ps_enc_ctxt->ps_stat_prms->s_src_prms,
        sizeof(ihevce_src_params_t));
    /*Copy the target params*/
    memcpy(
        &ps_enc_ctxt->s_runtime_tgt_params,
        &ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id],
        sizeof(ihevce_tgt_params_t));
    ps_enc_ctxt->s_lap_stat_prms.e_arch_type = ps_enc_ctxt->ps_stat_prms->e_arch_type;
    ps_enc_ctxt->s_lap_stat_prms.u1_is_popcnt_available = ps_enc_ctxt->u1_is_popcnt_available;

    /* copy the create prms as runtime prms */
    memcpy(
        &ps_enc_ctxt->s_runtime_src_prms,
        &ps_enc_ctxt->ps_stat_prms->s_src_prms,
        sizeof(ihevce_src_params_t));
    /*Copy the target params*/
    memcpy(
        &ps_enc_ctxt->s_runtime_tgt_params,
        &ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id],
        sizeof(ihevce_tgt_params_t));

    /* copy the run time coding parameters */
    memcpy(
        &ps_enc_ctxt->s_runtime_coding_prms,
        &ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms,
        sizeof(ihevce_coding_params_t));
    /*change in run time parameter*/
    if(ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_reference_frames == -1)
    {
        ps_enc_ctxt->s_runtime_coding_prms.i4_max_reference_frames = (DEFAULT_MAX_REFERENCE_PICS)
                                                                     << i4_field_pic;
        ps_enc_ctxt->s_lap_stat_prms.i4_max_reference_frames =
            ps_enc_ctxt->s_runtime_coding_prms.i4_max_reference_frames;
    }
    ASSERT(i4_num_enc_loop_frm_pllel == i4_num_me_frm_pllel);

    if((1 == i4_num_enc_loop_frm_pllel) && (1 == i4_num_me_frm_pllel))
    {
        max_num_ref_pics = ps_enc_ctxt->s_runtime_coding_prms.i4_max_reference_frames;
    }
    else
    {
        max_num_ref_pics =
            ps_enc_ctxt->s_runtime_coding_prms.i4_max_reference_frames * i4_num_enc_loop_frm_pllel;
    }
    /* --------------------------------------------------------------------- */
    /* --------------  Collating the number of memtabs required ------------ */
    /* --------------------------------------------------------------------- */

    /* Memtabs for syntactical tiles */
    total_memtabs_req += ihevce_tiles_get_num_mem_recs();

    /* ---------- Enc loop Memtabs --------- */
    total_memtabs_req +=
        ihevce_enc_loop_get_num_mem_recs(i4_num_bitrate_inst, i4_num_enc_loop_frm_pllel);
    /* ---------- ME Memtabs --------------- */
    total_memtabs_req += ihevce_me_get_num_mem_recs(i4_num_me_frm_pllel);

    /* ---------- Coarse ME Memtabs --------------- */
    total_memtabs_req += ihevce_coarse_me_get_num_mem_recs();
    /* ---------- IPE Memtabs -------------- */
    total_memtabs_req += ihevce_ipe_get_num_mem_recs();

    /* ---------- ECD Memtabs -------------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        total_memtabs_req += ihevce_entropy_get_num_mem_recs();
    }
    if(0 == ps_enc_ctxt->i4_resolution_id)
    {
        /* ---------- LAP Memtabs--------------- */
        total_memtabs_req += ihevce_lap_get_num_mem_recs();
    }
    /* ---------- Decomp Pre Intra Memtabs--------------- */
    total_memtabs_req += ihevce_decomp_pre_intra_get_num_mem_recs();

    /* ---------- RC memtabs --------------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        total_memtabs_req += ihevce_rc_get_num_mem_recs(); /*HEVC_RC*/
    }

    /* ---------- System Memtabs ----------- */
    total_memtabs_req += TOTAL_SYSTEM_MEM_RECS;  //increment this based on final requirement

    /* -----Frameproc Entcod Que Memtabs --- */
    /* one queue for each bit-rate is used */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        total_memtabs_req += ihevce_buff_que_get_num_mem_recs();
    }
    /* mrs:memtab for one queue for encoder owned input queue, This is only request for memtab, currently more than
    required memtabs are allocated. Hence my change of using memtab for yuv buffers is surviving. Only memtab
    usage and initialization needs to be exact sync*/
    total_memtabs_req += ihevce_buff_que_get_num_mem_recs();

    /* ---Pre-encode Encode Que Mem requests -- */
    total_memtabs_req += ihevce_buff_que_get_num_mem_recs();

    /* -----ME / Enc-RD opt Que Mem requests --- */
    total_memtabs_req += ihevce_buff_que_get_num_mem_recs();

    /* ----Pre-encode L0 IPE to enc Que Mem requests -- */
    total_memtabs_req += ihevce_buff_que_get_num_mem_recs();

    /* --- ME-EncLoop Dep Mngr Row-Row Mem requests -- */
    total_memtabs_req += NUM_ME_ENC_BUFS * ihevce_dmgr_get_num_mem_recs();

    /* --- Prev. frame EncLoop Done Dep Mngr Frm-Frm Mem requests -- */
    total_memtabs_req += i4_num_enc_loop_frm_pllel * ihevce_dmgr_get_num_mem_recs();

    /* --- Prev. frame EncLoop Done for re-encode Dep Mngr Frm-Frm Mem requests -- */
    total_memtabs_req += ihevce_dmgr_get_num_mem_recs();

    /* --- Prev. frame ME Done Dep Mngr Frm-Frm Mem requests -- */
    total_memtabs_req += i4_num_me_frm_pllel * ihevce_dmgr_get_num_mem_recs();

    /* --- Prev. frame PreEnc L1 Done Dep Mngr Frm-Frm Mem requests -- */
    total_memtabs_req += ihevce_dmgr_get_num_mem_recs();

    /* --- Prev. frame PreEnc HME Done Dep Mngr Frm-Frm Mem requests -- */
    total_memtabs_req += ihevce_dmgr_get_num_mem_recs();

    /* --- Prev. frame PreEnc L0 Done Dep Mngr Frm-Frm Mem requests -- */
    total_memtabs_req += ihevce_dmgr_get_num_mem_recs();

    /* --- ME-Prev Recon Dep Mngr Row-Frm Mem requests -- */
    total_memtabs_req +=
        (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) * ihevce_dmgr_get_num_mem_recs();

    /* ----- allocate memomry for memtabs --- */
    {
        iv_mem_rec_t s_memtab;

        s_memtab.i4_size = sizeof(iv_mem_rec_t);
        s_memtab.i4_mem_size = total_memtabs_req * sizeof(iv_mem_rec_t);
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
        s_memtab.i4_mem_alignment = 4;

        ps_intrf_ctxt->ihevce_mem_alloc(
            ps_intrf_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt->ps_stat_prms->s_sys_api, &s_memtab);
        if(s_memtab.pv_base == NULL)
        {
            ps_intrf_ctxt->i4_error_code = IHEVCE_CANNOT_ALLOCATE_MEMORY;
            return;
        }

        ps_memtab = (iv_mem_rec_t *)s_memtab.pv_base;
    }

    /* --------------------------------------------------------------------- */
    /* ------------------  Collating memory requirements ------------------- */
    /* --------------------------------------------------------------------- */

    /* ----------- Tiles mem requests -------------*/
    total_memtabs_used += ihevce_tiles_get_mem_recs(
        &ps_memtab[total_memtabs_used],
        ps_enc_ctxt->ps_stat_prms,
        &ps_enc_ctxt->s_frm_ctb_prms,
        i4_resolution_id,
        space_for_mem_in_enc_grp);

    /* ---------- Enc loop Mem requests --------- */
    total_memtabs_used += ihevce_enc_loop_get_mem_recs(
        &ps_memtab[total_memtabs_used],
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
        i4_num_bitrate_inst,
        i4_num_enc_loop_frm_pllel,
        space_for_mem_in_enc_grp,
        i4_resolution_id);
    /* ---------- ME Mem requests --------------- */
    total_memtabs_used += ihevce_me_get_mem_recs(
        &ps_memtab[total_memtabs_used],
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
        space_for_mem_in_enc_grp,
        i4_resolution_id,
        i4_num_me_frm_pllel);

    /* ---------- Coarse ME Mem requests --------------- */
    total_memtabs_used += ihevce_coarse_me_get_mem_recs(
        &ps_memtab[total_memtabs_used],
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
        space_for_mem_in_pre_enc_grp,
        i4_resolution_id);
    /* ---------- IPE Mem requests -------------- */
    total_memtabs_used += ihevce_ipe_get_mem_recs(
        &ps_memtab[total_memtabs_used],
        ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
        space_for_mem_in_pre_enc_grp);
    /* ---------- ECD Mem requests -------------- */
    i4_num_bitrate_inst = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
                              .i4_num_bitrate_instances;
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        total_memtabs_used += ihevce_entropy_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            ps_enc_ctxt->ps_stat_prms,
            space_for_mem_in_pre_enc_grp,
            i4_resolution_id);
    }

    if(0 == i4_resolution_id)
    {
        /* ---------- LAP Mem requests--------------- */
        total_memtabs_used +=
            ihevce_lap_get_mem_recs(&ps_memtab[total_memtabs_used], space_for_mem_in_pre_enc_grp);
    }

    /* -------- DECOMPOSITION PRE INTRA Mem requests-------- */
    total_memtabs_used += ihevce_decomp_pre_intra_get_mem_recs(
        &ps_memtab[total_memtabs_used],
        ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
        space_for_mem_in_pre_enc_grp);

    /* ---------- RC Mem requests --------------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        total_memtabs_used += ihevce_rc_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            ps_enc_ctxt->ps_stat_prms,
            space_for_mem_in_pre_enc_grp,
            &ps_enc_ctxt->ps_stat_prms->s_sys_api);
    }

    /* ---------- System Mem requests ----------- */

    /* allocate memory for pps tile */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    if(1 == ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_tiles_enabled_flag)
    {
        ps_memtab[total_memtabs_used].i4_mem_size =
            (ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_num_tile_cols *
             ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_num_tile_rows) *
            (sizeof(tile_t));
    }
    else
    {
        ps_memtab[total_memtabs_used].i4_mem_size = sizeof(tile_t);
    }

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* recon picture buffer pointer array */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) * (sizeof(recon_pic_buf_t *));

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;
    }

    /* recon picture buffers structures */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) * (sizeof(recon_pic_buf_t));

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;
    }

    /* reference/recon picture buffers */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        WORD32 i4_chroma_buf_size_shift =
            -(ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_internal_bit_depth <= 8) +
            (ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV);

        buf_size = ((num_ctb_horz * ctb_size) + (PAD_HORZ << 1));
        buf_size = buf_size * ((num_ctb_vert * ctb_size) + (PAD_VERT << 1));
        buf_size = buf_size * (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS);

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        /* If HBD, both 8bit and 16 bit luma buffers are required, whereas only 16bit chroma buffers are required */
        ps_memtab[total_memtabs_used].i4_mem_size =
            /* Luma */
            (buf_size * ((ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8)
                             ? BUFFER_SIZE_MULTIPLIER_IF_HBD
                             : 1)) +
            /* Chroma */
            (SHL_NEG(buf_size, i4_chroma_buf_size_shift));

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;
    }
    /* reference/recon picture subpel planes */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size =
        buf_size * (3 + L0ME_IN_OPENLOOP_MODE); /* 3 planes */

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;
    /* reference colocated MV bank */
    /* Keep memory for an extra CTB at the right and bottom of frame.
    This extra space is needed by dist-encoding and unused in non-dist-encoding */
    buf_size = (num_ctb_horz + 1) * (num_ctb_vert + 1) * num_pu_in_ctb;
    buf_size = buf_size * sizeof(pu_col_mv_t) * (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) *
               i4_num_bitrate_inst;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* reference colocated MV bank map */
    /* Keep memory for an extra CTB at the right and bottom of frame.
    This extra space is needed by dist-encoding and unused in non-dist-encoding */
    buf_size = (num_ctb_horz + 1) * (num_ctb_vert + 1) * num_pu_in_ctb;
    buf_size = buf_size * sizeof(UWORD8) * (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) *
               i4_num_bitrate_inst;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* reference collocated MV bank map offsets map */
    buf_size = num_ctb_horz * num_ctb_vert;
    buf_size = buf_size * sizeof(UWORD16) * (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) *
               i4_num_bitrate_inst;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* reference colocated MV bank ctb offset */
    buf_size = num_ctb_horz;
    buf_size = buf_size * num_ctb_vert;
    buf_size = buf_size * sizeof(UWORD32) * (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS) *
               i4_num_bitrate_inst;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* recon picture buffer pointer array for pre enc group */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size =
        (max_num_ref_pics + 1) * (sizeof(recon_pic_buf_t *));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* recon picture buffers structures for pre enc group */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = (max_num_ref_pics + 1) * (sizeof(recon_pic_buf_t));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;
    {
        num_input_buf_per_queue = ihevce_lap_get_num_ip_bufs(&ps_enc_ctxt->s_lap_stat_prms);
        {
            WORD32 i4_count_temp = 0, i4_last_queue_length;

            /*First allocate the memory for the buffer based on resolution*/
            WORD32 ctb_align_pic_wd = ps_enc_ctxt->s_runtime_tgt_params.i4_width +
                                      SET_CTB_ALIGN(
                                          ps_enc_ctxt->s_runtime_tgt_params.i4_width,
                                          ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

            WORD32 ctb_align_pic_ht = ps_enc_ctxt->s_runtime_tgt_params.i4_height +
                                      SET_CTB_ALIGN(
                                          ps_enc_ctxt->s_runtime_tgt_params.i4_height,
                                          ps_enc_ctxt->s_frm_ctb_prms.i4_ctb_size);

            i4_last_queue_length = (num_input_buf_per_queue % MAX_QUEUE);

            if((num_input_buf_per_queue % MAX_QUEUE) == 0)
                i4_last_queue_length = MAX_QUEUE;

            ps_enc_ctxt->i4_num_input_buf_per_queue = num_input_buf_per_queue;
            i4_yuv_min_size =
                (ctb_align_pic_wd * ctb_align_pic_ht) +
                ((ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV)
                     ? (ctb_align_pic_wd * ctb_align_pic_ht)
                     : ((ctb_align_pic_wd * ctb_align_pic_ht) >> 1));
            i4_luma_min_size = (ctb_align_pic_wd * ctb_align_pic_ht);

            /*Inorder to allocate memory for the large buffer sizes overflowing WORD32 we are splitting the memtabs using i4_total_hbd_queues and MAX_HBD_QUEUE*/
            i4_total_queues = num_input_buf_per_queue / MAX_QUEUE;

            if((num_input_buf_per_queue % MAX_QUEUE) != 0)
            {
                i4_total_queues++;
            }

            ASSERT(i4_total_queues < 5);

            for(i4_count_temp = 0; i4_count_temp < i4_total_queues; i4_count_temp++)
            {
                ps_memtab[total_memtabs_used].i4_mem_alignment = 32;

                ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;
                /*Memory size for yuv buffer of one frame * num of input required to stored in the queue*/
                if((i4_count_temp < (i4_total_queues - 1)))
                    ps_memtab[total_memtabs_used].i4_mem_size = i4_yuv_min_size * MAX_QUEUE;
                else
                    ps_memtab[total_memtabs_used].i4_mem_size =
                        (i4_yuv_min_size)*i4_last_queue_length;

                /* increment the memtab counter */
                total_memtabs_used++;
                total_system_memtabs++;
            }
        }
        /*memory for input buffer structure*/
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (num_input_buf_per_queue) * (sizeof(ihevce_lap_enc_buf_t *));

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* frame process/entropy coding buffer structures */
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (num_input_buf_per_queue) * (sizeof(ihevce_lap_enc_buf_t));
        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /*input synch ctrl command*/
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (num_input_buf_per_queue) * (ENC_COMMAND_BUFF_SIZE);

        total_memtabs_used++;
        total_system_memtabs++;
    }

    /* Pre-encode/encode coding buffer pointer array */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size =
        (num_bufs_preenc_me_que) * (sizeof(pre_enc_me_ctxt_t *));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* frame process/entropy coding buffer structures */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size =
        (num_bufs_preenc_me_que) * (sizeof(pre_enc_me_ctxt_t));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* Pre-encode L0 IPE output to ME buffer pointer*/
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size =
        (num_bufs_L0_ipe_enc) * (sizeof(pre_enc_L0_ipe_encloop_ctxt_t *));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* Pre-encode L0 IPE output to ME buffer */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size =
        (num_bufs_L0_ipe_enc) * (sizeof(pre_enc_L0_ipe_encloop_ctxt_t));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* CTB analyse Frame level  */
    buf_size = num_ctb_horz;
    buf_size = buf_size * num_ctb_vert;
    buf_size = buf_size * sizeof(ctb_analyse_t) * num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* ME layer ctxt pointer */
    buf_size = sizeof(layer_ctxt_t) * num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* ME layer MV bank ctxt pointer */
    buf_size = sizeof(layer_mv_t) * num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* ME layer MV bank pointer */
    buf_size = mv_bank_size * num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* ME layer ref idx bank pointer */
    buf_size = ref_idx_bank_size * num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;
    /* Frame level array to store 8x8 intra cost */
    buf_size = (num_ctb_horz * ctb_size) >> 3;
    buf_size *= ((num_ctb_vert * ctb_size) >> 3);
    buf_size *= sizeof(double) * num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* Frame level array to store ctb intra cost and modes */
    buf_size = (num_ctb_horz * num_ctb_vert);
    buf_size *= sizeof(ipe_l0_ctb_analyse_for_me_t) * num_bufs_L0_ipe_enc;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /*
    * Layer early decision buffer L1 buf.Since the pre intra analysis always
    * expects memory for ihevce_ed_blk_t for complete ctbs, align the width and
    * height in layer to mutiple of 32.
    */
    buf_size = (a_ctb_align_wd[1] >> 5) * (a_ctb_align_ht[1] >> 5) * sizeof(ihevce_ed_ctb_l1_t) *
               num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_pre_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /*
    * Layer early decision buffer L1 buf.Since the pre intra analysis always
    * expects memory for ihevce_ed_blk_t for complete ctbs, align the width and
    * height in layer to mutiple of 32.
    */
    buf_size = (a_ctb_align_wd[1] >> 2) * (a_ctb_align_ht[1] >> 2) * sizeof(ihevce_ed_blk_t) *
               num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_pre_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /*
    * Layer early decision buffer L2 buf.Since the pre intra analysis always
    * expects memory for ihevce_ed_blk_t for complete ctbs, align the width and
    * height in layer to mutiple of 16.
    */
    buf_size = (a_ctb_align_wd[2] >> 2) * (a_ctb_align_ht[2] >> 2) * sizeof(ihevce_ed_blk_t) *
               num_bufs_preenc_me_que;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_pre_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* following is the buffer requirement of
    que between me and enc*/

    /* me/enc que buffer pointer array */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = (NUM_ME_ENC_BUFS) * (sizeof(me_enc_rdopt_ctxt_t *));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* fme/enc que buffer structures */
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = (NUM_ME_ENC_BUFS) * (sizeof(me_enc_rdopt_ctxt_t));

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* Job Queue related memory                            */
    /* max num ctb rows is doubled to take care worst case */
    /* requirements because of HME layers                  */
    buf_size = (MAX_NUM_VERT_UNITS_FRM) * (NUM_ENC_JOBS_QUES)*NUM_ME_ENC_BUFS;  //PING_PONG_BUF;
    /* In tile case, based on the number of column tiles,
    we will have  separate jobQ per column tile        */
    if(1 == ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_tiles_enabled_flag)
    {
        buf_size *= ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_num_tile_cols;
    }
    buf_size *= sizeof(job_queue_t);
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* cur_ctb_cu_tree_t Frame level  */
    buf_size = num_ctb_horz * MAX_NUM_NODES_CU_TREE;
    buf_size = buf_size * num_ctb_vert;

    /* ps_cu_analyse_inter buffer is used to popualte outputs form ME after using cu analyse form IPE */
    buf_size = buf_size * sizeof(cur_ctb_cu_tree_t) * NUM_ME_ENC_BUFS;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* me_ctb_data_t Frame level  */
    buf_size = num_ctb_horz * num_ctb_vert;

    /* This buffer is used to */
    buf_size = buf_size * sizeof(me_ctb_data_t) * NUM_ME_ENC_BUFS;

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* following is for each bit-rate */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        /* frame process/entropy coding buffer pointer array */
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (NUM_FRMPROC_ENTCOD_BUFS) * (sizeof(frm_proc_ent_cod_ctxt_t *));

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* frame process/entropy coding buffer structures */
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size =
            (NUM_FRMPROC_ENTCOD_BUFS) * (sizeof(frm_proc_ent_cod_ctxt_t));

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* CTB enc loop Frame level  */
        buf_size = num_ctb_horz;
        buf_size = buf_size * num_ctb_vert;
        buf_size = buf_size * sizeof(ctb_enc_loop_out_t) * NUM_FRMPROC_ENTCOD_BUFS;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* CU enc loop Frame level  */
        buf_size = num_ctb_horz * num_cu_in_ctb;
        buf_size = buf_size * num_ctb_vert;
        buf_size = buf_size * sizeof(cu_enc_loop_out_t) * NUM_FRMPROC_ENTCOD_BUFS;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* TU enc loop Frame level  */
        buf_size = num_ctb_horz * num_tu_in_ctb;
        buf_size = buf_size * num_ctb_vert;
        buf_size = buf_size * sizeof(tu_enc_loop_out_t) * NUM_FRMPROC_ENTCOD_BUFS;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* PU enc loop Frame level  */
        buf_size = num_ctb_horz * num_pu_in_ctb;
        buf_size = buf_size * num_ctb_vert;
        buf_size = buf_size * sizeof(pu_t) * NUM_FRMPROC_ENTCOD_BUFS;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* Coeffs Frame level  */
        buf_size =
            num_ctb_horz * ((ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV)
                                ? (num_tu_in_ctb << 1)
                                : ((num_tu_in_ctb * 3) >> 1));
        buf_size = buf_size * num_ctb_vert;
        buf_size = buf_size * sizeof(UWORD8) * MAX_SCAN_COEFFS_BYTES_4x4 * NUM_FRMPROC_ENTCOD_BUFS;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;

        /* SEI Payload Data */
        buf_size = sizeof(UWORD8) * MAX_NUMBER_OF_SEI_PAYLOAD * MAX_SEI_PAYLOAD_PER_TLV *
                   NUM_FRMPROC_ENTCOD_BUFS;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;
        ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

        ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

        /* increment the memtab counter */
        total_memtabs_used++;
        total_system_memtabs++;
    }

    /* ------ Working mem frame level -------*/
    buf_size = ((num_ctb_horz * ctb_size) + 16);
    buf_size *= ((num_ctb_vert * ctb_size) + 23);
    buf_size *= sizeof(WORD16);
    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;
    /* Job Queue related memory                            */
    /* max num ctb rows is doubled to take care worst case */
    /* requirements because of HME layers                  */
    buf_size = (MAX_NUM_VERT_UNITS_FRM) * (NUM_PRE_ENC_JOBS_QUES) * (max_delay_preenc_l0_que);
    buf_size *= sizeof(job_queue_t);

    ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

    ps_memtab[total_memtabs_used].e_mem_type = (IV_MEM_TYPE_T)space_for_mem_in_enc_grp;

    ps_memtab[total_memtabs_used].i4_mem_size = buf_size;

    /* increment the memtab counter */
    total_memtabs_used++;
    total_system_memtabs++;

    /* check on the system memtabs */
    ASSERT(total_system_memtabs <= TOTAL_SYSTEM_MEM_RECS);

    /* -----Frameproc Entcod Que Mem requests --- */
    /*  derive for each bit-rate */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        total_memtabs_used += ihevce_buff_que_get_mem_recs(
            &ps_memtab[total_memtabs_used], NUM_FRMPROC_ENTCOD_BUFS, space_for_mem_in_enc_grp);
    }
    /*mrs: Request memory for the input yuv queue*/
    total_memtabs_used += ihevce_buff_que_get_mem_recs(
        &ps_memtab[total_memtabs_used], num_input_buf_per_queue, space_for_mem_in_enc_grp);
    /*------ The encoder owned input buffer queue*/
    /* -----Pre-encode Encode Que Mem requests --- */
    total_memtabs_used += ihevce_buff_que_get_mem_recs(
        &ps_memtab[total_memtabs_used], num_bufs_preenc_me_que, space_for_mem_in_enc_grp);

    /* -----ME / Enc-RD opt Que Mem requests --- */
    total_memtabs_used += ihevce_buff_que_get_mem_recs(
        &ps_memtab[total_memtabs_used], NUM_ME_ENC_BUFS, space_for_mem_in_enc_grp);

    /* -----Pre-encode L0 IPE to enc Que Mem requests --- */
    total_memtabs_used += ihevce_buff_que_get_mem_recs(
        &ps_memtab[total_memtabs_used], num_bufs_L0_ipe_enc, space_for_mem_in_enc_grp);

    /* ---------- Dependency Manager allocations -------- */
    {
        /* --- ME-EncLoop Dep Mngr Row-Row Mem requests -- */
        for(ctr = 0; ctr < NUM_ME_ENC_BUFS; ctr++)
        {
            total_memtabs_used += ihevce_dmgr_get_mem_recs(
                &ps_memtab[total_memtabs_used],
                DEP_MNGR_ROW_ROW_SYNC,
                (a_ctb_align_ht[0] / ctb_size),
                ps_enc_ctxt->ps_stat_prms->s_app_tile_params
                    .i4_num_tile_cols, /* Number of Col Tiles */
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                space_for_mem_in_enc_grp);
        }

        for(ctr = 0; ctr < i4_num_enc_loop_frm_pllel; ctr++)
        {
            /* --- Prev. frame EncLoop Done Dep Mngr Frm-Frm Mem requests -- */
            total_memtabs_used += ihevce_dmgr_get_mem_recs(
                &ps_memtab[total_memtabs_used],
                DEP_MNGR_FRM_FRM_SYNC,
                (a_ctb_align_ht[0] / ctb_size),
                1, /* Number of Col Tiles : Don't care for FRM_FRM */
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                space_for_mem_in_enc_grp);
        }
        /* --- Prev. frame EncLoop Done for re-encode Dep Mngr Frm-Frm Mem requests -- */
        total_memtabs_used += ihevce_dmgr_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
            space_for_mem_in_enc_grp);
        for(ctr = 0; ctr < i4_num_me_frm_pllel; ctr++)
        {
            /* --- Prev. frame ME Done Dep Mngr Frm-Frm Mem requests -- */
            total_memtabs_used += ihevce_dmgr_get_mem_recs(
                &ps_memtab[total_memtabs_used],
                DEP_MNGR_FRM_FRM_SYNC,
                (a_ctb_align_ht[0] / ctb_size),
                1, /* Number of Col Tiles : Don't care for FRM_FRM */
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                space_for_mem_in_enc_grp);
        }

        /* --- Prev. frame PreEnc L1 Done Dep Mngr Frm-Frm Mem requests -- */
        total_memtabs_used += ihevce_dmgr_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
            space_for_mem_in_enc_grp);

        /* --- Prev. frame PreEnc HME Done Dep Mngr Frm-Frm Mem requests -- */
        total_memtabs_used += ihevce_dmgr_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
            space_for_mem_in_enc_grp);

        /* --- Prev. frame PreEnc L0 Done Dep Mngr Frm-Frm Mem requests -- */
        total_memtabs_used += ihevce_dmgr_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
            space_for_mem_in_enc_grp);

        /* --- ME-Prev Recon Dep Mngr Row-Frm Mem requests -- */
        for(ctr = 0; ctr < (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS); ctr++)
        {
            WORD32 i4_num_units = num_ctb_horz * num_ctb_vert;

            total_memtabs_used += ihevce_dmgr_map_get_mem_recs(
                &ps_memtab[total_memtabs_used],
                i4_num_units,
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                space_for_mem_in_enc_grp);
        }
    }

    /* ----- allocate memory as per requests ---- */

    /* check on memtabs requested v/s memtabs used */
    //ittiam : should put an assert

    //ASSERT(total_memtabs_used == total_memtabs_req);

    for(ctr = 0; ctr < total_memtabs_used; ctr++)
    {
        UWORD8 *pu1_mem = NULL;
        ps_intrf_ctxt->ihevce_mem_alloc(
            ps_intrf_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt->ps_stat_prms->s_sys_api, &ps_memtab[ctr]);

        pu1_mem = (UWORD8 *)ps_memtab[ctr].pv_base;

        if(NULL == pu1_mem)
        {
            ps_intrf_ctxt->i4_error_code = IHEVCE_CANNOT_ALLOCATE_MEMORY;
            return;
        }

        memset(pu1_mem, 0, ps_memtab[ctr].i4_mem_size);
    }

    /* --------------------------------------------------------------------- */
    /* --------- Initialisation of Modules & System memory ----------------- */
    /* --------------------------------------------------------------------- */

    /* store the final allocated memtabs */
    ps_enc_ctxt->s_mem_mngr.i4_num_create_memtabs = total_memtabs_used;
    ps_enc_ctxt->s_mem_mngr.ps_create_memtab = ps_memtab;

    /* ---------- Tiles Mem init --------- */
    ps_enc_ctxt->ps_tile_params_base = (ihevce_tile_params_t *)ihevce_tiles_mem_init(
        ps_memtab, ps_enc_ctxt->ps_stat_prms, ps_enc_ctxt, i4_resolution_id);

    ps_memtab += ihevce_tiles_get_num_mem_recs();

    /* ---------- Enc loop Mem init --------- */
    ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt = ihevce_enc_loop_init(
        ps_memtab,
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
        ps_intrf_ctxt->pv_osal_handle,
        &ps_enc_ctxt->s_func_selector,
        &ps_enc_ctxt->s_rc_quant,
        ps_enc_ctxt->ps_tile_params_base,
        i4_resolution_id,
        i4_num_enc_loop_frm_pllel,
        ps_enc_ctxt->u1_is_popcnt_available);

    ps_memtab += ihevce_enc_loop_get_num_mem_recs(i4_num_bitrate_inst, i4_num_enc_loop_frm_pllel);
    /* ---------- ME Mem init --------------- */
    ps_enc_ctxt->s_module_ctxt.pv_me_ctxt = ihevce_me_init(
        ps_memtab,
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
        ps_intrf_ctxt->pv_osal_handle,
        &ps_enc_ctxt->s_rc_quant,
        (void *)ps_enc_ctxt->ps_tile_params_base,
        i4_resolution_id,
        i4_num_me_frm_pllel,
        ps_enc_ctxt->u1_is_popcnt_available);

    ps_memtab += ihevce_me_get_num_mem_recs(i4_num_me_frm_pllel);

    /* ---------- Coarse ME Mem init --------------- */
    ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt = ihevce_coarse_me_init(
        ps_memtab,
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
        ps_intrf_ctxt->pv_osal_handle,
        i4_resolution_id,
        ps_enc_ctxt->u1_is_popcnt_available);

    ps_memtab += ihevce_coarse_me_get_num_mem_recs();
    /* ---------- IPE Mem init -------------- */
    ps_enc_ctxt->s_module_ctxt.pv_ipe_ctxt = ihevce_ipe_init(
        ps_memtab,
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
        ps_enc_ctxt->i4_ref_mbr_id,
        &ps_enc_ctxt->s_func_selector,
        &ps_enc_ctxt->s_rc_quant,
        i4_resolution_id,
        ps_enc_ctxt->u1_is_popcnt_available);

    ps_memtab += ihevce_ipe_get_num_mem_recs();

    ps_enc_ctxt->s_rc_quant.i2_max_qp = 51;
    ps_enc_ctxt->s_rc_quant.i2_min_qp = 0;
    ps_enc_ctxt->s_rc_quant.i1_qp_offset = 0;
    ps_enc_ctxt->s_rc_quant.i2_max_qscale =
        228 << 3;  // Q3 format is mantained for accuarate calc at lower qp
    ps_enc_ctxt->s_rc_quant.i2_min_qscale = 1;

    /* ---------- ECD Mem init -------------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        ps_enc_ctxt->s_module_ctxt.apv_ent_cod_ctxt[i] = ihevce_entropy_init(
            ps_memtab,
            ps_enc_ctxt->ps_stat_prms,
            (void *)ps_enc_ctxt->ps_tile_params_base,
            i4_resolution_id);

        ps_memtab += ihevce_entropy_get_num_mem_recs();
    }

    /* ---------- LAP Mem init--------------- */
    if(i4_resolution_id == 0)
    {
        ps_enc_ctxt->s_module_ctxt.pv_lap_ctxt =
            ihevce_lap_init(ps_memtab, &ps_enc_ctxt->s_lap_stat_prms, ps_enc_ctxt->ps_stat_prms);

        ps_memtab += ihevce_lap_get_num_mem_recs();
    }
    /*-----------DECOMPOSITION PRE INTRA init----*/
    ps_enc_ctxt->s_module_ctxt.pv_decomp_pre_intra_ctxt = ihevce_decomp_pre_intra_init(
        ps_memtab,
        ps_enc_ctxt->ps_stat_prms,
        ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
        &ps_enc_ctxt->s_func_selector,
        i4_resolution_id,
        ps_enc_ctxt->u1_is_popcnt_available);

    ps_memtab += ihevce_decomp_pre_intra_get_num_mem_recs();

    /* ---------- RC Mem init --------------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        /*swaping of buf_id for 0th and reference bitrate location, as encoder
        assumes always 0th loc for reference bitrate and app must receive in
        the configured order*/
        if(i == 0)
        {
            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i] = ihevce_rc_mem_init(
                ps_memtab,
                ps_enc_ctxt->ps_stat_prms,
                ps_enc_ctxt->i4_ref_mbr_id,
                &ps_enc_ctxt->s_rc_quant,
                ps_enc_ctxt->i4_resolution_id,
                ps_enc_ctxt->i4_look_ahead_frames_in_first_pass);
        }
        else if(i == ps_enc_ctxt->i4_ref_mbr_id)
        {
            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i] = ihevce_rc_mem_init(
                ps_memtab,
                ps_enc_ctxt->ps_stat_prms,
                0,
                &ps_enc_ctxt->s_rc_quant,
                ps_enc_ctxt->i4_resolution_id,
                ps_enc_ctxt->i4_look_ahead_frames_in_first_pass);
        }
        else
        {
            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i] = ihevce_rc_mem_init(
                ps_memtab,
                ps_enc_ctxt->ps_stat_prms,
                i,
                &ps_enc_ctxt->s_rc_quant,
                ps_enc_ctxt->i4_resolution_id,
                ps_enc_ctxt->i4_look_ahead_frames_in_first_pass);
        }
        ps_memtab += ihevce_rc_get_num_mem_recs();
    }

    /* ---------- System Mem init ----------- */
    {
        recon_pic_buf_t **pps_pic_bufs[IHEVCE_MAX_NUM_BITRATES];
        recon_pic_buf_t *ps_pic_bufs[IHEVCE_MAX_NUM_BITRATES];
        void *pv_recon_buf[IHEVCE_MAX_NUM_BITRATES];
#if(SRC_PADDING_FOR_TRAQO || ENABLE_SSD_CALC_RC)
        void *pv_recon_buf_source[IHEVCE_MAX_NUM_BITRATES] = { NULL };
#endif
        void *pv_uv_recon_buf[IHEVCE_MAX_NUM_BITRATES];
        UWORD8 *pu1_subpel_buf;
        pu_col_mv_t *ps_col_mv;
        UWORD8 *pu1_col_mv_map;
        UWORD16 *pu2_col_num_pu_map;
        UWORD32 *pu4_col_mv_off;
        WORD32 luma_frm_size;
        WORD32 recon_stride; /* stride for Y and UV(interleave) */
        WORD32 luma_frm_height; /* including padding    */
        WORD32 num_pu_in_frm;

        /* pps tile memory */
        for(i = 0; i < i4_num_bitrate_inst; i++)
        {
            ps_enc_ctxt->as_pps[i].ps_tile = (tile_t *)ps_memtab->pv_base;
        }

        ps_memtab++; /* increment the memtabs */

        /* recon picture buffer pointer array */
        for(i = 0; i < i4_num_bitrate_inst; i++)
        {
            pps_pic_bufs[i] = (recon_pic_buf_t **)ps_memtab->pv_base;
            ps_memtab++; /* increment the memtabs */
        }

        /* recon picture buffers structures */
        for(i = 0; i < i4_num_bitrate_inst; i++)
        {
            ps_pic_bufs[i] = (recon_pic_buf_t *)ps_memtab->pv_base;
            ps_memtab++; /* increment the memtabs */
        }

        /* reference/recon picture buffers */
        for(i = 0; i < i4_num_bitrate_inst; i++)
        {
            pv_recon_buf[i] = ps_memtab->pv_base;
            ps_memtab++; /* increment the memtabs */
        }
        /* reference/recon picture subpel planes */
        pu1_subpel_buf = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;
        /* reference colocated MV bank */
        ps_col_mv = (pu_col_mv_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* reference colocated MV bank map */
        pu1_col_mv_map = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* reference collocated MV bank map offsets map */
        pu2_col_num_pu_map = (UWORD16 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* reference colocated MV bank ctb offset */
        pu4_col_mv_off = (UWORD32 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* compute the stride and frame height after accounting for padding */
        recon_stride = ((num_ctb_horz * ctb_size) + (PAD_HORZ << 1));
        luma_frm_height = ((num_ctb_vert * ctb_size) + (PAD_VERT << 1));
        luma_frm_size = recon_stride * luma_frm_height;
        /* The subpel buffer is also incremented to take care of padding */
        /* Both luma and subpel buffer use same stride                   */
        pu1_subpel_buf += (recon_stride * PAD_VERT);
        pu1_subpel_buf += PAD_HORZ;

        /* Keep memory for an extra CTB at the right and bottom of frame.
        This extra space is needed by dist-encoding and unused in non-dist-encoding */
        num_pu_in_frm = (num_ctb_horz + 1) * num_pu_in_ctb * (num_ctb_vert + 1);

        for(i = 0; i < i4_num_bitrate_inst; i++)
        {
            pv_uv_recon_buf[i] = pv_recon_buf[i];

            /* increment the recon buffer to take care of padding */
            pv_recon_buf[i] = (UWORD8 *)pv_recon_buf[i] + (recon_stride * PAD_VERT) + PAD_HORZ;

            /* chroma buffer starts at the end of luma buffer */
            pv_uv_recon_buf[i] = (UWORD8 *)pv_uv_recon_buf[i] + luma_frm_size;
            if(ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_internal_bit_depth == 8)
            {
                /* increment the chroma recon buffer to take care of padding    */
                /* vert padding halved but horiz is same due to uv interleave   */
                pv_uv_recon_buf[i] =
                    (UWORD8 *)pv_uv_recon_buf[i] + (recon_stride * (PAD_VERT >> 1)) +
                    ((ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV)
                         ? (recon_stride * (PAD_VERT >> 1))
                         : 0);
                pv_uv_recon_buf[i] = (UWORD8 *)pv_uv_recon_buf[i] + PAD_HORZ;
            }

            /* loop to initialise all the memories */
            /* initialize recon buffers */
            /* only YUV buffers are allocated for each bit-rate instnaces.
            Subpel buffers and col buffers are made NULL for auxiliary bit-rate instances,
            since ME and IPE happens only for reference bit-rate instnace */
            for(ctr = 0; ctr < (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS); ctr++)
            {
                pps_pic_bufs[i][ctr] =
                    ps_pic_bufs[i];  //check the index of pps [i] should be first or last index?!!

                ps_pic_bufs[i]->s_yuv_buf_desc.i4_size = sizeof(iv_enc_yuv_buf_t);
                ps_pic_bufs[i]->s_yuv_buf_desc.pv_y_buf = pv_recon_buf[i];
                ps_pic_bufs[i]->s_yuv_buf_desc.pv_v_buf = NULL;
                {
                    ps_pic_bufs[i]->s_yuv_buf_desc.pv_u_buf = pv_uv_recon_buf[i];
                }
                ps_pic_bufs[i]->apu1_y_sub_pel_planes[0] = ((i == 0) ? pu1_subpel_buf : NULL);
                ps_pic_bufs[i]->apu1_y_sub_pel_planes[1] =
                    ((i == 0) ? (pu1_subpel_buf + luma_frm_size) : NULL);
                ps_pic_bufs[i]->apu1_y_sub_pel_planes[2] =
                    ((i == 0) ? (pu1_subpel_buf + (luma_frm_size * 2)) : NULL);
                ps_pic_bufs[i]->ps_frm_col_mv = ps_col_mv;
                ps_pic_bufs[i]->pu1_frm_pu_map = pu1_col_mv_map;
                ps_pic_bufs[i]->pu2_num_pu_map = pu2_col_num_pu_map;
                ps_pic_bufs[i]->pu4_pu_off = pu4_col_mv_off;
                ps_pic_bufs[i]->i4_is_free = 1;
                ps_pic_bufs[i]->i4_poc = -1;
                ps_pic_bufs[i]->i4_display_num = -1;
                ps_pic_bufs[i]->i4_buf_id = ctr;

                /* frame level buff increments */
                ps_col_mv += num_pu_in_frm;
                pu1_col_mv_map += num_pu_in_frm;
                pu2_col_num_pu_map += (num_ctb_horz * num_ctb_vert);
                pu4_col_mv_off += (num_ctb_horz * num_ctb_vert);

                if(ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV)
                {
                    pv_recon_buf[i] = (UWORD8 *)pv_recon_buf[i] + (luma_frm_size << 1);
                    pv_uv_recon_buf[i] = (UWORD8 *)pv_uv_recon_buf[i] + (luma_frm_size << 1);
                }
                else
                {
                    pv_recon_buf[i] = (UWORD8 *)pv_recon_buf[i] + ((3 * luma_frm_size) >> 1);
                    pv_uv_recon_buf[i] = (UWORD8 *)pv_uv_recon_buf[i] + ((3 * luma_frm_size) >> 1);
                }
                pu1_subpel_buf += ((3 + L0ME_IN_OPENLOOP_MODE) * luma_frm_size); /* 3 planes */
                ps_pic_bufs[i]++;
            }  //ctr ends

            /* store the queue pointer and num buffs to context */
            ps_enc_ctxt->pps_recon_buf_q[i] = pps_pic_bufs[i];
            ps_enc_ctxt->ai4_num_buf_recon_q[i] = (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS);

        }  //bitrate ctr ends

    }  //end of system memory init

    /* Pre encode group recon buffer  containier NO Buffers will be allocated / used */
    {
        recon_pic_buf_t *ps_pic_bufs;

        /* recon picture buffer pointer array */
        pps_pre_enc_pic_bufs = (recon_pic_buf_t **)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* recon picture buffers structures */
        ps_pic_bufs = (recon_pic_buf_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* loop to initialise all the memories */
        for(ctr = 0; ctr < (max_num_ref_pics + 1); ctr++)
        {
            pps_pre_enc_pic_bufs[ctr] = ps_pic_bufs;

            ps_pic_bufs->s_yuv_buf_desc.i4_size = sizeof(iv_enc_yuv_buf_t);
            ps_pic_bufs->s_yuv_buf_desc.pv_y_buf = NULL;
            ps_pic_bufs->s_yuv_buf_desc.pv_u_buf = NULL;
            ps_pic_bufs->s_yuv_buf_desc.pv_v_buf = NULL;
            ps_pic_bufs->apu1_y_sub_pel_planes[0] = NULL;
            ps_pic_bufs->apu1_y_sub_pel_planes[1] = NULL;
            ps_pic_bufs->apu1_y_sub_pel_planes[2] = NULL;
            ps_pic_bufs->ps_frm_col_mv = NULL;
            ps_pic_bufs->pu1_frm_pu_map = NULL;
            ps_pic_bufs->pu2_num_pu_map = NULL;
            ps_pic_bufs->pu4_pu_off = NULL;
            ps_pic_bufs->i4_is_free = 1;
            ps_pic_bufs->i4_poc = -1;
            ps_pic_bufs->i4_buf_id = ctr;

            /* frame level buff increments */
            ps_pic_bufs++;
        }

        /* store the queue pointer and num buffs to context */
        ps_enc_ctxt->pps_pre_enc_recon_buf_q = pps_pre_enc_pic_bufs;
        ps_enc_ctxt->i4_pre_enc_num_buf_recon_q = (max_num_ref_pics + 1);
    }

    /* Frame level buffers and Que between pre-encode & encode */
    {
        pre_enc_me_ctxt_t *ps_pre_enc_bufs;
        pre_enc_L0_ipe_encloop_ctxt_t *ps_L0_ipe_enc_bufs;
        ihevce_lap_enc_buf_t *ps_lap_enc_input_buf;
        ctb_analyse_t *ps_ctb_analyse;
        UWORD8 *pu1_me_lyr_ctxt;
        UWORD8 *pu1_me_lyr_bank_ctxt;
        UWORD8 *pu1_mv_bank;
        UWORD8 *pu1_ref_idx_bank;
        double *plf_intra_8x8_cost;
        ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse_ctb;
        ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;
        ihevce_ed_blk_t *ps_layer1_buf;
        ihevce_ed_blk_t *ps_layer2_buf;
        UWORD8 *pu1_lap_input_yuv_buf[4];
        UWORD8 *pu1_input_synch_ctrl_cmd;
        WORD32 i4_count = 0;
        /*initialize the memory for input buffer*/
        {
            for(i4_count = 0; i4_count < i4_total_queues; i4_count++)
            {
                pu1_lap_input_yuv_buf[i4_count] = (UWORD8 *)ps_memtab->pv_base;
                /* increment the memtabs */
                ps_memtab++;
            }
            pps_lap_enc_input_bufs = (ihevce_lap_enc_buf_t **)ps_memtab->pv_base;
            /* increment the memtabs */
            ps_memtab++;

            /*memory for the input buffer structure*/
            ps_lap_enc_input_buf = (ihevce_lap_enc_buf_t *)ps_memtab->pv_base;
            ps_memtab++;

            pu1_input_synch_ctrl_cmd = (UWORD8 *)ps_memtab->pv_base;
            ps_memtab++;
        }
        /* pre encode /encode coding buffer pointer array */
        pps_pre_enc_bufs = (pre_enc_me_ctxt_t **)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* pre encode /encode buffer structure */
        ps_pre_enc_bufs = (pre_enc_me_ctxt_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /*  Pre-encode L0 IPE output to ME buffer pointer */
        pps_L0_ipe_enc_bufs = (pre_enc_L0_ipe_encloop_ctxt_t **)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* Pre-encode L0 IPE output to ME buffer */
        ps_L0_ipe_enc_bufs = (pre_enc_L0_ipe_encloop_ctxt_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* CTB analyse Frame level  */
        ps_ctb_analyse = (ctb_analyse_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* ME layer ctxt Frame level  */
        pu1_me_lyr_ctxt = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* ME layer bank ctxt Frame level  */
        pu1_me_lyr_bank_ctxt = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* ME layer MV bank Frame level  */
        pu1_mv_bank = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* ME layer ref idx bank Frame level  */
        pu1_ref_idx_bank = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;
        /* 8x8 intra costs for entire frame */
        plf_intra_8x8_cost = (double *)ps_memtab->pv_base;
        ps_memtab++;

        /* ctb intra costs and modes for entire frame */
        ps_ipe_analyse_ctb = (ipe_l0_ctb_analyse_for_me_t *)ps_memtab->pv_base;
        ps_memtab++;

        /*Contains ctb level information at pre-intra stage */
        ps_ed_ctb_l1 = (ihevce_ed_ctb_l1_t *)ps_memtab->pv_base;
        ps_memtab++;

        /* Layer L1 buf */
        ps_layer1_buf = (ihevce_ed_blk_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* Layer2 buf */
        ps_layer2_buf = (ihevce_ed_blk_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* loop to initialise all the memories*/
        /*mrs: assign individual input yuv frame pointers here*/

        i4_count = 0;
        /* loop to initialise the buffer pointer */
        for(ctr = 0; ctr < num_input_buf_per_queue; ctr++)
        {
            pps_lap_enc_input_bufs[ctr] = &ps_lap_enc_input_buf[ctr];

            pps_lap_enc_input_bufs[ctr]->s_input_buf.i4_size = sizeof(iv_input_data_ctrl_buffs_t);

            pps_lap_enc_input_bufs[ctr]->s_input_buf.pv_synch_ctrl_bufs = pu1_input_synch_ctrl_cmd;

            pps_lap_enc_input_bufs[ctr]->s_input_buf.s_input_buf.i4_size = sizeof(iv_yuv_buf_t);

            pu1_input_synch_ctrl_cmd += ENC_COMMAND_BUFF_SIZE;
            /*pointer to i/p buf initialised to null in case of run time allocation*/

            {
                pps_lap_enc_input_bufs[ctr]->s_lap_out.s_input_buf.pv_y_buf =
                    pu1_lap_input_yuv_buf[i4_count];

                pps_lap_enc_input_bufs[ctr]->s_lap_out.s_input_buf.pv_u_buf =
                    pu1_lap_input_yuv_buf[i4_count] + i4_luma_min_size;

                pps_lap_enc_input_bufs[ctr]->s_lap_out.s_input_buf.pv_v_buf =
                    NULL; /*since yuv 420 format*/

                pu1_lap_input_yuv_buf[i4_count] += i4_yuv_min_size;

                if(((ctr + 1) % MAX_QUEUE) == 0)
                    i4_count++;
            }
        }
        for(ctr = 0; ctr < num_bufs_preenc_me_que; ctr++)
        {
            pps_pre_enc_bufs[ctr] = ps_pre_enc_bufs;

            ps_pre_enc_bufs->ps_ctb_analyse = ps_ctb_analyse;
            ps_pre_enc_bufs->pv_me_lyr_ctxt = (void *)pu1_me_lyr_ctxt;
            ps_pre_enc_bufs->pv_me_lyr_bnk_ctxt = (void *)pu1_me_lyr_bank_ctxt;
            ps_pre_enc_bufs->pv_me_mv_bank = (void *)pu1_mv_bank;
            ps_pre_enc_bufs->pv_me_ref_idx = (void *)pu1_ref_idx_bank;
            ps_pre_enc_bufs->ps_layer1_buf = ps_layer1_buf;
            ps_pre_enc_bufs->ps_layer2_buf = ps_layer2_buf;
            ps_pre_enc_bufs->ps_ed_ctb_l1 = ps_ed_ctb_l1;
            ps_pre_enc_bufs->plf_intra_8x8_cost = plf_intra_8x8_cost;

            ps_ctb_analyse += num_ctb_horz * num_ctb_vert;
            pu1_me_lyr_ctxt += sizeof(layer_ctxt_t);
            pu1_me_lyr_bank_ctxt += sizeof(layer_mv_t);
            pu1_mv_bank += mv_bank_size;
            pu1_ref_idx_bank += ref_idx_bank_size;
            plf_intra_8x8_cost +=
                (((num_ctb_horz * ctb_size) >> 3) * ((num_ctb_vert * ctb_size) >> 3));
            ps_ed_ctb_l1 += (a_ctb_align_wd[1] >> 5) * (a_ctb_align_ht[1] >> 5);
            ps_layer1_buf += (a_ctb_align_wd[1] >> 2) * (a_ctb_align_ht[1] >> 2);
            ps_layer2_buf += (a_ctb_align_wd[2] >> 2) * (a_ctb_align_ht[2] >> 2);
            ps_pre_enc_bufs++;
        }

        for(ctr = 0; ctr < num_bufs_L0_ipe_enc; ctr++)
        {
            pps_L0_ipe_enc_bufs[ctr] = ps_L0_ipe_enc_bufs;
            ps_L0_ipe_enc_bufs->ps_ipe_analyse_ctb = ps_ipe_analyse_ctb;
            ps_ipe_analyse_ctb += num_ctb_horz * num_ctb_vert;
            ps_L0_ipe_enc_bufs++;
        }
    }

    /* Frame level que between ME and Enc rd-opt */
    {
        me_enc_rdopt_ctxt_t *ps_me_enc_bufs;
        job_queue_t *ps_job_q_enc;
        me_ctb_data_t *ps_cur_ctb_me_data;
        cur_ctb_cu_tree_t *ps_cur_ctb_cu_tree;

        /* pre encode /encode coding buffer pointer array */
        pps_me_enc_bufs = (me_enc_rdopt_ctxt_t **)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* pre encode /encode buffer structure */
        ps_me_enc_bufs = (me_enc_rdopt_ctxt_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /*me and enc job queue memory */
        ps_job_q_enc = (job_queue_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /*ctb me data memory*/
        ps_cur_ctb_cu_tree = (cur_ctb_cu_tree_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /*ctb me data memory*/
        ps_cur_ctb_me_data = (me_ctb_data_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* loop to initialise all the memories */
        for(ctr = 0; ctr < NUM_ME_ENC_BUFS; ctr++)
        {
            pps_me_enc_bufs[ctr] = ps_me_enc_bufs;

            ps_me_enc_bufs->ps_job_q_enc = ps_job_q_enc;
            ps_me_enc_bufs->ps_cur_ctb_cu_tree = ps_cur_ctb_cu_tree;
            ps_me_enc_bufs->ps_cur_ctb_me_data = ps_cur_ctb_me_data;

            ps_job_q_enc += (MAX_NUM_VERT_UNITS_FRM * NUM_ENC_JOBS_QUES);
            /* In tile case, based on the number of column tiles,
            increment jobQ per column tile        */
            if(1 == ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_tiles_enabled_flag)
            {
                WORD32 col_tile_ctr;
                for(col_tile_ctr = 1;
                    col_tile_ctr < ps_enc_ctxt->ps_stat_prms->s_app_tile_params.i4_num_tile_cols;
                    col_tile_ctr++)
                {
                    ps_job_q_enc += (MAX_NUM_VERT_UNITS_FRM * NUM_ENC_JOBS_QUES);
                }
            }

            ps_cur_ctb_cu_tree += (num_ctb_horz * MAX_NUM_NODES_CU_TREE * num_ctb_vert);
            ps_cur_ctb_me_data += (num_ctb_horz * num_ctb_vert);

            ps_me_enc_bufs++;
        }
    }
    /* Frame level Que between frame process & entropy */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        frm_proc_ent_cod_ctxt_t *ps_frmp_ent_bufs;
        ctb_enc_loop_out_t *ps_ctb;
        cu_enc_loop_out_t *ps_cu;
        tu_enc_loop_out_t *ps_tu;
        pu_t *ps_pu;
        UWORD8 *pu1_coeffs;
        WORD32 num_ctb_in_frm;
        WORD32 coeff_size;
        UWORD8 *pu1_sei_payload;

        /* frame process/entropy coding buffer pointer array */
        pps_frm_proc_ent_cod_bufs[i] = (frm_proc_ent_cod_ctxt_t **)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* frame process/entropy coding buffer structure */
        ps_frmp_ent_bufs = (frm_proc_ent_cod_ctxt_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* CTB enc loop Frame level  */
        ps_ctb = (ctb_enc_loop_out_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* CU enc loop Frame level  */
        ps_cu = (cu_enc_loop_out_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* TU enc loop Frame level  */
        ps_tu = (tu_enc_loop_out_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* PU enc loop Frame level  */
        ps_pu = (pu_t *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* Coeffs Frame level  */
        pu1_coeffs = (UWORD8 *)ps_memtab->pv_base;
        /* increment the memtabs */
        ps_memtab++;

        /* CC User Data  */
        pu1_sei_payload = (UWORD8 *)ps_memtab->pv_base;
        ps_memtab++;

        num_ctb_in_frm = num_ctb_horz * num_ctb_vert;

        /* calculate the coeff size */
        coeff_size =
            num_ctb_horz * ((ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format == IV_YUV_422SP_UV)
                                ? (num_tu_in_ctb << 1)
                                : ((num_tu_in_ctb * 3) >> 1));
        coeff_size = coeff_size * num_ctb_vert * MAX_SCAN_COEFFS_BYTES_4x4;
        /* loop to initialise all the memories */
        for(ctr = 0; ctr < NUM_FRMPROC_ENTCOD_BUFS; ctr++)
        {
            WORD32 num_sei;
            pps_frm_proc_ent_cod_bufs[i][ctr] = ps_frmp_ent_bufs;

            ps_frmp_ent_bufs->ps_frm_ctb_data = ps_ctb;
            ps_frmp_ent_bufs->ps_frm_cu_data = ps_cu;
            ps_frmp_ent_bufs->ps_frm_pu_data = ps_pu;
            ps_frmp_ent_bufs->ps_frm_tu_data = ps_tu;
            ps_frmp_ent_bufs->pv_coeff_data = pu1_coeffs;

            /* memset the slice headers and buffer to keep track */
            memset(&ps_frmp_ent_bufs->s_slice_hdr, 0, sizeof(slice_header_t));

            /*PIC_INFO*/
            memset(&ps_frmp_ent_bufs->s_pic_level_info, 0, sizeof(s_pic_level_acc_info_t));

            ps_ctb += num_ctb_in_frm;
            ps_cu += num_ctb_in_frm * num_cu_in_ctb;
            ps_pu += num_ctb_in_frm * num_pu_in_ctb;
            ps_tu += num_ctb_in_frm * num_tu_in_ctb;

            pu1_coeffs += coeff_size;

            for(num_sei = 0; num_sei < MAX_NUMBER_OF_SEI_PAYLOAD; num_sei++)
            {
                ps_frmp_ent_bufs->as_sei_payload[num_sei].pu1_sei_payload = pu1_sei_payload;
                ps_frmp_ent_bufs->as_sei_payload[num_sei].u4_payload_type = 0;
                ps_frmp_ent_bufs->as_sei_payload[num_sei].u4_payload_length = 0;
                pu1_sei_payload += MAX_SEI_PAYLOAD_PER_TLV;
            }

            ps_frmp_ent_bufs++;
        }
    }

    /* Working memory for encoder */
    ps_enc_ctxt->pu1_frm_lvl_wkg_mem = (UWORD8 *)ps_memtab->pv_base;
    ps_memtab++;

    /* Job Que memory */
    /* Job que memory distribution is as follows                                                 _______
    enc_group_ping -> MAX_NUM_VERT_UNITS_FRM for all the passes (NUM_ENC_JOBS_QUES)------------>|_______|
    enc_group_pong -> MAX_NUM_VERT_UNITS_FRM for all the passes (NUM_ENC_JOBS_QUES)------------>|_______|
    pre_enc_group_ping -> MAX_NUM_VERT_UNITS_FRM for all the passes (NUM_PRE_ENC_JOBS_QUES)---->|_______|
    pre_enc_group_ping -> MAX_NUM_VERT_UNITS_FRM for all the passes (NUM_PRE_ENC_JOBS_QUES)---->|_______|
    */

    ps_enc_ctxt->s_multi_thrd.aps_job_q_pre_enc[0] = (job_queue_t *)ps_memtab->pv_base;
    for(ctr = 1; ctr < max_delay_preenc_l0_que; ctr++)
    {
        ps_enc_ctxt->s_multi_thrd.aps_job_q_pre_enc[ctr] =
            ps_enc_ctxt->s_multi_thrd.aps_job_q_pre_enc[0] +
            (MAX_NUM_VERT_UNITS_FRM * NUM_PRE_ENC_JOBS_QUES * ctr);
    }
    ps_memtab++;

    /* -----Frameproc Entcod Que mem_init --- */
    /* init ptrs for each bit-rate */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_FRM_PRS_ENT_COD_Q + i] = ihevce_buff_que_init(
            ps_memtab, NUM_FRMPROC_ENTCOD_BUFS, (void **)pps_frm_proc_ent_cod_bufs[i]);
        ps_memtab += ihevce_buff_que_get_num_mem_recs();
    }
    /*mrs*/
    /* ----Encoder owned input buffer queue init----*/
    ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_ENC_INPUT_Q] =
        ihevce_buff_que_init(ps_memtab, num_input_buf_per_queue, (void **)pps_lap_enc_input_bufs);
    ps_memtab += ihevce_buff_que_get_num_mem_recs();

    /* -----Pre-Encode / Encode Que mem_init --- */
    ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_PRE_ENC_ME_Q] =
        ihevce_buff_que_init(ps_memtab, num_bufs_preenc_me_que, (void **)pps_pre_enc_bufs);

    ps_memtab += ihevce_buff_que_get_num_mem_recs();

    /* -----ME / Enc-RD opt Que mem_init --- */
    ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_ME_ENC_RDOPT_Q] =
        ihevce_buff_que_init(ps_memtab, NUM_ME_ENC_BUFS, (void **)pps_me_enc_bufs);

    ps_memtab += ihevce_buff_que_get_num_mem_recs();

    /* -----Pre-Encode L0 IPE to enc queue --- */
    ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_L0_IPE_ENC_Q] =
        ihevce_buff_que_init(ps_memtab, num_bufs_L0_ipe_enc, (void **)pps_L0_ipe_enc_bufs);

    ps_memtab += ihevce_buff_que_get_num_mem_recs();

    /* ---------- Dependency Manager allocations -------- */
    {
        osal_sem_attr_t attr = OSAL_DEFAULT_SEM_ATTR;
        WORD32 i1_is_sem_enabled;

        if(ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
               .i4_quality_preset >= IHEVCE_QUALITY_P4)
        {
            i1_is_sem_enabled = 0;
        }
        else
        {
            i1_is_sem_enabled = 1;
        }

        /* allocate semaphores for all the threads in pre-enc and enc */
        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds; ctr++)
        {
            ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle[ctr] =
                osal_sem_create(ps_intrf_ctxt->pv_osal_handle, &attr);
            if(NULL == ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle[ctr])
            {
                ps_intrf_ctxt->i4_error_code = IHEVCE_CANNOT_ALLOCATE_MEMORY;
                return;
            }
        }

        for(ctr = 0; ctr < ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds; ctr++)
        {
            ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle[ctr] =
                osal_sem_create(ps_intrf_ctxt->pv_osal_handle, &attr);
            if(NULL == ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle[ctr])
            {
                ps_intrf_ctxt->i4_error_code = IHEVCE_CANNOT_ALLOCATE_MEMORY;
                return;
            }
        }

        /* --- ME-EncLoop Dep Mngr Row-Row Init -- */
        for(ctr = 0; ctr < NUM_ME_ENC_BUFS; ctr++)
        {
            me_enc_rdopt_ctxt_t *ps_me_enc_bufs = pps_me_enc_bufs[ctr];

            ps_me_enc_bufs->pv_dep_mngr_encloop_dep_me = ihevce_dmgr_init(
                ps_memtab,
                ps_intrf_ctxt->pv_osal_handle,
                DEP_MNGR_ROW_ROW_SYNC,
                (a_ctb_align_ht[0] / ctb_size),
                (a_ctb_align_wd[0] / ctb_size),
                ps_enc_ctxt->ps_tile_params_base->i4_num_tile_cols, /* Number of Col Tiles */
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                i1_is_sem_enabled /*Sem Disabled/Enabled*/
            );
            ps_memtab += ihevce_dmgr_get_num_mem_recs();

            /* Register Enc group semaphore handles */
            ihevce_dmgr_reg_sem_hdls(
                ps_me_enc_bufs->pv_dep_mngr_encloop_dep_me,
                ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle,
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds);

            /* Register the handle in multithread ctxt also for free purpose */
            ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_encloop_dep_me[ctr] =
                ps_me_enc_bufs->pv_dep_mngr_encloop_dep_me;
        }

        for(ctr = 0; ctr < i4_num_enc_loop_frm_pllel; ctr++)
        {
            /* --- Prev. frame EncLoop Done Dep Mngr Frm-Frm Mem Init -- */
            ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_done[ctr] = ihevce_dmgr_init(
                ps_memtab,
                ps_intrf_ctxt->pv_osal_handle,
                DEP_MNGR_FRM_FRM_SYNC,
                (a_ctb_align_ht[0] / ctb_size),
                (a_ctb_align_wd[0] / ctb_size),
                1, /* Number of Col Tiles : Don't care for FRM_FRM */
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                1 /*Sem Enabled*/
            );
            ps_memtab += ihevce_dmgr_get_num_mem_recs();

            /* Register Enc group semaphore handles */
            ihevce_dmgr_reg_sem_hdls(
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_done[ctr],
                ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle,
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds);
        }
        /* --- Prev. frame EncLoop Done Dep Mngr  for re-encode  Frm-Frm Mem Init -- */
        ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_enc_done_for_reenc = ihevce_dmgr_init(
            ps_memtab,
            ps_intrf_ctxt->pv_osal_handle,
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            (a_ctb_align_wd[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
            1 /*Sem Enabled*/
        );
        ps_memtab += ihevce_dmgr_get_num_mem_recs();

        /* Register Enc group semaphore handles */
        ihevce_dmgr_reg_sem_hdls(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_enc_done_for_reenc,
            ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle,
            ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds);
        for(ctr = 0; ctr < i4_num_me_frm_pllel; ctr++)
        {
            /* --- Prev. frame ME Done Dep Mngr Frm-Frm Mem Init -- */
            ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_me_done[ctr] = ihevce_dmgr_init(
                ps_memtab,
                ps_intrf_ctxt->pv_osal_handle,
                DEP_MNGR_FRM_FRM_SYNC,
                (a_ctb_align_ht[0] / ctb_size),
                (a_ctb_align_wd[0] / ctb_size),
                1, /* Number of Col Tiles : Don't care for FRM_FRM */
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                1 /*Sem Enabled*/
            );
            ps_memtab += ihevce_dmgr_get_num_mem_recs();

            /* Register Enc group semaphore handles */
            ihevce_dmgr_reg_sem_hdls(
                ps_enc_ctxt->s_multi_thrd.apv_dep_mngr_prev_frame_me_done[ctr],
                ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle,
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds);
        }
        /* --- Prev. frame PreEnc L1 Done Dep Mngr Frm-Frm Mem Init -- */
        ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l1 = ihevce_dmgr_init(
            ps_memtab,
            ps_intrf_ctxt->pv_osal_handle,
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            (a_ctb_align_wd[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
            1 /*Sem Enabled*/
        );
        ps_memtab += ihevce_dmgr_get_num_mem_recs();

        /* Register Pre-Enc group semaphore handles */
        ihevce_dmgr_reg_sem_hdls(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l1,
            ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle,
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds);

        /* --- Prev. frame PreEnc HME Done Dep Mngr Frm-Frm Mem Init -- */
        ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_coarse_me = ihevce_dmgr_init(
            ps_memtab,
            ps_intrf_ctxt->pv_osal_handle,
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            (a_ctb_align_wd[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
            1 /*Sem Enabled*/
        );
        ps_memtab += ihevce_dmgr_get_num_mem_recs();

        /* Register Pre-Enc group semaphore handles */
        ihevce_dmgr_reg_sem_hdls(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_coarse_me,
            ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle,
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds);

        /* --- Prev. frame PreEnc L0 Done Dep Mngr Frm-Frm Mem Init -- */
        ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l0 = ihevce_dmgr_init(
            ps_memtab,
            ps_intrf_ctxt->pv_osal_handle,
            DEP_MNGR_FRM_FRM_SYNC,
            (a_ctb_align_ht[0] / ctb_size),
            (a_ctb_align_wd[0] / ctb_size),
            1, /* Number of Col Tiles : Don't care for FRM_FRM */
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds,
            1 /*Sem Enabled*/
        );
        ps_memtab += ihevce_dmgr_get_num_mem_recs();

        /* Register Pre-Enc group semaphore handles */
        ihevce_dmgr_reg_sem_hdls(
            ps_enc_ctxt->s_multi_thrd.pv_dep_mngr_prev_frame_pre_enc_l0,
            ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle,
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds);

        /* --- ME-Prev Recon Dep Mngr Row-Frm Mem init -- */
        for(ctr = 0; ctr < (max_num_ref_pics + 1 + NUM_EXTRA_RECON_BUFS); ctr++)
        {
            WORD32 ai4_tile_xtra_ctb[4] = { 0 };

            ps_enc_ctxt->pps_recon_buf_q[0][ctr]->pv_dep_mngr_recon = ihevce_dmgr_map_init(
                ps_memtab,
                num_ctb_vert,
                num_ctb_horz,
                i1_is_sem_enabled, /*Sem Disabled/Enabled*/
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds,
                ai4_tile_xtra_ctb);

            ps_memtab += ihevce_dmgr_get_num_mem_recs();

            /* Register Enc group semaphore handles */
            ihevce_dmgr_reg_sem_hdls(
                ps_enc_ctxt->pps_recon_buf_q[0][ctr]->pv_dep_mngr_recon,
                ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle,
                ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds);
        }

        /* ------ Module level register semaphores -------- */
        ihevce_coarse_me_reg_thrds_sem(
            ps_enc_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
            ps_enc_ctxt->s_multi_thrd.apv_pre_enc_thrd_sem_handle,
            ps_enc_ctxt->s_multi_thrd.i4_num_pre_enc_proc_thrds);

        ihevce_enc_loop_reg_sem_hdls(
            ps_enc_ctxt->s_module_ctxt.pv_enc_loop_ctxt,
            ps_enc_ctxt->s_multi_thrd.apv_enc_thrd_sem_handle,
            ps_enc_ctxt->s_multi_thrd.i4_num_enc_proc_thrds);
    }

    /* copy the run time source parameters from create time prms */
    memcpy(
        &ps_enc_ctxt->s_runtime_src_prms,
        &ps_enc_ctxt->ps_stat_prms->s_src_prms,
        sizeof(ihevce_src_params_t));

    memcpy(
        &ps_enc_ctxt->s_runtime_tgt_params,
        &ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id],
        sizeof(ihevce_tgt_params_t));

    /* copy the run time coding parameters from create time prms */
    memcpy(
        &ps_enc_ctxt->s_runtime_coding_prms,
        &ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms,
        sizeof(ihevce_coding_params_t));

    /*change in run time parameter*/
    if(ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_max_reference_frames == -1)
    {
        ps_enc_ctxt->s_runtime_coding_prms.i4_max_reference_frames = (DEFAULT_MAX_REFERENCE_PICS)
                                                                     << i4_field_pic;

        ps_enc_ctxt->s_lap_stat_prms.i4_max_reference_frames =
            ps_enc_ctxt->s_runtime_coding_prms.i4_max_reference_frames;
    }

    /* populate the frame level ctb parameters based on run time params */
    ihevce_set_pre_enc_prms(ps_enc_ctxt);

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_mem_manager_que_init \endif
*
* \brief
*    Encoder Que memory init function
*
* \param[in] Encoder context pointer
* \param[in] High level Encoder context pointer
* \param[in] Buffer descriptors
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_mem_manager_que_init(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_hle_ctxt_t *ps_hle_ctxt,
    iv_input_data_ctrl_buffs_desc_t *ps_input_data_ctrl_buffs_desc,
    iv_input_asynch_ctrl_buffs_desc_t *ps_input_asynch_ctrl_buffs_desc,
    iv_output_data_buffs_desc_t *ps_output_data_buffs_desc,
    iv_recon_data_buffs_desc_t *ps_recon_data_buffs_desc)
{
    /* local variables */
    WORD32 total_memtabs_req = 0;
    WORD32 total_memtabs_used = 0;
    WORD32 ctr;
    iv_mem_rec_t *ps_memtab;
    WORD32 i;  //counter variable
    iv_output_data_buffs_desc_t *ps_out_desc;
    iv_recon_data_buffs_desc_t *ps_rec_desc;
    WORD32 i4_num_bitrate_inst;  //number of bit-rate instance
    /* storing 0th instance's pointer. This will be used for assigning buffer queue handles for input/output queues */
    enc_ctxt_t *ps_enc_ctxt_base = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[0];

    i4_num_bitrate_inst = ps_enc_ctxt->i4_num_bitrates;
    //ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[0].i4_num_bitrate_instances;

    /* --------------------------------------------------------------------- */
    /* --------------  Collating the number of memtabs required ------------ */
    /* --------------------------------------------------------------------- */

    /* ------ Input Data Que Memtab -------- */
    if(0 == ps_enc_ctxt->i4_resolution_id)
    {
        /* array of pointers for input */
        total_memtabs_req++;

        /* pointers for input desc */
        total_memtabs_req++;

        /* que manager buffer requirements */
        total_memtabs_req += ihevce_buff_que_get_num_mem_recs();

        /* ------ Input Control Que memtab ----- */
        /* array of pointers for input control */
        total_memtabs_req++;

        /* pointers for input control desc */
        total_memtabs_req++;

        /* que manager buffer requirements */
        total_memtabs_req += ihevce_buff_que_get_num_mem_recs();
    }

    /* ------ Output Data Que Memtab -------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        /* array of pointers for output */
        total_memtabs_req++;

        /* pointers for output desc */
        total_memtabs_req++;

        /* que manager buffer requirements */
        total_memtabs_req += ihevce_buff_que_get_num_mem_recs();
    }

    /* ------ Recon Data Que Memtab -------- */
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        if(ps_hle_ctxt->ps_static_cfg_prms->i4_save_recon)
        {
            /* array of pointers for input */
            total_memtabs_req++;

            /* pointers for input desc */
            total_memtabs_req++;

            /* que manager buffer requirements */
            total_memtabs_req += ihevce_buff_que_get_num_mem_recs();
        }
    }

    /* ----- allocate memomry for memtabs --- */
    {
        iv_mem_rec_t s_memtab;

        s_memtab.i4_size = sizeof(iv_mem_rec_t);
        s_memtab.i4_mem_size = total_memtabs_req * sizeof(iv_mem_rec_t);
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
        s_memtab.i4_mem_alignment = 4;

        ps_hle_ctxt->ihevce_mem_alloc(
            ps_hle_ctxt->pv_mem_mgr_hdl, &ps_hle_ctxt->ps_static_cfg_prms->s_sys_api, &s_memtab);
        if(s_memtab.pv_base == NULL)
        {
            ps_hle_ctxt->i4_error_code = IHEVCE_CANNOT_ALLOCATE_MEMORY;
            return;
        }
        ps_memtab = (iv_mem_rec_t *)s_memtab.pv_base;
    }

    /* --------------------------------------------------------------------- */
    /* ------------------  Collating memory requirements ------------------- */
    /* --------------------------------------------------------------------- */
    if(0 == ps_enc_ctxt->i4_resolution_id)
    {
        /* ------ Input Data Que memory requests -------- */
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_memtab[total_memtabs_used].i4_mem_size =
            ((ps_input_data_ctrl_buffs_desc->i4_num_yuv_bufs) * (sizeof(ihevce_lap_enc_buf_t *)));

        /* increment the memtab counter */
        total_memtabs_used++;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_memtab[total_memtabs_used].i4_mem_size =
            ((ps_input_data_ctrl_buffs_desc->i4_num_yuv_bufs) * (sizeof(ihevce_lap_enc_buf_t)));

        /* increment the memtab counter */
        total_memtabs_used++;

        /* call the Que manager get mem recs */
        total_memtabs_used += ihevce_buff_que_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            ps_input_data_ctrl_buffs_desc->i4_num_yuv_bufs,
            IV_EXT_CACHEABLE_NORMAL_MEM);

        /* ------ Input Control Que memory requests -------- */
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_memtab[total_memtabs_used].i4_mem_size =
            ((ps_input_asynch_ctrl_buffs_desc->i4_num_asynch_ctrl_bufs) *
             (sizeof(iv_input_ctrl_buffs_t *)));

        /* increment the memtab counter */
        total_memtabs_used++;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_memtab[total_memtabs_used].i4_mem_size =
            ((ps_input_asynch_ctrl_buffs_desc->i4_num_asynch_ctrl_bufs) *
             (sizeof(iv_input_ctrl_buffs_t)));

        /* increment the memtab counter */
        total_memtabs_used++;

        /* call the Que manager get mem recs */
        total_memtabs_used += ihevce_buff_que_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            ps_input_asynch_ctrl_buffs_desc->i4_num_asynch_ctrl_bufs,
            IV_EXT_CACHEABLE_NORMAL_MEM);
    }

    /* ------ Output data Que memory requests -------- */
    ps_out_desc = ps_output_data_buffs_desc;
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_memtab[total_memtabs_used].i4_mem_size =
            ((ps_out_desc->i4_num_bitstream_bufs) * (sizeof(iv_output_data_buffs_t *)));

        /* increment the memtab counter */
        total_memtabs_used++;

        ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

        ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

        ps_memtab[total_memtabs_used].i4_mem_size =
            ((ps_out_desc->i4_num_bitstream_bufs) * (sizeof(iv_output_data_buffs_t)));

        /* increment the memtab counter */
        total_memtabs_used++;

        /* call the Que manager get mem recs */
        total_memtabs_used += ihevce_buff_que_get_mem_recs(
            &ps_memtab[total_memtabs_used],
            ps_out_desc->i4_num_bitstream_bufs,
            IV_EXT_CACHEABLE_NORMAL_MEM);
        ps_out_desc++;
    }

    //recon_dump
    /* ------ Recon Data Que memory requests -------- */
    ps_rec_desc = ps_recon_data_buffs_desc;
    if(ps_hle_ctxt->ps_static_cfg_prms->i4_save_recon)
    {
        for(i = 0; i < i4_num_bitrate_inst; i++)
        {
            ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

            ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

            ps_memtab[total_memtabs_used].i4_mem_size =
                ((ps_rec_desc->i4_num_recon_bufs) * (sizeof(iv_enc_recon_data_buffs_t *)));

            /* increment the memtab counter */
            total_memtabs_used++;

            ps_memtab[total_memtabs_used].i4_mem_alignment = 8;

            ps_memtab[total_memtabs_used].e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

            ps_memtab[total_memtabs_used].i4_mem_size =
                ((ps_rec_desc->i4_num_recon_bufs) * (sizeof(iv_enc_recon_data_buffs_t)));

            /* increment the memtab counter */
            total_memtabs_used++;

            /* call the Que manager get mem recs */
            total_memtabs_used += ihevce_buff_que_get_mem_recs(
                &ps_memtab[total_memtabs_used],
                ps_rec_desc->i4_num_recon_bufs,
                IV_EXT_CACHEABLE_NORMAL_MEM);

            ps_rec_desc++;
        }
    }

    /* ----- allocate memory as per requests ---- */

    /* check on memtabs requested v/s memtabs used */
    //ittiam : should put an assert
    ASSERT(total_memtabs_req == total_memtabs_used);
    for(ctr = 0; ctr < total_memtabs_used; ctr++)
    {
        UWORD8 *pu1_mem = NULL;
        ps_hle_ctxt->ihevce_mem_alloc(
            ps_hle_ctxt->pv_mem_mgr_hdl,
            &ps_hle_ctxt->ps_static_cfg_prms->s_sys_api,
            &ps_memtab[ctr]);

        pu1_mem = (UWORD8 *)ps_memtab[ctr].pv_base;

        if(NULL == pu1_mem)
        {
            ps_hle_ctxt->i4_error_code = IHEVCE_CANNOT_ALLOCATE_MEMORY;
            return;
        }
    }

    /* store the final allocated memtabs */
    ps_enc_ctxt->s_mem_mngr.i4_num_q_memtabs = total_memtabs_used;
    ps_enc_ctxt->s_mem_mngr.ps_q_memtab = ps_memtab;

    /* --------------------------------------------------------------------- */
    /* -------------- Initialisation of Queues memory ---------------------- */
    /* --------------------------------------------------------------------- */

    /* ---------- Input Data Que Mem init --------------- */
    if(0 == ps_enc_ctxt->i4_resolution_id)
    {
        ihevce_lap_enc_buf_t **pps_inp_bufs;
        ihevce_lap_enc_buf_t *ps_inp_bufs;

        pps_inp_bufs = (ihevce_lap_enc_buf_t **)ps_memtab->pv_base;
        ps_memtab++;

        ps_inp_bufs = (ihevce_lap_enc_buf_t *)ps_memtab->pv_base;
        ps_memtab++;

        /* loop to initialise the buffer pointer */
        for(ctr = 0; ctr < ps_input_data_ctrl_buffs_desc->i4_num_yuv_bufs; ctr++)
        {
            pps_inp_bufs[ctr] = &ps_inp_bufs[ctr];

            pps_inp_bufs[ctr]->s_input_buf.i4_size = sizeof(iv_input_data_ctrl_buffs_t);

            pps_inp_bufs[ctr]->s_input_buf.s_input_buf.i4_size = sizeof(iv_yuv_buf_t);

            /*pointer to i/p buf initialised to null in case of run time allocation*/
            if(ps_hle_ctxt->i4_create_time_input_allocation == 1)
            {
                pps_inp_bufs[ctr]->s_input_buf.pv_synch_ctrl_bufs =
                    ps_input_data_ctrl_buffs_desc->ppv_synch_ctrl_bufs[ctr];

                pps_inp_bufs[ctr]->s_input_buf.s_input_buf.pv_y_buf =
                    ps_input_data_ctrl_buffs_desc->ppv_y_buf[ctr];

                pps_inp_bufs[ctr]->s_input_buf.s_input_buf.pv_u_buf =
                    ps_input_data_ctrl_buffs_desc->ppv_u_buf[ctr];

                pps_inp_bufs[ctr]->s_input_buf.s_input_buf.pv_v_buf =
                    ps_input_data_ctrl_buffs_desc->ppv_v_buf[ctr];
            }
            else
            {
                pps_inp_bufs[ctr]->s_input_buf.pv_synch_ctrl_bufs = NULL;

                pps_inp_bufs[ctr]->s_input_buf.s_input_buf.pv_y_buf = NULL;

                pps_inp_bufs[ctr]->s_input_buf.s_input_buf.pv_u_buf = NULL;

                pps_inp_bufs[ctr]->s_input_buf.s_input_buf.pv_v_buf = NULL;
            }
        }

        /* Get the input data buffer Q handle */
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_INPUT_DATA_CTRL_Q] = ihevce_buff_que_init(
            ps_memtab, ps_input_data_ctrl_buffs_desc->i4_num_yuv_bufs, (void **)pps_inp_bufs);

        /* increment the memtab pointer */
        ps_memtab += ihevce_buff_que_get_num_mem_recs();
    }
    else
    {
        /* Get the input data buffer Q handle from 0th instance */
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_INPUT_DATA_CTRL_Q] =
            ps_enc_ctxt_base->s_enc_ques.apv_q_hdl[IHEVCE_INPUT_DATA_CTRL_Q];
    }

    /* ---------- Input control Que Mem init --------------- */
    if(0 == ps_enc_ctxt->i4_resolution_id)
    {
        iv_input_ctrl_buffs_t **pps_inp_bufs;
        iv_input_ctrl_buffs_t *ps_inp_bufs;

        pps_inp_bufs = (iv_input_ctrl_buffs_t **)ps_memtab->pv_base;
        ps_memtab++;

        ps_inp_bufs = (iv_input_ctrl_buffs_t *)ps_memtab->pv_base;
        ps_memtab++;

        /* loop to initialise the buffer pointer */
        for(ctr = 0; ctr < ps_input_asynch_ctrl_buffs_desc->i4_num_asynch_ctrl_bufs; ctr++)
        {
            pps_inp_bufs[ctr] = &ps_inp_bufs[ctr];

            pps_inp_bufs[ctr]->i4_size = sizeof(iv_input_ctrl_buffs_t);

            pps_inp_bufs[ctr]->pv_asynch_ctrl_bufs =
                ps_input_asynch_ctrl_buffs_desc->ppv_asynch_ctrl_bufs[ctr];
        }

        /* Get the input control buffer Q handle */
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_INPUT_ASYNCH_CTRL_Q] = ihevce_buff_que_init(
            ps_memtab,
            ps_input_asynch_ctrl_buffs_desc->i4_num_asynch_ctrl_bufs,
            (void **)pps_inp_bufs);

        /* increment the memtab pointer */
        ps_memtab += ihevce_buff_que_get_num_mem_recs();
    }
    else
    {
        /* Get the input control buffer Q handle from 0th instance */
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_INPUT_ASYNCH_CTRL_Q] =
            ps_enc_ctxt_base->s_enc_ques.apv_q_hdl[IHEVCE_INPUT_ASYNCH_CTRL_Q];
    }

    /* ---------- Output data Que Mem init --------------- */
    ps_out_desc = ps_output_data_buffs_desc;
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        iv_output_data_buffs_t **pps_out_bufs;
        iv_output_data_buffs_t *ps_out_bufs;

        pps_out_bufs = (iv_output_data_buffs_t **)ps_memtab->pv_base;
        ps_memtab++;

        ps_out_bufs = (iv_output_data_buffs_t *)ps_memtab->pv_base;
        ps_memtab++;

        /* loop to initialise the buffer pointer */
        for(ctr = 0; ctr < ps_out_desc->i4_num_bitstream_bufs; ctr++)
        {
            pps_out_bufs[ctr] = &ps_out_bufs[ctr];

            pps_out_bufs[ctr]->i4_size = sizeof(iv_output_data_buffs_t);

            pps_out_bufs[ctr]->i4_bitstream_buf_size = ps_out_desc->i4_size_bitstream_buf;

            /*pointer to o/p buf initialised to null in case of run time allocation*/
            if(ps_hle_ctxt->i4_create_time_output_allocation == 1)
            {
                pps_out_bufs[ctr]->pv_bitstream_bufs = ps_out_desc->ppv_bitstream_bufs[ctr];
            }
            else
            {
                pps_out_bufs[ctr]->pv_bitstream_bufs = NULL;
            }
        }

        /* Get the output data buffer Q handle */
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_OUTPUT_DATA_Q + i] = ihevce_buff_que_init(
            ps_memtab, ps_out_desc->i4_num_bitstream_bufs, (void **)pps_out_bufs);

        /* increment the memtab pointer */
        ps_memtab += ihevce_buff_que_get_num_mem_recs();

        ps_out_desc++;
    }

    /* ----------Recon data Que Mem init --------------- */
    ps_rec_desc = ps_recon_data_buffs_desc;
    for(i = 0; i < i4_num_bitrate_inst; i++)
    {
        if(ps_hle_ctxt->ps_static_cfg_prms->i4_save_recon)
        {
            iv_enc_recon_data_buffs_t **pps_recon_bufs;
            iv_enc_recon_data_buffs_t *ps_recon_bufs;

            pps_recon_bufs = (iv_enc_recon_data_buffs_t **)ps_memtab->pv_base;
            ps_memtab++;

            ps_recon_bufs = (iv_enc_recon_data_buffs_t *)ps_memtab->pv_base;
            ps_memtab++;

            /* loop to initialise the buffer pointer */
            for(ctr = 0; ctr < ps_rec_desc->i4_num_recon_bufs; ctr++)
            {
                pps_recon_bufs[ctr] = &ps_recon_bufs[ctr];

                pps_recon_bufs[ctr]->i4_size = sizeof(iv_enc_recon_data_buffs_t);

                pps_recon_bufs[ctr]->pv_y_buf = ps_rec_desc->ppv_y_buf[ctr];

                pps_recon_bufs[ctr]->pv_cb_buf = ps_rec_desc->ppv_u_buf[ctr];

                pps_recon_bufs[ctr]->pv_cr_buf = ps_rec_desc->ppv_v_buf[ctr];
            }

            /* Get the output data buffer Q handle */
            ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_RECON_DATA_Q + i] = ihevce_buff_que_init(
                ps_memtab, ps_rec_desc->i4_num_recon_bufs, (void **)pps_recon_bufs);

            /* increment the memtab pointer */
            ps_memtab += ihevce_buff_que_get_num_mem_recs();

            ps_rec_desc++;
        }
        else
        {
            ps_enc_ctxt->s_enc_ques.apv_q_hdl[IHEVCE_RECON_DATA_Q + i] = NULL;
        }
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_mem_manager_free \endif
*
* \brief
*    Encoder memory free function
*
* \param[in] Processing interface context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_mem_manager_free(enc_ctxt_t *ps_enc_ctxt, ihevce_hle_ctxt_t *ps_intrf_ctxt)
{
    WORD32 ctr;

    /* run a loop to free all the memory allocated create time */
    for(ctr = 0; ctr < ps_enc_ctxt->s_mem_mngr.i4_num_create_memtabs; ctr++)
    {
        ps_intrf_ctxt->ihevce_mem_free(
            ps_intrf_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt->s_mem_mngr.ps_create_memtab[ctr]);
    }

    /* free the memtab memory */
    {
        iv_mem_rec_t s_memtab;

        s_memtab.i4_size = sizeof(iv_mem_rec_t);
        s_memtab.i4_mem_size = ps_enc_ctxt->s_mem_mngr.i4_num_create_memtabs * sizeof(iv_mem_rec_t);
        s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
        s_memtab.i4_mem_alignment = 4;
        s_memtab.pv_base = (void *)ps_enc_ctxt->s_mem_mngr.ps_create_memtab;

        ps_intrf_ctxt->ihevce_mem_free(ps_intrf_ctxt->pv_mem_mgr_hdl, &s_memtab);
    }

    if(1 == ps_enc_ctxt->i4_io_queues_created)
    {
        /* run a loop to free all the memory allocated durign que creation */
        for(ctr = 0; ctr < ps_enc_ctxt->s_mem_mngr.i4_num_q_memtabs; ctr++)
        {
            ps_intrf_ctxt->ihevce_mem_free(
                ps_intrf_ctxt->pv_mem_mgr_hdl, &ps_enc_ctxt->s_mem_mngr.ps_q_memtab[ctr]);
        }

        /* free the  memtab memory */
        {
            iv_mem_rec_t s_memtab;

            s_memtab.i4_size = sizeof(iv_mem_rec_t);
            s_memtab.i4_mem_size = ps_enc_ctxt->s_mem_mngr.i4_num_q_memtabs * sizeof(iv_mem_rec_t);
            s_memtab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;
            s_memtab.i4_mem_alignment = 4;
            s_memtab.pv_base = (void *)ps_enc_ctxt->s_mem_mngr.ps_q_memtab;

            ps_intrf_ctxt->ihevce_mem_free(ps_intrf_ctxt->pv_mem_mgr_hdl, &s_memtab);
        }
    }
    return;
}
