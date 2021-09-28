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
 * \file ihevce_entropy_cod.c
 *
 * \brief
 *    This file contains interface function definitions related to Entroy coding
 *
 * \date
 *    18/09/2012
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
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
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
#include "ihevce_entropy_structs.h"
#include "ihevce_entropy_interface.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_rc_interface.h"
#include "ihevce_encode_header.h"
#include "ihevce_encode_header_sei_vui.h"
#include "ihevce_trace.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Extern variables                                                          */
/*****************************************************************************/
UWORD8 gau1_pic_type_string[5][11] = {
    { "I-SLICE  " }, { "P-SLICE  " }, { "B-SLICE  " }, { "IDR-SLICE" }, { "b-SLICE  " }
};

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define PSNR_FROM_MSE(x, bit_depth)                                                                \
    ((x == 0) ? 99.999999 : (20 * log10(((1 << bit_depth) - 1) / sqrt(x))))

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
 ******************************************************************************
 * \if Function name : ihevce_ent_coding_thrd \endif
 *
 * \brief
 *   Entropy coding thread interface function
 *
 * \param[in] Frame process pointer
 *
 * \return
 *    None
 *
 * \author
 *  Ittiam
 *
 *****************************************************************************
 */
WORD32 ihevce_ent_coding_thrd(void *pv_frm_proc_thrd_ctxt)
{
    /* local variabels */
    frm_proc_thrd_ctxt_t *ps_thrd_ctxt;
    enc_ctxt_t *ps_enc_ctxt;
    WORD32 i4_thrd_id;
    ihevce_hle_ctxt_t *ps_hle_ctxt;
    WORD32 end_flag;
    WORD32 out_buf_id;
    WORD32 inp_buf_id;
    WORD32 entropy_error = 0;
    WORD32 i4_bitrate_instance_num, i4_resolution_id, i4_out_res_id;
    WORD32 i4_bufque_id;
    UWORD32 u4_encode_frm_num = 0;
    UWORD32 u4_au_cpb_removal_delay_minus1 = 0;
    WORD32 i4_no_output = 0;
    WORD32 i4_do_entr_last = 1;
    WORD32 i4_use_dummy_buffer = 0;
    void *pv_entropy_hdl;
    entropy_context_t *ps_entropy_ctxt;

    iv_output_data_buffs_t *ps_curr_out = NULL;
    frm_proc_ent_cod_ctxt_t *ps_curr_inp = NULL;
    iv_output_data_buffs_t s_curr_out_dummy;

    /* derive local variables */
    ps_thrd_ctxt = (frm_proc_thrd_ctxt_t *)pv_frm_proc_thrd_ctxt;
    i4_thrd_id = ps_thrd_ctxt->i4_thrd_id;
    ps_hle_ctxt = (ihevce_hle_ctxt_t *)ps_thrd_ctxt->ps_hle_ctxt;
    ps_enc_ctxt = (enc_ctxt_t *)ps_thrd_ctxt->pv_enc_ctxt;
    end_flag = 0;
    i4_bitrate_instance_num = i4_thrd_id;
    i4_bufque_id = i4_thrd_id;
    i4_resolution_id = ps_enc_ctxt->i4_resolution_id;
    i4_out_res_id = i4_resolution_id;

    /*swaping of buf_id for 0th and reference bitrate location, as encoder
    assumes always 0th loc for reference bitrate and app must receive in
    the configured order*/
    i4_bufque_id = i4_bitrate_instance_num;
    if(i4_bitrate_instance_num == 0)
    {
        i4_bufque_id = ps_enc_ctxt->i4_ref_mbr_id;
    }
    else if(i4_bitrate_instance_num == ps_enc_ctxt->i4_ref_mbr_id)
    {
        i4_bufque_id = 0;
    }

    if(1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out)
    {
        i4_bufque_id = 0;
        i4_out_res_id = 0;
    }
    pv_entropy_hdl = ps_enc_ctxt->s_module_ctxt.apv_ent_cod_ctxt[i4_bitrate_instance_num];
    ps_entropy_ctxt = (entropy_context_t *)pv_entropy_hdl;
    /* ---------- Processing Loop until end command is recieved --------- */
    while(0 == end_flag)
    {
        /*Get a buffer pointer*/
        /* ------- get next input buffer from Frame buffer que ---------- */
        ps_curr_inp = (frm_proc_ent_cod_ctxt_t *)ihevce_q_get_filled_buff(
            (void *)ps_enc_ctxt,
            (IHEVCE_FRM_PRS_ENT_COD_Q + i4_bitrate_instance_num),
            &inp_buf_id,
            BUFF_QUE_BLOCKING_MODE);
        if(1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out)
        {
            if(1 == ps_curr_inp->i4_out_flush_flag)
            {
                if(1 == ps_enc_ctxt->s_multi_thrd.pi4_active_res_id[i4_resolution_id])
                    ps_enc_ctxt->s_multi_thrd.pi4_active_res_id[i4_resolution_id] = 0;
                else
                    ASSERT(0);
            }
            else
            {
                if(0 == ps_enc_ctxt->s_multi_thrd.pi4_active_res_id[i4_resolution_id])
                {
                    /* During change in resolution check whether prev res is active before starting to dump new resolution */
                    WORD32 other_res_active = 1;
                    WORD32 ctr;
                    volatile WORD32 *pi4_active_res_check;
                    pi4_active_res_check = ps_enc_ctxt->s_multi_thrd.pi4_active_res_id;
                    while(other_res_active)
                    {
                        /* Continue in polling mode untill all the other resolutions are in passive mode */
                        other_res_active = 0;
                        for(ctr = 0;
                            ctr < ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_num_res_layers;
                            ctr++)
                        {
                            if(ctr != i4_resolution_id)
                            {
                                /* Check whether any resolution other than current resolution is active */
                                /* If its active it means that previous resolution has not finished entropy */
                                /* Wait for it to finish entropy*/
                                other_res_active |= pi4_active_res_check[ctr];
                            }
                        }
                        if(1 == ps_curr_inp->i4_end_flag)
                        {
                            i4_no_output = 1;
                        }
                    }

                    if(0 == ps_curr_inp->i4_end_flag)
                    {
                        ps_enc_ctxt->s_multi_thrd.pi4_active_res_id[i4_resolution_id] = 1;
                    }
                }
            }
        }
        if(0 == ps_curr_inp->i4_out_flush_flag)
        {
            if(1 == i4_no_output)
            {
                ps_curr_out = NULL;
            }
            else
            {
                if(!i4_use_dummy_buffer)
                {
                    /* ------- get a filled descriptor from output Que ------------ */
                    ps_curr_out = (iv_output_data_buffs_t *)ihevce_q_get_filled_buff(
                        (void *)ps_enc_ctxt,
                        (IHEVCE_OUTPUT_DATA_Q + i4_bufque_id),
                        &out_buf_id,
                        BUFF_QUE_BLOCKING_MODE);
                }
                else
                {
                    ps_curr_out = &s_curr_out_dummy;
                    out_buf_id = 0;
                    ps_curr_out->i4_bitstream_buf_size = ps_entropy_ctxt->i4_bitstream_buf_size;
                    ps_curr_out->pv_bitstream_bufs = ps_entropy_ctxt->pv_dummy_out_buf;
                }
            }
        }

        PROFILE_START(
            &ps_hle_ctxt->profile_entropy[ps_enc_ctxt->i4_resolution_id][i4_bitrate_instance_num]);
        /* Content Light Level Information */
        {
            ps_curr_inp->s_sei.i1_sei_cll_enable =
                (WORD8)ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_sei_cll_enable;
            ps_curr_inp->s_sei.s_cll_info_sei_params.u2_sei_max_cll =
                ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.u2_sei_max_cll;
            ps_curr_inp->s_sei.s_cll_info_sei_params.u2_sei_avg_cll =
                ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.u2_sei_avg_cll;
        }
        if((NULL != ps_curr_out) && (NULL != ps_curr_inp))

        {
            WORD32 i;

            /*PIC_INFO: reset the pic-level info flags*/
            ps_curr_inp->s_pic_level_info.i8_total_cu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_intra_cu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_inter_cu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_skip_cu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_pu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_intra_pu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_non_skipped_inter_pu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_merge_pu = 0;
            for(i = 0; i < 4; i++)
            {
                ps_curr_inp->s_pic_level_info.i8_total_cu_based_on_size[i] = 0;
                ps_curr_inp->s_pic_level_info.i8_total_2nx2n_intra_pu[i] = 0;
                ps_curr_inp->s_pic_level_info.i8_total_2nx2n_inter_pu[i] = 0;
                ps_curr_inp->s_pic_level_info.i8_total_tu_based_on_size[i] = 0;
                ps_curr_inp->s_pic_level_info.i8_total_smp_inter_pu[i] = 0;
                if(i != 3)
                {
                    ps_curr_inp->s_pic_level_info.i8_total_amp_inter_pu[i] = 0;
                    ps_curr_inp->s_pic_level_info.i8_total_nxn_inter_pu[i] = 0;
                }
            }

            ps_curr_inp->s_pic_level_info.i8_total_nxn_intra_pu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_L0_mode = 0;
            ps_curr_inp->s_pic_level_info.i8_total_L1_mode = 0;
            ps_curr_inp->s_pic_level_info.i8_total_BI_mode = 0;
            //ps_curr_inp->s_pic_level_info.u4_frame_intra_sad = ps_enc_ctxt->u4
            for(i = 0; i < MAX_DPB_SIZE; i++)
            {
                ps_curr_inp->s_pic_level_info.i8_total_L0_ref_idx[i] = 0;
                ps_curr_inp->s_pic_level_info.i8_total_L1_ref_idx[i] = 0;
            }

            ps_curr_inp->s_pic_level_info.i8_total_tu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_non_coded_tu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_intra_coded_tu = 0;
            ps_curr_inp->s_pic_level_info.i8_total_inter_coded_tu = 0;

            ps_curr_inp->s_pic_level_info.i8_total_qp = 0;
            ps_curr_inp->s_pic_level_info.i8_total_qp_min_cu = 0;
            ps_curr_inp->s_pic_level_info.i4_min_qp = 100;
            ps_curr_inp->s_pic_level_info.i4_max_qp = 0;
            ps_curr_inp->s_pic_level_info.i4_max_frame_qp = 0;

            ps_curr_inp->s_pic_level_info.i8_sum_squared_frame_qp = 0;
            ps_curr_inp->s_pic_level_info.i8_total_frame_qp = 0;
            ps_curr_inp->s_pic_level_info.f_total_buffer_underflow = 0;
            ps_curr_inp->s_pic_level_info.f_total_buffer_overflow = 0;
            ps_curr_inp->s_pic_level_info.f_max_buffer_underflow = 0;
            ps_curr_inp->s_pic_level_info.f_max_buffer_overflow = 0;

            ps_curr_inp->s_pic_level_info.u8_bits_estimated_intra = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_inter = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_slice_header = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_sao = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_split_cu_flag = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_cu_hdr_bits = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_split_tu_flag = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_qp_delta_bits = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_cbf_luma_bits = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_cbf_chroma_bits = 0;

            ps_curr_inp->s_pic_level_info.u8_bits_estimated_res_luma_bits = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_res_chroma_bits = 0;

            ps_curr_inp->s_pic_level_info.u8_bits_estimated_ref_id = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_mvd = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_merge_flag = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_mpm_luma = 0;
            ps_curr_inp->s_pic_level_info.u8_bits_estimated_mpm_chroma = 0;

            if(1 == ps_curr_inp->i4_frm_proc_valid_flag)
            {
                /* --- Init of buffering period and pic timing SEI related params ----*/
                {
                    UWORD32 i4_dbf, i4_buffersize, i4_trgt_bit_rate;
                    if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
                    {
                        ihevce_get_dbf_buffer_size(
                            (void *)ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bitrate_instance_num],
                            &i4_buffersize,
                            &i4_dbf,
                            &i4_trgt_bit_rate);
                    }
                    else
                    {
                        /* Default initializations in CQP mode */
                        WORD32 codec_level =
                            ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.as_tgt_params[0]
                                .i4_codec_level;
                        WORD32 codec_level_index = ihevce_get_level_index(codec_level);
                        WORD32 codec_tier =
                            ps_enc_ctxt->ps_stat_prms->s_out_strm_prms.i4_codec_tier;

                        i4_buffersize =
                            (UWORD32)g_as_level_data[codec_level_index].i4_max_cpb[codec_tier] *
                            CBP_VCL_FACTOR;
                        i4_trgt_bit_rate =
                            (UWORD32)g_as_level_data[codec_level_index].i4_max_bit_rate[codec_tier] *
                            CBP_VCL_FACTOR;
                        i4_dbf = i4_buffersize;
                    }

                    ps_curr_inp->s_sei.s_buf_period_sei_params.u4_buffer_size_sei = i4_buffersize;
                    ps_curr_inp->s_sei.s_buf_period_sei_params.u4_dbf_sei = i4_dbf;
                    ps_curr_inp->s_sei.s_buf_period_sei_params.u4_target_bit_rate_sei =
                        i4_trgt_bit_rate;

                    /* ----------------- Derivation of u4_au_cpb_removal_delay_minus1  --------------------------------*/
                    ps_curr_inp->s_sei.s_pic_timing_sei_params.u4_au_cpb_removal_delay_minus1 =
                        u4_au_cpb_removal_delay_minus1;

                    /* ----------------- Derivation of u4_pic_dpb_output_delay  --------------------------------*/
                    ps_curr_inp->s_sei.s_pic_timing_sei_params.u4_pic_dpb_output_delay =
                        ps_curr_inp->ps_sps->ai1_sps_max_num_reorder_pics[0] +
                        ps_curr_inp->i4_display_num - u4_encode_frm_num;
                }
                /* call the core entropy coding entry point function */
                entropy_error = ihevce_entropy_encode_frame(
                    pv_entropy_hdl, ps_curr_out, ps_curr_inp, ps_curr_out->i4_bitstream_buf_size);

                /* ----------------- Derivation of u4_au_cpb_removal_delay_minus1  --------------------------------*/
                if(ps_curr_inp->s_sei.i1_buf_period_params_present_flag)
                {
                    /* Reset u4_au_cpb_removal_delay_minus1 after every buffering period as subsequent pictiming is w.r.t new buffering period SEI */
                    u4_au_cpb_removal_delay_minus1 = 0;
                }
                else
                {
                    /* cpb delay is circularly incremented with wrap around based on max length signalled in VUI */
                    UWORD8 u1_au_cpb_removal_delay_length =
                        ps_curr_inp->ps_sps->s_vui_parameters.s_vui_hrd_parameters
                            .u1_au_cpb_removal_delay_length_minus1 +
                        1;

                    UWORD32 u4_max_cpb_removal_delay_val =
                        (1 << u1_au_cpb_removal_delay_length) - 1;

                    u4_au_cpb_removal_delay_minus1 = (u4_au_cpb_removal_delay_minus1 + 1) &
                                                     u4_max_cpb_removal_delay_val;
                }
                /* Debug prints for entropy error */
                if(entropy_error)
                {
                    DBG_PRINTF("Entropy encode error %x\n", entropy_error);
                    DEBUG("Entropy encode error %d\n", entropy_error);
                }
                if(ps_enc_ctxt->ps_stat_prms->s_config_prms.i4_rate_control_mode != 3)
                {
                    /* acquire mutex lock for rate control calls */
                    osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

                    /* get frame rate/bit rate/max buffer size */
                    ihevce_vbv_compliance_frame_level_update(
                        ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bitrate_instance_num],
                        (ps_curr_out->i4_bytes_generated << 3),
                        i4_resolution_id,
                        i4_bitrate_instance_num,
                        ps_curr_inp->s_sei.s_pic_timing_sei_params.u4_au_cpb_removal_delay_minus1);
                    /* release mutex lock after rate control calls */
                    osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
                }

                /*correct the mismatch between rdopt and entropy thread mismatch*/
                {
                    /* acquire mutex lock for rate control calls */
                    osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

                    /*Set to -1 when no change in bitrate, other-wise set to encoder bufferfullness at that moment*/
                    ps_curr_out->i8_cur_vbv_level = ps_curr_inp->i8_buf_level_bitrate_change;
                    if(ps_curr_inp->i8_buf_level_bitrate_change != -1)
                    {
                        LWORD64 bitrate, buffer_size;
                        ASSERT(
                            i4_bitrate_instance_num ==
                            0);  //since dynamic change in bitrate is not supported in multi bitrate and resolution
                        get_avg_bitrate_bufsize(
                            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bitrate_instance_num],
                            &bitrate,
                            &buffer_size);

                        change_bitrate_vbv_complaince(
                            ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bitrate_instance_num],
                            bitrate,
                            buffer_size);
                        /*Change bitrate in SEI-VUI related context as well*/
                        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms
                            .as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                            .ai4_tgt_bitrate[i4_bitrate_instance_num] = (WORD32)bitrate;
                        ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms
                            .as_tgt_params[ps_enc_ctxt->i4_resolution_id]
                            .ai4_max_vbv_buffer_size[i4_bitrate_instance_num] = (WORD32)buffer_size;
                    }
                    /*account for error to meet bitrate more precisely*/
                    ihevce_rc_rdopt_entropy_bit_correct(
                        ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[i4_bitrate_instance_num],
                        (ps_curr_out->i4_bytes_generated << 3),
                        inp_buf_id);  //ps_curr_inp->i4_inp_timestamp_low

                    /* release mutex lock after rate control calls */
                    osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
                }
                u4_encode_frm_num++;
            }
            else
            {
                ps_curr_out->i4_bytes_generated = 0;
                ps_curr_out->i4_encoded_frame_type = IV_NA_FRAME;
            }

            ps_curr_out->i4_buf_id = out_buf_id;
            end_flag = ps_curr_inp->i4_end_flag;
            ps_curr_out->i4_end_flag = ps_curr_inp->i4_end_flag;

            if(1 == ps_enc_ctxt->s_multi_thrd.i4_force_end_flag)
            {
                end_flag = 1;
                ps_curr_out->i4_end_flag = 1;
            }
            if(!i4_use_dummy_buffer)
            {
                /* Call back to Apln. saying buffer is produced */
                ps_hle_ctxt->ihevce_output_strm_fill_done(
                    ps_hle_ctxt->pv_out_cb_handle,
                    ps_curr_out,
                    i4_bufque_id, /* br intance */
                    i4_out_res_id /* res_instance*/);
            }

            if(ps_curr_inp->i4_frm_proc_valid_flag)
            {
                ps_curr_inp->s_pic_level_info.u8_total_bits_generated =
                    ps_curr_out->i4_bytes_generated * 8;
            }

            /* --- release the current output buffer ---- */
            if(!i4_use_dummy_buffer)
            {
                ihevce_q_rel_buf(
                    (void *)ps_enc_ctxt, (IHEVCE_OUTPUT_DATA_Q + i4_bufque_id), out_buf_id);
            }

            /* release the input buffer*/
            ihevce_q_rel_buf(
                (void *)ps_enc_ctxt,
                (IHEVCE_FRM_PRS_ENT_COD_Q + i4_bitrate_instance_num),
                inp_buf_id);

            /* reset the pointers to NULL */
            ps_curr_inp = NULL;
            ps_curr_out = NULL;
        }
        else
        {
            end_flag = ps_curr_inp->i4_end_flag;
            if(NULL != ps_curr_inp)
            {
                /* release the input buffer*/
                ihevce_q_rel_buf(
                    (void *)ps_enc_ctxt,
                    (IHEVCE_FRM_PRS_ENT_COD_Q + i4_bitrate_instance_num),
                    inp_buf_id);
            }

            // ASSERT(0);
        }
        PROFILE_STOP(
            &ps_hle_ctxt->profile_entropy[ps_enc_ctxt->i4_resolution_id][i4_bitrate_instance_num],
            NULL);
    }

    /* Release all the buffers the application might have queued in */
    /* Do this only if its not a force end */
    if(1 != ps_enc_ctxt->s_multi_thrd.i4_force_end_flag)
    {
        end_flag = 0;
    }
    if(1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out)
    {
        ps_enc_ctxt->ps_stat_prms->i4_outbuf_buf_free_control = 1;
        i4_do_entr_last = ps_enc_ctxt->s_multi_thrd.pi4_active_res_id[i4_resolution_id];
    }
    else
    {
        i4_do_entr_last = 1;
    }

    if((1 == i4_do_entr_last) && (!i4_use_dummy_buffer))
    {
        while(0 == end_flag)
        {
            if(1 == ps_enc_ctxt->ps_stat_prms->i4_outbuf_buf_free_control)  //FFMPEG application
            {
                /* ------- get a filled descriptor from output Que ------------ */
                ps_curr_out = (iv_output_data_buffs_t *)ihevce_q_get_filled_buff(
                    (void *)ps_enc_ctxt,
                    (IHEVCE_OUTPUT_DATA_Q + i4_bufque_id),
                    &out_buf_id,
                    BUFF_QUE_NON_BLOCKING_MODE);

                /* Update the end_flag from application */
                end_flag = (ps_curr_out == NULL);
            }
            else if(
                0 == ps_enc_ctxt->ps_stat_prms
                         ->i4_outbuf_buf_free_control)  // process call control based application
            {
                ps_curr_out = (iv_output_data_buffs_t *)ihevce_q_get_filled_buff(
                    (void *)ps_enc_ctxt,
                    (IHEVCE_OUTPUT_DATA_Q + i4_bufque_id),
                    &out_buf_id,
                    BUFF_QUE_BLOCKING_MODE);
            }
            else
            {
                /* should not enter here */
                ASSERT(0);
            }

            if(ps_curr_out)
            {
                end_flag = ps_curr_out->i4_is_last_buf;

                /* Fill the min. necessory things */
                ps_curr_out->i4_process_ret_sts = IV_SUCCESS;
                ps_curr_out->i4_end_flag = 1;
                ps_curr_out->i4_bytes_generated = 0;

                /* Call back to Apln. saying buffer is produced */
                ps_hle_ctxt->ihevce_output_strm_fill_done(
                    ps_hle_ctxt->pv_out_cb_handle,
                    ps_curr_out,
                    i4_bufque_id, /* br intance */
                    i4_out_res_id /* res_instance*/);

                /* --- release the current output buffer ---- */
                ihevce_q_rel_buf(
                    (void *)ps_enc_ctxt, (IHEVCE_OUTPUT_DATA_Q + i4_bufque_id), out_buf_id);
            }
        }
    }
    if(1 == ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out)
    {
        /* Mres single out usecase: Set active res_id to zero for curretn res so that other resolutions exit entropy */
        ps_enc_ctxt->s_multi_thrd.pi4_active_res_id[i4_resolution_id] = 0;
    }

    return (0);
}

/**
******************************************************************************
*
*  @brief Generate sps, pps and vps header for
*
*  @par   Description
*  This function generates nal headers like SPS/PPS/VPS
*
*  @param[in]   ps_hle_ctxt
*  pointer to high level interface context (handle)
*
*  @param[in]   i4_bitrate_instance_id
*  bitrate id
*
*  @param[in]   i4_resolution_id
*  resolution id
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_entropy_encode_header(
    ihevce_hle_ctxt_t *ps_hle_ctxt, WORD32 i4_bitrate_instance_id, WORD32 i4_resolution_id)
{
    WORD32 ret = IHEVCE_SUCCESS;
    bitstrm_t s_bit_strm;
    bitstrm_t *ps_bitstrm = &s_bit_strm;
    enc_ctxt_t *ps_enc_ctxt;
    ihevce_tgt_layer_params_t *ps_tgt_lyr_prms;
    sps_t *ps_sps;
    vps_t *ps_vps;
    pps_t *ps_pps;
    iv_output_data_buffs_t *ps_curr_out = NULL;
    WORD32 out_buf_id;

    /* sanity checks */
    if((ps_hle_ctxt == NULL) || (ps_hle_ctxt->i4_size != sizeof(ihevce_hle_ctxt_t)) ||
       ps_hle_ctxt->i4_hle_init_done != 1)
        return IHEVCE_FAIL;

    ps_tgt_lyr_prms = (ihevce_tgt_layer_params_t *)&ps_hle_ctxt->ps_static_cfg_prms->s_tgt_lyr_prms;

    if(i4_resolution_id >= ps_tgt_lyr_prms->i4_num_res_layers)
        return IHEVCE_FAIL;

    if(i4_bitrate_instance_id >=
       ps_tgt_lyr_prms->as_tgt_params[i4_resolution_id].i4_num_bitrate_instances)
        return IHEVCE_FAIL;

    ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[i4_resolution_id];
    ps_sps = &ps_enc_ctxt->as_sps[i4_bitrate_instance_id];
    ps_vps = &ps_enc_ctxt->as_vps[i4_bitrate_instance_id];
    ps_pps = &ps_enc_ctxt->as_pps[i4_bitrate_instance_id];

    /* ------- get a filled descriptor from output Que ------------ */
    ps_curr_out = (iv_output_data_buffs_t *)ihevce_q_get_filled_buff(
        (void *)ps_enc_ctxt,
        (IHEVCE_OUTPUT_DATA_Q + i4_bitrate_instance_id),
        &out_buf_id,
        BUFF_QUE_BLOCKING_MODE);

    /* Initialize the bitstream engine */
    ret |= ihevce_bitstrm_init(
        ps_bitstrm, (UWORD8 *)ps_curr_out->pv_bitstream_bufs, ps_curr_out->i4_bitstream_buf_size);

    /* Reset Bitstream NAL counter */
    ps_bitstrm->i4_num_nal = 0;

    /* generate vps */
    ret |= ihevce_generate_vps(ps_bitstrm, ps_vps);

    /* generate sps */
    ret |= ihevce_generate_sps(ps_bitstrm, ps_sps);

    /* generate pps */
    ret |= ihevce_generate_pps(ps_bitstrm, ps_pps);

    /* attach the time stamp of the input to output */
    ps_curr_out->i4_out_timestamp_low = 0;
    ps_curr_out->i4_out_timestamp_high = 0;

    /*attach the app frame info of this buffer */
    ps_curr_out->pv_app_frm_ctxt = NULL;

    /* frame never skipped for now */
    ps_curr_out->i4_frame_skipped = 0;

    /* update error code and return */
    ps_curr_out->i4_process_error_code = ret;

    ps_curr_out->i4_bytes_generated = ps_bitstrm->u4_strm_buf_offset;

    /* ------------------- Initialize non-VCL prefix NAL Size/offsets --------------------*/
    {
        WORD32 num_non_vcl_prefix_nals = ps_bitstrm->i4_num_nal;
        WORD32 ctr = 0;

        ASSERT(num_non_vcl_prefix_nals <= MAX_NUM_PREFIX_NALS_PER_AU);

        ps_curr_out->i4_num_non_vcl_prefix_nals = num_non_vcl_prefix_nals;
        for(ctr = 0; ctr < MIN(num_non_vcl_prefix_nals, MAX_NUM_PREFIX_NALS_PER_AU); ctr++)
        {
            /* NAL offset is derive by subtracting Bistream base from NAL start pointer */
            ULWORD64 u8_cur_nal_start = (ULWORD64)ps_bitstrm->apu1_nal_start[ctr];

#if POPULATE_NAL_SIZE

            /* ----------Populate NAL Size  -------------*/
            if((ctr + 1) < num_non_vcl_prefix_nals)
            {
                ULWORD64 u8_next_nal_start = (ULWORD64)ps_bitstrm->apu1_nal_start[ctr + 1];
                ps_curr_out->ai4_size_non_vcl_prefix_nals[ctr] =
                    (UWORD32)(u8_next_nal_start - u8_cur_nal_start);
            }
            else
            {
                ULWORD64 u8_next_nal_start =
                    (ULWORD64)ps_bitstrm->pu1_strm_buffer + ps_bitstrm->u4_strm_buf_offset;
                ps_curr_out->ai4_size_non_vcl_prefix_nals[ctr] =
                    (UWORD32)(u8_next_nal_start - u8_cur_nal_start);
            }
            ASSERT(ps_curr_out->ai4_size_non_vcl_prefix_nals[ctr] > 0);

#elif POPULATE_NAL_OFFSET

            /* ----------Populate NAL Offset  -------------*/

            ASSERT(u8_cur_nal_start >= u8_bitstream_base);
            ps_curr_out->ai4_off_non_vcl_prefix_nals[ctr] =
                (UWORD32)(u8_cur_nal_start - u8_bitstream_base);

            if(ctr)
            {
                /* sanity check on increasing NAL offsets */
                ASSERT(
                    ps_curr_out->ai4_off_non_vcl_prefix_nals[ctr] >
                    ps_curr_out->ai4_off_non_vcl_prefix_nals[ctr - 1]);
            }
#endif /* POPULATE_NAL_SIZE */
        }
    }

    ps_curr_out->i4_buf_id = out_buf_id;
    ps_curr_out->i4_end_flag = 0;
    if(IHEVCE_SUCCESS == ret)
    {
        ps_curr_out->i4_process_ret_sts = IV_SUCCESS;
    }
    else
    {
        ps_curr_out->i4_process_ret_sts = IV_FAIL;
    }
    ps_curr_out->i4_encoded_frame_type = IV_NA_FRAME;

    /* Call back to Apln. saying buffer is produced */
    ps_hle_ctxt->ihevce_output_strm_fill_done(
        ps_hle_ctxt->pv_out_cb_handle, ps_curr_out, i4_bitrate_instance_id, i4_resolution_id);

    /* release the input buffer*/
    ihevce_q_rel_buf(
        (void *)ps_enc_ctxt, (IHEVCE_OUTPUT_DATA_Q + i4_bitrate_instance_id), out_buf_id);

    return ret;
}
